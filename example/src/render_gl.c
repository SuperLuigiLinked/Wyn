/**
 * @file render_gl.c
 */

#include "utils.h"
#include "common.h"
#include "render.h"

// ================================================================================================================================

static void render_init(Render* const restrict render)
{
    (void)render;
}

static void render_deinit(Render* const restrict render)
{
    (void)render;    
}

// ================================================================================================================================

static void render_iterate(Render* const restrict render)
{
    render->frame_ts = wyt_nanotime();
    {
        wyt_nanosleep_for(nanos_per_second / frames_per_second);
    }
    render->frame_te = wyt_nanotime();
    ++render->frame;
}

static void render_debug(Render* const restrict render)
{
    const uint64_t frame = render->frame - 1;
    const double fpn = (double)frames_per_second / (double)nanos_per_second;
    const double frame_fs = (double)(render->frame_ts - render->common->epoch) * fpn;
    const double frame_fe = (double)(render->frame_te - render->common->epoch) * fpn;
    const double frame_el = frame_fe - frame_fs;
    const int64_t dropped = (int64_t)((uint64_t)frame_fs - (uint64_t)frame);

    LOG("[RENDER] (%6"PRIu64") %+"PRIi64" | %9.2f %9.2f %9.2f |\n", frame, dropped, frame_fs, frame_fe, frame_el);
}

// ================================================================================================================================

extern wyt_retval_t WYT_ENTRY render_loop(void* arg)
{
    Render render = { .common = arg };
    render_init(&render);

    while (!wyn_quitting())
    {
        render_iterate(&render);
        render_debug(&render);
    }

    render_deinit(&render);
    return 0;
}

// ================================================================================================================================

