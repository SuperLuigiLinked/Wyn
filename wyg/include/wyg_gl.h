/**
 * @file wyg_gl.h
 */

#if defined(__APPLE__)
    #include <OpenGL/OpenGL.h>
    #include <OpenGL/gl.h>
#elif defined(_WIN32)
    #include <Windows.h>
    #include <gl/GL.h>
#else
    #include <GL/glx.h>
    #include <GL/gl.h>
#endif

// ================================================================================================================================

extern void wyg_gl_create_context(void);

extern void wyg_gl_make_current(void);

extern void wyg_gl_destroy_context(void);

// ================================================================================================================================
