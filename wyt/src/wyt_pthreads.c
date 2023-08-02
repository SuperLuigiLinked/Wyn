/*
    Wyt - Pthreads.c
*/

#include "wyt.h"

#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <pthread.h>
#include <sched.h>

// ================================================================================================================================
//  Macros
// --------------------------------------------------------------------------------------------------------------------------------

// https://man7.org/linux/man-pages/man3/abort.3.html
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
     * https://man7.org/linux/man-pages/man3/clock_gettime.3.html
     */
{
    struct timespec tp;
    const int res = clock_gettime(CLOCK_BOOTTIME, &tp);
    WYT_ASSERT(res == 0);

    return (wyt_time_t)tp.tv_sec * 1'000'000'000uLL + (wyt_time_t)tp.tv_nsec;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_nanosleep_for(wyt_duration_t duration)
{
    if (duration <= 0) return;

    wyt_nanosleep_until(wyt_nanotime() + (wyt_time_t)duration);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_nanosleep_until(wyt_time_t timepoint)
    /*
     * https://man7.org/linux/man-pages/man2/clock_nanosleep.2.html
     */
{
    const struct timespec tp = {
        .tv_sec = timepoint / 1'000'000'000uLL,
        .tv_nsec = timepoint % 1'000'000'000uLL,
    };
    
    int res;
    do {
        res = clock_nanosleep(CLOCK_BOOTTIME, TIMER_ABSTIME, &tp, NULL);
    } while ((res == -1) && (errno == EINTR));
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_yield(void)
    /*
     * https://man7.org/linux/man-pages/man2/sched_yield.2.html
     */
{
    (void)sched_yield();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_thread_t wyt_spawn(void (*func)(void*), void* arg)
    /*
     * https://man7.org/linux/man-pages/man3/malloc.3.html
     * https://man7.org/linux/man-pages/man3/pthread_create.3.html
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
     * https://man7.org/linux/man-pages/man3/pthread_exit.3.html
     */
{
    pthread_exit(NULL);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_join(wyt_thread_t thread)
    /*
     * https://man7.org/linux/man-pages/man3/pthread_join.3.html
     */
{
    const int res = pthread_join((pthread_t)thread, NULL);
    WYT_ASSERT(res == 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyt_detach(wyt_thread_t thread)
    /*
     * https://man7.org/linux/man-pages/man3/pthread_detach.3.html
     */
{
    const int res = pthread_detach((pthread_t)thread);
    WYT_ASSERT(res == 0);
}

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

inline static void* wyt_thread_entry(void* args)
    /*
     * https://man7.org/linux/man-pages/man3/free.3.html
     */
{
    struct wyt_thread_args thunk = *(struct wyt_thread_args*)args;
    
    free(args);

    thunk.func(thunk.arg);
    return NULL;
}

// ================================================================================================================================
