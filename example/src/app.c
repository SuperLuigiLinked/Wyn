/**
 * @file app.c
 */

#include "utils.h"
#include "app.h"

// ================================================================================================================================

static void app_quit_callback(void*)
{
    wyn_quit();
}

extern void app_quit(App* const app [[maybe_unused]])
{
    const wyn_exec_t res = wyn_execute_async(app_quit_callback, NULL);
    ASSERT(res != wyn_exec_failed);
}

extern bool app_quitting(const App* const app [[maybe_unused]])
{
    return wyn_quitting();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_set_epoch(App* const app, const wyt_time_t epoch)
{
    app->epoch = epoch;
}

extern wyt_time_t app_get_epoch(const App* const app)
{
    return app->epoch;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern Debug* app_get_debug(App* const app)
{
    return &app->debug;
}

extern Events* app_get_events(App* const app)
{
    return &app->events;
}

extern Logic* app_get_logic(App* const app)
{
    return &app->logic;
}

extern Render* app_get_render(App* const app)
{
    return &app->render;
}

// ================================================================================================================================

extern App* app_create(void)
{
    App* const app = malloc(sizeof(App));
    ASSERT(app != NULL);
    
    *app = (App){};

    debug_init(&app->debug);
    events_init(&app->events);
    logic_init(&app->logic);
    render_init(&app->render);

    return app;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_destroy(App* const app)
{
    render_deinit(&app->render);
    logic_deinit(&app->logic);
    events_deinit(&app->events);
    debug_deinit(&app->debug);

    free(app);
}

// ================================================================================================================================
