/*
 * System execution loop and interrupt handling
 */

#include "os_type.h"
#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "colors.h"
#include "sys.h"
#include "console.h"

#define TAG "sys"


/*
 * Event handler queue management.
 * This allows to process interrupt-driven events
 * within the main execution loop context.
 */

typedef struct {
	sys_event_handler *handler;
	void *ctx;
	int id;
	int timeout;
} sys_event_t;

#define SYS_NEVENTS      32
#define SYS_NTIMEOUTS    16
#define SYS_TIMER_PERIOD 1 //ms

LOCAL os_event_t sys_fifo[SYS_NEVENTS];

static sys_event_t sys_events[SYS_NEVENTS];
static volatile int sys_event_put = 0;
static volatile int sys_event_get = 0;
static int sys_event_count = 0;
static uint8_t sys_full_signaled = 0;
static volatile uint8_t sys_shutdown_requested = 0;
static int sys_timer_idle = 1;

LOCAL os_timer_t sys_timer;

static sys_event_t sys_timeouts[SYS_NTIMEOUTS];


static inline int sys_queue_len(void)
{
	int len = sys_event_put - sys_event_get;
	return (len >= 0) ? len : (SYS_NEVENTS + len);
}


static int sys_queue_id(sys_event_handler *handler, void *ctx, int id)
{
	sys_event_t *ev;

	/* Check queue can accept new action */
	if (sys_queue_len() >= (SYS_NEVENTS-1)) {
		if (!sys_full_signaled)
			cprintf(TAG, COLOR_RED"PANIC: System event queue is full\n");
		sys_full_signaled = 1;
		return -1;
	}

	sys_full_signaled = 0;

	/* Equeue action */
	ev = &sys_events[sys_event_put];
	ev->handler = handler;
	ev->ctx = ctx;
	ev->id = id;
	ev->timeout = 0;

	sys_event_put++;
	if (sys_event_put >= SYS_NEVENTS)
		sys_event_put = 0;

	/* Send message to the sys task */
	system_os_post(USER_TASK_PRIO_2, 0, 0);

	return sys_event_count;
}

/* sys main task */
LOCAL void sys_task(os_event_t *e)
{
	/* Dequeue and call action routines */
	sys_event_t *ev = &sys_events[sys_event_get];
	sys_event_t ev2 = *ev;

	/* Free the queue entry before calling event handler */
	ev->handler = NULL;
	ev->ctx = NULL;

	sys_event_get++;
	if (sys_event_get >= SYS_NEVENTS)
		sys_event_get = 0;

	/* Call event handler */
	if (ev2.handler)
		ev2.handler(ev2.ctx, ev2.id);
}


int sys_queue(sys_event_handler *handler, void *ctx)
{
	if (sys_shutdown_requested)
		return -1;

	/* Increment event id counter */
	sys_event_count++;
	if (sys_event_count < 0)
		sys_event_count = 0;

	return sys_queue_id(handler, ctx, sys_event_count);
}


/*
 * Non-blocking timer management.
 */

static volatile int sys_timeout_dt = 0;
static int sys_timeout_count = 0;
static volatile uint32_t sys_period = 0;

static void sys_timeout_update_dt(int delay)
{
	if ((sys_timeout_dt <= 0) || (sys_timeout_dt > delay))
		sys_timeout_dt = delay;
}

static int sys_timeout_new(void)
{
	int i;

	for (i = 0; i < SYS_NTIMEOUTS; i++) {
		if (sys_timeouts[i].handler == NULL)
			return i;
	}

	return -1;
}


static void sys_timeout_trig(int delay, int id)
{
	/* Stop running timer and get remaining time,
	   then restart with smallest value */

	if (sys_timer_idle) {
		sys_timeout_dt = delay;
		sys_timer_idle = 0;
	}
	else {
		os_timer_disarm(&sys_timer);
		uint32_t remaining = sys_period;
		uint32_t done      = sys_timeout_dt - remaining;
		int i;

		for (i=0; i<SYS_NTIMEOUTS; i++) {
			sys_event_t *ev = &sys_timeouts[i];
			if ((ev->timeout > 0) && (ev->id != id)) {
				ev->timeout -= done;
			}
		}

		if (remaining < delay) {
			sys_timeout_dt = remaining;
		}
		else {
			sys_timeout_dt = delay;
		}
	}

	/* Re-arm timer */
	sys_period = sys_timeout_dt;
	os_timer_arm(&sys_timer, SYS_TIMER_PERIOD, 1);
}


int sys_timeout_add(int milliseconds, sys_event_handler *handler, void *ctx)
{
	int i = sys_timeout_new();
	sys_event_t *ev;

	if (sys_shutdown_requested)
		return -1;

	if (i < 0) {
		cprintf(TAG, COLOR_RED"PANIC: All timeout slots are busy\n");
		return -1;
	}

	/* Increment redundency counter */
	sys_timeout_count += SYS_NTIMEOUTS;
	if (sys_timeout_count < 0)
		sys_timeout_count = 0;

	ev = &sys_timeouts[i];
	ev->handler = handler;
	ev->ctx = ctx;
	ev->id = sys_timeout_count + i;
	ev->timeout = milliseconds;

	sys_timeout_trig(milliseconds, ev->id);

	return ev->id;
}


void sys_timeout_del(int id)
{
	os_timer_disarm(&sys_timer);
	if (id > 0) {
		int i = id % SYS_NTIMEOUTS;
		sys_event_t *ev = &sys_timeouts[i];


		if (id == ev->id) {
			ev->handler = NULL;
			ev->id = 0;
		}
	}
	os_timer_arm(&sys_timer, SYS_TIMER_PERIOD, 1);
}

/*
 * SYS timer interrupt handler
 */

LOCAL void ICACHE_FLASH_ATTR sys_timer_callback(void *arg)
{
	int dt;
	int i;

	sys_period -= SYS_TIMER_PERIOD;

	if (sys_period <= 0) {

		os_timer_disarm(&sys_timer);

		dt = sys_timeout_dt;
		sys_timeout_dt = 0;

		/* Process running sys timers */
		for (i = 0; i < SYS_NTIMEOUTS; i++) {
			sys_event_t *ev = &sys_timeouts[i];
			if (ev->handler) {
				ev->timeout -= dt;
				if (ev->timeout <= 0) {
					sys_queue_id(ev->handler, ev->ctx, ev->id);
					ev->handler = NULL;
					ev->id = 0;
				}
				else {
					sys_timeout_update_dt(ev->timeout);
				}
			}
		}

		/* Restart timer for next timeout event */
		if (sys_timeout_dt > 0) {
			sys_period = sys_timeout_dt;
			os_timer_arm(&sys_timer, SYS_TIMER_PERIOD, 1);
		}
		else {
			sys_timer_idle = 1;
		}
	}
}

/*
 * Blocking sleep.
 * The timer used for this primitives is also used
 * for the UART rx timeout management.
 * This is permitted because these are both blocking calls which
 * might never run concurrently.
 */

void msleep(int milliseconds)
{
	while (milliseconds) {
		os_delay_us(1000);
		milliseconds--;
	}
}


/*
 * Blocking sleep.
 * The timer used for this primitives is also used
 * for the UART rx timeout management.
 * This is permitted because these are both blocking calls which
 * might never run concurrently.
 */

void usleep(int microseconds)
{
	os_delay_us((uint16_t) microseconds);
}


/*
 * Low-level system and main loop initialization
 */

void sys_init(void)
{
	int i;

	/* Clear event queue */
	for (i = 0; i < SYS_NEVENTS; i++) {
		sys_event_t *ev = &sys_events[i];
		ev->handler = NULL;
		ev->ctx = NULL;
		ev->id = 0;
		ev->timeout = 0;
	}

	sys_event_put = 0;
	sys_event_get = 0;
	sys_event_count = 0;

	/* Init an esp8266 os_timer */
	os_timer_setfn(&sys_timer, (os_timer_func_t *)sys_timer_callback, NULL);

	/* Init a sys task */
	if (system_os_task(sys_task, USER_TASK_PRIO_2, sys_fifo, SYS_NEVENTS) == TRUE) {
		cprintf(TAG, "Task set up with success\n");
	}
	else {
		cprintf(TAG, "Failed to set up task\n");
	}
}

void sys_shutdown(void)
{
	/* Disable timers */
	//os_timer_disarm(&sys_timer);

	sys_shutdown_requested = 1;
	sys_event_put = 0;
	sys_event_get = 0;
}

