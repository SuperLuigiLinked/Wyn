/**
 * @file render_gl.c
 */

#include "utils.h"
#include "common.h"
#include "render.h"
#include "update.h"

// ================================================================================================================================

static void render_init(Render* const restrict self);
static void render_deinit(Render* const restrict self);

extern wyt_retval_t WYT_ENTRY render_loop(void* arg);

static void render_iterate(Render* const restrict self);
static void render_present(Render* const restrict self);
static void render_update_await(Render* const restrict self);
static void render_update_signal(Render* const restrict self);

static void render_debug(const Render* const restrict self);

// ================================================================================================================================

static void render_init(Render* const restrict self)
{
    (void)self;
}

static void render_deinit(Render* const restrict self)
{
    (void)self;    
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyt_retval_t WYT_ENTRY render_loop(void* common)
{
    Render self = { .common = common };
    render_init(&self);

    while (!wyn_quitting())
    {
        render_update_await(&self);
        if (!self.state) break;
        render_iterate(&self);
        render_update_signal(&self);

        render_present(&self);

        render_debug(&self);
    }

    render_deinit(&self);
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void render_iterate(Render* const restrict self)
{
    self->frame_ts = wyt_nanotime();
    {
    }
    self->frame_te = wyt_nanotime();
    ++self->frame;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void render_present(Render* const restrict self)
{
    (void)self;
    wyt_nanosleep_for(nanos_per_second / frames_per_second * 120);
}

// --------------------------------------------------------------------------------------------------------------------------------

static void render_update_await(Render* const restrict self)
{
    wyt_sem_acquire(self->common->sem_u2r);
    self->state = atomic_exchange_explicit(&self->common->state_ptr, NULL, memory_order_consume);
}

static void render_update_signal(Render* const restrict self)
{
    self->state = NULL;
    const bool res = wyt_sem_release(self->common->sem_r2u);
    ASSERT(res != false);
}

// --------------------------------------------------------------------------------------------------------------------------------

static void render_debug(const Render* const restrict self)
{
    const uint64_t frame = self->frame - 1;
    const double fpn = (double)frames_per_second / (double)nanos_per_second;
    const double frame_fs = (double)(self->frame_ts - self->common->epoch) * fpn;
    const double frame_fe = (double)(self->frame_te - self->common->epoch) * fpn;
    const double frame_el = frame_fe - frame_fs;
    const int64_t dropped = (int64_t)((uint64_t)frame_fs - (uint64_t)frame);

    LOG("[RENDER] (%6"PRIu64") %+"PRIi64" | %9.2f %9.2f %9.2f |\n", frame, dropped, frame_fs, frame_fe, frame_el);
}

// ================================================================================================================================

