/**
 * @file events.c
 */

#include "utils.h"
#include "common.h"
#include "events.h"

// ================================================================================================================================

static void events_init(Events* const restrict self)
{  
    self->common->window = wyn_window_open();
    ASSERT(self->common->window != 0);

    self->common->sem_u2r = wyt_sem_create(1, 0);
    ASSERT(self->common->sem_u2r != 0);

    self->common->sem_r2u = wyt_sem_create(1, 0);
    ASSERT(self->common->sem_r2u != 0);

    self->common->epoch = wyt_nanotime();

    self->update_thread = wyt_spawn(update_loop, self->common);
    ASSERT(self->update_thread != 0);

    self->render_thread = wyt_spawn(render_loop, self->common);
    ASSERT(self->render_thread != 0);

    wyn_window_show(self->common->window);
}

static void events_deinit(Events* const restrict self)
{
    if (self->render_thread != 0)
    {
        (void)wyt_join(self->render_thread);
        self->render_thread = 0;
    }

    if (self->update_thread != 0)
    {
        (void)wyt_join(self->update_thread);
        self->update_thread = 0;
    }

    if (self->common->sem_r2u != 0)
    {
        wyt_sem_destroy(self->common->sem_r2u);
        self->common->sem_r2u = 0;
    }

    if (self->common->sem_u2r != 0)
    {
        wyt_sem_destroy(self->common->sem_u2r);
        self->common->sem_u2r = 0;
    }

    if (self->common->window != 0)
    {
        wyn_window_close(self->common->window);
        self->common->window = 0;
    }
}

// ================================================================================================================================

extern void wyn_on_start(void* userdata)
{
    Events* const restrict self = userdata;
    LOG("[EVENTS] (%"PRIu64") START\n", ++self->events);

    events_init(self);
}

extern void wyn_on_stop(void* userdata)
{
    Events* const restrict self = userdata;
    LOG("[EVENTS] (%"PRIu64") STOP\n", ++self->events);
    
    events_deinit(self);
}

extern void wyn_on_window_close_request(void* userdata, wyn_window_t window)
{
    Events* const restrict self = userdata;
    LOG("[EVENTS] (%"PRIu64") CLOSE\n", ++self->events);

    if (window == self->common->window)
    {
        wyn_quit();
    }
}

extern void wyn_on_window_redraw(void* userdata, wyn_window_t window)
{
    Events* const restrict self = userdata;
    LOG("[EVENTS] (%"PRIu64") REDRAW\n", ++self->events);

    if (window == self->common->window)
    {
        (void)window;
    }
}

// ================================================================================================================================

extern void events_loop(void* common)
{
    Events self = { .common = common };
    wyn_run(&self);
}

// ================================================================================================================================
