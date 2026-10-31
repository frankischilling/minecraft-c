#include "../../application_state.h"
#include "../../display/camera_surface.h"
#include "first_person_hand_internal.h"
#include "held_item_draw_internal.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include "renderer_camera_internal.h"

#include <emmintrin.h>
#include <stdlib.h>

struct mc_client_first_person_hand_call {
    mc_client_first_person_hand_state state;
    mc_client_first_person_hand_error failure;
    mc_client_renderer_camera_call *effects[4];
    mc_client_renderer_camera_state effect_states[4];
    mc_client_renderer_camera_error effect_errors[4], effect_cleanup[4];
    mc_client_held_item_draw_call *held, *overlays;
    mc_client_held_item_draw_state held_state, overlays_state;
    mc_client_held_item_draw_error held_error, overlays_error, held_cleanup, overlays_cleanup;
    mc_entity *sleep_captures[2];
};

static mc_client_first_person_hand_status result(mc_client_first_person_hand_error *error,
                                                 mc_client_first_person_hand_status status,
                                                 const char *operation) {
    if (error != NULL)
        *error = (mc_client_first_person_hand_error){.status = status,
                                                     .operation = operation,
                                                     .native_policy =
                                                         status != MC_CLIENT_FIRST_PERSON_HAND_OK};
    return status;
}

static bool fail(mc_client_first_person_hand_call *call, mc_client_first_person_hand_status status,
                 const char *operation, unsigned int pc, bool native) {
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.site = (mc_client_graphics_site){"px", "b", "(FI)V", (int32_t)pc};
    call->failure.native_policy = native;
    call->state.source_pc = pc;
    return false;
}

static bool display_result(mc_client_first_person_hand_call *call, bool ok, unsigned int pc) {
    if (!ok)
        return fail(call, MC_CLIENT_FIRST_PERSON_HAND_NATIVE_FAILED,
                    call->failure.display.operation, pc, true);
    ++call->state.transforms;
    return true;
}

static bool settings_at(mc_client_first_person_hand_call *call, unsigned int app_pc,
                        unsigned int settings_pc, mc_game_settings_state *out) {
    client_application *app = call->state.owner->fields.j;
    if (app == NULL)
        return fail(call, MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL, "actual hand-pass App receiver",
                    app_pc, false);
    return mc_game_settings_read_state(app->settings, out) ||
           fail(call,
                app->settings == NULL ? MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL
                                      : MC_CLIENT_FIRST_PERSON_HAND_NATIVE_FAILED,
                "actual hand-pass Settings fields", settings_pc, app->settings != NULL);
}

static bool sleeping_at(mc_client_first_person_hand_call *call, bool second, bool *out) {
    const unsigned int app_pc = second ? 123u : 73u, invoke_pc = second ? 126u : 76u;
    client_application *app = call->state.owner->fields.j;
    if (app == NULL)
        return fail(call, MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL,
                    "actual hand-pass sleeping App receiver", app_pc, false);
    mc_entity *camera = app->i;
    if (camera == NULL)
        return fail(call, MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL,
                    "actual hand-pass sleeping camera receiver", invoke_pc, false);
    call->failure.entity = mc_entity_retain(camera);
    if (call->failure.entity != MC_ENTITY_OK)
        return fail(call, MC_CLIENT_FIRST_PERSON_HAND_NATIVE_FAILED,
                    "hand-pass sleeping camera occurrence", invoke_pc, true);
    call->sleep_captures[second ? 1 : 0] = camera;
    if (mc_player_is_sleeping(camera, out, &call->failure.player) != MC_PLAYER_OK) {
        call->failure.child = (mc_client_graphics_site){
            call->failure.player.site.class_name, call->failure.player.site.method,
            call->failure.player.site.descriptor, (int32_t)call->failure.player.site.pc};
        return fail(call, MC_CLIENT_FIRST_PERSON_HAND_CHILD_FAILED, call->failure.player.operation,
                    invoke_pc, call->failure.player.status != MC_PLAYER_NULL_REFERENCE);
    }
    if (second) {
        call->state.second_sleep_read = true;
        call->state.second_sleeping = *out;
    } else {
        call->state.first_sleep_read = true;
        call->state.first_sleeping = *out;
    }
    return true;
}

static bool effect(mc_client_first_person_hand_call *call, mc_math_classes *math,
                   unsigned int index, bool bob, unsigned int pc) {
    const mc_client_renderer_camera_status status =
        entity_renderer_camera_effect_entered(call->state.owner, math, call->state.partial, bob,
                                              &call->effects[index], &call->effect_errors[index]);
    if (call->effects[index] != NULL) {
        (void)mc_client_entity_renderer_camera_inspect(call->effects[index],
                                                       &call->effect_states[index]);
        call->state.effects[index] = &call->effect_states[index];
    }
    if (status != MC_CLIENT_RENDERER_CAMERA_OK) {
        const mc_client_renderer_camera_error *error = call->effects[index] == NULL
                                                           ? &call->effect_errors[index]
                                                           : call->effect_states[index].error;
        call->failure.camera = error;
        call->failure.child = error->site;
        return fail(call,
                    status == MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL
                        ? MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL
                    : status == MC_CLIENT_RENDERER_CAMERA_UNSUPPORTED_FP
                        ? MC_CLIENT_FIRST_PERSON_HAND_UNSUPPORTED_FP
                    : status == MC_CLIENT_RENDERER_CAMERA_DEPENDENCY
                        ? MC_CLIENT_FIRST_PERSON_HAND_DEPENDENCY
                        : MC_CLIENT_FIRST_PERSON_HAND_CHILD_FAILED,
                    error->operation, pc, error->native_policy);
    }
    call->state.transforms += call->effect_states[index].transforms;
    if (bob)
        ++call->state.bob_calls;
    else
        ++call->state.hurt_calls;
    return true;
}

static bool held(mc_client_first_person_hand_call *call, mc_math_classes *math, bool overlays) {
    mc_client_source_px *owner = call->state.owner;
    const unsigned int pc = overlays ? 137u : 100u;
    if (owner->fields.c == NULL)
        return fail(call, MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL,
                    "actual hand-pass held renderer receiver", pc, false);
    if (owner->pending_ra == NULL || owner->pending_ra->source != owner->fields.c)
        return fail(call, MC_CLIENT_FIRST_PERSON_HAND_DEPENDENCY,
                    "actual hand-pass held renderer occurrence", pc, true);
    mc_client_held_item_draw_call **slot = overlays ? &call->overlays : &call->held;
    mc_client_held_item_draw_state *state = overlays ? &call->overlays_state : &call->held_state;
    mc_client_held_item_draw_error *error = overlays ? &call->overlays_error : &call->held_error;
    /* The first RA call remains retained through the parent. Its completed
     * native occurrence must retire before the later call on the same RA. */
    if (overlays && call->held != NULL) {
        if (mc_client_held_item_draw_retire(&call->held, &call->held_cleanup) !=
            MC_CLIENT_HELD_ITEM_DRAW_OK) {
            call->failure.held_item = &call->held_cleanup;
            return fail(call, MC_CLIENT_FIRST_PERSON_HAND_BUSY,
                        "completed held draw native cleanup", pc, true);
        }
        call->held_state.owner = NULL;
        call->held_state.player = NULL;
        call->held_state.stack = NULL;
        call->held_state.failure = NULL;
    }
    const mc_client_held_item_draw_status status = mc_client_held_item_draw_begin(
        owner->pending_ra, math, call->state.partial, overlays, slot, error);
    if (*slot != NULL) {
        (void)mc_client_held_item_draw_inspect(*slot, state);
        if (overlays)
            call->state.overlays = state;
        else
            call->state.held_item = state;
    }
    if (status != MC_CLIENT_HELD_ITEM_DRAW_OK) {
        call->failure.held_item = error;
        call->failure.child = error->site;
        return fail(call,
                    status == MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL
                        ? MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL
                    : status == MC_CLIENT_HELD_ITEM_DRAW_UNSUPPORTED_FP
                        ? MC_CLIENT_FIRST_PERSON_HAND_UNSUPPORTED_FP
                    : status == MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY
                        ? MC_CLIENT_FIRST_PERSON_HAND_DEPENDENCY
                        : MC_CLIENT_FIRST_PERSON_HAND_CHILD_FAILED,
                    error->operation, pc, error->native_policy);
    }
    if (overlays)
        call->state.overlays_returned = true;
    else
        call->state.held_returned = true;
    return true;
}

static bool body(mc_client_first_person_hand_call *call, mc_math_classes *math) {
    mc_client_display *display = call->state.owner->services.display;
    if (!display_result(call,
                        mc_client_display_source_load_identity(display, &call->failure.display), 0))
        return false;
    call->state.modelview_reset = true;
    mc_game_settings_state settings;
    if (!settings_at(call, 7, 10, &settings))
        return false;
    call->state.anaglyph_read = true;
    call->state.anaglyph = settings.values.anaglyph;
    if (settings.values.anaglyph) {
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
            return fail(call, MC_CLIENT_FIRST_PERSON_HAND_UNSUPPORTED_FP,
                        "hand-pass native floating environment", 21, true);
        const int32_t wrapped = mc_java_i32_sub(mc_java_i32_mul(call->state.eye, 2), 1);
        const float offset =
            _mm_cvtss_f32(_mm_mul_ss(_mm_cvtsi32_ss(_mm_setzero_ps(), wrapped), _mm_set_ss(.1f)));
        if (!display_result(
                call,
                mc_client_display_source_translate(display, offset, 0, 0, &call->failure.display),
                27))
            return false;
    }
    if (!display_result(call, mc_client_display_source_push(display, &call->failure.display), 30))
        return false;
    call->state.matrix_pushed = true;
    if (!effect(call, math, 0, false, 35) || !settings_at(call, 42, 45, &settings))
        return false;
    if (settings.values.view_bobbing && !effect(call, math, 1, true, 53))
        return false;
    if (!settings_at(call, 60, 63, &settings))
        return false;
    bool sleeping;
    if (!settings.values.third_person_view) {
        if (!sleeping_at(call, false, &sleeping))
            return false;
        if (!sleeping) {
            if (!settings_at(call, 86, 89, &settings))
                return false;
            if (!settings.values.hide_gui && !held(call, math, false))
                return false;
        }
    }
    if (!display_result(call, mc_client_display_source_pop(display, &call->failure.display), 103))
        return false;
    call->state.matrix_popped = true;
    if (!settings_at(call, 110, 113, &settings))
        return false;
    if (!settings.values.third_person_view) {
        if (!sleeping_at(call, true, &sleeping))
            return false;
        if (!sleeping && (!held(call, math, true) || !effect(call, math, 2, false, 142)))
            return false;
    }
    if (!settings_at(call, 149, 152, &settings))
        return false;
    if (settings.values.view_bobbing && !effect(call, math, 3, true, 160))
        return false;
    call->state.returned = true;
    call->state.source_pc = 163;
    if (call->overlays != NULL) {
        if (mc_client_held_item_draw_retire(&call->overlays, &call->overlays_cleanup) !=
            MC_CLIENT_HELD_ITEM_DRAW_OK) {
            call->failure.held_item = &call->overlays_cleanup;
            return fail(call, MC_CLIENT_FIRST_PERSON_HAND_BUSY,
                        "completed hand overlays native cleanup", 163, true);
        }
        call->overlays_state.owner = NULL;
        call->overlays_state.player = NULL;
        call->overlays_state.stack = NULL;
        call->overlays_state.failure = NULL;
    }
    for (size_t i = 0; i < 4; ++i) {
        if (mc_client_entity_renderer_camera_retire(&call->effects[i], &call->effect_cleanup[i]) !=
            MC_CLIENT_RENDERER_CAMERA_OK) {
            call->failure.camera = &call->effect_cleanup[i];
            return fail(call, MC_CLIENT_FIRST_PERSON_HAND_BUSY,
                        "completed hand camera effect native cleanup", 163, true);
        }
        call->effect_states[i].owner = NULL;
        call->effect_states[i].error = NULL;
    }
    return true;
}

mc_client_first_person_hand_status entity_renderer_first_person_hand_entered(
    mc_client_source_px *owner, mc_math_classes *math, float partial, int32_t eye,
    mc_client_first_person_hand_call **out, mc_client_first_person_hand_error *error) {
    if (owner == NULL || math == NULL || out == NULL || *out != NULL)
        return result(error, MC_CLIENT_FIRST_PERSON_HAND_ARGUMENT, "hand-pass entered arguments");
    if (!owner->active || owner->phase != MC_CLIENT_ENTITY_RENDERER_READY)
        return result(error, MC_CLIENT_FIRST_PERSON_HAND_BUSY,
                      "hand-pass requires active parent PX");
    mc_client_first_person_hand_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return result(error, MC_CLIENT_FIRST_PERSON_HAND_ALLOCATION, "hand-pass call allocation");
    call->state.owner = owner;
    call->state.partial = partial;
    call->state.eye = eye;
    call->state.failure = &call->failure;
    call->state.active = true;
    *out = call;
    (void)body(call, math);
    call->state.active = false;
    if (error != NULL)
        *error = call->failure;
    return call->failure.status;
}

bool mc_client_first_person_hand_inspect(const mc_client_first_person_hand_call *call,
                                         mc_client_first_person_hand_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    return true;
}

mc_client_first_person_hand_status
mc_client_first_person_hand_retire(mc_client_first_person_hand_call **slot,
                                   mc_client_first_person_hand_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_FIRST_PERSON_HAND_ARGUMENT, "hand-pass cleanup slot");
    mc_client_first_person_hand_call *call = *slot;
    if (call == NULL)
        return result(error, MC_CLIENT_FIRST_PERSON_HAND_OK, NULL);
    if (call->state.active)
        return result(error, MC_CLIENT_FIRST_PERSON_HAND_BUSY, "hand-pass active cleanup");
    call->state.retiring = true;
    if (mc_client_held_item_draw_retire(&call->overlays, &call->overlays_cleanup) !=
            MC_CLIENT_HELD_ITEM_DRAW_OK ||
        mc_client_held_item_draw_retire(&call->held, &call->held_cleanup) !=
            MC_CLIENT_HELD_ITEM_DRAW_OK)
        return result(error, MC_CLIENT_FIRST_PERSON_HAND_BUSY, "hand-pass held draw cleanup");
    call->state.held_item = call->state.overlays = NULL;
    for (size_t i = 0; i < 4; ++i) {
        if (mc_client_entity_renderer_camera_retire(&call->effects[i], &call->effect_cleanup[i]) !=
            MC_CLIENT_RENDERER_CAMERA_OK)
            return result(error, MC_CLIENT_FIRST_PERSON_HAND_BUSY,
                          "hand-pass camera effect cleanup");
        call->state.effects[i] = NULL;
    }
    for (size_t i = 0; i < 2; ++i)
        mc_entity_release(call->sleep_captures[i]);
    free(call);
    *slot = NULL;
    return result(error, MC_CLIENT_FIRST_PERSON_HAND_OK, NULL);
}
