/**
 * @file wyn.h
 *
 * @brief Wyn: Cross-Platform Windowing Library.
 *
 * All Wyn functions must be called on the Main Thread, unless otherwise specified.
 *
 * The user must first call `wyn_run` to start the Event Loop.
 * The library will then call the user-defined `wyn_on_*` callback functions as relevant while it runs.
 *
 * From the time `wyn_on_start` is called, until the time `wyn_on_stop` returns,
 * it is safe to call other Wyn functions and use Wyn pointers/handles.
 */

#pragma once

#ifndef WYN_H
#define WYN_H

// ================================================================================================================================
//  Type Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Handle to a Window.
 */
typedef void* wyn_window_t;

/**
 * @brief Result of scheduling an exec-callback.
 */
enum wyn_status_t
{
    wyn_status_canceled = -1,  ///< The callback was canceled (such as during shutting down).
    wyn_status_pending = 0,   ///< The callback is scheduled to be executed.
    wyn_status_complete = 1,  ///< The callback has finished execution.
};
typedef enum wyn_status_t wyn_status_t;

struct wyn_task_t
{
    void (*func)(void*);
    void* args;
    _Atomic(struct wyn_task_t*) next;
    _Atomic(enum wyn_status_t) status;
};
typedef struct wyn_task_t wyn_task_t;

// ================================================================================================================================
//  API Functions
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Runs the Event Loop.
 * @param[in] userdata [nullable] A pointer that will be passed to all user-callback functions.
 * @warning This function is not reentrant. Do not call this function while the Event Loop is already running.
 */
extern void wyn_run(void* userdata);

/**
 * @brief Causes the Event Loop to terminate.
 */
extern void wyn_quit(void);

/**
 * @brief Queries whether or not the Event Loop is stopping.
 * @return `true` if stopping, `false` otherwise.
 */
extern bool wyn_quitting(void);

/**
 * @brief Queries whether or not the Event Loop is on the calling thread.
 * @return `true` if this thread is the Event Thread, `false` otherwise.
 */
extern bool wyn_is_this_thread(void);

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Adds a Task to the end of the Task Queue.
 * @param[in,out] task [non-null] The task to enqueue.
 * @warning Once queued, the pointer must remain valid and the data unchanged until the status is no longer "pending". 
 */
extern void wyn_task_enqueue(wyn_task_t* task);

/**
 * @brief Removes a Task from the front of the Task Queue.
 * @return [nullable] The dequeued task, or NULL if there are none.
 * @warning Must only be called on the Event Thread.
 * @note Normally, the user need not call this function, as the Event Loop will dequeue tasks while running.
 *       In certain circumstances, the user may want to call this function themselves in their event handlers.
 */
extern wyn_task_t* wyn_task_dequeue(void);

/**
 * @brief Signals to the Event Thread that Tasks are pending.
 * @note Under normal circumstances, the Event Loop will not start dequeing tasks until it is signalled.
 */
extern void wyn_task_signal(void);

/**
 * @brief Atomically checks the Task's status.
 * @return The status of the task's execution. May still be pending.
 * @param[in] task [non-null] The task to poll.
 */
extern wyn_status_t wyn_task_poll(const wyn_task_t* task);

/**
 * @brief Waits until the Task is no longer pending.
 * @return The status of the task's execution. Will be either canceled/completed.
 * @param[in] task [non-null] The task to await.
 */
extern wyn_status_t wyn_task_await(const wyn_task_t* task);

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Attempts to open a new Window.
 * @return [nullable] A handle to the new Window, if successful.
 */
extern wyn_window_t wyn_window_open(void);

/**
 * @brief Closes a previously opened Window.
 * @param[in] window [non-null] A handle to the Window.
 * @warning Once a Window has been closed, its handle is invalidated and must not be used.
 */
extern void wyn_window_close(wyn_window_t window);

/**
 * @brief Shows a hidden Window.
 * @param[in] window [non-null] A handle to the Window.
 */
extern void wyn_window_show(wyn_window_t window);

/**
 * @brief Hides a visible Window.
 * @param[in] window [non-null] A handle to the Window.
 */
extern void wyn_window_hide(wyn_window_t window);

// --------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

// ================================================================================================================================
//  User Callbacks
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Called once after the Event Loop has been initialized.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 */
extern void wyn_on_start(void* userdata);

/**
 * @brief Called once before the Event Loop has been terminated.
 * @details Once this function is called, exec-callbacks can no longer be scheduled, and all pending callbacks are canceled.
 *          After this function returns, all remaining windows will be forcibly closed, without calling the user-callback.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 */
extern void wyn_on_stop(void* userdata);

/**
 * @brief Called when a Window is requested to close.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window that is about to close.
 */
extern void wyn_on_window_close_request(void* userdata, wyn_window_t window);

/**
 * @brief Called when a Window needs its contents redrawn.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window that needs its contents redrawn.
 */
extern void wyn_on_window_redraw(void* userdata, wyn_window_t window);

#ifdef __cplusplus
}
#endif

// ================================================================================================================================

#endif /* WYN_H */
