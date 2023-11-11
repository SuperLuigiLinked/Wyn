/**
 * @file app.h
 */

#pragma once

// ================================================================================================================================

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <math.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include <wyt.h>
#include <wyn.h>

// --------------------------------------------------------------------------------------------------------------------------------

#define ASSERT(expr) if (expr) {} else abort()

#define LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// ================================================================================================================================

struct App
{
    wyt_time_t epoch;
    
    wyn_window_t window;

    uint64_t num_events;
};
typedef struct App App;

// ================================================================================================================================