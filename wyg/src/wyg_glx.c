/**
 * @file wyg_glx.c
 */
 
#include "wyg.h"

#include <X11/Xlib-xcb.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

// ================================================================================================================================

extern wyg_context_t* wyg_create_context(wyg_window_t window);

extern void wyg_make_current(wyg_context_t* context);

extern void wyg_destroy_context(wyg_context_t* context);

extern void wyg_swap_buffers(void);

// ================================================================================================================================
