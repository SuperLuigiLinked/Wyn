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
extern int wyn_run(void);

/**
 * Causes the Event Loop to terminate, if it is running.
 */
extern void wyn_quit(struct wyn_events_t* events, int code);

/**
 * Attempts to open a new Window. May return NULL.
 */
extern wyn_window_t wyn_open_window(struct wyn_events_t* events);

/**
 * Closes a previously opened Window.
 */
extern void wyn_close_window(struct wyn_events_t* events, wyn_window_t window);

/**
 * Shows a hidden Window.
 */
extern void wyn_show_window(struct wyn_events_t* events, wyn_window_t window);

/**
 * Hides a visible Window.
 */
extern void wyn_hide_window(struct wyn_events_t* events, wyn_window_t window);

// ================================================================================================================================ //
// User Callbacks
// -------------------------------------------------------------------------------------------------------------------------------- //

/**
 * Called once after the Event Loop has been initialized.
 */
extern void wyn_on_start(struct wyn_events_t* events);

/**
 * Called once before the Event Loop has been terminated.
 */
extern void wyn_on_stop(struct wyn_events_t* events);

/**
 * Called when a Window is about to close.
 */
extern void wyn_on_window_close(struct wyn_events_t* events, wyn_window_t window);

// ================================================================================================================================ //
