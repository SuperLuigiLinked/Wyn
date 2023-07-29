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
    void* userdata;

    HINSTANCE hInstance;
    ATOM atom;
};

static struct wyn_events_t g_events = {};

// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(void* userdata);
static void wyn_terminate(void);

static int wyn_run_native(void);
static LRESULT CALLBACK wyn_wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// ================================================================================================================================ //
// Public Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

extern int wyn_run(void* userdata)
{
    int code = EXIT_FAILURE;

    if (wyn_init(userdata))
    {
        wyn_on_start(userdata);
        code = wyn_run_native();
        wyn_on_stop(userdata);
    }
    wyn_terminate();

    return code;
}

extern void wyn_quit(int code)
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

static bool wyn_init(void* userdata)
{
    g_events = (struct wyn_events_t){
        .userdata = userdata,
        .hInstance = NULL,
        .atom = 0,
    };
    
    g_events.hInstance = GetModuleHandleW(NULL);
    if (g_events.hInstance == 0) return false;

    const WNDCLASSEXW wndclass = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = wc_style,
        .lpfnWndProc = wyn_wndproc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = g_events.hInstance,
        .hIcon = LoadIcon(g_events.hInstance, IDI_APPLICATION),
        .hCursor = LoadCursor(g_events.hInstance, IDC_ARROW),
        .hbrBackground = 0,
        .lpszMenuName = 0,
        .lpszClassName = wc_name,
        .hIconSm = 0,
    };

    g_events.atom = RegisterClassExW(&wndclass);
    if (g_events.atom == 0) return false;

    return true;
}

static void wyn_terminate(void)
{
    if (g_events.atom != 0)
    {
        [[maybe_unused]] BOOL res = UnregisterClassW(wc_name, g_events.hInstance);
    }
}

// -------------------------------------------------------------------------------------------------------------------------------- //

static int wyn_run_native(void)
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
    const wyn_window_t window = (wyn_window_t)hWnd;

    switch (uMsg)
    {
        case WM_CLOSE:
        {
            wyn_on_window_close(g_events.userdata, window);
        }
        break;
    }

    const LRESULT res = DefWindowProcW(hWnd, uMsg, wParam, lParam);
    return res;
}

// -------------------------------------------------------------------------------------------------------------------------------- //

extern wyn_window_t wyn_open_window(void)
{
    const HWND hWnd = CreateWindowExW(
        ex_style, wc_name, L"", ws_style,
        0, 0, 640, 480,
        NULL, NULL, g_events.hInstance, NULL
    );

    return (wyn_window_t)hWnd;
}

extern void wyn_close_window(wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] const BOOL res = CloseWindow(hWnd);
}

extern void wyn_show_window(wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] const BOOL res = ShowWindow(hWnd, SW_SHOW);
}

extern void wyn_hide_window(wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] const BOOL res = ShowWindow(hWnd, SW_HIDE);
}

// ================================================================================================================================ //
