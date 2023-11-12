/**
 * @file wyn_stubs.c
 * @brief Stubs for Wyn callback functions.
 */

#include "wyn.h"

// ================================================================================================================================

extern void __attribute__((weak)) wyn_on_start(void* userdata)                              { (void)userdata; }
extern void __attribute__((weak)) wyn_on_stop(void* userdata)                               { (void)userdata; }
extern void __attribute__((weak)) wyn_on_signal(void* userdata)                             { (void)userdata; }
extern void __attribute__((weak)) wyn_on_window_close(void* userdata, wyn_window_t window)  { (void)userdata; wyn_window_close(window); wyn_quit(); }
extern void __attribute__((weak)) wyn_on_window_redraw(void* userdata, wyn_window_t window) { (void)userdata; (void)window; }

// ================================================================================================================================
