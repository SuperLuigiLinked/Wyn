/*
    Example - main.c
*/

#include <stdio.h>

#include <wyn.h>
#include <wyt.h>

// ================================================================================================================================ //

[[maybe_unused]]
static void test_time(void)
{
    {
        const wyt_time_t tp1 = wyt_nanotime();
        const wyt_time_t tp2 = wyt_nanotime();
        const wyt_duration_t elapsed = (wyt_duration_t)(tp2 - tp1);
        fprintf(stderr, "%lld\n", elapsed);
    }

    {
        const wyt_time_t tp1 = wyt_nanotime();
        wyt_yield();
        const wyt_time_t tp2 = wyt_nanotime();
        const wyt_duration_t elapsed = (wyt_duration_t)(tp2 - tp1);
        fprintf(stderr, "%lld\n", elapsed);
    }

    {
        const wyt_time_t tp1 = wyt_nanotime();
        extern void cpp_yield(void); cpp_yield();
        const wyt_time_t tp2 = wyt_nanotime();
        const wyt_duration_t elapsed = (wyt_duration_t)(tp2 - tp1);
        fprintf(stderr, "%lld\n", elapsed);
    }
}

[[maybe_unused]]
static void test_sleep(void)
{
    const wyt_duration_t dur = 500;
    //const wyt_duration_t dur = 1'000'000'000;
    fprintf(stderr, "Expected: %10lld | %+10lld\n", dur, 0LL);

    {
        const wyt_time_t tp1 = wyt_nanotime();
        wyt_nanosleep_for(dur);
        const wyt_time_t tp2 = wyt_nanotime();
        const wyt_duration_t elapsed = (wyt_duration_t)(tp2 - tp1);
        const wyt_time_t expected = tp1 + (wyt_time_t)dur;
        const wyt_duration_t diff = (wyt_duration_t)(tp2 - expected);
        fprintf(stderr, "Sleep F.: %10lld | %+10lld\n", elapsed, diff);
    }
    
    {
        const wyt_time_t tp1 = wyt_nanotime();
        const wyt_time_t expected = tp1 + (wyt_time_t)dur;
        wyt_nanosleep_until(expected);
        const wyt_time_t tp2 = wyt_nanotime();
        const wyt_duration_t elapsed = (wyt_duration_t)(tp2 - tp1);
        const wyt_duration_t diff = (wyt_duration_t)(tp2 - expected);
        fprintf(stderr, "Sleep U.: %10lld | %+10lld\n", elapsed, diff);
    }
}

[[maybe_unused]]
static void thread_func(void* arg [[maybe_unused]])
{
    fputs("[THREAD] HELLO!\n", stderr);
    wyt_nanosleep_for(2'000'000'000uLL);
    fputs("[THREAD] GOODBYE!\n", stderr);
}

[[maybe_unused]]
static void test_spawn(void) 
{
    fputs("[main] spawning\n", stderr);
    const wyt_time_t tp1 = wyt_nanotime();
    const wyt_thread_t thread = wyt_spawn(thread_func, NULL);
    const wyt_time_t tp2 = wyt_nanotime();
    const wyt_duration_t elapsed = (wyt_duration_t)(tp2 - tp1);
    fprintf(stderr, "SPAWN: %lld\n", elapsed);
    if (!thread) return;

    fputs("[main] joining\n", stderr);
    wyt_join(thread);
    fputs("[main] terminating\n", stderr);
}

[[maybe_unused]]
static void test_frames(void)
{
    const wyt_time_t epoch = wyt_nanotime();
    for (size_t i = 0; i < 5'000'000; ++i)
    {
        const wyt_time_t next = epoch + 16'666'666uLL * i;
        wyt_nanosleep_until(next);
        volatile const wyt_time_t tp = wyt_nanotime();
        volatile const wyt_duration_t elapsed = (wyt_duration_t)(tp - epoch);
        volatile const double millis = (double)elapsed / 1'000'000.0;
        volatile const double frames = (double)elapsed / (1'000'000'000.0 / 60.0);
        volatile const signed diff = (signed)frames - (signed)i;
        fprintf(stderr, "[%7zu]: %f <%d> | %f | %10llu\n", i, frames, diff, millis, elapsed);
        //if (millis >= 8.0) break;
    }
}

[[maybe_unused]]
extern void test_cpp(void);

// int WINAPI wWinMain
// (
//     _In_ HINSTANCE hInstance,
//     _In_opt_ HINSTANCE hPrevInstance,
//     _In_ LPWSTR lpCmdLine,
//     _In_ int nShowCmd
// )
int main(void)
{
    test_time();
    //test_sleep();
    //test_spawn();
    //test_frames();
    //test_cpp();
    return 0;

    // wyt_thread_t thread;

    // fputs("[START]\n", stderr);
    // const int code = wyn_run(&thread);
    // fputs("[STOP]\n", stderr);

    // return code;
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
    wyt_join(*thread);
}

extern void wyn_on_window_close(void* userdata, wyn_window_t window)
{
    (void)userdata; (void)window;

    fputs("[WYN - CLOSE]\n", stderr);
    
    wyn_quit(0);
}

// ================================================================================================================================ //
