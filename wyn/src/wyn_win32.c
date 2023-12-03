/**
 * @file wyn_win32.c
 * @brief Implementation of Wyn for the Win32 backend.
 */

#include "wyn.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <Windows.h>
#include <windowsx.h>

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef _VC_NODEFAULTLIB
    /**
     * @see Win32:
     * - https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-fatalexit
     */
    #define WYN_ASSERT(expr) if (expr) {} else FatalExit(1)
#else
    /**
     * @see Win32:
     * - https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort
     */
    #define WYN_ASSERT(expr) if (expr) {} else abort()
#endif

/**
 * @see C:
 * - https://en.cppreference.com/w/c/io/fprintf
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
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
 * - https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandlew
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadiconw
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadcursorw
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassexw
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw
 */
static bool wyn_reinit(void* const userdata)
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
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unregisterclassw
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
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumthreadwindows
 */
static void wyn_destroy_windows(void)
{
    [[maybe_unused]] const BOOL res = EnumThreadWindows(wyn_state.tid_main, wyn_destroy_windows_callback, 0);
}

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/ms633496(v=vs.85)
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
 */
static BOOL CALLBACK wyn_destroy_windows_callback(HWND const hwnd, LPARAM const lparam [[maybe_unused]])
{
    [[maybe_unused]] const BOOL res = DestroyWindow(hwnd);
    return TRUE;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagew
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translatemessage
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dispatchmessagew
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
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
 */
static void wyn_convert_text(wyn_window_t const window, const WCHAR* src_chr, const int src_len)
{
    char dst_chr[5];
    const int dst_len = WideCharToMultiByte(CP_UTF8, 0, src_chr, src_len, dst_chr, sizeof(dst_chr) - 1, NULL, NULL);
    dst_chr[dst_len] = '\0';
    
    WYN_ASSERT((dst_len > 0) && (dst_len <= 4));

    wyn_on_text(wyn_state.userdata, window, (const wyn_utf8_t*)dst_chr);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close
 * - https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-app
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowprocw
 */
static LRESULT CALLBACK wyn_msgproc(HWND const hwnd, UINT const umsg, WPARAM const wparam, LPARAM const lparam)
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

static LRESULT CALLBACK wyn_wndproc(HWND const hwnd, UINT const umsg, WPARAM const wparam, LPARAM const lparam)
{
    //WYN_LOG("[WND-PROC] | %16p | %4x | %16llx | %16llx |\n", (void*)hwnd, umsg, wparam, lparam);

    const wyn_window_t window = (wyn_window_t)hwnd;

    switch (umsg)
    {
        // https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close
        case WM_CLOSE:
        {
            wyn_on_window_close(wyn_state.userdata, window);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/gdi/wm-paint
        case WM_PAINT:
        {
            wyn_on_window_redraw(wyn_state.userdata, window);
            break; //return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanged
        case WM_WINDOWPOSCHANGED:
        {
            wyn_on_window_reposition(wyn_state.userdata, window, wyn_window_position(window), (wyn_coord_t)1.0);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousemove
        case WM_MOUSEMOVE:
        {
            const int xpos = GET_X_LPARAM(lparam);
            const int ypos = GET_Y_LPARAM(lparam);
            wyn_on_cursor(wyn_state.userdata, window, (wyn_coord_t)xpos, (wyn_coord_t)ypos);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousehover
        case WM_MOUSEHOVER:
        {
            // WYN_LOG("[WYN] WM_MOUSEHOVER\n");
            break;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseleave
        case WM_MOUSELEAVE:
        {
            // WYN_LOG("[WYN] WM_MOUSELEAVE\n");
            break;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousewheel
        case WM_MOUSEWHEEL:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
            const double norm = (double)delta / (double)WHEEL_DELTA;
            wyn_on_scroll(wyn_state.userdata, window, 0.0, norm);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousehwheel
        case WM_MOUSEHWHEEL:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
            const double norm = (double)delta / (double)WHEEL_DELTA;
            wyn_on_scroll(wyn_state.userdata, window, norm, 0.0);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-lbuttondown
        case WM_LBUTTONDOWN:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_LBUTTON, true);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-rbuttondown
        case WM_RBUTTONDOWN:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_RBUTTON, true);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mbuttondown
        case WM_MBUTTONDOWN:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_MBUTTON, true);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-xbuttondown
        case WM_XBUTTONDOWN:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            const WORD button = GET_XBUTTON_WPARAM(wparam);
            if (button == XBUTTON1) wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_XBUTTON1, true);
            if (button == XBUTTON2) wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_XBUTTON2, true);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-lbuttonup
        case WM_LBUTTONUP:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_LBUTTON, false);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-rbuttonup
        case WM_RBUTTONUP:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_RBUTTON, false);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mbuttonup
        case WM_MBUTTONUP:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_MBUTTON, false);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-xbuttonup
        case WM_XBUTTONUP:
        {
            [[maybe_unused]] const int xpos = GET_X_LPARAM(lparam);
            [[maybe_unused]] const int ypos = GET_Y_LPARAM(lparam);
            [[maybe_unused]] const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            const WORD button = GET_XBUTTON_WPARAM(wparam);
            if (button == XBUTTON1) wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_XBUTTON1, false);
            if (button == XBUTTON2) wyn_on_mouse(wyn_state.userdata, window, (wyn_button_t)MK_XBUTTON2, false);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-char
        case WM_CHAR:
        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syschar
        case WM_SYSCHAR:
        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-deadchar
        case WM_DEADCHAR:
        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-sysdeadchar
        case WM_SYSDEADCHAR:
        {
            // { [0] = High, [1] = Low }
            static WCHAR surrogates[2] = { 0, 0 };
            static HWND surrogate_hwnd = 0;

            // ----------------------------------------------------------------

            if ((surrogate_hwnd != 0) && (surrogate_hwnd != hwnd))
            {
                surrogates[0] = 0;
                surrogates[1] = 0;
                surrogate_hwnd = 0;
            }

            // ----------------------------------------------------------------

            if (IS_HIGH_SURROGATE(wparam))
            {
                surrogates[0] = (WCHAR)wparam;
                surrogate_hwnd = hwnd;
            }
            else if (IS_LOW_SURROGATE(wparam))
            {
                surrogates[1] = (WCHAR)wparam;
                surrogate_hwnd = hwnd;
            }
            else
            {
                surrogates[0] = 0;
                surrogates[1] = 0;
                surrogate_hwnd = 0;
            }

            // ----------------------------------------------------------------

            if (surrogates[0] || surrogates[1])
            {
                if (IS_SURROGATE_PAIR(surrogates[0], surrogates[1]))
                {
                    wyn_convert_text(window, surrogates, 2);
                }

                if (surrogates[0] && surrogates[1])
                {
                    surrogates[0] = 0;
                    surrogates[1] = 0;
                    surrogate_hwnd = 0;
                }
            }
            else
            {
                const WCHAR character = (WCHAR)wparam;
                wyn_convert_text(window, &character, 1);
            }

            // ----------------------------------------------------------------

            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keydown
        case WM_KEYDOWN:
        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syskeydown
        case WM_SYSKEYDOWN:
        {
            wyn_on_keyboard(wyn_state.userdata, window, (wyn_keycode_t)wparam, true);
            return 0;
        }

        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keyup
        case WM_KEYUP:
        // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syskeyup
        case WM_SYSKEYUP:
        {
            wyn_on_keyboard(wyn_state.userdata, window, (wyn_keycode_t)wparam, false);
            return 0;
        }

        // default:
        // {
        //     WYN_LOG("[WYN] UMSG: %u\n", (unsigned)umsg);
        // }
    }

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowprocw
    const LRESULT res = DefWindowProcW(hwnd, umsg, wparam, lparam);
    return res;
}

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* const userdata)
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
 * @see C:
 * - https://en.cppreference.com/w/c/atomic/atomic_store
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
 */
extern void wyn_quit(void)
{
    atomic_store_explicit(&wyn_state.quitting, true, memory_order_relaxed);
    PostQuitMessage(0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see C:
 * - https://en.cppreference.com/w/c/atomic/atomic_load
 */
extern wyn_bool_t wyn_quitting(void)
{
    return (wyn_bool_t)atomic_load_explicit(&wyn_state.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
 */
extern wyn_bool_t wyn_is_this_thread(void)
{
    return (wyn_bool_t)(GetCurrentThreadId() == wyn_state.tid_main);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew
 */
extern void wyn_signal(void)
{
    const BOOL res = PostMessageW(wyn_state.msg_hwnd, WM_APP, 0, 0);
    WYN_ASSERT(res != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw
 */
extern wyn_window_t wyn_window_open(void)
{
    const HWND hwnd = CreateWindowExW(
        WYN_EX_STYLE, WYN_WND_CLASS, L"", WYN_WS_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, wyn_state.hinstance, NULL
    );

    return (wyn_window_t)hwnd;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
 */
extern void wyn_window_close(wyn_window_t const window)
{
    const HWND hwnd = (HWND)window;
    [[maybe_unused]] const BOOL res = DestroyWindow(hwnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
 */
extern void wyn_window_show(wyn_window_t const window)
{
    const HWND hwnd = (HWND)window;
    [[maybe_unused]] const BOOL res = ShowWindow(hwnd, SW_SHOW);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
 */
extern void wyn_window_hide(wyn_window_t const window)
{
    const HWND hwnd = (HWND)window;
    [[maybe_unused]] const BOOL res = ShowWindow(hwnd, SW_HIDE);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_coord_t wyn_window_scale(wyn_window_t const window)
{
    (void)window;
    return (wyn_coord_t)1.0;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
 */
extern wyn_extent_t wyn_window_size(wyn_window_t const window)
{
    const HWND hwnd = (HWND)window;

    RECT rect;
    const BOOL res = GetClientRect(hwnd, &rect);
    WYN_ASSERT(res != 0);

    return (wyn_extent_t){ .w = (wyn_coord_t)(rect.right), .h = (wyn_coord_t)(rect.bottom) };
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectexfordpi
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos
 */
extern void wyn_window_resize(wyn_window_t const window, wyn_extent_t const extent)
{
    const HWND hwnd = (HWND)window;
    const wyn_coord_t rounded_w = ceil(extent.w);
    const wyn_coord_t rounded_h = ceil(extent.h);

    const UINT dpi = GetDpiForWindow(hwnd);
    const DWORD ws_style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    const DWORD ex_style = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    RECT rect = { .right = (LONG)rounded_w, .bottom = (LONG)rounded_h };
    const BOOL res_adj = AdjustWindowRectExForDpi(&rect, ws_style, FALSE, ex_style, dpi);
    WYN_ASSERT(res_adj != 0);

    const BOOL res_set = SetWindowPos(
        hwnd, 0, 0, 0, (int)(rect.right - rect.left), (int)(rect.bottom - rect.top),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE
    );
    WYN_ASSERT(res_set != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-clienttoscreen
 */
extern wyn_rect_t wyn_window_position(wyn_window_t const window)
{
    const HWND hwnd = (HWND)window;

    RECT rect;
    const BOOL res_rect = GetClientRect(hwnd, &rect);
    WYN_ASSERT(res_rect != 0);

    POINT point = {};
    const BOOL res_point = ClientToScreen(hwnd, &point);
    WYN_ASSERT(res_point != 0);

    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)point.x, .y = (wyn_coord_t)point.y },
        .extent = { .w = (wyn_coord_t)rect.right, .h = (wyn_coord_t)rect.bottom }
    };
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectexfordpi
 * - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos
 */
extern void wyn_window_reposition(wyn_window_t const window, const wyn_point_t* const origin, const wyn_extent_t* const extent)
{
    const wyn_coord_t rounded_x = origin ? floor(origin->x) : 0.0;
    const wyn_coord_t rounded_y = origin ? floor(origin->y) : 0.0;
    const wyn_coord_t rounded_w = extent ? ceil(extent->w) : 0.0;
    const wyn_coord_t rounded_h = extent ? ceil(extent->h) : 0.0;

    const HWND hwnd = (HWND)window;

    const UINT dpi = GetDpiForWindow(hwnd);
    const DWORD ws_style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    const DWORD ex_style = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    RECT rect = { .left = (LONG)rounded_x, .top = (LONG)rounded_y, .right = (LONG)(rounded_x + rounded_w), .bottom = (LONG)(rounded_y + rounded_h) };
    const BOOL res_adj = AdjustWindowRectExForDpi(&rect, ws_style, FALSE, ex_style, dpi);
    WYN_ASSERT(res_adj != 0);

    const BOOL res_set = SetWindowPos(
        hwnd, 0, (int)rect.left, (int)rect.top, (int)(rect.right - rect.left), (int)(rect.bottom - rect.top),
        (origin ? 0 : SWP_NOMOVE) | (extent ? 0 : SWP_NOSIZE) | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE
    );
    WYN_ASSERT(res_set != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_retitle(wyn_window_t const window, const wyn_utf8_t* const title)
{
    if (title)
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
        const int req_chr = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)title, -1, NULL, 0);
        WYN_ASSERT(req_chr > 0);

        // https://en.cppreference.com/w/c/memory/malloc
        WCHAR* const allocation = malloc((size_t)req_chr * sizeof(WCHAR));
        WYN_ASSERT(allocation);

        // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
        const int res_cvt = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)title, -1, allocation, req_chr);
        WYN_ASSERT(res_cvt == req_chr);

        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowtextw
        const BOOL res = SetWindowTextW((HWND)window, allocation);
        WYN_ASSERT(res != 0);

        // https://en.cppreference.com/w/c/memory/free
        free(allocation);
    }
    else
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowtextw
        const BOOL res = SetWindowTextW((HWND)window, L"");
        WYN_ASSERT(res != 0);
    }
}


// ================================================================================================================================

extern void* wyn_native_context(wyn_window_t const window)
{
    (void)window;
    return wyn_state.hinstance;
}

// ================================================================================================================================

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
 */
extern const wyn_vb_mapping_t* wyn_vb_mapping(void)
{
    static const wyn_vb_mapping_t mapping = {
        [wyn_vb_left]   = MK_LBUTTON, 
        [wyn_vb_right]  = MK_RBUTTON,
        [wyn_vb_middle] = MK_MBUTTON,
    };
    return &mapping;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
 */
extern const wyn_vk_mapping_t* wyn_vk_mapping(void)
{
    static const wyn_vk_mapping_t mapping = {
        [wyn_vk_0]              = '0',
        [wyn_vk_1]              = '1',
        [wyn_vk_2]              = '2',
        [wyn_vk_3]              = '3',
        [wyn_vk_4]              = '4',
        [wyn_vk_5]              = '5',
        [wyn_vk_6]              = '6',
        [wyn_vk_7]              = '7',
        [wyn_vk_8]              = '8',
        [wyn_vk_9]              = '9',
        [wyn_vk_A]              = 'A',
        [wyn_vk_B]              = 'B',
        [wyn_vk_C]              = 'C',
        [wyn_vk_D]              = 'D',
        [wyn_vk_E]              = 'E',
        [wyn_vk_F]              = 'F',
        [wyn_vk_G]              = 'G',
        [wyn_vk_H]              = 'H',
        [wyn_vk_I]              = 'I',
        [wyn_vk_J]              = 'J',
        [wyn_vk_K]              = 'K',
        [wyn_vk_L]              = 'L',
        [wyn_vk_M]              = 'M',
        [wyn_vk_N]              = 'N',
        [wyn_vk_O]              = 'O',
        [wyn_vk_P]              = 'P',
        [wyn_vk_Q]              = 'Q',
        [wyn_vk_R]              = 'R',
        [wyn_vk_S]              = 'S',
        [wyn_vk_T]              = 'T',
        [wyn_vk_U]              = 'U',
        [wyn_vk_V]              = 'V',
        [wyn_vk_W]              = 'W',
        [wyn_vk_X]              = 'X',
        [wyn_vk_Y]              = 'Y',
        [wyn_vk_Z]              = 'Z',
        [wyn_vk_Left]           = VK_LEFT,
        [wyn_vk_Right]          = VK_RIGHT,
        [wyn_vk_Up]             = VK_UP,
        [wyn_vk_Down]           = VK_DOWN,
        [wyn_vk_Period]         = VK_OEM_PERIOD,
        [wyn_vk_Comma]          = VK_OEM_COMMA,
        [wyn_vk_Semicolon]      = VK_OEM_1,
        [wyn_vk_Quote]          = VK_OEM_7,
        [wyn_vk_Slash]          = VK_OEM_2,
        [wyn_vk_Backslash]      = VK_OEM_5,
        [wyn_vk_BracketL]       = VK_OEM_4,
        [wyn_vk_BracketR]       = VK_OEM_6,
        [wyn_vk_Plus]           = VK_OEM_PLUS,
        [wyn_vk_Minus]          = VK_OEM_MINUS,
        [wyn_vk_Accent]         = VK_OEM_3,
        [wyn_vk_Control]        = VK_CONTROL,
        [wyn_vk_Start]          = VK_LWIN,
        [wyn_vk_Alt]            = VK_MENU,
        [wyn_vk_Space]          = VK_SPACE,
        [wyn_vk_Backspace]      = VK_BACK,
        [wyn_vk_Delete]         = VK_DELETE,
        [wyn_vk_Insert]         = VK_INSERT,
        [wyn_vk_Shift]          = VK_SHIFT,
        [wyn_vk_CapsLock]       = VK_CAPITAL,
        [wyn_vk_Tab]            = VK_TAB,
        [wyn_vk_Enter]          = VK_RETURN,
        [wyn_vk_Escape]         = VK_ESCAPE,
        [wyn_vk_Home]           = VK_HOME,
        [wyn_vk_End]            = VK_END,
        [wyn_vk_PageUp]         = VK_PRIOR,
        [wyn_vk_PageDown]       = VK_NEXT,
        [wyn_vk_F1]             = VK_F1,
        [wyn_vk_F2]             = VK_F2,
        [wyn_vk_F3]             = VK_F3,
        [wyn_vk_F4]             = VK_F4,
        [wyn_vk_F5]             = VK_F5,
        [wyn_vk_F6]             = VK_F6,
        [wyn_vk_F7]             = VK_F7,
        [wyn_vk_F8]             = VK_F8,
        [wyn_vk_F9]             = VK_F9,
        [wyn_vk_F10]            = VK_F10,
        [wyn_vk_F11]            = VK_F11,
        [wyn_vk_F12]            = VK_F12,
        [wyn_vk_PrintScreen]    = VK_SNAPSHOT,
        [wyn_vk_ScrollLock]     = VK_SCROLL,
        [wyn_vk_NumLock]        = VK_NUMLOCK,
        [wyn_vk_Numpad0]        = VK_NUMPAD0,
        [wyn_vk_Numpad1]        = VK_NUMPAD0,
        [wyn_vk_Numpad2]        = VK_NUMPAD0,
        [wyn_vk_Numpad3]        = VK_NUMPAD0,
        [wyn_vk_Numpad4]        = VK_NUMPAD0,
        [wyn_vk_Numpad5]        = VK_NUMPAD0,
        [wyn_vk_Numpad6]        = VK_NUMPAD0,
        [wyn_vk_Numpad7]        = VK_NUMPAD0,
        [wyn_vk_Numpad8]        = VK_NUMPAD0,
        [wyn_vk_Numpad9]        = VK_NUMPAD0,
        [wyn_vk_NumpadPlus]     = VK_ADD,
        [wyn_vk_NumpadMinus]    = VK_SUBTRACT,
        [wyn_vk_NumpadMultiply] = VK_MULTIPLY,
        [wyn_vk_NumpadDivide]   = VK_DIVIDE,
        [wyn_vk_NumpadPeriod]   = VK_DECIMAL,
    };
    return &mapping;
}

// ================================================================================================================================
