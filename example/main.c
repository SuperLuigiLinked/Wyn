/*
    Wyn - Example.c
*/

#include <wyn.h>

#include <stdio.h>

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
    return wyn_run();
}

// ================================================================================================================================ //

extern void wyn_on_start(struct wyn_events_t* events)
{
    puts("[WYN - START]");
    const wyn_window_t window = wyn_open_window(events);
    if (!window) wyn_quit(events, 1);
    wyn_show_window(events, window);
}

extern void wyn_on_stop(struct wyn_events_t* events [[maybe_unused]])
{
    puts("[WYN - STOP]");
}

extern void wyn_on_window_close(struct wyn_events_t* events, wyn_window_t window)
{
    puts("[WYN - CLOSE]");
    wyn_quit(events, 0);
}

// ================================================================================================================================ //
