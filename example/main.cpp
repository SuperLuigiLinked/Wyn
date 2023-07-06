/*
    Wyn - Example.c
*/

#include <wyn.h>

#include <stdio.h>

#include <thread>
#include <chrono>

// ================================================================================================================================ //

static std::thread cpp_thread{};

void thread_func()
{    
    puts("Hello from C++ Thread!");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    puts("Goodbye from C++ Thread!");
}

// int WINAPI wWinMain
// (
//     _In_ HINSTANCE hInstance,
//     _In_opt_ HINSTANCE hPrevInstance,
//     _In_ LPWSTR lpCmdLine,
//     _In_ int nShowCmd
// )
int main(void)
{
    fputs("[START]\n", stderr);
    const int code = wyn_run();
    fputs("[STOP]\n", stderr);

    return code;
}

// ================================================================================================================================ //

WYN_API void wyn_on_start(struct wyn_events_t* events)
{
    fputs("[WYN - START]\n", stderr);

    const wyn_window_t window = wyn_open_window(events);
    if (!window) return wyn_quit(events, 1);

    wyn_show_window(events, window);

    cpp_thread = std::thread(thread_func);
}

WYN_API void wyn_on_stop(struct wyn_events_t* events)
{
    fputs("[WYN - STOP]\n", stderr);

    cpp_thread.join();
}

WYN_API void wyn_on_window_close(struct wyn_events_t* events, wyn_window_t window)
{
    fputs("[WYN - CLOSE]\n", stderr);
    
    wyn_quit(events, 0);
}

// ================================================================================================================================ //
