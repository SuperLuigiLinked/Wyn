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
    void* userdata;

    Display* display;

    int epoll_fd;
    int xlib_fd;
    int quit_fd;
};

static struct wyn_events_t g_events = {};

// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(void* userdata);
static void wyn_terminate();

static int wyn_run_native();

static int wyn_xlib_error_handler(Display* display, XErrorEvent* error);
static int wyn_xlib_io_error_handler(Display* display);
static void wyn_xlib_io_error_exit_handler(Display* display, void* userdata);

// ================================================================================================================================ //
// Public Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

extern int wyn_run(void* userdata)
{
    int code = EXIT_FAILURE;

    if (wyn_init(userdata))
    {
        wyn_on_start(userdata);
        code = wyn_run_native();
        wyn_on_stop(userdata);
    }
    wyn_terminate();

    return code;
}

extern void wyn_quit(int code)
{
    [[maybe_unused]] const int res = eventfd_write(g_events.quit_fd, (eventfd_t)code);
}

// ================================================================================================================================ //
// Private Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(void* userdata)
{
    g_events = (struct wyn_events_t){
        .userdata = userdata,
        .display = NULL,
        .epoll_fd = -1,
        .xlib_fd = -1,
        .quit_fd = -1,
    };

    g_events.display = XOpenDisplay(0);
    if (g_events.display == 0) return false;

    [[maybe_unused]] XErrorHandler prev_error = XSetErrorHandler(wyn_xlib_error_handler);
    [[maybe_unused]] XIOErrorHandler prev_io_error = XSetIOErrorHandler(wyn_xlib_io_error_handler);
    XSetIOErrorExitHandler(g_events.display, wyn_xlib_io_error_exit_handler, NULL);

    g_events.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (g_events.epoll_fd == -1) return false;

    {        
        g_events.xlib_fd = ConnectionNumber(g_events.display);
        if (g_events.xlib_fd == -1) return false;

        struct epoll_event evt = { .events = EPOLLIN, .data = { .fd = g_events.xlib_fd } };
        const int res = epoll_ctl(g_events.epoll_fd, EPOLL_CTL_ADD, g_events.xlib_fd, &evt);
        if (res == -1) return false;
    }

    {
        g_events.quit_fd = eventfd(0, EFD_CLOEXEC);
        if (g_events.quit_fd == -1) return false;

        struct epoll_event evt = { .events = EPOLLIN, .data = { .fd = g_events.quit_fd } };
        const int res = epoll_ctl(g_events.epoll_fd, EPOLL_CTL_ADD, g_events.quit_fd, &evt);
        if (res == -1) return false;
    }    

    return true;
}

static void wyn_terminate(void)
{
    if (g_events.quit_fd != -1)
    {
        [[maybe_unused]] const int res = close(g_events.quit_fd);
    }

    if (g_events.epoll_fd != -1)
    {
        [[maybe_unused]] const int res = close(g_events.epoll_fd);
    }

    if (g_events.display)
    {
        [[maybe_unused]] const int res = XCloseDisplay(g_events.display);
    }
}

// -------------------------------------------------------------------------------------------------------------------------------- //

static int wyn_run_native(void)
{
    {
        [[maybe_unused]] const int res = XFlush(g_events.display);
    }
    
    for (;;)
    {
        fputs("[POLLING]\n", stderr);
        
        struct epoll_event evt = {};

        {
            const int res = epoll_wait(g_events.epoll_fd, &evt, 1, -1);
            if (res == -1) return -1;
        }

        if (evt.data.fd == g_events.quit_fd)
        {
            fputs("[EPOLL-QUIT]\n", stderr);

            eventfd_t code = 0;
            const int res = eventfd_read(evt.data.fd, &code);
            return (res == -1) ? -1 : (int)(code);
        }
        else if (evt.data.fd == g_events.xlib_fd)
        {
            fputs("[EPOLL-XLIB]\n", stderr);

            while (XPending(g_events.display) > 0)
            {
                fputs("[EVENT]\n", stderr);
                
                XEvent event = {};
                [[maybe_unused]] const int res = XNextEvent(g_events.display, &event);
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

    wyn_quit(EXIT_FAILURE);
}

// -------------------------------------------------------------------------------------------------------------------------------- //

extern wyn_window_t wyn_open_window(void)
{
    Screen* const screen = DefaultScreenOfDisplay(g_events.display);
    const Window root = RootWindowOfScreen(screen);

    const Window xWnd = XCreateSimpleWindow(
        g_events.display, root,
        0, 0, 640, 480,
        0, None, None
    );

    return (wyn_window_t)xWnd;
}

extern void wyn_close_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;

    [[maybe_unused]] const int res = XDestroyWindow(g_events.display, xWnd);
}

extern void wyn_show_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;
    
    [[maybe_unused]] const int res = XMapRaised(g_events.display, xWnd);
}

extern void wyn_hide_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;

    [[maybe_unused]] const int res = XUnmapWindow(g_events.display, xWnd);
}

// ================================================================================================================================ //
