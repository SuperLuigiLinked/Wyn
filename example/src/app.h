/**
 * @file app.h
 */

#pragma once

#include "utils.h"
#include "debug.h"
#include "events.h"
#include "logic.h"
#include "render.h"

// ================================================================================================================================

struct App
{
    wyt_time_t epoch;

    Debug debug;
    Events events;
    Logic logic;
    Render render;
};

// ================================================================================================================================

