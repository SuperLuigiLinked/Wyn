/**
 * @file debug.c
 */

#include "debug.h"

#include "app.h"
#include "events.h"
#include "logic.h"
#include "render.h"

// ================================================================================================================================

extern void app_debug(App* const app)
{
    const double fpn = (double)frames_per_second / (double)nanos_per_second;
    const double frame_ts = (double)(app->debug->update_ts - app->epoch) * fpn;
    const double frame_te = (double)(app->debug->render_te - app->epoch) * fpn;
    const double frame_el = frame_te - frame_ts;
    const uint64_t updates = app->logic->frame - 1;
    const uint64_t renders = app->render->frame - 1;
    const int64_t dropped_u = (int64_t)frame_ts - (int64_t)updates;
    const int64_t dropped_r = (int64_t)frame_ts - (int64_t)renders;

    LOG(
        "FRAME | %9"PRIu64" %9"PRIu64" | %+"PRIi64"  %+"PRIi64" | %12.2f %12.2f | %.2f\n",
        updates, renders, dropped_u, dropped_r, frame_ts, frame_te, frame_el
    );
}

// ================================================================================================================================

extern Debug* debug_create(void)
{
    Debug* const debug = malloc(sizeof(Debug));
    
    *debug = (Debug){};

    return debug;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void debug_destroy(Debug* const debug)
{
    free(debug);
}

// ================================================================================================================================
