/*
    Wyn.h
*/

#pragma once

/**
 *  All Wyn functions must be called on the Main Thread, unless otherwise specified.
 *
 *  The user must first call `wyn_run` to start the Event Loop.
 *  The library will then call the user-defined `wyn_on_*` callback functions as relevant while it runs.
 *
 *  From the time `wyn_on_start` is called, until the time `wyn_on_stop` returns,
 *  it is safe to call other Wyn functions and use Wyn pointers/handles.
 */

// ================================================================================================================================ //
// Macros
// -------------------------------------------------------------------------------------------------------------------------------- //

#ifdef __cplusplus
#   define WYN_API extern "C"
#else
#   define WYN_API extern
#endif

// ================================================================================================================================ //
// Type Declarations
// -------------------------------------------------------------------------------------------------------------------------------- //

/**
 * Event Loop state.
 */
struct wyn_events_t;

/**
 * Window handle type.
 */
typedef void* wyn_window_t;

// ================================================================================================================================ //
// API Functions
// -------------------------------------------------------------------------------------------------------------------------------- //


/**
 * Starts the Event Loop. Must not be called while the Event Loop is already running.
 */
WYN_API int wyn_run(void);

/**
 * Causes the Event Loop to terminate, if it is running.
 * If this function is called multiple times, it is unspecified which exit code is used.
 */
WYN_API void wyn_quit(struct wyn_events_t* events, int code);

/**
 * Attempts to open a new Window. May return NULL.
 */
WYN_API wyn_window_t wyn_open_window(struct wyn_events_t* events);

/**
 * Closes a previously opened Window.
 */
WYN_API void wyn_close_window(struct wyn_events_t* events, wyn_window_t window);

/**
 * Shows a hidden Window.
 */
WYN_API void wyn_show_window(struct wyn_events_t* events, wyn_window_t window);

/**
 * Hides a visible Window.
 */
WYN_API void wyn_hide_window(struct wyn_events_t* events, wyn_window_t window);

// ================================================================================================================================ //
// User Callbacks
// -------------------------------------------------------------------------------------------------------------------------------- //

/**
 * Called once after the Event Loop has been initialized.
 */
WYN_API void wyn_on_start(struct wyn_events_t* events);

/**
 * Called once before the Event Loop has been terminated.
 * If any windows are not closed after this function returns, they will be closed without calling the user-defined callback.
 */
WYN_API void wyn_on_stop(struct wyn_events_t* events);

/**
 * Called when a Window is about to close.
 */
WYN_API void wyn_on_window_close(struct wyn_events_t* events, wyn_window_t window);

// ================================================================================================================================ //
