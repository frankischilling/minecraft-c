#include "mouse_helper.h"

#include <stdlib.h>
#include <string.h>

#define VY_CTOR "(Ljava/awt/Component;)V"

struct mc_client_source_vy {
    mc_client_render_engine_classes *classes;
    mc_client_render_arena arena;
    mc_client_mouse_helper_options options;
    mc_client_mouse_helper_phase phase;
    size_t holds;
    bool active, c_stored, e_stored, d_stored;
    int32_t a, b, e, last_store, pending_pc;
    mc_client_source_canvas *c;
    mc_client_sdk_cursor *d, *candidate;
    mc_client_direct_int_buffer *delays, *images;
    mc_client_mouse_helper_error failure;
    mc_client_sdk_input *input; /* Native borrow protected by the owned Cursor's Display lease. */
    const char *last_method;
};

static mc_client_mouse_helper_status fail(mc_client_mouse_helper_error *error,
                                          mc_client_mouse_helper_status status,
                                          const char *operation, int32_t pc, bool native) {
    error->status = status;
    error->operation = operation;
    error->site = (mc_client_graphics_site){"vy", "<init>", VY_CTOR, pc};
    error->native_policy = native;
    return status;
}

static mc_client_mouse_helper_status publish(mc_client_mouse_helper_error *error,
                                             mc_client_mouse_helper_error *out) {
    if (out != NULL)
        *out = *error;
    return error->status;
}

mc_client_mouse_helper_options mc_client_mouse_helper_default_options(void) {
    return (mc_client_mouse_helper_options){65536u, 32u, mc_client_sdk_cursor_default_limits()};
}

mc_client_mouse_helper_status
mc_client_mouse_helper_allocate(mc_client_render_engine_classes *classes,
                                const mc_client_mouse_helper_options *options,
                                mc_client_source_vy **out, mc_client_mouse_helper_error *error) {
    mc_client_mouse_helper_error e = {0};
    const mc_client_mouse_helper_options bounds =
        options == NULL ? mc_client_mouse_helper_default_options() : *options;
    if (classes == NULL || out == NULL) {
        fail(&e, MC_CLIENT_MOUSE_HELPER_INVALID_ARGUMENT, "native vy allocation arguments", -1,
             true);
    } else if (bounds.max_bytes < sizeof(mc_client_source_vy) || bounds.max_allocations == 0) {
        fail(&e, MC_CLIENT_MOUSE_HELPER_LIMIT, "native vy owner bound", -1, true);
    } else {
        mc_client_source_vy *owner = calloc(1, sizeof(*owner));
        if (owner == NULL) {
            fail(&e, MC_CLIENT_MOUSE_HELPER_ALLOCATION_FAILED, "native vy RAW allocation", -1,
                 true);
        } else {
            e.classes = mc_client_render_engine_classes_retain(classes);
            if (e.classes != MC_CLIENT_RENDER_ENGINE_OK) {
                free(owner);
                fail(&e, MC_CLIENT_MOUSE_HELPER_GRAPHICS_FAILED, "vy actual class lifetime hold",
                     -1, true);
            } else {
                owner->classes = classes;
                owner->options = bounds;
                owner->holds = 1;
                owner->last_store = owner->pending_pc = -1;
                owner->arena.options =
                    (mc_client_render_engine_options){bounds.max_bytes, bounds.max_allocations};
                mc_client_render_arena_commit_external(&owner->arena, sizeof(*owner));
                *out = owner;
            }
        }
    }
    return publish(&e, error);
}

mc_client_mouse_helper_status mc_client_mouse_helper_construct(mc_client_source_vy *owner,
                                                               mc_client_source_canvas *component,
                                                               mc_client_display *display,
                                                               mc_client_sdk_input *input,
                                                               mc_client_mouse_helper_error *out) {
    mc_client_mouse_helper_error e = {0};
    if (owner == NULL || owner->active || owner->phase != MC_CLIENT_MOUSE_HELPER_RAW) {
        fail(&e,
             owner != NULL && owner->active ? MC_CLIENT_MOUSE_HELPER_BUSY
                                            : MC_CLIENT_MOUSE_HELPER_INVALID_PHASE,
             "native vy one-shot constructor admission", -1, true);
        return publish(&e, out);
    }
    owner->active = true;
    owner->phase = MC_CLIENT_MOUSE_HELPER_CONSTRUCTING;
    owner->input = input;
    owner->last_method = "<init>";
    owner->pending_pc = 1; /* Actual empty Object constructor. */
    owner->e = 10;
    owner->e_stored = true;
    owner->last_store = 7;
    owner->c = component;
    owner->c_stored = true;
    owner->last_store = 12;
    owner->pending_pc = 16;
    if (!mc_client_ge_d(owner->classes, &owner->arena, 1,
                        (mc_client_graphics_site){"vy", "<init>", VY_CTOR, 16}, &owner->delays,
                        &e.graphics))
        goto graphics_failed;
    owner->pending_pc = 22;
    if (!mc_client_ge_int_put(owner->delays, 0,
                              (mc_client_graphics_site){"vy", "<init>", VY_CTOR, 22}, &e.graphics))
        goto graphics_failed;
    owner->pending_pc = 28;
    if (!mc_client_ge_int_flip(owner->delays,
                               (mc_client_graphics_site){"vy", "<init>", VY_CTOR, 28}, &e.graphics))
        goto graphics_failed;
    owner->pending_pc = 34;
    if (!mc_client_ge_d(owner->classes, &owner->arena, 1024,
                        (mc_client_graphics_site){"vy", "<init>", VY_CTOR, 34}, &owner->images,
                        &e.graphics))
        goto graphics_failed;
    owner->pending_pc = 39;
    if (mc_client_sdk_cursor_allocate(display, &owner->options.cursor, &owner->candidate,
                                      &e.cursor) != MC_CLIENT_SDK_CURSOR_OK)
        goto cursor_failed;
    owner->pending_pc = 54;
    if (mc_client_sdk_cursor_construct32(owner->candidate, input, owner->images, owner->delays,
                                         &e.cursor) != MC_CLIENT_SDK_CURSOR_OK) {
        if (e.cursor.status == MC_CLIENT_SDK_CURSOR_SOURCE_EXCEPTION &&
            e.cursor.exception_class != NULL &&
            strcmp(e.cursor.exception_class, "org/lwjgl/LWJGLException") == 0) {
            owner->pending_pc = 67;
            fail(&e, MC_CLIENT_MOUSE_HELPER_UNIMPLEMENTED_DEPENDENCY,
                 "actual vy caught LWJGLException.printStackTrace", 67, false);
            e.exception_class = e.cursor.exception_class;
            e.diagnostic_frontier = true;
            goto done;
        }
        goto cursor_failed;
    }
    owner->d = owner->candidate;
    owner->candidate = NULL;
    owner->d_stored = true;
    owner->last_store = 57;
    owner->pending_pc = 70;
    goto done;
graphics_failed:
    fail(&e,
         e.graphics.status == MC_CLIENT_GRAPHICS_LIMIT ? MC_CLIENT_MOUSE_HELPER_LIMIT
                                                       : MC_CLIENT_MOUSE_HELPER_GRAPHICS_FAILED,
         "actual vy ge/direct-buffer child", owner->pending_pc, e.graphics.native_policy);
    goto done;
cursor_failed:
    fail(&e,
         e.cursor.status == MC_CLIENT_SDK_CURSOR_SOURCE_EXCEPTION
             ? MC_CLIENT_MOUSE_HELPER_SOURCE_EXCEPTION
         : e.cursor.status == MC_CLIENT_SDK_CURSOR_BUSY  ? MC_CLIENT_MOUSE_HELPER_BUSY
         : e.cursor.status == MC_CLIENT_SDK_CURSOR_LIMIT ? MC_CLIENT_MOUSE_HELPER_LIMIT
         : e.cursor.status == MC_CLIENT_SDK_CURSOR_ALLOCATION_FAILED
             ? MC_CLIENT_MOUSE_HELPER_ALLOCATION_FAILED
             : MC_CLIENT_MOUSE_HELPER_CURSOR_FAILED,
         "actual vy native Cursor child", owner->pending_pc, e.cursor.native_policy);
    e.exception_class = e.cursor.exception_class;
done:
    owner->phase = e.status == MC_CLIENT_MOUSE_HELPER_OK ? MC_CLIENT_MOUSE_HELPER_READY
                                                         : MC_CLIENT_MOUSE_HELPER_FAILED;
    owner->active = false;
    if (e.status != MC_CLIENT_MOUSE_HELPER_OK)
        owner->failure = e;
    return publish(&e, out);
}

static bool method_enter(mc_client_source_vy *owner, const char *method,
                         mc_client_mouse_helper_error *error) {
    error->site = (mc_client_graphics_site){"vy", method, "()V", -1};
    if (owner == NULL || owner->active || owner->phase != MC_CLIENT_MOUSE_HELPER_READY) {
        error->status = owner == NULL   ? MC_CLIENT_MOUSE_HELPER_INVALID_ARGUMENT
                        : owner->active ? MC_CLIENT_MOUSE_HELPER_BUSY
                                        : MC_CLIENT_MOUSE_HELPER_INVALID_PHASE;
        error->operation = "native vy READY method admission";
        error->native_policy = true;
        return false;
    }
    owner->active = true;
    owner->last_method = method;
    return true;
}

static mc_client_mouse_helper_status method_leave(mc_client_source_vy *owner,
                                                  mc_client_mouse_helper_error *error,
                                                  mc_client_mouse_helper_error *out) {
    owner->active = false;
    if (error->status != MC_CLIENT_MOUSE_HELPER_OK)
        owner->failure = *error;
    return publish(error, out);
}

static bool method_input(mc_client_source_vy *owner, mc_client_sdk_input *input,
                         mc_client_mouse_helper_error *error, int32_t pc) {
    error->site.pc = pc;
    if (input != NULL && input == owner->input)
        return true;
    error->status = MC_CLIENT_MOUSE_HELPER_INVALID_ARGUMENT;
    error->operation = "native vy same canonical SDK input";
    error->native_policy = true;
    return false;
}

static void input_failed(mc_client_mouse_helper_error *error) {
    error->status = error->input.status == MC_CLIENT_SDK_INPUT_BUSY ? MC_CLIENT_MOUSE_HELPER_BUSY
                    : error->input.status == MC_CLIENT_SDK_INPUT_SOURCE_ILLEGAL_STATE ||
                            error->input.status == MC_CLIENT_SDK_INPUT_SOURCE_INDEX
                        ? MC_CLIENT_MOUSE_HELPER_SOURCE_EXCEPTION
                        : MC_CLIENT_MOUSE_HELPER_INPUT_FAILED;
    error->operation = error->input.operation;
    error->native_policy = error->input.native_policy;
    error->child = (mc_client_graphics_site){error->input.site.class_name, error->input.site.method,
                                             error->input.site.descriptor, error->input.site.pc};
}

mc_client_mouse_helper_status mc_client_mouse_helper_grab(mc_client_source_vy *owner,
                                                          mc_client_sdk_input *input,
                                                          mc_client_mouse_helper_error *out) {
    mc_client_mouse_helper_error error = {0};
    if (!method_enter(owner, "a", &error))
        return publish(&error, out);
    if (!method_input(owner, input, &error, 1))
        return method_leave(owner, &error, out);
    if (mc_client_sdk_mouse_set_grabbed(input, true, &error.input) != MC_CLIENT_SDK_INPUT_OK) {
        input_failed(&error);
        return method_leave(owner, &error, out);
    }
    owner->a = 0;
    owner->last_store = 6;
    owner->b = 0;
    owner->last_store = 11;
    error.site.pc = 14;
    error.source_returned = true;
    return method_leave(owner, &error, out);
}

mc_client_mouse_helper_status
mc_client_mouse_helper_read_motion(mc_client_source_vy *owner, mc_client_sdk_input *input,
                                   mc_client_mouse_helper_error *out) {
    mc_client_mouse_helper_error error = {0};
    if (!method_enter(owner, "c", &error))
        return publish(&error, out);
    if (!method_input(owner, input, &error, 1))
        return method_leave(owner, &error, out);
    int32_t dx;
    if (mc_client_sdk_mouse_take_dx(input, &dx, &error.input) != MC_CLIENT_SDK_INPUT_OK) {
        input_failed(&error);
        return method_leave(owner, &error, out);
    }
    owner->a = dx;
    owner->last_store = 4;
    error.site.pc = 8;
    int32_t dy;
    if (mc_client_sdk_mouse_take_dy(input, &dy, &error.input) != MC_CLIENT_SDK_INPUT_OK) {
        input_failed(&error);
        return method_leave(owner, &error, out);
    }
    owner->b = dy;
    owner->last_store = 11;
    error.site.pc = 14;
    error.source_returned = true;
    return method_leave(owner, &error, out);
}

mc_client_mouse_helper_status mc_client_mouse_helper_ungrab(mc_client_source_vy *owner,
                                                            mc_client_sdk_input *input,
                                                            mc_client_mouse_helper_error *out) {
    (void)input; /* No Mouse call precedes the reached Component.getWidth. */
    mc_client_mouse_helper_error error = {0};
    if (!method_enter(owner, "b", &error))
        return publish(&error, out);
    error.site.pc = 4;
    error.child = (mc_client_graphics_site){"java/awt/Component", "getWidth", "()I", 0};
    if (owner->c == NULL) {
        error.status = MC_CLIENT_MOUSE_HELPER_NULL_REFERENCE;
        error.operation = "actual vy Component.getWidth null receiver";
        error.exception_class = "java/lang/NullPointerException";
    } else {
        error.status = MC_CLIENT_MOUSE_HELPER_UNIMPLEMENTED_DEPENDENCY;
        error.operation = "actual Component.getWidth owner dependency";
        error.native_policy = true;
    }
    return method_leave(owner, &error, out);
}

mc_client_mouse_helper_status mc_client_mouse_helper_retain(mc_client_source_vy *owner,
                                                            mc_client_mouse_helper_error *error) {
    mc_client_mouse_helper_error e = {0};
    if (owner == NULL)
        fail(&e, MC_CLIENT_MOUSE_HELPER_INVALID_ARGUMENT, "native vy retain", -1, true);
    else if (owner->active || owner->phase == MC_CLIENT_MOUSE_HELPER_RETIRING)
        fail(&e, MC_CLIENT_MOUSE_HELPER_BUSY, "native vy active/retiring hold", -1, true);
    else if (owner->holds == SIZE_MAX)
        fail(&e, MC_CLIENT_MOUSE_HELPER_LIMIT, "native vy holds", -1, true);
    else
        ++owner->holds;
    return publish(&e, error);
}

mc_client_mouse_helper_status mc_client_mouse_helper_release(mc_client_source_vy **slot,
                                                             mc_client_mouse_helper_error *error) {
    mc_client_mouse_helper_error e = {0};
    if (slot == NULL) {
        fail(&e, MC_CLIENT_MOUSE_HELPER_INVALID_ARGUMENT, "native vy release slot", -1, true);
    } else if (*slot != NULL) {
        mc_client_source_vy *owner = *slot;
        if (owner->active) {
            fail(&e, MC_CLIENT_MOUSE_HELPER_BUSY, "native vy active release", -1, true);
        } else if (owner->holds > 1) {
            --owner->holds;
            *slot = NULL;
        } else {
            owner->active = true;
            owner->phase = MC_CLIENT_MOUSE_HELPER_RETIRING;
            const mc_client_sdk_cursor_status first =
                mc_client_sdk_cursor_release(&owner->candidate, &e.cursor);
            const mc_client_sdk_cursor_status second =
                first == MC_CLIENT_SDK_CURSOR_OK
                    ? mc_client_sdk_cursor_release(&owner->d, &e.cursor)
                    : first;
            if (second != MC_CLIENT_SDK_CURSOR_OK) {
                fail(&e,
                     second == MC_CLIENT_SDK_CURSOR_BUSY ? MC_CLIENT_MOUSE_HELPER_BUSY
                                                         : MC_CLIENT_MOUSE_HELPER_CURSOR_FAILED,
                     "native vy cursor retirement", -1, true);
            } else {
                e.classes = mc_client_render_engine_classes_release(owner->classes);
                if (e.classes != MC_CLIENT_RENDER_ENGINE_OK) {
                    fail(&e,
                         e.classes == MC_CLIENT_RENDER_ENGINE_BUSY
                             ? MC_CLIENT_MOUSE_HELPER_BUSY
                             : MC_CLIENT_MOUSE_HELPER_GRAPHICS_FAILED,
                         "native vy class lifetime retirement", -1, true);
                } else {
                    owner->classes = NULL;
                    mc_client_render_arena_free(&owner->arena);
                    free(owner);
                    *slot = NULL;
                }
            }
            if (*slot != NULL) {
                owner->active = false;
                owner->failure = e;
            }
        }
    }
    return publish(&e, error);
}

bool mc_client_mouse_helper_inspect(const mc_client_source_vy *owner,
                                    mc_client_mouse_helper_state *out) {
    if (owner == NULL || out == NULL)
        return false;
    *out = (mc_client_mouse_helper_state){.phase = owner->phase,
                                          .holds = owner->holds,
                                          .native_bytes = owner->arena.bytes,
                                          .native_allocations = owner->arena.allocations,
                                          .active = owner->active,
                                          .source_c_stored = owner->c_stored,
                                          .source_e_stored = owner->e_stored,
                                          .source_d_stored = owner->d_stored,
                                          .source_a = owner->a,
                                          .source_b = owner->b,
                                          .source_e = owner->e,
                                          .last_store_pc = owner->last_store,
                                          .pending_pc = owner->pending_pc,
                                          .source_c = owner->c,
                                          .source_d = owner->d,
                                          .cursor_candidate = owner->candidate,
                                          .delay_buffer = owner->delays,
                                          .image_buffer = owner->images,
                                          .failure = &owner->failure,
                                          .last_method = owner->last_method};
    return true;
}
