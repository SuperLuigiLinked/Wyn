/**
 * @file events.c
 */

#include "utils.h"
#include "debug.h"
#include "events.h"

// ================================================================================================================================

extern void wyn_on_start(void* userdata)
{
    LOG("[WYN] <START>\n");
    ASSERT(userdata != NULL);

    App* const app = userdata;
    Events* const events = app_get_events(app);

    app_set_epoch(app, wyt_nanotime());

    events->window = wyn_open_window();
    if (!events->window) { wyn_quit(); return; }
    wyn_show_window(events->window);

    events->thread = wyt_spawn(app_logic_thread, userdata);
    if (!events->thread) { wyn_quit(); return; }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_stop(void* userdata)
{
    LOG("[WYN] <STOP>\n");
    ASSERT(userdata != NULL);

    App* const app = userdata;
    Events* const events = app_get_events(app);

    app_quit(app);

    if (events->thread)
    {
        wyt_join(events->thread);
        events->thread = NULL;
    }
    
    if (events->window)
    {
        wyn_close_window(events->window);
        events->window = NULL;
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_window_close(void* userdata, wyn_window_t window)
{
    LOG("[WYN] <CLOSE>\n");
    ASSERT(userdata != NULL);

    App* const app = userdata;
    Events* const events = app_get_events(app);

    if (window == events->window)
    {
        app_quit(app);

        if (events->thread)
        {
            wyt_join(events->thread);
            events->thread = NULL;
        }

        events->window = NULL;
    }    
}

// ================================================================================================================================

extern void events_init(Events* const events)
{
    *events = (Events){};
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void events_deinit(Events* const events)
{
    (void)events;
}

// ================================================================================================================================
