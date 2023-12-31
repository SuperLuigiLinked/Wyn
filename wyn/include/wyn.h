/**
 * @file wyn.h
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

#include "wyc.h"

// ================================================================================================================================
//  Type Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Language-agnostic boolean type.
 */
#ifdef __cplusplus
    typedef bool wyn_bool_t;
#else
    typedef _Bool wyn_bool_t;
#endif

/**
 * @brief Handle to a Window.
 */
typedef void* wyn_window_t;

/**
 * @brief Handle to a Display.
 */
typedef void* wyn_display_t;

/**
 * @brief Callback function for enumerating displays.
 * @param[in] userdata [nullable] Pointer specified when calling `wyn_enumerate_displays`.
 * @param[in] display  [non-null] Handle to a Display. This handle is only valid during this callback, and no longer.
 * @return `true` to continue iterating, `false` to stop iterating.
 */
typedef wyn_bool_t (*wyn_display_callback)(void* userdata, wyn_display_t display);

/**
 * @brief Floating-point type for coordinates/extents/deltas.
 */
typedef double wyn_coord_t;

/**
 * @brief A 2D Point.
 */
struct wyn_point_t { wyn_coord_t x, y; };
typedef struct wyn_point_t wyn_point_t;

/**
 * @brief A 2D Extent.
 */
struct wyn_extent_t { wyn_coord_t w, h; };
typedef struct wyn_extent_t wyn_extent_t;

/**
 * @brief A 2D Rectangle.
 */
struct wyn_rect_t { wyn_point_t origin; wyn_extent_t extent; };
typedef struct wyn_rect_t wyn_rect_t;

/**
 * @brief A character type capable of holding UTF-8 code units.
 */
typedef unsigned char wyn_utf8_t;

/**
 * @brief Creates a UTF-8 encoded string literal, if possible.
 */
#if defined(__cplusplus)
    #if (__cplusplus >= 201103L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 201103L)) 
        #define WYN_UTF8(str) reinterpret_cast<const wyn_utf8_t*>(u8 ## str)
    #else
        #define WYN_UTF8(str) reinterpret_cast<const wyn_utf8_t*>(str)
    #endif
#elif defined(__STDC_VERSION__)
    #if (__STDC_VERSION__ >= 201112L)
        #define WYN_UTF8(str) ((const wyn_utf8_t*) u8 ## str)
    #else
        #define WYN_UTF8(str) ((const wyn_utf8_t*) str)
    #endif
#else
    #define WYN_UTF8(str) ((const wyn_utf8_t*) str)
#endif

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
extern wyn_bool_t wyn_quitting(void);

/**
 * @brief Queries whether or not the Event Loop is on the calling thread.
 * @return `true` if this thread is the Event Thread, `false` otherwise.
 * @note This function may be called from any thread.
 */
extern wyn_bool_t wyn_is_this_thread(void);

/**
 * @brief Wakes up the Event Thread and calls the `wyn_on_signal` user-callback on that thread.
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
 * @return The scale to convert from Screen Coordinates to Pixel Coordinates.
 * @note On most platforms, this value is always `1.0`. Some platforms (e.g. MacOS) may return other values, like `2.0`.
 */
extern wyn_coord_t wyn_window_scale(wyn_window_t window);

/**
 * @brief Queries the Position of a Window.
 * @param[in] window [non-null] A handle to the Window.
 * @return The content rectangle for the Window, in Screen Coordinates.
 */
extern wyn_rect_t wyn_window_position(wyn_window_t window);

/**
 * @brief Sets the Position of a Window.
 * @param[in] window [non-null] A handle to the Window.
 * @param[in] origin [nullable] The content origin, in Screen Coordinates.
 * @param[in] extent [nullable] The content extent, in Screen Coordinates.
 * @note If the origin/extent is NULL, the previous origin/extent is kept.
 * @note If the Window is Fullscreen, this call may be ignored.
 */
extern void wyn_window_reposition(wyn_window_t window, const wyn_point_t* origin, const wyn_extent_t* extent);

/**
 * @brief Queries a Window's Fullscreen status.
 * @param[in] window [non-null] A handle to the Window.
 * @return `true` if the Window is Fullscreen, `false` otherwise.
 */
extern wyn_bool_t wyn_window_is_fullscreen(wyn_window_t window);

/**
 * @brief Sets a Window's Fullscreen status.
 * @param[in] window [non-null] A handle to the Window.
 * @param     status `true` to enter Fullscreen, `false` to exit Fullscreen.
 */
extern void wyn_window_fullscreen(wyn_window_t window, wyn_bool_t status);

/**
 * @brief Sets the title of a Window.
 * @param[in] window [non-null] A handle to the Window.
 * @param[in] title  [nullable] A NULL-terminated UTF-8 encoded Text for the title, or NULL to reset the title.
 */
extern void wyn_window_retitle(wyn_window_t window, const wyn_utf8_t* title);

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Iterates over the currently available list of Displays.
 * @param[in] callback [nullable] The callback function to call for each Display.
                                  If NULL, the number of Displays is still calculated.
 * @param[in] userdata [nullable] The user-provided pointer to pass to the callback function. 
 * @return The number of Displays that were enumerated.
 */
extern unsigned int wyn_enumerate_displays(wyn_display_callback callback, void* userdata);

/**
 * @brief Queries the Position of a Display.
 * @return The rectangle for the Display, in Screen Coordinates.
 */
extern wyn_rect_t wyn_display_position(wyn_display_t display);

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
 * @details After this function returns, all remaining windows will be forcibly closed, without calling the user-callback.
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

/**
 * @brief Called when a Window's focus changes.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window that is about to close.
 * @param     focused  `true` if the Window gained focused, `false` otherwise.
 */
extern void wyn_on_window_focus(void* userdata, wyn_window_t window, wyn_bool_t focused);

/**
 * @brief Called when a Window is resized.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window that was resized.
 * @param     content  The new content rectangle, in Screen Coordinates.
 * @param     scale    The new scale.
 */
extern void wyn_on_window_reposition(void* userdata, wyn_window_t window, wyn_rect_t content, wyn_coord_t scale);

/**
 * @brief Called when the list of available Displays may have been changed.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 */
extern void wyn_on_display_change(void* userdata);

/**
 * @brief Called when a Cursor is moved across a Window.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window.
 * @param     sx       The new X-coordinate, relative to the Origin of the Window's content rectangle, in Screen Coordinates.
 * @param     sy       The new Y-coordinate, relative to the Origin of the Window's content rectangle, in Screen Coordinates.
 */
extern void wyn_on_cursor(void* userdata, wyn_window_t window, wyn_coord_t sx, wyn_coord_t sy);

/**
 * @brief Called when a Cursor is moved out of a Window.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window.
 */
extern void wyn_on_cursor_exit(void* userdata, wyn_window_t window);

/**
 * @brief Called when a Scroll input occurs on a Window.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window.
 * @param     dx       The normalized horizontal scroll delta.
 * @param     dy       The normalized vertical scroll delta.
 */
extern void wyn_on_scroll(void* userdata, wyn_window_t window, wyn_coord_t dx, wyn_coord_t dy);

/**
 * @brief Called when a Mouse Button is pressed/released on a Window.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window.
 * @param     button   The virtual code representing the Button.
 * @param     pressed  `true` if the button was pressed, `false` if the button was released.
 * @note Different platforms use different Virtual Codes to represent different Mouse Buttons.
 */
extern void wyn_on_mouse(void* userdata, wyn_window_t window, wyn_button_t button, wyn_bool_t pressed);

/**
 * @brief Called when a Key is pressed/released on a Window.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window.
 * @param     keycode  The virtual code representing the Key.
 * @param     pressed  `true` if the key was pressed, `false` if the key was released.
 * @note Different platforms use different Virtual Codes to represent different Keys.
 */
extern void wyn_on_keyboard(void* userdata, wyn_window_t window, wyn_keycode_t keycode, wyn_bool_t pressed);

/**
 * @brief Called when a Text is input on a Window.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @param[in] window   [non-null] A handle to the Window.
 * @param[in] text     [non-null] The NULL-terminated UTF-8 encoded text.
 */
extern void wyn_on_text(void* userdata, wyn_window_t window, const wyn_utf8_t* text);

#ifdef __cplusplus
}
#endif

// ================================================================================================================================

#endif /* WYN_H */
