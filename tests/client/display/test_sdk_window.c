#include "events.h"
#include "mc/resources/files/files.h"
#include "sdk_queue.h"
#include "sdk_update.h"
#include "staged.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <xmmintrin.h>

typedef struct callback_probe {
    mc_client_display **display;
    mc_client_sdk_update *updates;
    mc_client_sdk_input *input;
    Uint32 type;
    size_t callbacks, busy;
    bool enabled, registered, failed, change_fp;
} callback_probe;

static callback_probe probe;

static bool SDLCALL event_watch(void *context, SDL_Event *event) {
    callback_probe *p = context;
    if (!p->enabled || event->type != p->type)
        return true;
    ++p->callbacks;
    mc_client_display *same = *p->display;
    mc_client_display_stage_error display_error;
    mc_client_sdk_update_error update_error;
    mc_client_sdk_input_error input_error;
    mc_client_sdk_input *input_slot = p->input;
    bool next = true;
    mc_client_sdk_mouse_event mouse = {.x = 123};
    const bool busy =
        !mc_client_display_stage_retire(p->display, &display_error) && *p->display == same &&
        display_error.display.status == MC_CLIENT_DISPLAY_BUSY &&
        mc_client_sdk_display_update(p->updates, &update_error) == MC_CLIENT_SDK_UPDATE_BUSY &&
        !update_error.swapped &&
        mc_client_sdk_input_retire(&input_slot, &input_error) == MC_CLIENT_SDK_INPUT_BUSY &&
        input_slot == p->input &&
        mc_client_sdk_mouse_next(p->input, &next, &mouse, &input_error) ==
            MC_CLIENT_SDK_INPUT_BUSY &&
        next && mouse.x == 123;
    if (busy)
        ++p->busy;
    else
        p->failed = true;
    if (p->change_fp)
        _mm_setcsr(_mm_getcsr() | 0x8000u);
    return true;
}

static void actual_pump_then_watch_event(void) {
    SDL_PumpEvents();
    if (probe.enabled) {
        SDL_Event event = {0};
        event.type = probe.type;
        event.user.windowID = SDL_GetWindowID(mc_client_display_window(*probe.display));
        if (!SDL_PushEvent(&event))
            probe.failed = true;
    }
}

#ifdef _WIN32
#include <windows.h>

static bool real_icon(HWND window, WPARAM slot, LONG edge) {
    HICON icon = (HICON)SendMessageW(window, WM_GETICON, slot, 0);
    ICONINFO info = {0};
    if (icon == NULL || !GetIconInfo(icon, &info))
        return false;
    BITMAP bitmap = {0};
    const bool ok = info.fIcon && info.hbmColor != NULL &&
                    GetObjectW(info.hbmColor, (int)sizeof(bitmap), &bitmap) != 0 &&
                    bitmap.bmWidth == edge && bitmap.bmHeight == edge;
    if (info.hbmColor != NULL)
        (void)DeleteObject(info.hbmColor);
    if (info.hbmMask != NULL)
        (void)DeleteObject(info.hbmMask);
    return ok;
}
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static bool real_icon(Display *display, Window window) {
    XWMHints *hints = XGetWMHints(display, window);
    if (hints == NULL)
        return false;
    Window root;
    int x, y;
    unsigned width, height, border, depth;
    bool ok = (hints->flags & (IconPixmapHint | IconMaskHint)) == (IconPixmapHint | IconMaskHint);
    if (ok)
        ok = XGetGeometry(display, hints->icon_pixmap, &root, &x, &y, &width, &height, &border,
                          &depth) &&
             width == 32 && height == 32 && depth == 24;
    if (ok)
        ok = XGetGeometry(display, hints->icon_mask, &root, &x, &y, &width, &height, &border,
                          &depth) &&
             width == 32 && height == 32 && depth == 1;
    (void)XFree(hints);
    return ok;
}
#endif

static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_client_sdk_icons *icons = NULL;
    mc_client_display *display = NULL, *missing = NULL;
    mc_client_display_mode *mode = NULL, *unbounded = NULL;
    mc_player_string *title = NULL;
    mc_client_sdk_icons_error icons_error;
    mc_client_display_stage_error error;
    mc_client_sdk_input_error input_error;
    mc_client_sdk_update_error update_error;
    mc_client_display_stage_state stage;
    mc_client_display_mode_state mode_state;
    mc_client_sdk_input_state input_state;
    mc_client_sdk_bytes_state bytes;
    CHECK(argc == 2);
    CHECK(mc_client_display_mode_allocate(&unbounded, &error));
    CHECK(mc_client_display_mode_construct(unbounded, -5, 0, &error));
    CHECK(mc_client_display_mode_inspect(unbounded, &mode_state) && mode_state.width == -5 &&
          mode_state.height == 0);
    CHECK(mc_client_display_stage_allocate(&missing, &error));
    CHECK(!mc_client_display_stage_set_mode(missing, unbounded, &error) &&
          error.kind == MC_CLIENT_DISPLAY_NATIVE_POLICY);
    CHECK(mc_client_display_stage_inspect(missing, &stage) && !stage.class_initialized &&
          !stage.window_created);
    CHECK(mc_client_display_stage_retire(&missing, &error) && missing == NULL);
    CHECK(mc_resource_files_create(argv[1], &files) == MC_REGISTRY_OK);
    CHECK(mc_client_sdk_icons_allocate(mc_resource_files_source(files), &icons, &icons_error) ==
          MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_client_sdk_icons_construct(icons, &icons_error) == MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_client_display_stage_allocate(&display, &error));
    CHECK(mc_client_display_stage_bind_icons(display, icons, &error));
    CHECK(mc_client_display_stage_set_mode(display, unbounded, &error));
    CHECK(mc_client_display_stage_inspect(display, &stage) && stage.current_mode == unbounded &&
          !stage.window_created);
    mc_client_sdk_update *updates = mc_client_display_update_owner(display);
    bool pending_close = true;
    CHECK(mc_client_sdk_display_close_requested(updates, &pending_close, &update_error) ==
          MC_CLIENT_SDK_UPDATE_SOURCE_ILLEGAL_STATE);
    CHECK(update_error.site.pc == 12 && pending_close);
    int32_t width = -1;
    CHECK(mc_client_sdk_display_width(updates, &width, &update_error) == MC_CLIENT_SDK_UPDATE_OK &&
          width == 0);
    CHECK(mc_client_display_mode_allocate(&mode, &error));
    CHECK(mc_client_display_mode_construct(mode, 160, 120, &error));
    CHECK(mc_client_display_stage_set_mode(display, mode, &error));
    CHECK(mc_client_display_stage_set_title(display, NULL, &error));
    CHECK(mc_client_display_stage_inspect(display, &stage) && stage.title != NULL);
    mc_player_string_view text;
    CHECK(mc_player_string_read(stage.title, &text) && text.length == 0);
    const uint16_t units[] = {'S', 'D', 'K', ' ', 'o', 'w', 'n', 'e', 'r'};
    CHECK(mc_player_string_create(units, sizeof(units) / sizeof(units[0]), &title) == MC_PLAYER_OK);
    CHECK(mc_client_display_stage_set_title(display, title, &error));
    CHECK(mc_client_display_stage_inspect(display, &stage) && stage.title == title);
    CHECK(mc_client_display_create_staged(display, true, &error));
    CHECK(mc_client_display_stage_inspect(display, &stage) &&
          stage.phase == MC_CLIENT_DISPLAY_READY && stage.window_created);
    mc_client_sdk_input *input = mc_client_display_inputs(display);
    CHECK(mc_client_sdk_input_inspect(input, &input_state) &&
          input_state.phase == MC_CLIENT_SDK_INPUT_READY);
    CHECK(input_state.mouse_created && input_state.keyboard_created &&
          !input_state.controllers_created);
    CHECK(input_state.coordinates != NULL && input_state.button_names_count == 16 &&
          input_state.key_names_count == 131);
    CHECK(mc_client_sdk_bytes_inspect(input_state.mouse_read, &bytes) && bytes.capacity == 1100);
    CHECK(mc_client_sdk_bytes_inspect(input_state.keyboard_read, &bytes) && bytes.capacity == 900);
    probe = (callback_probe){.display = &display,
                             .updates = updates,
                             .input = input,
                             .type = SDL_RegisterEvents(1),
                             .enabled = true};
    CHECK(probe.type != 0 && SDL_AddEventWatch(event_watch, &probe));
    probe.registered = true;
    CHECK(mc_client_sdk_display_process_messages(updates, &update_error) ==
          MC_CLIENT_SDK_UPDATE_OK);
    bool next = false;
    mc_client_sdk_mouse_event mouse;
    mc_client_sdk_key_event key;
    do {
        CHECK(mc_client_sdk_mouse_next(input, &next, &mouse, &input_error) ==
              MC_CLIENT_SDK_INPUT_OK);
    } while (next);
    do {
        CHECK(mc_client_sdk_keyboard_next(input, &next, &key, &input_error) ==
              MC_CLIENT_SDK_INPUT_OK);
    } while (next);
    CHECK(mc_client_sdk_input_inspect(input, &input_state));
    const mc_client_sdk_queue *previous_queue = input_state.mouse_events;
    SDL_Window *window = mc_client_display_window(display);
    const SDL_WindowID id = SDL_GetWindowID(window);
#ifdef _WIN32
    HWND native = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                         SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    CHECK(native != NULL && real_icon(native, ICON_BIG, 32) && real_icon(native, ICON_SMALL, 16));
#elif defined(__linux__)
    Display *native = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                             SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    const Window native_window = (Window)SDL_GetNumberProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    CHECK(native != NULL && native_window != None && real_icon(native, native_window));
#endif
    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = id;
    event.key.scancode = SDL_SCANCODE_A;
    CHECK(SDL_PushEvent(&event));
    for (int32_t i = 0; i < 200; ++i) {
        event = (SDL_Event){0};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.windowID = id;
        event.motion.x = (float)i;
        event.motion.y = 10.0f;
        event.motion.timestamp = 1000u + (uint64_t)i;
        CHECK(SDL_PushEvent(&event));
    }
    CHECK(mc_client_sdk_mouse_set_grabbed(input, false, &input_error) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(mc_client_sdk_input_inspect(input, &input_state));
    CHECK(input_state.mouse_events == previous_queue); /* same false: Linux does not replace */
    CHECK(mc_client_sdk_queue_inspect(input_state.mouse_events, &bytes));
    CHECK(bytes.position == (input_state.platform.lwjgl_platform == 1 ? 150u * 22u : 0u));
    bool held = true;
    CHECK(mc_client_sdk_keyboard_key_down(input, 30, &held, &input_error) ==
              MC_CLIENT_SDK_INPUT_OK &&
          !held);
    CHECK(mc_client_sdk_display_process_messages(updates, &update_error) ==
          MC_CLIENT_SDK_UPDATE_OK);
    CHECK(mc_client_sdk_keyboard_key_down(input, 30, &held, &input_error) ==
              MC_CLIENT_SDK_INPUT_OK &&
          held);
    CHECK(mc_client_sdk_keyboard_next(input, &next, &key, &input_error) == MC_CLIENT_SDK_INPUT_OK &&
          next);
    CHECK(key.key == 30 && key.pressed && key.character == 0 && !key.repeat);
    held = true;
    CHECK(mc_client_sdk_keyboard_key_down(input, -1, &held, &input_error) ==
              MC_CLIENT_SDK_INPUT_SOURCE_INDEX &&
          held);
    CHECK(input_error.site.pc == 26);
    event = (SDL_Event){0};
    event.type = SDL_EVENT_WINDOW_EXPOSED;
    event.window.windowID = id;
    CHECK(SDL_PushEvent(&event));
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    CHECK(SDL_PushEvent(&event));
    CHECK(mc_client_sdk_display_process_messages(updates, &update_error) ==
          MC_CLIENT_SDK_UPDATE_OK);
    CHECK(mc_client_sdk_display_close_requested(updates, &pending_close, &update_error) ==
              MC_CLIENT_SDK_UPDATE_OK &&
          pending_close);
    CHECK(mc_client_sdk_display_close_requested(updates, &pending_close, &update_error) ==
              MC_CLIENT_SDK_UPDATE_OK &&
          !pending_close);
    mc_client_events_state before, after;
    CHECK(mc_client_events_inspect(mc_client_display_event_port(display), &before) && before.dirty);
    CHECK((SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) == 0);
    CHECK(mc_client_sdk_display_update_messages(updates, false, &update_error) ==
          MC_CLIENT_SDK_UPDATE_OK);
    CHECK(update_error.swapped && !update_error.messages_pumped);
    CHECK(mc_client_events_inspect(mc_client_display_event_port(display), &after) && after.dirty &&
          after.pump_generation == before.pump_generation);
    CHECK(mc_client_sdk_display_update(updates, &update_error) == MC_CLIENT_SDK_UPDATE_OK &&
          update_error.messages_pumped);
    CHECK(probe.callbacks != 0 && probe.busy == probe.callbacks && !probe.failed);
    const unsigned int before_fp = _mm_getcsr();
    probe.change_fp = true;
    const mc_client_sdk_update_status fp_status =
        mc_client_sdk_display_update(updates, &update_error);
    const unsigned int after_fp = _mm_getcsr();
    _mm_setcsr(before_fp); /* only the authored controller restores its own state */
    CHECK(fp_status == MC_CLIENT_SDK_UPDATE_INPUT_FAILED && update_error.swapped &&
          update_error.messages_pumped);
    CHECK((after_fp & 0xffc0u) != 0x1f80u &&
          update_error.input.status == MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP);
    bool reached_poll = update_error.input.site.pc == 31;
    for (size_t i = 0; i < update_error.input.caller_count; ++i)
        if (update_error.input.callers[i].pc == 31)
            reached_poll = true;
    CHECK(reached_poll && !probe.failed);
    CHECK(mc_client_sdk_display_update(updates, &update_error) ==
              MC_CLIENT_SDK_UPDATE_INVALID_PHASE &&
          !update_error.swapped);
    probe.enabled = false;
    SDL_RemoveEventWatch(event_watch, &probe);
    probe.registered = false;
    CHECK(mc_client_display_stage_retire(&display, &error) && display == NULL);
    result = 0;
cleanup:
    probe.enabled = false;
    if (probe.registered) {
        SDL_RemoveEventWatch(event_watch, &probe);
        probe.registered = false;
    }
    if (display != NULL && !mc_client_display_stage_retire(&display, &error))
        result = 1;
    if (missing != NULL && !mc_client_display_stage_retire(&missing, &error))
        result = 1;
    mc_player_string_release(title);
    mc_client_display_mode_release(&mode);
    mc_client_display_mode_release(&unbounded);
    if (icons != NULL &&
        mc_client_sdk_icons_release(&icons, &icons_error) != MC_CLIENT_SDK_ICONS_OK)
        result = 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    if (result == 0)
        printf("SDK_WINDOW_OK|checks=%u\n", checks);
    return result;
}

/* Native controller seam: include the EXACT shared transport TU once and
 * forward its real SDL pump. A real SDL_PushEvent then invokes the real event
 * watch while the actual update/transport guards are live. No runtime hook,
 * replacement state, successful stub or fake created flag is introduced.
 * The link map must prove archive events.o is not also extracted. */
#define SDL_PumpEvents actual_pump_then_watch_event
#include "../../../src/client/display/events.c"
#undef SDL_PumpEvents
