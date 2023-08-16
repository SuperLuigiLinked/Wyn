/**
 * @file debug.h
 */

#pragma once

#include "utils.h"

// ================================================================================================================================

struct Debug
{
    uint64_t events;

    wyt_time_t update_ts;
    wyt_time_t update_te;
    wyt_time_t update_el;

    wyt_time_t render_ts;
    wyt_time_t render_te;
    wyt_time_t render_el;
};

// ================================================================================================================================
