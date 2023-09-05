/**
 * @file wyg_nsgl.c
 */
 
#include "wyg.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

// ================================================================================================================================

extern wyg_context_t* wyg_create_context(wyg_window_t window);

extern void wyg_make_current(wyg_context_t* context);

extern void wyg_destroy_context(wyg_context_t* context);

extern void wyg_swap_buffers(wyg_context_t* context);

extern void* wyg_load(wyg_context_t* context, const char* name);

// ================================================================================================================================
