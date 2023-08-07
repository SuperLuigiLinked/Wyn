/**
 * @file wyn_stubs.c
 * @brief Stubs for Wyn callback functions.
 */

#include "wyn.h"

// ================================================================================================================================

#pragma weak wyn_on_start
#pragma weak wyn_on_stop
#pragma weak wyn_on_window_close

// ================================================================================================================================

extern void wyn_on_start(void* userdata)                                { (void)userdata; }
extern void wyn_on_stop(void* userdata)                                 { (void)userdata; }
extern void wyn_on_window_close(void* userdata, wyn_window_t window)    { (void)userdata; (void)window; wyn_quit(); }

// ================================================================================================================================
