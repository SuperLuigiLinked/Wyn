/*
    Wyt - Win32.c
*/

#include "wyt.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#ifndef _VC_NODEFAULTLIB
    #include <process.h>
#endif

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef _VC_NODEFAULTLIB
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-fatalexit
     */
    #define WYT_ASSERT(expr) if (expr) {} else FatalExit(1)
#else
    /*
     * https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort
     */
    #define WYT_ASSERT(expr) if (expr) {} else abort()
#endif

#if defined(__GNUC__)
    /*
     * https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005funreachable
     */
    #define WYT_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    /*
     * https://learn.microsoft.com/en-us/cpp/intrinsics/assume
     */
    #define WYT_UNREACHABLE() __assume(false)
#else
    #define WYT_UNREACHABLE() WYT_ASSERT(false)
#endif

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * Scales an Unsigned Integer `val` by a Fraction `num / den`.
 * Assumes:
 *   - `(den - 1) * num` does not overflow
 *   - `den != 0`
 */
inline static wyt_time_t wyt_scale(const wyt_time_t val, const wyt_time_t num, const wyt_time_t den);

/**
 * Entry point for threads created by `wyt_spawn`.
 * Due to platform API differences, the user-provided function often cannot be called directly.
 * This function acts as a wrapper to unify the different function signatures between platforms.
 */
#ifdef _VC_NODEFAULTLIB
inline static DWORD WINAPI wyt_thread_entry(void* args);
#else
inline static unsigned __stdcall wyt_thread_entry(void* args);
#endif

/**
 * Contains all the state necessary to pass arguments to a newly spawned thread in a thread-safe way.
 */
struct wyt_thread_args
{
    void (*func)(void*);
    void* arg;
};

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

inline static wyt_time_t wyt_scale(const wyt_time_t val, const wyt_time_t num, const wyt_time_t den)
{
    const wyt_time_t whole = (val / den) * num;
    const wyt_time_t fract = ((val % den) * num) / den;
    return whole + fract;
}

// --------------------------------------------------------------------------------------------------------------------------------

#ifdef _VC_NODEFAULTLIB
inline static DWORD WINAPI wyt_thread_entry(void* args)
#else
inline static unsigned __stdcall wyt_thread_entry(void* args)
#endif
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheap
     * https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapfree
     */
{
    struct wyt_thread_args thunk = *(struct wyt_thread_args*)args;
    
    const HANDLE heap = GetProcessHeap();
    WYT_ASSERT(heap != NULL);

    const BOOL res = HeapFree(heap, 0, args);
    WYT_ASSERT(res != FALSE);

    thunk.func(thunk.arg);
    return 0;
}

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_time_t wyt_nanotime(void)
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter
     * https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency
     */
{
    LARGE_INTEGER qpf, qpc;
    (void)QueryPerformanceCounter(&qpc);
    (void)QueryPerformanceFrequency(&qpf);

    if ((wyt_time_t)qpf.QuadPart == 10'000'000uLL) // Optimize for common 10 MHz frequency.
        return (wyt_time_t)qpc.QuadPart * 100uLL;
    else
        return wyt_scale((wyt_time_t)qpc.QuadPart, 1'000'000'000uLL, (wyt_time_t)qpf.QuadPart);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_nanosleep_for(wyt_duration_t duration)
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw
     * https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer
     * https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
     * https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
     */
{
    if (duration <= 0) return;

    const HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, SYNCHRONIZE | TIMER_MODIFY_STATE);
    WYT_ASSERT(timer != NULL);

    const LARGE_INTEGER due = { .QuadPart = duration / -100 };
    const BOOL res1 = SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
    WYT_ASSERT(res1 != FALSE);

    const DWORD obj = WaitForSingleObject(timer, INFINITE);
    WYT_ASSERT(obj == WAIT_OBJECT_0);
        
    const BOOL res2 = CloseHandle(timer);
    WYT_ASSERT(res2 != FALSE);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_nanosleep_until(wyt_time_t timepoint)
{
    wyt_nanosleep_for((wyt_duration_t)(timepoint - wyt_nanotime()));
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_yield(void)
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep
     */
{
    Sleep(0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_thread_t wyt_spawn(void (*func)(void*), void* arg)
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread
     * https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/beginthread-beginthreadex
     * https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheap
     * https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc
     */
{
    const HANDLE heap = GetProcessHeap();
    if (heap == NULL) return NULL;

    struct wyt_thread_args* thread_args = HeapAlloc(heap, 0, sizeof(struct wyt_thread_args));
    if (thread_args == NULL) return NULL;

    *thread_args = (struct wyt_thread_args){
        .func = func,
        .arg = arg,
    };

#ifdef _VC_NODEFAULTLIB
    const HANDLE handle = CreateThread(NULL, 0, wyt_thread_entry, thread_args, 0, NULL);
#else
    const uintptr_t handle = _beginthreadex(NULL, 0, wyt_thread_entry, thread_args, 0, NULL);
#endif

    return (wyt_thread_t)handle;
}

// --------------------------------------------------------------------------------------------------------------------------------

WYT_NORETURN extern void wyt_exit(void)
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitthread
     * https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/endthread-endthreadex
     */
{
#ifdef _VC_NODEFAULTLIB
    ExitThread(0);
#else
    _endthreadex(0);
#endif

    WYT_UNREACHABLE();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_join(wyt_thread_t thread)
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
     * https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
     */
{
    const HANDLE handle = (HANDLE)thread;

    const DWORD obj = WaitForSingleObject(handle, INFINITE);
    WYT_ASSERT(obj == WAIT_OBJECT_0);

    const BOOL res = CloseHandle(handle);
    WYT_ASSERT(res != FALSE);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_detach(wyt_thread_t thread)
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
     */
{
    const HANDLE handle = (HANDLE)thread;

    const BOOL res = CloseHandle(handle);
    WYT_ASSERT(res != FALSE);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_tid_t wyt_current_tid(void)
    /*
     * https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
     */
{
    const DWORD tid = GetCurrentThreadId();
    return (wyt_tid_t)tid;
}

// ================================================================================================================================
