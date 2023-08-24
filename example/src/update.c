/**
 * @file update.c
 */

#include "utils.h"
#include "common.h"
#include "update.h"

// ================================================================================================================================

static void update_init(Update* const restrict update)
{
    (void)update;
}

static void update_deinit(Update* const restrict update)
{
    (void)update;    
}

// ================================================================================================================================

static void update_iterate(Update* const restrict update)
{
    update->frame_ts = wyt_nanotime();
    {
        wyt_nanosleep_for(nanos_per_second / frames_per_second);
    }
    update->frame_te = wyt_nanotime();
    ++update->frame;
}

static void update_debug(Update* const restrict update)
{
    const uint64_t frame = update->frame - 1;
    const double fpn = (double)frames_per_second / (double)nanos_per_second;
    const double frame_fs = (double)(update->frame_ts - update->common->epoch) * fpn;
    const double frame_fe = (double)(update->frame_te - update->common->epoch) * fpn;
    const double frame_el = frame_fe - frame_fs;
    const int64_t dropped = (int64_t)((uint64_t)frame_fs - (uint64_t)frame);

    LOG("[UPDATE] (%6"PRIu64") %+"PRIi64" | %9.2f %9.2f %9.2f |\n", frame, dropped, frame_fs, frame_fe, frame_el);
}

// ================================================================================================================================

extern wyt_retval_t WYT_ENTRY update_loop(void* arg)
{
    Update update = { .common = arg };
    update_init(&update);

    while (!wyn_quitting())
    {
        update_iterate(&update);
        update_debug(&update);
    }

    update_deinit(&update);
    return 0;
}

// ================================================================================================================================
