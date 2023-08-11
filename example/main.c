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

#define LOG(...) (void)fprintf(stderr, __VA_ARGS__)

[[maybe_unused]] extern void test_cpp(void);

// ================================================================================================================================

static void print_ids(void)
{
    const wyt_pid_t pid = wyt_pid();
    const wyt_tid_t tid = wyt_tid();
    LOG("[PID] %llu | [TID] %llu\n", (unsigned long long)pid, (unsigned long long)tid);
}

static wyt_retval_t WYT_ENTRY thread_func(void* arg [[maybe_unused]])
{
    print_ids();
    return 0;
}

static void test_threads(void)
{
    print_ids();

    const wyt_time_t t1 = wyt_nanotime();
    const wyt_thread_t thread = wyt_spawn(thread_func, NULL);
    const wyt_time_t t2 = wyt_nanotime();

    const wyt_time_t t3 = wyt_nanotime();
    if (thread) wyt_join(thread);
    const wyt_time_t t4 = wyt_nanotime();

    const wyt_duration_t e1 = (wyt_duration_t)(t2 - t1);
    const wyt_duration_t e2 = (wyt_duration_t)(t4 - t3);
    LOG("[E1] %lld ns\n", e1);
    LOG("[E2] %lld ns\n", e2);
}

// ================================================================================================================================

[[maybe_unused]]
static void quit_async(void*)
{
    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_start(void* userdata)
{
    fputs("[WYN] <START>\n", stderr);

    struct AppState* state = (struct AppState*)userdata;
    ASSERT(state != NULL);

    state->window = wyn_open_window();
    if (!state->window) { wyn_quit(); return; }
    wyn_show_window(state->window);

    // state->thread = wyt_spawn(thread_func, userdata);
    // if (!state->thread) { wyn_quit(); return; }
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
    test_threads();
    return 0;

    // struct AppState state = {
    //     .thread = NULL,
    //     .window = NULL,
    // };

    // fputs("[START]\n", stderr);
    // wyn_run(&state);
    // fputs("[STOP]\n", stderr);

    // return 0;
}

// ================================================================================================================================
