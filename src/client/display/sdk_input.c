#include "mc/client/settings/game_option.h"
#include "sdk_controllers_internal.h"
#include "sdk_display_internal.h"
#include "sdk_input_internal.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <stdlib.h>
#include <string.h>

mc_client_sdk_input_status mc_client_sdk_input_fail(mc_client_sdk_input_error *e,
                                                    mc_client_sdk_input_status status,
                                                    const char *operation, const char *class_name,
                                                    const char *method, const char *descriptor,
                                                    int32_t pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->site = (mc_client_sdk_input_site){class_name, method, descriptor, pc};
    e->native_policy = native;
    return status;
}

void mc_client_sdk_input_caller(mc_client_sdk_input_error *e, const char *class_name,
                                const char *method, const char *descriptor, int32_t pc) {
    if (e->caller_count < sizeof(e->callers) / sizeof(e->callers[0]))
        e->callers[e->caller_count++] =
            (mc_client_sdk_input_site){class_name, method, descriptor, pc};
    else {
        /* Preserve the actual reached primary and all admitted frames. This
         * extra native-policy annotation cannot replace a complete child. */
        e->caller_limit = true;
        e->pending_caller = (mc_client_sdk_input_site){class_name, method, descriptor, pc};
    }
}

bool mc_client_sdk_input_enter(mc_client_sdk_input *o, const char *class_name, const char *method,
                               const char *descriptor, mc_client_sdk_input_error *e) {
    if (o == NULL || !SDL_IsMainThread()) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                 "SDK input actual owner/main thread", class_name, method,
                                 descriptor, -1, true);
        return false;
    }
    const bool parent_busy =
        o != NULL && !o->parent_dispatch && mc_client_display_sdk_external_busy(o->display);
    if (o->phase != MC_CLIENT_SDK_INPUT_READY || o->active || parent_busy) {
        mc_client_sdk_input_fail(
            e,
            o->active || parent_busy ? MC_CLIENT_SDK_INPUT_BUSY : MC_CLIENT_SDK_INPUT_INVALID_PHASE,
            "SDK input native live-owner admission", class_name, method, descriptor, -1, true);
        return false;
    }
    o->active = true;
    return true;
}

mc_client_sdk_input_status mc_client_sdk_input_leave(mc_client_sdk_input *o,
                                                     mc_client_sdk_input_error *e,
                                                     mc_client_sdk_input_error *out) {
    if (e->status != MC_CLIENT_SDK_INPUT_OK) {
        o->failure = *e;
        if (e->native_policy)
            o->phase = MC_CLIENT_SDK_INPUT_FAILED;
    }
    o->active = false;
    if (out != NULL)
        *out = *e;
    return e->status;
}

mc_client_sdk_input_status mc_client_sdk_input_allocate(mc_client_display *display,
                                                        mc_client_event_port *events,
                                                        mc_client_sdk_input **out,
                                                        mc_client_sdk_input_error *error) {
    mc_client_sdk_input_error e = {0};
    if (display == NULL || events == NULL || out == NULL || !SDL_IsMainThread() ||
        !mc_client_display_sdk_input_candidate(display, out) ||
        events != mc_client_display_event_port(display))
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                 "SDK input native allocation", "org/lwjgl/opengl/Display",
                                 "initControls", "()V", -1, true);
    else {
        mc_client_sdk_input *o = calloc(1, sizeof(*o));
        if (o == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                     "SDK input native allocation", "org/lwjgl/opengl/Display",
                                     "initControls", "()V", -1, true);
        else {
            o->display = display;
            o->controllers_last_clinit_store = -1;
            o->controllers_last_create_pc = -1;
            o->window = mc_client_display_window(display);
            o->events = events;
            o->last_mouse_store = o->last_keyboard_store = -1;
            *out = o;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static bool name(mc_client_sdk_input *o, mc_client_sdk_name_map *map, size_t index,
                 const char *ascii, mc_client_sdk_input_error *e, const char *class_name,
                 const char *method, int32_t pc) {
    uint16_t units[32];
    const size_t n = strlen(ascii);
    if (n > sizeof(units) / sizeof(units[0])) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_LIMIT, "SDK native name extent", class_name,
                                 method, "()V", pc, true);
        return false;
    }
    for (size_t i = 0; i < n; ++i)
        units[i] = (uint8_t)ascii[i];
    e->string = mc_player_string_create(units, n, &map->names[index]);
    if (e->string != MC_PLAYER_OK) {
        mc_client_sdk_input_fail(
            e,
            e->string == MC_PLAYER_ALLOCATION_FAILED ? MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED
                                                     : MC_CLIENT_SDK_INPUT_LIMIT,
            "SDK actual immutable name owner", class_name, method, "()V", pc, true);
        return false;
    }
    ++map->count;
    (void)o;
    return true;
}

static bool bytes(mc_client_sdk_bytes **out, size_t size, mc_client_sdk_input_error *e,
                  const char *class_name, const char *method, int32_t pc) {
    const mc_client_sdk_queue_status child = mc_client_sdk_bytes_create(size, out);
    if (child == MC_CLIENT_SDK_QUEUE_OK)
        return true;
    const char *descriptor =
        strcmp(method, "create") == 0 ? "(Lorg/lwjgl/opengl/InputImplementation;)V" : "()V";
    e->queue = (mc_client_sdk_queue_error){child, method, descriptor, pc};
    mc_client_sdk_input_fail(
        e,
        child == MC_CLIENT_SDK_QUEUE_ALLOCATION_FAILED ? MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED
                                                       : MC_CLIENT_SDK_INPUT_LIMIT,
        "SDK actual buffer allocation", class_name, method, descriptor, pc, true);
    return false;
}

static bool mouse_initialize(mc_client_sdk_input *o, mc_client_sdk_input_error *e) {
    /* Sys.initialize is the real linked/initialized SDL/platform binding here;
     * no JVM/native-library load is required by this C implementation. */
    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0 || o->window == NULL) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                 "SDK Mouse real platform bootstrap", "org/lwjgl/input/Mouse",
                                 "initialize", "()V", 0, true);
        return false;
    }
    o->button_map.capacity = 16;
    o->button_map.names = calloc(16, sizeof(*o->button_map.names));
    if (o->button_map.names == NULL) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "SDK Mouse actual name array", "org/lwjgl/input/Mouse",
                                 "initialize", "()V", 5, true);
        return false;
    }
    for (size_t i = 0; i < 16; ++i) {
        char value[] = "BUTTON00";
        if (i < 10) {
            value[6] = (char)('0' + i);
            value[7] = '\0';
        } else {
            value[6] = '1';
            value[7] = (char)('0' + i - 10);
        }
        if (!name(o, &o->button_map, i, value, e, "org/lwjgl/input/Mouse", "initialize", 39))
            return false;
    }
    o->mouse_initialized = true; /* initialize68 */
    o->last_mouse_store = 68;
    return true;
}

static bool keyboard_class(mc_client_sdk_input *o, mc_client_sdk_input_error *e) {
    o->key_map.capacity = 256;
    o->key_map.names = calloc(256, sizeof(*o->key_map.names));
    if (o->key_map.names == NULL) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "SDK Keyboard actual name array", "org/lwjgl/input/Keyboard",
                                 "<clinit>", "()V", 3, true);
        return false;
    }
    /* Same retained KEY_ facts as the existing canonical name-table module.
     * This native map owns real immutable Strings at those actual indices;
     * reflective Java allocation/exception behavior is not fabricated. */
    for (int32_t key = 0; key < 256; ++key) {
        const char *value = NULL;
        if (mc_game_key_name(key, &value) != MC_GAME_OPTION_OK) {
            mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "SDK canonical key-name facts", "org/lwjgl/input/Keyboard",
                                     "<clinit>", "()V", 125, true);
            return false;
        }
        if (value != NULL && !name(o, &o->key_map, (size_t)key, value, e,
                                   "org/lwjgl/input/Keyboard", "<clinit>", 133))
            return false;
    }
    if (!bytes(&o->key_down, 256, e, "org/lwjgl/input/Keyboard", "<clinit>", 189))
        return false;
    o->current_key = calloc(1, sizeof(*o->current_key));
    if (o->current_key == NULL) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "SDK Keyboard actual KeyEvent owners", "org/lwjgl/input/Keyboard",
                                 "<clinit>", "()V", 195, true);
        return false;
    }
    o->temporary_key = calloc(1, sizeof(*o->temporary_key));
    if (o->temporary_key == NULL) {
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "SDK Keyboard actual temporary KeyEvent owner",
                                 "org/lwjgl/input/Keyboard", "<clinit>", "()V", 206, true);
        return false;
    }
    return true;
}

mc_client_sdk_input_status mc_client_sdk_input_construct_default(mc_client_sdk_input *o,
                                                                 mc_client_sdk_input_error *error) {
    mc_client_sdk_input_error e = {0};
    if (o == NULL || !SDL_IsMainThread() || o->phase != MC_CLIENT_SDK_INPUT_RAW ||
        mc_client_display_inputs(o->display) != o) {
        mc_client_sdk_input_fail(&e,
                                 o == NULL ? MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT
                                           : MC_CLIENT_SDK_INPUT_INVALID_PHASE,
                                 "SDK input once-only native construction",
                                 "org/lwjgl/opengl/Display", "initControls", "()V", -1, true);
        if (error != NULL)
            *error = e;
        return e.status;
    }
    o->phase = MC_CLIENT_SDK_INPUT_CONSTRUCTING;
    o->active = true;
    /* Default Mouse class: real button-name map, -1 buttonCount, default clip
     * property true, actual Windows/Linux platform discovered, null Cursor. */
    o->clip_coordinates = true;
    o->button_count = -1; /* Mouse.<clinit>1; not the private backend capability */
    const char *driver = SDL_GetCurrentVideoDriver();
    o->emulate_cursor_animation = driver != NULL && strcmp(driver, "windows") == 0; /* <clinit>35 */
    if (!mouse_initialize(o, &e))
        goto done;
    e.platform = mc_client_sdk_platform_query(o->window, &o->platform);
    if (e.platform != MC_CLIENT_SDK_PLATFORM_OK) {
        mc_client_sdk_input_fail(&e,
                                 e.platform == MC_CLIENT_SDK_PLATFORM_UNSUPPORTED
                                     ? MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE
                                     : MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                 "SDK actual InputImplementation.createMouse attachment",
                                 "org/lwjgl/input/Mouse", "create",
                                 "(Lorg/lwjgl/opengl/InputImplementation;)V", 23, true);
        goto done;
    }
    if (mc_client_sdk_queue_create(22, &o->mouse_queue, &e.queue) != MC_CLIENT_SDK_QUEUE_OK ||
        !bytes(&o->mouse_temp, 22, &e, "org/lwjgl/input/Mouse", "create", 23)) {
        if (e.status == MC_CLIENT_SDK_INPUT_OK)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                     "SDK Mouse native event queue", "org/lwjgl/input/Mouse",
                                     "create", "(Lorg/lwjgl/opengl/InputImplementation;)V", 23,
                                     true);
        goto done;
    }
    o->backend_buttons =
        calloc(o->platform.button_count == 0 ? 1 : (size_t)o->platform.button_count, 1);
    if (o->backend_buttons == NULL) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "actual native mouse button cache", "org/lwjgl/input/Mouse",
                                 "create", "(Lorg/lwjgl/opengl/InputImplementation;)V", 23, true);
        goto done;
    }
    o->has_wheel = o->platform.has_wheel; /* actual hasWheel36 */
    o->last_mouse_store = 36;
    o->mouse_created = true;
    o->last_mouse_store = 40;
    o->button_count = o->platform.button_count; /* actual buttonCount51 */
    o->last_mouse_store = 51;
    if (!bytes(&o->buttons, (size_t)o->button_count, &e, "org/lwjgl/input/Mouse", "create", 57))
        goto done;
    o->coordinates = calloc(3, sizeof(*o->coordinates)); /* actual direct IntBuffer3 at64/67 */
    if (o->coordinates == NULL) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "SDK Mouse actual coordinate buffer", "org/lwjgl/input/Mouse",
                                 "create", "(Lorg/lwjgl/opengl/InputImplementation;)V", 64, true);
        goto done;
    }
    if (!bytes(&o->mouse_read, 1100, &e, "org/lwjgl/input/Mouse", "create", 97))
        goto done;
    (void)mc_client_sdk_bytes_limit(o->mouse_read, 0); /* actual limit107 */
    if (!SDL_SetWindowRelativeMouseMode(o->window, false)) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                 "SDK actual grabMouse(false)", "org/lwjgl/input/Mouse",
                                 "setGrabbed", "(Z)V", 77, true);
        goto done;
    }
    /* Source setGrabbed(false) polls at82 then resets the transient deltas/read
     * buffer. Read actual SDL synchronous state without pumping/consuming its
     * queue. The first source poll after messages sees retained real events. */
    int width = 0, height = 0;
    if (!SDL_GetWindowSize(o->window, &width, &height)) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                 "SDK Mouse actual window extent", "org/lwjgl/input/Mouse", "poll",
                                 "()V", 31, true);
        goto done;
    }
    o->width = width;
    o->height = height;
    if (!mc_client_sdk_mouse_poll_body(o, false, &e))
        goto done;
    o->current_mouse.x = o->x;    /* setGrabbed88 */
    o->current_mouse.y = o->y;    /* setGrabbed94 */
    o->event_raw_x = o->x;        /* setGrabbed100 */
    o->event_raw_y = o->y;        /* setGrabbed106 */
    o->dx = o->dy = o->wheel = 0; /* resetMouse2/6/9 */
    mc_client_sdk_bytes_state read;
    (void)mc_client_sdk_bytes_inspect(o->mouse_read, &read);
    (void)mc_client_sdk_bytes_position(o->mouse_read, read.limit); /* resetMouse21 */
    if (!keyboard_class(o, &e))
        goto done;
    o->keyboard_initialized = true; /* Keyboard.initialize11 after real Sys */
    o->last_keyboard_store = 11;
    if (mc_client_sdk_queue_create(18, &o->keyboard_queue, &e.queue) != MC_CLIENT_SDK_QUEUE_OK ||
        !bytes(&o->keyboard_temp, 18, &e, "org/lwjgl/input/Keyboard", "create", 23)) {
        if (e.status == MC_CLIENT_SDK_INPUT_OK)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                     "SDK Keyboard native event queue", "org/lwjgl/input/Keyboard",
                                     "create", "(Lorg/lwjgl/opengl/InputImplementation;)V", 23,
                                     true);
        goto done;
    }
    /* Physical window text attachment is real; no successful fake platform
     * callback or zero keyboard state is substituted. */
    if (!SDL_StartTextInput(o->window)) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                 "SDK actual keyboard window attachment",
                                 "org/lwjgl/input/Keyboard", "create",
                                 "(Lorg/lwjgl/opengl/InputImplementation;)V", 23, true);
        goto done;
    }
    o->text_input_started = true;
    o->keyboard_created = true;
    o->last_keyboard_store = 29;
    if (!bytes(&o->keyboard_read, 900, &e, "org/lwjgl/input/Keyboard", "create", 35))
        goto done;
    (void)mc_client_sdk_bytes_limit(o->keyboard_read, 0); /* actual reset4 */
done:
    o->active = false;
    o->phase =
        e.status == MC_CLIENT_SDK_INPUT_OK ? MC_CLIENT_SDK_INPUT_READY : MC_CLIENT_SDK_INPUT_FAILED;
    if (e.status != MC_CLIENT_SDK_INPUT_OK)
        o->failure = e;
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_sdk_input_inspect(const mc_client_sdk_input *o, mc_client_sdk_input_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_sdk_input_state){
        .phase = o->phase,
        .active = o->active,
        .mouse_initialized = o->mouse_initialized,
        .mouse_created = o->mouse_created,
        .keyboard_initialized = o->keyboard_initialized,
        .keyboard_created = o->keyboard_created,
        .controllers_created = o->controllers_created,
        .grabbed = o->grabbed,
        .clip_coordinates = o->clip_coordinates,
        .repeat_enabled = o->repeat_enabled,
        .platform = o->platform,
        .x = o->x,
        .y = o->y,
        .dx = o->dx,
        .dy = o->dy,
        .wheel = o->wheel,
        .button_count = o->button_count,
        .buttons = o->buttons,
        .key_down = o->key_down,
        .mouse_read = o->mouse_read,
        .coordinates = o->coordinates,
        .keyboard_read = o->keyboard_read,
        .mouse_events = o->mouse_queue,
        .keyboard_events = o->keyboard_queue,
        .button_names = (const mc_player_string *const *)o->button_map.names,
        .key_names = (const mc_player_string *const *)o->key_map.names,
        .button_names_count = o->button_map.count,
        .key_names_count = o->key_map.count,
        .source_queue_full_results = o->queue_full_results,
        .last_mouse_store_pc = o->last_mouse_store,
        .last_keyboard_store_pc = o->last_keyboard_store,
        .pending_native_event = o->pending,
        .failure = &o->failure,
        .current_key = o->current_key == NULL ? (mc_client_sdk_key_event){0} : *o->current_key,
        .current_mouse = o->current_mouse};
    return true;
}

mc_client_sdk_input_status mc_client_sdk_input_retire(mc_client_sdk_input **slot,
                                                      mc_client_sdk_input_error *error) {
    mc_client_sdk_input_error e = {0};
    if (slot == NULL || !SDL_IsMainThread())
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                 "SDK input native retirement", NULL, NULL, NULL, -1, true);
    else if (*slot != NULL) {
        mc_client_sdk_input *o = *slot;
        mc_client_events_state events;
        if (o->active || mc_client_display_sdk_external_busy(o->display) ||
            !mc_client_events_inspect(o->events, &events) || events.active) {
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_BUSY,
                                     "SDK input active native retirement", NULL, NULL, NULL, -1,
                                     true);
        } else {
            o->phase = MC_CLIENT_SDK_INPUT_RETIRING;
            o->active = true;
            if (mc_client_sdk_controllers_native_cleanup(o, &e) != MC_CLIENT_SDK_INPUT_OK) {
                o->active = false;
                o->failure = e;
            } else if (o->backend_grabbed && !SDL_SetWindowRelativeMouseMode(o->window, false)) {
                o->active = false;
                mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                         "SDK actual native grab retirement", NULL, NULL, NULL, -1,
                                         true);
                o->failure = e;
            } else if (o->text_input_started && !SDL_StopTextInput(o->window)) {
                o->backend_grabbed = false;
                o->active = false;
                mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                                         "SDK actual text attachment cleanup", NULL, NULL, NULL, -1,
                                         true);
                o->failure = e;
            } else {
                o->text_input_started = false;
                o->backend_grabbed = false;
                mc_client_event_release(&o->pending);
                mc_client_sdk_name_map *maps[] = {&o->button_map, &o->key_map};
                for (size_t j = 0; j < 2; ++j) {
                    for (size_t i = 0; i < maps[j]->capacity; ++i)
                        if (maps[j]->names != NULL)
                            mc_player_string_release(maps[j]->names[i]);
                    free(maps[j]->names);
                }
                mc_client_sdk_bytes_free(&o->buttons);
                mc_client_sdk_bytes_free(&o->key_down);
                mc_client_sdk_bytes_free(&o->mouse_read);
                mc_client_sdk_bytes_free(&o->keyboard_read);
                mc_client_sdk_bytes_free(&o->mouse_temp);
                mc_client_sdk_bytes_free(&o->keyboard_temp);
                mc_client_sdk_queue_free(&o->mouse_queue);
                mc_client_sdk_queue_free(&o->keyboard_queue);
                free(o->current_key);
                free(o->temporary_key);
                free(o->backend_buttons);
                free(o->coordinates);
                free(o->controllers);
                free(o->controller_events);
                free(o);
                *slot = NULL;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
