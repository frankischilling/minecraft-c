#include "mc/core/java/java.h"
#include "sdk_display_internal.h"
#include "sdk_input_internal.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static int32_t clip(int32_t value, int32_t extent) {
    const int32_t lower = value > 0 ? value : 0;
    const int32_t upper = mc_java_i32_sub(extent, 1);
    return lower < upper ? lower : upper;
}

bool mc_client_sdk_mouse_poll_body(mc_client_sdk_input *o, bool collect,
                                   mc_client_sdk_input_error *e) {
    int native_width = 0, native_height = 0;
    if (!SDL_GetWindowSize(o->window, &native_width, &native_height)) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                 "SDK actual backend window coordinates", "org/lwjgl/input/Mouse",
                                 "poll", "()V", 31, true);
        return false;
    }
    o->width = native_width;
    o->height = native_height;
    if (collect && !mc_client_sdk_input_collect_mouse(o, e)) {
        mc_client_sdk_input_caller(e, "org/lwjgl/input/Mouse", "poll", "()V", 31);
        return false;
    }
    float fx = 0, fy = 0;
    const SDL_MouseButtonFlags mask = SDL_GetMouseState(&fx, &fy);
    (void)mask; /* The concrete backend owns its processed-window button cache. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP,
                                 "SDK reached actual SDL position conversion",
                                 "org/lwjgl/input/Mouse", "poll", "()V", 31, true);
        return false;
    }
    if (SDL_GetWindowRelativeMouseMode(o->window) != o->backend_grabbed) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                 "SDK foreign native grab-mode mutation", "org/lwjgl/input/Mouse",
                                 "poll", "()V", 31, true);
        return false;
    }
    int32_t cx = o->absolute_x, cy = o->absolute_y;
    const bool focused = SDL_GetMouseFocus() == o->window;
    if (o->grabbed) {
        /* The concrete backend accumulator comes from the same retained SDL
         * motion occurrences as its EventQueue. SDL's own relative cache can
         * be flushed by a grab request and is not a second input owner. */
        cx = o->native_dx;
        cy = o->native_dy;
    } else if (focused) {
        cx = mc_java_float_to_i32(fx);
        cy = mc_java_i32_sub(mc_java_i32_sub(native_height, 1), mc_java_float_to_i32(fy));
    }
    /* Both concrete SDK polls reset their relative accumulators regardless
     * of whether this call returned absolute or grabbed coordinates. */
    o->native_dx = o->native_dy = 0;
    o->native_x = cx;
    o->native_y = cy;
    /* SDL's cache is window-relative to its current focus. Never forward some
     * other window's position/buttons as this actual InputImplementation. */
    for (int32_t i = 0; i < o->platform.button_count; ++i) {
        if (!mc_client_sdk_bytes_set(o->buttons, (size_t)i, o->backend_buttons[i])) {
            mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_LIMIT,
                                     "SDK actual button ByteBuffer extent", "org/lwjgl/input/Mouse",
                                     "poll", "()V", 31, true);
            return false;
        }
    }
    const int32_t dwheel = o->native_wheel;
    o->native_wheel = 0;
    o->coordinates[0] = cx;
    o->coordinates[1] = cy;
    o->coordinates[2] = dwheel;
    if (o->grabbed) {
        /* Relative mode is admitted only through the real setGrabbed body;
         * its backend accumulator is supplied by retained SDL motion records. */
        o->dx = mc_java_i32_add(o->dx, cx);
        o->last_mouse_store = 71;
        o->dy = mc_java_i32_add(o->dy, cy);
        o->last_mouse_store = 79;
        o->x = mc_java_i32_add(o->x, cx);
        o->last_mouse_store = 87;
        o->y = mc_java_i32_add(o->y, cy);
        o->last_mouse_store = 95;
        o->absolute_x = o->x;
        o->last_mouse_store = 103;
        o->absolute_y = o->y;
        o->last_mouse_store = 111;
    } else {
        o->dx = mc_java_i32_sub(cx, o->absolute_x);
        o->last_mouse_store = 122;
        o->dy = mc_java_i32_sub(cy, o->absolute_y);
        o->last_mouse_store = 130;
        o->x = cx;
        o->last_mouse_store = 135;
        o->absolute_x = cx;
        o->last_mouse_store = 138;
        o->y = cy;
        o->last_mouse_store = 143;
        o->absolute_y = cy;
        o->last_mouse_store = 146;
    }
    if (o->clip_coordinates) {
        int32_t width, height;
        if (!mc_client_display_sdk_source_extent(o->display, true, &width)) {
            mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_INVALID_PHASE,
                                     "SDK actual source Display extent owner",
                                     "org/lwjgl/input/Mouse", "poll", "()V", 155, true);
            return false;
        }
        o->x = clip(o->x, width);
        o->last_mouse_store = 170;
        if (!mc_client_display_sdk_source_extent(o->display, false, &height)) {
            mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_INVALID_PHASE,
                                     "SDK actual source Display height owner",
                                     "org/lwjgl/input/Mouse", "poll", "()V", 173, true);
            return false;
        }
        o->y = clip(o->y, height);
        o->last_mouse_store = 188;
    }
    o->wheel = mc_java_i32_add(o->wheel, dwheel);
    o->last_mouse_store = 196;
    if (!mc_client_sdk_input_read_queue(o, true, e)) {
        mc_client_sdk_input_caller(e, "org/lwjgl/input/Mouse", "poll", "()V", 199);
        return false;
    }
    return true;
}

mc_client_sdk_input_status mc_client_sdk_mouse_poll(mc_client_sdk_input *o,
                                                    mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (!mc_client_sdk_input_enter(o, "org/lwjgl/input/Mouse", "poll", "()V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (!o->mouse_created)
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_SOURCE_ILLEGAL_STATE,
                                 "actual uncreated Mouse.poll", "org/lwjgl/input/Mouse", "poll",
                                 "()V", 12, false);
    else
        (void)mc_client_sdk_mouse_poll_body(o, true, &e);
    return mc_client_sdk_input_leave(o, &e, out);
}

mc_client_sdk_input_status mc_client_sdk_keyboard_poll(mc_client_sdk_input *o,
                                                       mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (!mc_client_sdk_input_enter(o, "org/lwjgl/input/Keyboard", "poll", "()V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (!o->keyboard_created) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_SOURCE_ILLEGAL_STATE,
                                 "actual uncreated Keyboard.poll", "org/lwjgl/input/Keyboard",
                                 "poll", "()V", 12, false);
        return mc_client_sdk_input_leave(o, &e, out);
    }
    if (!mc_client_sdk_input_collect_keyboard(o, &e)) {
        mc_client_sdk_input_caller(&e, "org/lwjgl/input/Keyboard", "poll", "()V", 28);
        return mc_client_sdk_input_leave(o, &e, out);
    }
    int count = 0;
    const bool *keys = SDL_GetKeyboardState(&count);
    if (keys == NULL || count <= 0) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                 "actual SDL keyboard state attachment", "org/lwjgl/input/Keyboard",
                                 "poll", "()V", 28, true);
        return mc_client_sdk_input_leave(o, &e, out);
    }
    /* Like the original concrete backends, the actual InputImplementation
     * owns its down-state cache populated by processed window input records.
     * SDL's independently queried cache is observed for attachment validity;
     * SDL_PushEvent does not mutate that physical cache. */
    for (size_t i = 0; i < 256; ++i)
        (void)mc_client_sdk_bytes_set(o->key_down, i, o->backend_keys[i]);
    if (!mc_client_sdk_input_read_queue(o, false, &e))
        mc_client_sdk_input_caller(&e, "org/lwjgl/input/Keyboard", "poll", "()V", 33);
    return mc_client_sdk_input_leave(o, &e, out);
}

mc_client_sdk_input_status mc_client_sdk_mouse_update_cursor(mc_client_sdk_input *o,
                                                             mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (!mc_client_sdk_input_enter(o, "org/lwjgl/input/Mouse", "updateCursor", "()V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    /* Default currentCursor is a real NULL field, rather than an always-OK
     * replacement of the nonnull animation branch. */
    if (o->emulate_cursor_animation && o->current_cursor != NULL)
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                 "SDK reached explicit Cursor animation", "org/lwjgl/input/Mouse",
                                 "updateCursor", "()V", 21, true);
    return mc_client_sdk_input_leave(o, &e, out);
}

mc_client_sdk_input_status mc_client_sdk_mouse_set_grabbed(mc_client_sdk_input *o, bool grabbed,
                                                           mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (!mc_client_sdk_input_enter(o, "org/lwjgl/input/Mouse", "setGrabbed", "(Z)V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    const bool old = o->grabbed;
    o->grabbed = grabbed;
    o->last_mouse_store = 11;
    if (o->mouse_created) {
        if (grabbed && !old) {
            o->grab_x = o->x;
            o->last_mouse_store = 31;
            o->grab_y = o->y;
            o->last_mouse_store = 37;
        }
        /* Preserve SDL records before its documented relative-mode motion
         * flush. Translate mouse records using the OLD concrete backend mode;
         * keyboard/window occurrences remain in their shared transport. */
        const mc_client_events_status retained =
            mc_client_events_retain_pending(o->events, &e.events);
        if (retained != MC_CLIENT_EVENTS_OK || !mc_client_sdk_input_collect_mouse(o, &e)) {
            if (e.status == MC_CLIENT_SDK_INPUT_OK)
                mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                         "SDK retain before native grab", "org/lwjgl/input/Mouse",
                                         "setGrabbed", "(Z)V", 77, true);
            else
                mc_client_sdk_input_caller(&e, "org/lwjgl/input/Mouse", "setGrabbed", "(Z)V", 77);
            return mc_client_sdk_input_leave(o, &e, out);
        }
        if (!grabbed && old) {
            /* The admitted actual Windows/X11 SDL attachment has real cursor
             * warp support. This is the source capability-bit1 path at51/68;
             * a different native platform was rejected during construction. */
            if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
                mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP,
                                         "SDK reached real SDL warp coordinate conversion",
                                         "org/lwjgl/input/Mouse", "setGrabbed", "(Z)V", 68, true);
                return mc_client_sdk_input_leave(o, &e, out);
            }
            SDL_WarpMouseInWindow(o->window, (float)o->grab_x,
                                  (float)mc_java_i32_sub(mc_java_i32_sub(o->height, 1), o->grab_y));
        }
        const bool changed = o->backend_grabbed != grabbed;
        mc_client_sdk_queue *replacement = NULL;
        if (o->platform.lwjgl_platform == 1 && changed &&
            mc_client_sdk_queue_create(22, &replacement, &e.queue) != MC_CLIENT_SDK_QUEUE_OK) {
            mc_client_sdk_input_fail(&e,
                                     e.queue.status == MC_CLIENT_SDK_QUEUE_ALLOCATION_FAILED
                                         ? MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED
                                         : MC_CLIENT_SDK_INPUT_LIMIT,
                                     "actual LinuxMouse.reset replacement queue",
                                     "org/lwjgl/input/Mouse", "setGrabbed", "(Z)V", 77, true);
            return mc_client_sdk_input_leave(o, &e, out);
        }
        if (!SDL_SetWindowRelativeMouseMode(o->window, grabbed)) {
            mc_client_sdk_queue_free(&replacement);
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                     "actual SDL grabMouse request", "org/lwjgl/input/Mouse",
                                     "setGrabbed", "(Z)V", 77, true);
        } else {
            o->backend_grabbed = grabbed;
            /* WindowsMouse.grab clears its EventQueue on every call (79).
             * LinuxMouse.changeGrabbed/reset replaces that queue and zeros
             * relative accumulators only when the native grab changes. */
            if (o->platform.lwjgl_platform == 3)
                mc_client_sdk_queue_clear(o->mouse_queue);
            if (replacement != NULL) {
                mc_client_sdk_queue_free(&o->mouse_queue);
                o->mouse_queue = replacement;
            }
            if (o->platform.lwjgl_platform == 1 && changed)
                o->native_dx = o->native_dy = 0;
            if (mc_client_sdk_mouse_poll_body(o, true, &e)) {
                o->current_mouse.x = o->x;
                o->last_mouse_store = 88;
                o->current_mouse.y = o->y;
                o->last_mouse_store = 94;
                o->event_raw_x = o->x;
                o->last_mouse_store = 100;
                o->event_raw_y = o->y;
                o->last_mouse_store = 106;
                o->wheel = o->dy = o->dx = 0; /* actual resetMouse2/6/9 */
                mc_client_sdk_bytes_state state;
                (void)mc_client_sdk_bytes_inspect(o->mouse_read, &state);
                (void)mc_client_sdk_bytes_position(o->mouse_read, state.limit); /* resetMouse21 */
            } else
                mc_client_sdk_input_caller(&e, "org/lwjgl/input/Mouse", "setGrabbed", "(Z)V", 82);
        }
    }
    return mc_client_sdk_input_leave(o, &e, out);
}

mc_client_sdk_input_status mc_client_sdk_controllers_is_created(mc_client_sdk_input *o,
                                                                bool *result,
                                                                mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (result == NULL ||
        !mc_client_sdk_input_enter(o, "org/lwjgl/input/Controllers", "isCreated", "()Z", &e)) {
        if (result == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "Controllers native output", "org/lwjgl/input/Controllers",
                                     "isCreated", "()Z", -1, true);
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (o->controllers == NULL) {
        o->controllers = calloc(1, sizeof(*o->controllers));
        if (o->controllers == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                     "actual Controllers first ArrayList",
                                     "org/lwjgl/input/Controllers", "<clinit>", "()V", 0, true);
    }
    if (e.status == MC_CLIENT_SDK_INPUT_OK && o->controller_events == NULL) {
        o->controller_events = calloc(1, sizeof(*o->controller_events));
        if (o->controller_events == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                     "actual Controllers second ArrayList",
                                     "org/lwjgl/input/Controllers", "<clinit>", "()V", 10, true);
    }
    if (e.status == MC_CLIENT_SDK_INPUT_OK)
        *result = o->controllers_created;
    return mc_client_sdk_input_leave(o, &e, out);
}

static uint32_t u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int32_t i32(const uint8_t *p) {
    const uint32_t bits = u32(p);
    int32_t out;
    memcpy(&out, &bits, sizeof(out));
    return out;
}

static int64_t i64(const uint8_t *p) {
    const uint64_t bits = ((uint64_t)u32(p) << 32) | u32(p + 4);
    int64_t out;
    memcpy(&out, &bits, sizeof(out));
    return out;
}

mc_client_sdk_input_status mc_client_sdk_mouse_next(mc_client_sdk_input *o, bool *result,
                                                    mc_client_sdk_mouse_event *event,
                                                    mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (result == NULL || event == NULL ||
        !mc_client_sdk_input_enter(o, "org/lwjgl/input/Mouse", "next", "()Z", &e)) {
        if (result == NULL || event == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "Mouse.next native outputs", "org/lwjgl/input/Mouse", "next",
                                     "()Z", -1, true);
        if (out != NULL)
            *out = e;
        return e.status;
    }
    const size_t remaining = mc_client_sdk_bytes_remaining(o->mouse_read);
    if (remaining == 0)
        *result = false;
    else if (remaining < 22) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                 "native Mouse frame invariant", "org/lwjgl/input/Mouse", "next",
                                 "()Z", 34, true);
    } else {
        uint8_t raw[22];
        (void)mc_client_sdk_bytes_read(o->mouse_read, raw, sizeof(raw));
        o->current_mouse.button = raw[0] < 128 ? raw[0] : (int32_t)raw[0] - 256;
        o->current_mouse.pressed = raw[1] != 0;
        const int32_t x = i32(raw + 2), y = i32(raw + 6);
        if (o->grabbed) {
            o->current_mouse.dx = x;
            o->current_mouse.dy = y;
            o->current_mouse.x = mc_java_i32_add(o->current_mouse.x, x);
            o->current_mouse.y = mc_java_i32_add(o->current_mouse.y, y);
        } else {
            o->current_mouse.dx = mc_java_i32_sub(x, o->event_raw_x);
            o->current_mouse.dy = mc_java_i32_sub(y, o->event_raw_y);
            o->current_mouse.x = x;
            o->current_mouse.y = y;
        }
        o->event_raw_x = o->current_mouse.x;
        o->event_raw_y = o->current_mouse.y;
        if (o->clip_coordinates) {
            int32_t width, height;
            if (!mc_client_display_sdk_source_extent(o->display, true, &width)) {
                mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_PHASE,
                                         "SDK actual source Display event extent",
                                         "org/lwjgl/input/Mouse", "next", "()Z", 168, true);
                return mc_client_sdk_input_leave(o, &e, out);
            }
            o->current_mouse.x = clip(o->current_mouse.x, width);
            if (!mc_client_display_sdk_source_extent(o->display, false, &height)) {
                mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_PHASE,
                                         "SDK actual source Display event height",
                                         "org/lwjgl/input/Mouse", "next", "()Z", 186, true);
                return mc_client_sdk_input_leave(o, &e, out);
            }
            o->current_mouse.y = clip(o->current_mouse.y, height);
        }
        o->current_mouse.wheel = i32(raw + 10);
        o->current_mouse.nanos = i64(raw + 14);
        *event = o->current_mouse;
        *result = true;
    }
    return mc_client_sdk_input_leave(o, &e, out);
}

mc_client_sdk_input_status mc_client_sdk_keyboard_next(mc_client_sdk_input *o, bool *result,
                                                       mc_client_sdk_key_event *event,
                                                       mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (result == NULL || event == NULL ||
        !mc_client_sdk_input_enter(o, "org/lwjgl/input/Keyboard", "next", "()Z", &e)) {
        if (result == NULL || event == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "Keyboard.next native outputs", "org/lwjgl/input/Keyboard",
                                     "next", "()Z", -1, true);
        if (out != NULL)
            *out = e;
        return e.status;
    }
    *result = false;
    while (mc_client_sdk_bytes_remaining(o->keyboard_read) != 0) {
        if (mc_client_sdk_bytes_remaining(o->keyboard_read) < 18) {
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                     "native Keyboard frame invariant", "org/lwjgl/input/Keyboard",
                                     "readNext", "(Lorg/lwjgl/input/Keyboard$KeyEvent;)Z", 13,
                                     true);
            break;
        }
        uint8_t raw[18];
        (void)mc_client_sdk_bytes_read(o->keyboard_read, raw, sizeof(raw));
        o->current_key->key = (int32_t)(u32(raw) & 255u);
        o->current_key->pressed = raw[4] != 0;
        o->current_key->character = u32(raw + 5);
        o->current_key->nanos = i64(raw + 9);
        o->current_key->repeat = raw[17] == 1;
        if (o->current_key->repeat && !o->repeat_enabled)
            continue;
        *event = *o->current_key;
        *result = true;
        break;
    }
    return mc_client_sdk_input_leave(o, &e, out);
}

static mc_client_sdk_input_status take_delta(mc_client_sdk_input *o, int32_t *out,
                                             mc_client_sdk_input_error *error, int which) {
    mc_client_sdk_input_error e = {0};
    const char *method = which == 0 ? "getDX" : which == 1 ? "getDY" : "getDWheel";
    if (out == NULL || !mc_client_sdk_input_enter(o, "org/lwjgl/input/Mouse", method, "()I", &e)) {
        if (out == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "Mouse delta native output", "org/lwjgl/input/Mouse", method,
                                     "()I", -1, true);
        if (error != NULL)
            *error = e;
        return e.status;
    }
    int32_t *slot = which == 0 ? &o->dx : which == 1 ? &o->dy : &o->wheel;
    *out = *slot;
    *slot = 0;
    return mc_client_sdk_input_leave(o, &e, error);
}

mc_client_sdk_input_status mc_client_sdk_mouse_take_dx(mc_client_sdk_input *o, int32_t *out,
                                                       mc_client_sdk_input_error *e) {
    return take_delta(o, out, e, 0);
}

mc_client_sdk_input_status mc_client_sdk_mouse_take_dy(mc_client_sdk_input *o, int32_t *out,
                                                       mc_client_sdk_input_error *e) {
    return take_delta(o, out, e, 1);
}

mc_client_sdk_input_status mc_client_sdk_mouse_take_wheel(mc_client_sdk_input *o, int32_t *out,
                                                          mc_client_sdk_input_error *e) {
    return take_delta(o, out, e, 2);
}

static mc_client_sdk_input_status mouse_integer(mc_client_sdk_input *o, int32_t *out,
                                                mc_client_sdk_input_error *error, int which) {
    mc_client_sdk_input_error e = {0};
    const char *method = which == 0 ? "getX" : which == 1 ? "getY" : "getButtonCount";
    if (out == NULL || !mc_client_sdk_input_enter(o, "org/lwjgl/input/Mouse", method, "()I", &e)) {
        if (out == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "Mouse native integer output", "org/lwjgl/input/Mouse", method,
                                     "()I", -1, true);
        if (error != NULL)
            *error = e;
        return e.status;
    }
    *out = which == 0 ? o->x : which == 1 ? o->y : o->button_count;
    return mc_client_sdk_input_leave(o, &e, error);
}

mc_client_sdk_input_status mc_client_sdk_mouse_x(mc_client_sdk_input *o, int32_t *out,
                                                 mc_client_sdk_input_error *e) {
    return mouse_integer(o, out, e, 0);
}

mc_client_sdk_input_status mc_client_sdk_mouse_y(mc_client_sdk_input *o, int32_t *out,
                                                 mc_client_sdk_input_error *e) {
    return mouse_integer(o, out, e, 1);
}

mc_client_sdk_input_status mc_client_sdk_mouse_button_count(mc_client_sdk_input *o, int32_t *out,
                                                            mc_client_sdk_input_error *e) {
    return mouse_integer(o, out, e, 2);
}

mc_client_sdk_input_status mc_client_sdk_mouse_has_wheel(mc_client_sdk_input *o, bool *out,
                                                         mc_client_sdk_input_error *error) {
    mc_client_sdk_input_error e = {0};
    if (out == NULL ||
        !mc_client_sdk_input_enter(o, "org/lwjgl/input/Mouse", "hasWheel", "()Z", &e)) {
        if (out == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "Mouse native boolean output", "org/lwjgl/input/Mouse",
                                     "hasWheel", "()Z", -1, true);
        if (error != NULL)
            *error = e;
        return e.status;
    }
    *out = o->has_wheel;
    return mc_client_sdk_input_leave(o, &e, error);
}

static mc_client_sdk_input_status down(mc_client_sdk_input *o, int32_t index, bool *out,
                                       mc_client_sdk_input_error *error, bool mouse) {
    mc_client_sdk_input_error e = {0};
    const char *class_name = mouse ? "org/lwjgl/input/Mouse" : "org/lwjgl/input/Keyboard";
    const char *method = mouse ? "isButtonDown" : "isKeyDown";
    if (out == NULL || !mc_client_sdk_input_enter(o, class_name, method, "(I)Z", &e)) {
        if (out == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "SDK native state output", class_name, method, "(I)Z", -1,
                                     true);
        if (error != NULL)
            *error = e;
        return e.status;
    }
    if (!(mouse ? o->mouse_created : o->keyboard_created))
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_SOURCE_ILLEGAL_STATE,
                                 "actual uncreated state query", class_name, method, "(I)Z", 12,
                                 false);
    else if (mouse && (index < 0 || index >= o->button_count))
        *out = false; /* actual33 return before ByteBuffer.get41 */
    else {
        mc_client_sdk_bytes_state bytes;
        (void)mc_client_sdk_bytes_inspect(mouse ? o->buttons : o->key_down, &bytes);
        if (index < 0 || (size_t)index >= bytes.limit)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_SOURCE_INDEX,
                                     "actual absolute ByteBuffer index", class_name, method, "(I)Z",
                                     mouse ? 41 : 26, false);
        else
            *out = mouse ? bytes.data[index] == 1 : bytes.data[index] != 0;
    }
    return mc_client_sdk_input_leave(o, &e, error);
}

mc_client_sdk_input_status mc_client_sdk_mouse_button_down(mc_client_sdk_input *o, int32_t index,
                                                           bool *out,
                                                           mc_client_sdk_input_error *e) {
    return down(o, index, out, e, true);
}

mc_client_sdk_input_status mc_client_sdk_keyboard_key_down(mc_client_sdk_input *o, int32_t index,
                                                           bool *out,
                                                           mc_client_sdk_input_error *e) {
    return down(o, index, out, e, false);
}

mc_client_sdk_input_status mc_client_sdk_keyboard_enable_repeat(mc_client_sdk_input *o, bool value,
                                                                mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (!mc_client_sdk_input_enter(o, "org/lwjgl/input/Keyboard", "enableRepeatEvents", "(Z)V",
                                   &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    o->repeat_enabled = value; /* actual putstatic7 */
    o->last_keyboard_store = 7;
    return mc_client_sdk_input_leave(o, &e, out);
}
