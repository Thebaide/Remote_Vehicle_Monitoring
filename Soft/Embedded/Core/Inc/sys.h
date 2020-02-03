/*
 * System execution loop and interrupt handling
 */

#ifndef __SYS_H
#define __SYS_H

/**
 * @brief Init system runtime.
 *
 * This function must be called at system startup
 * in order to initialized runtime and timer gears.
 */
extern void sys_init(void);

/**
 * @brief Shutdown system runtime.
 *
 * This function shut down all system resources.
 */
extern void sys_shutdown(void);

extern void sys_time_show(void);

/**
 * @brief System event handler function type
 */
typedef void sys_event_handler(void *ctx, int id);

/**
 * @brief Macro used to easily cast a function as a system event handler.
 */
#define SYS_EVENT_HANDLER(func) ((sys_event_handler *)(func))

/**
 * @brief Queue a function to be called in the main processing loop.
 * @param handler
 *   Pointer to function to call
 * @param ctx
 *   Context pointer (ctx) to be passed to the function
 *
 * This primitive inserts a new function to be called in the main processing loop.<br/>
 * This is typically used from an interrupt handler, in order to queue a function
 * that will be executed in the main loop context.
 */
extern int sys_queue(sys_event_handler *handler, void *ctx);

/**
 * Start a periodic timer instance.
 *
 * @param milliseconds
 *   Timer period in milliseconds.
 * @param handler
 *   Callback function to call when timer expires
 * @param ctx
 *   Opaque parameter to give to callback function
 * @return
 *   The timer instance identifier
 */
extern int sys_timeout_add(int milliseconds, sys_event_handler *handler, void *ctx);

/**
 * Delete a periodic timer instance.
 *
 * @param id
 *   The timer instance identifier
 */
extern void sys_timeout_del(int id);

#endif /* __SYS_H */
