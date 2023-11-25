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
//  Macros
// --------------------------------------------------------------------------------------------------------------------------------

#if defined(__cplusplus)    // C++
    #define WYN_BOOL bool
#else                       // C
    #define WYN_BOOL _Bool
#endif

// ================================================================================================================================
//  Type Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Handle to a Window.
 */
typedef void* wyn_window_t;

/**
 * @brief Unit for a coordinate/extent.
 */
typedef double wyn_coord_t;

/**
 * @brief A 2D Point.
 */
struct wyn_point_t { wyn_coord_t x, y; };
typedef struct wyn_point_t wyn_point_t;

/**
 * @brief A 2D Size.
 */
struct wyn_size_t { wyn_coord_t w, h; };
typedef struct wyn_size_t wyn_size_t;

/**
 * @brief A 2D Rectangle.
 */
struct wyn_rect_t { wyn_point_t center; wyn_size_t size; };
typedef struct wyn_rect_t wyn_rect_t;

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
 * @note This function may be called from any thread.
 */
extern WYN_BOOL wyn_quitting(void);

/**
 * @brief Queries whether or not the Event Loop is on the calling thread.
 * @return `true` if this thread is the Event Thread, `false` otherwise.
 * @note This function may be called from any thread.
 */
extern WYN_BOOL wyn_is_this_thread(void);

/**
 * @brief Wakes up the Event Thread and calls the `wyn_on_signal` user-callback.
 * @note This function may be called from any thread.
 */
extern void wyn_signal(void);

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

/**
 * @brief Queries the scale of a Window.
 * @param[in] window [non-null] A handle to the Window.
 * @return The scale of the Window relative to the pixel resolution of its framebuffer.
 * @note On most platforms, this value is always `1.0`. Some platforms (e.g. MacOS) may return other values, like `2.0`.
 */
extern double wyn_window_scale(wyn_window_t window);

/**
 * @brief Queries the size of a Window.
 * @param[in] window [non-null] A handle to the Window.
 * @return The size of the Window's framebuffer, in Pixels.
 */
extern wyn_size_t wyn_window_size(wyn_window_t window);

/**
 * @brief Resizes a Window.
 * @param[in] window [non-null] A handle to the Window.
 * @param size The new size of the Window's framebuffer, in Pixels.
 */
extern void wyn_window_resize(wyn_window_t window, wyn_size_t size);

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Returns platform-specific data, potentially associated with a Window.
 * @param[in] window [non-null] A handle to a Window.
 * @return [nullable]
 * -    Win32: (HWND) -> HINSTANCE
 * -     Xlib: (Window) -> Display*
 * -      Xcb: (xcb_window_t) -> xcb_connection_t*
 * - Xlib-Xcb: (xcb_window_t) -> xcb_connection_t*
 * -  Wayland: (wl_surface*) -> wl_display*
 * -    Cocoa: (NSWindow*) -> NSView*
 * @note This function may be called from any thread.
 */
extern void* wyn_native_context(wyn_window_t window);

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
 * @brief Called whenever the Event Loop is woken up by a call to `wyn_signal`.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 */
extern void wyn_on_signal(void* userdata);

/**
 * @brief Called when a Window is requested to close.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window that is about to close.
 * @note The Window will not close automatically. The user may choose whether or not to close the Window manually.
 */
extern void wyn_on_window_close(void* userdata, wyn_window_t window);

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
