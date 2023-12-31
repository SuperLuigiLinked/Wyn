/**
 * @file wyn_win32.c
 * @brief Implementation of Wyn for the Win32 backend.
 */

/// @see https://learn.microsoft.com/en-us/windows/win32/intl/conventions-for-function-prototypes
#define UNICODE

#include <wyn.h>

#if !(defined(__STDC_NO_ATOMICS__) && __STDC_NO_ATOMICS__)
    #include <stdatomic.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include <Windows.h>
#include <windowsx.h>

#if __STDC_VERSION__ <= 201710L
    #ifdef true
        #undef true
    #endif
    #ifdef false
        #undef false
    #endif
    #define true ((wyn_bool_t)1)
    #define false ((wyn_bool_t)0)
#endif

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef _VC_NODEFAULTLIB
    /// @see FatalExit | <Windows.h> <winbase.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-fatalexit
    #define WYN_ASSERT(expr) if (expr) {} else FatalExit(1)
#else
    /// @see abort | <stdlib.h> <process.h> [CRT] | https://en.cppreference.com/w/c/program/abort | https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort
    #define WYN_ASSERT(expr) if (expr) {} else abort()
#endif

#ifdef NDEBUG
    #define WYN_ASSUME(expr) ((void)0)
#else
    #define WYN_ASSUME(expr) WYN_ASSERT(expr)
#endif

#ifdef _VC_NODEFAULTLIB
    #define WYN_LOG(...) ((void)0)
#else
    /// @see fprintf | <stdio.h> [CRT] | https://en.cppreference.com/w/c/io/fprintf | https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fprintf-fprintf-l-fwprintf-fwprintf-l
    #define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)
#endif

#if __STDC_VERSION__ >= 201904L
    /// @see [[maybe_unused]] | (C23) | https://en.cppreference.com/w/c/language/attributes/maybe_unused
    #define WYN_UNUSED [[maybe_unused]]
#else
    #define WYN_UNUSED
#endif

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Win32 Window-Style for Bordered Windows.
 */
#define WYN_WIN32_WS_STYLE_BORDERED (WS_OVERLAPPEDWINDOW | (WS_CLIPCHILDREN | WS_CLIPSIBLINGS))

/**
 * @brief Win32 Ex-Style for Bordered Windows.
 */
#define WYN_WIN32_EX_STYLE_BORDERED (0)

/**
 * @brief Win32 Window-Style for Borderless Windows.
 */
#define WYN_WIN32_WS_STYLE_BORDERLESS (WS_POPUP | (WS_CLIPCHILDREN | WS_CLIPSIBLINGS))

/**
 * @brief Win32 Ex-Style for Borderless Windows.
 */
#define WYN_WIN32_EX_STYLE_BORDERLESS (0)

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Data for enumerating monitors.
 */
struct wyn_win32_monitor_data_t
{
    wyn_display_callback callback; ///< User-provided callback function.
    void* userdata; ///< User-provided callback argument.
    unsigned counter; ///< Monitor counter.
};
typedef struct wyn_win32_monitor_data_t wyn_win32_monitor_data_t;

/**
 * @brief Win32 backend state.
 */
struct wyn_win32_t
{
    void* userdata; ///< The pointer provided by the user when the Event Loop was started.
#ifdef _VC_NODEFAULTLIB
    HANDLE heap; ///< Process Heap for allocations.
#endif
    HINSTANCE hinstance; ///< HINSTANCE for the application.
    HWND msg_hwnd; ///< Message-only Window for sending messages.
    ATOM msg_atom; ///< Atom for the message-only Window.
    ATOM wnd_atom; ///< Atom for user-created Windows.
    DWORD tid_main; ///< Thread ID of the Main Thread.
    HWND surrogate_hwnd; ///< Last HWND to receive character input.
    WCHAR surrogate_high; ///< Tracks surrogate pairs. @see https://learn.microsoft.com/en-us/windows/win32/intl/surrogates-and-supplementary-characters
#if defined(__STDC_NO_ATOMICS__) && __STDC_NO_ATOMICS__
    LONG quitting; ///< Flag to indicate the Event Loop is quitting.    
#else
    _Atomic(wyn_bool_t) quitting; ///< Flag to indicate the Event Loop is quitting.    
#endif
};
typedef struct wyn_win32_t wyn_win32_t;

/**
 * @brief Static instance of Win32 backend.
 */
static wyn_win32_t wyn_win32;

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes all Wyn state.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @return `true` if successful, `false` if there were errors.
 */
static wyn_bool_t wyn_win32_reinit(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_win32_deinit(void);

/**
 * @brief Destroys all remaining windows, without notifying the user.
 */
static void wyn_win32_destroy_windows(void);

/**
 * @brief Callback function for destroying windows.
 * @see EnumThreadWndProc | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/ms633496(v=vs.85)
 */
static BOOL CALLBACK wyn_win32_destroy_windows_callback(HWND hwnd, LPARAM lparam);

/**
 * @brief Callback function for enumerating monitors.
 * @see MonitorEnumProc | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-monitorenumproc
 */
static BOOL CALLBACK wyn_win32_enum_monitors_callback(HMONITOR monitor, HDC hdc, LPRECT rect, LPARAM lparam);

/**
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_win32_event_loop(void);

/**
 * @brief WndProc for the message-only Window.
 * @see WndProc | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc
 */
static LRESULT CALLBACK wyn_win32_msgproc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);

/**
 * @brief WndProc for user-created Windows.
 * @see WndProc | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc
 */
static LRESULT CALLBACK wyn_win32_wndproc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);

/**
 * @brief Handler for Mouse Events.
 */
static inline void wyn_win32_wndproc_mouse(HWND hwnd, WPARAM wparam, LPARAM lparam, wyn_button_t button, wyn_bool_t pressed);

/**
 * @brief Handler for Text Events.
 */
static inline void wyn_win32_wndproc_text(wyn_window_t window, const WCHAR* src_chr, int src_len);

/**
 * @brief Allocates heap memory.
 * @param bytes The number of bytes to allocate.
 * @return Pointer to the allocated memory, or NULL on failure.
 */
static void* wyn_win32_heap_alloc(size_t bytes);

/**
 * @brief Frees heap memory.
 * @param[in] ptr [nullable] The memory-allocation to free, or NULL.
 */
static void wyn_win32_heap_free(void* ptr);

/**
 * @brief Converts from wyn coords to native coords, rounding down.
 * @param val [non-negative] The value to round down.
 * @return `floor(val)`
 */
static LONG wyn_win32_floor(wyn_coord_t val);

/**
 * @brief Converts from wyn coords to native coords, rounding up.
 * @param val [non-negative] The value to round up.
 * @return `ceil(val)`
 */
static LONG wyn_win32_ceil(wyn_coord_t val);

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

static wyn_bool_t wyn_win32_reinit(void* const userdata)
{
    wyn_win32 = (struct wyn_win32_t){
        .userdata = userdata,
    #ifdef _VC_NODEFAULTLIB
        .heap = NULL,
    #endif
        .hinstance = NULL,
        .msg_hwnd = NULL,
        .msg_atom = 0,
        .wnd_atom = 0,
        .tid_main = 0,
        .surrogate_hwnd = NULL,
        .surrogate_high = 0,
        .quitting = 0,
    };
    {
        /// @see GetModuleHandleW | <Windows.h> <libloaderapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandlew
        wyn_win32.hinstance = GetModuleHandleW(NULL);
        if (wyn_win32.hinstance == NULL) return false;
    }
    #ifdef _VC_NODEFAULTLIB
    {
        /// @see GetProcessHeap | <Windows.h> <heapapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheap
        wyn_win32.heap = GetProcessHeap();
        if (wyn_win32.heap == NULL) return false;
    }
    #endif
    {
        /// @see GetCurrentThreadId | <Windows.h> <processthreadsapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
        wyn_win32.tid_main = GetCurrentThreadId();
    }
    {
        /// @see LoadIconW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadiconw
        const HICON icon = LoadIconW(NULL, IDI_APPLICATION);
        if (icon == NULL) return false;

        /// @see LoadCursorW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadcursorw
        const HCURSOR cursor = LoadCursorW(NULL, IDC_ARROW);
        if (cursor == NULL) return false;
        
        /// @see WNDCLASSEXW | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-wndclassexw
        const WNDCLASSEXW wnd_class = {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = wyn_win32_wndproc,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = wyn_win32.hinstance,
            .hIcon = icon,
            .hCursor = cursor,
            .hbrBackground = NULL,
            .lpszMenuName = NULL,
            .lpszClassName = L"Wyn-Wnd",
            .hIconSm = NULL,
        };
        
        /// @see RegisterClassExW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassexw
        wyn_win32.wnd_atom = RegisterClassExW(&wnd_class);
        if (wyn_win32.wnd_atom == 0) return false;
    }
    {
        /// @see WNDCLASSEXW | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-wndclassexw
        const WNDCLASSEXW msg_class = {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = 0,
            .lpfnWndProc = wyn_win32_msgproc,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = wyn_win32.hinstance,
            .hIcon = NULL,
            .hCursor = NULL,
            .hbrBackground = NULL,
            .lpszMenuName = NULL,
            .lpszClassName = L"Wyn-Msg",
            .hIconSm = NULL,
        };

        /// @see RegisterClassExW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassexw
        wyn_win32.msg_atom = RegisterClassExW(&msg_class);
        if (wyn_win32.msg_atom == 0) return false;
    }
    {
        /// @see CreateWindowExW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw
        wyn_win32.msg_hwnd = CreateWindowExW(
            0, MAKEINTATOM(wyn_win32.msg_atom), L"", 0,
            0, 0, 0, 0,
            HWND_MESSAGE, NULL, wyn_win32.hinstance, NULL
        );
        if (wyn_win32.msg_hwnd == NULL) return false;
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_win32_deinit(void)
{
    wyn_win32_destroy_windows();

    if (wyn_win32.msg_hwnd != NULL)
    {
        /// @see DestroyWindow | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
        const BOOL res_hwnd = DestroyWindow(wyn_win32.msg_hwnd);
        (void)(res_hwnd != 0);
    }

    if (wyn_win32.msg_atom != 0)
    {
        /// @see UnregisterClassW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unregisterclassw
        const BOOL res_msg = UnregisterClassW(MAKEINTATOM(wyn_win32.msg_atom), wyn_win32.hinstance);
        (void)(res_msg != 0);
    }

    if (wyn_win32.wnd_atom != 0)
    {
        /// @see UnregisterClassW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unregisterclassw
        const BOOL res_wnd = UnregisterClassW(MAKEINTATOM(wyn_win32.wnd_atom), wyn_win32.hinstance);
        (void)(res_wnd != 0);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_win32_destroy_windows(void)
{
    /// @see EnumThreadWindows | <Windows.h> <winuser.h> [User32] (Windows 2000) | <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumthreadwindows
    WYN_UNUSED const BOOL res = EnumThreadWindows(wyn_win32.tid_main, wyn_win32_destroy_windows_callback, 0);
}

static BOOL CALLBACK wyn_win32_destroy_windows_callback(HWND const hwnd, LPARAM const lparam WYN_UNUSED)
{
    /// @see DestroyWindow | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
    const BOOL res_destroy = DestroyWindow(hwnd);
    (void)(res_destroy != 0);

    return TRUE;
}

// --------------------------------------------------------------------------------------------------------------------------------

static BOOL CALLBACK wyn_win32_enum_monitors_callback(HMONITOR const monitor, HDC const hdc WYN_UNUSED, LPRECT const rect WYN_UNUSED, LPARAM const lparam)
{
    wyn_win32_monitor_data_t* const data = (wyn_win32_monitor_data_t*)lparam;
    WYN_ASSUME(data != NULL);

    ++data->counter;
    if (!data->callback) return TRUE;

    /// @see MONITORINFO | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-monitorinfo
    MONITORINFO info = { .cbSize = sizeof(MONITORINFO) };

    /// @see GetMonitorInfoW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmonitorinfow
    const BOOL res_info = GetMonitorInfoW(monitor, &info);
    WYN_ASSERT(res_info != 0);

    const wyn_display_t display = (wyn_display_t)&info;
    const wyn_bool_t res = data->callback(data->userdata, display);
    return (BOOL)res;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_win32_event_loop(void)
{
    for (;;)
    {
        /// @see MSG | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-msg
        MSG msg;

        /// @see GetMessageW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagew
        const BOOL res = GetMessageW(&msg, 0, 0, 0);
        if (res == -1) break; // -1;
        if (res == 0) break; // (int)msg.wparam;

        /// @see TranslateMessage | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translatemessage
        WYN_UNUSED const BOOL res1 = TranslateMessage(&msg);

        /// @see DispatchMessageW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dispatchmessagew
        WYN_UNUSED const LRESULT res2 = DispatchMessageW(&msg);
    }

    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

static LRESULT CALLBACK wyn_win32_msgproc(HWND const hwnd, UINT const umsg, WPARAM const wparam, LPARAM const lparam)
{
    // WYN_LOG("[MSG-PROC] | %16p | %4x | %16llx | %16llx |\n", (void*)hwnd, (unsigned int)umsg, (unsigned long long)wparam, (unsigned long long)lparam);

    switch (umsg)
    {
        /// @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close
        case WM_CLOSE:
        {
            /// @see PostQuitMessage | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
            PostQuitMessage(1);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-app
        case WM_APP:
        {
            wyn_on_signal(wyn_win32.userdata);
            break;
        }
    }

    /// @see DefWindowProcW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowprocw
    const LRESULT res = DefWindowProcW(hwnd, umsg, wparam, lparam);
    return res;
}

// --------------------------------------------------------------------------------------------------------------------------------

/// @see WndProc | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc
static LRESULT CALLBACK wyn_win32_wndproc(HWND const hwnd, UINT const umsg, WPARAM const wparam, LPARAM const lparam)
{
    // WYN_LOG("[WND-PROC] | %16p | %4x | %16llx | %16llx |\n", (void*)hwnd, umsg, wparam, lparam);

    const wyn_window_t window = (wyn_window_t)hwnd;

    switch (umsg)
    {
        /// @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close
        case WM_CLOSE:
        {
            wyn_on_window_close(wyn_win32.userdata, window);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/gdi/wm-paint
        case WM_PAINT:
        {
            wyn_on_window_redraw(wyn_win32.userdata, window);
            break; //return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-activate
        case WM_ACTIVATE:
        {
            wyn_on_window_focus(wyn_win32.userdata, window, wparam != WA_INACTIVE);
            break;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanged
        case WM_WINDOWPOSCHANGED:
        {
            wyn_on_window_reposition(wyn_win32.userdata, window, wyn_window_position(window), (wyn_coord_t)1.0);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/devio/wm-devicechange
        case WM_DEVICECHANGE:
        {
            WYN_LOG("[WYN] WM_DEVICECHANGE\n");
            wyn_on_display_change(wyn_win32.userdata);
            break;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/gdi/wm-devmodechange
        case WM_DEVMODECHANGE:
        {
            WYN_LOG("[WYN] WM_DEVMODECHANGE\n");
            wyn_on_display_change(wyn_win32.userdata);
            break;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousemove
        case WM_MOUSEMOVE:
        {
            /// @see GET_X_LPARAM | <windowsx.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/windowsx/nf-windowsx-get_x_lparam
            const int xpos = GET_X_LPARAM(lparam);
            /// @see GET_Y_LPARAM | <windowsx.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/windowsx/nf-windowsx-get_y_lparam
            const int ypos = GET_Y_LPARAM(lparam);
            wyn_on_cursor(wyn_win32.userdata, window, (wyn_coord_t)xpos, (wyn_coord_t)ypos);

            /// @see TRACKMOUSEEVENT | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-trackmouseevent
            TRACKMOUSEEVENT track = {
                .cbSize = sizeof(TRACKMOUSEEVENT),
                .dwFlags = TME_LEAVE | TME_HOVER,
                .hwndTrack = hwnd,
                .dwHoverTime = HOVER_DEFAULT,
            };
            /// @see TrackMouseEvent | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackmouseevent
            const BOOL res_track = TrackMouseEvent(&track);
            WYN_ASSERT(res_track != 0);

            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousehover
        case WM_MOUSEHOVER:
        {
            WYN_LOG("[WYN] WM_MOUSEHOVER\n");
            break;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseleave
        case WM_MOUSELEAVE:
        {
            wyn_on_cursor_exit(wyn_win32.userdata, window);
            break;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousewheel
        case WM_MOUSEWHEEL:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousehwheel
        case WM_MOUSEHWHEEL:
        {
            /// @see GET_X_LPARAM | <windowsx.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/windowsx/nf-windowsx-get_x_lparam
            WYN_UNUSED const int xpos = GET_X_LPARAM(lparam);
            /// @see GET_Y_LPARAM | <windowsx.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/windowsx/nf-windowsx-get_y_lparam
            WYN_UNUSED const int ypos = GET_Y_LPARAM(lparam);
            /// @see GET_KEYSTATE_WPARAM | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-get_keystate_wparam
            WYN_UNUSED const WORD mods = GET_KEYSTATE_WPARAM(wparam);
            /// @see GET_WHEEL_DELTA_WPARAM | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-get_wheel_delta_wparam
            const short delta = GET_WHEEL_DELTA_WPARAM(wparam);

            const wyn_coord_t norm = (wyn_coord_t)delta / (wyn_coord_t)WHEEL_DELTA;
            const wyn_coord_t dx = (umsg == WM_MOUSEHWHEEL ? norm : 0.0);
            const wyn_coord_t dy = (umsg == WM_MOUSEWHEEL  ? norm : 0.0);
            wyn_on_scroll(wyn_win32.userdata, window, dx, dy);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-lbuttondown
        case WM_LBUTTONDOWN:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-lbuttonup
        case WM_LBUTTONUP:
        {
            wyn_win32_wndproc_mouse(hwnd, wparam, lparam, (wyn_button_t)MK_LBUTTON, umsg == WM_LBUTTONDOWN);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-rbuttondown
        case WM_RBUTTONDOWN:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-rbuttonup
        case WM_RBUTTONUP:
        {
            wyn_win32_wndproc_mouse(hwnd, wparam, lparam, (wyn_button_t)MK_RBUTTON, umsg == WM_RBUTTONDOWN);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mbuttondown
        case WM_MBUTTONDOWN:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mbuttonup
        case WM_MBUTTONUP:        
        {
            wyn_win32_wndproc_mouse(hwnd, wparam, lparam, (wyn_button_t)MK_MBUTTON, umsg == WM_MBUTTONDOWN);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-xbuttondown
        case WM_XBUTTONDOWN:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-xbuttonup
        case WM_XBUTTONUP:
        {
            /// @see GET_XBUTTON_WPARAM | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-get_xbutton_wparam
            const WORD button = GET_XBUTTON_WPARAM(wparam);
            if (button == XBUTTON1) wyn_win32_wndproc_mouse(hwnd, wparam, lparam, (wyn_button_t)MK_XBUTTON1, umsg == WM_XBUTTONDOWN);
            if (button == XBUTTON2) wyn_win32_wndproc_mouse(hwnd, wparam, lparam, (wyn_button_t)MK_XBUTTON2, umsg == WM_XBUTTONDOWN);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-char
        case WM_CHAR:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syschar
        case WM_SYSCHAR:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-deadchar
        case WM_DEADCHAR:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-sysdeadchar
        case WM_SYSDEADCHAR:
        {
            const WCHAR code = (WCHAR)wparam;

            if (wyn_win32.surrogate_hwnd != hwnd)
            {
                wyn_win32.surrogate_hwnd = hwnd;
                wyn_win32.surrogate_high = 0;
            }

            /// @see IS_HIGH_SURROGATE | <Windows.h> <winnls.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/Winnls/nf-winnls-is_high_surrogate
            if (IS_HIGH_SURROGATE(code))
            {
                wyn_win32.surrogate_high = code;
                return 0;
            }
            /// @see IS_LOW_SURROGATE | <Windows.h> <winnls.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/Winnls/nf-winnls-is_low_surrogate
            else if (IS_LOW_SURROGATE(code))
            {
                if (wyn_win32.surrogate_high != 0)
                {
                    const WCHAR pair[2] = { wyn_win32.surrogate_high, code };
                    wyn_win32_wndproc_text(window, pair, 2);
                }
            }
            else
            {
                wyn_win32_wndproc_text(window, &code, 1);
            }

            wyn_win32.surrogate_high = 0;
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keydown
        case WM_KEYDOWN:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syskeydown
        case WM_SYSKEYDOWN:
        {
            wyn_on_keyboard(wyn_win32.userdata, window, (wyn_keycode_t)wparam, true);
            return 0;
        }
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keyup
        case WM_KEYUP:
        /// @see https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syskeyup
        case WM_SYSKEYUP:
        {
            wyn_on_keyboard(wyn_win32.userdata, window, (wyn_keycode_t)wparam, false);
            return 0;
        }
        // default:
        // {
        //     WYN_LOG("[WYN] UMSG: %u\n", (unsigned)umsg);
        // }
    }

    /// @see DefWindowProcW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowprocw
    const LRESULT res = DefWindowProcW(hwnd, umsg, wparam, lparam);
    return res;
}

static inline void wyn_win32_wndproc_mouse(HWND const hwnd, WPARAM const wparam, LPARAM const lparam, wyn_button_t const button, wyn_bool_t const pressed)
{
    /// @see GET_X_LPARAM | <windowsx.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/windowsx/nf-windowsx-get_x_lparam
    WYN_UNUSED const int xpos = GET_X_LPARAM(lparam);
    /// @see GET_Y_LPARAM | <windowsx.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/windowsx/nf-windowsx-get_y_lparam
    WYN_UNUSED const int ypos = GET_Y_LPARAM(lparam);
    /// @see GET_KEYSTATE_WPARAM | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-get_keystate_wparam
    WYN_UNUSED const WORD mods = GET_KEYSTATE_WPARAM(wparam);

    if (pressed)
    {
        /// @see SetCapture | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture
        WYN_UNUSED HWND prev_cap = SetCapture(hwnd);
    }
    else
    {
        if ((mods & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) == 0)
        {
            /// @see ReleaseCapture | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture
            const BOOL res_release = ReleaseCapture();
            (void)(res_release != 0);
        }
    }

    wyn_on_mouse(wyn_win32.userdata, (wyn_window_t)hwnd, button, pressed);
}

static void wyn_win32_wndproc_text(wyn_window_t const window, const WCHAR* const src_chr, const int src_len)
{
    /// @see WideCharToMultiByte | <Windows.h> <stringapiset.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
    char dst_chr[5];
    const int dst_len = WideCharToMultiByte(CP_UTF8, 0, src_chr, src_len, dst_chr, sizeof(dst_chr) - 1, NULL, NULL);
    dst_chr[dst_len] = '\0';

    if (dst_len > 0)
        wyn_on_text(wyn_win32.userdata, window, (const wyn_utf8_t*)dst_chr);
}

// --------------------------------------------------------------------------------------------------------------------------------

static void* wyn_win32_heap_alloc(size_t const bytes)
{
#ifdef _VC_NODEFAULTLIB
    /// @see HeapAlloc | <Windows.h> <heapapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc
    return HeapAlloc(wyn_win32.heap, 0, bytes);
#else
    /// @see malloc | <stdlib.h> <malloc.h> [CRT] | https://en.cppreference.com/w/c/memory/malloc | https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/malloc
    return malloc(bytes);
#endif
}

static void wyn_win32_heap_free(void* const ptr)
{
#ifdef _VC_NODEFAULTLIB
    /// @see HeapFree | <Windows.h> <heapapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapfree
    HeapFree(wyn_win32.heap, 0, ptr);
#else
    /// @see free | <stdlib.h> <malloc.h> [CRT] | https://en.cppreference.com/w/c/memory/free | https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/free
    free(ptr);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

static LONG wyn_win32_floor(wyn_coord_t const val)
{
    WYN_ASSUME(val >= 0);
    
    return (LONG)val;
}

static LONG wyn_win32_ceil(wyn_coord_t const val)
{
    WYN_ASSUME(val >= 0);

    LONG const cast = (LONG)val;
    wyn_coord_t const recast = (wyn_coord_t)cast;
    return (recast == val) ? cast : cast + 1; 
}

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* const userdata)
{
    if (wyn_win32_reinit(userdata))
    {
        wyn_on_start(userdata);
        wyn_win32_event_loop();
        wyn_on_stop(userdata);
    }
    wyn_win32_deinit();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_quit(void)
{
#if defined(__STDC_NO_ATOMICS__) && __STDC_NO_ATOMICS__
    /// @see InterlockedExchangeNoFence | <Windows.h> <winnt.h> [Kernel32] (Windows 8) | https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/hh972659(v=vs.85)
    (void)InterlockedExchangeNoFence(&wyn_win32.quitting, TRUE);
#else
    /// @see atomic_store_explicit | <stdatomic.h> (C11) | https://en.cppreference.com/w/c/atomic/atomic_store
    atomic_store_explicit(&wyn_win32.quitting, true, memory_order_relaxed);
#endif

    /// @see PostQuitMessage | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
    PostQuitMessage(0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_quitting(void)
{
#if defined(__STDC_NO_ATOMICS__) && __STDC_NO_ATOMICS__
    /// @see InterlockedCompareExchangeNoFence | <Windows.h> <winnt.h> [Kernel32] (Windows 8) | https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/hh972645(v=vs.85)
    return (wyn_bool_t)InterlockedCompareExchangeNoFence(&wyn_win32.quitting, FALSE, FALSE);
#else
    /// @see atomic_load_explicit | <stdatomic.h> (C11) | https://en.cppreference.com/w/c/atomic/atomic_load
    return atomic_load_explicit(&wyn_win32.quitting, memory_order_relaxed);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_is_this_thread(void)
{
    /// @see GetCurrentThreadId | <Windows.h> <processthreadsapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
    return (wyn_bool_t)(GetCurrentThreadId() == wyn_win32.tid_main);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_signal(void)
{
    /// @see PostMessageW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew
    const BOOL res = PostMessageW(wyn_win32.msg_hwnd, WM_APP, 0, 0);
    WYN_ASSERT(res != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_window_t wyn_window_open(void)
{
    /// @see CreateWindowExW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw
    const HWND hwnd = CreateWindowExW(
        WYN_WIN32_EX_STYLE_BORDERED, MAKEINTATOM(wyn_win32.wnd_atom), L"", WYN_WIN32_WS_STYLE_BORDERED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, wyn_win32.hinstance, NULL
    );
    return (wyn_window_t)hwnd;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_close(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    const HWND hwnd = (HWND)window;

    /// @see DestroyWindow | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow
    const BOOL res_destroy = DestroyWindow(hwnd);
    (void)(res_destroy != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_show(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    const HWND hwnd = (HWND)window;

    /// @see ShowWindow | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
    WYN_UNUSED const BOOL res_show = ShowWindow(hwnd, SW_SHOW);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_hide(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    const HWND hwnd = (HWND)window;

    /// @see ShowWindow | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
    WYN_UNUSED const BOOL res_hide = ShowWindow(hwnd, SW_HIDE);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_coord_t wyn_window_scale(wyn_window_t const window WYN_UNUSED)
{
    WYN_ASSUME(window != NULL);

    return (wyn_coord_t)1.0;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_rect_t wyn_window_position(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    const HWND hwnd = (HWND)window;

    /// @see GetClientRect | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
    RECT rect;
    const BOOL res_rect = GetClientRect(hwnd, &rect);
    WYN_ASSERT(res_rect != 0);

    /// @see ClientToScreen | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-clienttoscreen
    POINT point = { 0, 0 };
    const BOOL res_point = ClientToScreen(hwnd, &point);
    WYN_ASSERT(res_point != 0);

    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)point.x, .y = (wyn_coord_t)point.y },
        .extent = { .w = (wyn_coord_t)rect.right, .h = (wyn_coord_t)rect.bottom }
    };
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_reposition(wyn_window_t const window, const wyn_point_t* const origin, const wyn_extent_t* const extent)
{
    WYN_ASSUME(window != NULL);
    const HWND hwnd = (HWND)window;

    if (wyn_window_is_fullscreen(window)) return;

    const LONG rounded_x = origin ? wyn_win32_floor(origin->x) : 0;
    const LONG rounded_y = origin ? wyn_win32_floor(origin->y) : 0;
    const LONG rounded_w = extent ? wyn_win32_ceil(extent->w) : 0;
    const LONG rounded_h = extent ? wyn_win32_ceil(extent->h) : 0;
    
    /// @see GetDpiForWindow | <Windows.h> <winuser.h> [User32] (Windows 10, version 1607) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow
    const UINT dpi = GetDpiForWindow(hwnd);
    WYN_ASSUME(dpi != 0);

    /// @see GetWindowLongPtrW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw
    const LONG_PTR ws_style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    /// @see AdjustWindowRectExForDpi | <Windows.h> <winuser.h> [User32] (Windows 10, version 1607) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectexfordpi
    RECT rect = { .left = rounded_x, .top = rounded_y, .right = rounded_x + rounded_w, .bottom = rounded_y + rounded_h };
    const BOOL res_adj = AdjustWindowRectExForDpi(&rect, (DWORD)ws_style, FALSE, (DWORD)ex_style, dpi);
    WYN_ASSERT(res_adj != 0);

    /// @see SetWindowPos | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos
    const BOOL res_set = SetWindowPos(
        hwnd, 0, (int)rect.left, (int)rect.top, (int)(rect.right - rect.left), (int)(rect.bottom - rect.top),
        (origin ? 0 : SWP_NOMOVE) | (extent ? 0 : SWP_NOSIZE) | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE
    );
    WYN_ASSERT(res_set != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_window_is_fullscreen(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    const HWND hwnd = (HWND)window;

    /// @see GetWindowLongPtrW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);

    /// @see IsZoomed | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iszoomed
    const BOOL maximized = IsZoomed(hwnd);

    return ((style & WS_POPUP) != 0) && maximized;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_fullscreen(wyn_window_t const window, wyn_bool_t const status)
{
    WYN_ASSUME(window != NULL);
    const wyn_bool_t was_fullscreen = wyn_window_is_fullscreen(window);
    if (was_fullscreen == status) return;

    const HWND hwnd = (HWND)window;
    
    /// @see SetWindowLongPtrW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw
    /// @see ShowWindow | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
    if (status)
    {
        WYN_UNUSED const LONG_PTR res_ws = SetWindowLongPtrW(hwnd, GWL_STYLE, WYN_WIN32_WS_STYLE_BORDERLESS);
        WYN_UNUSED const LONG_PTR res_ex = SetWindowLongPtrW(hwnd, GWL_EXSTYLE, WYN_WIN32_EX_STYLE_BORDERLESS);
        WYN_UNUSED const BOOL res_show = ShowWindow(hwnd, SW_MAXIMIZE);
    }
    else
    {
        WYN_UNUSED const BOOL res_show = ShowWindow(hwnd, SW_RESTORE);
        WYN_UNUSED const LONG_PTR res_ws = SetWindowLongPtrW(hwnd, GWL_STYLE, WYN_WIN32_WS_STYLE_BORDERED | WS_VISIBLE);
        WYN_UNUSED const LONG_PTR res_ex = SetWindowLongPtrW(hwnd, GWL_EXSTYLE, WYN_WIN32_EX_STYLE_BORDERED);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_retitle(wyn_window_t const window, const wyn_utf8_t* title)
{
    WYN_ASSUME(window != NULL);

    enum { buffer_chrs = 32 };
    WCHAR buffer[buffer_chrs];
    WCHAR* output = buffer;

    if (title)
    {
        /// @see MultiByteToWideChar | <Windows.h> <stringapiset.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
        const int req_chrs = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)title, -1, NULL, 0);
        WYN_ASSERT(req_chrs > 0);

        if (req_chrs > buffer_chrs)
        {
            WCHAR* const allocation = wyn_win32_heap_alloc((size_t)req_chrs * sizeof(WCHAR));
            WYN_ASSERT(allocation);
            output = allocation;
        }

        /// @see MultiByteToWideChar | <Windows.h> <stringapiset.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
        const int res_cvt = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)title, -1, output, req_chrs);
        WYN_ASSERT(res_cvt == req_chrs);
    }
    else
    {
        buffer[0] = L'\0';
    }

    /// @see SetWindowTextW | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowtextw
    const BOOL res = SetWindowTextW((HWND)window, output);
    WYN_ASSERT(res != 0);

    if (output != buffer)
    {
        wyn_win32_heap_free(output);
    }
}

// ================================================================================================================================

extern unsigned int wyn_enumerate_displays(wyn_display_callback callback, void* userdata)
{
    wyn_win32_monitor_data_t const data = { .callback = callback, .userdata = userdata, .counter = 0 };

    /// @see EnumDisplayMonitors | <Windows.h> <winuser.h> [User32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaymonitors
    const BOOL res_edm = EnumDisplayMonitors(NULL, NULL, wyn_win32_enum_monitors_callback, (LPARAM)&data);
    (void)(res_edm != 0);

    return data.counter;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_rect_t wyn_display_position(wyn_display_t const display)
{
    WYN_ASSUME(display != NULL);

    /// @see MONITORINFO | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-monitorinfo
    const MONITORINFO* const info = (const MONITORINFO*)display;

    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)info->rcMonitor.left, .y = (wyn_coord_t)(info->rcMonitor.top) },
        .extent = { .w = (wyn_coord_t)(info->rcMonitor.right - info->rcMonitor.left), .h = (wyn_coord_t)(info->rcMonitor.bottom - info->rcMonitor.top) }
    };
}

// ================================================================================================================================

extern void* wyn_native_context(wyn_window_t const window WYN_UNUSED)
{
    WYN_ASSUME(window != NULL);

    return wyn_win32.hinstance;
}

// ================================================================================================================================

extern const wyn_vb_mapping_t* wyn_vb_mapping(void)
{
    /// @see MK_* | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-lbuttondown#parameters
    static const wyn_vb_mapping_t mapping = {
        [wyn_vb_left]   = MK_LBUTTON, 
        [wyn_vb_right]  = MK_RBUTTON,
        [wyn_vb_middle] = MK_MBUTTON,
    };
    return &mapping;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern const wyn_vk_mapping_t* wyn_vk_mapping(void)
{
    /// @see VK_* | <Windows.h> <winuser.h> (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
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
        [wyn_vk_NumpadAdd]      = VK_ADD,
        [wyn_vk_NumpadSubtract] = VK_SUBTRACT,
        [wyn_vk_NumpadMultiply] = VK_MULTIPLY,
        [wyn_vk_NumpadDivide]   = VK_DIVIDE,
        [wyn_vk_NumpadDecimal]  = VK_DECIMAL,
    };
    return &mapping;
}

// ================================================================================================================================
