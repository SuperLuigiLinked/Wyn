/**
 * @file example/App.h
 */

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#include <wyt.h>
#include <wyn.h>

// ================================================================================================================================

#define ASSERT(expr) if (expr) {} else abort()

#define LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// ================================================================================================================================

struct AppState
{
    _Atomic(bool) quit_flag;

    wyt_thread_t thread;
    wyn_window_t window;

};

// ================================================================================================================================

extern wyt_retval_t WYT_ENTRY app_gfx_func(void* arg);

// ================================================================================================================================

inline static void app_quit_async(void*)
{
    wyn_quit();
}

inline static void app_quit(struct AppState* const app)
{
    const bool quitting = atomic_exchange_explicit(&app->quit_flag, true, memory_order_relaxed);
    if (!quitting) wyn_execute_async(app_quit_async, NULL);
}

inline static bool app_quitting(const struct AppState* const app)
{
    return atomic_load_explicit(&app->quit_flag, memory_order_relaxed);
}

// ================================================================================================================================
