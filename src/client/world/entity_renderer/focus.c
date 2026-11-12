#include "mc/client/world/renderer_focus.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../../application_state.h"
#include "internal.h"
#include "mc/entity/player/mouse_filter.h"

#include <stdlib.h>
#include <string.h>

struct mc_client_entity_renderer_focus_call {
    mc_client_renderer_focus_state state;
    mc_client_renderer_focus_error failure;
    bool owner_held;
};

static mc_client_renderer_focus_status result(mc_client_renderer_focus_error *error,
                                              mc_client_renderer_focus_status status,
                                              const char *operation) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->status = status;
        error->operation = operation;
        error->native_policy = status != MC_CLIENT_RENDERER_FOCUS_OK;
        error->site.pc = error->child.pc = -1;
    }
    return status;
}

static bool fail(mc_client_entity_renderer_focus_call *call, mc_client_renderer_focus_status status,
                 const char *operation, unsigned int pc, bool native) {
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.site = (mc_client_graphics_site){"px", "b", "(F)V", (int32_t)pc};
    call->failure.native_policy = native;
    call->state.source_pc = pc;
    return false;
}

static client_application *app_at(mc_client_entity_renderer_focus_call *call, unsigned int pc) {
    client_application *app = call->state.owner->fields.j;
    if (app == NULL)
        fail(call, MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL, "actual PX focus null App receiver", pc,
             false);
    return app;
}

static bool settings_at(mc_client_entity_renderer_focus_call *call, unsigned int app_pc,
                        unsigned int pc, mc_game_settings_state *out) {
    client_application *app = app_at(call, app_pc);
    if (app == NULL)
        return false;
    if (app->settings == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL,
                    "actual PX focus null Settings receiver", pc, false);
    if (!mc_game_settings_read_state(app->settings, out))
        return fail(call, MC_CLIENT_RENDERER_FOCUS_NATIVE_FAILED,
                    "PX focus canonical Settings fields", pc, true);
    return true;
}

static float converted(int32_t value) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

static bool motion_fields(mc_client_entity_renderer_focus_call *call, unsigned int app_pc,
                          unsigned int pc, bool y, int32_t *out) {
    client_application *app = app_at(call, app_pc);
    if (app == NULL)
        return false;
    if (app->C == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL,
                    "actual PX focus null MouseHelper fields", pc, false);
    mc_client_mouse_helper_state state;
    if (!mc_client_mouse_helper_inspect(app->C, &state))
        return fail(call, MC_CLIENT_RENDERER_FOCUS_NATIVE_FAILED,
                    "PX focus canonical MouseHelper fields", pc, true);
    *out = y ? state.source_b : state.source_a;
    return true;
}

static bool filter(mc_client_entity_renderer_focus_call *call, size_t index, unsigned int pc,
                   float factor, float *value) {
    mc_player_mouse_filter_fields *receiver = call->state.owner->fields.filters[index];
    if (receiver == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL, "actual PX focus null CU receiver",
                    pc, false);
    call->failure.entity = mc_player_mouse_filter_apply(receiver, *value, factor, value);
    if (call->failure.entity != MC_ENTITY_OK) {
        call->failure.child = (mc_client_graphics_site){"cu", "a", "(FF)F", 0};
        return fail(call, MC_CLIENT_RENDERER_FOCUS_CHILD_FAILED, "PX focus actual CU filter", pc,
                    true);
    }
    return true;
}

static bool body(mc_client_entity_renderer_focus_call *call, mc_client_sdk_input *input) {
    mc_client_renderer_focus_state *s = &call->state;
    client_application *app = app_at(call, 42);
    if (app == NULL)
        return false;
    s->focused = app->N;
    if (!s->focused) {
        s->segment_completed = true;
        s->source_pc = 194;
        return true;
    }
    app = app_at(call, 52);
    if (app == NULL)
        return false;
    if (app->C == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL,
                    "actual PX focus null MouseHelper receiver", 55, false);
    s->motion_called = true;
    const mc_client_mouse_helper_status motion =
        mc_client_mouse_helper_read_motion(app->C, input, &call->failure.mouse);
    if (motion != MC_CLIENT_MOUSE_HELPER_OK) {
        call->failure.child = call->failure.mouse.site;
        return fail(call, MC_CLIENT_RENDERER_FOCUS_CHILD_FAILED, call->failure.mouse.operation, 55,
                    call->failure.mouse.native_policy);
    }
    s->motion_returned = true;
    mc_game_settings_state settings;
    if (!settings_at(call, 62, 65, &settings))
        return false;
    memcpy(&s->sensitivity, &settings.values.mouse_sensitivity, sizeof(float));
    s->sensitivity_read = true;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(call, MC_CLIENT_RENDERER_FOCUS_UNSUPPORTED_FP,
                    "PX focus native floating environment", 70, true);
    s->scale = at_fadd(at_fmul(s->sensitivity, 0.6f), 0.2f);
    s->gain = at_fmul(at_fmul(at_fmul(s->scale, s->scale), s->scale), 8.0f);
    if (!motion_fields(call, 88, 91, false, &s->dx))
        return false;
    s->yaw_delta = at_fmul(converted(s->dx), s->gain);
    if (!motion_fields(call, 103, 106, true, &s->dy))
        return false;
    s->pitch_delta = at_fmul(converted(s->dy), s->gain);
    if (!settings_at(call, 121, 124, &settings))
        return false;
    s->invert_read = true;
    s->inverted = settings.values.invert_mouse;
    if (!settings_at(call, 137, 140, &settings))
        return false;
    s->smooth_read = true;
    s->smooth = settings.values.smooth_camera;
    if (s->smooth) {
        if (!filter(call, 0, 156, at_fmul(0.05f, s->gain), &s->yaw_delta))
            return false;
        s->n_returned = true;
        if (!filter(call, 1, 171, at_fmul(0.05f, s->gain), &s->pitch_delta))
            return false;
        s->o_returned = true;
    }
    app = app_at(call, 180);
    if (app == NULL)
        return false;
    mc_entity *player = app->h; /* JVM captures receiver before the final multiply. */
    s->pitch_delta = at_fmul(s->pitch_delta, s->inverted ? -1.0f : 1.0f);
    if (player == NULL)
        return fail(call, MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL,
                    "actual PX focus null Player turn receiver", 191, false);
    s->turn_called = true;
    call->failure.entity = mc_entity_turn(player, s->yaw_delta, s->pitch_delta);
    if (call->failure.entity != MC_ENTITY_OK) {
        call->failure.child = (mc_client_graphics_site){"sn", "d", "(FF)V", 0};
        return fail(call, MC_CLIENT_RENDERER_FOCUS_CHILD_FAILED,
                    "PX focus actual inherited Player turn", 191, true);
    }
    s->turn_returned = true;
    s->segment_completed = true;
    s->source_pc = 194;
    return true;
}

mc_client_renderer_focus_status
mc_client_entity_renderer_focus_begin(mc_client_source_px *owner, mc_client_sdk_input *input,
                                      mc_client_entity_renderer_focus_call **out,
                                      mc_client_renderer_focus_error *error) {
    if (out == NULL || *out != NULL)
        return result(error, MC_CLIENT_RENDERER_FOCUS_ARGUMENT, "PX focus empty output slot");
    if (owner != NULL && (owner->active || owner->focus_call != NULL ||
                          owner->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING ||
                          owner->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING))
        return result(error, MC_CLIENT_RENDERER_FOCUS_BUSY, "PX focus active or pending call");
    mc_client_entity_renderer_focus_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return result(error, MC_CLIENT_RENDERER_FOCUS_ALLOCATION, "PX focus diagnostic allocation");
    call->state.owner = owner;
    call->state.failure = &call->failure;
    call->state.active = true;
    *out = call;
    if (owner == NULL)
        fail(call, MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL, "actual PX focus null receiver", 39,
             false);
    else if (mc_client_entity_renderer_retain(owner, &call->failure.owner) !=
             MC_CLIENT_ENTITY_RENDERER_OK)
        fail(call, MC_CLIENT_RENDERER_FOCUS_NATIVE_FAILED, "PX focus native occurrence", 38, true);
    else {
        call->owner_held = true;
        owner->focus_call = call;
        owner->active = true;
        (void)body(call, input);
        owner->active = false;
    }
    call->state.active = false;
    if (error != NULL)
        *error = call->failure;
    return call->failure.status;
}

bool mc_client_entity_renderer_focus_inspect(const mc_client_entity_renderer_focus_call *call,
                                             mc_client_renderer_focus_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    return true;
}

mc_client_renderer_focus_status
mc_client_entity_renderer_focus_retire(mc_client_entity_renderer_focus_call **slot,
                                       mc_client_renderer_focus_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_RENDERER_FOCUS_ARGUMENT, "PX focus retirement slot");
    mc_client_entity_renderer_focus_call *call = *slot;
    if (call == NULL)
        return result(error, MC_CLIENT_RENDERER_FOCUS_OK, NULL);
    if (call->state.active || (call->state.owner != NULL && call->state.owner->active))
        return result(error, MC_CLIENT_RENDERER_FOCUS_BUSY, "PX focus active retirement");
    if (call->owner_held) {
        mc_client_source_px *owner = call->state.owner;
        mc_client_entity_renderer_error cleanup_owner;
        owner->focus_call = NULL;
        if (mc_client_entity_renderer_release(&owner, &cleanup_owner) !=
            MC_CLIENT_ENTITY_RENDERER_OK) {
            call->state.owner->focus_call = call;
            const mc_client_renderer_focus_status status =
                result(error, MC_CLIENT_RENDERER_FOCUS_BUSY, "PX focus owner retirement");
            if (error != NULL)
                error->owner = cleanup_owner;
            return status;
        }
    }
    free(call);
    *slot = NULL;
    return result(error, MC_CLIENT_RENDERER_FOCUS_OK, NULL);
}
