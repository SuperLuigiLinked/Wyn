/**
 * @file wyg_gl.h
 */

// #if defined(__APPLE__)
//     #include <OpenGL/OpenGL.h>
//     #include <OpenGL/gl.h>
// #elif defined(_WIN32)
//     #include <Windows.h>
//     #include <gl/GL.h>
// #else
//     #include <GL/glx.h>
//     #include <GL/gl.h>
// #endif

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wreserved-identifier"
#endif

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include <EGL/egl.h>

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

// ================================================================================================================================

extern void wyg_gl_create_context(void);

extern void wyg_gl_make_current(void);

extern void wyg_gl_destroy_context(void);

// ================================================================================================================================
