/**
 * @file wyn_stubs.c
 * @brief Stubs for Wyn callback functions.
 */

#include "wyn.h"

// ================================================================================================================================

extern void __attribute__((weak)) wyn_on_start(void* userdata)
{ (void)userdata; }

extern void __attribute__((weak)) wyn_on_stop(void* userdata)
{ (void)userdata; }

extern void __attribute__((weak)) wyn_on_signal(void* userdata)
{ (void)userdata; }

extern void __attribute__((weak)) wyn_on_window_close(void* userdata, wyn_window_t window)
{ (void)userdata; wyn_window_close(window); wyn_quit(); }

extern void __attribute__((weak)) wyn_on_window_redraw(void* userdata, wyn_window_t window)
{ (void)userdata; (void)window; }

extern void __attribute__((weak)) wyn_on_window_reposition(void* userdata, wyn_window_t window, wyn_rect_t content, wyn_coord_t scale)
{ (void)userdata; (void)window; (void)content; (void)scale; }

extern void __attribute__((weak)) wyn_on_cursor(void* userdata, wyn_window_t window, wyn_coord_t sx, wyn_coord_t sy)
{ (void)userdata; (void)window; (void)sx; (void)sy; }

extern void __attribute__((weak)) wyn_on_scroll(void* userdata, wyn_window_t window, wyn_coord_t dx, wyn_coord_t dy)
{ (void)userdata; (void)window; (void)dx; (void)dy; }

extern void __attribute__((weak)) wyn_on_mouse(void* userdata, wyn_window_t window, wyn_button_t button, wyn_bool_t pressed)
{ (void)userdata; (void)window; (void)button; (void)pressed; }

extern void __attribute__((weak)) wyn_on_keyboard(void* userdata, wyn_window_t window, wyn_keycode_t keycode, wyn_bool_t pressed)
{ (void)userdata; (void)window; (void)keycode; (void)pressed; }

extern void __attribute__((weak)) wyn_on_text(void* userdata, wyn_window_t window, const wyn_utf8_t* text)
{ (void)userdata; (void)window; (void)text; }

// ================================================================================================================================
