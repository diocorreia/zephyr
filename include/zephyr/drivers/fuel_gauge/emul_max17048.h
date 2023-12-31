/*
 * Copyright 2023, Diogo Correia <dcorreia@protonmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/emul.h>

#ifndef ZEPHYR_INCLUDE_DRIVERS_FUEL_GAUGE_EMUL_MAX17048_H_
#define ZEPHYR_INCLUDE_DRIVERS_FUEL_GAUGE_EMUL_MAX17048_H_

/**
 * @cond INTERNAL_HIDDEN
 *
 * These are for internal use only, so skip these in public documentation.
 */
__subsystem struct max17048_emul_driver_api {
	int (*set_voltage)(const struct emul *target, uint16_t voltage_in_mV);
	int (*set_soc)(const struct emul *target, float soc_in_percentage);
};

/**
 * @brief Set the voltage value.
 *
 * The corresponding fuel-gauge emulator updates the vendor specific registers
 * and generates an interrupt for processing by the real driver, if supported.
 *
 * @param target Pointer to the emulator structure for the MAX17048.
 * @param soc_in_percentage Value of voltage in milivolts.
 *
 * @retval 0 If successful.
 * @retval -EINVAL if the \ref voltage_in_mV is invalid or \ref target is not supported.
 */
static inline int max17048_emul_set_voltage(const struct emul *target, uint16_t voltage_in_mV)
{
	const struct max17048_emul_driver_api *backend_api =
		(const struct max17048_emul_driver_api *)target->backend_api;

	return backend_api->set_voltage(target, voltage_in_mV);
}

/**
 * @brief Set the state-of-charge value.
 *
 * The corresponding fuel-gauge emulator updates the vendor specific registers
 * and generates an interrupt for processing by the real driver, if supported.
 *
 * @param target Pointer to the emulator structure for the MAX17048.
 * @param soc_in_percentage Value of state-of-charge in percentage.
 *
 * @retval 0 If successful.
 * @retval -EINVAL if the \ref voltage_in_mV is invalid or \ref target is not supported.
 */
static inline int max17048_emul_set_soc(const struct emul *target, float soc_in_percentage)
{
	const struct max17048_emul_driver_api *backend_api =
		(const struct max17048_emul_driver_api *)target->backend_api;

	return backend_api->set_soc(target, soc_in_percentage);
}

#endif /* ZEPHYR_INCLUDE_DRIVERS_FUEL_GAUGE_EMUL_MAX17048_H_ */
