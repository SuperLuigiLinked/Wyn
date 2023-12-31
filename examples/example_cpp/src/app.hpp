/**
 * @file app.hpp
 */

#pragma once

// ================================================================================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

#include <cstdlib>
#include <cstdio>
#include <cinttypes>

#include <wyt.h>
#include <wyn.h>

// --------------------------------------------------------------------------------------------------------------------------------

#define ASSERT(expr) if (expr) {} else std::abort()

#define LOG(...) static_cast<void>(std::fprintf(stderr, __VA_ARGS__))

// ================================================================================================================================

struct App
{
    wyt_utime_t epoch;
    std::uint64_t num_events;
    
    const wyn_vb_mapping_t* vb_mapping;
    const wyn_vk_mapping_t* vk_mapping;
    wyn_window_t window;

};
typedef struct App App;

// ================================================================================================================================
