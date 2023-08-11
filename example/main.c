/**
 * @file example/main.c
 */

#include <stdio.h>
#include <stdlib.h>

#include <wyn.h>
#include <wyt.h>

// ================================================================================================================================

#define ASSERT(expr) if (expr) {} else abort()

#define LOG(...) (void)fprintf(stderr, __VA_ARGS__)

[[maybe_unused]] extern void test_cpp(void);

// ================================================================================================================================

struct AppState
{
    wyt_thread_t thread;
    wyn_window_t window;
};

// ================================================================================================================================

[[maybe_unused]]
static void quit_async(void*)
{
    wyn_quit();
}

static wyt_retval_t WYT_ENTRY thread_func(void* arg)
{
    [[maybe_unused]] struct AppState* const state = arg;

    //wyn_execute_async(quit_async, NULL);
    return 0;
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
    {
        struct AppState state = {
            .thread = NULL,
            .window = NULL,
        };

        LOG("[START]\n");
        wyn_run(&state);
        LOG("[STOP]\n");
    }
    LOG("\n");
    {
        struct AppState state = {
            .thread = NULL,
            .window = NULL,
        };

        LOG("[START]\n");
        wyn_run(&state);
        LOG("[STOP]\n");
    }
    return 0;
}

// ================================================================================================================================
