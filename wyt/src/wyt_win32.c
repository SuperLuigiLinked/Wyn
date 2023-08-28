/**
 * @file wyt_win32.c
 * @brief Implementation of Wyt for the Win32 backend.
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
    /**
     * @see Win32:
     * - https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-fatalexit
     */
    #define WYT_ASSERT(expr) if (expr) {} else FatalExit(1)
#else
    /**
     * @see C:
     * - https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort
     */
    #define WYT_ASSERT(expr) if (expr) {} else abort()
#endif

#if defined(__GNUC__) || defined(__clang__)
    /**
     * @see GCC:
     * - https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005funreachable
     * @see Clang:
     * - https://clang.llvm.org/docs/LanguageExtensions.html#builtin-unreachable
     */
    #define WYT_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    /**
     * @see MSVC:
     * - https://learn.microsoft.com/en-us/cpp/intrinsics/assume
     */
    #define WYT_UNREACHABLE() __assume(0)
#else
    #define WYT_UNREACHABLE() WYT_ASSERT(false)
#endif

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter
 * - https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency
 */
extern wyt_time_t wyt_nanotime(void)
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

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw
 * - https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer
 * - https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
 * - https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
 */
extern void wyt_nanosleep_for(wyt_duration_t duration)
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

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep
 */
extern void wyt_yield(void)
{
    Sleep(0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread
 * - https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/beginthread-beginthreadex
 * - https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheap
 * - https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc
 */
extern wyt_thread_t wyt_spawn(wyt_entry_t func, void* arg)
{
#ifdef _VC_NODEFAULTLIB
    const HANDLE handle = CreateThread(NULL, 0, func, arg, 0, NULL);
#else
    const uintptr_t handle = _beginthreadex(NULL, 0, func, arg, 0, NULL);
#endif
    return (wyt_thread_t)handle;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitthread
 * - https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/endthread-endthreadex
 */
WYT_NORETURN extern void wyt_exit(wyt_retval_t retval)
{
#ifdef _VC_NODEFAULTLIB
    ExitThread(retval);
#else
    _endthreadex(retval);
#endif

    WYT_UNREACHABLE();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
 * - https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodethread
 * - https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
 */
extern wyt_retval_t wyt_join(wyt_thread_t thread)
{
    const HANDLE handle = (HANDLE)thread;

    const DWORD res_wait = WaitForSingleObject(handle, INFINITE);
    WYT_ASSERT(res_wait == WAIT_OBJECT_0);

    DWORD retval;
    const BOOL res_exit = GetExitCodeThread(handle, &retval);
    WYT_ASSERT(res_exit != 0);

    const BOOL res_close = CloseHandle(handle);
    WYT_ASSERT(res_close != 0);

    return (wyt_retval_t)retval;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
 */
extern void wyt_detach(wyt_thread_t thread)
{
    const HANDLE handle = (HANDLE)thread;

    const BOOL res = CloseHandle(handle);
    WYT_ASSERT(res != FALSE);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
 */
extern wyt_tid_t wyt_tid(void)
{
    return (wyt_tid_t)GetCurrentThreadId();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocessid
 */
extern wyt_pid_t wyt_pid(void)
{
    return (wyt_pid_t)GetCurrentProcessId();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createsemaphoreexa
 */
extern wyt_sem_t wyt_sem_create(unsigned int maximum, unsigned int initial)
{
    const HANDLE handle = CreateSemaphoreExW(NULL, (LONG)initial, (LONG)maximum, NULL, 0, SYNCHRONIZE | SEMAPHORE_MODIFY_STATE);
    return (wyt_sem_t)handle;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
 */
extern void wyt_sem_destroy(wyt_sem_t sem)
{
    const HANDLE handle = (HANDLE)sem;
    const BOOL res = CloseHandle(handle);
    WYT_ASSERT(res != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-releasesemaphore
 */
extern WYT_BOOL wyt_sem_release(wyt_sem_t sem)
{
    const HANDLE handle = (HANDLE)sem;
    return ReleaseSemaphore(handle, 1, NULL) != FALSE;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - 
 */
extern void wyt_sem_acquire(wyt_sem_t sem)
{
    const HANDLE handle = (HANDLE)sem;
    const DWORD res = WaitForSingleObject(handle, INFINITE);
    WYT_ASSERT(res == WAIT_OBJECT_0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Win32:
 * - 
 */
extern WYT_BOOL wyt_sem_try_acquire(wyt_sem_t sem)
{
    const HANDLE handle = (HANDLE)sem;
    const DWORD res = WaitForSingleObject(handle, 0);
    return res == WAIT_OBJECT_0;
}

// ================================================================================================================================
