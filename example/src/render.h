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
    uint64_t frame;
    wyn_window_t window;

#if defined(WYG_EGL)
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
#endif
};

// ================================================================================================================================
