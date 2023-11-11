/**
 * @file wyn_win32.c
 * @brief Implementation of Wyn for the Win32 backend.
 */

#include "wyn.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#include <Windows.h>

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef _VC_NODEFAULTLIB
    /**
     * @see https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-fatalexit
     */
    #define WYN_ASSERT(expr) if (expr) {} else FatalExit(1)
#else
    /**
     * @see https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort
     */
    #define WYN_ASSERT(expr) if (expr) {} else abort()
#endif

/**
 * @see https://en.cppreference.com/w/c/io/fprintf
 */
#define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// --------------------------------------------------------------------------------------------------------------------------------

#define WYN_MSG_CLASS L"Wyn-Msg"
#define WYN_WND_CLASS L"Wyn-Wnd"
#define WYN_CS_STYLE (CS_HREDRAW | CS_VREDRAW)
#define WYN_EX_STYLE (0)
#define WYN_WS_STYLE (WS_OVERLAPPEDWINDOW | (WS_CLIPCHILDREN | WS_CLIPSIBLINGS))

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Internal structure for holding Wyn state.
 */
struct wyn_state_t
{
    void* userdata; ///< The pointer provided by the user when the Event Loop was started.
    _Atomic(bool) quitting; ///< Flag to indicate the Event Loop is quitting.
    
    HINSTANCE hinstance; ///< HINSTANCE for the application.
    HWND msg_hwnd; ///< Message-only Window for sending messages.
    ATOM msg_atom; ///< Atom for the message-only Window.
    ATOM wnd_atom; ///< Atom for regular Windows.

    DWORD tid_main; ///< Thread ID of the Main Thread.
};

/**
 * @brief Static instance of all Wyn state.
 */
static struct wyn_state_t wyn_state;

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes all Wyn state.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @return `true` if successful, `false` if there were errors.
 */
static bool wyn_reinit(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_deinit(void);

/**
 * @brief Destroys all remaining windows, without notifying the user.
 */
static void wyn_destroy_windows(void);

/**
 * @brief Callback function for destroying windows.
 */
static BOOL CALLBACK wyn_destroy_windows_callback(HWND hwnd, LPARAM lparam);

/**
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_run_native(void);

/**
 * @brief WndProc for the message-only Window.
 */
static LRESULT CALLBACK wyn_msgproc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);

/**
 * @brief WndProc for regular Windows.
 */
static LRESULT CALLBACK wyn_wndproc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);


// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
 * @see https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe
 * @see https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandlew
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassexw
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw
 */
static bool wyn_reinit(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .quitting = false,
        .tid_main = 0,
        .hinstance = NULL,
        .msg_hwnd = NULL,
        .msg_atom = 0,
        .wnd_atom = 0,
    };
    
    {
        wyn_state.tid_main = GetCurrentThreadId();
    }
    
    {
        wyn_state.hinstance = GetModuleHandleW(NULL);
        if (wyn_state.hinstance == 0) return false;
    }

    {
        const HICON icon = LoadIcon(NULL, IDI_APPLICATION);
        const HCURSOR cursor = LoadCursor(NULL, IDC_ARROW);

        const WNDCLASSEXW wnd_class = {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = WYN_CS_STYLE,
            .lpfnWndProc = wyn_wndproc,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = wyn_state.hinstance,
            .hIcon = icon,
            .hCursor = cursor,
            .hbrBackground = NULL,
            .lpszMenuName = NULL,
            .lpszClassName = WYN_WND_CLASS,
            .hIconSm = NULL,
        };
        wyn_state.wnd_atom = RegisterClassExW(&wnd_class);
        if (wyn_state.wnd_atom == 0) return false;
    }

    {
        const WNDCLASSEXW msg_class = {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = 0,
            .lpfnWndProc = wyn_msgproc,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = wyn_state.hinstance,
            .hIcon = NULL,
            .hCursor = NULL,
            .hbrBackground = NULL,
            .lpszMenuName = NULL,
            .lpszClassName = WYN_MSG_CLASS,
            .hIconSm = NULL,
        };
        wyn_state.msg_atom = RegisterClassExW(&msg_class);
        if (wyn_state.msg_atom == 0) return false;
    }

    {
        wyn_state.msg_hwnd = CreateWindowExW(
            0, WYN_MSG_CLASS, L"", 0,
            0, 0, 0, 0,
            HWND_MESSAGE, NULL, wyn_state.hinstance, NULL
        );
        if (wyn_state.msg_hwnd == NULL) return false;
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unregisterclassw
 */
static void wyn_deinit(void)
{
    wyn_destroy_windows();

    if (wyn_state.msg_hwnd != NULL)
    {
        [[maybe_unused]] const BOOL res = DestroyWindow(wyn_state.msg_hwnd);
    }

    if (wyn_state.msg_atom != 0)
    {
        [[maybe_unused]] const BOOL res = UnregisterClassW(WYN_MSG_CLASS, wyn_state.hinstance);
    }

    if (wyn_state.wnd_atom != 0)
    {
        [[maybe_unused]] const BOOL res = UnregisterClassW(WYN_WND_CLASS, wyn_state.hinstance);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumthreadwindows
 */
static void wyn_destroy_windows(void)
{
    [[maybe_unused]] const BOOL res = EnumThreadWindows(wyn_state.tid_main, wyn_destroy_windows_callback, 0);
}

/**
 * @see https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/ms633496(v=vs.85)
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
 */
static BOOL CALLBACK wyn_destroy_windows_callback(HWND hwnd, LPARAM lparam [[maybe_unused]])
{
    [[maybe_unused]] const BOOL res = DestroyWindow(hwnd);
    return TRUE;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagew
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translatemessage
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dispatchmessagew
 */
static void wyn_run_native(void)
{
    for (;;)
    {
        MSG msg;
        const BOOL res = GetMessageW(&msg, 0, 0, 0);
        if (res == -1) break; // -1;
        if (res == 0) break; // (int)msg.wparam;

        [[maybe_unused]] const BOOL res1 = TranslateMessage(&msg);
        [[maybe_unused]] const LRESULT res2 = DispatchMessageW(&msg);
    }

    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close
 * @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-app
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowprocw
 */
static LRESULT CALLBACK wyn_msgproc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    //WYN_LOG("[MSG-PROC] | %16p | %4x | %16llx | %16llx |\n", (void*)hwnd, umsg, wparam, lparam);

    switch (umsg)
    {
        case WM_CLOSE:
        {
            // WYN_LOG("[MSG HWND] CLOSED!\n");
            PostQuitMessage(1);
            return 0;
        }

        case WM_APP:
        {
            wyn_on_signal(wyn_state.userdata);
            break;
        }
    }

    const LRESULT res = DefWindowProcW(hwnd, umsg, wparam, lparam);
    return res;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowprocw
 */
static LRESULT CALLBACK wyn_wndproc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    //WYN_LOG("[WND-PROC] | %16p | %4x | %16llx | %16llx |\n", (void*)hwnd, umsg, wparam, lparam);

    const wyn_window_t window = (wyn_window_t)hwnd;

    switch (umsg)
    {
        case WM_CLOSE:
        {
            wyn_on_window_close(wyn_state.userdata, window);
            return 0;
        }
    }

    const LRESULT res = DefWindowProcW(hwnd, umsg, wparam, lparam);
    return res;
}

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* userdata)
{
    if (wyn_reinit(userdata))
    {
        wyn_on_start(userdata);
        wyn_run_native();
        wyn_on_stop(userdata);
    }
    wyn_deinit();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_store
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
 */
extern void wyn_quit(void)
{
    atomic_store_explicit(&wyn_state.quitting, true, memory_order_relaxed);
    PostQuitMessage(0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_load
 */
extern bool wyn_quitting(void)
{
    return atomic_load_explicit(&wyn_state.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
 */
extern bool wyn_is_this_thread(void)
{
    return GetCurrentThreadId() == wyn_state.tid_main;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew
 */
extern void wyn_signal(void)
{
    [[maybe_unused]] const BOOL res = PostMessageW(wyn_state.msg_hwnd, WM_APP, 0, 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw
 */
extern wyn_window_t wyn_window_open(void)
{
    const HWND hwnd = CreateWindowExW(
        WYN_EX_STYLE, WYN_WND_CLASS, L"", WYN_WS_STYLE,
        0, 0, 640, 480,
        NULL, NULL, wyn_state.hinstance, NULL
    );

    return (wyn_window_t)hwnd;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
 */
extern void wyn_window_close(wyn_window_t window)
{
    const HWND hwnd = (HWND)window;
    [[maybe_unused]] const BOOL res = DestroyWindow(hwnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
 */
extern void wyn_window_show(wyn_window_t window)
{
    const HWND hwnd = (HWND)window;
    [[maybe_unused]] const BOOL res = ShowWindow(hwnd, SW_SHOW);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
 */
extern void wyn_window_hide(wyn_window_t window)
{
    const HWND hwnd = (HWND)window;
    [[maybe_unused]] const BOOL res = ShowWindow(hwnd, SW_HIDE);
}

// ================================================================================================================================
