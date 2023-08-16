/**
 * @file render_gl.c
 */

#include "utils.h"
#include "debug.h"
#include "events.h"
#include "logic.h"
#include "render.h"

// ================================================================================================================================

static void render_target_window(Render* const render, const wyn_window_t window)
{
    render->window = window;

#if defined(WYG_EGL)
    const EGLBoolean res_api = eglBindAPI(EGL_OPENGL_API);
    ASSERT(res_api == EGL_TRUE);

    render->context = eglCreateContext(render->display, render->config, EGL_NO_CONTEXT, NULL);
    ASSERT(render->context != EGL_NO_CONTEXT);

    render->surface = eglCreateWindowSurface(render->display, render->config, (EGLNativeWindowType)window, NULL);
    
    const EGLBoolean res_make = eglMakeCurrent(render->display, render->surface, render->surface, render->context);
    ASSERT(res_make == EGL_TRUE);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_render(App* const app)
{
    [[maybe_unused]] Debug* const debug = app_get_debug(app);
    [[maybe_unused]] Events* const events = app_get_events(app);
    [[maybe_unused]] Logic* const logic = app_get_logic(app);
    [[maybe_unused]] Render* const render = app_get_render(app);

    if (!render->window) render_target_window(render, events->window);

    debug->render_ts = wyt_nanotime();
    {
        const float val = fabsf((float)((int64_t)(render->frame % 510) - 255)) / 255.0f;
        glClearColor(val, val, val, val);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();

    #if defined(WYG_EGL)
        eglSwapBuffers(render->display, render->surface);
    #endif
    }
    debug->render_te = wyt_nanotime();
    debug->render_el = debug->render_ts - debug->render_te;
    ++render->frame;
}

// ================================================================================================================================

extern void render_init(Render* const render)
{
    *render = (Render){};

#if defined(WYG_EGL)
    render->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ASSERT(render->display != NULL);

    const EGLBoolean res_init = eglInitialize(render->display, NULL, NULL);
    ASSERT(res_init == EGL_TRUE);
    
    const EGLint attribute_list[] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_NONE
    };
    EGLint num_config;
    const EGLBoolean res_choose = eglChooseConfig(render->display, attribute_list, &render->config, 1, &num_config);
    ASSERT(res_choose == EGL_TRUE);
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void render_deinit(Render* const render)
{
    const EGLBoolean res_terminate = eglTerminate(render->display);
    ASSERT(res_terminate == EGL_TRUE);
}

// ================================================================================================================================
