/**
 * @file app.c
 */

#include "app.h"

// ================================================================================================================================

static void app_quit_callback(void*)
{
    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_quit(App* const app)
{
    const bool quitting = atomic_exchange_explicit(&app->quit_flag, true, memory_order_relaxed);
    if (!quitting) wyn_execute_async(app_quit_callback, NULL);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern bool app_quitting(const App* const app)
{
    return atomic_load_explicit(&app->quit_flag, memory_order_relaxed);
}

// ================================================================================================================================

extern App* app_create(void)
{
    App* const app = malloc(sizeof(App));
    
    *app = (App){};

    app->debug = debug_create();
    app->events = events_create();
    app->logic = logic_create();
    app->render = render_create();

    return app;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_destroy(App* const app)
{
    render_destroy(app->render);
    logic_destroy(app->logic);
    events_destroy(app->events);
    debug_destroy(app->debug);

    free(app);
}

// ================================================================================================================================
