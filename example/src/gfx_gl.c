/**
 * @file example/gfx_gl.c
 */

#include <wyt.h>
#include <wyn.h>
#include <wyg_gl.h>

#include "App.h"

#include <math.h>

// ================================================================================================================================

enum : wyt_time_t { nanos_per_second = 1'000'000'000 };
enum : wyt_time_t { frames_per_second = 60 };

struct GfxDebug
{
    wyt_time_t frame_ts;

    wyt_time_t update_ts;
    wyt_time_t update_te;
    wyt_duration_t update_e;

    wyt_time_t render_ts;
    wyt_time_t render_te;
    wyt_duration_t render_e;
};

struct GfxState
{
    wyt_time_t epoch;
    size_t updates;
    size_t renders;

    struct GfxDebug dbg;
};

// ================================================================================================================================

static struct GfxState gfx_init(struct AppState* app)
{
    struct GfxState state = {};

    (void)app;

    return state;
}

// ================================================================================================================================

static void gfx_vsync(const wyt_time_t epoch)
{
    const wyt_time_t now = wyt_nanotime();
    const wyt_time_t now_nanos = now - epoch;
    const wyt_time_t now_frames = wyt_scale(now_nanos, frames_per_second, nanos_per_second);
    const wyt_time_t next_frames = now_frames + 1;
    const wyt_time_t next_nanos = wyt_scale(next_frames, nanos_per_second, frames_per_second);
    const wyt_time_t next = epoch + next_nanos;

    wyt_nanosleep_until(next);
}

// ================================================================================================================================

static void gfx_debug(struct GfxState* const gfx)
{
    const double fpn = (double)frames_per_second / (double)nanos_per_second;
    const double frame_ts = fpn * (double)(gfx->dbg.update_ts - gfx->epoch);
    const double frame_te = fpn * (double)(gfx->dbg.render_te - gfx->epoch);
    const double frame_e = frame_te - frame_ts;
    const size_t updates = gfx->updates - 1;
    const ptrdiff_t dropped = (ptrdiff_t)frame_ts - (ptrdiff_t)updates;

    LOG("FRAME [%6zu] %+td | F: %9.2f %9.2f %5.2f | U: %9lld ns | R: %9lld ns |\n",
        updates, dropped, frame_ts, frame_te, frame_e, gfx->dbg.update_e,  gfx->dbg.render_e
    );
}

// ================================================================================================================================

static void gfx_update(struct GfxState* const gfx)
{
    ++gfx->updates;
}

// ================================================================================================================================

static void gfx_render(struct GfxState* const gfx)
{
    ++gfx->renders;
}

// ================================================================================================================================

extern wyt_retval_t WYT_ENTRY app_gfx_func(void* arg)
{
    LOG("[GFX] START\n");
    {
        struct AppState* const app = arg;
        ASSERT(app != NULL);

        struct GfxState gfx = gfx_init(app);
        gfx.epoch = wyt_nanotime();

        while (!app_quitting(app))
        {
            {
                gfx.dbg.update_ts = wyt_nanotime();
                gfx_update(&gfx);
                gfx.dbg.update_te = wyt_nanotime();
                gfx.dbg.update_e = (wyt_duration_t)(gfx.dbg.update_te - gfx.dbg.update_ts);
            }
            {
                gfx.dbg.render_ts = wyt_nanotime();
                gfx_render(&gfx);
                gfx.dbg.render_te = wyt_nanotime();
                gfx.dbg.render_e = (wyt_duration_t)(gfx.dbg.render_te - gfx.dbg.render_ts);
            }
            gfx_debug(&gfx);
            gfx_vsync(gfx.epoch);
        }
    }
    LOG("[GFX] STOP\n");
    return 0;
}

// ================================================================================================================================

