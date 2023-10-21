/**
 * @file wyg_wgl.c
 */
 
#include "wyg.h"

#include <Windows.h>
#include <gl/GL.h>
#include <GL/wgl.h>

#include <stdlib.h>
#include <stdio.h>

// ================================================================================================================================

/**
 * @see C:
 * - https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort
 */
#define WYG_ASSERT(expr) if (expr) {} else abort()

/**
 * @see C:
 * - https://en.cppreference.com/w/c/io/fprintf
 */
#define WYG_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// ================================================================================================================================

struct wyg_context_t
{
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    int format;
};

// ================================================================================================================================

extern wyg_context_t* wyg_create_context(wyg_window_t window)
{
    wyg_context_t* const restrict context = malloc(sizeof(wyg_context_t));
    
    if (context != 0)
    {
        context->hwnd = (HWND)window;
        WYG_ASSERT(context->hwnd != 0);

        context->hdc = GetDC(context->hwnd);
        WYG_ASSERT(context->hdc != 0);

        const PIXELFORMATDESCRIPTOR pfd = {
            .nSize           = sizeof(PIXELFORMATDESCRIPTOR),
            .nVersion        = 1,
            .dwFlags         = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            .iPixelType      = PFD_TYPE_RGBA,
            .cColorBits      = 32,
            .cRedBits        = 8,
            // .cRedShift       = 0,
            .cGreenBits      = 8,
            // .cGreenShift     = 0,
            .cBlueBits       = 8,
            // .cBlueShift      = 0,
            .cAlphaBits      = 8,
            // .cAlphaShift     = 0,
            // .cAccumBits      = 0,
            // .cAccumRedBits   = 0,
            // .cAccumGreenBits = 0,
            // .cAccumBlueBits  = 0,
            // .cAccumAlphaBits = 0,
            .cDepthBits      = 32,
            .cStencilBits    = 0,
            .cAuxBuffers     = 0,
            .iLayerType      = PFD_MAIN_PLANE,
            // .bReserved       = 0,
            // .dwLayerMask     = 0,
            // .dwVisibleMask   = 0,
            // .dwDamageMask    = 0,
        };
        context->format = ChoosePixelFormat(context->hdc, &pfd);
        WYG_ASSERT(context->format != 0);

        const BOOL res_set = SetPixelFormat(context->hdc, context->format, &pfd);
        WYG_ASSERT(res_set != FALSE);

        context->hglrc = wglCreateContext(context->hdc);
        WYG_ASSERT(context->hglrc != 0);
    }

    return context;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyg_make_current(wyg_context_t* const restrict context)
{
    if (context == 0)
    {
        const BOOL res = wglMakeCurrent(0, 0);
        WYG_ASSERT(res != FALSE);
    }
    else
    {
        const BOOL res = wglMakeCurrent(context->hdc, context->hglrc);
        WYG_ASSERT(res != FALSE);
    
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunknown-warning-option"
        #pragma GCC diagnostic ignored "-Wincompatible-function-pointer-types"
        #pragma GCC diagnostic ignored "-Wincompatible-function-pointer-types-strict"
        {
            const PFNWGLSWAPINTERVALEXTPROC gl_SetSwapInterval = wglGetProcAddress("wglSwapIntervalEXT");
            WYG_ASSERT(gl_SetSwapInterval != 0);

            const PFNWGLGETSWAPINTERVALEXTPROC gl_GetSwapInterval = wglGetProcAddress("wglGetSwapIntervalEXT");
            WYG_ASSERT(gl_GetSwapInterval != 0);

            [[maybe_unused]] const int swap_get = gl_GetSwapInterval();
            [[maybe_unused]] const int swap_set = gl_SetSwapInterval(0);
            [[maybe_unused]] const int swap_net = gl_GetSwapInterval();

            // WYG_LOG("[SWAP-GET] %i\n", swap_get);
            // WYG_LOG("[SWAP-SET] %i\n", swap_set);
            // WYG_LOG("[SWAP-NET] %i\n", swap_net);
            // __debugbreak();
        }
        #pragma GCC diagnostic pop
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyg_destroy_context(wyg_context_t* const restrict context)
{
    WYG_ASSERT(context != 0);

    {
        const BOOL res = wglDeleteContext(context->hglrc);
        WYG_ASSERT(res != FALSE);
    }

    {
        const int res = ReleaseDC(context->hwnd, context->hdc);
        WYG_ASSERT(res != 0);
    }

    free(context);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyg_swap_buffers(wyg_context_t* const restrict context)
{
    WYG_ASSERT(context != 0);
    
    const BOOL res = SwapBuffers(context->hdc);
    WYG_ASSERT(res != FALSE);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void* wyg_load(wyg_context_t* const restrict context, const char* const name)
{
    (void)context;
    return (void*)wglGetProcAddress(name);
}

// ================================================================================================================================
