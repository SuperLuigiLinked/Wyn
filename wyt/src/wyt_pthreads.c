/**
 * @file wyt_pthreads.c
 * @brief Implementation of Wyt for the Pthreads backend.
 */

#ifndef __APPLE__
    #define _GNU_SOURCE
#endif

#include <wyt.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#ifdef __APPLE__
    #include <dispatch/dispatch.h>
#else
    #include <sched.h>
    #include <semaphore.h>
#endif

#if (__STDC_VERSION__ <= 201710L)
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

/// @see abort | <stdlib.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/program/abort | https://man7.org/linux/man-pages/man3/abort.3.html | https://www.unix.com/man-page/mojave/3/abort/
#define WYT_ASSERT(expr) if (expr) {} else abort()

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
#ifdef __APPLE__
    /// @see clock_gettime_nsec_np | <time.h> [libc] (macOS 10.12) | https://www.unix.com/man-page/mojave/3/clock_gettime_nsec_np/
    const uint64_t res = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
    WYT_ASSERT(res != 0);

    return (wyt_utime_t)res;
#else
    /// @see clock_gettime | <time.h> [libc] (Linux 2.6) | https://man7.org/linux/man-pages/man3/clock_gettime.3.html
    /// @see CLOCK_BOOTTIME | <time.h> (Linux 2.6.39)
    struct timespec tp;
    const int res = clock_gettime(CLOCK_BOOTTIME, &tp);
    WYT_ASSERT(res == 0);
    
    return (wyt_utime_t)tp.tv_sec * 1000000000uLL + (wyt_utime_t)tp.tv_nsec;
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_nanosleep_for(wyt_stime_t const duration)
{
    if (duration <= 0) return;

#ifdef __APPLE__
    struct timespec dur = {
        .tv_sec = (wyt_utime_t)duration / 1000000000uLL,
        .tv_nsec = (wyt_utime_t)duration % 1000000000uLL,
    };

    int res;
    do {
        /// @see nanosleep | <time.h> [libc] (POSIX.1) | https://www.unix.com/man-page/mojave/2/nanosleep/
        res = nanosleep(&dur, &dur);
    } while ((res == -1) && (errno == EINTR));
    
    WYT_ASSERT(res != -1);
#else
    wyt_nanosleep_until(wyt_nanotime() + (wyt_utime_t)duration);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_nanosleep_until(wyt_utime_t const timepoint)
{
#ifdef __APPLE__
    wyt_nanosleep_for((wyt_stime_t)(timepoint - wyt_nanotime()));
#else
    const struct timespec tp = {
        .tv_sec = timepoint / 1000000000uLL,
        .tv_nsec = timepoint % 1000000000uLL,
    };
    
    int res;
    do {
        /// @see clock_nanosleep | <time.h> [libc] (Linux 2.6) | https://man7.org/linux/man-pages/man2/clock_nanosleep.2.html
        /// @see CLOCK_BOOTTIME | <time.h> (Linux 2.6.39)
        res = clock_nanosleep(CLOCK_BOOTTIME, TIMER_ABSTIME, &tp, NULL);
    } while ((res == -1) && (errno == EINTR));

    WYT_ASSERT(res != -1);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_yield(void)
{
#ifdef __APPLE__
    /// @see pthread_yield_np | <pthread.h> [libpthread] (macOS 10.4) | https://www.unix.com/man-page/mojave/3/pthread_yield_np/
    pthread_yield_np();
#else
    /// @see sched_yield | <sched.h> [libc] (POSIX.1) | https://man7.org/linux/man-pages/man3/sched_yield.3p.html | https://man7.org/linux/man-pages/man2/sched_yield.2.html
    const int res = sched_yield();
    (void)(res != -1);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_thread_t wyt_spawn(wyt_entry_t const func, void* const arg)
{
    /// @see pthread_create | <pthread.h> [libpthread] (POSIX.1) (macOS 10.4) | https://man7.org/linux/man-pages/man3/pthread_create.3.html | https://www.unix.com/man-page/mojave/3/pthread_create/
    pthread_t thread;
    const int res = pthread_create(&thread, NULL, func, arg);
    if (res != 0) return NULL;

    // Assumes `pthread_t` is an integer/pointer.
    _Static_assert(sizeof(pthread_t) <= sizeof(wyt_thread_t), "`pthread_t` too large");

    return (wyt_thread_t)thread;
}

// --------------------------------------------------------------------------------------------------------------------------------

WYT_NORETURN extern void wyt_exit(wyt_retval_t const retval)
{
    /// @see pthread_exit | <pthread.h> [libpthread] (POSIX.1) (macOS 10.4) | https://man7.org/linux/man-pages/man3/pthread_exit.3.html | https://www.unix.com/man-page/mojave/3/pthread_exit/
    pthread_exit(retval);
    
    WYT_UNREACHABLE();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_retval_t wyt_join(wyt_thread_t const thread)
{
    /// @see pthread_join | <pthread.h> [libpthread] (POSIX.1) (macOS 10.4) | https://man7.org/linux/man-pages/man3/pthread_join.3.html | https://www.unix.com/man-page/mojave/3/pthread_join/
    wyt_retval_t retval;
    const int res = pthread_join((pthread_t)thread, &retval);
    WYT_ASSERT(res == 0);
    return retval;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_detach(wyt_thread_t const thread)
{
    /// @see pthread_detach | <pthread.h> [libpthread] (POSIX.1) (macOS 10.4) | https://man7.org/linux/man-pages/man3/pthread_detach.3.html | https://www.unix.com/man-page/mojave/3/pthread_detach/
    const int res = pthread_detach((pthread_t)thread);
    WYT_ASSERT(res == 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_tid_t wyt_tid(void)
{
#ifdef __APPLE__
    /// @see pthread_threadid_np | <pthread.h> [libpthread] (macOS 10.6) | https://www.unix.com/man-page/mojave/3/pthread_threadid_np/
    uint64_t tid;
    const int res = pthread_threadid_np(NULL, &tid);
    WYT_ASSERT(res == 0);
#else
    /// @see gettid | <unistd.h> [libc] (Linux 2.4.11) | https://man7.org/linux/man-pages/man2/gettid.2.html
    const pid_t tid = gettid();
#endif
    return (wyt_tid_t)tid;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_pid_t wyt_pid(void)
{
    /// @see getpid | <unistd.h> [libc] (POSIX.1) | https://man7.org/linux/man-pages/man2/getpid.2.html | https://www.unix.com/man-page/mojave/2/getpid/
    return (wyt_pid_t)getpid();   
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_sem_t wyt_sem_create(const int maximum, const int initial)
{
    if ((maximum < initial) || (maximum < 0) || (initial < 0)) return NULL;

#ifdef __APPLE__
    /// @see dispatch_semaphore_create | <dispatch/dispatch.h> [libdispatch] (macOS 10.6) | https://developer.apple.com/documentation/dispatch/1452955-dispatch_semaphore_create
    const dispatch_semaphore_t obj = dispatch_semaphore_create((intptr_t)initial);

    return (wyt_sem_t)obj;
#else
    if (maximum > SEM_VALUE_MAX) return NULL;

    /// @see malloc | <stdlib.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/memory/malloc | https://man7.org/linux/man-pages/man3/malloc.3.html
    sem_t* const ptr = malloc(sizeof(sem_t));
    if (ptr == NULL) return NULL;

    /// @see sem_init | <semaphore.h> [libpthread] (POSIX.1) | https://man7.org/linux/man-pages/man3/sem_init.3.html
    const int res = sem_init(ptr, 0, (unsigned int)initial);
    if (res == 0) return (wyt_sem_t)ptr;

    /// @see free | <stdlib.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/memory/free | https://man7.org/linux/man-pages/man3/free.3.html
    free(ptr);
    return NULL;
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_sem_destroy(wyt_sem_t const sem)
{
    WYT_ASSUME(sem != NULL);

#ifdef __APPLE__
    const dispatch_semaphore_t obj = (dispatch_semaphore_t)sem;

    /// @see dispatch_release | <dispatch/dispatch.h> [libdispatch] (macOS 10.6) | https://developer.apple.com/documentation/dispatch/1496328-dispatch_release
    dispatch_release(obj);
#else
    sem_t* const ptr = (sem_t*)sem;

    /// @see sem_destroy | <semaphore.h> [libpthread] (POSIX.1) | https://man7.org/linux/man-pages/man3/sem_destroy.3.html
    const int res = sem_destroy(ptr);
    WYT_ASSERT(res == 0);

    /// @see free | <stdlib.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/memory/free | https://man7.org/linux/man-pages/man3/free.3.html
    free(ptr);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_bool_t wyt_sem_release(wyt_sem_t const sem)
{
    WYT_ASSUME(sem != NULL);

#if defined(__APPLE__)
    const dispatch_semaphore_t obj = (dispatch_semaphore_t)sem;

    /// @see dispatch_semaphore_signal | <dispatch/dispatch.h> [libdispatch] (macOS 10.6) | https://developer.apple.com/documentation/dispatch/1452919-dispatch_semaphore_signal
    const intptr_t res = dispatch_semaphore_signal(obj);
    (void)(res != 0);

    return true; // Assume success (did not overflow)
#else
    sem_t* const ptr = (sem_t*)sem;

    /// @see sem_post | <semaphore.h> [libpthread] (POSIX.1) | https://man7.org/linux/man-pages/man3/sem_post.3.html
    const int res = sem_post(ptr);
    return res == 0;
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_sem_acquire(wyt_sem_t const sem)
{
    WYT_ASSUME(sem != NULL);

#if defined(__APPLE__)
    const dispatch_semaphore_t obj = (dispatch_semaphore_t)sem;

    /// @see dispatch_semaphore_wait | <dispatch/dispatch.h> [libdispatch] (macOS 10.6) | https://developer.apple.com/documentation/dispatch/1453087-dispatch_semaphore_wait
    const intptr_t res = dispatch_semaphore_wait(obj, DISPATCH_TIME_FOREVER);
    WYT_ASSERT(res == 0);
#else
    sem_t* const ptr = (sem_t*)sem;

    int res;
    do {
        /// @see sem_wait | <semaphore.h> [libpthread] (POSIX.1) | https://man7.org/linux/man-pages/man3/sem_wait.3.html
        res = sem_wait(ptr);
    } while ((res != 0) && (errno == EINTR));

    WYT_ASSERT(res == 0);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_bool_t wyt_sem_try_acquire(wyt_sem_t const sem)
{
    WYT_ASSUME(sem != NULL);

#if defined(__APPLE__)
    const dispatch_semaphore_t obj = (dispatch_semaphore_t)sem;

    /// @see dispatch_semaphore_wait | <dispatch/dispatch.h> [libdispatch] (macOS 10.6) | https://developer.apple.com/documentation/dispatch/1453087-dispatch_semaphore_wait
    const intptr_t res = dispatch_semaphore_wait(obj, DISPATCH_TIME_NOW);
    return res == 0;
#else
    sem_t* const ptr = (sem_t*)sem;

    int res;
    do {
        /// @see sem_wait | <semaphore.h> [libpthread] (POSIX.1) | https://man7.org/linux/man-pages/man3/sem_wait.3.html
        res = sem_trywait(ptr);
    } while ((res != 0) && (errno == EINTR));

    if (res == 0) return true;
    
    WYT_ASSERT(errno == EAGAIN);
    return false;
#endif
}

// ================================================================================================================================
