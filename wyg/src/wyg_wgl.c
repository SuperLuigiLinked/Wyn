/**
 * @file wyg_wgl.c
 */
 
#include "wyg.h"

#include <Windows.h>
#include <gl/GL.h>
#include <GL/wgl.h>
#include <GL/wglext.h>

// ================================================================================================================================

extern wyg_context_t* wyg_create_context(wyg_window_t window);

extern void wyg_make_current(wyg_context_t* context);

extern void wyg_destroy_context(wyg_context_t* context);

extern void wyg_swap_buffers(void);

// ================================================================================================================================
