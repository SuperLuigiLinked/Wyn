/**
 * @file example/App.h
 */

#include <stdio.h>
#include <stdlib.h>

#include <wyt.h>
#include <wyn.h>

// ================================================================================================================================

#define ASSERT(expr) if (expr) {} else abort()

#define LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// ================================================================================================================================

struct AppState
{
    wyt_thread_t thread;
    wyn_window_t window;
};

// ================================================================================================================================

extern wyt_retval_t WYT_ENTRY gfx_func(void* arg);

// ================================================================================================================================