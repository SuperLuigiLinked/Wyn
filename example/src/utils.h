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
extern void app_destroy(App* app);

extern Debug* debug_create(void);
extern void debug_destroy(Debug* debug);

extern Events* events_create(void);
extern void events_destroy(Events* events);

extern Logic* logic_create(void);
extern void logic_destroy(Logic* logic);

extern Render* render_create(void);
extern void render_destroy(Render* render);

// --------------------------------------------------------------------------------------------------------------------------------

extern void app_quit(App* const app);
extern bool app_quitting(const App* const app);

extern void app_update(App* const app);
extern void app_render(App* const app);
extern void app_debug(App* const app);

extern wyt_retval_t WYT_ENTRY app_logic_thread(void* arg);

// ================================================================================================================================
