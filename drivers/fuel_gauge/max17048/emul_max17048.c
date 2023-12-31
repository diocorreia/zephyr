/*
 * Copyright 2023, Alvaro Garcia <maxpowel@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Emulator for max17048 fuel gauge
 */

#define DT_DRV_COMPAT maxim_max17048

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(maxim_max17048);

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/drivers/fuel_gauge/emul_max17048.h>
#include <zephyr/sys/byteorder.h>

#include "max17048.h"

static const float vcell_lsb = 78.125;

static int crate_value = 0x4000;

void emul_max17048_set_crate_status(int value)
{
	crate_value = value;
}

/** Static configuration for the emulator */
struct max17048_emul_cfg {
	/** I2C address of emulator */
	uint16_t addr;
};

/** Run-time data used by the emulator */
struct max17048_emul_data {
	uint16_t soc_reg;
	uint16_t vcell_reg;
};

int emul_max17048_set_voltage(const struct emul *target, uint16_t voltage_in_mV)
{
	uint32_t voltage_in_uV;

	if (target == NULL) {
		return -EINVAL;
	}

	struct max17048_emul_data *data = target->data;

	/** The maximum supported value for VCELL in mV is 5119. The value is obtained
	 * from the conversion of this value in uV (5119921.875uV). The value in uV is
	 * calculated by multiplying the maximum value of a uint16 register by the value
	 * of in uV for a single bit (78.125uV). */
	if (voltage_in_mV > 5119) {
		LOG_ERR("Invalid VCELL value set on emulator.");
		return -EINVAL;
	}

	voltage_in_uV = voltage_in_mV * 1000;
	voltage_in_uV = DIV_ROUND_UP(voltage_in_uV, vcell_lsb);

	data->vcell_reg = (uint16_t)voltage_in_uV;

	return 0;
}

int emul_max17048_set_soc(const struct emul *target, float soc_in_percentage)
{
	if (target == NULL) {
		return -EINVAL;
	}

	struct max17048_emul_data *data = target->data;

	if (soc_in_percentage > 100.0f) {
		LOG_ERR("Invalid SoC value set on emulator.");
		return -EINVAL;
	}

	data->soc_reg = (uint16_t)(soc_in_percentage * 256);

	return -EINVAL;
}

/** API to configure the emulator on run-time */
static const struct max17048_emul_driver_api max17048_emul_driver_api = {
	.set_voltage = emul_max17048_set_voltage,
	.set_soc = emul_max17048_set_soc,
};

static int emul_max17048_reg_write(const struct emul *target, int reg, int val)
{

	return -EIO;
}

static int emul_max17048_reg_read(const struct emul *target, int reg, int *val)
{
	struct max17048_emul_data *data = target->data;

	switch (reg) {
	case REGISTER_VERSION:
		*val = 0x0010;
		break;
	case REGISTER_CRATE:
		*val = crate_value;
		break;
	case REGISTER_SOC:
		*val = data->soc_reg;
		break;
	case REGISTER_VCELL:
		*val = data->vcell_reg;
		break;
	default:
		LOG_ERR("Unknown register 0x%x read", reg);
		return -EIO;
	}
	LOG_INF("read 0x%x = 0x%x", reg, *val);

	return 0;
}

static int max17048_emul_transfer_i2c(const struct emul *target, struct i2c_msg *msgs, int num_msgs,
				      int addr)
{
	/* Largely copied from emul_bmi160.c */
	unsigned int val;
	int reg;
	int rc;

	__ASSERT_NO_MSG(msgs && num_msgs);

	i2c_dump_msgs_rw(target->dev, msgs, num_msgs, addr, false);
	switch (num_msgs) {
	case 2:
		if (msgs->flags & I2C_MSG_READ) {
			LOG_ERR("Unexpected read");
			return -EIO;
		}
		if (msgs->len != 1) {
			LOG_ERR("Unexpected msg0 length %d", msgs->len);
			return -EIO;
		}
		reg = msgs->buf[0];

		/* Now process the 'read' part of the message */
		msgs++;
		if (msgs->flags & I2C_MSG_READ) {
			switch (msgs->len - 1) {
			case 1:
				rc = emul_max17048_reg_read(target, reg, &val);
				if (rc) {
					/* Return before writing bad value to message buffer */
					return rc;
				}

				/* MAX17048 sends MSB first */
				sys_put_be16(val, msgs->buf);
				break;
			default:
				LOG_ERR("Unexpected msg1 length %d", msgs->len);
				return -EIO;
			}
		} else {
			/* We write a word */
			if (msgs->len != 2) {
				LOG_ERR("Unexpected msg1 length %d", msgs->len);
			}
			uint16_t value = sys_get_be16(msgs->buf);

			rc = emul_max17048_reg_write(target, reg, value);
		}
		break;
	default:
		LOG_ERR("Invalid number of messages: %d", num_msgs);
		return -EIO;
	}

	return rc;
}

static const struct i2c_emul_api max17048_emul_api_i2c = {
	.transfer = max17048_emul_transfer_i2c,
};

/**
 * Set up a new emulator (I2C)
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 * @return 0 indicating success (always)
 */
static int emul_max17048_init(const struct emul *target, const struct device *parent)
{
	ARG_UNUSED(target);
	ARG_UNUSED(parent);

	return 0;
}

/*
 * Main instantiation macro.
 */
#define MAX17048_EMUL(n)                                                                           \
	static const struct max17048_emul_cfg max17048_emul_cfg_##n = {                            \
		.addr = DT_INST_REG_ADDR(n),                                                       \
	};                                                                                         \
	static struct max17048_emul_data max17048_emul_data_##n = {                                \
		.soc_reg = 0x2535,                                                                 \
		.vcell_reg = 0x8743,                                                               \
	};                                                                                         \
	EMUL_DT_INST_DEFINE(n, emul_max17048_init, &max17048_emul_data_##n,                        \
			    &max17048_emul_cfg_##n, &max17048_emul_api_i2c,                        \
			    &max17048_emul_driver_api)

DT_INST_FOREACH_STATUS_OKAY(MAX17048_EMUL)
