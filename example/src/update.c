/**
 * @file update.c
 */

#include "utils.h"
#include "common.h"
#include "update.h"

// ================================================================================================================================

static void update_init(Update* const restrict self);
static void update_deinit(Update* const restrict self);

extern wyt_retval_t WYT_ENTRY update_loop(void* arg);

static void update_iterate(Update* const restrict self);
static void update_vsync(Update* const self);
static void update_render_signal(Update* const self);
static void update_render_await(Update* const self);

static void update_debug(const Update* const self);

// ================================================================================================================================

static void update_init(Update* const restrict self)
{
    (void)self;
}

static void update_deinit(Update* const restrict self)
{
    // Ensure Render Thread is not soft-locked.
    {
        atomic_store_explicit(&self->common->state_ptr, NULL, memory_order_release);
        const bool res = wyt_sem_release(self->common->sem_u2r);
        ASSERT(res != false);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_retval_t WYT_ENTRY update_loop(void* common)
{
    Update self = { .common = common };
    update_init(&self);

    while (!wyn_quitting())
    {
        update_iterate(&self);
        update_render_signal(&self);
        update_vsync(&self);
        update_render_await(&self);

        update_debug(&self);
    }

    update_deinit(&self);
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void update_iterate(Update* const restrict self)
{
    self->frame_ts = wyt_nanotime();
    {
    }
    self->frame_te = wyt_nanotime();
    ++self->frame;
}


// --------------------------------------------------------------------------------------------------------------------------------

static void update_vsync(Update* const self)
{
    (void)self;
    wyt_nanosleep_for(nanos_per_second / frames_per_second * 30);
}

// --------------------------------------------------------------------------------------------------------------------------------

static void update_render_signal(Update* const self)
{
    atomic_store_explicit(&self->common->state_ptr, self, memory_order_release);
    const bool res = wyt_sem_release(self->common->sem_u2r);
    ASSERT(res != false);
}

static void update_render_await(Update* const self)
{
    if (!wyt_sem_try_acquire(self->common->sem_u2r))
    {
        wyt_sem_acquire(self->common->sem_r2u);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

static void update_debug(const Update* const self)
{
    const uint64_t frame = self->frame - 1;
    const double fpn = (double)frames_per_second / (double)nanos_per_second;
    const double frame_fs = (double)(self->frame_ts - self->common->epoch) * fpn;
    const double frame_fe = (double)(self->frame_te - self->common->epoch) * fpn;
    const double frame_el = frame_fe - frame_fs;
    const int64_t dropped = (int64_t)((uint64_t)frame_fs - (uint64_t)frame);

    LOG("[UPDATE] (%6"PRIu64") %+"PRIi64" | %9.2f %9.2f %9.2f |\n", frame, dropped, frame_fs, frame_fe, frame_el);
}

// ================================================================================================================================
