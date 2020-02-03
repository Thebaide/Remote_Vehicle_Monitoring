/*
 * System execution loop and interrupt handling
 */

#ifndef __SYSSG_SYS_H__
#define __SYSSG_SYS_H__

/** \addtogroup sys System library
  * @{
  */

/** \addtogroup sys_runtime System Runtime Management
  * @{
  */

/** \addtogroup sys_runtime_init Runtime initialization
  * @{
  */

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

/** @} */


/** \addtogroup sys_runtime_queue Queued function call
  * @{
  */

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

/** @} */

/** \addtogroup sys_runtime_timeout Delayed function call
  * @{
  */

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

/** @} */

/** \addtogroup sys_runtime_led Leightweight LED blink management, based on system timer
  * @{
  * These functions allow to manage LED blinking on the interrupt context.<br/>
  * This allows switching LEDs without the overhead of system timeout management and
  * its callback queue.
  */

/**
 * @brief Blocking sleep
 * @param milliseconds
 * @return none
 */
extern void msleep(int milliseconds);

/**
 * @brief Blocking sleep
 * @param microseconds
 * @return none
 */
extern void usleep(int microseconds);

/** @} */

/** @} */

/** @} */

#endif /* __SYSSG_SYS_H__ */
