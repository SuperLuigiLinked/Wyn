/**
 * @file render.h
 */

#pragma once

#include "utils.h"

#ifdef WYN_EXAMPLE_GL
#   include <wyg_gl.h>
#endif

// ================================================================================================================================

struct Render
{
    Common* common;

    uint64_t frame;
    wyt_time_t frame_ts;
    wyt_time_t frame_te;

#if defined(WYG_WGL)
    HDC hdc;
    HGLRC hglrc;
#elif defined(WYG_EGL)
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
#endif
};

// ================================================================================================================================
