/**
 * @file events.c
 */

#include "utils.h"
#include "common.h"
#include "events.h"

// ================================================================================================================================

static void events_init(Events* const restrict events)
{
    events->common->window = wyn_open_window();
    ASSERT(events->common->window != 0);

    events->common->epoch = wyt_nanotime();

    events->update_thread = wyt_spawn(update_loop, events->common);
    ASSERT(events->update_thread != 0);

    events->render_thread = wyt_spawn(render_loop, events->common);
    ASSERT(events->render_thread != 0);

    wyn_show_window(events->common->window);
}

static void events_deinit(Events* const restrict events)
{
    if (events->render_thread != 0)
    {
        (void)wyt_join(events->render_thread);
        events->render_thread = 0;
    }

    if (events->update_thread != 0)
    {
        (void)wyt_join(events->update_thread);
        events->update_thread = 0;
    }

    if (events->common->window != 0)
    {
        wyn_close_window(events->common->window);
        events->common->window = 0;
    }
}

// ================================================================================================================================

extern void wyn_on_start(void* userdata)
{
    Events* const restrict events = userdata;
    LOG("[EVENTS] (%"PRIu64") START\n", ++events->events);

    events_init(events);
}

extern void wyn_on_stop(void* userdata)
{
    Events* const restrict events = userdata;
    LOG("[EVENTS] (%"PRIu64") STOP\n", ++events->events);
    
    events_deinit(events);
}

extern void wyn_on_window_close_request(void* userdata, wyn_window_t window)
{
    Events* const restrict events = userdata;
    LOG("[EVENTS] (%"PRIu64") CLOSE\n", ++events->events);

    if (window == events->common->window)
    {
        wyn_quit();
    }
}

extern void wyn_on_window_redraw(void* userdata, wyn_window_t window)
{
    Events* const restrict events = userdata;
    LOG("[EVENTS] (%"PRIu64") REDRAW\n", ++events->events);

    if (window == events->common->window)
    {
        (void)window;
    }
}

// ================================================================================================================================

extern void events_loop(void)
{
    Common common = {};
    {
        Events events = { .common = &common };
        wyn_run(&events);
    }
}

// ================================================================================================================================
