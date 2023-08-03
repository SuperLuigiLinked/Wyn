/*
    Example - main.c
*/

#include <stdio.h>

#include <wyn.h>
#include <wyt.h>

// ================================================================================================================================ //

[[maybe_unused]]
extern void test_cpp(void);

static void thread_func(void* arg [[maybe_unused]])
{
    fputs("[THREAD] HELLO!\n", stderr);
    wyt_nanosleep_for(5'000'000'000uLL);
    fputs("[THREAD] GOODBYE!\n", stderr);
}

// ================================================================================================================================ //

// int WINAPI wWinMain
// (
//     _In_ HINSTANCE hInstance,
//     _In_opt_ HINSTANCE hPrevInstance,
//     _In_ LPWSTR lpCmdLine,
//     _In_ int nShowCmd
// )
int main(void)
{
    wyt_thread_t thread = NULL;

    fputs("[START]\n", stderr);
    const int code = wyn_run(&thread);
    fputs("[STOP]\n", stderr);

    return code;
}

// ================================================================================================================================ //

extern void wyn_on_start(void* userdata)
{
    (void)userdata;

    fputs("[WYN - START]\n", stderr);

    const wyn_window_t window = wyn_open_window();
    if (!window) { wyn_quit(1); return; }

    wyn_show_window(window);

    wyt_thread_t* thread = (wyt_thread_t*)userdata;
    *thread = wyt_spawn(thread_func, NULL);
}

extern void wyn_on_stop(void* userdata)
{
    (void)userdata;

    fputs("[WYN - STOP]\n", stderr);

    wyt_thread_t* thread = (wyt_thread_t*)userdata;
    if (*thread != NULL) wyt_join(*thread);
}

extern void wyn_on_window_close(void* userdata, wyn_window_t window)
{
    (void)userdata; (void)window;

    fputs("[WYN - CLOSE]\n", stderr);
    
    wyn_quit(0);
}

// ================================================================================================================================ //
