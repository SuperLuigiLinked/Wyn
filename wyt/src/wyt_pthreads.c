/*
    Wyt - Pthreads.c
*/

#include "wyt.h"

#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>
#include <pthread.h>
#include <sched.h>

// ================================================================================================================================
//  Macros
// --------------------------------------------------------------------------------------------------------------------------------

/*
 * Linux: https://man7.org/linux/man-pages/man3/abort.3.html
 * Apple: https://www.manpagez.com/man/3/abort/
 */
#define WYT_ASSERT(expr) if (expr) {} else abort()

// ================================================================================================================================
//  Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * Entry point for threads created by `wyt_spawn`.
 * Due to platform API differences, the user-provided function often cannot be called directly.
 * This function acts as a wrapper to unify the different function signatures between platforms.
 */
inline static void* wyt_thread_entry(void* args);

/**
 * Contains all the state necessary to pass arguments to a newly spawned thread in a thread-safe way.
 */
struct wyt_thread_args
{
    void (*func)(void*);
    void* arg;
};

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_time_t wyt_nanotime(void)
    /*
     * Linux: https://man7.org/linux/man-pages/man3/clock_gettime.3.html
     * Apple: https://www.manpagez.com/man/3/clock_gettime_nsec_np/
     */
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

extern void wyt_nanosleep_for(wyt_duration_t duration)
    /*
     * Apple: https://www.manpagez.com/man/2/nanosleep/
     */
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

extern void wyt_nanosleep_until(wyt_time_t timepoint)
    /*
     * Linux: https://man7.org/linux/man-pages/man2/clock_nanosleep.2.html
     */
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

extern void wyt_yield(void)
    /*
     * Linux: https://man7.org/linux/man-pages/man2/sched_yield.2.html
     * POSIX: https://man7.org/linux/man-pages/man3/sched_yield.3p.html
     */
{
    const int res = sched_yield();
    (void)res;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_thread_t wyt_spawn(void (*func)(void*), void* arg)
    /*
     * Linux: https://man7.org/linux/man-pages/man3/malloc.3.html
     *      : https://man7.org/linux/man-pages/man3/pthread_create.3.html
     * Apple: https://www.manpagez.com/man/3/malloc/
     *      : https://www.manpagez.com/man/3/pthread_create/
     */
{
    struct wyt_thread_args* thread_args = malloc(sizeof(struct wyt_thread_args));
    if (thread_args == NULL) return NULL;

    *thread_args = (struct wyt_thread_args){
        .func = func,
        .arg = arg,
    };

    pthread_t thread;
    const int res = pthread_create(&thread, NULL, wyt_thread_entry, thread_args);
    if (res != 0) return NULL;

    // Assumes `pthread_t` is an integer/pointer.
    _Static_assert(sizeof(pthread_t) <= sizeof(wyt_thread_t));

    return (wyt_thread_t)thread;
}

// --------------------------------------------------------------------------------------------------------------------------------

WYT_NORETURN extern void wyt_exit(void)
    /*
     * Linux: https://man7.org/linux/man-pages/man3/pthread_exit.3.html
     * Apple: https://www.manpagez.com/man/3/pthread_exit/
     */
{
    pthread_exit(NULL);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_join(wyt_thread_t thread)
    /*
     * Linux: https://man7.org/linux/man-pages/man3/pthread_join.3.html
     * Apple: https://www.manpagez.com/man/3/pthread_join/
     */
{
    const int res = pthread_join((pthread_t)thread, NULL);
    WYT_ASSERT(res == 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_detach(wyt_thread_t thread)
    /*
     * Linux: https://man7.org/linux/man-pages/man3/pthread_detach.3.html
     * Apple: https://www.manpagez.com/man/3/pthread_detach/
     */
{
    const int res = pthread_detach((pthread_t)thread);
    WYT_ASSERT(res == 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_tid_t wyt_current_tid(void)
    /*
     * Linux: https://man7.org/linux/man-pages/man2/gettid.2.html
     */
{
    const pid_t tid = gettid();
    return (wyt_tid_t)tid;
}

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

inline static void* wyt_thread_entry(void* args)
    /*
     * Linux: https://man7.org/linux/man-pages/man3/free.3.html
     * Apple: https://www.manpagez.com/man/3/free/
     */
{
    struct wyt_thread_args thunk = *(struct wyt_thread_args*)args;
    
    free(args);

    thunk.func(thunk.arg);
    return NULL;
}

// ================================================================================================================================
