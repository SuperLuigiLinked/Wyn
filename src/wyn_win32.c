/*
    Wyn - Win32.c
*/

#include "wyn.h"

#include <stdlib.h>
#include <stdio.h>

#include <Windows.h>

// ================================================================================================================================ //
// Macros
// -------------------------------------------------------------------------------------------------------------------------------- //

// #ifdef NDEBUG
// #   define WYN_ASSERT(expr) if (expr) {} else abort()
// #else
// #   define WYN_ASSERT(expr) if (expr) {} else FatalAppExitW(0, L"Assertion Failed!\n(" L###expr L")")
// #endif

// ================================================================================================================================ //
// Declarations
// -------------------------------------------------------------------------------------------------------------------------------- //

struct wyn_events_t
{
    HINSTANCE hInstance;
    ATOM atom;
};

static struct wyn_events_t g_events = {};

// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(struct wyn_events_t* events);
static void wyn_terminate(struct wyn_events_t* events);

static int wyn_run_native(struct wyn_events_t* events);
static LRESULT CALLBACK wyn_wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// ================================================================================================================================ //
// Public Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

extern int wyn_run(void)
{
    int code = EXIT_FAILURE;

    if (wyn_init(&g_events))
    {
        wyn_on_start(&g_events);
        code = wyn_run_native(&g_events);
        wyn_on_stop(&g_events);
    }
    wyn_terminate(&g_events);

    return code;
}

extern void wyn_quit(struct wyn_events_t* events [[maybe_unused]], int code)
{
    PostQuitMessage(code);
}

// ================================================================================================================================ //
// Private Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

static const wchar_t* const wc_name = L"Wyn";
static const DWORD wc_style = CS_HREDRAW | CS_VREDRAW;
static const DWORD ws_style = WS_OVERLAPPEDWINDOW;
static const DWORD ex_style = WS_EX_NOREDIRECTIONBITMAP;

// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(struct wyn_events_t* events)
{
    *events = (struct wyn_events_t){};
    
    events->hInstance = GetModuleHandleW(NULL);
    if (events->hInstance == 0) return false;

    const WNDCLASSEXW wndclass = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = wc_style,
        .lpfnWndProc = wyn_wndproc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = events->hInstance,
        .hIcon = LoadIcon(events->hInstance, IDI_APPLICATION),
        .hCursor = LoadCursor(events->hInstance, IDC_ARROW),
        .hbrBackground = 0,
        .lpszMenuName = 0,
        .lpszClassName = wc_name,
        .hIconSm = 0,
    };

    events->atom = RegisterClassExW(&wndclass);
    if (events->atom == 0) return false;

    return true;
}

static void wyn_terminate(struct wyn_events_t* events)
{
    if (events->atom != 0)
    {
        [[maybe_unused]] BOOL res = UnregisterClassW(wc_name, events->hInstance);
    }
}

// -------------------------------------------------------------------------------------------------------------------------------- //

static int wyn_run_native(struct wyn_events_t* events [[maybe_unused]])
{
    for (;;)
    {
        MSG msg = {};
        BOOL res = GetMessageW(&msg, 0, 0, 0);
        if (res == -1) return -1;
        if (res == 0) return (int)(msg.wParam);

        [[maybe_unused]] BOOL res1 = TranslateMessage(&msg);
        [[maybe_unused]] LRESULT res2 = DispatchMessageW(&msg);
    }
}

static LRESULT CALLBACK wyn_wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const wyn_window_t window = (wyn_window_t)(hWnd);

    switch (uMsg)
    {
        case WM_CLOSE:
        {
            wyn_on_window_close(&g_events, window);
        }
        break;
    }

    const LRESULT res = DefWindowProcW(hWnd, uMsg, wParam, lParam);
    return res;
}

// -------------------------------------------------------------------------------------------------------------------------------- //

extern wyn_window_t wyn_open_window(struct wyn_events_t* events)
{
    const HWND hWnd = CreateWindowExW(
        ex_style, wc_name, L"", ws_style,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, events->hInstance, NULL
    );

    return (wyn_window_t)(hWnd);
}

extern void wyn_close_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] BOOL res = CloseWindow(hWnd);
}

extern void wyn_show_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] BOOL res = ShowWindow(hWnd, SW_SHOW);
}

extern void wyn_hide_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] BOOL res = ShowWindow(hWnd, SW_HIDE);
}

// ================================================================================================================================ //
