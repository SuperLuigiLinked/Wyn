/**
 * @file utils.h
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <math.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include <wyt.h>
#include <wyn.h>

// ================================================================================================================================

#define ASSERT(expr) if (expr) {} else abort()

#define LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// --------------------------------------------------------------------------------------------------------------------------------

enum : wyt_time_t { nanos_per_second = 1'000'000'000 };
enum : wyt_time_t { frames_per_second = 60 };

// ================================================================================================================================

typedef struct Common Common;
typedef struct Events Events;
typedef struct Update Update;
typedef struct Render Render;

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_run(void);
extern void events_loop(void* common);
extern wyt_retval_t WYT_ENTRY update_loop(void* common);
extern wyt_retval_t WYT_ENTRY render_loop(void* common);

// ================================================================================================================================
