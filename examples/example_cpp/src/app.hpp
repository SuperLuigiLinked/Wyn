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

// --------------------------------------------------------------------------------------------------------------------------------

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

#if defined(__clang__)
    #define COMPILER "Clang"
#elif defined(__GNUC__)
    #define COMPILER "GCC"
#elif defined(_MSC_VER)
    #define COMPILER "MSVC"
#else
    #define COMPILER "<?>"
#endif

#if defined(_MSVC_LANG)
    #define STANDARD "C++ " STRINGIFY(_MSVC_LANG)
#elif defined(__cplusplus)
    #define STANDARD "C++ " STRINGIFY(__cplusplus)
#elif defined(__STDC_VERSION__)
    #define STANDARD "C " STRINGIFY(__STDC_VERSION__)
#else
    #define STANDARD "<?>"
#endif

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
