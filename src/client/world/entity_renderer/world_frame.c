#include "mc/client/world/world_frame.h"

#include "../../../world/runtime/chunk_provider_internal.h"
#include "../../application_state.h"
#include "../../display/source_surface.h"
#include "../../display/world_frame_surface.h"
#include "../../view_slot.h"
#include "internal.h"
#include "mc/core/java/java.h"

#include <emmintrin.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Source World-frame arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Source World-frame arithmetic requires strict floating transformations"
#endif

struct mc_client_world_frame_call {
    mc_client_world_frame_state state;
    mc_client_world_frame_error failure;
    mc_client_entity_renderer_error owner_error, owner_cleanup;
    mc_client_entity_renderer_pick_call *pick;
    mc_client_entity_renderer_pick_state pick_state;
    mc_entity_snapshot camera;
    mc_world_runtime *fog_world;
    mc_entity *fog_camera;
    bool owner_held, camera_held, fog_world_held, fog_camera_held;
};

static bool fail(mc_client_world_frame_call *call, mc_client_world_frame_status status,
                 const char *operation, const char *method, const char *descriptor, unsigned int pc,
                 bool native) {
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.native_policy = native;
    call->failure.site = (mc_client_graphics_site){"px", method, descriptor, (int32_t)pc};
    call->state.source_pc = pc;
    return false;
}

static bool outer_fail(mc_client_world_frame_call *call, mc_client_world_frame_status status,
                       const char *operation, unsigned int pc, bool native) {
    return fail(call, status, operation, "a", "(FJ)V", pc, native);
}

static client_application *app_at(mc_client_world_frame_call *call, unsigned int pc,
                                  const char *method, const char *descriptor) {
    client_application *app = call->state.owner->fields.j;
    if (app == NULL) {
        fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL, "actual World-frame App receiver", method,
             descriptor, pc, false);
        return NULL;
    }
    call->state.app = app;
    return app;
}

static bool display_result(mc_client_world_frame_call *call, bool result, unsigned int pc) {
    return result || outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                                call->failure.display.operation, pc, true);
}

static bool fp(mc_client_world_frame_call *call, const char *method, const char *descriptor,
               unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(call, MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP,
                "World-frame native floating environment", method, descriptor, pc, true);
}

static bool capture_entity(mc_client_world_frame_call *call, mc_entity *entity, bool *held,
                           unsigned int pc, const char *method, const char *descriptor) {
    if (entity == NULL)
        return true; /* Source capture accepts NULL until a later dereference. */
    const mc_entity_status status = mc_entity_retain(entity);
    if (status != MC_ENTITY_OK) {
        call->failure.entity = status;
        return fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED, "World-frame Entity occurrence",
                    method, descriptor, pc, true);
    }
    *held = true;
    return true;
}

static double interpolate(double previous, double current, float partial) {
    const __m128d difference = _mm_sub_sd(_mm_set_sd(current), _mm_set_sd(previous));
    const __m128d scaled = _mm_mul_sd(difference, _mm_set_sd((double)partial));
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(previous), scaled));
}

static bool fog_prefix(mc_client_world_frame_call *call) {
    client_application *app = app_at(call, 4, "h", "(F)V");
    if (app == NULL)
        return false;
    call->fog_world = app->active_world; /* Actual captured fd local2 at7. */
    call->state.fog_world_captured = true;
    if (call->fog_world != NULL) {
        if (!mc_world_runtime_binding_retain(call->fog_world))
            return fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                        "World-frame fog World occurrence", "h", "(F)V", 7, true);
        call->fog_world_held = true;
    }
    app = app_at(call, 12, "h", "(F)V");
    if (app == NULL)
        return false;
    call->fog_camera = app->i; /* Actual independently captured ls local3 at15. */
    call->state.fog_camera_captured = true;
    if (!capture_entity(call, call->fog_camera, &call->fog_camera_held, 15, "h", "(F)V"))
        return false;
    app = app_at(call, 22, "h", "(F)V");
    if (app == NULL)
        return false;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return fail(call,
                    app->settings == NULL ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                                          : MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                    "actual fog render-distance Settings receiver", "h", "(F)V", 25,
                    app->settings != NULL);
    call->state.fog_distance_read = true;
    call->state.fog_render_distance = settings.values.render_distance;
    if (!fp(call, "h", "(F)V", 29))
        return false;
    const int32_t denominator = mc_java_i32_sub(4, settings.values.render_distance);
    const __m128 converted = _mm_cvtsi32_ss(_mm_setzero_ps(), denominator);
    const float fraction = _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(1.0f), converted));
    const double powered = pow((double)fraction, 0.25);
    const float power = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(powered)));
    call->state.fog_distance_blend = _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(1.0f), _mm_set_ss(power)));
    call->state.fog_blend_computed = true;
    /* Actual App/i arguments are reloaded at49/52 before the captured fd56
     * invocation. A NULL Entity is passed to that method without preflight. */
    app = app_at(call, 52, "h", "(F)V");
    if (app == NULL)
        return false;
    (void)app->i;
    if (call->fog_world == NULL)
        return fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL, "actual fog sky-color World receiver",
                    "h", "(F)V", 56, false);
    call->failure.child = (mc_client_graphics_site){"fd", "a", "(Lsn;F)Lbt;", 0};
    return fail(call, MC_CLIENT_WORLD_FRAME_DEPENDENCY,
                "actual World sky-color query is not connected", "h", "(F)V", 56, true);
}

static bool body(mc_client_world_frame_call *call, mc_math_classes *math) {
    mc_client_source_px *owner = call->state.owner;
    mc_client_display *display = owner->services.display;
    if (!display_result(
            call, mc_client_display_source_cull_test(display, true, &call->failure.display), 3))
        return false;
    call->state.cull_enabled = true;
    if (!display_result(
            call, mc_client_display_source_depth_test(display, true, &call->failure.display), 9))
        return false;
    call->state.depth_enabled = true;
    client_application *app = app_at(call, 16, "a", "(FJ)V");
    if (app == NULL)
        return false;
    if (app->i == NULL) {
        app = app_at(call, 26, "a", "(FJ)V");
        if (app == NULL)
            return false;
        call->failure.entity = mc_client_view_slot_store(app, app->h);
        if (call->failure.entity != MC_ENTITY_OK)
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                              "World-frame source view occurrence", 33, true);
        call->state.view_stored = true;
    }
    /* Hand the canonical active guard to the existing complete picker. The
     * enclosing retained call remains active, and no SDL event pump occurs. */
    owner->active = false;
    const mc_client_pick_status picked =
        mc_client_entity_renderer_pick_begin(owner, math, call->state.partial, &call->pick);
    if (call->pick != NULL) {
        (void)mc_client_entity_renderer_pick_inspect(call->pick, &call->pick_state);
        call->state.picking = &call->pick_state;
    }
    owner->active = true;
    if (picked != MC_CLIENT_PICK_OK) {
        if (call->pick != NULL)
            call->failure.picking = call->pick_state.failure;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                          call->failure.picking == NULL ? "World-frame picker native admission"
                                                        : call->failure.picking->operation,
                          38,
                          call->failure.picking == NULL || call->failure.picking->native_policy);
    }
    call->state.picking_returned = true;
    owner->active = false;
    const mc_client_pick_status retired = mc_client_entity_renderer_pick_retire(&call->pick);
    owner->active = true;
    if (retired != MC_CLIENT_PICK_OK) {
        if (call->pick != NULL)
            (void)mc_client_entity_renderer_pick_inspect(call->pick, &call->pick_state);
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "World-frame picker native child cleanup", 38, true);
    }
    call->state.picking = NULL;
    app = app_at(call, 45, "a", "(FJ)V");
    if (app == NULL)
        return false;
    call->state.camera = app->i;
    if (!capture_entity(call, call->state.camera, &call->camera_held, 48, "a", "(FJ)V"))
        return false;
    app = app_at(call, 54, "a", "(FJ)V");
    if (app == NULL)
        return false;
    call->state.render_global = app->g;
    app = app_at(call, 63, "a", "(FJ)V");
    if (app == NULL)
        return false;
    call->state.effects = app->effect_renderer;
    if (call->state.camera == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual World-frame camera receiver", 70, false);
    call->failure.entity = mc_entity_get_snapshot(call->state.camera, &call->camera);
    if (call->failure.entity != MC_ENTITY_OK)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "World-frame actual camera fields", 70, true);
    if (!fp(call, "a", "(FJ)V", 83))
        return false;
    call->state.render_x =
        interpolate(call->camera.base.bl, call->camera.base.x, call->state.partial);
    call->state.render_y =
        interpolate(call->camera.base.bm, call->camera.base.y, call->state.partial);
    call->state.render_z =
        interpolate(call->camera.base.bn, call->camera.base.z, call->state.partial);
    app = app_at(call, 138, "a", "(FJ)V");
    if (app == NULL)
        return false;
    call->state.world = app->active_world;
    if (call->state.world == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual World-frame provider World receiver", 141, false);
    mc_world_runtime_fields world;
    if (mc_world_runtime_fields_get(call->state.world, &world) != MC_WORLD_STORAGE_OK)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "World-frame actual World.v access", 141, true);
    call->state.provider_read = true;
    switch (mc_chunk_provider_concrete_kind_of(world.v)) {
        case MC_CHUNK_PROVIDER_NULL:
        case MC_CHUNK_PROVIDER_MAP_CACHE:
            break; /* Neither is kx. No centered-grid operation is reached. */
        default:
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_DEPENDENCY,
                              "World-frame actual provider class provenance", 148, true);
    }
    app = app_at(call, 205, "a", "(FJ)V");
    if (app == NULL)
        return false;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return outer_fail(call,
                          app->settings == NULL ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                                                : MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "World-frame anaglyph Settings receiver", 208, app->settings != NULL);
    call->state.anaglyph_read = true;
    call->state.anaglyph = settings.values.anaglyph;
    if (call->state.anaglyph) {
        owner->services.models->px_b = 0; /* Actual first eye putstatic216. */
        call->state.eye_stored = true;
        if (!display_result(call,
                            mc_client_display_source_color_mask(display, false, true, true, false,
                                                                &call->failure.display),
                            229))
            return false;
        call->state.color_masked = true;
    }
    app = app_at(call, 248, "a", "(FJ)V");
    if (app == NULL)
        return false;
    call->state.width = app->d;
    app = app_at(call, 255, "a", "(FJ)V");
    if (app == NULL)
        return false;
    call->state.height = app->e;
    if (!display_result(call,
                        mc_client_display_source_viewport(
                            display, call->state.width, call->state.height, &call->failure.display),
                        258))
        return false;
    call->state.viewport_set = true;
    return fog_prefix(call); /* Actual h invocation at263. */
}

static mc_client_world_frame_status result(mc_client_world_frame_error *error,
                                           mc_client_world_frame_status status,
                                           const char *operation) {
    if (error != NULL)
        *error = (mc_client_world_frame_error){.status = status,
                                               .operation = operation,
                                               .native_policy = status != MC_CLIENT_WORLD_FRAME_OK};
    return status;
}

mc_client_world_frame_status mc_client_entity_renderer_world_frame_begin(
    mc_client_source_px *owner, mc_math_classes *math, float partial, int64_t deadline,
    mc_client_world_frame_call **out, mc_client_world_frame_error *error) {
    if (owner == NULL || out == NULL || *out != NULL)
        return result(error, MC_CLIENT_WORLD_FRAME_ARGUMENT, "World-frame native owner/output");
    if (owner->active || owner->phase != MC_CLIENT_ENTITY_RENDERER_READY ||
        owner->pick_call != NULL || owner->world_frame_call != NULL)
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame active/pending owner");
    mc_client_world_frame_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return result(error, MC_CLIENT_WORLD_FRAME_ALLOCATION, "World-frame diagnostic allocation");
    call->state.owner = owner;
    call->state.partial = partial;
    call->state.deadline = deadline;
    call->state.failure = &call->failure;
    if (mc_client_entity_renderer_retain(owner, &call->owner_error) !=
        MC_CLIENT_ENTITY_RENDERER_OK) {
        free(call);
        return result(error, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED, "World-frame PX occurrence");
    }
    call->owner_held = true;
    owner->world_frame_call = call;
    *out = call;
    call->state.active = owner->active = true;
    (void)body(call, math);
    call->state.active = owner->active = false;
    if (error != NULL)
        *error = call->failure;
    return call->failure.status;
}

bool mc_client_entity_renderer_world_frame_inspect(const mc_client_world_frame_call *call,
                                                   mc_client_world_frame_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    return true;
}

mc_client_world_frame_status
mc_client_entity_renderer_world_frame_retire(mc_client_world_frame_call **slot,
                                             mc_client_world_frame_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_WORLD_FRAME_ARGUMENT, "World-frame retirement slot");
    mc_client_world_frame_call *call = *slot;
    if (call == NULL)
        return result(error, MC_CLIENT_WORLD_FRAME_OK, NULL);
    if (call->state.active || call->state.owner->active)
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame active retirement");
    if (mc_client_entity_renderer_pick_retire(&call->pick) != MC_CLIENT_PICK_OK) {
        if (call->pick != NULL)
            (void)mc_client_entity_renderer_pick_inspect(call->pick, &call->pick_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame picker retirement");
    }
    call->state.picking = NULL;
    call->failure.picking = NULL;
    if (call->fog_camera_held) {
        mc_entity_release(call->fog_camera);
        call->fog_camera_held = false;
    }
    if (call->camera_held) {
        mc_entity_release(call->state.camera);
        call->camera_held = false;
    }
    if (call->fog_world_held) {
        mc_world_runtime_binding_release(call->fog_world);
        call->fog_world_held = false;
    }
    if (call->owner_held) {
        mc_client_source_px *owner = call->state.owner;
        if (owner->world_frame_call != call)
            return result(error, MC_CLIENT_WORLD_FRAME_BUSY,
                          "World-frame pending occurrence changed");
        owner->world_frame_call = NULL;
        if (mc_client_entity_renderer_release(&owner, &call->owner_cleanup) !=
            MC_CLIENT_ENTITY_RENDERER_OK) {
            call->state.owner->world_frame_call = call;
            return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame PX occurrence release");
        }
        call->owner_held = false;
    }
    free(call);
    *slot = NULL;
    return result(error, MC_CLIENT_WORLD_FRAME_OK, NULL);
}
