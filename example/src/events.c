/**
 * @file events.c
 */

#include "app.h"

// ================================================================================================================================

static wyn_bool_t primary_display_callback(void* const userdata, wyn_display_t const display)
{
    ASSERT(userdata != 0);
    *(wyn_rect_t*)userdata = wyn_display_position(display);
    return 0;
}

static wyn_rect_t primary_display(void)
{
    wyn_rect_t rect = {0};
    const unsigned display_count = wyn_enumerate_displays(primary_display_callback, &rect);
    ASSERT(display_count == 1);

    return rect;
}

// ================================================================================================================================

static void app_reinit(App* const self)
{
    self->epoch = wyt_nanotime();
    self->vb_mapping = wyn_vb_mapping();
    self->vk_mapping = wyn_vk_mapping();

    const wyn_rect_t monitor = primary_display();
    const wyn_extent_t window_extent = { .w = 640.0, .h = 480.0 };
    const wyn_point_t window_origin = { .x = monitor.origin.x + (monitor.extent.w - window_extent.w) / 2, .y = monitor.origin.y + (monitor.extent.h - window_extent.h) / 2 };
    const wyn_utf8_t* const window_title = (const wyn_utf8_t*)u8"Wyn Example";

    self->window = wyn_window_open();
    ASSERT(self->window != 0);

    wyn_window_reposition(self->window, &window_origin, &window_extent);
    wyn_window_retitle(self->window, window_title);
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
    LOG("[EVENTS] (%"PRIu64") START\n", (uint64_t)++self->num_events);

    app_reinit(self);
}

extern void wyn_on_stop(void* const userdata)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") STOP\n", (uint64_t)++self->num_events);
    
    app_deinit(self);
}

extern void wyn_on_signal(void* const userdata)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") SIGNAL\n", (uint64_t)++self->num_events);
    
    wyn_quit();
}

extern void wyn_on_window_close(void* const userdata, wyn_window_t const window)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} CLOSE\n", (uint64_t)++self->num_events, (void*)window);
    if (window != self->window) return;
    
    wyn_quit();
}

extern void wyn_on_window_redraw(void* const userdata, wyn_window_t const window)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} REDRAW\n", (uint64_t)++self->num_events, (void*)window);
    if (window != self->window) return;

}

extern void wyn_on_window_focus(void* const userdata, wyn_window_t const window, wyt_bool_t const focused)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} FOCUS | %u\n", (uint64_t)++self->num_events, (void*)window, focused);
    if (window != self->window) return;

}

extern void wyn_on_window_reposition(void* const userdata, wyn_window_t const window, wyn_rect_t const content, wyn_coord_t const scale)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} REPOSITION | (%f , %f) (%f x %f) [%f]\n", (uint64_t)++self->num_events, (void*)window, (double)content.origin.x, (double)content.origin.y, (double)content.extent.w, (double)content.extent.h, (double)scale);
    if (window != self->window) return;

}

extern void wyn_on_display_change(void* const userdata)
{
    App* const self = userdata;
    const unsigned int count = wyn_enumerate_displays(NULL, NULL);
    LOG("[EVENTS] (%"PRIu64") DISPLAYS | %u\n", (uint64_t)++self->num_events, (unsigned int)count);

}

extern void wyn_on_cursor(void* const userdata, wyn_window_t const window, wyn_coord_t const sx, wyn_coord_t const sy)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} CURSOR | (%f , %f)\n", (uint64_t)++self->num_events, (void*)window, (double)sx, (double)sy);
    if (window != self->window) return;

}

extern void wyn_on_cursor_exit(void* const userdata, wyn_window_t const window)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} CURSOR EXIT\n", (uint64_t)++self->num_events, (void*)window);
    if (window != self->window) return;

}

extern void wyn_on_scroll(void* const userdata, wyn_window_t const window, wyn_coord_t const dx, wyn_coord_t const dy)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} SCROLL | [%f , %f]\n", (uint64_t)++self->num_events, (void*)window, (double)dx, (double)dy);
    if (window != self->window) return;

}

extern void wyn_on_mouse(void* const userdata, wyn_window_t const window, wyn_button_t const button, wyn_bool_t const pressed)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} MOUSE | %d (%d)\n", (uint64_t)++self->num_events, (void*)window, (int)button, (int)pressed);
    if (window != self->window) return;

}

extern void wyn_on_keyboard(void* const userdata, wyn_window_t const window, wyn_keycode_t const keycode, wyn_bool_t const pressed)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} KEYBOARD | %d (%d)\n", (uint64_t)++self->num_events, (void*)window, (int)keycode, (int)pressed);
    if (window != self->window) return;

    if (keycode == (*self->vk_mapping)[wyn_vk_Escape])
    {
        if (pressed)
        {
            const wyt_bool_t was_fullscreen = wyn_window_is_fullscreen(self->window);
            wyn_window_fullscreen(self->window, !was_fullscreen);
        }
    }
}

extern void wyn_on_text(void* const userdata, wyn_window_t const window, const wyn_utf8_t* const text)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} TEXT | [%zu] \"%s\"\n", (uint64_t)++self->num_events, (void*)window, strlen((const char*)text), (const char*)text);
    if (window != self->window) return;

}

// ================================================================================================================================
