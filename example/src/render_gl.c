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

    render->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    [[maybe_unused]] const EGLBoolean res_init = eglInitialize(render->display, NULL, NULL);
    
    const EGLint attribute_list[] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_NONE
    };
    EGLint num_config;
    [[maybe_unused]] const EGLBoolean res_choose = eglChooseConfig(render->display, attribute_list, &render->config, 1, &num_config);

    render->context = eglCreateContext(render->display, render->config, EGL_NO_CONTEXT, NULL);
    render->surface = eglCreateWindowSurface(render->display, render->config, (EGLNativeWindowType)window, NULL);
    
    [[maybe_unused]] const EGLBoolean res_make = eglMakeCurrent(render->display, render->surface, render->surface, render->context);
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
        /* clear the color buffer */
        glClearColor(1.0, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();

        eglSwapBuffers(render->display, render->surface);
    }
    debug->render_te = wyt_nanotime();
    debug->render_el = debug->render_ts - debug->render_te;
    ++render->frame;
}

// ================================================================================================================================

extern void render_init(Render* const render)
{
    *render = (Render){};
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void render_deinit(Render* const render)
{
    (void)render;
}

// ================================================================================================================================
