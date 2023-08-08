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
 * From the time `wyn_on_start` is called, until the time `wyn_on_stop` returns (and all exec-callbacks are cleared),
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

// ================================================================================================================================
//  API Functions
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief Schedules a callback function to be executed synchronously on the Main Thread.
 * @details This function returns after the callback has finished executing.
 * @param[in] func [non-null] The callback function to execute.
 * @param[in] arg  [nullable] The argument to pass to the callback function.
 * @note This function may be called from any thread.
 */
extern void wyn_execute(void (*func)(void*), void* arg);

/**
 * @brief Schedules a callback function to be executed asynchronously on the Main Thread.
 * @details This function returns immediately, even when called on the Main Thread.
 * @param func [non-null] The callback function to execute.
 * @param arg  [nullable] The argument to pass to the callback function.
 * @note This function may be called from any thread.
 */
extern void wyn_execute_async(void (*func)(void*), void* arg);

/**
 * @brief Attempts to open a new Window.
 * @return [nullable] A handle to the new Window, if successful.
 */
extern wyn_window_t wyn_open_window(void);

/**
 * @brief Closes a previously opened Window.
 * @param window [non-null] A handle to the Window.
 * @warning Once a Window has been closed, its handle is invalidated and must not be used.
 */
extern void wyn_close_window(wyn_window_t window);

/**
 * @brief Shows a hidden Window.
 * @param window [non-null] A handle to the Window.
 */
extern void wyn_show_window(wyn_window_t window);

/**
 * @brief Hides a visible Window.
 * @param window [non-null] A handle to the Window.
 */
extern void wyn_hide_window(wyn_window_t window);

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
 * @details After this function returns, the following will happen:
 *            1. All remaining exec-callbacks will be called.
 *            2. All remaining windows will be forcibly closed, without calling the user-callback.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 */
extern void wyn_on_stop(void* userdata);

/**
 * @brief Called when a Window is about to close.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window that is about to close.
 * @warning Once a Window has been closed, its handle is invalidated and must not be used.
 */
extern void wyn_on_window_close(void* userdata, wyn_window_t window);

#ifdef __cplusplus
}
#endif

// ================================================================================================================================

#endif /* WYN_H */
