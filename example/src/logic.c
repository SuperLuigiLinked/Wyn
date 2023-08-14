/**
 * @file logic.c
 */

#include "logic.h"

#include "app.h"
#include "debug.h"

// ================================================================================================================================

extern void app_update(App* const app)
{
    app->debug->update_ts = wyt_nanotime();
    {

    }
    app->debug->update_te = wyt_nanotime();
    app->debug->update_el = app->debug->update_ts - app->debug->update_te;
    ++app->logic->frame;
}

// ================================================================================================================================

static void logic_iterate(void* arg)
{
    App* const app = arg;
    ASSERT(app != NULL);

    if (app_quitting(app)) return;

    app_update(app);
    app_render(app);
    app_debug(app);
}

// --------------------------------------------------------------------------------------------------------------------------------

static void logic_vsync(const wyt_time_t epoch, const wyt_time_t last_tick)
{
    const wyt_time_t last_nanos = last_tick - epoch;
    const wyt_time_t last_frames = wyt_scale(last_nanos, frames_per_second, nanos_per_second);
    const wyt_time_t next_frames = last_frames + 1;
    const wyt_time_t next_nanos = wyt_scale(next_frames, nanos_per_second, frames_per_second);
    const wyt_time_t next_tick = epoch + next_nanos;
    wyt_nanosleep_until(next_tick);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_retval_t WYT_ENTRY app_logic_thread(void* arg)
{
    App* const app = arg;
    ASSERT(app != NULL);

    const wyt_time_t epoch = app->epoch;
    wyt_time_t last_tick = epoch;

    while (!app_quitting(app))
    {
        wyn_execute_async(logic_iterate, arg);
        logic_vsync(epoch, last_tick);
        last_tick = wyt_nanotime();
    }

    return 0;
}

// ================================================================================================================================

extern Logic* logic_create(void)
{
    Logic* const logic = malloc(sizeof(Logic));
    ASSERT(logic != NULL);
    
    *logic = (Logic){};
    
    return logic;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void logic_destroy(Logic* const logic)
{
    free(logic);
}

// ================================================================================================================================
