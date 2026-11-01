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
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>
#ifdef _WIN32
#include <windows.h>
#else
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
    CHECK(strcmp(profile, "hidden") == 0 || strcmp(profile, "visible") == 0 || fp_position ||
          fp_ungrab);
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
