/**
 * @file render_gl.c
 */

#include "render.h"

#include "app.h"
#include "debug.h"
#include "logic.h"

// ================================================================================================================================

extern void app_render(App* const app)
{
    app->debug->render_ts = wyt_nanotime();
    {

    }
    app->debug->render_te = wyt_nanotime();
    app->debug->render_el = app->debug->render_ts - app->debug->render_te;
    ++app->render->frame;
}

// ================================================================================================================================

extern Render* render_create(void)
{
    Render* const render = malloc(sizeof(Render));
    
    *render = (Render){};
    
    return render;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void render_destroy(Render* const render)
{
    free(render);
}

// ================================================================================================================================
