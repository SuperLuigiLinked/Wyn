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

#ifdef WYN_EXAMPLE_GL
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
#endif
};

// ================================================================================================================================
