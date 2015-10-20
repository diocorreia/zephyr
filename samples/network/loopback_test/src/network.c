/* network.c - Networking demo */

/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

#include <net/net_core.h>
#include <net/net_socket.h>

#include <net_driver_loopback.h>

/* Longer packet sending works only if fragmentation is supported
 * by network stack.
 */
#if 0
/* Generated by http://www.lipsum.com/
 * 2 paragraphs, 185 words, 1231 bytes of Lorem Ipsum
 * The main() will add one null byte at the end so the maximum
 * length for the data to send is 1232 bytes.
 */
static const char *lorem_ipsum =
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam congue non neque vel tempor. In id porta nibh, ut cursus tortor. Morbi eleifend tristique vehicula. Nunc vitae risus mauris. Praesent vel imperdiet dolor, et ultricies nibh. Aliquam erat volutpat. Maecenas pellentesque dolor vitae dictum tincidunt. Fusce vel nibh nec leo tristique auctor eu a massa. Nam et tellus ac tortor sollicitudin semper vitae nec tortor. Aliquam nec lacus velit. Maecenas ornare ullamcorper justo non auctor. Donec aliquam feugiat turpis, quis elementum sem rutrum ut. Sed eu ullamcorper libero, ut suscipit magna."
	"\n"
	"Donec vehicula magna ut varius aliquam. Ut vitae commodo nulla, quis ornare dolor. Nulla tortor sem, venenatis eu iaculis id, commodo ut massa. Sed est lorem, euismod vitae enim sed, hendrerit gravida felis. Donec eros lacus, auctor ut ultricies eget, lobortis quis nisl. Aliquam sit amet blandit eros. Interdum et malesuada fames ac ante ipsum primis in faucibus. Quisque egestas nisl leo, sed consectetur leo ornare eu. Suspendisse vitae urna vel purus maximus finibus. Proin sed sollicitudin turpis. Mauris interdum neque eu tellus pellentesque, id fringilla nisi fermentum. Suspendisse gravida pharetra sodales orci aliquam.";
#else
static const char *lorem_ipsum =
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit.";
#endif

#ifdef CONFIG_MICROKERNEL

#error "Microkernel version not supported yet."

/*
 * Microkernel version of hello world demo has two tasks that utilize
 * semaphores and sleeps to take turns printing a greeting message at
 * a controlled rate.
 */

#include <microkernel.h>

/* specify delay between greetings (in ms); compute equivalent in ticks */

#define SLEEPTIME  500
#define SLEEPTICKS (SLEEPTIME * sys_clock_ticks_per_sec / 1000)

#else /*  CONFIG_NANOKERNEL */

/*
 * Nanokernel version of hello world demo has a task and a fiber that utilize
 * semaphores and timers to take turns printing a greeting message at
 * a controlled rate.
 */

#include <nanokernel.h>

/* specify delay between greetings (in ms); compute equivalent in ticks */

#define SLEEPTIME  500
#define SLEEPTICKS (SLEEPTIME * sys_clock_ticks_per_sec / 1000)

#define STACKSIZE 2000

char fiberStack[STACKSIZE];

struct nano_sem nanoSemTask;
struct nano_sem nanoSemFiber;

const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;            /* ::  */
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;  /* ::1 */

static struct net_addr any_addr;
static struct net_addr loopback_addr;

void fiberEntry(void)
{
	struct nano_timer timer;
	uint32_t data[2] = {0, 0};
	struct net_context *ctx;
	struct net_buf *buf;

	ctx = net_context_get(IPPROTO_UDP,
			      &any_addr, 0,
			      &loopback_addr, 4242);
	if (!ctx) {
		PRINT("%s: Cannot get network context\n", __func__);
		return;
	}

	nano_sem_init (&nanoSemFiber);
	nano_timer_init (&timer, data);

	while (1) {
		/* wait for task to let us have a turn */
		nano_fiber_sem_take_wait (&nanoSemFiber);

		buf = net_receive(ctx, TICKS_NONE);
		if (buf) {
			PRINT("%s: received %d bytes\n", __func__,
				net_buf_datalen(buf));
			net_buf_put(buf);
		}

		/* wait a while, then let task have a turn */
		nano_fiber_timer_start (&timer, SLEEPTICKS);
		nano_fiber_timer_wait (&timer);
		nano_fiber_sem_give (&nanoSemTask);
	}
}

void main(void)
{
	struct nano_timer timer;
	uint32_t data[2] = {0, 0};
	struct net_context *ctx;
	struct net_buf *buf;
	int len = strlen(lorem_ipsum);

	/* Pretend to be ethernet with 6 byte mac */
	uint8_t mac[] = { 0x0a, 0xbe, 0xef, 0x15, 0xf0, 0x0d };

	PRINT("%s: run net_loopback_test\n", __func__);

	net_init();
	net_driver_loopback_init();

	any_addr.in6_addr = in6addr_any;
	any_addr.family = AF_INET6;

	loopback_addr.in6_addr = in6addr_loopback;
	loopback_addr.family = AF_INET6;

	net_set_mac(mac, sizeof(mac));

	ctx = net_context_get(IPPROTO_UDP,
			      &loopback_addr, 4242,
			      &any_addr, 0);
	if (!ctx) {
		PRINT("Cannot get network context\n");
		return;
	}

	task_fiber_start (&fiberStack[0], STACKSIZE,
			(nano_fiber_entry_t) fiberEntry, 0, 0, 7, 0);

	nano_sem_init(&nanoSemTask);
	nano_timer_init(&timer, data);

	while (1) {
		buf = net_buf_get_tx(ctx);
		if (buf) {
			uint8_t *ptr;
			uint16_t sent_len;

			ptr = net_buf_add(buf, 0);
			memcpy(ptr, lorem_ipsum, len);
			ptr = net_buf_add(buf, len);
			ptr = net_buf_add(buf, 1); /* add \0 */
			*ptr = '\0';
			sent_len = buf->len;

			if (net_send(buf) < 0) {
				PRINT("%s: sending %d bytes failed\n",
					__func__, len);
				net_buf_put(buf);
			} else
				PRINT("%s: sent %d bytes\n", __func__,
					sent_len);
		}

		/* wait a while, then let fiber have a turn */
		nano_task_timer_start(&timer, SLEEPTICKS);
		nano_task_timer_wait(&timer);
		nano_task_sem_give(&nanoSemFiber);

		/* now wait for fiber to let us have a turn */
		nano_task_sem_take_wait(&nanoSemTask);
	}
}

#endif /* CONFIG_MICROKERNEL ||  CONFIG_NANOKERNEL */
