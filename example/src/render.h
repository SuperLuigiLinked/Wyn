/**
 * @file render.h
 */

#pragma once

#include "utils.h"

#if defined(WYN_EXAMPLE_GL)
#   include <wyg.h>
#   define GL_GLEXT_PROTOTYPES
#   include <GL/glcorearb.h>
#endif

// ================================================================================================================================

struct Render
{
    Common* common;
    const Update* state;

    uint64_t frame;
    wyt_time_t frame_ts;
    wyt_time_t frame_te;

#if defined(WYN_EXAMPLE_GL)
    wyg_context_t* context;
#endif
};

// ================================================================================================================================
