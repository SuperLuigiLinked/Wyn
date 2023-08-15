/**
 * @file render_gl.c
 */

#include "utils.h"
#include "debug.h"
#include "logic.h"
#include "render.h"

// ================================================================================================================================

extern void app_render(App* const app)
{
    Debug* const debug = app_get_debug(app);
    Logic* const logic = app_get_logic(app);
    Render* const render = app_get_render(app);

    debug->render_ts = wyt_nanotime();
    {
        (void)logic;
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
