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

    const wyn_size_t old_size = wyn_window_size(self->window);
    wyn_window_resize(self->window, (wyn_size_t){ .w = 640.0, .h = 480.0 });
    const wyn_size_t new_size = wyn_window_size(self->window);
    
    LOG("[APP] (%.2f x %.2f) -> (%.2f x %.2f)\n", (double)old_size.w, (double)old_size.h, (double)new_size.w, (double)new_size.h);

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
