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

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Internal structure for holding Wyn state.
 */
struct wyn_state_t
{
    void* userdata;         ///< The pointer provided by the user when the Event Loop was started.
    HANDLE read_pipe;       ///< Read-end of the callback pipe.
    HANDLE write_pipe;      ///< Write-end of the callback pipe.
    atomic_size_t len_pipe; ///< Number of callbacks enqueued on the pipe.
    HINSTANCE hinstance;    ///< HINSTANCE for the application.
    HWND msg_hwnd;          ///< Message-only Window for sending messages.
    ATOM msg_atom;          ///< Atom for the message-only Window.
    ATOM wnd_atom;          ///< Atom for regular Windows.
};

/**
 * @brief Static instance of all Wyn state.
 * @details Because Wyn can only be used on the Main Thread, it is safe to have static-storage state.
 *          This state must be global so it can be reached by callbacks on certain platforms.
 */
static struct wyn_state_t wyn_state = {};

/**
 * @brief Struct for passing callbacks with arguments.
 */
struct wyn_callback_t
{
    void (*func)(void*);    ///< The function to call.
    void* arg;              ///< The argument to pass to the function.
};

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes all Wyn state.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @return `true` if successful, `false` if there were errors.
 */
static bool wyn_init(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_terminate(void);

/**
 * @brief Runs all pending exec-callbacks.
 */
static void wyn_clear_events(void);

/**
 * @brief Destroys all remaining windows, without notifying the user.
 */
static void wyn_destroy_windows(void);

/**
 * @brief Callback function for destroying windows.
 */
static BOOL CALLBACK wyn_destroy_windows_callback(HWND hWnd, LPARAM lParam);

/**
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_run_native(void);

/**
 * @brief WndProc for the message-only Window.
 */
static LRESULT CALLBACK wyn_msgproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/**
 * @brief WndProc for regular Windows.
 */
static LRESULT CALLBACK wyn_wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe
 * @see https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandlew
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassexw
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw
 */
static bool wyn_init(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .read_pipe = NULL,
        .write_pipe = NULL,
        .len_pipe = 0,
        .hinstance = NULL,
        .msg_hwnd = NULL,
        .msg_atom = 0,
        .wnd_atom = 0,
    };

    {
        const BOOL res = CreatePipe(&wyn_state.read_pipe, &wyn_state.write_pipe, NULL, 0);
        if (res == 0) return false;
    }

    {
        wyn_state.hinstance = GetModuleHandleW(NULL);
        if (wyn_state.hinstance == 0) return false;
    }

    {
        const WNDCLASSEXW wnd_class = {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = wyn_wndproc,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = wyn_state.hinstance,
            .hIcon = LoadIcon(wyn_state.hinstance, IDI_APPLICATION),
            .hCursor = LoadCursor(wyn_state.hinstance, IDC_ARROW),
            .hbrBackground = NULL,
            .lpszMenuName = NULL,
            .lpszClassName = L"Wyn-Wnd",
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
            .lpszClassName = L"Wyn-Msg",
            .hIconSm = NULL,
        };
        wyn_state.msg_atom = RegisterClassExW(&msg_class);
        if (wyn_state.msg_atom == 0) return false;
    }

    {
        wyn_state.msg_hwnd = CreateWindowExW(
            0, L"Wyn-Msg", L"", 0,
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
static void wyn_terminate(void)
{
    wyn_clear_events();
    wyn_destroy_windows();

    if (wyn_state.msg_hwnd != NULL)
    {
        [[maybe_unused]] const BOOL res = DestroyWindow(wyn_state.msg_hwnd);
    }

    if (wyn_state.msg_atom != 0)
    {
        [[maybe_unused]] const BOOL res = UnregisterClassW(L"Wyn-Msg", wyn_state.hinstance);
    }

    if (wyn_state.wnd_atom != 0)
    {
        [[maybe_unused]] const BOOL res = UnregisterClassW(L"Wyn-Wnd", wyn_state.hinstance);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_load
 * @see https://en.cppreference.com/w/c/atomic/atomic_fetch_sub
 * @see https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
 */
static void wyn_clear_events(void)
{
    size_t rem = 0;

    rem = atomic_load_explicit(&wyn_state.len_pipe, memory_order_acquire);
    if (rem == 0) return;

    do {
        rem = atomic_fetch_sub_explicit(&wyn_state.len_pipe, 1, memory_order_acq_rel);
        {
            struct wyn_callback_t callback = {};
            {
                DWORD bytes_read = 0;
                const BOOL res = ReadFile(wyn_state.read_pipe, &callback, sizeof(callback), &bytes_read, NULL);
                WYN_ASSERT(res != FALSE);
                WYN_ASSERT(bytes_read == sizeof(callback));
                WYN_ASSERT(callback.func != NULL);
            }
            callback.func(callback.arg);
        }
    } while (rem > 1);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumthreadwindows
 * @see https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
 */
static void wyn_destroy_windows(void)
{
    (void)EnumThreadWindows(GetCurrentThreadId(), wyn_destroy_windows_callback, 0);
}

/**
 * @see https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/ms633496(v=vs.85)
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
 */
static BOOL CALLBACK wyn_destroy_windows_callback(HWND hWnd, LPARAM lParam [[maybe_unused]])
{
    (void)DestroyWindow(hWnd);
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
        if (res == -1) return; // -1;
        if (res == 0) return; // (int)msg.wParam;

        (void)TranslateMessage(&msg);
        (void)DispatchMessageW(&msg);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close
 * @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-app
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowprocw
 */
static LRESULT CALLBACK wyn_msgproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //fprintf(stderr, "[MSG-PROC] | %16p | %4x | %16llx | %16llx |\n", (void*)hWnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
        case WM_CLOSE:
        {
            fputs("[MSG HWND] CLOSED!\n", stderr);
            PostQuitMessage(1);
            return 0;
        }

        case WM_APP:
        {
            wyn_clear_events();
        }
        break;
    }

    const LRESULT res = DefWindowProcW(hWnd, uMsg, wParam, lParam);
    return res;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowprocw
 */
static LRESULT CALLBACK wyn_wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //fprintf(stderr, "[WND-PROC] | %16p | %4x | %16llx | %16llx |\n", (void*)hWnd, uMsg, wParam, lParam);

    const wyn_window_t window = (wyn_window_t)hWnd;

    switch (uMsg)
    {
        case WM_CLOSE:
        {
            wyn_on_window_close(wyn_state.userdata, window);
        }
        break;
    }

    const LRESULT res = DefWindowProcW(hWnd, uMsg, wParam, lParam);
    return res;
}

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* userdata)
{
    if (wyn_init(userdata))
    {
        wyn_on_start(userdata);
        wyn_run_native();
        wyn_on_stop(userdata);
    }
    wyn_terminate();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
 */
extern void wyn_quit(void)
{
    PostQuitMessage(0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_fetch_add
 * @see https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagew
 * @see https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror
 */
extern void wyn_execute(void (*func)(void*), void* arg)
{
    {
        DWORD bytes_written = 0;
        const struct wyn_callback_t callback = { .func = func, .arg = arg };
    
        const BOOL res = WriteFile(wyn_state.write_pipe, &callback, sizeof(callback), &bytes_written, NULL);
        WYN_ASSERT(res != FALSE);
        WYN_ASSERT(bytes_written == sizeof(callback));
        
        atomic_fetch_add_explicit(&wyn_state.len_pipe, 1, memory_order_acq_rel);
    }

    {
        [[maybe_unused]] const LRESULT ret = SendMessageW(wyn_state.msg_hwnd, WM_APP, 0, 0);
        const DWORD err = GetLastError();
        WYN_ASSERT(err != ERROR_ACCESS_DENIED);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_fetch_add
 * @see https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew
 */
extern void wyn_execute_async(void (*func)(void*), void* arg)
{
    {
        DWORD bytes_written = 0;
        const struct wyn_callback_t callback = { .func = func, .arg = arg };

        const BOOL res = WriteFile(wyn_state.write_pipe, &callback, sizeof(callback), &bytes_written, NULL);
        WYN_ASSERT(res != FALSE);
        WYN_ASSERT(bytes_written == sizeof(callback));
        
        atomic_fetch_add_explicit(&wyn_state.len_pipe, 1, memory_order_acq_rel);
    }    

    {
        const BOOL res = PostMessageW(wyn_state.msg_hwnd, WM_APP, 0, 0);
        WYN_ASSERT(res != FALSE);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw
 */
extern wyn_window_t wyn_open_window(void)
{
    const HWND hWnd = CreateWindowExW(
        0, L"Wyn-Wnd", L"", WS_OVERLAPPEDWINDOW,
        0, 0, 640, 480,
        NULL, NULL, wyn_state.hinstance, NULL
    );

    return (wyn_window_t)hWnd;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closewindow
 */
extern void wyn_close_window(wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] const BOOL res = CloseWindow(hWnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
 */
extern void wyn_show_window(wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] const BOOL res = ShowWindow(hWnd, SW_SHOW);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
 */
extern void wyn_hide_window(wyn_window_t window)
{
    const HWND hWnd = (HWND)window;
    [[maybe_unused]] const BOOL res = ShowWindow(hWnd, SW_HIDE);
}

// ================================================================================================================================
