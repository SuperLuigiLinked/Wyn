/**
 * @file wyn_xlib.c
 * @brief Implementation of Wyn for the Xlib backend.
 */

#include "wyn.h"

#define _GNU_SOURCE

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h> 
#include <poll.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <linux/limits.h>
#include <X11/Xlib.h>

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man3/abort.3.html
 */
#define WYN_ASSERT(expr) if (expr) {} else abort()

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Internal structure for holding Wyn state.
 */
struct wyn_state_t
{
    void* userdata;     ///< The pointer provided by the user when the Event Loop was started.
    
    Display* display;   ///< The connection to the X Window System.
    
    pid_t tid_main;     ///< Thread ID of the Main Thread.

    int xlib_fd;        ///< File Descriptor for the Xlib Connection.
    int read_pipe;      ///< File Descriptor for the Read-end of the Exec-Pipe.
    int write_pipe;     ///< File Descriptor for the Write-end of the Exec-Pipe.
};

/**
 * @brief Static instance of all Wyn state.
 * @details Because Wyn can only be used on the Main Thread, it is safe to have static-storage state.
 *          This state must be global so it can be reached by callbacks on certain platforms.
 */
static struct wyn_state_t wyn_state = {};

/**
 * @brief Struct for passing callbacks with arguments.
 */
struct wyn_callback_t
{
    void (*func)(void*);    ///< The function to call.
    void* arg;              ///< The argument to pass to the function.
    _Atomic uint32_t* flag; ///< Flag for synchronous execution.
};

_Static_assert(sizeof(struct wyn_callback_t) <= PIPE_BUF, "Atomic pipe operations not possible.");

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes all Wyn state.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @return `true` if successful, `false` if there were errors.
 */
static bool wyn_init(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_terminate(void);

/**
 * @brief Runs all pending exec-callbacks.
 * @return `true` if the Event Loop should quit, `false` otherwise.
 */
static bool wyn_clear_events(void);

/**
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_run_native();

/**
 * @brief Xlib Error Handler.
 */
static int wyn_xlib_error_handler(Display* display, XErrorEvent* error);

/**
 * @brief Xlib IO Error Handler.
 */
static int wyn_xlib_io_error_handler(Display* display);

/**
 * @brief Xlib IO Error Exit Handler.
 */
static void wyn_xlib_io_error_exit_handler(Display* display, void* userdata);

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_futex_wait(_Atomic uint32_t* addr, uint32_t val)
{
    while (atomic_load_explicit(addr, memory_order_acquire) == val)
    {
        const long res = syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL);
        WYN_ASSERT((res == 0) || (errno == EAGAIN));
    }
}

static void wyn_futex_wake(_Atomic uint32_t* addr, uint32_t val)
{
    (void)atomic_store_explicit(addr, val, memory_order_release);
    const long res = syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, 1);
    WYN_ASSERT(res != -1);
}

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/pipe.2.html
 */
static bool wyn_init(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .display = NULL,
        .tid_main = 0,
        .xlib_fd = -1,
        .read_pipe = -1,
        .write_pipe = -1,
    };

    wyn_state.tid_main = gettid();

    {
        wyn_state.display = XOpenDisplay(0);
        if (wyn_state.display == 0) return false;
    }

    {
        [[maybe_unused]] const XErrorHandler prev_error = XSetErrorHandler(wyn_xlib_error_handler);
        [[maybe_unused]] const XIOErrorHandler prev_io_error = XSetIOErrorHandler(wyn_xlib_io_error_handler);
        XSetIOErrorExitHandler(wyn_state.display, wyn_xlib_io_error_exit_handler, NULL);
    }

    {        
        wyn_state.xlib_fd = ConnectionNumber(wyn_state.display);
        if (wyn_state.xlib_fd == -1) return false;
    }

    {
        int pipe_fds[2];
        const int res = pipe2(pipe_fds, O_CLOEXEC | O_DIRECT);
        if (res == -1) return false;

        wyn_state.read_pipe = pipe_fds[0];
        wyn_state.write_pipe = pipe_fds[1];

        fcntl(wyn_state.read_pipe, F_SETFL, O_NONBLOCK);
    }    

    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_terminate(void)
{
    (void)wyn_clear_events();

    if (wyn_state.write_pipe != -1)
    {
        [[maybe_unused]] const int res = close(wyn_state.write_pipe);
    }

    if (wyn_state.read_pipe != -1)
    {
        [[maybe_unused]] const int res = close(wyn_state.read_pipe);
    }

    if (wyn_state.display != NULL)
    {
        [[maybe_unused]] const int res = XCloseDisplay(wyn_state.display);
    }
}
// --------------------------------------------------------------------------------------------------------------------------------

static bool wyn_clear_events(void)
{
    _Alignas(struct wyn_callback_t) char buf[sizeof(struct wyn_callback_t)];

    const ssize_t res = read(wyn_state.read_pipe, buf, sizeof(buf));
    WYN_ASSERT((res != -1) || (errno == EAGAIN));

    if (res == sizeof(struct wyn_callback_t))
    {
        struct wyn_callback_t callback;
        (void)memcpy(&callback, buf, sizeof(callback));
        
        WYN_ASSERT(callback.func != NULL);
        callback.func(callback.arg);

        if (callback.flag != NULL)
            wyn_futex_wake(callback.flag, 1);

        return false;
    }
    else
    {
        return res != 0;
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_run_native(void)
{
    {
        [[maybe_unused]] const int res = XFlush(wyn_state.display);
    }
    
    for (;;)
    {
        struct pollfd fds[] = {
            [0] = { .fd = wyn_state.read_pipe, .events = POLLIN, .revents = 0 },
            [1] = { .fd = wyn_state.xlib_fd, .events = POLLIN, .revents = 0 },
        };
        const nfds_t nfds = sizeof(fds) / sizeof(*fds);
        const int res_poll = poll(fds, nfds, -1);
        WYN_ASSERT(res_poll != -1);
        WYN_ASSERT(res_poll != 0);

        const short pipe_events = fds[0].revents;
        const short xlib_events = fds[1].revents;

        if (pipe_events != 0)
        {
            fprintf(stderr, "[POLL-PIPE] %04hX\n", pipe_events);
            WYN_ASSERT(pipe_events == POLLIN);

            const bool quit = wyn_clear_events();
            if (quit) return;
        }
        
        if (xlib_events != 0)
        {
            fprintf(stderr, "[POLL-XLIB] %04hX\n", xlib_events);

            if (xlib_events & POLLIN)
            {
                while (XPending(wyn_state.display) > 0)
                {
                    fputs("[EVENT]\n", stderr);
                    
                    XEvent event = {};
                    [[maybe_unused]] const int res = XNextEvent(wyn_state.display, &event);
                }
            }

            if (xlib_events & POLLHUP)
            {
                return;
            }

            WYN_ASSERT(xlib_events == POLLIN);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

static int wyn_xlib_error_handler(Display* display [[maybe_unused]], XErrorEvent* error [[maybe_unused]])
{
    fputs("[XLIB ERROR]\n", stderr);
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------

static int wyn_xlib_io_error_handler(Display* display [[maybe_unused]])
{
    fputs("[XLIB IO ERROR]\n", stderr);
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_xlib_io_error_exit_handler(Display* display [[maybe_unused]], void* userdata [[maybe_unused]])
{
    fputs("[XLIB IO ERROR EXIT]\n", stderr);
    wyn_quit();
}

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* userdata)
{
    if (wyn_init(userdata))
    {
        wyn_on_start(userdata);
        wyn_run_native();
        wyn_on_stop(userdata);
    }
    wyn_terminate();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_quit(void)
{
    const char buf[] = { 0 };
    const ssize_t res = write(wyn_state.write_pipe, buf, sizeof(buf));
    WYN_ASSERT(res == sizeof(buf));
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_execute(void (*func)(void *), void *arg)
{
    if (gettid() == wyn_state.tid_main)
    {
        func(arg);
    }
    else
    {
        _Atomic(uint32_t) flag = 0;

        const struct wyn_callback_t callback = { .func = func, .arg = arg, .flag = &flag };
        const ssize_t res = write(wyn_state.write_pipe, &callback, sizeof(callback));
        WYN_ASSERT(res == sizeof(callback));

        wyn_futex_wait(&flag, 0);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_execute_async(void (*func)(void *), void *arg)
{
    const struct wyn_callback_t callback = { .func = func, .arg = arg, .flag = NULL };
    const ssize_t res = write(wyn_state.write_pipe, &callback, sizeof(callback));
    WYN_ASSERT(res == sizeof(callback));
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_window_t wyn_open_window(void)
{
    Screen* const screen = DefaultScreenOfDisplay(wyn_state.display);
    const Window root = RootWindowOfScreen(screen);

    const Window xWnd = XCreateSimpleWindow(
        wyn_state.display, root,
        0, 0, 640, 480,
        0, None, None
    );

    return (wyn_window_t)xWnd;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_close_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;

    [[maybe_unused]] const int res = XDestroyWindow(wyn_state.display, xWnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_show_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;
    
    [[maybe_unused]] const int res = XMapRaised(wyn_state.display, xWnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_hide_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;

    [[maybe_unused]] const int res = XUnmapWindow(wyn_state.display, xWnd);
}

// ================================================================================================================================
