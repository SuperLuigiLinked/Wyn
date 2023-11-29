/**
 * @file events.c
 */

#include "app.h"

// ================================================================================================================================

static void app_reinit(App* const self)
{
    self->epoch = wyt_nanotime();

    self->window = wyn_window_open();
    ASSERT(self->window != 0);

    const wyn_size_t old_size = wyn_window_size(self->window);
    const double scale = wyn_window_scale(self->window);
    wyn_window_resize(self->window, (wyn_size_t){ .w = 640.0 * scale, .h = 480.0 * scale });
    const wyn_size_t new_size = wyn_window_size(self->window);
    LOG("[APP] (%.2f x %.2f) -> (%.2f x %.2f) [%.2f]\n", (double)old_size.w, (double)old_size.h, (double)new_size.w, (double)new_size.h, (double)scale);

    wyn_window_retitle(self->window, (const wyn_utf8_t*)u8"Wyn Example");
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
    LOG("[EVENTS] (%"PRIu64") START\n", ++self->num_events);

    app_reinit(self);
}

extern void wyn_on_stop(void* const userdata)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") STOP\n", ++self->num_events);
    
    app_deinit(self);
}

extern void wyn_on_signal(void* const userdata)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") SIGNAL\n", ++self->num_events);
    
    wyn_quit();
}

extern void wyn_on_window_close(void* const userdata, wyn_window_t const window)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} CLOSE\n", ++self->num_events, (void*)window);
    if (window != self->window) return;
    
    wyn_quit();
}

extern void wyn_on_window_redraw(void* const userdata, wyn_window_t const window)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} REDRAW\n", ++self->num_events, (void*)window);
    if (window != self->window) return;

}

extern void wyn_on_window_resize(void* const userdata, wyn_window_t const window, wyn_coord_t const pw, wyn_coord_t const ph)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} RESIZE | (%f x %f)\n", ++self->num_events, (void*)window, (double)pw, (double)ph);
    if (window != self->window) return;

}

extern void wyn_on_cursor(void* const userdata, wyn_window_t const window, wyn_coord_t const px, wyn_coord_t const py)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} CURSOR | (%f , %f)\n", ++self->num_events, (void*)window, (double)px, (double)py);
    if (window != self->window) return;

}

extern void wyn_on_scroll(void* const userdata, wyn_window_t const window, int const dx, int const dy)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} SCROLL | [%d , %d]\n", ++self->num_events, (void*)window, (int)dx, (int)dy);
    if (window != self->window) return;

}

extern void wyn_on_mouse(void* const userdata, wyn_window_t const window, wyn_button_t const button, bool const pressed)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} MOUSE | %d (%d)\n", ++self->num_events, (void*)window, (int)button, (int)pressed);
    if (window != self->window) return;

}

extern void wyn_on_keyboard(void* const userdata, wyn_window_t const window, wyn_keycode_t const keycode, bool const pressed)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} KEYBOARD | %d (%d)\n", ++self->num_events, (void*)window, (int)keycode, (int)pressed);
    if (window != self->window) return;

}

extern void wyn_on_character(void* const userdata, wyn_window_t const window, wyn_utf8_t const code)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} CHARACTER | %3d '%c'\n", ++self->num_events, (void*)window, (int)code, (char)code);
    if (window != self->window) return;

}

// ================================================================================================================================
