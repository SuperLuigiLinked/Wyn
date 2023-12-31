/**
 * @file wyt.h
 * @brief Wyt: Cross-Platform Threading/Timing Library.
 */

#pragma once

#ifndef WYT_H
#define WYT_H

// ================================================================================================================================
//  Macros
// --------------------------------------------------------------------------------------------------------------------------------

#if defined(__cplusplus)
    #if (__cplusplus >= 200809L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 200809L))
        /// @see [[noreturn]] | (C++11) | https://en.cppreference.com/w/cpp/feature_test
        #define WYT_NORETURN [[noreturn]]
    #endif
#elif defined(__STDC_VERSION__)
    #if (__STDC_VERSION__ >= 202202L)
        /// @see [[noreturn]] | (C23) | https://en.cppreference.com/w/c/language/attributes
        #define WYT_NORETURN [[noreturn]]
    #elif (__STDC_VERSION__ >= 201112L)
        /// @see _Noreturn | (C11) | https://en.cppreference.com/w/c/language/_Noreturn
        #define WYT_NORETURN _Noreturn
    #endif
#endif
#ifndef WYT_NORETURN
    #define WYT_NORETURN
#endif

/**
 * @brief Language-agnostic boolean type.
 */
#ifdef __cplusplus
    typedef bool wyt_bool_t;
#else
    typedef _Bool wyt_bool_t;
#endif

// ================================================================================================================================
//  Type Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Unsigned Integer capable of holding Timepoints.
 */
typedef unsigned long long wyt_utime_t;

/**
 * @brief Signed Integer capable of holding the difference between Timepoints.
 */
typedef signed long long wyt_stime_t;

/**
 * @brief Handle to a Thread.
 */
typedef void* wyt_thread_t;

/**
 * @brief Return Value type for Wyt Threads.
 * @details Guaranteed to be either an Integer or a Pointer.
 * @warning There are no guarantees on the size of this type. (Potentially smaller than a pointer)
 */
#ifdef _WIN32
    #ifdef _VC_NODEFAULTLIB
        typedef unsigned long wyt_retval_t;
    #else
        typedef unsigned int wyt_retval_t;
    #endif
#else
    typedef void* wyt_retval_t;
#endif

/**
 * @brief Calling Convention for Wyt Threads.
 */
#ifdef _WIN32
    #define WYT_ENTRY __stdcall
#else
    #define WYT_ENTRY /* __cdecl */
#endif

/**
 * @brief Pointer to a callback function that can serve as an Entry-Point for Wyt Threads.
 */
typedef wyt_retval_t (WYT_ENTRY* wyt_entry_t)(void*);

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

/**
 * @brief Handle to a Semaphore.
 */
typedef void* wyt_sem_t;

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
extern wyt_utime_t wyt_nanotime(void);

/**
 * @brief Sleeps the current thread for at least `duration` nanoseconds.
 * @details If the duration is less than or equal to 0, this function will return immediately.
 * @param duration The duration to sleep for, based on the same clock as `wyt_nanotime`.
 */
extern void wyt_nanosleep_for(wyt_stime_t duration);

/**
 * @brief Sleeps the current thread until at least `timepoint` has passed.
 * @details If the timepoint has already passed, this function will return immediately.
 * @param timepoint The timepoint to sleep until, based on the same clock as `wyt_nanotime`.
 */
extern void wyt_nanosleep_until(wyt_utime_t timepoint);

/**
 * @brief Yields execution of the current thread temporarily.
 */
extern void wyt_yield(void);

/**
 * @brief Attempts to spawn a new thread.
 * @param[in] func [non-null] The entry-function to call on the new thread.
 * @param[in] arg  [nullable] The argument to pass to the thread's entry-function.
 * @return [nullable] NON-NULL handle to the new thread on success, NULL on failure.
 * @warning If successful, the returned handle must be passed to either `wyt_join` or `wyt_detach` in order to not leak resources.
 */
extern wyt_thread_t wyt_spawn(wyt_entry_t func, void* arg);

/**
 * @brief Terminates the current thread.
 * @details The effects are the same as returning from the thread's entry-function.
 * @param retval The value to return to `wyt_join`.
 */
WYT_NORETURN extern void wyt_exit(wyt_retval_t retval);

/**
 * @brief Waits until the specified thread has terminated.
 * @param[in] thread [non-null] A handle to the thread to join.
 * @return The value returned by the thread.
 * @warning After calling this function, the thread handle is invalid and must not be used.
 * @warning A thread must not attempt to join itself.
 */
extern wyt_retval_t wyt_join(wyt_thread_t thread);

/**
 * @brief Detaches the specified thread, allowing it to execute independently.
 * @param[in] thread [non-null] A handle to the thread to detach.
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
 * @brief Attempts to create a new semaphore.
 * @param maximum [positive] The suggested maximum value the internal counter can have.
 * @param initial [non-negative] The initial value of the internal counter.
 * @return [nullable] NON-NULL handle to the new semaphore on success, NULL on failure.
 * @warning If successful, the returned handle must be passed to `wyt_sem_destroy` in order to not leak resources.
 * @warning The actual maximum value may potentially be greater than the passed in value.
 */
extern wyt_sem_t wyt_sem_create(int maximum, int initial);

/**
 * @brief Destroys a semaphore.
 * @param[in] sem [non-null] Handle to a semaphore.
 * @warning After calling this function, the semaphore handle is invalid and must not be used.
 * @warning On some platforms, the internal counter must be greater than or equal its initial value when destroyed, otherwise an error occurs.
 */
extern void wyt_sem_destroy(wyt_sem_t sem);

/**
 * @brief Attempts to increment the semaphore's internal counter.
 * @param[in] sem [non-null] Handle to a semaphore.
 * @return `true` if successful, `false` otherwise.
 * @warning On some platforms, the internal counter can be incremented past the suggested maximum.
 */
extern wyt_bool_t wyt_sem_release(wyt_sem_t sem);

/**
 * @brief Decrements the semaphore's internal counter, blocking until successful.
 * @param[in] sem [non-null] Handle to a semaphore.
 */
extern void wyt_sem_acquire(wyt_sem_t sem);

/**
 * @brief Attempts to decrement the semaphore's internal counter.
 * @param[in] sem [non-null] Handle to a semaphore.
 * @return `true` if successful, `false` otherwise. 
 */
extern wyt_bool_t wyt_sem_try_acquire(wyt_sem_t sem);

/**
 * @brief Scales an Unsigned Integer `val` by a Fraction `num / den`.
 * @details Assumes:
 *            - `den != 0`
 *            - `(den - 1) * num` does not overflow
 * @return The value of `val * (num / den)`, rounded down.
 */
#ifdef __cplusplus
constexpr
#endif
static inline wyt_utime_t wyt_scale(const wyt_utime_t val, const wyt_utime_t num, const wyt_utime_t den)
#ifdef __cplusplus
noexcept
#endif
{
    return ((val / den) * num) + (((val % den) * num) / den);
}

#ifdef __cplusplus
}
#endif

// ================================================================================================================================

#endif
