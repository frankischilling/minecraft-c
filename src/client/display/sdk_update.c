#include "sdk_update.h"
#include "sdk_display_internal.h"
#include "sdk_input_internal.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <stdlib.h>

struct mc_client_sdk_update {
    mc_client_display *display;
    mc_client_sdk_input *input;
    mc_client_event_port *events;
    bool active, window_resized, failed;
    int32_t width, height, last_store;
    mc_client_sdk_update_error failure;
};

static mc_client_sdk_update_status failure(mc_client_sdk_update_error *e,
                                           mc_client_sdk_update_status status,
                                           const char *operation, const char *method,
                                           const char *descriptor, int32_t pc) {
    e->status = status;
    e->operation = operation;
    e->site = (mc_client_sdk_input_site){"org/lwjgl/opengl/Display", method, descriptor, pc};
    return status;
}

mc_client_sdk_update_status
mc_client_sdk_update_allocate(mc_client_display *display, mc_client_sdk_input *input,
                              mc_client_event_port *events, int32_t width, int32_t height,
                              mc_client_sdk_update **out, mc_client_sdk_update_error *error) {
    mc_client_sdk_update_error e = {0};
    if (display == NULL || out == NULL || !SDL_IsMainThread() || input != NULL || events != NULL ||
        width != 0 || height != 0 || !mc_client_display_sdk_update_candidate(display, out))
        failure(&e, MC_CLIENT_SDK_UPDATE_INVALID_ARGUMENT, "Display update native allocation", NULL,
                NULL, -1);
    else {
        mc_client_sdk_update *o = calloc(1, sizeof(*o));
        if (o == NULL)
            failure(&e, MC_CLIENT_SDK_UPDATE_ALLOCATION_FAILED, "Display update native allocation",
                    NULL, NULL, -1);
        else {
            o->display = display;
            o->input = input;
            o->events = events;
            o->width = width;
            o->height = height;
            o->last_store = -1;
            *out = o;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_sdk_update_status mc_client_sdk_update_attach(mc_client_sdk_update *o,
                                                        mc_client_sdk_input *input,
                                                        mc_client_event_port *events,
                                                        mc_client_sdk_update_error *out) {
    mc_client_sdk_update_error e = {0};
    if (o == NULL || input == NULL || events == NULL || !SDL_IsMainThread() ||
        mc_client_display_update_owner(o->display) != o ||
        mc_client_display_inputs(o->display) != input ||
        mc_client_display_event_port(o->display) != events)
        failure(&e, MC_CLIENT_SDK_UPDATE_INVALID_ARGUMENT, "Display update actual child attachment",
                NULL, NULL, -1);
    else if (o->active || o->input != NULL || o->events != NULL)
        failure(&e, MC_CLIENT_SDK_UPDATE_INVALID_PHASE, "Display update once-only child attachment",
                NULL, NULL, -1);
    else {
        o->input = input;
        o->events = events;
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

void mc_client_sdk_update_create_dimensions(mc_client_sdk_update *o, int32_t width,
                                            int32_t height) {
    o->width = width;
    o->last_store = 86;
    o->height = height;
    o->last_store = 95;
}

static void caller(mc_client_sdk_update_error *e, const char *method, const char *descriptor,
                   int32_t pc) {
    /* The fixed maximum is processMessages40 -> update(Z)140 -> update()1.
     * Leaf input diagnostics carry their separate complete caller chain. */
    if (e->caller_count < sizeof(e->callers) / sizeof(e->callers[0]))
        e->callers[e->caller_count++] =
            (mc_client_sdk_input_site){"org/lwjgl/opengl/Display", method, descriptor, pc};
}

static bool enter(mc_client_sdk_update *o, const char *method, const char *descriptor,
                  mc_client_sdk_update_error *e) {
    if (o == NULL || !SDL_IsMainThread()) {
        failure(e, MC_CLIENT_SDK_UPDATE_INVALID_ARGUMENT, "Display native update admission", method,
                descriptor, -1);
        return false;
    }
    if (o->active || mc_client_display_sdk_external_busy(o->display)) {
        failure(e, MC_CLIENT_SDK_UPDATE_BUSY, "Display active update admission", method, descriptor,
                -1);
        return false;
    }
    if (o->failed || !mc_client_display_sdk_live(o->display)) {
        failure(e, MC_CLIENT_SDK_UPDATE_INVALID_PHASE, "Display live native update owner", method,
                descriptor, -1);
        return false;
    }
    o->active = true;
    return true;
}

static mc_client_sdk_update_status leave(mc_client_sdk_update *o, mc_client_sdk_update_error *e,
                                         mc_client_sdk_update_error *out) {
    if (e->status != MC_CLIENT_SDK_UPDATE_OK) {
        o->failure = *e;
        if (e->status == MC_CLIENT_SDK_UPDATE_PLATFORM_FAILED ||
            e->status == MC_CLIENT_SDK_UPDATE_UNSUPPORTED_PROFILE ||
            (e->status == MC_CLIENT_SDK_UPDATE_INPUT_FAILED && e->input.native_policy))
            o->failed = true;
    }
    o->active = false;
    if (out != NULL)
        *out = *e;
    return e->status;
}

static bool devices(mc_client_sdk_update *o, mc_client_sdk_update_error *e) {
    mc_client_sdk_input *input = o->input;
    /* Only this private dispatch token admits child entry while its actual
     * Display.update owner is active. Each child sets its own active guard
     * before any external SDL operation; callbacks cannot reuse the token. */
    input->parent_dispatch = true;
    if (input->mouse_created) { /* actual Mouse.isCreated0 */
        if (mc_client_sdk_mouse_poll(input, &e->input) != MC_CLIENT_SDK_INPUT_OK) {
            mc_client_sdk_input_caller(&e->input, "org/lwjgl/opengl/Display", "pollDevices", "()V",
                                       6);
            goto failed;
        }
        if (mc_client_sdk_mouse_update_cursor(input, &e->input) != MC_CLIENT_SDK_INPUT_OK) {
            mc_client_sdk_input_caller(&e->input, "org/lwjgl/opengl/Display", "pollDevices", "()V",
                                       9);
            goto failed;
        }
    }
    if (input->keyboard_created && /* actual Keyboard.isCreated12 */
        mc_client_sdk_keyboard_poll(input, &e->input) != MC_CLIENT_SDK_INPUT_OK) {
        mc_client_sdk_input_caller(&e->input, "org/lwjgl/opengl/Display", "pollDevices", "()V", 18);
        goto failed;
    }
    bool controllers = false;
    if (mc_client_sdk_controllers_is_created(input, &controllers, &e->input) !=
        MC_CLIENT_SDK_INPUT_OK) {
        mc_client_sdk_input_caller(&e->input, "org/lwjgl/opengl/Display", "pollDevices", "()V", 21);
        goto failed;
    }
    input->parent_dispatch = false;
    if (controllers) {
        failure(e, MC_CLIENT_SDK_UPDATE_UNSUPPORTED_PROFILE,
                "actual created JInput controller poll", "pollDevices", "()V", 27);
        return false;
    }
    return true;
failed:
    input->parent_dispatch = false;
    failure(e, MC_CLIENT_SDK_UPDATE_INPUT_FAILED, "Display actual input child", "pollDevices",
            "()V", -1);
    return false;
}

static bool messages(mc_client_sdk_update *o, mc_client_sdk_update_error *e) {
    if (!mc_client_display_sdk_created(o->display)) {
        failure(e, MC_CLIENT_SDK_UPDATE_SOURCE_ILLEGAL_STATE,
                "actual uncreated Display.processMessages", "processMessages", "()V", 12);
        return false;
    }
    const mc_client_events_status pump = mc_client_events_pump(o->events, &e->events);
    e->messages_pumped = e->events.pump_completed;
    if (pump != MC_CLIENT_EVENTS_OK) {
        failure(e, MC_CLIENT_SDK_UPDATE_PLATFORM_FAILED,
                "actual shared DisplayImplementation.update", "processMessages", "()V", 25);
        return false;
    }
    /* SDL has processed real window metadata into the port's native backend
     * state. This dispatcher consumes those window records only after that
     * effect; input remains in the same FIFO until its actual device poll.
     * Close is retained as its destructive SDK getter's actual pending flag. */
    for (;;) {
        mc_client_event_record *record = NULL;
        if (mc_client_events_take(o->events, SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST, false,
                                  &record, &e->dispatch) != MC_CLIENT_EVENTS_OK) {
            failure(e, MC_CLIENT_SDK_UPDATE_PLATFORM_FAILED, "actual native window dispatch",
                    "processMessages", "()V", 25);
            return false;
        }
        if (record == NULL)
            break;
        mc_client_event_release(&record);
    }
    if (!devices(o, e)) {
        caller(e, "processMessages", "()V", 40);
        return false;
    }
    return true;
}

mc_client_sdk_update_status
mc_client_sdk_display_process_messages(mc_client_sdk_update *o, mc_client_sdk_update_error *out) {
    mc_client_sdk_update_error e = {0};
    if (!enter(o, "processMessages", "()V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    (void)messages(o, &e);
    return leave(o, &e, out);
}

static mc_client_sdk_update_status window_boolean(mc_client_sdk_update *o, bool close, bool *result,
                                                  mc_client_sdk_update_error *out) {
    mc_client_sdk_update_error e = {0};
    const char *method = close ? "isCloseRequested" : "isActive";
    if (result == NULL || !enter(o, method, "()Z", &e)) {
        if (result == NULL)
            failure(&e, MC_CLIENT_SDK_UPDATE_INVALID_ARGUMENT, "Display native boolean output",
                    method, "()Z", -1);
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (!mc_client_display_sdk_created(o->display))
        failure(&e, MC_CLIENT_SDK_UPDATE_SOURCE_ILLEGAL_STATE,
                "actual uncreated Display state query", method, "()Z", 12);
    else if (close)
        *result = mc_client_events_take_close(o->events); /* actual backend read1/clear7 */
    else
        *result = (SDL_GetWindowFlags(mc_client_display_window(o->display)) &
                   SDL_WINDOW_INPUT_FOCUS) != 0;
    return leave(o, &e, out);
}

mc_client_sdk_update_status mc_client_sdk_display_close_requested(mc_client_sdk_update *o,
                                                                  bool *result,
                                                                  mc_client_sdk_update_error *e) {
    return window_boolean(o, true, result, e);
}

mc_client_sdk_update_status mc_client_sdk_display_active(mc_client_sdk_update *o, bool *result,
                                                         mc_client_sdk_update_error *e) {
    return window_boolean(o, false, result, e);
}

static mc_client_sdk_update_status source_extent(mc_client_sdk_update *o, bool width,
                                                 int32_t *result, mc_client_sdk_update_error *out) {
    mc_client_sdk_update_error e = {0};
    const char *method = width ? "getWidth" : "getHeight";
    if (result == NULL || !enter(o, method, "()I", &e)) {
        if (result == NULL)
            failure(&e, MC_CLIENT_SDK_UPDATE_INVALID_ARGUMENT, "Display native extent output",
                    method, "()I", -1);
        if (out != NULL)
            *out = e;
        return e.status;
    }
    /* This actual windowed/null-parent branch reads source width/height26;
     * no created guard or App/native pixel substitution is inserted. */
    *result = width ? o->width : o->height;
    return leave(o, &e, out);
}

mc_client_sdk_update_status mc_client_sdk_display_width(mc_client_sdk_update *o, int32_t *result,
                                                        mc_client_sdk_update_error *e) {
    return source_extent(o, true, result, e);
}

mc_client_sdk_update_status mc_client_sdk_display_height(mc_client_sdk_update *o, int32_t *result,
                                                         mc_client_sdk_update_error *e) {
    return source_extent(o, false, result, e);
}

mc_client_sdk_update_status mc_client_sdk_display_update_messages(mc_client_sdk_update *o,
                                                                  bool process,
                                                                  mc_client_sdk_update_error *out) {
    mc_client_sdk_update_error e = {0};
    if (!enter(o, "update", "(Z)V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (!mc_client_display_sdk_created(o->display)) {
        failure(&e, MC_CLIENT_SDK_UPDATE_SOURCE_ILLEGAL_STATE, "actual uncreated Display.update",
                "update", "(Z)V", 12);
        goto done;
    }
    /* Both original WindowsDisplay/LinuxDisplay.isVisible return !minimized.
     * Native hidden is a test/window option, not a fabricated Source minimum. */
    const bool visible =
        (SDL_GetWindowFlags(mc_client_display_window(o->display)) & SDL_WINDOW_MINIMIZED) == 0;
    const bool swap =
        visible || mc_client_events_take_dirty(o->events); /*25 then36 only when needed*/
    if (swap) {
        if (!mc_client_display_sdk_swap(o->display, &e.swapped, &e.display)) {
            /* A native SDL/GL failure is not manufactured LWJGLException or
             * its caught RuntimeException56. The completed prefix is retained. */
            failure(&e, MC_CLIENT_SDK_UPDATE_PLATFORM_FAILED,
                    "actual conditional Display.swapBuffers", "update", "(Z)V", 44);
            goto done;
        }
    }
    if ((SDL_GetWindowFlags(mc_client_display_window(o->display)) & SDL_WINDOW_FULLSCREEN) != 0) {
        failure(&e, MC_CLIENT_SDK_UPDATE_UNSUPPORTED_PROFILE,
                "foreign native fullscreen outside the actual source windowed profile", "update",
                "(Z)V", 60);
        goto done;
    }
    /* This finite profile is real windowed/null-parent creation. Unsupported
     * fullscreen/AWT mutation never enters through a fabricated source flag. */
    bool resized = false;
    if (!mc_client_display_sdk_resized(o->display, &resized, &e.display)) {
        failure(&e, MC_CLIENT_SDK_UPDATE_PLATFORM_FAILED, "actual native wasResized", "update",
                "(Z)V", 75);
        goto done;
    }
    o->window_resized = resized;
    o->last_store = 88;
    if (resized) {
        int32_t value;
        if (!mc_client_display_sdk_extent(o->display, true, &value, &e.display)) {
            failure(&e, MC_CLIENT_SDK_UPDATE_PLATFORM_FAILED, "actual native width", "update",
                    "(Z)V", 100);
            goto done;
        }
        o->width = value;
        o->last_store = 105;
        e.width_stored = true;
        if (!mc_client_display_sdk_extent(o->display, false, &value, &e.display)) {
            failure(&e, MC_CLIENT_SDK_UPDATE_PLATFORM_FAILED, "actual native height", "update",
                    "(Z)V", 111);
            goto done;
        }
        o->height = value;
        o->last_store = 116;
        e.height_stored = true;
    }
    if (process && !messages(o, &e))
        caller(&e, "update", "(Z)V", 140);
done:
    return leave(o, &e, out);
}

mc_client_sdk_update_status mc_client_sdk_display_update(mc_client_sdk_update *o,
                                                         mc_client_sdk_update_error *out) {
    mc_client_sdk_update_error e = {0};
    const mc_client_sdk_update_status status = mc_client_sdk_display_update_messages(o, true, &e);
    if (status != MC_CLIENT_SDK_UPDATE_OK) {
        caller(&e, "update", "()V", 1);
        if (o != NULL)
            o->failure = e;
    }
    if (out != NULL)
        *out = e;
    return status;
}

bool mc_client_sdk_update_inspect(const mc_client_sdk_update *o, mc_client_sdk_update_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_sdk_update_state){o->active, o->window_resized, o->failed,  o->width,
                                        o->height, o->last_store,     &o->failure};
    return true;
}

mc_client_sdk_update_status mc_client_sdk_update_retire(mc_client_sdk_update **slot,
                                                        mc_client_sdk_update_error *out) {
    mc_client_sdk_update_error e = {0};
    if (slot == NULL || !SDL_IsMainThread())
        failure(&e, MC_CLIENT_SDK_UPDATE_INVALID_ARGUMENT, "Display update native retirement", NULL,
                NULL, -1);
    else if (*slot != NULL && (*slot)->active)
        failure(&e, MC_CLIENT_SDK_UPDATE_BUSY, "Display active update retirement", NULL, NULL, -1);
    else if (*slot != NULL) {
        free(*slot);
        *slot = NULL;
    }
    if (out != NULL)
        *out = e;
    return e.status;
}
