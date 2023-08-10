/**
 * @file wyt.h
 *
 * @brief Wyt: Cross-Platform Threading/Timing Library.
 */

#pragma once

#ifndef WYT_H
#define WYT_H

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
 * @brief Unsigned Integer capable of holding Timepoints.
 */
typedef unsigned long long wyt_time_t;

/**
 * @brief Signed Integer capable of holding the difference between Timepoints.
 */
typedef signed long long wyt_duration_t;

/**
 * @brief Handle to a Thread.
 */
typedef void* wyt_thread_t;

/**
 * @brief Integer capable of holding a Thread Identifier.
 * @details A Thread ID is guaranteed to be unique at least as long as the thread is still running.
 */
typedef unsigned long long wyt_tid_t;

// ================================================================================================================================
//  API Functions
// --------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Gets a nanosecond timepoint (relative to an unspecified epoch) from a monotonic clock.
 * @return The approximate timepoint the function was called at.
 */
extern wyt_time_t wyt_nanotime(void);

/**
 * @brief Sleeps the current thread for at least `duration` nanoseconds.
 * @details If the duration is less than or equal to 0, this function will return immediately.
 * @param duration The duration to sleep for, based on the same clock as `wyt_nanotime`.
 */
extern void wyt_nanosleep_for(wyt_duration_t duration);

/**
 * @brief Sleeps the current thread until at least `timepoint` has passed.
 * @details If the timepoint has already passed, this function will return immediately.
 * @param timepoint The timepoint to sleep until, based on the same clock as `wyt_nanotime`.
 */
extern void wyt_nanosleep_until(wyt_time_t timepoint);

/**
 * @brief Yields execution of the current thread temporarily.
 */
extern void wyt_yield(void);

/**
 * @brief Attempts to spawn a new thread.
 * @param func [non-null] The start function to call on the new thread.
 * @param arg  [nullable] The argument to pass to the thread's start function.
 * @return [nullable] NON-NULL handle to the new thread on success, NULL on failure.
 * @warning If successful, the returned thread handle must either be passed to `wyt_join` or `wyt_detach` in order to not leak resources.
 */
extern wyt_thread_t wyt_spawn(void (*func)(void*), void* arg);

/**
 * @brief Terminates the current thread.
 * @details The effects are the same as returning from the thread's start function.
 */
WYT_NORETURN extern void wyt_exit(void);

/**
 * @brief Waits until the specified thread has terminated.
 * @param thread [non-null] A handle to the thread to join.
 * @warning After calling this function, the thread handle is invalid and must not be used.
 * @warning A thread must not attempt to join itself.
 */
extern void wyt_join(wyt_thread_t thread);

/**
 * @brief Detaches the specified thread, allowing it to execute independently.
 * @param thread [non-null] A handle to the thread to detach.
 * @warning After calling this function, the thread handle is invalid and must not be used.
 * @warning A thread must not attempt to detach itself.
 */
extern void wyt_detach(wyt_thread_t thread);

/**
 * @brief Gets the Thread ID for the Current Thread.
 * @return The Current Thread's ID.
 */
extern wyt_tid_t wyt_current_tid(void);

#ifdef __cplusplus
}
#endif

// ================================================================================================================================

#endif
