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
 */

// ================================================================================================================================ //
// Type Declarations
// -------------------------------------------------------------------------------------------------------------------------------- //

/*
 * Unsigned Integer capable of holding Timepoints.
 */
typedef unsigned long long wyt_time_t;

/*
 * Signed Integer capable of holding the difference between Timepoints.
 */
typedef signed long long wyt_duration_t;

/*
 * Handle to a Thread.
 */
typedef void* wyt_thread_t;

// ================================================================================================================================ //
// API Functions
// -------------------------------------------------------------------------------------------------------------------------------- //

/**
 * Sleeps the current thread for `duration` nanoseconds.
 */
extern void wyt_nanosleep_for(wyt_duration_t duration);

/**
 * Sleeps the current thread until `timepoint` has passed.
 */
extern void wyt_nanosleep_until(wyt_time_t timepoint);

/**
 * Returns the current processor time with nanosecond resolution.
 */
extern wyt_time_t wyt_nanotime(void);

/**
 * Spawns a new thread.
 */
extern wyt_thread_t wyt_spawn(void (*func)(void*), void* arg);

/**
 * Waits until the specified thread has terminated.
 */
extern void wyt_join(wyt_thread_t thread);

// ================================================================================================================================ //

#ifdef __cplusplus
}
#endif

#endif
