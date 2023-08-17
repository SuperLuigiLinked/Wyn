/**
 * @file wyn_stubs.c
 * @brief Stubs for Wyn callback functions.
 */

#include "wyn.h"

// ================================================================================================================================

#pragma weak wyn_on_start
#pragma weak wyn_on_stop
#pragma weak wyn_on_window_close_request

// ================================================================================================================================

extern void wyn_on_start(void* userdata)                                        { (void)userdata; }
extern void wyn_on_stop(void* userdata)                                         { (void)userdata; }
extern void wyn_on_window_close_request(void* userdata, wyn_window_t window)    { (void)userdata; wyn_close_window(window); wyn_quit(); }
extern void wyn_on_window_redraw(void* userdata, wyn_window_t window)           { (void)userdata; (void)window; }

// ================================================================================================================================
