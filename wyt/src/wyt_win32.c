/**
 * @file wyt_win32.c
 * @brief Implementation of Wyt for the Win32 backend.
 */

#define WIN32_LEAN_AND_MEAN

#include <wyt.h>

#include <stddef.h>
#include <stdint.h>

#include <Windows.h>
#include <process.h>

#if __STDC_VERSION__ <= 201710L
    #ifdef true
        #undef true
    #endif
    #ifdef false
        #undef false
    #endif
    #define true ((wyt_bool_t)1)
    #define false ((wyt_bool_t)0)
#endif

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef _VC_NODEFAULTLIB
    /// @see FatalExit | <Windows.h> <winbase.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-fatalexit
    #define WYT_ASSERT(expr) if (expr) {} else FatalExit(1)
#else
    /// @see abort | <stdlib.h> <process.h> [CRT] | https://en.cppreference.com/w/c/program/abort | https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort
    #define WYT_ASSERT(expr) if (expr) {} else abort()
#endif

#ifdef NDEBUG
    #define WYT_ASSUME(expr) ((void)0)
#else
    #define WYT_ASSUME(expr) WYT_ASSERT(expr)
#endif

#if defined(unreachable)
    /// @see unreachable | (C23) | https://en.cppreference.com/w/c/program/unreachable 
    #define WYT_UNREACHABLE() unreachable()
#elif defined(__GNUC__) || defined(__clang__)
    /// @see __builtin_unreachable | (GCC) | https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005funreachable
    /// @see __builtin_unreachable | (Clang) | https://clang.llvm.org/docs/LanguageExtensions.html#builtin-unreachable
    #define WYT_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    /// @see __assume | (MSVC) | https://learn.microsoft.com/en-us/cpp/intrinsics/assume
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

extern wyt_utime_t wyt_nanotime(void)
{
    /// @see LARGE_INTEGER | <Windows.h> <winnt.h> | https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-large_integer-r1
    LARGE_INTEGER qpf, qpc;
    /// @see QueryPerformanceCounter | <Windows.h> <profileapi.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter
    (void)QueryPerformanceCounter(&qpc);
    /// @see QueryPerformanceFrequency | <Windows.h> <profileapi.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency
    (void)QueryPerformanceFrequency(&qpf);

    if ((wyt_utime_t)qpf.QuadPart == 10000000uLL) // Optimize for common 10 MHz frequency.
        return (wyt_utime_t)qpc.QuadPart * 100uLL;
    else
        return wyt_scale((wyt_utime_t)qpc.QuadPart, 1000000000uLL, (wyt_utime_t)qpf.QuadPart);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_nanosleep_for(wyt_stime_t const duration)
{
    if (duration <= 0) return;

    /// @see CreateWaitableTimerExW | <Windows.h> <synchapi.h> [Kernel32] (Windows Vista) | https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw
    const HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, SYNCHRONIZE | TIMER_MODIFY_STATE);
    WYT_ASSERT(timer != NULL);

    /// @see SetWaitableTimer | <Windows.h> <synchapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer
    const LARGE_INTEGER due = { .QuadPart = duration / -100 };
    const BOOL res1 = SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
    WYT_ASSERT(res1 != 0);

    /// @see WaitForSingleObject | <Windows.h> <synchapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
    const DWORD obj = WaitForSingleObject(timer, INFINITE);
    WYT_ASSERT(obj == WAIT_OBJECT_0);
        
    /// @see CloseHandle | <Windows.h> <handleapi.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
    const BOOL res2 = CloseHandle(timer);
    WYT_ASSERT(res2 != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_nanosleep_until(wyt_utime_t const timepoint)
{
    wyt_nanosleep_for((wyt_stime_t)(timepoint - wyt_nanotime()));
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_yield(void)
{
    /// @see Sleep | <Windows.h> <synchapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep
    Sleep(0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_thread_t wyt_spawn(wyt_entry_t const func, void* const arg)
{
    WYT_ASSUME(func != NULL);
    
#ifdef _VC_NODEFAULTLIB
    /// @see CreateThread | <Windows.h> <processthreadsapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread
    const HANDLE handle = CreateThread(NULL, 0, func, arg, 0, NULL);
#else
    /// @see _beginthreadex | <process.h> [CRT] | https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/beginthread-beginthreadex
    const uintptr_t handle = _beginthreadex(NULL, 0, func, arg, 0, NULL);
#endif
    return (wyt_thread_t)handle;
}

// --------------------------------------------------------------------------------------------------------------------------------

WYT_NORETURN extern void wyt_exit(wyt_retval_t const retval)
{
#ifdef _VC_NODEFAULTLIB
    /// @see ExitThread | <Windows.h> <processthreadsapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitthread
    ExitThread(retval);
#else
    /// @see _endthreadex | <process.h> [CRT] | https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/endthread-endthreadex
    _endthreadex(retval);
#endif

    WYT_UNREACHABLE();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_retval_t wyt_join(wyt_thread_t const thread)
{
    WYT_ASSUME(thread != NULL);
    const HANDLE handle = (HANDLE)thread;

    /// @see WaitForSingleObject | <Windows.h> <synchapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
    const DWORD res_wait = WaitForSingleObject(handle, INFINITE);
    WYT_ASSERT(res_wait == WAIT_OBJECT_0);

    /// @see GetExitCodeThread | <Windows.h> <processthreadsapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodethread
    DWORD retval;
    const BOOL res_exit = GetExitCodeThread(handle, &retval);
    WYT_ASSERT(res_exit != 0);

    /// @see CloseHandle | <Windows.h> <handleapi.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
    const BOOL res_close = CloseHandle(handle);
    WYT_ASSERT(res_close != 0);

    return (wyt_retval_t)retval;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_detach(wyt_thread_t const thread)
{
    WYT_ASSUME(thread != NULL);
    const HANDLE handle = (HANDLE)thread;

    /// @see CloseHandle | <Windows.h> <handleapi.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
    const BOOL res = CloseHandle(handle);
    WYT_ASSERT(res != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_tid_t wyt_tid(void)
{
    /// @see GetCurrentThreadId | <Windows.h> <processthreadsapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
    return (wyt_tid_t)GetCurrentThreadId();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_pid_t wyt_pid(void)
{
    /// @see GetCurrentProcessId | <Windows.h> <processthreadsapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocessid
    return (wyt_pid_t)GetCurrentProcessId();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_sem_t wyt_sem_create(const int maximum, const int initial)
{
    if ((maximum < initial) || (maximum < 0) || (initial < 0)) return NULL;
    
    /// @see CreateSemaphoreExW | <Windows.h> <winbase.h> [Kernel32] (Windows Vista) | https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createsemaphoreexa
    const HANDLE handle = CreateSemaphoreExW(NULL, (LONG)initial, (LONG)maximum, NULL, 0, SYNCHRONIZE | SEMAPHORE_MODIFY_STATE);
    return (wyt_sem_t)handle;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_sem_destroy(wyt_sem_t const sem)
{
    WYT_ASSUME(sem != NULL);
    const HANDLE handle = (HANDLE)sem;

    /// @see CloseHandle | <Windows.h> <handleapi.h> [Kernel32] (Windows 2000) | https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
    const BOOL res = CloseHandle(handle);
    WYT_ASSERT(res != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_bool_t wyt_sem_release(wyt_sem_t const sem)
{
    WYT_ASSUME(sem != NULL);
    const HANDLE handle = (HANDLE)sem;

    /// @see ReleaseSemaphore | <Windows.h> <synchapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-releasesemaphore
    return ReleaseSemaphore(handle, 1, NULL) != 0;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_sem_acquire(wyt_sem_t const sem)
{
    WYT_ASSUME(sem != NULL);
    const HANDLE handle = (HANDLE)sem;

    /// @see WaitForSingleObject | <Windows.h> <synchapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
    const DWORD res = WaitForSingleObject(handle, INFINITE);
    WYT_ASSERT(res == WAIT_OBJECT_0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_bool_t wyt_sem_try_acquire(wyt_sem_t const sem)
{
    WYT_ASSUME(sem != NULL);
    const HANDLE handle = (HANDLE)sem;
    
    /// @see WaitForSingleObject | <Windows.h> <synchapi.h> [Kernel32] (Windows XP) | https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
    const DWORD res = WaitForSingleObject(handle, 0);
    return res == WAIT_OBJECT_0;
}

// ================================================================================================================================
