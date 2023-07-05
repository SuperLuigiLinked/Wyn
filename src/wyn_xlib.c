/*
    Wyn - Xlib.c
*/

#include "wyn.h"

#include <stdlib.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

// ================================================================================================================================ //
// Macros
// -------------------------------------------------------------------------------------------------------------------------------- //

// #ifdef NDEBUG
// #   define WYN_ASSERT(expr) if (expr) {} else abort()
// #else
// #   define WYN_ASSERT(expr) if (expr) {} else abort()
// #endif

// ================================================================================================================================ //
// Declarations
// -------------------------------------------------------------------------------------------------------------------------------- //

struct wyn_events_t
{
    Display* display;

    int epoll_fd;
    int xlib_fd;
    int quit_fd;
};

static struct wyn_events_t g_events = {};

// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(struct wyn_events_t* events);
static void wyn_terminate(struct wyn_events_t* events);

static int wyn_run_native(struct wyn_events_t* events);

static int wyn_xlib_error_handler(Display* display, XErrorEvent* error);
static int wyn_xlib_io_error_handler(Display* display);
static void wyn_xlib_io_error_exit_handler(Display* display, void* userdata);

// ================================================================================================================================ //
// Public Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

extern int wyn_run(void)
{
    int code = EXIT_FAILURE;

    if (wyn_init(&g_events))
    {
        wyn_on_start(&g_events);
        code = wyn_run_native(&g_events);
        wyn_on_stop(&g_events);
    }
    wyn_terminate(&g_events);

    return code;
}

extern void wyn_quit(struct wyn_events_t* events, int code)
{
    [[maybe_unused]] const int res = eventfd_write(events->quit_fd, (eventfd_t)(code));
}

// ================================================================================================================================ //
// Private Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(struct wyn_events_t* events)
{
    *events = (struct wyn_events_t){
        .display = 0,
        .epoll_fd = -1,
        .xlib_fd = -1,
        .quit_fd = -1,
    };

    events->display = XOpenDisplay(0);
    if (events->display == 0) return false;

    [[maybe_unused]] XErrorHandler prev_error = XSetErrorHandler(wyn_xlib_error_handler);
    [[maybe_unused]] XIOErrorHandler prev_io_error = XSetIOErrorHandler(wyn_xlib_io_error_handler);
    XSetIOErrorExitHandler(events->display, wyn_xlib_io_error_exit_handler, events);

    events->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (events->epoll_fd == -1) return false;

    {        
        events->xlib_fd = ConnectionNumber(events->display);
        if (events->xlib_fd == -1) return false;

        struct epoll_event evt = { .events = EPOLLIN, .data = { .fd = events->xlib_fd } };
        const int res = epoll_ctl(events->epoll_fd, EPOLL_CTL_ADD, events->xlib_fd, &evt);
        if (res == -1) return false;
    }

    {
        events->quit_fd = eventfd(0, EFD_CLOEXEC);
        if (events->quit_fd == -1) return false;

        struct epoll_event evt = { .events = EPOLLIN, .data = { .fd = events->quit_fd } };
        const int res = epoll_ctl(events->epoll_fd, EPOLL_CTL_ADD, events->quit_fd, &evt);
        if (res == -1) return false;
    }    

    return true;
}

static void wyn_terminate(struct wyn_events_t* events)
{
    if (events->quit_fd != -1)
    {
        [[maybe_unused]] const int res = close(events->quit_fd);
    }

    if (events->epoll_fd != -1)
    {
        [[maybe_unused]] const int res = close(events->epoll_fd);
    }

    if (events->display)
    {
        [[maybe_unused]] const int res = XCloseDisplay(events->display);
    }
}

// -------------------------------------------------------------------------------------------------------------------------------- //

static int wyn_run_native(struct wyn_events_t* events)
{
    {
        [[maybe_unused]] const int res = XFlush(events->display);
    }
    
    for (;;)
    {
        fputs("[POLLING]\n", stderr);
        
        struct epoll_event evt = {};

        {
            const int res = epoll_wait(events->epoll_fd, &evt, 1, -1);
            if (res == -1) return -1;
        }

        if (evt.data.fd == events->quit_fd)
        {
            fputs("[EPOLL-QUIT]\n", stderr);

            eventfd_t code = 0;
            const int res = eventfd_read(evt.data.fd, &code);
            return (res == -1) ? -1 : (int)(code);
        }
        else if (evt.data.fd == events->xlib_fd)
        {
            fputs("[EPOLL-XLIB]\n", stderr);

            while (XPending(events->display) > 0)
            {
                fputs("[EVENT]\n", stderr);
                
                XEvent event = {};
                [[maybe_unused]] const int res = XNextEvent(events->display, &event);
            }
        }
    }
}

static int wyn_xlib_error_handler(Display* display [[maybe_unused]], XErrorEvent* error [[maybe_unused]])
{
    fputs("[XLIB ERROR]\n", stderr);
    return 0;
}

static int wyn_xlib_io_error_handler(Display* display [[maybe_unused]])
{
    fputs("[XLIB IO ERROR]\n", stderr);
    return 0;
}

static void wyn_xlib_io_error_exit_handler(Display* display [[maybe_unused]], void* userdata [[maybe_unused]])
{
    fputs("[XLIB IO ERROR EXIT]\n", stderr);

    struct wyn_events_t* const events = &g_events;

    wyn_quit(events, EXIT_FAILURE);
}

// -------------------------------------------------------------------------------------------------------------------------------- //

extern wyn_window_t wyn_open_window(struct wyn_events_t* events [[maybe_unused]])
{
    Screen* const screen = DefaultScreenOfDisplay(events->display);
    const Window root = RootWindowOfScreen(screen);

    const Window xWnd = XCreateSimpleWindow(
        events->display, root,
        0, 0, 640, 480,
        0, None, None
    );

    return (wyn_window_t)(xWnd);
}

extern void wyn_close_window(struct wyn_events_t* events, wyn_window_t window)
{
    const Window xWnd = (Window)(window);

    [[maybe_unused]] const int res = XDestroyWindow(events->display, xWnd);
}

extern void wyn_show_window(struct wyn_events_t* events, wyn_window_t window)
{
    const Window xWnd = (Window)(window);
    
    [[maybe_unused]] const int res = XMapRaised(events->display, xWnd);
}

extern void wyn_hide_window(struct wyn_events_t* events, wyn_window_t window)
{
    const Window xWnd = (Window)(window);

    [[maybe_unused]] const int res = XUnmapWindow(events->display, xWnd);
}

// ================================================================================================================================ //
