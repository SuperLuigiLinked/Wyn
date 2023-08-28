/**
 * @file wyt_pthreads.c
 * @brief Implementation of Wyt for the Pthreads backend.
 */

#include "wyt.h"

#ifndef __APPLE__
    #define _GNU_SOURCE
#endif

#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#if defined(__APPLE__)
    #include <dispatch/dispatch.h>
#else
    #include <semaphore.h>
#endif

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/abort.3.html
 * @see Apple:
 * - https://www.manpagez.com/man/3/abort/
 */
#define WYT_ASSERT(expr) if (expr) {} else abort()

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
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/clock_gettime.3.html
 * @see Apple:
 * - https://www.manpagez.com/man/3/clock_gettime_nsec_np/
 */
extern wyt_time_t wyt_nanotime(void)
{
#ifdef __APPLE__
    const uint64_t res = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
    WYT_ASSERT(res != 0);

    return (wyt_time_t)res;
#else
    struct timespec tp;
    const int res = clock_gettime(CLOCK_BOOTTIME, &tp);
    WYT_ASSERT(res == 0);
    
    return (wyt_time_t)tp.tv_sec * 1'000'000'000uLL + (wyt_time_t)tp.tv_nsec;
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Apple:
 * - https://www.manpagez.com/man/2/nanosleep/
 */
extern void wyt_nanosleep_for(wyt_duration_t duration)
{
    if (duration <= 0) return;

#ifdef __APPLE__
    struct timespec dur = {
        .tv_sec = (wyt_time_t)duration / 1'000'000'000uLL,
        .tv_nsec = (wyt_time_t)duration % 1'000'000'000uLL,
    };

    int res;
    do {
        res = nanosleep(&dur, &dur);
    } while ((res == -1) && (errno == EINTR));
    
    WYT_ASSERT((res != -1));
#else
    wyt_nanosleep_until(wyt_nanotime() + (wyt_time_t)duration);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/clock_nanosleep.2.html
 */
extern void wyt_nanosleep_until(wyt_time_t timepoint)
{
#ifdef __APPLE__
    wyt_nanosleep_for((wyt_duration_t)(timepoint - wyt_nanotime()));
#else
    const struct timespec tp = {
        .tv_sec = timepoint / 1'000'000'000uLL,
        .tv_nsec = timepoint % 1'000'000'000uLL,
    };
    
    int res;
    do {
        res = clock_nanosleep(CLOCK_BOOTTIME, TIMER_ABSTIME, &tp, NULL);
    } while ((res == -1) && (errno == EINTR));

    WYT_ASSERT((res != -1));
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/sched_yield.2.html
 * @see Posix:
 * - https://man7.org/linux/man-pages/man3/sched_yield.3p.html
 */
extern void wyt_yield(void)
{
    [[maybe_unused]] const int res = sched_yield();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/pthread_create.3.html
 * @see Apple:
 * - https://www.manpagez.com/man/3/pthread_create/
 */
extern wyt_thread_t wyt_spawn(wyt_entry_t func, void* arg)
{
    pthread_t thread;
    const int res = pthread_create(&thread, NULL, func, arg);
    if (res != 0) return NULL;

    // Assumes `pthread_t` is an integer/pointer.
    _Static_assert(sizeof(pthread_t) <= sizeof(wyt_thread_t));

    return (wyt_thread_t)thread;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/pthread_exit.3.html
 * @see Apple:
 * - https://www.manpagez.com/man/3/pthread_exit/
 */
WYT_NORETURN extern void wyt_exit(wyt_retval_t retval)
{
    pthread_exit(retval);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/pthread_join.3.html
 * @see Apple:
 * - https://www.manpagez.com/man/3/pthread_join/
 */
extern wyt_retval_t wyt_join(wyt_thread_t thread)
{
    wyt_retval_t retval;
    const int res = pthread_join((pthread_t)thread, &retval);
    WYT_ASSERT(res == 0);
    return retval;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/pthread_detach.3.html
 * @see Apple:
 * - https://www.manpagez.com/man/3/pthread_detach/
 */
extern void wyt_detach(wyt_thread_t thread)
{
    const int res = pthread_detach((pthread_t)thread);
    WYT_ASSERT(res == 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/gettid.2.html
 * @see Apple:
 * - https://www.manpagez.com/man/3/pthread_threadid_np/
 */
extern wyt_tid_t wyt_tid(void)
{
#ifdef __APPLE__
    uint64_t tid;
    const int res = pthread_threadid_np(NULL, &tid);
    WYT_ASSERT(res == 0);
#else
    const pid_t tid = gettid();
#endif
    return (wyt_tid_t)tid;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/getpid.2.html
 * @see Apple:
 * - https://www.manpagez.com/man/2/getpid/
 */
extern wyt_pid_t wyt_pid(void)
{
    return (wyt_pid_t)getpid();   
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/sem_init.3.html
 * @see Apple:
 * - https://developer.apple.com/documentation/dispatch/1452955-dispatch_semaphore_create
 */
extern wyt_sem_t wyt_sem_create(int maximum, int initial)
{
    if ((maximum < initial) || (maximum < 0) || (initial < 0)) return NULL;

#if defined(__APPLE__)
    const dispatch_semaphore_t obj = dispatch_semaphore_create((intptr_t)initial);

    return (wyt_sem_t)obj;
#else
    if (maximum > SEM_VALUE_MAX) return NULL;

    sem_t* const ptr = malloc(sizeof(sem_t));
    if (ptr == NULL) return NULL;

    const int res = sem_init(ptr, 0, (unsigned int)initial);
    WYT_ASSERT(res == 0);

    return (wyt_sem_t)ptr;
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/sem_destroy.3.html
 * @see Apple:
 * - https://developer.apple.com/documentation/dispatch/1496328-dispatch_release
 */
extern void wyt_sem_destroy(wyt_sem_t sem)
{
#if defined(__APPLE__)
    const dispatch_semaphore_t obj = (dispatch_semaphore_t)sem;

    dispatch_release(obj);
#else
    sem_t* const ptr = (sem_t*)sem;

    const int res = sem_destroy(ptr);
    WYT_ASSERT(res == 0);

    free(ptr);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/sem_post.3.html
 * @see Apple:
 * - https://developer.apple.com/documentation/dispatch/1452919-dispatch_semaphore_signal
 */
extern WYT_BOOL wyt_sem_release(wyt_sem_t sem)
{
#if defined(__APPLE__)
    const dispatch_semaphore_t obj = (dispatch_semaphore_t)sem;

    [[maybe_unused]] const intptr_t res = dispatch_semaphore_signal(obj);
    return true; // Assume success (did not overflow)
#else
    sem_t* const ptr = (sem_t*)sem;

    const int res = sem_post(ptr);
    return res == 0;
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/sem_wait.3.html
 * @see Apple:
 * - https://developer.apple.com/documentation/dispatch/1453087-dispatch_semaphore_wait
 */
extern void wyt_sem_acquire(wyt_sem_t sem)
{
#if defined(__APPLE__)
    const dispatch_semaphore_t obj = (dispatch_semaphore_t)sem;

    const intptr_t res = dispatch_semaphore_wait(obj, DISPATCH_TIME_FOREVER);
    WYT_ASSERT(res == 0);
#else
    sem_t* const ptr = (sem_t*)sem;

    int res;
    do { res = sem_wait(ptr); } while ((res != 0) && (errno == EINTR));

    WYT_ASSERT(res == 0);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/sem_wait.3.html
 * @see Apple:
 * - https://developer.apple.com/documentation/dispatch/1453087-dispatch_semaphore_wait
 */
extern WYT_BOOL wyt_sem_try_acquire(wyt_sem_t sem)
{
#if defined(__APPLE__)
    const dispatch_semaphore_t obj = (dispatch_semaphore_t)sem;

    const intptr_t res = dispatch_semaphore_wait(obj, DISPATCH_TIME_NOW);
    return res == 0;
#else
    sem_t* const ptr = (sem_t*)sem;

    int res;
    do { res = sem_trywait(ptr); } while ((res != 0) && (errno == EINTR));

    if (res == 0) return true;
    
    WYT_ASSERT(errno == EAGAIN);
    return false;
#endif
}

// ================================================================================================================================
