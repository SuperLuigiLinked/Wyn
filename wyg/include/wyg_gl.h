/**
 * @file wyg_gl.h
 */

#if defined(WYG_WGL)
    #include <Windows.h>
    #include <gl/GL.h>
#elif defined(WYG_GLX)
    #include <GL/glx.h>
    #include <GL/gl.h>
#elif defined(WYG_EGL)
    #include <EGL/egl.h>
    #include <GL/gl.h>
#elif defined(WYG_NSGL)
    #include <OpenGL/OpenGL.h>
    #include <OpenGL/gl.h>
#endif

// #if defined(__GNUC__) || defined(__clang__)
//     #pragma GCC diagnostic push
//     #pragma GCC diagnostic ignored "-Wreserved-identifier"
//     #pragma GCC diagnostic ignored "-Wnonportable-system-include-path"
// #endif
// #define GL_GLEXT_PROTOTYPES
// #include <GL/glcorearb.h>
// #include <EGL/egl.h>
// #if defined(__GNUC__) || defined(__clang__)
//     #pragma GCC diagnostic pop
// #endif

// ================================================================================================================================

extern void wyg_gl_create_context(void);

extern void wyg_gl_make_current(void);

extern void wyg_gl_destroy_context(void);

// ================================================================================================================================
