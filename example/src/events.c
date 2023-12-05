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

    const wyn_extent_t old_size = wyn_window_size(self->window);
    const double scale = wyn_window_scale(self->window);
    wyn_window_resize(self->window, (wyn_extent_t){ .w = 640.0 * scale, .h = 480.0 * scale });
    const wyn_extent_t new_size = wyn_window_size(self->window);
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

extern void wyn_on_window_focus(void* const userdata, wyn_window_t const window, bool const focused)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} FOCUS | %u\n", ++self->num_events, (void*)window, focused);
    if (window != self->window) return;

}

extern void wyn_on_window_reposition(void* const userdata, wyn_window_t const window, wyn_rect_t const content, wyn_coord_t const scale)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} REPOSITION | (%f , %f) (%f x %f) [%f]\n", ++self->num_events, (void*)window, (double)content.origin.x, (double)content.origin.y, (double)content.extent.w, (double)content.extent.h, (double)scale);
    if (window != self->window) return;

}

extern void wyn_on_cursor(void* const userdata, wyn_window_t const window, wyn_coord_t const sx, wyn_coord_t const sy)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} CURSOR | (%f , %f)\n", ++self->num_events, (void*)window, (double)sx, (double)sy);
    if (window != self->window) return;

}

extern void wyn_on_cursor_exit(void* const userdata, wyn_window_t const window)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} CURSOR EXIT\n", ++self->num_events, (void*)window);
    if (window != self->window) return;

}

extern void wyn_on_scroll(void* const userdata, wyn_window_t const window, wyn_coord_t const dx, wyn_coord_t const dy)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} SCROLL | [%f , %f]\n", ++self->num_events, (void*)window, (double)dx, (double)dy);
    if (window != self->window) return;

}

extern void wyn_on_mouse(void* const userdata, wyn_window_t const window, wyn_button_t const button, wyn_bool_t const pressed)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} MOUSE | %d (%d)\n", ++self->num_events, (void*)window, (int)button, (int)pressed);
    if (window != self->window) return;

}

extern void wyn_on_keyboard(void* const userdata, wyn_window_t const window, wyn_keycode_t const keycode, wyn_bool_t const pressed)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} KEYBOARD | %d (%d)\n", ++self->num_events, (void*)window, (int)keycode, (int)pressed);
    if (window != self->window) return;

}

extern void wyn_on_text(void* const userdata, wyn_window_t const window, const wyn_utf8_t* const text)
{
    App* const self = userdata;
    LOG("[EVENTS] (%"PRIu64") {%p} TEXT | [%zu] \"%s\"\n", ++self->num_events, (void*)window, strlen((const char*)text), (const char*)text);
    if (window != self->window) return;

}

// ================================================================================================================================
