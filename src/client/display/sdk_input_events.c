#include "mc/core/java/java.h"
#include "sdk_input_internal.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <string.h>
#include <xmmintrin.h>

/* Maps actual SDL physical locations to pinned KEY_ constants. This is native
 * input adaptation, not a replacement Keyboard.poll/next algorithm. */
#include "sdk_key_codes.inc"

int32_t mc_client_sdk_input_key_code(int32_t scancode) {
    if (scancode == SDL_SCANCODE_UNKNOWN)
        return 0;
    if (scancode < 0 || (size_t)scancode >= sizeof(sdk_key_codes) / sizeof(sdk_key_codes[0]) ||
        sdk_key_codes[scancode] == 0)
        return -1;
    return sdk_key_codes[scancode];
}

static void u32(uint8_t *out, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        out[i] = (uint8_t)(value >> (24u - 8u * i));
}

static void u64(uint8_t *out, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        out[i] = (uint8_t)(value >> (56u - 8u * i));
}

static bool emit(mc_client_sdk_input *o, bool mouse, const uint8_t *bytes, size_t count,
                 mc_client_sdk_input_error *e) {
    mc_client_sdk_bytes *temp = mouse ? o->mouse_temp : o->keyboard_temp;
    mc_client_sdk_bytes_clear(temp);
    if (!mc_client_sdk_bytes_write(temp, bytes, count)) {
        mc_client_sdk_input_fail(
            e, MC_CLIENT_SDK_INPUT_LIMIT, "SDK actual native event-frame extent",
            "org/lwjgl/opengl/EventQueue", "putEvent", "(Ljava/nio/ByteBuffer;)Z", -1, true);
        return false;
    }
    mc_client_sdk_bytes_flip(temp);
    bool result = false;
    if (mc_client_sdk_queue_put(mouse ? o->mouse_queue : o->keyboard_queue, temp, &result,
                                &e->queue) != MC_CLIENT_SDK_QUEUE_OK) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_LIMIT, "SDK native event queue operation",
                                 "org/lwjgl/opengl/EventQueue", "putEvent",
                                 "(Ljava/nio/ByteBuffer;)Z", e->queue.pc, true);
        return false;
    }
    /* Both pinned native backends ignore EventQueue.putEvent's full=false.
     * Preserve that explicit source result; it is not an unreported native
     * byte-budget discard. The transport has retained all prior events. */
    if (!result && o->queue_full_results == SIZE_MAX) {
        mc_client_sdk_input_fail(
            e, MC_CLIENT_SDK_INPUT_LIMIT, "native source-full observation counter",
            "org/lwjgl/opengl/EventQueue", "putEvent", "(Ljava/nio/ByteBuffer;)Z", 78, true);
        return false;
    }
    if (!result)
        ++o->queue_full_results;
    return true;
}

static bool keyboard_event(mc_client_sdk_input *o, int32_t key, bool pressed, uint32_t character,
                           bool repeat, uint64_t nanos, mc_client_sdk_input_error *e) {
    uint8_t data[18];
    u32(data, (uint32_t)key);
    data[4] = pressed ? 1 : 0;
    u32(data + 5, character);
    u64(data + 9, nanos);
    data[17] = repeat ? 1 : 0;
    return emit(o, false, data, sizeof(data), e);
}

bool mc_client_sdk_input_flush_key(mc_client_sdk_input *o, mc_client_sdk_input_error *e) {
    if (!o->key_pending)
        return true;
    const mc_client_sdk_key_event v = o->retained_key;
    if (!keyboard_event(o, v.key, v.pressed, v.character, v.repeat, (uint64_t)v.nanos, e))
        return false;
    o->key_pending = false;
    return true;
}

static bool committed_unit(mc_client_sdk_input *o, uint32_t unit, uint64_t nanos,
                           mc_client_sdk_input_error *e) {
    if (o->key_pending && o->retained_key.pressed && o->retained_key.character == 0) {
        o->retained_key.character = unit;
        return true;
    }
    if (!mc_client_sdk_input_flush_key(o, e))
        return false;
    int64_t timestamp;
    memcpy(&timestamp, &nanos, sizeof(timestamp));
    o->retained_key = (mc_client_sdk_key_event){0, unit, timestamp, true, false};
    o->key_pending = true;
    return true;
}

static bool text_events(mc_client_sdk_input *o, const SDL_Event *event,
                        mc_client_sdk_input_error *e) {
    const unsigned char *p = (const unsigned char *)event->text.text;
    if (p == NULL) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_EVENT,
                                 "null native committed-text payload", NULL,
                                 "SDL committed-text ingestion", NULL, -1, true);
        return false;
    }
    /* SDL committed text is UTF8. Decode the complete retained payload before
     * emitting anything, so malformed native text leaves its record retained.
     * The first unit attaches to the retained physical key when available;
     * additional/IME-only units use key0. Raw Windows/X11 native message timing
     * and coalescing parity is not inferred from this SDL adaptation. */
    size_t bytes = strlen((const char *)p), at = 0;
    while (at < bytes) {
        const uint32_t first = p[at++];
        uint32_t scalar = first, trailing = 0, minimum = 0;
        if (first >= 0xc2u && first <= 0xdfu) {
            scalar = first & 31u;
            trailing = 1;
            minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            scalar = first & 15u;
            trailing = 2;
            minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            scalar = first & 7u;
            trailing = 3;
            minimum = 0x10000u;
        } else if (first >= 0x80u)
            goto unsupported;
        if (trailing > bytes - at)
            goto unsupported;
        for (uint32_t j = 0; j < trailing; ++j) {
            if ((p[at] & 0xc0u) != 0x80u)
                goto unsupported;
            scalar = (scalar << 6) | (p[at++] & 63u);
        }
        if (scalar < minimum || scalar > 0x10ffffu || (scalar >= 0xd800u && scalar <= 0xdfffu))
            goto unsupported;
    }
    at = 0;
    while (at < bytes) {
        const uint32_t first = p[at++];
        uint32_t scalar = first, trailing = 0;
        if (first >= 0xf0u) {
            scalar = first & 7u;
            trailing = 3;
        } else if (first >= 0xe0u) {
            scalar = first & 15u;
            trailing = 2;
        } else if (first >= 0xc0u) {
            scalar = first & 31u;
            trailing = 1;
        }
        for (uint32_t j = 0; j < trailing; ++j)
            scalar = (scalar << 6) | (p[at++] & 63u);
        if (scalar > 0xffffu) {
            scalar -= 0x10000u;
            if (!committed_unit(o, 0xd800u + (scalar >> 10), event->common.timestamp, e))
                return false;
            scalar = 0xdc00u + (scalar & 0x3ffu);
        }
        if (!committed_unit(o, scalar, event->common.timestamp, e))
            return false;
    }
    return true;
unsupported:
    mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_EVENT,
                             "SDK malformed committed SDL UTF8", NULL,
                             "SDL committed-text ingestion", NULL, -1, true);
    return false;
}

static bool mouse_event(mc_client_sdk_input *o, const SDL_Event *event, int native_height,
                        mc_client_sdk_input_error *e) {
    int32_t button = -1, x = o->native_x, y = o->native_y, wheel = 0;
    bool pressed = false;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP,
                                 "SDK reached SDL coordinate conversion", NULL,
                                 "SDL coordinate ingestion", NULL, -1, true);
        return false;
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        x = mc_java_float_to_i32(o->backend_grabbed ? event->motion.xrel : event->motion.x);
        y = o->backend_grabbed ? mc_java_i32_neg(mc_java_float_to_i32(event->motion.yrel))
                               : mc_java_i32_sub(mc_java_i32_sub(native_height, 1),
                                                 mc_java_float_to_i32(event->motion.y));
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
               event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        const uint8_t b = event->button.button;
        button = b == SDL_BUTTON_LEFT     ? 0
                 : b == SDL_BUTTON_RIGHT  ? 1
                 : b == SDL_BUTTON_MIDDLE ? 2
                                          : (int32_t)b - 1;
        /* SDL X11 folds physical8/9 to X1/X2 and subtracts4 for later buttons.
         * The actual SDK maps8/9 to3/4, but physical>9 to raw-1. */
        if (o->platform.lwjgl_platform == 1 && b > SDL_BUTTON_X2)
            button = (int32_t)b + 3;
        if (b == 0 || button < 0 || button > INT8_MAX || button >= o->platform.button_count)
            goto unsupported;
        pressed = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        x = o->backend_grabbed ? 0 : mc_java_float_to_i32(event->button.x);
        y = o->backend_grabbed ? 0
                               : mc_java_i32_sub(mc_java_i32_sub(native_height, 1),
                                                 mc_java_float_to_i32(event->button.y));
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        if (event->wheel.x != 0.0f)
            goto unsupported;
        volatile float scaled = event->wheel.y * 120.0f;
        if (!(scaled >= -0x1p31f && scaled < 0x1p31f))
            goto unsupported;
        wheel = mc_java_float_to_i32(scaled);
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            wheel = mc_java_i32_neg(wheel);
    } else
        goto unsupported;
    if (o->backend_grabbed && event->type == SDL_EVENT_MOUSE_WHEEL)
        x = y = 0;
    uint8_t data[22];
    data[0] = (uint8_t)button;
    data[1] = pressed ? 1 : 0;
    u32(data + 2, (uint32_t)x);
    u32(data + 6, (uint32_t)y);
    u32(data + 10, (uint32_t)wheel);
    u64(data + 14, event->common.timestamp);
    if (!emit(o, true, data, sizeof(data), e))
        return false;
    if (button >= 0)
        o->backend_buttons[button] = pressed ? 1 : 0;
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        if (o->backend_grabbed) {
            o->native_dx = mc_java_i32_add(o->native_dx, x);
            o->native_dy = mc_java_i32_add(o->native_dy, y);
        } else {
            o->native_dx = mc_java_i32_add(o->native_dx, mc_java_i32_sub(x, o->native_x));
            o->native_dy = mc_java_i32_add(o->native_dy, mc_java_i32_sub(y, o->native_y));
            o->native_x = x;
            o->native_y = y;
        }
    } else if (button >= 0 && !o->backend_grabbed) {
        o->native_x = x;
        o->native_y = y;
    }
    o->native_wheel = mc_java_i32_add(o->native_wheel, wheel);
    return true;
unsupported:
    mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_EVENT,
                             "SDK unimplemented SDL mouse translation", NULL, "SDL mouse ingestion",
                             NULL, -1, true);
    return false;
}

static bool collect(mc_client_sdk_input *o, bool mouse, mc_client_sdk_input_error *e) {
    for (;;) {
        if (o->pending == NULL) {
            const mc_client_events_status status = mc_client_events_take(
                o->events, mouse ? SDL_EVENT_MOUSE_MOTION : SDL_EVENT_KEY_DOWN,
                mouse ? SDL_EVENT_MOUSE_WHEEL : SDL_EVENT_TEXT_INPUT, false, &o->pending,
                &e->events);
            if (status != MC_CLIENT_EVENTS_OK) {
                mc_client_sdk_input_fail(e,
                                         status == MC_CLIENT_EVENTS_BUSY
                                             ? MC_CLIENT_SDK_INPUT_BUSY
                                             : MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                         "SDK actual retained SDL transport", NULL,
                                         "SDL input ingestion", NULL, -1, true);
                return false;
            }
        }
        if (o->pending == NULL)
            return true;
        const SDL_Event *event = mc_client_event_read(o->pending);
        e->pending_event_type = event->type;
        bool ok = false;
        if (mouse) {
            int width, height;
            if (mc_client_event_window_extent(o->pending, &width, &height))
                ok = mouse_event(o, event, height, e);
        } else if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
            const int32_t key = mc_client_sdk_input_key_code((int32_t)event->key.scancode);
            if (key >= 0 && mc_client_sdk_input_flush_key(o, e)) {
                int64_t timestamp;
                const uint64_t nanos = event->common.timestamp;
                memcpy(&timestamp, &nanos, sizeof(timestamp));
                o->backend_keys[key] = event->type == SDL_EVENT_KEY_DOWN ? 1 : 0;
                o->retained_key = (mc_client_sdk_key_event){
                    key, 0, timestamp, event->type == SDL_EVENT_KEY_DOWN, event->key.repeat};
                o->key_pending = true;
                ok = true;
            }
        } else if (event->type == SDL_EVENT_TEXT_INPUT)
            ok = text_events(o, event, e);
        if (!ok) {
            if (e->status == MC_CLIENT_SDK_INPUT_OK)
                mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_EVENT,
                                         "SDK unimplemented SDL input translation", NULL,
                                         "SDL input ingestion", NULL, -1, true);
            return false;
        }
        mc_client_event_release(&o->pending);
        e->pending_event_type = 0;
    }
}

bool mc_client_sdk_input_collect_mouse(mc_client_sdk_input *o, mc_client_sdk_input_error *e) {
    return collect(o, true, e);
}

bool mc_client_sdk_input_collect_keyboard(mc_client_sdk_input *o, mc_client_sdk_input_error *e) {
    return collect(o, false, e);
}

bool mc_client_sdk_input_read_queue(mc_client_sdk_input *o, bool mouse,
                                    mc_client_sdk_input_error *e) {
    mc_client_sdk_bytes *read = mouse ? o->mouse_read : o->keyboard_read;
    mc_client_sdk_bytes_compact(read); /* actual compact3 before implementation13 */
    if (!mouse && !mc_client_sdk_input_flush_key(o, e)) {
        mc_client_sdk_input_caller(e, "org/lwjgl/input/Keyboard", "read", "()V", 13);
        return false;
    }
    if (mc_client_sdk_queue_copy(mouse ? o->mouse_queue : o->keyboard_queue, read, &e->queue) !=
        MC_CLIENT_SDK_QUEUE_OK) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_LIMIT, "SDK actual EventQueue.copyEvents",
                                 "org/lwjgl/opengl/EventQueue", "copyEvents",
                                 "(Ljava/nio/ByteBuffer;)V", e->queue.pc, true);
        mc_client_sdk_input_caller(e, mouse ? "org/lwjgl/input/Mouse" : "org/lwjgl/input/Keyboard",
                                   "read", "()V", 13);
        return false;
    }
    mc_client_sdk_bytes_flip(read); /* actual flip21 */
    return true;
}
