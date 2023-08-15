/**
 * @file utils.h
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <math.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include <wyt.h>
#include <wyn.h>

// ================================================================================================================================

#define ASSERT(expr) if (expr) {} else abort()

#define LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// --------------------------------------------------------------------------------------------------------------------------------

enum : wyt_time_t { nanos_per_second = 1'000'000'000 };
enum : wyt_time_t { frames_per_second = 60 };

// ================================================================================================================================

typedef struct App App;
typedef struct Debug Debug;
typedef struct Events Events;
typedef struct Logic Logic;
typedef struct Render Render;

// --------------------------------------------------------------------------------------------------------------------------------

extern App* app_create(void);
extern void app_destroy(App* const app);

extern void debug_init(Debug* const debug);
extern void debug_deinit(Debug* const debug);

extern void events_init(Events* const events);
extern void events_deinit(Events* const events);

extern void logic_init(Logic* const logic);
extern void logic_deinit(Logic* const logic);

extern void render_init(Render* const render);
extern void render_deinit(Render* const render);

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_quit(App* const app);
extern bool app_quitting(const App* const app);

extern void app_set_epoch(App* const app, const wyt_time_t epoch);
extern wyt_time_t app_get_epoch(const App* const app);

extern Debug* app_get_debug(App* const app);
extern Events* app_get_events(App* const app);
extern Logic* app_get_logic(App* const app);
extern Render* app_get_render(App* const app);

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_update(App* const app);
extern void app_render(App* const app);
extern void app_debug(App* const app);

extern wyt_retval_t WYT_ENTRY app_logic_thread(void* arg);

// ================================================================================================================================
