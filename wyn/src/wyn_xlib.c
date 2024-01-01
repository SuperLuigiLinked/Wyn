/**
 * @file wyn_xlib.c
 * @brief Implementation of Wyn for the Xlib backend.
 */

#define _GNU_SOURCE

#include <wyn.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/types.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrandr.h>

#if (__STDC_VERSION__ <= 201710L)
    #ifdef true
        #undef true
    #endif
    #ifdef false
        #undef false
    #endif
    #define true ((wyn_bool_t)1)
    #define false ((wyn_bool_t)0)
#endif

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/// @see abort | <stdlib.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/program/abort | https://man7.org/linux/man-pages/man3/abort.3.html
#define WYN_ASSERT(expr) if (expr) {} else abort()

#ifdef NDEBUG
    #define WYN_ASSUME(expr) ((void)0)
#else
    #define WYN_ASSUME(expr) WYN_ASSERT(expr)
#endif

/// @see fprintf | <stdio.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/io/fprintf | https://man7.org/linux/man-pages/man3/printf.3.html | https://man7.org/linux/man-pages/man3/fprintf.3p.html
#define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

#define WYN_UNUSED(x) ((void)(x))

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Indices for Wyn X-Atoms.
 */
enum wyn_xlib_atom_t {
    wyn_xlib_atom_WM_PROTOCOLS,
    wyn_xlib_atom_WM_DELETE_WINDOW,
    wyn_xlib_atom_NET_WM_STATE,
    wyn_xlib_atom_NET_WM_STATE_FULLSCREEN,
    wyn_xlib_atom_len,
};
typedef enum wyn_xlib_atom_t wyn_xlib_atom_t;

/**
 * @brief Names for Wyn X-Atoms.
 */
static const char* const wyn_xlib_atom_names[wyn_xlib_atom_len] = {
    [wyn_xlib_atom_WM_PROTOCOLS] = "WM_PROTOCOLS",
    [wyn_xlib_atom_WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
    [wyn_xlib_atom_NET_WM_STATE] = "_NET_WM_STATE",
    [wyn_xlib_atom_NET_WM_STATE_FULLSCREEN] = "_NET_WM_STATE_FULLSCREEN",
};

/**
 * @brief Xlib backend state.
 */
struct wyn_xlib_t
{
    void* userdata; ///< The pointer provided by the user when the Event Loop was started.

    Display* display; ///< The Xlib Connection to the X Window System.

    XIM xim; ///< X Input Manager.

    Atom atoms[wyn_xlib_atom_len]; ///< List of cached X Atoms.

    pid_t tid_main; ///< Thread ID of the Main Thread.

    int x11_fd; ///< File Descriptor for the X11 Connection.
    int evt_fd; ///< File Descriptor for the Event Signaler.

    int xrr_event_base; ///< Base value for XRR Events.
    int xrr_error_base; ///< Base value for XRR Errors.

    _Atomic(wyn_bool_t) quitting; ///< Flag to indicate the Event Loop is quitting.
};

/**
 * @brief Static instance of Xlib backend.
 */
static struct wyn_xlib_t wyn_xlib;

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes all Wyn state.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @return `true` if successful, `false` if there were errors.
 */
static wyn_bool_t wyn_xlib_reinit(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_xlib_deinit(void);

/**
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_xlib_event_loop(void);

/**
 * @brief Responds to all pending X11 Events.
 * @param sync If true, syncs with the X Server before polling events.
 */
static void wyn_xlib_dispatch_x11(wyn_bool_t sync);

/**
 * @brief Responds to all pending Signal Events.
 */
static void wyn_xlib_dispatch_evt(void);

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

/**
 * @brief Converts from wyn coords to native coords, rounding down.
 * @param val [non-negative] The value to round down.
 * @return `floor(val)`
 */
static int wyn_xlib_floor(wyn_coord_t val);

/**
 * @brief Converts from wyn coords to native coords, rounding up.
 * @param val [non-negative] The value to round up.
 * @return `ceil(val)`
 */
static int wyn_xlib_ceil(wyn_coord_t val);

/**
 * @brief Converts a Keysym into a Keycode.
 */
static inline wyn_keycode_t wyn_xlib_map_keysym(const KeySym keysym);

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

static wyn_bool_t wyn_xlib_reinit(void* userdata)
{
    wyn_xlib = (struct wyn_xlib_t){
        .userdata = userdata,
        .display = NULL,
        .xim = NULL,
        .atoms = {0},
        .tid_main = 0,
        .x11_fd = -1,
        .evt_fd = -1,
        .xrr_event_base = 0,
        .xrr_error_base = 0,
        .quitting = false,
    };
    {
        /// @see XOpenDisplay | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XOpenDisplay.3.xhtml | https://man.archlinux.org/man/extra/libx11/XOpenDisplay.3.en
        wyn_xlib.display = XOpenDisplay(NULL);
        if (wyn_xlib.display == NULL) return false;
    }
    {
        /// @see gettid | <unistd.h> [libc] (Linux 2.4.11) | https://man7.org/linux/man-pages/man2/gettid.2.html
        wyn_xlib.tid_main = gettid();
    }
    {
        /// @see XSetErrorHandler | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetErrorHandler.3.en
        const XErrorHandler prev_error = XSetErrorHandler(wyn_xlib_error_handler);
        WYN_UNUSED(prev_error);
        /// @see XSetIOErrorHandler | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetIOErrorHandler.3.en
        const XIOErrorHandler prev_io_error = XSetIOErrorHandler(wyn_xlib_io_error_handler);
        WYN_UNUSED(prev_io_error);
        /// @see XSetIOErrorExitHandler | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetIOErrorHandler.3.en
        XSetIOErrorExitHandler(wyn_xlib.display, wyn_xlib_io_error_exit_handler, NULL);
    }
    {
        /// @see ConnectionNumber | <X11/Xlib.h> (Xlib) | https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml | https://man.archlinux.org/man/extra/libx11/ConnectionNumber.3.en | https://tronche.com/gui/x/xlib/display/display-macros.html#ConnectionNumber
        wyn_xlib.x11_fd = ConnectionNumber(wyn_xlib.display);
        if (wyn_xlib.x11_fd == -1) return false;

        /// @see eventfd | <sys/eventfd.h> [libc] (Linux 2.6.22) | https://man7.org/linux/man-pages/man2/eventfd.2.html
        /// @see EFD_SEMAPHORE | <sys/eventfd.h> (Linux 2.6.30)
        wyn_xlib.evt_fd = eventfd(0, EFD_SEMAPHORE);
        if (wyn_xlib.evt_fd == -1) return false;
    }
    {
    #if 0
        /// @see XInternAtoms | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XInternAtom.3.xhtml | https://man.archlinux.org/man/extra/libx11/XInternAtoms.3.en
        const Status res_atoms = XInternAtoms(wyn_xlib.display, (char**)wyn_xlib_atom_names, wyn_xlib_atom_len, True, wyn_xlib.atoms);
        if (res_atoms == 0) return false;
    #else
        /// @see XInternAtom | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XInternAtom.3.xhtml | https://man.archlinux.org/man/extra/libx11/XInternAtom.3.en
        for (int i = 0; i < wyn_xlib_atom_len; ++i)
        {
            wyn_xlib.atoms[i] = XInternAtom(wyn_xlib.display, wyn_xlib_atom_names[i], True);
            if (wyn_xlib.atoms[i] == None) return false;
        }
    #endif
    }
    {
        /// @see XOpenIM | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XOpenIM.3.xhtml | https://man.archlinux.org/man/extra/libx11/XOpenIM.3.en
        wyn_xlib.xim = XOpenIM(wyn_xlib.display, NULL, NULL, NULL);
        if (wyn_xlib.xim == NULL) return false;

        /// @see XkbSetDetectableAutoRepeat | <X11/XKBlib.h> [libX11] (Xkb) | https://www.x.org/releases/current/doc/man/man3/XkbSetDetectableAutoRepeat.3.xhtml | https://man.archlinux.org/man/extra/libx11/XkbSetDetectableAutoRepeat.3.en
        const Bool res_repeat = XkbSetDetectableAutoRepeat(wyn_xlib.display, true, NULL);
        if (res_repeat != True) return false;
    }
    {
        /// @see XRRQueryExtension | <X11/extensions/Xrandr.h> [libXrandr] (Xrandr) | https://www.x.org/releases/current/doc/man/man3/Xrandr.3.xhtml | https://linux.die.net/man/3/xrrqueryextension
        const Bool res_query = XRRQueryExtension(wyn_xlib.display, &wyn_xlib.xrr_event_base, &wyn_xlib.xrr_error_base);
        if (res_query != True) return false;

        /// @see XRRSelectInput | <X11/extensions/Xrandr.h> [libXrandr] (Xrandr) | https://www.x.org/releases/current/doc/man/man3/Xrandr.3.xhtml | https://linux.die.net/man/3/xrrselectinput
        /// @see DefaultRootWindow | <X11/Xlib.h> (Xlib) | https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml | https://man.archlinux.org/man/extra/libx11/DefaultRootWindow.3.en | https://tronche.com/gui/x/xlib/display/display-macros.html#DefaultRootWindow
        XRRSelectInput(wyn_xlib.display, DefaultRootWindow(wyn_xlib.display), RRScreenChangeNotifyMask);
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_xlib_deinit(void)
{
    if (wyn_xlib.evt_fd != -1)
    {
        /// @see close | <unistd.h> [libc] (POSIX.1) | https://man7.org/linux/man-pages/man2/close.2.html
        const int res_evt = close(wyn_xlib.evt_fd);
        (void)(res_evt == 0);
    }
    if (wyn_xlib.xim != NULL)
    {
        /// @see XCloseIM | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XOpenIM.3.xhtml | https://man.archlinux.org/man/extra/libx11/XCloseIM.3.en
        const Status res_im = XCloseIM(wyn_xlib.xim);
        WYN_UNUSED(res_im);
    }
    if (wyn_xlib.display != NULL)
    {
        /// @see XCloseDisplay | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XOpenDisplay.3.xhtml | https://man.archlinux.org/man/extra/libx11/XCloseDisplay.3.en
        const int res_disp = XCloseDisplay(wyn_xlib.display);
        WYN_UNUSED(res_disp);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_xlib_event_loop(void)
{
    {
        /// @see XFlush | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XFlush.3.xhtml | https://man.archlinux.org/man/extra/libx11/XFlush.3.en
        const int res_flush = XFlush(wyn_xlib.display);
        WYN_UNUSED(res_flush);
    }

    while (!wyn_quitting())
    {
        enum { evt_idx, x11_idx, nfds };

        /// @see poll | <poll.h> [libc] (Linux 2.1.23) | https://man7.org/linux/man-pages/man2/poll.2.html
        struct pollfd fds[nfds] = {
            [evt_idx] = { .fd = wyn_xlib.evt_fd, .events = POLLIN, .revents = 0 },
            [x11_idx] = { .fd = wyn_xlib.x11_fd, .events = POLLIN, .revents = 0 },
        };
        const int res_poll = poll(fds, nfds, -1);
        WYN_ASSERT((res_poll != -1) && (res_poll != 0));

        const short evt_events = fds[evt_idx].revents;
        const short x11_events = fds[x11_idx].revents;

        if (evt_events != 0)
        {
            WYN_ASSERT(evt_events == POLLIN);
            wyn_xlib_dispatch_evt();
        }
        
        if (x11_events != 0)
        {
            WYN_ASSERT(x11_events == POLLIN);
            wyn_xlib_dispatch_x11(false);
        }
    }

    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_xlib_dispatch_x11(wyn_bool_t const sync)
{
    #define WYN_EVT_LOG(...) // WYN_LOG(__VA_ARGS__)

    if (sync)
    {
        /// @see XSync | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XFlush.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSync.3.en
        const int res_sync = XSync(wyn_xlib.display, False);
        WYN_UNUSED(res_sync);
    }

    /// @see XPending | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XFlush.3.xhtml | https://man.archlinux.org/man/extra/libx11/XPending.3.en
    while (XPending(wyn_xlib.display) > 0)
    {
        /// @see XEvent | <X11/Xlib> (Xlib) | https://www.x.org/releases/current/doc/man/man3/XAnyEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XEvent.3.en
        XEvent event;
        /// @see XNextEvent | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XNextEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XNextEvent.3.en
        const int res_next = XNextEvent(wyn_xlib.display, &event);
        WYN_UNUSED(res_next);

        WYN_EVT_LOG("[X-EVENT] (%2d)\n", event.type);

        switch (event.type)
        {
            /// @see ClientMessage | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XClientMessageEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XClientMessageEvent.3.en
            case ClientMessage:
            {
                const XClientMessageEvent* const xevt = &event.xclient;

                /// @see WM_PROTOCOLS | (ICCCM) | https://tronche.com/gui/x/icccm/sec-4.html#WM_PROTOCOLS
                if (xevt->message_type == wyn_xlib.atoms[wyn_xlib_atom_WM_PROTOCOLS])
                {
                    WYN_ASSERT(xevt->format == 32);
                    Atom const atom = (Atom)xevt->data.l[0];

                    /// @see WM_DELETE_WINDOW | (ICCCM) | https://tronche.com/gui/x/icccm/sec-4.html#WM_PROTOCOLS
                    if (atom == wyn_xlib.atoms[wyn_xlib_atom_WM_DELETE_WINDOW])
                    {
                        WYN_EVT_LOG("* WM_PROTOCOLS/WM_DELETE_WINDOW\n");
                        wyn_on_window_close(wyn_xlib.userdata, (wyn_window_t)xevt->window);
                    }
                    else
                    {
                        WYN_EVT_LOG("* WM_PROTOCOLS/(%lu)\n", (unsigned long)atom);
                    }
                }
                else
                {
                    WYN_EVT_LOG("* ClientMessage/(%lu)\n", (unsigned long)event.xclient.message_type);
                }

                break;
            }

            /// @see Expose | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XExposeEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XExposeEvent.3.en
            case Expose:
            {
                const XExposeEvent* const xevt = &event.xexpose;
                wyn_on_window_redraw(wyn_xlib.userdata, (wyn_window_t)xevt->window);
                break;
            }

            /// @see FocusIn | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XFocusChangeEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XFocusChangeEvent.3.en
            case FocusIn:
            {
                const XFocusInEvent* const xevt = &event.xfocus;
                wyn_on_window_focus(wyn_xlib.userdata, (wyn_window_t)xevt->window, true);
                break;
            }

            /// @see FocusOut | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XFocusChangeEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XFocusChangeEvent.3.en
            case FocusOut:
            {
                const XFocusInEvent* const xevt = &event.xfocus;
                wyn_on_window_focus(wyn_xlib.userdata, (wyn_window_t)xevt->window, false);
                break;
            }

            /// @see ConfigureNotify | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XConfigureEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XConfigureEvent.3.en
            case ConfigureNotify:
            {
                const XConfigureEvent* const xevt = &event.xconfigure;
                const wyn_rect_t content = {
                    .origin = { .x = (wyn_coord_t)xevt->x, .y = (wyn_coord_t)xevt->y },
                    .extent = { .w =(wyn_coord_t)xevt->width, .h = (wyn_coord_t)xevt->height }
                };
                wyn_on_window_reposition(wyn_xlib.userdata, (wyn_window_t)xevt->window, content, (wyn_coord_t)1.0);
                break;
            }

            /// @see MotionNotify | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XMotionEvent.3.en
            case MotionNotify:
            {
                const XPointerMovedEvent* const xevt = &event.xmotion;
                wyn_on_cursor(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)xevt->x, (wyn_coord_t)xevt->y);
                break;
            }
            
            /// @see EnterNotify | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XCrossingEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XCrossingEvent.3.en
            case EnterNotify:
            {
                const XEnterWindowEvent* const xevt = &event.xcrossing;
                WYN_UNUSED(xevt);
                break;
            }
            
            /// @see LeaveNotify | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XCrossingEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XCrossingEvent.3.en
            case LeaveNotify:
            {
                const XLeaveWindowEvent* const xevt = &event.xcrossing;
                wyn_on_cursor_exit(wyn_xlib.userdata, (wyn_window_t)xevt->window);
                break;
            }

            /// @see ButtonPress | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XButtonEvent.3.en
            case ButtonPress:
            {
                const XButtonPressedEvent* const xevt = &event.xbutton;
                
                switch (xevt->button)
                {
                case 4:
                    wyn_on_scroll(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)0.0, (wyn_coord_t)1.0);
                    break;
                case 5:
                    wyn_on_scroll(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)0.0, (wyn_coord_t)-1.0);
                    break;
                case 6:
                    wyn_on_scroll(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)-1.0, (wyn_coord_t)0.0);
                    break;
                case 7:
                    wyn_on_scroll(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)1.0, (wyn_coord_t)0.0);
                    break;
                default:
                    wyn_on_mouse(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_button_t)xevt->button, true);
                    break;
                }

                break;
            }

            /// @see ButtonRelease | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XButtonEvent.3.en
            case ButtonRelease:
            {
                const XButtonReleasedEvent* const xevt = &event.xbutton;
                
                switch (xevt->button)
                {
                case 4:
                case 5:
                case 6:
                case 7:
                    break;
                default:
                    wyn_on_mouse(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_button_t)xevt->button, false);
                    break;
                }

                break;
            }

            /// @see KeyPress | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XKeyEvent.3.en
            case KeyPress:
            {
                const XKeyPressedEvent* const xevt = &event.xkey;
                wyn_on_keyboard(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_keycode_t)xevt->keycode, true);
                // {
                //     const KeyCode keycode = (KeyCode)xevt->keycode;
                //     const KeySym keysym = XKeycodeToKeysym(wyn_xlib.display, keycode, 0);
                //     WYN_LOG("[WYN] %u -> %u\n", (unsigned)keycode, (unsigned)keysym);
                // }
                {
                    /// @see XCreateIC | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XCreateIC.3.xhtml | https://man.archlinux.org/man/extra/libx11/XCreateIC.3.en
                    const XIC xic = XCreateIC(wyn_xlib.xim,
                        XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, xevt->window,
                        XNFocusWindow,  xevt->window,
                        (void*)0
                    );
                    WYN_ASSERT(xic != NULL);

                    {
                        /// @see Xutf8LookupString | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XmbLookupString.3.xhtml | https://man.archlinux.org/man/extra/libx11/Xutf8LookupString.3.en
                        KeySym keysym = 0;
                        Status status = 0;
                        char buffer[5] = {0};
                        const int len = Xutf8LookupString(xic, &event.xkey, buffer, sizeof(buffer) - 1, &keysym, &status);
                        
                        // WYN_LOG("[WYN] <%d> (%ld) [%d] \"%.4s\"\n", (int)status, (long)keysym, (int)len, (const char*)buffer);
                        // for (int i = 0; i < len; ++i)
                        // {
                        //     WYN_LOG("* %02X\n", (unsigned)(unsigned char)buffer[i]);
                        // }

                        if (len > 0)
                            wyn_on_text(wyn_xlib.userdata, (wyn_window_t)xevt->window, (const wyn_utf8_t*)buffer);
                    }

                    /// @see XCreateIC | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XCreateIC.3.xhtml | https://man.archlinux.org/man/extra/libx11/XDestroyIC.3.en
                    XDestroyIC(xic);
                }

                break;
            }

            /// @see KeyRelease | <X11/X.h> (X11) | https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XKeyEvent.3.en
            case KeyRelease:
            {
                const XKeyReleasedEvent* const xevt = &event.xkey;
                wyn_on_keyboard(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_keycode_t)xevt->keycode, false);
                break;
            }

            default:
            {
                /// @see Xrandr | <X11/extensions/randr.h> [libXrandr] (Xrandr) | https://man.archlinux.org/man/extra/libxrandr/Xrandr.3.en
                const int xrr_evt = event.type - wyn_xlib.xrr_event_base;
                if ((unsigned)xrr_evt < RRNumberEvents)
                {
                    switch (xrr_evt)
                    {
                        case RRScreenChangeNotify:
                        {
                            const XRRScreenChangeNotifyEvent* const xevt = (const XRRScreenChangeNotifyEvent*)&event;
                            WYN_UNUSED(xevt);
                            wyn_on_display_change(wyn_xlib.userdata);
                            break;
                        }
                        
                        // case RRNotify:
                        // {
                        //     const XRRNotifyEvent* const xevt = (const XRRNotifyEvent*)&event;
                        //     WYN_UNUSED(xevt);
                        //     break;
                        // }
                    }
                }
            }
        }
    }

    #undef WYN_EVT_LOG
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_xlib_dispatch_evt(void)
{
    /// @see read | <unistd.h> [libc] (POSIX.1) | https://man7.org/linux/man-pages/man2/read.2.html
    uint64_t val = 0;
    const ssize_t res = read(wyn_xlib.evt_fd, &val, sizeof(val));
    WYN_ASSERT(res != -1);

    wyn_on_signal(wyn_xlib.userdata);
}

// --------------------------------------------------------------------------------------------------------------------------------

/// @see XSetErrorHandler | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetErrorHandler.3.en
static int wyn_xlib_error_handler(Display* const display, XErrorEvent* const error)
{
    WYN_UNUSED(display); WYN_UNUSED(error);

    WYN_LOG("[XLIB ERROR] <%d> %hhu (%hhu.%hhu)\n",
        error->type, error->error_code, error->request_code, error->minor_code
    );
    return 0;
}

/// @see XSetIOErrorHandler | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetIOErrorHandler.3.en
static int wyn_xlib_io_error_handler(Display* const display)
{
    WYN_UNUSED(display);

    WYN_LOG("[XLIB IO ERROR]\n");
    return 0;
}

/// @see XSetIOErrorExitHandler | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetIOErrorHandler.3.en
static void wyn_xlib_io_error_exit_handler(Display* const display, void* const userdata)
{
    WYN_UNUSED(display); WYN_UNUSED(userdata);

    WYN_LOG("[XLIB IO ERROR EXIT]\n");
    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

static int wyn_xlib_floor(wyn_coord_t const val)
{
    const int cast = (int)val;
    return cast - ((val < 0) && ((wyn_coord_t)cast != val));
}

static int wyn_xlib_ceil(wyn_coord_t const val)
{
    const int cast = (int)val;
    return cast + ((val >= 0) && ((wyn_coord_t)cast != val));
}

// --------------------------------------------------------------------------------------------------------------------------------

static inline wyn_keycode_t wyn_xlib_map_keysym(const KeySym keysym)
{
    if (keysym == NoSymbol) return (wyn_keycode_t)~0;

    /// @see XKeysymToKeycode | <X11/Xlib.h> [libX11] (Xlib) | https://man.archlinux.org/man/extra/libx11/XKeysymToKeycode.3.en
    const KeyCode keycode = XKeysymToKeycode(wyn_xlib.display, keysym);
    return keycode == NoSymbol ? (wyn_keycode_t)~0 : (wyn_keycode_t)keycode; 
}

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* const userdata)
{
    if (wyn_xlib_reinit(userdata))
    {
        wyn_on_start(userdata);
        wyn_xlib_event_loop();
        wyn_on_stop(userdata);
    }
    wyn_xlib_deinit();
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_quit(void)
{
    /// @see atomic_store_explicit | <stdatomic.h> (C11) | https://en.cppreference.com/w/c/atomic/atomic_store
    atomic_store_explicit(&wyn_xlib.quitting, true, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_quitting(void)
{
    /// @see atomic_load_explicit | <stdatomic.h> (C11) | https://en.cppreference.com/w/c/atomic/atomic_load
    return atomic_load_explicit(&wyn_xlib.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_is_this_thread(void)
{
    /// @see gettid | <unistd.h> [libc] (Linux 2.4.11) | https://man7.org/linux/man-pages/man2/gettid.2.html
    return (wyn_bool_t)(gettid() == wyn_xlib.tid_main);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_signal(void)
{
    /// @see write | <unistd.h> [libc] (POSIX.1) | https://man7.org/linux/man-pages/man2/write.2.html
    const uint64_t val = 1;
    const ssize_t res = write(wyn_xlib.evt_fd, &val, sizeof(val));
    WYN_ASSERT(res != -1);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_window_t wyn_window_open(void)
{
    /// @see DefaultScreenOfDisplay | <X11/Xlib.h> (Xlib) | https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml | https://man.archlinux.org/man/extra/libx11/DefaultScreenOfDisplay.3.en
    Screen* const screen = DefaultScreenOfDisplay(wyn_xlib.display);
    /// @see RootWindowOfScreen | <X11/Xlib.h> (Xlib) | https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml | https://man.archlinux.org/man/extra/libx11/RootWindowOfScreen.3.en
    Window const root = RootWindowOfScreen(screen);

    /// @see XSetWindowAttributes | <X11/Xlib.h> (Xlib) | https://www.x.org/releases/current/doc/man/man3/XCreateWindow.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetWindowAttributes.3.en
    XSetWindowAttributes attr = {
        .event_mask = NoEventMask
            | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask
            | EnterWindowMask | LeaveWindowMask
            // | PointerMotionHintMask
            | PointerMotionMask | Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask | ButtonMotionMask
            | KeymapStateMask
            | ExposureMask
            | VisibilityChangeMask
            | StructureNotifyMask
            // | ResizeRedirectMask
            | SubstructureNotifyMask
            // | SubstructureRedirectMask
            | FocusChangeMask
            | PropertyChangeMask
            | ColormapChangeMask
            | OwnerGrabButtonMask
    };
    const unsigned long mask = CWEventMask;

    /// @see XCreateWindow | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XCreateWindow.3.xhtml | https://man.archlinux.org/man/extra/libx11/XCreateWindow.3.en
    Window const x11_window = XCreateWindow(
        wyn_xlib.display, root,
        0, 0, 640, 480,
        0, CopyFromParent, InputOutput, CopyFromParent,
        mask, &attr
    );

    if (x11_window != 0)
    {
        /// @see XSetWMProtocols | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSetWMProtocols.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetWMProtocols.3.en
        const Status res_proto = XSetWMProtocols(wyn_xlib.display, x11_window, wyn_xlib.atoms, 2);
        WYN_ASSERT(res_proto != 0);

    #if 0
        /// @see XSetWindowBackground | <X11/Xlib.h> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XChangeWindowAttributes.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSetWindowBackground.3.en
        const int res_back = XSetWindowBackground(wyn_xlib.display, x11_window, BlackPixel(wyn_xlib.display, DefaultScreen(wyn_xlib.display)));
        WYN_UNUSED(res_back);
    #endif
    }

    return (wyn_window_t)x11_window;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_close(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    Window const x11_window = (Window)window;
    
    /// @see XDestroyWindow | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XDestroyWindow.3.xhtml | https://man.archlinux.org/man/extra/libx11/XDestroyWindow.3.en
    const int res = XDestroyWindow(wyn_xlib.display, x11_window);
    WYN_UNUSED(res);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_show(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    Window const x11_window = (Window)window;
    
    /// @see XMapRaised | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XMapWindow.3.xhtml | https://man.archlinux.org/man/extra/libx11/XMapRaised.3.en
    const int res = XMapRaised(wyn_xlib.display, x11_window);
    WYN_UNUSED(res);

    wyn_xlib_dispatch_x11(true);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_hide(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    Window const x11_window = (Window)window;

    /// @see XUnmapWindow | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XUnmapWindow.3.xhtml | https://man.archlinux.org/man/extra/libx11/XUnmapWindow.3.en
    const int res = XUnmapWindow(wyn_xlib.display, x11_window);
    WYN_UNUSED(res);

    wyn_xlib_dispatch_x11(true);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_coord_t wyn_window_scale(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    WYN_UNUSED(window);

    return (wyn_coord_t)1.0;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_rect_t wyn_window_position(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    Window const x11_window = (Window)window;
        
    /// @see XGetWindowAttributes | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XGetWindowAttributes.3.xhtml | https://man.archlinux.org/man/extra/libx11/XGetWindowAttributes.3.en
    XWindowAttributes attr;
    const Status res = XGetWindowAttributes(wyn_xlib.display, x11_window, &attr);
    WYN_ASSERT(res != 0);
    
    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)(attr.x), .y = (wyn_coord_t)(attr.y) },
        .extent = { .w = (wyn_coord_t)(attr.width), .h = (wyn_coord_t)(attr.height) }
    };
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_reposition(wyn_window_t const window, const wyn_point_t* const origin, const wyn_extent_t* const extent)
{
    WYN_ASSUME(window != NULL);
    Window const x11_window = (Window)window;

    const int rounded_x = origin ? wyn_xlib_floor(origin->x) : 0;
    const int rounded_y = origin ? wyn_xlib_floor(origin->y) : 0;
    const int rounded_w = extent ? wyn_xlib_ceil(extent->w) : 0;
    const int rounded_h = extent ? wyn_xlib_ceil(extent->h) : 0;
    
    if (origin && extent)
    {
        /// @see XMoveResizeWindow | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml | https://man.archlinux.org/man/extra/libx11/XMoveResizeWindow.3.en
        const int res = XMoveResizeWindow(wyn_xlib.display, x11_window, (int)rounded_x, (int)rounded_y, (unsigned int)rounded_w, (unsigned int)rounded_h);
        WYN_UNUSED(res);
    }
    else if (extent)
    {
        /// @see XResizeWindow | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml | https://man.archlinux.org/man/extra/libx11/XResizeWindow.3.en
        const int res = XResizeWindow(wyn_xlib.display, x11_window, (unsigned int)rounded_w, (unsigned int)rounded_h);
        WYN_UNUSED(res);
    }
    else if (origin)
    {
        /// @see XMoveWindow | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml | https://man.archlinux.org/man/extra/libx11/XMoveWindow.3.en
        const int res = XMoveWindow(wyn_xlib.display, x11_window, (int)rounded_x, (int)rounded_y);
        WYN_UNUSED(res);
    }

    wyn_xlib_dispatch_x11(true);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_window_is_fullscreen(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    Window const x11_window = (Window)window;

    Atom prop_type = 0;
    int prop_format = 0;
    unsigned long num_items = 0;
    unsigned long extra_bytes = 0;
    unsigned char* value = NULL;

    /// @see XGetWindowProperty | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XGetWindowProperty.3.xhtml | https://man.archlinux.org/man/extra/libx11/XGetWindowProperty.3.en
    const int res = XGetWindowProperty(
        wyn_xlib.display, x11_window,
        wyn_xlib.atoms[wyn_xlib_atom_NET_WM_STATE], 0, sizeof(Atom), False, XA_ATOM,
        &prop_type, &prop_format, &num_items, &extra_bytes, &value
    );
    WYN_ASSERT(res == Success);
    WYN_ASSERT(prop_type == XA_ATOM);
    WYN_ASSERT(prop_format == 32);

    wyn_bool_t found = false;
    for (unsigned long idx = 0; idx < num_items; ++idx)
    {
        /// @see memcpy | <string.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/string/byte/memcpy | https://man7.org/linux/man-pages/man3/memcpy.3.html
        long item = 0; 
        memcpy(&item, value + sizeof(item) * idx, sizeof(item));
        const Atom atom = (Atom)item;

    #if 0
        /// @see XGetAtomName | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XInternAtom.3.xhtml | https://man.archlinux.org/man/extra/libx11/XGetAtomName.3.en
        const char* name = XGetAtomName(wyn_xlib.display, atom);
        WYN_LOG("[WYN] [%lu] S: {%ld} \"%s\" |\n", idx, atom, name);
    #endif

        found |= (atom == wyn_xlib.atoms[wyn_xlib_atom_NET_WM_STATE_FULLSCREEN]);
        if (found) break;
    }

    /// @see XFree | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XFree.3.xhtml | https://man.archlinux.org/man/extra/libx11/XFree.3.en
    const int res_free = XFree(value);
    WYN_UNUSED(res_free);

    return found;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_fullscreen(wyn_window_t const window, wyn_bool_t const status)
{
    WYN_ASSUME(window != NULL);
    Window const x11_window = (Window)window;

    XEvent xevt = {
        .xclient = {
            .type = ClientMessage,
            .window = x11_window,
            .message_type = wyn_xlib.atoms[wyn_xlib_atom_NET_WM_STATE],
            .format = 32,
            .data = { .l = { (long)status, (long)wyn_xlib.atoms[wyn_xlib_atom_NET_WM_STATE_FULLSCREEN], 0, 1, 0 } },
        }
    };
    /// @see XSendEvent | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSendEvent.3.xhtml | https://man.archlinux.org/man/extra/libx11/XSendEvent.3.en
    /// @see DefaultRootWindow | <X11/Xlib.h> (Xlib) | https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml | https://man.archlinux.org/man/extra/libx11/DefaultRootWindow.3.en | https://tronche.com/gui/x/xlib/display/display-macros.html#DefaultRootWindow
    const Status res_send = XSendEvent(wyn_xlib.display, DefaultRootWindow(wyn_xlib.display), 0, SubstructureRedirectMask | SubstructureNotifyMask, &xevt);
    WYN_ASSERT(res_send != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_retitle(wyn_window_t const window, const wyn_utf8_t* const title)
{
    WYN_ASSUME(window != NULL);
    Window const x11_window = (Window)window;

    /// @see XStoreName | <X11/Xlib> [libX11] (Xlib) | https://www.x.org/releases/current/doc/man/man3/XSetWMName.3.xhtml | https://man.archlinux.org/man/extra/libx11/XStoreName.3.en
    const int res = XStoreName(wyn_xlib.display, x11_window, (title ? (const char*)title : ""));
    WYN_UNUSED(res);
}

// ================================================================================================================================

extern unsigned int wyn_enumerate_displays(wyn_display_callback const callback, void* const userdata)
{
    unsigned int counter = 0;

    /// @see DefaultRootWindow | <X11/Xlib.h> (Xlib) | https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml | https://man.archlinux.org/man/extra/libx11/DefaultRootWindow.3.en | https://tronche.com/gui/x/xlib/display/display-macros.html#DefaultRootWindow
    /// @see XRRGetScreenResourcesCurrent | <X11/extensions/Xrandr.h> [libXrandr] (Xrandr) | https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt#n1140
    XRRScreenResources* const xrr = XRRGetScreenResourcesCurrent(wyn_xlib.display, DefaultRootWindow(wyn_xlib.display));
    if (xrr)
    {
        wyn_bool_t cont = true;
        
        for (int idx = 0; cont && (idx < xrr->ncrtc); ++idx)
        {
            RRCrtc const crtc = xrr->crtcs[idx];
            if (crtc != None)
            {
                /// @see XRRGetCrtcInfo | <X11/extensions/Xrandr.h> [libXrandr] (Xrandr) | https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt#n975
                XRRCrtcInfo* const info = XRRGetCrtcInfo(wyn_xlib.display, xrr, crtc);
                if ((info != None) && (info->mode != None))
                {
                    ++counter;
                    if (callback) cont = callback(userdata, info);
                }
                XRRFreeCrtcInfo(info);
            }
        }
    }
    XRRFreeScreenResources(xrr);

    return counter;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_rect_t wyn_display_position(wyn_display_t const display)
{
    WYN_ASSUME(display != NULL);
    XRRCrtcInfo* const info = (XRRCrtcInfo*)display;

    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)info->x, .y = (wyn_coord_t)info->y },
        .extent = { .w = (wyn_coord_t)info->width, .h = (wyn_coord_t)info->height }
    };
}

// ================================================================================================================================

extern void* wyn_native_context(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    WYN_UNUSED(window);

    return wyn_xlib.display;
}

// ================================================================================================================================

extern const wyn_vb_mapping_t* wyn_vb_mapping(void)
{
    static const wyn_vb_mapping_t mapping = {
        [wyn_vb_left]   = Button1,
        [wyn_vb_right]  = Button3,
        [wyn_vb_middle] = Button2,
    };
    return &mapping;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern const wyn_vk_mapping_t* wyn_vk_mapping(void)
{
    static wyn_vk_mapping_t mapping = {0};
    mapping[wyn_vk_0]              = wyn_xlib_map_keysym(XK_0);
    mapping[wyn_vk_1]              = wyn_xlib_map_keysym(XK_1);
    mapping[wyn_vk_2]              = wyn_xlib_map_keysym(XK_2);
    mapping[wyn_vk_3]              = wyn_xlib_map_keysym(XK_3);
    mapping[wyn_vk_4]              = wyn_xlib_map_keysym(XK_4);
    mapping[wyn_vk_5]              = wyn_xlib_map_keysym(XK_5);
    mapping[wyn_vk_6]              = wyn_xlib_map_keysym(XK_6);
    mapping[wyn_vk_7]              = wyn_xlib_map_keysym(XK_7);
    mapping[wyn_vk_8]              = wyn_xlib_map_keysym(XK_8);
    mapping[wyn_vk_9]              = wyn_xlib_map_keysym(XK_9);
    mapping[wyn_vk_A]              = wyn_xlib_map_keysym(XK_A);
    mapping[wyn_vk_B]              = wyn_xlib_map_keysym(XK_B);
    mapping[wyn_vk_C]              = wyn_xlib_map_keysym(XK_C);
    mapping[wyn_vk_D]              = wyn_xlib_map_keysym(XK_D);
    mapping[wyn_vk_E]              = wyn_xlib_map_keysym(XK_E);
    mapping[wyn_vk_F]              = wyn_xlib_map_keysym(XK_F);
    mapping[wyn_vk_G]              = wyn_xlib_map_keysym(XK_G);
    mapping[wyn_vk_H]              = wyn_xlib_map_keysym(XK_H);
    mapping[wyn_vk_I]              = wyn_xlib_map_keysym(XK_I);
    mapping[wyn_vk_J]              = wyn_xlib_map_keysym(XK_J);
    mapping[wyn_vk_K]              = wyn_xlib_map_keysym(XK_K);
    mapping[wyn_vk_L]              = wyn_xlib_map_keysym(XK_L);
    mapping[wyn_vk_M]              = wyn_xlib_map_keysym(XK_M);
    mapping[wyn_vk_N]              = wyn_xlib_map_keysym(XK_N);
    mapping[wyn_vk_O]              = wyn_xlib_map_keysym(XK_O);
    mapping[wyn_vk_P]              = wyn_xlib_map_keysym(XK_P);
    mapping[wyn_vk_Q]              = wyn_xlib_map_keysym(XK_Q);
    mapping[wyn_vk_R]              = wyn_xlib_map_keysym(XK_R);
    mapping[wyn_vk_S]              = wyn_xlib_map_keysym(XK_S);
    mapping[wyn_vk_T]              = wyn_xlib_map_keysym(XK_T);
    mapping[wyn_vk_U]              = wyn_xlib_map_keysym(XK_U);
    mapping[wyn_vk_V]              = wyn_xlib_map_keysym(XK_V);
    mapping[wyn_vk_W]              = wyn_xlib_map_keysym(XK_W);
    mapping[wyn_vk_X]              = wyn_xlib_map_keysym(XK_X);
    mapping[wyn_vk_Y]              = wyn_xlib_map_keysym(XK_Y);
    mapping[wyn_vk_Z]              = wyn_xlib_map_keysym(XK_Z);
    mapping[wyn_vk_Left]           = wyn_xlib_map_keysym(XK_Left);
    mapping[wyn_vk_Right]          = wyn_xlib_map_keysym(XK_Right);
    mapping[wyn_vk_Up]             = wyn_xlib_map_keysym(XK_Up);
    mapping[wyn_vk_Down]           = wyn_xlib_map_keysym(XK_Down);
    mapping[wyn_vk_Period]         = wyn_xlib_map_keysym(XK_period);
    mapping[wyn_vk_Comma]          = wyn_xlib_map_keysym(XK_comma);
    mapping[wyn_vk_Semicolon]      = wyn_xlib_map_keysym(XK_semicolon);
    mapping[wyn_vk_Quote]          = wyn_xlib_map_keysym(XK_apostrophe);
    mapping[wyn_vk_Slash]          = wyn_xlib_map_keysym(XK_slash);
    mapping[wyn_vk_Backslash]      = wyn_xlib_map_keysym(XK_backslash);
    mapping[wyn_vk_BracketL]       = wyn_xlib_map_keysym(XK_bracketleft);
    mapping[wyn_vk_BracketR]       = wyn_xlib_map_keysym(XK_bracketright);
    mapping[wyn_vk_Plus]           = wyn_xlib_map_keysym(XK_plus);
    mapping[wyn_vk_Minus]          = wyn_xlib_map_keysym(XK_minus);
    mapping[wyn_vk_Accent]         = wyn_xlib_map_keysym(XK_grave);
    mapping[wyn_vk_Control]        = wyn_xlib_map_keysym(XK_Control_L);
    mapping[wyn_vk_Start]          = wyn_xlib_map_keysym(XK_Meta_L);
    mapping[wyn_vk_Alt]            = wyn_xlib_map_keysym(XK_Alt_L);
    mapping[wyn_vk_Space]          = wyn_xlib_map_keysym(XK_space);
    mapping[wyn_vk_Backspace]      = wyn_xlib_map_keysym(XK_BackSpace);
    mapping[wyn_vk_Delete]         = wyn_xlib_map_keysym(XK_Delete);
    mapping[wyn_vk_Insert]         = wyn_xlib_map_keysym(XK_Insert);
    mapping[wyn_vk_Shift]          = wyn_xlib_map_keysym(XK_Shift_L);
    mapping[wyn_vk_CapsLock]       = wyn_xlib_map_keysym(XK_Caps_Lock);
    mapping[wyn_vk_Tab]            = wyn_xlib_map_keysym(XK_Tab);
    mapping[wyn_vk_Enter]          = wyn_xlib_map_keysym(XK_Return);
    mapping[wyn_vk_Escape]         = wyn_xlib_map_keysym(XK_Escape);
    mapping[wyn_vk_Home]           = wyn_xlib_map_keysym(XK_Home);
    mapping[wyn_vk_End]            = wyn_xlib_map_keysym(XK_End);
    mapping[wyn_vk_PageUp]         = wyn_xlib_map_keysym(XK_Prior);
    mapping[wyn_vk_PageDown]       = wyn_xlib_map_keysym(XK_Next);
    mapping[wyn_vk_F1]             = wyn_xlib_map_keysym(XK_F1);
    mapping[wyn_vk_F2]             = wyn_xlib_map_keysym(XK_F2);
    mapping[wyn_vk_F3]             = wyn_xlib_map_keysym(XK_F3);
    mapping[wyn_vk_F4]             = wyn_xlib_map_keysym(XK_F4);
    mapping[wyn_vk_F5]             = wyn_xlib_map_keysym(XK_F5);
    mapping[wyn_vk_F6]             = wyn_xlib_map_keysym(XK_F6);
    mapping[wyn_vk_F7]             = wyn_xlib_map_keysym(XK_F7);
    mapping[wyn_vk_F8]             = wyn_xlib_map_keysym(XK_F8);
    mapping[wyn_vk_F9]             = wyn_xlib_map_keysym(XK_F9);
    mapping[wyn_vk_F10]            = wyn_xlib_map_keysym(XK_F10);
    mapping[wyn_vk_F11]            = wyn_xlib_map_keysym(XK_F11);
    mapping[wyn_vk_F12]            = wyn_xlib_map_keysym(XK_F12);
    mapping[wyn_vk_PrintScreen]    = wyn_xlib_map_keysym(XK_Print);
    mapping[wyn_vk_ScrollLock]     = wyn_xlib_map_keysym(XK_Scroll_Lock);
    mapping[wyn_vk_NumLock]        = wyn_xlib_map_keysym(XK_Num_Lock);
    mapping[wyn_vk_Numpad0]        = wyn_xlib_map_keysym(XK_KP_0);
    mapping[wyn_vk_Numpad1]        = wyn_xlib_map_keysym(XK_KP_1);
    mapping[wyn_vk_Numpad2]        = wyn_xlib_map_keysym(XK_KP_2);
    mapping[wyn_vk_Numpad3]        = wyn_xlib_map_keysym(XK_KP_3);
    mapping[wyn_vk_Numpad4]        = wyn_xlib_map_keysym(XK_KP_4);
    mapping[wyn_vk_Numpad5]        = wyn_xlib_map_keysym(XK_KP_5);
    mapping[wyn_vk_Numpad6]        = wyn_xlib_map_keysym(XK_KP_6);
    mapping[wyn_vk_Numpad7]        = wyn_xlib_map_keysym(XK_KP_7);
    mapping[wyn_vk_Numpad8]        = wyn_xlib_map_keysym(XK_KP_8);
    mapping[wyn_vk_Numpad9]        = wyn_xlib_map_keysym(XK_KP_9);
    mapping[wyn_vk_NumpadAdd]      = wyn_xlib_map_keysym(XK_KP_Add);
    mapping[wyn_vk_NumpadSubtract] = wyn_xlib_map_keysym(XK_KP_Subtract);
    mapping[wyn_vk_NumpadMultiply] = wyn_xlib_map_keysym(XK_KP_Multiply);
    mapping[wyn_vk_NumpadDivide]   = wyn_xlib_map_keysym(XK_KP_Divide);
    mapping[wyn_vk_NumpadDecimal]  = wyn_xlib_map_keysym(XK_KP_Decimal);
    return (const wyn_vk_mapping_t*)&mapping;
}

// ================================================================================================================================
