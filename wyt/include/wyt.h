/*
    Wyt.h
*/

#pragma once

#ifndef WYT_H
#define WYT_H 1

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  WYT: Cross-Platform Threading/Timing Library.
 *
 *  All pointers/handles passed to functions must be NON-NULL, and all return values will be NON-NULL, unless otherwise specified.
 */

// ================================================================================================================================
//  Macros
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
    #if __cplusplus >= 201103L              // C++11
        #define WYT_NORETURN [[noreturn]]
    #else
        #define WYT_NORETURN
    #endif
#else
    #if __STDC_VERSION__ >= 202311L         // C23
        #define WYT_NORETURN [[noreturn]]
    #elif __STDC_VERSION__ >= 201112L       // C11
        #define WYT_NORETURN _Noreturn
    #else
        #define WYT_NORETURN
    #endif
#endif

// ================================================================================================================================
//  Type Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * Unsigned Integer capable of holding Timepoints.
 */
typedef unsigned long long wyt_time_t;

/**
 * Signed Integer capable of holding the difference between Timepoints.
 */
typedef signed long long wyt_duration_t;

/**
 * Handle to a Thread.
 */
typedef void* wyt_thread_t;

/**
 * Integer capable of holding a unique Thread Identifier.
 */
typedef unsigned long long wyt_tid_t;

// ================================================================================================================================
//  API Functions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * Returns a nanosecond timepoint (relative to an unspecified epoch) from a monotonic clock.
 */
extern wyt_time_t wyt_nanotime(void);

/**
 * Sleeps the current thread for at least `duration` nanoseconds.
 */
extern void wyt_nanosleep_for(wyt_duration_t duration);

/**
 * Sleeps the current thread until at least `timepoint` has passed.
 */
extern void wyt_nanosleep_until(wyt_time_t timepoint);

/**
 * Yields execution of the current thread temporarily.
 */
extern void wyt_yield(void);

/**
 * Attempts to spawn a new thread, which then passes the provided (potentially NULL) `arg` to the `func` function.
 * The thread handle must either be passed to `wyt_join` or `wyt_detach` in order to not leak resources.
 * This function will return NULL if it is unable to create a new thread.
 */
extern wyt_thread_t wyt_spawn(void (*func)(void*), void* arg);

/**
 * Terminates the current thread.
 * The effects are the same as returning from the thread's start function.
 */
WYT_NORETURN extern void wyt_exit(void);

/**
 * Waits until the specified thread has terminated.
 * After calling this function, the thread handle is invalid and must not be used.
 * A thread must not attempt to join itself.
 */
extern void wyt_join(wyt_thread_t thread);

/**
 * Detaches the specified thread, allowing it to execute independently.
 * After calling this function, the thread handle is invalid and must not be used.
 * A thread must not attempt to detach itself.
 */
extern void wyt_detach(wyt_thread_t thread);

/**
 * Returns the Thread ID for the Current Thread.
 */
extern wyt_tid_t wyt_current_tid(void);

// ================================================================================================================================

#ifdef __cplusplus
}
#endif

#endif
