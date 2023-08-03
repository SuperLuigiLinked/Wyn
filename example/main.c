/*
    Example - main.c
*/

#include <stdio.h>
#include <stdlib.h>

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

static void test_tid_2(void* arg)
{
    const wyt_tid_t tid = wyt_current_tid();
    const wyt_tid_t mid = *(const wyt_tid_t*)arg;
    fprintf(stderr, "[THRD] %llu\n", tid);
    fprintf(stderr, "[MAIN] %llu\n", mid);
}

static void test_tid(void)
{
    wyt_tid_t tid = wyt_current_tid();
    fprintf(stderr, "[MAIN] %llu\n", tid);

    const wyt_thread_t thread = wyt_spawn(test_tid_2, &tid);
    if (!thread) abort();
    wyt_join(thread);
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
    const wyt_time_t tp1 = wyt_nanotime();
    test_tid();
    const wyt_time_t tp2 = wyt_nanotime();
    const wyt_duration_t elapsed = (wyt_duration_t)(tp2 - tp1);
    fprintf(stderr, "[time] %lld ns\n", elapsed);
    return 0;

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
