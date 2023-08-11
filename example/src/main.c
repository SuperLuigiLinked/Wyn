/**
 * @file example/main.c
 */

#include "App.h"

// ================================================================================================================================

[[maybe_unused]] extern void test_cpp(void);

// ================================================================================================================================

extern void wyn_on_start(void* userdata)
{
    LOG("[WYN] <START>\n");

    struct AppState* state = (struct AppState*)userdata;
    ASSERT(state != NULL);

    state->window = wyn_open_window();
    if (!state->window) { wyn_quit(); return; }
    wyn_show_window(state->window);

    state->thread = wyt_spawn(gfx_func, userdata);
    if (!state->thread) { wyn_quit(); return; }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_stop(void* userdata)
{
    LOG("[WYN] <STOP>\n");

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

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_window_close(void* userdata, wyn_window_t window)
{
    LOG("[WYN] <CLOSE>\n");

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
    struct AppState state = {
        .thread = NULL,
        .window = NULL,
    };

    LOG("[START]\n");
    wyn_run(&state);
    LOG("[STOP]\n");

    return 0;
}

// ================================================================================================================================
