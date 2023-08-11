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

/**
 * @brief Integer capable of holding a Process Identifier.
 * @details A Process ID is guaranteed to be unique at least as long as the process is still running.
 */
typedef unsigned long long wyt_pid_t;

#ifdef _WIN32
    #ifdef _VC_NODEFAULTLIB
        /**
         * @brief Return Value type for Wyt Threads.
         * @details Guaranteed to be either an Integer or a Pointer.
         * @warning There are no guarantees on the size of this type. (Potentially smaller than a pointer)
         */
        typedef unsigned long wyt_retval_t;

        /**
         * @brief Calling Convention for Wyt Threads.
         */
        #define WYT_ENTRY __stdcall
    #else
        /**
         * @brief Return Value type for Wyt Threads.
         * @details Guaranteed to be either an Integer or a Pointer.
         * @warning There are no guarantees on the size of this type. (Potentially smaller than a pointer)
         */
        typedef unsigned int wyt_retval_t;

        /**
         * @brief Calling Convention for Wyt Threads.
         */
        #define WYT_ENTRY __stdcall
    #endif
#else
    /**
     * @brief Return Value type for Wyt Threads.
     * @details Guaranteed to be either an Integer or a Pointer.
     * @warning There are no guarantees on the size of this type. (Potentially smaller than a pointer)
     */
    typedef void* wyt_retval_t;

    /**
     * @brief Calling Convention for Wyt Threads.
     */
    #define WYT_ENTRY /* __cdecl */
#endif

/**
 * @brief Pointer to a callback function that can serve as an Entry-Point for Wyt Threads.
 */
typedef wyt_retval_t (WYT_ENTRY* wyt_entry_t)(void*);

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
 * @param func [non-null] The entry-function to call on the new thread.
 * @param arg  [nullable] The argument to pass to the thread's entry-function.
 * @return [nullable] NON-NULL handle to the new thread on success, NULL on failure.
 * @warning If successful, the returned thread handle must either be passed to `wyt_join` or `wyt_detach` in order to not leak resources.
 */
extern wyt_thread_t wyt_spawn(wyt_entry_t func, void* arg);

/**
 * @brief Terminates the current thread.
 * @details The effects are the same as returning from the thread's entry-function.
 * @param retval The value to return to `wyn_join`.
 */
WYT_NORETURN extern void wyt_exit(wyt_retval_t retval);

/**
 * @brief Waits until the specified thread has terminated.
 * @param thread [non-null] A handle to the thread to join.
 * @return The value returned by the thread.
 * @warning After calling this function, the thread handle is invalid and must not be used.
 * @warning A thread must not attempt to join itself.
 */
extern wyt_retval_t wyt_join(wyt_thread_t thread);

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
extern wyt_tid_t wyt_tid(void);

/**
 * @brief Gets the Process ID for the Current Process.
 * @return The Current Process's ID.
 */
extern wyt_pid_t wyt_pid(void);

/**
 * @brief Scales an Unsigned Integer `val` by a Fraction `num / den`.
 * @details Assumes:
 *            - `(den - 1) * num` does not overflow
 *            - `den != 0`
 * @return The value of `val * (num / den)`, rounded down.
 */
inline static wyt_time_t wyt_scale(const wyt_time_t val, const wyt_time_t num, const wyt_time_t den)
{
    const wyt_time_t whole = (val / den) * num;
    const wyt_time_t fract = ((val % den) * num) / den;
    return whole + fract;
}

#ifdef __cplusplus
}
#endif

// ================================================================================================================================

#endif
