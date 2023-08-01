/*
    Wyn.h
*/

#pragma once

#ifndef WYN_H
#define WYN_H 1

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  WYN: Cross-Platform Windowing Library.
 *
 *  All Wyn functions must be called on the Main Thread, unless otherwise specified.
 *
 *  All pointers/handles passed to functions must be NON-NULL, and all return values will be NON-NULL, unless otherwise specified.
 *
 *  The user must first call `wyn_run` to start the Event Loop.
 *  The library will then call the user-defined `wyn_on_*` callback functions as relevant while it runs.
 *
 *  From the time `wyn_on_start` is called, until the time `wyn_on_stop` returns,
 *  it is safe to call other Wyn functions and use Wyn pointers/handles.
 */

// ================================================================================================================================ //
// Type Declarations
// -------------------------------------------------------------------------------------------------------------------------------- //

/**
 * Window handle type.
 */
typedef void* wyn_window_t;

// ================================================================================================================================ //
// API Functions
// -------------------------------------------------------------------------------------------------------------------------------- //

/**
 * Starts the Event Loop. Must not be called while the Event Loop is already running.
 * The `userdata` pointer provided is passed to all callback functions. (May be NULL)
 */
extern int wyn_run(void* userdata);

/**
 * Causes the Event Loop to terminate, if it is running.
 * If this function is called multiple times, it is unspecified which exit code is used.
 */
extern void wyn_quit(int code);

/**
 * Attempts to open a new Window. May return NULL.
 */
extern wyn_window_t wyn_open_window(void);

/**
 * Closes a previously opened Window.
 */
extern void wyn_close_window(wyn_window_t window);

/**
 * Shows a hidden Window.
 */
extern void wyn_show_window(wyn_window_t window);

/**
 * Hides a visible Window.
 */
extern void wyn_hide_window(wyn_window_t window);

// ================================================================================================================================ //
// User Callbacks
// -------------------------------------------------------------------------------------------------------------------------------- //

/**
 * Called once after the Event Loop has been initialized.
 */
extern void wyn_on_start(void* userdata);

/**
 * Called once before the Event Loop has been terminated.
 * If any windows are not closed after this function returns, they will be closed without calling the user-defined callback.
 */
extern void wyn_on_stop(void* userdata);

/**
 * Called when a Window is about to close.
 */
extern void wyn_on_window_close(void* userdata, wyn_window_t window);

// ================================================================================================================================ //

#ifdef __cplusplus
}
#endif

#endif
