/**
 * @file example/main.c
 */

#include <stdio.h>
#include <stdlib.h>

#include <wyn.h>
#include <wyt.h>

// ================================================================================================================================

struct AppState
{
    wyt_thread_t thread;
    wyn_window_t window;
};

#define ASSERT(expr) if (expr) {} else abort()

[[maybe_unused]] extern void test_cpp(void);

// ================================================================================================================================

#if 0
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
    struct AppState state = {
        .thread = NULL,
        .window = NULL,
    };

    fputs("[START]\n", stderr);
    wyn_run(&state);
    fputs("[STOP]\n", stderr);

    return 0;
}

// ================================================================================================================================

static void print_tid(void* label)
{
    const wyt_time_t tp = wyt_nanotime();

    const wyt_tid_t tid = wyt_current_tid();
    fprintf(stderr, "[TID %s] %llu\t\t(%llu)\n", (const char*)label, tid, tp);
}

[[maybe_unused]]
static void quit_async(void*)
{
    wyn_quit();
}

static void thread_func(void* arg)
{
    struct AppState* state = (struct AppState*)arg;
    ASSERT(state != NULL);

    print_tid("THREAD 1");
    wyn_execute(print_tid, "THREAD 2");
    wyn_execute_async(print_tid, "THREAD 3");

    //wyn_execute_async(quit_async, NULL);
}

// ================================================================================================================================

extern void wyn_on_start(void* userdata)
{
    fputs("[WYN] <START>\n", stderr);

    struct AppState* state = (struct AppState*)userdata;
    ASSERT(state != NULL);

    state->window = wyn_open_window();
    if (!state->window) { wyn_quit(); return; }
    wyn_show_window(state->window);

    print_tid("MAIN 1");
    wyn_execute(print_tid, "MAIN 2");
    wyn_execute_async(print_tid, "MAIN 3");

    state->thread = wyt_spawn(thread_func, userdata);
    if (!state->thread) { wyn_quit(); return; }
}

extern void wyn_on_stop(void* userdata)
{
    fputs("[WYN] <STOP>\n", stderr);

    struct AppState* state = (struct AppState*)userdata;
    ASSERT(state != NULL);

    if (state->thread)
    {
        wyt_join(state->thread);
        state->thread = NULL;
    }
    
    if (state->window)
    {
        wyn_close_window(state->window);
        state->window = NULL;
    }
}

extern void wyn_on_window_close(void* userdata, wyn_window_t window)
{
    fputs("[WYN] <CLOSE>\n", stderr);

    struct AppState* state = (struct AppState*)userdata;
    ASSERT(state != NULL);

    if (state->window == window)
    {
        wyn_quit();
        state->window = NULL;
    }    
}

// ================================================================================================================================
