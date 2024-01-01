/**
 * @file events.cpp
 */

#include "app.hpp"

// ================================================================================================================================

static wyn_bool_t primary_display_callback(void* const userdata, wyn_display_t const display)
{
    ASSERT(userdata != nullptr);
    *static_cast<wyn_rect_t*>(userdata) = wyn_display_position(display);
    return 0;
}

static wyn_rect_t primary_display(void)
{
    wyn_rect_t rect = {};
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
    const wyn_extent_t window_extent = { 640.0, 480.0 };
    const wyn_point_t window_origin = { monitor.origin.x + (monitor.extent.w - window_extent.w) / 2, monitor.origin.y + (monitor.extent.h - window_extent.h) / 2 };
    const wyn_utf8_t* const window_title = WYN_UTF8("Wyn Example" " | " COMPILER " | " STANDARD);

    self->window = wyn_window_open();
    ASSERT(self->window != nullptr);

    wyn_window_reposition(self->window, &window_origin, &window_extent);
    wyn_window_retitle(self->window, window_title);
    wyn_window_show(self->window);
}

static void app_deinit(App* const self)
{
    if (self->window != nullptr)
    {
        wyn_window_close(self->window);
        self->window = nullptr;
    }
}

// ================================================================================================================================

extern void wyn_on_start(void* const userdata)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") START\n", static_cast<std::uint64_t>(++self->num_events));

    app_reinit(self);
}

extern void wyn_on_stop(void* const userdata)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") STOP\n", static_cast<std::uint64_t>(++self->num_events));
    
    app_deinit(self);
}

extern void wyn_on_signal(void* const userdata)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") SIGNAL\n", static_cast<std::uint64_t>(++self->num_events));
    
    wyn_quit();
}

extern void wyn_on_window_close(void* const userdata, wyn_window_t const window)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} CLOSE\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window));
    if (window != self->window) return;
    
    wyn_quit();
}

extern void wyn_on_window_redraw(void* const userdata, wyn_window_t const window)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} REDRAW\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window));
    if (window != self->window) return;

}

extern void wyn_on_window_focus(void* const userdata, wyn_window_t const window, wyt_bool_t const focused)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} FOCUS | %d\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window), static_cast<int>(focused));
    if (window != self->window) return;

}

extern void wyn_on_window_reposition(void* const userdata, wyn_window_t const window, wyn_rect_t const content, wyn_coord_t const scale)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} REPOSITION | (%f , %f) (%f x %f) [%f]\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window), static_cast<double>(content.origin.x), static_cast<double>(content.origin.y), static_cast<double>(content.extent.w), static_cast<double>(content.extent.h), static_cast<double>(scale));
    if (window != self->window) return;

}

extern void wyn_on_display_change(void* const userdata)
{
    App* const self = static_cast<App*>(userdata);
    const unsigned int count = wyn_enumerate_displays(nullptr, nullptr);
    LOG("[EVENTS] (%" PRIu64 ") DISPLAYS | %u\n", static_cast<std::uint64_t>(++self->num_events), static_cast<unsigned int>(count));

}

extern void wyn_on_cursor(void* const userdata, wyn_window_t const window, wyn_coord_t const sx, wyn_coord_t const sy)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} CURSOR | (%f , %f)\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window), static_cast<double>(sx), static_cast<double>(sy));
    if (window != self->window) return;

}

extern void wyn_on_cursor_exit(void* const userdata, wyn_window_t const window)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} CURSOR EXIT\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window));
    if (window != self->window) return;

}

extern void wyn_on_scroll(void* const userdata, wyn_window_t const window, wyn_coord_t const dx, wyn_coord_t const dy)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} SCROLL | [%f , %f]\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window), static_cast<double>(dx), static_cast<double>(dy));
    if (window != self->window) return;

}

extern void wyn_on_mouse(void* const userdata, wyn_window_t const window, wyn_button_t const button, wyn_bool_t const pressed)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} MOUSE | %d (%d)\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window), static_cast<int>(button), static_cast<int>(pressed));
    if (window != self->window) return;

    ASSERT(self->vb_mapping != nullptr);
}

extern void wyn_on_keyboard(void* const userdata, wyn_window_t const window, wyn_keycode_t const keycode, wyn_bool_t const pressed)
{
    App* const self = static_cast<App*>(userdata);
    LOG("[EVENTS] (%" PRIu64 ") {%p} KEYBOARD | %d (%d)\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window), static_cast<int>(keycode), static_cast<int>(pressed));
    if (window != self->window) return;
    
    ASSERT(self->vk_mapping != nullptr);
    
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
    App* const self = static_cast<App*>(userdata);
    
    const char* const chars = reinterpret_cast<const char*>(text);
    const size_t chars_len = std::strlen(chars);

    LOG("[EVENTS] (%" PRIu64 ") {%p} TEXT | [%zu] \"%s\"\n", static_cast<std::uint64_t>(++self->num_events), static_cast<void*>(window), chars_len, chars);
    if (window != self->window) return;

}

// ================================================================================================================================
