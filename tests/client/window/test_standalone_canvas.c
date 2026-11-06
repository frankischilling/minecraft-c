/* Authored ordinary-host integration. Requires genuine imported resources;
 * all event faults below are declared native controls, never original evidence. */
#include "client/application_state.h"
#include "client/display/sdk_display_internal.h"
#include "client/display/sdk_input_internal.h"
#include "client/display/staged.h"
#include "client/gui/display_screen.h"
#include "client/gui/source_screen_internal.h"
#include "client/input/mouse_helper.h"
#include "client/runtime/bootstrap.h"
#include "client/runtime/menu_loop.h"
#include "client/startup/application_startup.h"
#include "client/window/frame.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Standalone Canvas check failed%d: %s\n", __LINE__, #x);               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static bool close_request(SDL_Window *window) {
    SDL_PropertiesID p = SDL_GetWindowProperties(window);
#ifdef _WIN32
    HWND hwnd = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    return hwnd != NULL && PostMessageW(hwnd, WM_CLOSE, 0, 0) != 0;
#else
    Display *d = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window w = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (d == NULL || w == 0)
        return false;
    XEvent e = {0};
    e.xclient.type = ClientMessage;
    e.xclient.window = w;
    e.xclient.format = 32;
    e.xclient.message_type = XInternAtom(d, "WM_PROTOCOLS", False);
    e.xclient.data.l[0] = (long)XInternAtom(d, "WM_DELETE_WINDOW", False);
    e.xclient.data.l[1] = CurrentTime;
    const Status sent = XSendEvent(d, w, False, NoEventMask, &e);
    XFlush(d);
    return sent != 0;
#endif
}

/* These readers observe the actual native peer, without SDL pumping or any
 * source Mouse poll. A submitted native warp must reach the measured point. */
static bool native_pointer(SDL_Window *window, int *x, int *y) {
    const SDL_PropertiesID p = SDL_GetWindowProperties(window);
#ifdef _WIN32
    HWND hwnd = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    POINT point;
    if (hwnd == NULL || !GetCursorPos(&point) || !ScreenToClient(hwnd, &point))
        return false;
    *x = point.x;
    *y = point.y;
    return true;
#else
    Display *d = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window w = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    Window root, child;
    int root_x, root_y;
    unsigned int mask;
    return d != NULL && w != 0 &&
           XQueryPointer(d, w, &root, &child, &root_x, &root_y, x, y, &mask) != 0;
#endif
}

static bool native_extent(SDL_Window *window, int *width, int *height) {
    const SDL_PropertiesID p = SDL_GetWindowProperties(window);
#ifdef _WIN32
    HWND hwnd = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    RECT rectangle;
    if (hwnd == NULL || !GetClientRect(hwnd, &rectangle))
        return false;
    *width = rectangle.right - rectangle.left;
    *height = rectangle.bottom - rectangle.top;
    return true;
#else
    Display *d = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window w = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    XWindowAttributes attributes;
    if (d == NULL || w == 0 || !XGetWindowAttributes(d, w, &attributes))
        return false;
    *width = attributes.width;
    *height = attributes.height;
    return true;
#endif
}

static bool native_origin_is_zero(SDL_Window *window) {
    const SDL_PropertiesID p = SDL_GetWindowProperties(window);
#ifdef _WIN32
    HWND child = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    POINT origin = {0, 0};
    HWND parent = child == NULL ? NULL : GetParent(child);
    return parent != NULL && ClientToScreen(child, &origin) && ScreenToClient(parent, &origin) &&
           origin.x == 0 && origin.y == 0;
#else
    Display *display = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window child = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    XWindowAttributes bounds;
    return display != NULL && child != 0 && XGetWindowAttributes(display, child, &bounds) &&
           bounds.x == 0 && bounds.y == 0;
#endif
}

static bool pointer_at(SDL_Window *window, int x, int y) {
    int actual_x = 0, actual_y = 0;
    for (unsigned i = 0; i < 100; ++i) {
        if (!native_pointer(window, &actual_x, &actual_y))
            return false;
        if (actual_x == x && actual_y == y)
            return true;
        SDL_Delay(2);
    }
    fprintf(stderr, "Canvas pointer timeout: expected%d,%d actual%d,%d\n", x, y, actual_x,
            actual_y);
    return false;
}

static bool native_has_focus(SDL_Window *window) {
    const SDL_PropertiesID p = SDL_GetWindowProperties(window);
#ifdef _WIN32
    HWND hwnd = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    return hwnd != NULL && GetFocus() == hwnd;
#else
    Display *d = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window w = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0), focused;
    int revert;
    if (d == NULL || w == 0)
        return false;
    XGetInputFocus(d, &focused, &revert);
    return focused == w;
#endif
}

static void request_focus(const mc_client_frame_state *host) {
    (void)SDL_RaiseWindow(host->frame_window);
    const SDL_PropertiesID p = SDL_GetWindowProperties(host->canvas_window);
#ifdef _WIN32
    HWND child = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    HWND frame = SDL_GetPointerProperty(SDL_GetWindowProperties(host->frame_window),
                                        SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (frame != NULL)
        (void)SetForegroundWindow(frame);
    if (child != NULL)
        (void)SetFocus(child);
#else
    Display *d = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window child = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    XWindowAttributes attributes;
    if (d != NULL && child != 0 && XGetWindowAttributes(d, child, &attributes) &&
        attributes.map_state == IsViewable) {
        XSetInputFocus(d, child, RevertToParent, CurrentTime);
        XFlush(d);
    }
#endif
}

static bool wait_focus(mc_client_sdk_update *update, const mc_client_frame_state *host,
                       bool active) {
    for (unsigned i = 0; i < 500; ++i) {
        if (active)
            request_focus(host);
        mc_client_sdk_update_error error;
        bool actual;
        if (mc_client_sdk_display_process_messages(update, &error) != MC_CLIENT_SDK_UPDATE_OK ||
            mc_client_sdk_display_active(update, &actual, &error) != MC_CLIENT_SDK_UPDATE_OK)
            return false;
        if (actual == active && native_has_focus(host->canvas_window) == active)
            return true;
        SDL_Delay(2);
    }
    bool final_active = false;
    mc_client_sdk_update_error final_error;
    (void)mc_client_sdk_display_active(update, &final_active, &final_error);
    fprintf(stderr,
            "Canvas focus timeout: expected%d sdk%d native%d child-flags%llu frame-flags%llu\n",
            active, final_active, native_has_focus(host->canvas_window),
            (unsigned long long)SDL_GetWindowFlags(host->canvas_window),
            (unsigned long long)SDL_GetWindowFlags(host->frame_window));
    return false;
}

static int capabilities(mc_client_sdk_input *input, int32_t *out) {
    mc_client_sdk_input_error error = {0};
    CHECK(mc_client_sdk_input_enter(input, "org/lwjgl/input/Cursor", "getCapabilities", "()I",
                                    &error));
    const bool queried = mc_client_sdk_native_cursor_capabilities(input, out, &error);
    mc_client_sdk_input_error returned;
    CHECK(mc_client_sdk_input_leave(input, &error, &returned) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(queried);
    printf("CANVAS native-cursor-capabilities=%d\n", *out);
    return 0;
}

static int mouse_helper(client_application *app) {
    mc_client_sdk_input *input = mc_client_display_inputs(app->display);
    mc_client_mouse_helper_error error;
    mc_client_mouse_helper_state state;
    CHECK(mc_client_mouse_helper_inspect(app->C, &state) && state.source_c == app->m);
    CHECK(mc_client_mouse_helper_grab(app->C, input, &error) == MC_CLIENT_MOUSE_HELPER_OK);
    CHECK(error.source_returned && error.site.pc == 14 && input->grabbed);
    CHECK(mc_client_mouse_helper_inspect(app->C, &state) && state.source_a == 0 &&
          state.source_b == 0 && state.last_store_pc == 11);
    CHECK(mc_client_mouse_helper_ungrab(app->C, input, &error) == MC_CLIENT_MOUSE_HELPER_OK);
    CHECK(error.source_returned && error.site.pc == 25 && !input->grabbed &&
          input->last_mouse_store == 106 && input->dx == 0 && input->dy == 0 && input->wheel == 0);
    CHECK(mc_client_mouse_helper_inspect(app->C, &state) && state.source_c == app->m &&
          strcmp(state.last_method, "b") == 0);
    /* An already ungrabbed b still reaches its own ordered cursor setter and
     * setGrabbed(false) poll/reset. It does not return early on the old flag. */
    CHECK(mc_client_mouse_helper_ungrab(app->C, input, &error) == MC_CLIENT_MOUSE_HELPER_OK);
    CHECK(error.source_returned && error.site.pc == 25 && !input->grabbed &&
          input->last_mouse_store == 106);
    return 0;
}

static int warp_after_resize(client_application *app, const mc_client_frame_state *host,
                             int32_t caps) {
    mc_client_sdk_input *input = mc_client_display_inputs(app->display);
    mc_client_sdk_input_error error;
    mc_client_mouse_helper_error helper;
    const int width = input->width, height = input->height;
    CHECK(!input->grabbed && width > 100 && height > 100);
    mc_client_events_state before, after;
    CHECK(mc_client_events_inspect(mc_client_display_event_port(app->display), &before));
    CHECK(SDL_SetWindowSize(host->canvas_window, width + 11, height + 11));
    int actual_width, actual_height;
    CHECK(native_extent(host->canvas_window, &actual_width, &actual_height) &&
          actual_width == width + 11 && actual_height == height + 11);
    CHECK(native_origin_is_zero(host->canvas_window));
    CHECK(input->width == width && input->height == height); /* No Mouse poll. */
    const int32_t dx = input->dx, dy = input->dy, wheel = input->wheel;
    const int32_t raw_x = input->event_raw_x, raw_y = input->event_raw_y;
    CHECK(mc_client_sdk_mouse_set_cursor_position(input, 61, 53, &error) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(input->x == 61 && input->y == 53 && input->current_mouse.x == 61 &&
          input->current_mouse.y == 53 && input->dx == dx && input->dy == dy &&
          input->wheel == wheel && input->event_raw_x == raw_x && input->event_raw_y == raw_y &&
          input->width == width && input->height == height);
    CHECK(mc_client_events_inspect(mc_client_display_event_port(app->display), &after) &&
          after.pump_generation == before.pump_generation && after.records == before.records);
    if ((caps & 1) != 0)
        CHECK(pointer_at(host->canvas_window, 61, actual_height - 1 - 53));
    else {
        CHECK(input->grab_x == 61 && input->grab_y == 53 && input->last_mouse_store == 74);
        puts("CANVAS cursor-position native warp skipped: actual capability0");
    }
    CHECK(SDL_SetWindowSize(host->canvas_window, width, height));
    mc_client_sdk_update_error update_error;
    CHECK(mc_client_sdk_display_process_messages(mc_client_display_update_owner(app->display),
                                                 &update_error) == MC_CLIENT_SDK_UPDATE_OK);
    CHECK(mc_client_mouse_helper_grab(app->C, input, &helper) == MC_CLIENT_MOUSE_HELPER_OK);
    CHECK(input->grabbed && input->height == height);
    /* The grabbed branch has no capability query, extent query or floating
     * conversion. A hostile rounding control must not manufacture a failure. */
    const unsigned int control = _mm_getcsr();
    _mm_setcsr((control & ~0x6000u) | 0x2000u);
    const mc_client_sdk_input_status positioned =
        mc_client_sdk_mouse_set_cursor_position(input, 43, 47, &error);
    _mm_setcsr(control);
    CHECK(positioned == MC_CLIENT_SDK_INPUT_OK && input->grabbed && input->grab_x == 43 &&
          input->grab_y == 47 && input->last_mouse_store == 74);
    CHECK(SDL_SetWindowSize(host->canvas_window, width + 13, height + 17));
    CHECK(native_extent(host->canvas_window, &actual_width, &actual_height) &&
          actual_width == width + 13 && actual_height == height + 17 && input->height == height);
    CHECK(native_origin_is_zero(host->canvas_window));
    CHECK(mc_client_sdk_mouse_set_grabbed(input, false, &error) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(!input->grabbed && input->last_mouse_store == 106 && input->dx == 0 && input->dy == 0 &&
          input->wheel == 0 && input->height == actual_height);
    if ((caps & 1) != 0)
        CHECK(pointer_at(host->canvas_window, 43, actual_height - 1 - 47));
    else
        puts("CANVAS ungrab native warp skipped: actual capability0");
    CHECK(SDL_SetWindowSize(host->canvas_window, width, height));
    CHECK(mc_client_sdk_display_process_messages(mc_client_display_update_owner(app->display),
                                                 &update_error) == MC_CLIENT_SDK_UPDATE_OK);
    printf("CANVAS resize-without-poll cursor-position=1 ungrab=1 cached-height=%d\n", height);
    return 0;
}

static int focus_cycle(client_application *app, const mc_client_frame_state *host) {
    mc_client_sdk_update *update = mc_client_display_update_owner(app->display);
    mc_client_source_da *original_screen = app->r;
    CHECK(!app->N && app->active_world == NULL && original_screen != NULL);
    mc_client_screen_classes *classes = original_screen->classes;
    CHECK(SDL_HideWindow(host->frame_window));
    CHECK(wait_focus(update, host, false));
    mc_client_display_screen_call *call = NULL;
    CHECK(mc_client_display_focus_run(app, classes, &call) == MC_CLIENT_DISPLAY_SCREEN_OK);
    CHECK(call->source_returned && call->focus_returned && call->focus_pc == 6 &&
          !call->display_active && !call->focus_stored && app->r == original_screen && !app->N);
    CHECK(mc_client_display_screen_retire(&call) == MC_CLIENT_DISPLAY_SCREEN_OK);
    CHECK(SDL_ShowWindow(host->frame_window) && SDL_ShowWindow(host->canvas_window));
    CHECK(wait_focus(update, host, true));
    CHECK(mc_client_display_focus_run(app, classes, &call) == MC_CLIENT_DISPLAY_SCREEN_OK);
    CHECK(call->source_returned && call->focus_returned && call->focus_pc == 51 &&
          call->display_active && call->focus_stored && call->mouse_grabbed &&
          call->focus_display != NULL && call->focus_display->fallback != NULL &&
          call->focus_display->focus_released && call->focus_display->mouse.source_returned &&
          call->focus_display->mouse.site.pc == 25 && !app->N);
    CHECK(mc_client_display_screen_retire(&call) == MC_CLIENT_DISPLAY_SCREEN_OK && app->r == NULL);
    /* Retiring the call ends its fallback source view. Restore the startup
     * screen through an actual display call, without assigning App.r or N. */
    CHECK(mc_client_display_screen_run(app, classes, original_screen, &call) ==
          MC_CLIENT_DISPLAY_SCREEN_OK);
    CHECK(call->source_returned && app->r == original_screen && !app->N);
    CHECK(mc_client_display_screen_retire(&call) == MC_CLIENT_DISPLAY_SCREEN_OK);
    puts("CANVAS real-focus lost=1 regained=1 inactive-g6=1 active-g51=1 nonnull-ungrab25=1");
    return 0;
}

#ifndef _WIN32
/* Bounded read-only X properties. No atom/property is created or changed. */
static bool x11_property32(Display *display, Window window, const char *name, Atom expected,
                           unsigned long *value) {
    if (display == NULL || window == None)
        return false;
    const Atom property = XInternAtom(display, name, True);
    if (property == None)
        return false;
    Atom actual = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char *data = NULL;
    const int status = XGetWindowProperty(display, window, property, 0, 1, False, expected, &actual,
                                          &format, &count, &remaining, &data);
    const bool observed = status == Success && actual == expected && format == 32 && count == 1 &&
                          remaining == 0 && data != NULL;
    if (observed)
        *value = ((const unsigned long *)data)[0] & 0xffffffffUL;
    if (data != NULL)
        XFree(data);
    return observed;
}

static void observe_x11_activation(const mc_client_frame_state *host, SDL_Window *other,
                                   const char *phase, long long cycle) {
    const SDL_PropertiesID properties = SDL_GetWindowProperties(host->frame_window);
    Display *display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    const Window frame =
        (Window)SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    const Window canvas = (Window)SDL_GetNumberProperty(
        SDL_GetWindowProperties(host->canvas_window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    const Window auxiliary = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(other),
                                                           SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    Window native_focus = None;
    int revert = RevertToNone;
    const bool have_focus = display != NULL && XGetInputFocus(display, &native_focus, &revert);
    unsigned long active = 0, frame_time = 0, canvas_time = 0, auxiliary_time = 0;
    const bool have_active =
        display != NULL && x11_property32(display, DefaultRootWindow(display), "_NET_ACTIVE_WINDOW",
                                          XA_WINDOW, &active);
    const bool have_frame_time =
        x11_property32(display, frame, "_NET_WM_USER_TIME", XA_CARDINAL, &frame_time);
    const bool have_canvas_time =
        x11_property32(display, canvas, "_NET_WM_USER_TIME", XA_CARDINAL, &canvas_time);
    const bool have_auxiliary_time =
        x11_property32(display, auxiliary, "_NET_WM_USER_TIME", XA_CARDINAL, &auxiliary_time);
    fprintf(stderr,
            "CANVAS WM-observation phase=%s cycle%lld native-query%d native-focus%lu "
            "Frame-xid%lu Canvas-xid%lu Aux-xid%lu WM-active-query%d WM-active%lu "
            "Frame-user-property-query%d Frame-user-property%lu Canvas-user-property-query%d "
            "Canvas-user-property%lu Aux-user-property-query%d Aux-user-property%lu\n",
            phase, cycle, have_focus, (unsigned long)native_focus, (unsigned long)frame,
            (unsigned long)canvas, (unsigned long)auxiliary, have_active, active, have_frame_time,
            frame_time, have_canvas_time, canvas_time, have_auxiliary_time, auxiliary_time);
}
#endif

static void request_other_focus(SDL_Window *window) {
    (void)SDL_RaiseWindow(window);
    const SDL_PropertiesID p = SDL_GetWindowProperties(window);
#ifdef _WIN32
    HWND hwnd = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (hwnd != NULL) {
        (void)SetForegroundWindow(hwnd);
        (void)SetFocus(hwnd);
    }
#else
    Display *display = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window xid = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    XWindowAttributes bounds;
    if (display != NULL && xid != 0 && XGetWindowAttributes(display, xid, &bounds) &&
        bounds.map_state == IsViewable) {
        XSetInputFocus(display, xid, RevertToParent, CurrentTime);
        XFlush(display);
    }
#endif
}

/* Reacquire only the real Frame. The production Frame event listener must
 * establish Canvas focus; this arm never directly focuses the child. */
static void request_frame_activation(const mc_client_frame_state *host) {
    (void)SDL_RaiseWindow(host->frame_window);
    const SDL_PropertiesID p = SDL_GetWindowProperties(host->frame_window);
#ifdef _WIN32
    HWND hwnd = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (hwnd != NULL)
        (void)SetForegroundWindow(hwnd);
#else
    Display *display = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window xid = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    XWindowAttributes bounds = {0};
    const bool have_bounds =
        display != NULL && xid != 0 && XGetWindowAttributes(display, xid, &bounds);
    if (have_bounds && bounds.map_state == IsViewable) {
        const Window canvas = (Window)SDL_GetNumberProperty(
            SDL_GetWindowProperties(host->canvas_window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        const uint64_t deadline = SDL_GetTicks() + 3000;
        mc_client_sdk_update *update = mc_client_display_update_owner(host->app->display);
        if (canvas == 0 || update == NULL) {
            fprintf(stderr, "Canvas X11 Frame activation missing Canvas or SDK peer\n");
            return;
        }
        Window focused = None;
        int revert = RevertToNone;
        for (unsigned int attempt = 0; attempt < 64 && SDL_GetTicks() < deadline; ++attempt) {
            XGetInputFocus(display, &focused, &revert);
            if (focused == canvas)
                return;
            /* A pending WM activation may overwrite an earlier request. Only
             * the real Frame is requested, and only while native focus is
             * outside this Frame/Canvas pair. The production Frame listener
             * remains responsible for focusing the child. */
            if (focused != xid)
                XSetInputFocus(display, xid, RevertToParent, CurrentTime);
            XSync(display, False);
            mc_client_sdk_update_error error;
            if (mc_client_sdk_display_process_messages(update, &error) != MC_CLIENT_SDK_UPDATE_OK) {
                fprintf(stderr, "Canvas X11 Frame activation SDK failure: %s\n",
                        error.operation == NULL ? "unknown operation" : error.operation);
                return;
            }
            XGetInputFocus(display, &focused, &revert);
            if (focused == canvas)
                return;
            SDL_Delay(2);
        }
        fprintf(stderr, "Canvas X11 Frame activation pending: frame%lu canvas%lu focus%lu\n",
                (unsigned long)xid, (unsigned long)canvas, (unsigned long)focused);
    } else {
        fprintf(stderr,
                "Canvas X11 Frame activation unavailable: display%d frame%lu bounds%d map%d\n",
                display != NULL, (unsigned long)xid, have_bounds, bounds.map_state);
    }
#endif
}

static int observe_reactivation(mc_client_sdk_update *update, const mc_client_frame_state *host,
                                SDL_Window *other, bool canvas, uint64_t deadline) {
    bool active = false;
#ifndef _WIN32
    mc_client_sdk_update_error error = {0};
    int processed = -1, active_status = -1;
#endif
    do {
#ifdef _WIN32
        mc_client_sdk_update_error error;
        CHECK(mc_client_sdk_display_process_messages(update, &error) == MC_CLIENT_SDK_UPDATE_OK &&
              mc_client_sdk_display_active(update, &active, &error) == MC_CLIENT_SDK_UPDATE_OK);
#else
        CHECK((processed = (int)mc_client_sdk_display_process_messages(update, &error)) ==
                  (int)MC_CLIENT_SDK_UPDATE_OK &&
              (active_status = (int)mc_client_sdk_display_active(update, &active, &error)) ==
                  (int)MC_CLIENT_SDK_UPDATE_OK);
#endif
        if (active == canvas && SDL_GetKeyboardFocus() == (canvas ? host->canvas_window : other) &&
            native_has_focus(canvas ? host->canvas_window : other))
            return 0;
        SDL_Delay(2);
    } while (SDL_GetTicks() < deadline);
    fprintf(stderr,
            "Canvas real reactivation timed out: expected-canvas%d SDK%d SDL-id%u native%d "
            "frame-flags%llu canvas-flags%llu\n",
            canvas, active,
            SDL_GetKeyboardFocus() == NULL ? 0 : SDL_GetWindowID(SDL_GetKeyboardFocus()),
            native_has_focus(host->canvas_window),
            (unsigned long long)SDL_GetWindowFlags(host->frame_window),
            (unsigned long long)SDL_GetWindowFlags(host->canvas_window));
#ifndef _WIN32
    const SDL_PropertiesID properties = SDL_GetWindowProperties(host->frame_window);
    Display *display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    const Window frame =
        (Window)SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    const Window child = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(host->canvas_window),
                                                       SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    const Window auxiliary = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(other),
                                                           SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    Window native_focus = None;
    int revert = RevertToNone;
    const bool have_focus = display != NULL && XGetInputFocus(display, &native_focus, &revert);
    XWindowAttributes frame_bounds = {0}, canvas_bounds = {0}, auxiliary_bounds = {0};
    const bool have_frame =
        display != NULL && frame != 0 && XGetWindowAttributes(display, frame, &frame_bounds);
    const bool have_canvas =
        display != NULL && child != 0 && XGetWindowAttributes(display, child, &canvas_bounds);
    const bool have_auxiliary = display != NULL && auxiliary != 0 &&
                                XGetWindowAttributes(display, auxiliary, &auxiliary_bounds);
    mc_client_events_state events = {0};
    const bool have_events =
        mc_client_events_inspect(mc_client_display_event_port(host->app->display), &events);
    mc_client_sdk_input_state input = {0};
    const bool have_input =
        mc_client_sdk_input_inspect(mc_client_display_inputs(host->app->display), &input);
    mc_client_sdk_bytes_state mouse_read = {0}, keyboard_read = {0};
    const bool have_mouse =
        have_input && mc_client_sdk_bytes_inspect(input.mouse_read, &mouse_read);
    const bool have_keyboard =
        have_input && mc_client_sdk_bytes_inspect(input.keyboard_read, &keyboard_read);
    const SDL_Event *pending = have_input && input.pending_native_event != NULL
                                   ? mc_client_event_read(input.pending_native_event)
                                   : NULL;
    SDL_Window *keyboard = SDL_GetKeyboardFocus();
    fprintf(
        stderr,
        "CANVAS timeout-observation expected-canvas%d processed%d active-status%d SDK-active%d "
        "SDL-id%u Frame-id%u Canvas-id%u Aux-id%u native-query%d native-focus%lu native-revert%d "
        "Frame-xid%lu Frame-query%d Frame-map%d Canvas-xid%lu Canvas-query%d Canvas-map%d "
        "Aux-xid%lu Aux-query%d Aux-map%d SDK-error%d SDK-operation=%s\n",
        canvas, processed, active_status, active, keyboard == NULL ? 0 : SDL_GetWindowID(keyboard),
        SDL_GetWindowID(host->frame_window), SDL_GetWindowID(host->canvas_window),
        SDL_GetWindowID(other), have_focus, (unsigned long)native_focus, revert,
        (unsigned long)frame, have_frame, have_frame ? frame_bounds.map_state : -1,
        (unsigned long)child, have_canvas, have_canvas ? canvas_bounds.map_state : -1,
        (unsigned long)auxiliary, have_auxiliary, have_auxiliary ? auxiliary_bounds.map_state : -1,
        (int)error.status, error.operation == NULL ? "none" : error.operation);
    fprintf(stderr,
            "CANVAS timeout-queues events-query%d ports%zu retained-global-records%zu bytes%zu "
            "tickets%zu port-tickets%zu pump-generation%" PRIu64 " events-active%d dirty%d "
            "input-query%d input-phase%d pending-native%d pending-type%u queue-full-results%zu "
            "mouse-query%d mouse-position%zu mouse-limit%zu keyboard-query%d keyboard-position%zu "
            "keyboard-limit%zu\n",
            have_events, events.ports, events.records, events.bytes, events.tickets,
            events.port_tickets, events.pump_generation, events.active, events.dirty, have_input,
            (int)input.phase, pending != NULL, pending == NULL ? 0 : pending->type,
            input.source_queue_full_results, have_mouse, mouse_read.position, mouse_read.limit,
            have_keyboard, keyboard_read.position, keyboard_read.limit);
    observe_x11_activation(host, other, "observation-timeout", -1);
#endif
    return 1;
}

#ifdef _WIN32
static bool native_cursor_constrained(SDL_Window *window) {
    HWND hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                       SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    RECT bounds, clip;
    POINT origin = {0, 0};
    if (hwnd == NULL || !GetClientRect(hwnd, &bounds) || !ClientToScreen(hwnd, &origin) ||
        !GetClipCursor(&clip))
        return false;
    return clip.right > clip.left && clip.bottom > clip.top && clip.left >= origin.x &&
           clip.top >= origin.y && clip.right <= origin.x + bounds.right &&
           clip.bottom <= origin.y + bounds.bottom;
}
#endif

/* Actual shown sibling activation, with Canvas kept visible throughout. No
 * focus records are pushed. Only the declared keyboard transport below uses
 * SDL_PushEvent; it is not a claim of physical HID delivery. */
static int real_reactivation(client_application *app, const mc_client_frame_state *host,
                             int32_t caps) {
    mc_client_sdk_update *update = mc_client_display_update_owner(app->display);
    mc_client_sdk_input *input = mc_client_display_inputs(app->display);
    mc_client_source_da *screen = app->r;
    const bool focused = app->N;
    const SDL_GLContext context = SDL_GL_GetCurrentContext();
    CHECK(screen != NULL && !focused && context != NULL &&
          SDL_GL_GetCurrentWindow() == host->canvas_window && !input->grabbed);
    SDL_Window *other =
        SDL_CreateWindow("Canvas reactivation control", 240, 140, SDL_WINDOW_HIDDEN);
    CHECK(other != NULL && other != host->frame_window && other != host->canvas_window);
    mc_client_event_port *other_events = NULL;
    mc_client_events_error events_error;
    CHECK(mc_client_events_attach(other, 0, &other_events, &events_error) == MC_CLIENT_EVENTS_OK);
    CHECK(SDL_ShowWindow(other));
    request_focus(host);
    CHECK(observe_reactivation(update, host, other, true, SDL_GetTicks() + 3000) == 0);
    mc_client_mouse_helper_error mouse_error;
    CHECK(mc_client_mouse_helper_grab(app->C, input, &mouse_error) == MC_CLIENT_MOUSE_HELPER_OK &&
          mouse_error.source_returned && input->grabbed &&
          SDL_GetWindowRelativeMouseMode(host->canvas_window));
    const uint64_t start_deadline = SDL_GetTicks() + 3000;
    size_t cycles = 0;
    while (cycles < 64 && (cycles < 2 || SDL_GetTicks() < start_deadline)) {
        /* A late start receives a complete observation bound, rather than
         * failing because the cycle-start sampling window expires midway. */
        const uint64_t deadline = SDL_GetTicks() + 3000;
        request_other_focus(other);
        CHECK(observe_reactivation(update, host, other, false, deadline) == 0);
        CHECK((SDL_GetWindowFlags(host->frame_window) & SDL_WINDOW_HIDDEN) == 0 &&
              (SDL_GetWindowFlags(host->canvas_window) & SDL_WINDOW_HIDDEN) == 0);
#ifndef _WIN32
        observe_x11_activation(host, other, "before-frame-request", (long long)cycles);
#endif
        request_frame_activation(host);
#ifndef _WIN32
        observe_x11_activation(host, other, "after-frame-request", (long long)cycles);
#endif
        CHECK(observe_reactivation(update, host, other, true, deadline) == 0);
        /* Native child focus is already established. Do not issue another
         * focus request while late actual parent activation messages drain. */
        for (size_t pump = 0; pump < 8; ++pump) {
            bool active = false;
            mc_client_sdk_update_error error;
#ifndef _WIN32
            const mc_client_sdk_update_status processed =
                mc_client_sdk_display_process_messages(update, &error);
            const bool active_called = processed == MC_CLIENT_SDK_UPDATE_OK;
            const int active_status =
                active_called ? (int)mc_client_sdk_display_active(update, &active, &error)
                              : -1; /* Explicit not-called diagnostic sentinel. */
            SDL_Window *keyboard = SDL_GetKeyboardFocus();
            const bool native_canvas = native_has_focus(host->canvas_window);
            const bool grabbed = input->grabbed;
            const bool relative = SDL_GetWindowRelativeMouseMode(host->canvas_window);
            SDL_Window *gl_window = SDL_GL_GetCurrentWindow();
            const SDL_GLContext gl_context = SDL_GL_GetCurrentContext();
            const bool same_screen = app->r == screen;
            const bool actual_focus_field = app->N;
            const bool passive = processed == MC_CLIENT_SDK_UPDATE_OK && active_called &&
                                 active_status == (int)MC_CLIENT_SDK_UPDATE_OK && active &&
                                 keyboard == host->canvas_window && native_canvas && grabbed &&
                                 relative && gl_window == host->canvas_window &&
                                 gl_context == context && same_screen &&
                                 actual_focus_field == focused;
            if (!passive) {
                const SDL_PropertiesID properties = SDL_GetWindowProperties(host->frame_window);
                Display *display =
                    SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
                const Window native_frame =
                    (Window)SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
                const Window native_child =
                    (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(host->canvas_window),
                                                  SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
                const Window native_other = (Window)SDL_GetNumberProperty(
                    SDL_GetWindowProperties(other), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
                Window native_focus = None;
                int revert = RevertToNone;
                if (display != NULL)
                    XGetInputFocus(display, &native_focus, &revert);
                fprintf(stderr,
                        "CANVAS passive-failure cycle%zu pump%zu processed%d active-called%d "
                        "active-status%d SDK-active%d SDL-id%u Frame-id%u Canvas-id%u Aux-id%u "
                        "native-canvas%d native-focus%lu native-Frame%lu native-Canvas%lu "
                        "native-Aux%lu native-revert%d grabbed%d backend-grabbed%d "
                        "relative%d GL-window-id%u GL-window-same%d GL-context-same%d "
                        "GL-context-null%d "
                        "screen-same%d App-N%d expected-N%d Frame-flags%llu Canvas-flags%llu "
                        "Aux-flags%llu SDK-error%d SDK-operation=%s\n",
                        cycles, pump, (int)processed, active_called, active_status, active,
                        keyboard == NULL ? 0 : SDL_GetWindowID(keyboard),
                        SDL_GetWindowID(host->frame_window), SDL_GetWindowID(host->canvas_window),
                        SDL_GetWindowID(other), native_canvas, (unsigned long)native_focus,
                        (unsigned long)native_frame, (unsigned long)native_child,
                        (unsigned long)native_other, revert, grabbed, input->backend_grabbed,
                        relative, gl_window == NULL ? 0 : SDL_GetWindowID(gl_window),
                        gl_window == host->canvas_window, gl_context == context, gl_context == NULL,
                        same_screen, actual_focus_field, focused,
                        (unsigned long long)SDL_GetWindowFlags(host->frame_window),
                        (unsigned long long)SDL_GetWindowFlags(host->canvas_window),
                        (unsigned long long)SDL_GetWindowFlags(other), (int)error.status,
                        error.operation == NULL ? "none" : error.operation);
            }
            CHECK(passive);
#else
            CHECK(
                mc_client_sdk_display_process_messages(update, &error) == MC_CLIENT_SDK_UPDATE_OK &&
                mc_client_sdk_display_active(update, &active, &error) == MC_CLIENT_SDK_UPDATE_OK &&
                active && SDL_GetKeyboardFocus() == host->canvas_window &&
                native_has_focus(host->canvas_window) && input->grabbed &&
                SDL_GetWindowRelativeMouseMode(host->canvas_window) &&
                SDL_GL_GetCurrentWindow() == host->canvas_window &&
                SDL_GL_GetCurrentContext() == context && app->r == screen && app->N == focused);
#endif
#ifdef _WIN32
            CHECK(native_cursor_constrained(host->canvas_window));
#endif
            SDL_Delay(2);
        }
        ++cycles;
    }
    CHECK(cycles >= 2);
    /* Deliver actual Canvas-targeted SDK keyboard records after reactivation.
     * The distinct timestamps identify these authored occurrences only. */
    mc_client_sdk_input_error input_error;
    bool next;
    mc_client_sdk_key_event key;
    size_t drained = 0;
    do {
        CHECK(mc_client_sdk_keyboard_next(input, &next, &key, &input_error) ==
              MC_CLIENT_SDK_INPUT_OK);
        ++drained;
    } while (next && drained < 256);
    CHECK(!next);
    const uint64_t stamp = UINT64_C(0x0102030405060708);
    for (size_t i = 0; i < 2; ++i) {
        SDL_Event event = {0};
        event.type = i == 0 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.timestamp = stamp + i;
        event.key.windowID = SDL_GetWindowID(host->canvas_window);
        event.key.scancode = SDL_SCANCODE_A;
        event.key.key = SDLK_A;
        event.key.down = i == 0;
        CHECK(SDL_PushEvent(&event));
    }
    mc_client_sdk_update_error update_error;
    CHECK(mc_client_sdk_display_process_messages(update, &update_error) == MC_CLIENT_SDK_UPDATE_OK);
    size_t authored = 0;
    drained = 0;
    do {
        CHECK(mc_client_sdk_keyboard_next(input, &next, &key, &input_error) ==
              MC_CLIENT_SDK_INPUT_OK);
        if (next && key.nanos >= (int64_t)stamp && key.nanos <= (int64_t)(stamp + 1)) {
            CHECK(authored < 2 && key.nanos == (int64_t)(stamp + authored) && key.key == 30 &&
                  key.pressed == (authored == 0) && !key.repeat);
            ++authored;
        }
        ++drained;
    } while (next && drained < 256);
    CHECK(!next && authored == 2);
    CHECK(mc_client_mouse_helper_ungrab(app->C, input, &mouse_error) == MC_CLIENT_MOUSE_HELPER_OK &&
          mouse_error.source_returned && mouse_error.site.pc == 25 && !input->grabbed &&
          !SDL_GetWindowRelativeMouseMode(host->canvas_window));
    int width, height;
    CHECK(native_extent(host->canvas_window, &width, &height));
    if ((caps & 1) != 0)
        CHECK(pointer_at(host->canvas_window, width / 2, height - 1 - height / 2));
    else
        puts("CANVAS reactivation native warp skipped: actual capability0");
    CHECK(mc_client_events_retire(&other_events, &events_error) == MC_CLIENT_EVENTS_OK &&
          other_events == NULL);
    SDL_DestroyWindow(other);
    CHECK(SDL_GL_GetCurrentWindow() == host->canvas_window &&
          SDL_GL_GetCurrentContext() == context && app->r == screen && app->N == focused);
#ifdef _WIN32
    const char *clip_scope = "win32";
#else
    const char *clip_scope = "not-observed";
#endif
    printf(
        "CANVAS reactivation cycles=%zu sibling-visible=1 native-canvas-focus=1 "
        "SDK-canvas-focus=1 frame-only-reactivation=1 passive-pumps=8 relative-grab=1 keyboard=2 "
        "warp-capability=%d "
        "native-clip=%s checked-owner-retirement=1\n",
        cycles, caps & 1, clip_scope);
    return 0;
}

static int fp_prefix(client_application *app, bool grabbed, int32_t caps) {
    if ((caps & 1) == 0) {
        puts("CANVAS warp FP prefix skipped: actual capability0");
        return 0;
    }
    mc_client_sdk_input *input = mc_client_display_inputs(app->display);
    mc_client_mouse_helper_error error;
    if (grabbed)
        CHECK(mc_client_mouse_helper_grab(app->C, input, &error) == MC_CLIENT_MOUSE_HELPER_OK);
    int32_t width, height;
    CHECK(mc_client_source_canvas_width(app->m, &width, NULL) == MC_CLIENT_CANVAS_OK &&
          mc_client_source_canvas_height(app->m, &height, NULL) == MC_CLIENT_CANVAS_OK);
    const unsigned int control = _mm_getcsr();
    _mm_setcsr((control & ~0x6000u) | 0x2000u);
    const mc_client_mouse_helper_status status =
        mc_client_mouse_helper_ungrab(app->C, input, &error);
    _mm_setcsr(control); /* Restore before assertions or cleanup. */
    CHECK(status == MC_CLIENT_MOUSE_HELPER_INPUT_FAILED && !error.source_returned &&
          error.site.pc == (grabbed ? 22 : 18) &&
          error.input.status == MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP &&
          error.input.site.pc == (grabbed ? 68 : 61));
    CHECK(input->phase == MC_CLIENT_SDK_INPUT_FAILED && !input->grabbed &&
          input->backend_grabbed == grabbed && input->x == width / 2 && input->y == height / 2 &&
          input->current_mouse.x == width / 2 && input->current_mouse.y == height / 2 &&
          input->last_mouse_store == (grabbed ? 11 : 35));
    CHECK(!app->N); /* No caller repair or source focus rollback. */
    printf("CANVAS FP-prefix helper-pc=%d input-pc=%d terminal-only=1\n", error.site.pc,
           error.input.site.pc);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2 || argc == 3);
    const char *profile = argc == 2 ? "hidden" : argv[2];
    const bool visible = strcmp(profile, "hidden") != 0;
    const bool fp_position = strcmp(profile, "visible-fp-position") == 0;
    const bool fp_ungrab = strcmp(profile, "visible-fp-ungrab") == 0;
    const bool reactivation = strcmp(profile, "visible-reactivation") == 0;
    CHECK(strcmp(profile, "hidden") == 0 || strcmp(profile, "visible") == 0 || fp_position ||
          fp_ungrab || reactivation);
    SDL_SetMainReady();
    const mc_client_application_options options = {.width = 854,
                                                   .height = 480,
                                                   .assets = argv[1],
                                                   .username = "CanvasCheck",
                                                   .hidden = !visible};
    mc_client_bootstrap *bootstrap = NULL;
    CHECK(mc_client_bootstrap_open_standalone(&options, &bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    client_application *app = mc_client_bootstrap_app(bootstrap);
    mc_client_source_frame *frame = mc_client_bootstrap_frame(bootstrap);
    mc_client_frame_state host;
    CHECK(app != NULL && frame != NULL && mc_client_source_frame_inspect(frame, &host));
    CHECK(app->m == host.canvas && app->m != NULL && host.app == app);
    mc_client_display_stage_state stage;
    mc_client_display_parent_state parent;
    CHECK(mc_client_display_stage_inspect(app->display, &stage) &&
          stage.initial_mode == stage.current_mode);
    CHECK(mc_client_display_stage_parent_inspect(app->display, &parent) &&
          parent.parent == app->m && parent.listener_parent == app->m && parent.borrowed_window &&
          parent.window == host.canvas_window);
    mc_client_display_mode_state mode = {0};
    const bool have_effective_mode = mc_client_display_mode_inspect(parent.effective_mode, &mode);
    if (!have_effective_mode || mode.width != 854 || mode.height != 480)
        fprintf(stderr, "Canvas effective Mode: present%d width%d height%d\n", have_effective_mode,
                mode.width, mode.height);
    CHECK(parent.effective_mode != stage.current_mode && have_effective_mode && mode.width == 854 &&
          mode.height == 480);
    CHECK(strcmp(SDL_GetWindowTitle(host.frame_window), "Minecraft") == 0 &&
          strcmp(SDL_GetWindowTitle(host.frame_window), SDL_GetWindowTitle(host.canvas_window)) !=
              0);
    mc_client_sdk_update *update = mc_client_display_update_owner(app->display);
    mc_client_sdk_update_error ue;
    if (visible) {
        CHECK(wait_focus(update, &host, true));
        int32_t caps;
        CHECK(capabilities(mc_client_display_inputs(app->display), &caps) == 0);
        if (fp_position || fp_ungrab) {
            CHECK(fp_prefix(app, fp_ungrab, caps) == 0);
            CHECK(mc_client_application_source_stop(app, NULL) == MC_CLIENT_STARTUP_OK);
            goto terminal;
        }
        CHECK(mouse_helper(app) == 0);
        CHECK(warp_after_resize(app, &host, caps) == 0);
        CHECK(focus_cycle(app, &host) == 0);
        if (reactivation)
            CHECK(real_reactivation(app, &host, caps) == 0);
    }
    /* Hidden-host native fault control: a queued focus-gained record must not
     * demand X11 viewability or Win32 foreground focus from an intentional
     * hidden parent. Exercise it while the retained event owner is active. */
    if (!visible) {
        SDL_Event focus = {0};
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        focus.window.windowID = SDL_GetWindowID(host.frame_window);
        CHECK(SDL_PushEvent(&focus));
    }
    CHECK(mc_client_sdk_display_process_messages(update, &ue) == MC_CLIENT_SDK_UPDATE_OK);
    CHECK(SDL_SetWindowSize(host.frame_window, 901, 503));
    int32_t width = 0, height = 0;
    for (unsigned i = 0; i < 100; ++i) {
        CHECK(mc_client_sdk_display_process_messages(update, &ue) == MC_CLIENT_SDK_UPDATE_OK);
        CHECK(mc_client_source_canvas_width(app->m, &width, NULL) == MC_CLIENT_CANVAS_OK);
        CHECK(mc_client_source_canvas_height(app->m, &height, NULL) == MC_CLIENT_CANVAS_OK);
        if (width == 901 && height == 503)
            break;
        SDL_Delay(2);
    }
    CHECK(width == 901 && height == 503);
    CHECK(mc_client_sdk_display_update(update, &ue) == MC_CLIENT_SDK_UPDATE_OK);
    CHECK(mc_client_display_stage_inspect(app->display, &stage) &&
          stage.initial_mode == stage.current_mode);
    mc_client_menu_loop *loop = NULL;
    CHECK(mc_client_menu_loop_create(bootstrap, &loop));
    CHECK(mc_client_menu_loop_frame(loop) && app->d == 901 && app->e == 503);
    CHECK(atomic_load(&app->J));
    const bool focused = app->N;
    CHECK(close_request(host.frame_window));
    for (unsigned i = 0; i < 100 && atomic_load(&app->J); ++i) {
        CHECK(mc_client_sdk_display_process_messages(update, &ue) == MC_CLIENT_SDK_UPDATE_OK);
        SDL_Delay(2);
    }
    CHECK(!atomic_load(&app->J) && app->N == focused);
    CHECK(mc_client_source_frame_inspect(frame, &host) && host.close_dispatched && !host.failed);
    for (unsigned i = 0; i < 35000 && loop != NULL; ++i) {
        if (!mc_client_menu_loop_terminal_retire(&loop))
            SDL_Delay(1);
    }
    CHECK(loop == NULL);
terminal:
    for (unsigned i = 0; i < 35000 && bootstrap != NULL; ++i) {
        if (mc_client_bootstrap_close(&bootstrap) != MC_CLIENT_BOOTSTRAP_OK)
            SDL_Delay(1);
    }
    CHECK(bootstrap == NULL);
    return 0;
}
