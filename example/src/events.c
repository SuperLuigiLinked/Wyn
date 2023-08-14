/**
 * @file events.c
 */

#include "events.h"

#include "app.h"

// ================================================================================================================================

extern void wyn_on_start(void* userdata)
{
    LOG("[WYN] <START>\n");

    App* const app = (App*)userdata;
    ASSERT(app != NULL);

    app->epoch = wyt_nanotime();

    app->events->window = wyn_open_window();
    if (!app->events->window) { wyn_quit(); return; }
    wyn_show_window(app->events->window);

    app->events->thread = wyt_spawn(app_logic_thread, userdata);
    if (!app->events->thread) { wyn_quit(); return; }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_stop(void* userdata)
{
    LOG("[WYN] <STOP>\n");

    App* const app = (App*)userdata;
    ASSERT(app != NULL);

    app_quit(app);

    if (app->events->thread)
    {
        wyt_join(app->events->thread);
        app->events->thread = NULL;
    }
    
    if (app->events->window)
    {
        wyn_close_window(app->events->window);
        app->events->window = NULL;
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_on_window_close(void* userdata, wyn_window_t window)
{
    LOG("[WYN] <CLOSE>\n");

    App* const app = (App*)userdata;
    ASSERT(app != NULL);

    if (window == app->events->window)
    {
        app_quit(app);

        if (app->events->thread)
        {
            wyt_join(app->events->thread);
            app->events->thread = NULL;
        }

        app->events->window = NULL;
    }    
}

// ================================================================================================================================

extern Events* events_create(void)
{
    Events* const events = malloc(sizeof(Events));
    
    *events = (Events){};
    
    return events;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void events_destroy(Events* const events)
{
    free(events);
}

// ================================================================================================================================
