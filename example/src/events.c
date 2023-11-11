/**
 * @file events.c
 */

#include "app.h"

// ================================================================================================================================

static void app_reinit(App* const self)
{
    self->epoch = wyt_nanotime();

    self->window = wyn_window_open();
    ASSERT(self->window != 0);

    wyn_window_show(self->window);
}

static void app_deinit(App* const self)
{
    if (self->window != 0)
    {
        wyn_window_close(self->window);
        self->window = 0;
    }
}

// ================================================================================================================================

extern void wyn_on_start(void* const userdata)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") START\n", ++self->num_events);

    app_reinit(self);
}

extern void wyn_on_stop(void* const userdata)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") STOP\n", ++self->num_events);
    
    app_deinit(self);
}

extern void wyn_on_signal(void* const userdata)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") SIGNAL\n", ++self->num_events);
    
    wyn_quit();
}

extern void wyn_on_window_close(void* const userdata, wyn_window_t const window)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") CLOSE\n", ++self->num_events);

    if (window == self->window)
    {
        wyn_quit();
    }
}

extern void wyn_on_window_redraw(void* const userdata, wyn_window_t const window)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") REDRAW\n", ++self->num_events);

    if (window == self->window)
    {
        (void)window;
    }
}

// ================================================================================================================================
