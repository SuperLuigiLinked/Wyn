/**
 * @file example.c
 */

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <wyt.h>
#include <wyn.h>

#ifdef WYN_EXAMPLE_GL
#   include <wyg_gl.h>
#endif

// ================================================================================================================================

#define ASSERT(expr) if (expr) {} else abort()
#define LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// --------------------------------------------------------------------------------------------------------------------------------

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// ================================================================================================================================

enum : wyt_time_t { nanos_per_second = 1'000'000'000 };
enum : wyt_time_t { frames_per_second = 60 };

// ================================================================================================================================

typedef struct App App;
typedef struct Logic Logic;
typedef struct Render Render;
typedef struct Events Events;
typedef struct Debug Debug;

// --------------------------------------------------------------------------------------------------------------------------------

struct Logic
{
    size_t frame;
    wyt_time_t last_tick;
};

// --------------------------------------------------------------------------------------------------------------------------------

struct Render
{
    size_t frame;
#ifdef WYN_EXAMPLE_GL

#endif
};

// --------------------------------------------------------------------------------------------------------------------------------

struct Events
{
    wyt_thread_t thread;
    wyn_window_t window;
};

// --------------------------------------------------------------------------------------------------------------------------------

struct Debug
{
    wyt_time_t update_ts;
    wyt_time_t update_te;
    wyt_time_t update_el;

    wyt_time_t render_ts;
    wyt_time_t render_te;
    wyt_time_t render_el;
};

// --------------------------------------------------------------------------------------------------------------------------------

struct App
{
    atomic_bool quit_flag;
    wyt_time_t epoch;

    Events events;
    Logic logic;
    Render render;
    Debug debug;
};

// ================================================================================================================================

static void app_quit_callback(void*)
{
    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

static void app_quit(App* const app)
{
    const bool quitting = atomic_exchange_explicit(&app->quit_flag, true, memory_order_relaxed);
    if (!quitting) wyn_execute_async(app_quit_callback, NULL);
}

// --------------------------------------------------------------------------------------------------------------------------------

static bool app_quitting(const App* const app)
{
    return atomic_load_explicit(&app->quit_flag, memory_order_relaxed);
}

// ================================================================================================================================

static void logic_vsync(App* const app)
{
    const wyt_time_t last_nanos = app->logic.last_tick - app->epoch;
    const wyt_time_t last_frames = wyt_scale(last_nanos, frames_per_second, nanos_per_second);
    const wyt_time_t next_frames = last_frames + 1;
    const wyt_time_t next_nanos = wyt_scale(next_frames, nanos_per_second, frames_per_second);
    const wyt_time_t next_tick = app->epoch + next_nanos;
    wyt_nanosleep_until(next_tick);
    app->logic.last_tick = wyt_nanotime();
}

// --------------------------------------------------------------------------------------------------------------------------------

static wyt_retval_t WYT_ENTRY logic_thread(void* arg)
{
    App* const app = arg;
    ASSERT(app != NULL);

    while (!app_quitting(app))
    {
        logic_vsync(app);
        
        ++app->logic.frame;
        
        LOG("FRAME [%zu] %llu\n", app->logic.frame, app->logic.last_tick - app->epoch);
    }

    return 0;
}

// ================================================================================================================================

extern void wyn_on_start(void* userdata)
{
    LOG("[WYN] <START>\n");

    App* const app = (App*)userdata;
    ASSERT(app != NULL);

    *app = (App){ .epoch = wyt_nanotime() };

    app->events.window = wyn_open_window();
    if (!app->events.window) { wyn_quit(); return; }
    wyn_show_window(app->events.window);

    app->events.thread = wyt_spawn(logic_thread, userdata);
    if (!app->events.thread) { wyn_quit(); return; }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_stop(void* userdata)
{
    LOG("[WYN] <STOP>\n");

    App* const app = (App*)userdata;
    ASSERT(app != NULL);

    app_quit(app);

    if (app->events.thread)
    {
        wyt_join(app->events.thread);
        app->events.thread = NULL;
    }
    
    if (app->events.window)
    {
        wyn_close_window(app->events.window);
        app->events.window = NULL;
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_window_close(void* userdata, wyn_window_t window)
{
    LOG("[WYN] <CLOSE>\n");

    App* const app = (App*)userdata;
    ASSERT(app != NULL);

    if (window == app->events.window)
    {
        app_quit(app);

        if (app->events.thread)
        {
            wyt_join(app->events.thread);
            app->events.thread = NULL;
        }

        app->events.window = NULL;
    }    
}

// ================================================================================================================================

#if defined(_WIN32) && 0
int WINAPI wWinMain
(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
)
#else
int main(void)
#endif
{
    LOG("[START]\n");
    
    App app;
    wyn_run(&app);

    LOG("[STOP]\n");
    return 0;
}

// ================================================================================================================================
