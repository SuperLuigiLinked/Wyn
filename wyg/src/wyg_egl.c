/**
 * @file wyg_egl.c
 */
 
#include "wyg.h"

#include <GL/gl.h>
#include <EGL/egl.h>

// ================================================================================================================================

extern wyg_context_t* wyg_create_context(wyg_window_t window);

extern void wyg_make_current(wyg_context_t* context);

extern void wyg_destroy_context(wyg_context_t* context);

extern void wyg_swap_buffers(void);

// ================================================================================================================================
