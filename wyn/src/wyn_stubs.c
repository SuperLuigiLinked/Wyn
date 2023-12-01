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

extern void __attribute__((weak)) wyn_on_window_resize(void* userdata, wyn_window_t window, wyn_coord_t pw, wyn_coord_t ph)
{ (void)userdata; (void)window; (void)pw; (void)ph; }

extern void __attribute__((weak)) wyn_on_cursor(void* userdata, wyn_window_t window, wyn_coord_t px, wyn_coord_t py)
{ (void)userdata; (void)window; (void)px; (void)py; }

extern void __attribute__((weak)) wyn_on_scroll(void* userdata, wyn_window_t window, double dx, double dy)
{ (void)userdata; (void)window; (void)dx; (void)dy; }

extern void __attribute__((weak)) wyn_on_mouse(void* userdata, wyn_window_t window, wyn_button_t button, bool pressed)
{ (void)userdata; (void)window; (void)button; (void)pressed; }

extern void __attribute__((weak)) wyn_on_keyboard(void* userdata, wyn_window_t window, wyn_keycode_t keycode, bool pressed)
{ (void)userdata; (void)window; (void)keycode; (void)pressed; }

extern void __attribute__((weak)) wyn_on_text(void* userdata, wyn_window_t window, const wyn_utf8_t* text)
{ (void)userdata; (void)window; (void)text; }

// ================================================================================================================================
