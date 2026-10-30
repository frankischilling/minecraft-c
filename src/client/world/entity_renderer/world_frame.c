#include "mc/client/world/world_frame.h"

#include "../../../entity/entity_internal.h"
#include "../../../world/runtime/chunk_provider_internal.h"
#include "../../application_state.h"
#include "../../display/fog_surface.h"
#include "../../display/source_surface.h"
#include "../../display/startup_surface.h"
#include "../../display/world_frame_surface.h"
#include "../../render_engine/lighting.h"
#include "../../view_slot.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/entity/living/view.h"
#include "mc/entity/player/player.h"
#include "precipitation_internal.h"
#include "renderer_camera_internal.h"
#include "renderer_fog_internal.h"
#include "renderer_fog_state_internal.h"
#ifdef MC_SOURCE_HAS_RENDER_GLOBAL
#include "../../startup/application_startup_internal.h"
#include "../render_global_internal.h"
#include "../render_global_sky_internal.h"
#endif

#include <emmintrin.h>
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
    mc_client_renderer_fog_call *fog;
    mc_client_renderer_fog_state fog_state;
    mc_client_renderer_fog_error fog_error, fog_cleanup;
    mc_client_renderer_camera_call *camera_setup;
    mc_client_renderer_camera_state camera_state;
    mc_client_renderer_camera_error camera_error, camera_cleanup;
    mc_client_renderer_fog_state_call *fog_setup[6];
    mc_client_renderer_fog_state_view fog_setup_state[6];
    mc_client_renderer_fog_state_error fog_setup_error[6], fog_setup_cleanup[6];
    mc_client_precipitation_call *precipitation;
    mc_client_precipitation_state precipitation_state;
    mc_client_precipitation_error precipitation_error, precipitation_cleanup;
    mc_client_effect_renderer *effects_occurrence;
#ifdef MC_SOURCE_HAS_RENDER_GLOBAL
    mc_client_effect_draw_call *lit_particles, *sprite_particles;
    mc_effect_draw_state lit_particles_state, sprite_particles_state;
    mc_effect_draw_error lit_particles_error, lit_particles_cleanup;
    mc_effect_draw_error sprite_particles_error, sprite_particles_cleanup;
    mc_effect_draw_error particles_retirement_error;
    mc_client_render_global_sky_call *sky;
    mc_client_render_global_sky_state sky_state;
    mc_client_render_global_sky_error sky_error, sky_cleanup;
    mc_client_source_frustum *culler;
    mc_client_source_frustum_state culler_state;
    mc_client_culling_call *culling;
    mc_client_culling_state culling_state;
    mc_client_culling_error culling_error, culling_cleanup;
    mc_client_render_global_update_call *update;
    mc_client_render_global_update_state update_state;
    mc_client_render_global_error update_error, update_cleanup;
    mc_client_render_global_submit_call *submission;
    mc_client_render_global_submit_state submission_state;
    mc_client_render_global_error submission_error, submission_cleanup;
    mc_client_render_global_submit_call *translucent_submission;
    mc_client_render_global_submit_state translucent_submission_state;
    mc_client_render_global_error translucent_submission_error, translucent_submission_cleanup;
    mc_client_render_global_submit_call *translucent_replay;
    mc_client_render_global_submit_state translucent_replay_state;
    mc_client_render_global_error translucent_replay_error, translucent_replay_cleanup;
    mc_client_render_global_error translucent_retirement_error;
    mc_client_render_global *render_global_occurrence;
    mc_client_render_global_error render_global_error, render_global_cleanup;
    mc_client_render_global_entities_call *entities;
    mc_client_render_global_entities_state entities_state;
    mc_client_render_global_error entities_error, entities_cleanup;
    mc_client_render_global_error entities_retirement_error;
    mc_client_render_engine *terrain_engine;
    mc_client_texture_lookup_error terrain_texture_error, terrain_texture_cleanup;
    mc_client_render_engine *translucent_engine;
    mc_client_texture_lookup_error translucent_texture_error, translucent_texture_cleanup;
    bool terrain_lookup_owned, translucent_lookup_owned;
    mc_client_graphics_error lighting_error;
    mc_client_graphics_error particle_lighting_error;
    mc_client_lighting_error lighting_enable_error;
    mc_entity_view_error eye_error;
    mc_entity_vector *entity_eye;
    mc_entity_classes *eye_classes;
    mc_entity_hit_result *selection_hit;
    mc_entity_hit_error selection_hit_cleanup;
    mc_item_stack *selection_stack;
    mc_platform_nanotime update_clock;
#endif
    bool owner_held, camera_held;
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

static bool fog_body(mc_client_world_frame_call *call, mc_math_classes *math) {
    const mc_client_renderer_fog_status status = entity_renderer_fog_entered(
        call->state.owner, math, call->state.partial, &call->fog, &call->fog_error);
    if (call->fog != NULL) {
        (void)mc_client_entity_renderer_fog_inspect(call->fog, &call->fog_state);
        call->state.fog = &call->fog_state;
        call->state.fog_world_captured = call->fog_state.world_captured;
        call->state.fog_camera_captured = call->fog_state.camera_captured;
        call->state.fog_distance_read = call->fog_state.distance_read;
        call->state.fog_blend_computed = call->fog_state.blend_computed;
        call->state.fog_render_distance = call->fog_state.render_distance;
        call->state.fog_distance_blend = call->fog_state.distance_blend;
    }
    if (status == MC_CLIENT_RENDERER_FOG_OK) {
        call->state.fog_returned = true;
        return true;
    }
    call->failure.fog = call->fog == NULL ? &call->fog_error : call->fog_state.failure;
    call->failure.status =
        status == MC_CLIENT_RENDERER_FOG_SOURCE_NULL      ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
        : status == MC_CLIENT_RENDERER_FOG_UNSUPPORTED_FP ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                                                          : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
    call->failure.operation = call->fog_error.operation;
    call->failure.native_policy = call->fog_error.native_policy;
    call->failure.site = call->fog_error.site;
    call->failure.child = call->fog_error.child;
    call->state.source_pc = 263;
    return false;
}

static bool fog_setup_body(mc_client_world_frame_call *call, unsigned int index, int32_t pass,
                           unsigned int pc) {
    const mc_client_renderer_fog_state_status status =
        entity_renderer_fog_state_entered(call->state.owner, pass, call->state.partial,
                                          &call->fog_setup[index], &call->fog_setup_error[index]);
    if (call->fog_setup[index] != NULL) {
        (void)mc_client_entity_renderer_fog_state_inspect(call->fog_setup[index],
                                                          &call->fog_setup_state[index]);
        if (index == 0)
            call->state.sky_fog = &call->fog_setup_state[index];
        else if (index == 1)
            call->state.terrain_fog = &call->fog_setup_state[index];
        else if (index == 2)
            call->state.draw_fog = &call->fog_setup_state[index];
        else if (index == 3)
            call->state.particle_fog = &call->fog_setup_state[index];
        else if (index == 4)
            call->state.translucent_fog = &call->fog_setup_state[index];
        else
            call->state.cloud_fog = &call->fog_setup_state[index];
    }
    if (status == MC_CLIENT_RENDERER_FOG_STATE_OK) {
        if (index == 0)
            call->state.sky_fog_returned = true;
        else if (index == 1)
            call->state.terrain_fog_returned = true;
        else if (index == 2)
            call->state.draw_fog_returned = true;
        else if (index == 3)
            call->state.particle_fog_returned = true;
        else if (index == 4)
            call->state.translucent_fog_returned = true;
        else
            call->state.cloud_fog_returned = true;
        return true;
    }
    const mc_client_renderer_fog_state_error *error = &call->fog_setup_error[index];
    call->failure.fog_setup =
        call->fog_setup[index] == NULL ? error : call->fog_setup_state[index].failure;
    call->failure.status = status == MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL
                               ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                           : status == MC_CLIENT_RENDERER_FOG_STATE_UNSUPPORTED_FP
                               ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                               : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
    call->failure.operation = error->operation;
    call->failure.native_policy = error->native_policy;
    call->failure.site = error->site;
    call->failure.child = error->child;
    call->state.source_pc = pc;
    return false;
}

#ifdef MC_SOURCE_HAS_RENDER_GLOBAL
#include "world_frame_after_particles.inc"

static bool entities_body(mc_client_world_frame_call *call, mc_math_classes *math) {
    if (call->state.render_global == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual captured Entity rendering receiver", 510, false);
    client_application *app = call->state.app;
    const mc_client_render_global_entities_services services = {
        .models = call->state.owner->services.models,
        .world_classes = app->world_classes,
        .profile = app->profile,
        .registry = app->registry,
        .manager_classes = app->startup == NULL ? NULL : app->startup->manager_classes,
        .math = math};
    const mc_client_render_global_status status = mc_client_render_global_entities_begin(
        call->state.render_global, call->entity_eye, call->culler, call->state.partial, &services,
        &call->entities, &call->entities_error);
    if (call->entities != NULL) {
        (void)mc_client_render_global_entities_inspect(call->entities, &call->entities_state);
        call->state.entities = &call->entities_state;
    }
    if (status != MC_CLIENT_RENDER_GLOBAL_OK) {
        call->failure.entities =
            call->entities == NULL ? &call->entities_error : call->entities_state.failure;
        call->failure.status = status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                   ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                               : status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                                   ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                               : status == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                   ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                                   : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
        call->failure.operation = call->entities_error.operation;
        call->failure.native_policy = call->entities_error.native_policy;
        call->failure.site = (mc_client_graphics_site){
            call->entities_error.source_class, call->entities_error.source_method,
            call->entities_error.source_descriptor, (int32_t)call->entities_error.source_pc};
        call->failure.child = call->entities_error.child;
        call->state.source_pc = 510;
        return false;
    }
    call->state.entities_returned = true;
    return particles_body(call, math);
}

static void sky_views_expired(mc_client_world_frame_call *call) {
    call->sky_state.sky_vector = NULL;
    call->sky_state.sunrise = NULL;
    call->sky_state.failure = NULL;
    call->sky_state.cleanup = NULL;
}

static bool sky_body(mc_client_world_frame_call *call, mc_math_classes *math) {
    if (call->state.render_global == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual World-frame RenderGlobal sky receiver", 312, false);
    const mc_client_render_global_status status = mc_client_render_global_sky_begin(
        call->state.render_global, math, call->state.partial, &call->sky, &call->sky_error);
    if (call->sky != NULL) {
        (void)mc_client_render_global_sky_inspect(call->sky, &call->sky_state);
        call->state.sky = &call->sky_state;
    }
    if (status != MC_CLIENT_RENDER_GLOBAL_OK) {
        call->failure.sky = call->sky == NULL ? &call->sky_error : call->sky_state.failure;
        call->failure.status = status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                   ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                               : status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                                   ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                                   : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
        call->failure.operation = call->sky_error.operation;
        call->failure.native_policy = call->sky_error.native_policy;
        call->failure.site = call->sky_error.site;
        call->failure.child = call->sky_error.child;
        call->state.source_pc = 312;
        return false;
    }
    call->state.sky_returned = true;
    /* End the completed native child before the next N method. Its copied
     * scalars remain; actual pool/array views and error borrows end here. */
    if (mc_client_render_global_sky_retire(&call->sky, &call->sky_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->sky != NULL)
            (void)mc_client_render_global_sky_inspect(call->sky, &call->sky_state);
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "World-frame completed sky native retirement", 312, true);
    }
    sky_views_expired(call);
    return true;
}

static bool culling_fail(mc_client_world_frame_call *call, mc_client_frustum_status status,
                         unsigned int pc) {
    call->failure.culling = &call->culling_error;
    call->failure.child = call->culling_error.site;
    return outer_fail(call,
                      status == MC_CLIENT_FRUSTUM_SOURCE_NULL ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                      : status == MC_CLIENT_FRUSTUM_UNSUPPORTED_FP
                          ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                          : MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                      call->culling_error.operation, pc, call->culling_error.native_policy);
}

static bool culling_body(mc_client_world_frame_call *call, mc_math_classes *math) {
    mc_client_source_px *owner = call->state.owner;
    mc_client_frustum_status status = mc_client_source_frustum_begin(
        owner->services.models, owner->services.graphics, owner->services.display, math,
        &call->culler, &call->culling_error);
    if (call->culler != NULL) {
        (void)mc_client_source_frustum_inspect(call->culler, &call->culler_state);
        call->state.culler = &call->culler_state;
    }
    if (status != MC_CLIENT_FRUSTUM_OK)
        return culling_fail(call, status, 350);
    call->state.frustum_constructed = true;
    status =
        mc_client_source_frustum_recenter(call->culler, call->state.render_x, call->state.render_y,
                                          call->state.render_z, &call->culling_error);
    (void)mc_client_source_frustum_inspect(call->culler, &call->culler_state);
    if (status != MC_CLIENT_FRUSTUM_OK)
        return culling_fail(call, status, 363);
    call->state.frustum_recentered = true;
    client_application *app = app_at(call, 372, "a", "(FJ)V");
    if (app == NULL)
        return false;
    if (app->g == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual post-camera RenderGlobal cull receiver", 378, false);
    status = mc_client_render_global_cull_begin(app->g, call->culler, call->state.partial,
                                                &call->culling, &call->culling_error);
    if (call->culling != NULL) {
        (void)mc_client_render_global_cull_inspect(call->culling, &call->culling_state);
        call->state.culling = &call->culling_state;
    }
    if (status != MC_CLIENT_FRUSTUM_OK)
        return culling_fail(call, status, 378);
    call->state.culling_returned = true;
    if (mc_client_render_global_cull_retire(&call->culling, &call->culling_cleanup) !=
        MC_CLIENT_FRUSTUM_OK) {
        if (call->culling != NULL)
            (void)mc_client_render_global_cull_inspect(call->culling, &call->culling_state);
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY, "completed N culling native retirement",
                          378, true);
    }
    call->culling_state.failure = NULL;
    for (;;) {
        app = app_at(call, 390, "a", "(FJ)V");
        if (app == NULL)
            return false;
        if (app->g == NULL)
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                              "actual ChunkRenderer compilation receiver", 396, false);
        const mc_client_render_global_status updated = mc_client_render_global_update_begin(
            app->g, call->state.camera, false, &call->update, &call->update_error);
        ++call->state.update_calls;
        if (call->update != NULL) {
            (void)mc_client_render_global_update_inspect(call->update, &call->update_state);
            call->state.update = &call->update_state;
        }
        if (updated != MC_CLIENT_RENDER_GLOBAL_OK) {
            call->failure.update =
                call->update == NULL ? &call->update_error : call->update_state.failure;
            call->failure.status = updated == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                       ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                                   : updated == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                       ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                                       : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
            call->failure.operation = call->update_error.operation;
            call->failure.native_policy = call->update_error.native_policy;
            call->failure.site = (mc_client_graphics_site){
                call->update_error.source_class, call->update_error.source_method,
                call->update_error.source_descriptor, (int32_t)call->update_error.source_pc};
            call->failure.child = call->update_error.child;
            call->state.source_pc = 396;
            return false;
        }
        const bool done = call->update_state.result;
        if (mc_client_render_global_update_retire(&call->update, &call->update_cleanup) !=
            MC_CLIENT_RENDER_GLOBAL_OK) {
            if (call->update != NULL)
                (void)mc_client_render_global_update_inspect(call->update, &call->update_state);
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY,
                              "completed N update native retirement", 396, true);
        }
        call->update_state.near_chunks = NULL;
        call->update_state.compile_order = NULL;
        call->update_state.compilation = NULL;
        call->update_state.failure = call->update_state.cleanup = NULL;
        if (done || call->state.deadline == 0)
            break;
        if (!call->update_clock.initialized) {
            call->failure.clock = mc_platform_nanotime_init(&call->update_clock);
            if (call->failure.clock != MC_PLATFORM_NANOTIME_OK)
                return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                                  "World-frame monotonic clock initialization", 415, true);
        }
        call->failure.clock =
            mc_platform_nanotime_sample(&call->update_clock, &call->state.last_clock);
        if (call->failure.clock != MC_PLATFORM_NANOTIME_OK)
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                              "World-frame monotonic clock sample", 415, true);
        call->state.deadline_remaining =
            mc_java_i64_sub(call->state.deadline, call->state.last_clock);
        if (call->state.deadline_remaining < 0 ||
            call->state.deadline_remaining > INT64_C(1000000000))
            break;
    }
    call->state.updates_returned = true;
    if (!fog_setup_body(call, 2, 0, 449) ||
        !display_result(call,
                        mc_client_display_source_fog_enable(owner->services.display, true,
                                                            &call->failure.display),
                        455))
        return false;
    app = app_at(call, 465, "a", "(FJ)V");
    if (app == NULL)
        return false;
    mc_client_render_engine *engine = app->render_engine;
    if (engine == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual terrain texture Engine receiver", 470, false);
    if (mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY, "terrain Engine native occurrence", 470,
                          true);
    call->terrain_engine = engine;
    static const uint16_t terrain[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    mc_client_texture texture;
    const mc_texture_pack_text key = {terrain, sizeof terrain / sizeof(*terrain), false};
    const bool terrain_lookup_pending = mc_client_texture_lookup_pending(engine);
    const mc_client_texture_lookup_status terrain_lookup_status = mc_client_render_engine_texture(
        engine, owner->services.display, NULL, key, &texture, &call->terrain_texture_error);
    call->terrain_lookup_owned =
        !terrain_lookup_pending && mc_client_texture_lookup_pending(engine);
    if (terrain_lookup_status != MC_CLIENT_LOOKUP_OK) {
        call->failure.texture = &call->terrain_texture_error;
        call->failure.child = call->terrain_texture_error.site;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                          call->terrain_texture_error.operation, 470,
                          call->terrain_texture_error.native_policy);
    }
    if (!display_result(call,
                        mc_client_display_source_bind_id(owner->services.display, texture,
                                                         &call->failure.display),
                        473))
        return false;
    call->state.terrain_texture_bound = true;
    mc_client_display *d = owner->services.display;
    if (!mc_client_lighting_disable(owner->services.graphics, d,
                                    (mc_client_graphics_site){"px", "a", "(FJ)V", 476},
                                    &call->lighting_error)) {
        call->failure.lighting = &call->lighting_error;
        call->failure.child = call->lighting_error.site;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_CHILD_FAILED, call->lighting_error.operation,
                          476, call->lighting_error.native_policy);
    }
    call->state.lighting_disabled = true;
    if (call->state.render_global == NULL)
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
                          "actual captured terrain RenderGlobal receiver", 486, false);
    const mc_client_render_global_status submitted = mc_client_render_global_submit_begin(
        call->state.render_global, call->state.camera, 0, (double)call->state.partial, math,
        &call->submission, &call->submission_error);
    if (call->submission != NULL) {
        (void)mc_client_render_global_submit_inspect(call->submission, &call->submission_state);
        call->state.submission = &call->submission_state;
    }
    if (submitted != MC_CLIENT_RENDER_GLOBAL_OK) {
        call->failure.submission =
            call->submission == NULL ? &call->submission_error : call->submission_state.failure;
        call->failure.status = submitted == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                   ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                               : submitted == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                   ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                                   : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
        call->failure.operation = call->submission_error.operation;
        call->failure.native_policy = call->submission_error.native_policy;
        call->failure.site = (mc_client_graphics_site){
            call->submission_error.source_class, call->submission_error.source_method,
            call->submission_error.source_descriptor, (int32_t)call->submission_error.source_pc};
        call->failure.child = call->submission_error.child;
        call->state.source_pc = 486;
        return false;
    }
    call->state.terrain_count = call->submission_state.result;
    if (mc_client_render_global_submit_retire(&call->submission, &call->submission_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->submission != NULL)
            (void)mc_client_render_global_submit_inspect(call->submission, &call->submission_state);
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_BUSY,
                          "completed N submission native retirement", 486, true);
    }
    call->submission_state.frames = NULL;
    call->submission_state.frame_count = 0;
    call->submission_state.rebuild = NULL;
    call->submission_state.failure = call->submission_state.cleanup = NULL;
    call->submission_state.owner = NULL;
    call->submission_state.camera = call->submission_state.batch_camera = NULL;
    call->submission_state.math = NULL;
    call->state.terrain_returned = true;
    if (!display_result(call,
                        mc_client_display_source_shade_model(owner->services.display, 7424,
                                                             &call->failure.display),
                        493))
        return false;
    call->state.standard_lighting = &call->lighting_enable_error;
    if (!mc_client_lighting_enable(
            owner->services.graphics, call->state.app->entities, math, owner->services.display,
            (mc_client_graphics_site){"px", "a", "(FJ)V", 496}, &call->lighting_enable_error)) {
        call->failure.lighting_enable = &call->lighting_enable_error;
        call->failure.child = call->lighting_enable_error.site;
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
                          call->lighting_enable_error.operation, 496,
                          call->lighting_enable_error.native_policy);
    }
    call->state.lighting_enabled = true;
    const mc_entity_view_status view_status = mc_entity_living_position(
        call->state.camera, call->state.partial, &call->entity_eye, &call->eye_error);
    if (view_status != MC_ENTITY_VIEW_OK) {
        call->failure.view = &call->eye_error;
        call->failure.child =
            (mc_client_graphics_site){call->eye_error.site.class_name, call->eye_error.site.method,
                                      call->eye_error.site.descriptor, call->eye_error.site.pc};
        return outer_fail(
            call,
            view_status == MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
            : view_status == MC_ENTITY_VIEW_UNSUPPORTED_FP ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                                                           : MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
            call->eye_error.operation, 504, call->eye_error.native_policy);
    }
    call->eye_classes = call->state.camera->classes;
    call->state.entity_eye = call->entity_eye;
    call->state.eye_position_returned = true;
    return entities_body(call, math);
}
#endif

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
#ifdef MC_SOURCE_HAS_RENDER_GLOBAL
    if (call->state.render_global != NULL) {
        if (mc_client_render_global_retain(call->state.render_global, &call->render_global_error) !=
            MC_CLIENT_RENDER_GLOBAL_OK) {
            call->failure.entities = &call->render_global_error;
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                              "World-frame captured N occurrence", 57, true);
        }
        call->render_global_occurrence = call->state.render_global;
    }
#endif
    app = app_at(call, 63, "a", "(FJ)V");
    if (app == NULL)
        return false;
    call->state.effects = app->effect_renderer;
    if (call->state.effects != NULL) {
        call->failure.effects = mc_client_effect_renderer_retain(call->state.effects);
        if (call->failure.effects != MC_EFFECT_RENDERER_OK)
            return outer_fail(call, MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                              "World-frame captured EffectRenderer occurrence", 66, true);
        call->effects_occurrence = call->state.effects;
    }
    call->state.effects_captured = true;
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
    if (!fog_body(call, math)) /* Actual h invocation at263. */
        return false;
    if (!display_result(
            call, mc_client_display_source_clear(display, 16640, &call->failure.display), 269))
        return false;
    call->state.buffers_cleared = true;
    if (!display_result(
            call, mc_client_display_source_cull_test(display, true, &call->failure.display), 275))
        return false;
    call->state.post_fog_cull_enabled = true;
    const mc_client_renderer_camera_status camera_status = entity_renderer_camera_entered(
        owner, math, call->state.partial, 0, &call->camera_setup, &call->camera_error);
    if (call->camera_setup != NULL) {
        (void)mc_client_entity_renderer_camera_inspect(call->camera_setup, &call->camera_state);
        call->state.camera_setup = &call->camera_state;
    }
    if (camera_status != MC_CLIENT_RENDERER_CAMERA_OK) {
        call->failure.camera =
            call->camera_setup == NULL ? &call->camera_error : call->camera_state.error;
        call->failure.status = camera_status == MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL
                                   ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                               : camera_status == MC_CLIENT_RENDERER_CAMERA_UNSUPPORTED_FP
                                   ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                               : camera_status == MC_CLIENT_RENDERER_CAMERA_DEPENDENCY
                                   ? MC_CLIENT_WORLD_FRAME_DEPENDENCY
                                   : MC_CLIENT_WORLD_FRAME_CHILD_FAILED;
        call->failure.operation = call->camera_error.operation;
        call->failure.native_policy = call->camera_error.native_policy;
        call->failure.site = call->camera_error.site;
        call->failure.child = call->camera_error.child;
        call->state.source_pc = 282;
        return false;
    }
    call->state.camera_returned = true;
    const mc_client_source_dr *frustum = NULL;
    const mc_client_frustum_status frustum_status =
        mc_client_frustum_update(owner->services.models, owner->services.graphics, display, math,
                                 &frustum, &call->failure.frustum);
    (void)mc_client_frustum_inspect(owner->services.models, &call->state.frustum);
    if (frustum_status != MC_CLIENT_FRUSTUM_OK) {
        call->failure.child = call->failure.frustum.site;
        return outer_fail(
            call,
            frustum_status == MC_CLIENT_FRUSTUM_SOURCE_NULL ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
            : frustum_status == MC_CLIENT_FRUSTUM_UNSUPPORTED_FP
                ? MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP
                : MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
            call->failure.frustum.operation, 285, call->failure.frustum.native_policy);
    }
    call->state.frustum_returned = true; /* Actual returned dr is popped288. */
    app = app_at(call, 293, "a", "(FJ)V");
    if (app == NULL)
        return false;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return outer_fail(call,
                          app->settings == NULL ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                                                : MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "actual post-frustum render-distance Settings receiver", 296,
                          app->settings != NULL);
    call->state.fog_distance_branch_read = true;
    call->state.post_camera_render_distance = settings.values.render_distance;
    if (settings.values.render_distance < 2) {
        if (!fog_setup_body(call, 0, -1, 306))
            return false;
#ifdef MC_SOURCE_HAS_RENDER_GLOBAL
        if (!sky_body(call, math))
            return false;
#else
        return outer_fail(call, MC_CLIENT_WORLD_FRAME_DEPENDENCY,
                          "RenderGlobal sky unavailable in this native build graph", 312, true);
#endif
    }
    if (!display_result(
            call, mc_client_display_source_fog_enable(display, true, &call->failure.display), 318))
        return false;
    call->state.fog_enabled = true;
    if (!fog_setup_body(call, 1, 1, 324))
        return false;
    app = app_at(call, 331, "a", "(FJ)V");
    if (app == NULL)
        return false;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return outer_fail(call,
                          app->settings == NULL ? MC_CLIENT_WORLD_FRAME_SOURCE_NULL
                                                : MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
                          "actual ambient-occlusion Settings receiver", 334, app->settings != NULL);
    call->state.ambient_occlusion_read = true;
    call->state.ambient_occlusion = settings.values.ambient_occlusion;
    if (settings.values.ambient_occlusion) {
        if (!display_result(
                call, mc_client_display_source_shade_model(display, 7425, &call->failure.display),
                343))
            return false;
        call->state.smooth_shade_set = true;
    }
#ifdef MC_SOURCE_HAS_RENDER_GLOBAL
    return culling_body(call, math);
#else
    return outer_fail(call, MC_CLIENT_WORLD_FRAME_DEPENDENCY,
                      "RenderGlobal culling unavailable in this native build graph", 350, true);
#endif
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
#ifdef MC_SOURCE_HAS_RENDER_GLOBAL
    const mc_effect_draw_error *particle_failure = call->sprite_particles_state.failure;
    if (mc_client_effect_renderer_draw_retire(
            &call->sprite_particles, &call->sprite_particles_cleanup) != MC_EFFECT_DRAW_OK) {
        if (call->sprite_particles != NULL)
            (void)mc_client_effect_renderer_draw_inspect(call->sprite_particles,
                                                         &call->sprite_particles_state);
        result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame sprite particle retirement");
        if (error != NULL)
            error->particles = &call->sprite_particles_cleanup;
        return MC_CLIENT_WORLD_FRAME_BUSY;
    }
    if (call->failure.particles == particle_failure)
        call->failure.particles = NULL;
    particle_views_expired(&call->sprite_particles_state);
    call->state.sprite_particles = NULL;
    particle_failure = call->lit_particles_state.failure;
    if (mc_client_effect_renderer_draw_retire(&call->lit_particles, &call->lit_particles_cleanup) !=
        MC_EFFECT_DRAW_OK) {
        if (call->lit_particles != NULL)
            (void)mc_client_effect_renderer_draw_inspect(call->lit_particles,
                                                         &call->lit_particles_state);
        result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame lit particle retirement");
        if (error != NULL)
            error->particles = &call->lit_particles_cleanup;
        return MC_CLIENT_WORLD_FRAME_BUSY;
    }
    if (call->failure.particles == particle_failure)
        call->failure.particles = NULL;
    particle_views_expired(&call->lit_particles_state);
    call->state.lit_particles = NULL;
    call->particles_retirement_error.texture = NULL;
    call->particles_retirement_error.manager.font = NULL;
    call->particles_retirement_error.manager.acquisition = NULL;
    const bool entities_retained = call->entities != NULL;
    if (mc_client_render_global_entities_retire(&call->entities, &call->entities_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->entities != NULL)
            (void)mc_client_render_global_entities_inspect(call->entities, &call->entities_state);
        result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame Entity rendering retirement");
        if (error != NULL)
            error->entities = &call->entities_cleanup;
        return MC_CLIENT_WORLD_FRAME_BUSY;
    }
    call->state.entities = NULL;
    entities_views_expired(call);
    render_error_views_expired(&call->entities_retirement_error);
    if (entities_retained)
        call->failure.entities = NULL; /* The actual child's borrowed failure view ended. */
    const mc_client_render_global_error *replay_failure = call->translucent_replay_state.failure;
    if (mc_client_render_global_submit_retire(&call->translucent_replay,
                                              &call->translucent_replay_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->translucent_replay != NULL)
            (void)mc_client_render_global_submit_inspect(call->translucent_replay,
                                                         &call->translucent_replay_state);
        result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame translucent replay retirement");
        if (error != NULL)
            error->submission = &call->translucent_replay_cleanup;
        return MC_CLIENT_WORLD_FRAME_BUSY;
    }
    if (call->failure.submission == replay_failure)
        call->failure.submission = NULL;
    submission_views_expired(&call->translucent_replay_state);
    call->state.translucent_replay = NULL;
    if (mc_client_render_global_submit_retire(&call->translucent_submission,
                                              &call->translucent_submission_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->translucent_submission != NULL)
            (void)mc_client_render_global_submit_inspect(call->translucent_submission,
                                                         &call->translucent_submission_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY,
                      "World-frame translucent N submission retirement");
    }
    call->state.translucent_submission = NULL;
    if (call->failure.submission == call->translucent_submission_state.failure)
        call->failure.submission = NULL;
    submission_views_expired(&call->translucent_submission_state);
    render_error_views_expired(&call->translucent_retirement_error);
    if (mc_entity_hit_result_release(&call->selection_hit, &call->selection_hit_cleanup) !=
        MC_ENTITY_HIT_OK) {
        result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame selection Hit retirement");
        if (error != NULL)
            error->hit = call->selection_hit_cleanup;
        return MC_CLIENT_WORLD_FRAME_BUSY;
    }
    call->state.selection_hit = NULL;
    mc_item_stack_release(call->selection_stack);
    call->selection_stack = NULL;
    call->state.selection_stack = NULL;
    if (call->entity_eye != NULL) {
        const mc_entity_status released =
            mc_entity_vector_release(call->eye_classes, call->entity_eye);
        if (released != MC_ENTITY_OK)
            return result(error, MC_CLIENT_WORLD_FRAME_BUSY,
                          "World-frame Entity eye vector retirement");
        call->entity_eye = NULL;
        call->state.entity_eye = NULL;
        call->eye_classes = NULL;
    }
    if (mc_client_render_global_submit_retire(&call->submission, &call->submission_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->submission != NULL)
            (void)mc_client_render_global_submit_inspect(call->submission, &call->submission_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame N submission retirement");
    }
    call->state.submission = NULL;
    call->failure.submission = NULL;
    if (mc_client_render_global_update_retire(&call->update, &call->update_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->update != NULL)
            (void)mc_client_render_global_update_inspect(call->update, &call->update_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame N update retirement");
    }
    call->state.update = NULL;
    call->failure.update = NULL;
    if (call->terrain_engine != NULL) {
        if (call->terrain_lookup_owned &&
            mc_client_render_engine_retire_lookup(
                call->terrain_engine, &call->terrain_texture_cleanup) != MC_CLIENT_LOOKUP_OK)
            return result(error, MC_CLIENT_WORLD_FRAME_BUSY,
                          "World-frame terrain lookup retirement");
        call->terrain_lookup_owned = false;
        if (mc_client_render_engine_release(call->terrain_engine) != MC_CLIENT_RENDER_ENGINE_OK)
            return result(error, MC_CLIENT_WORLD_FRAME_BUSY,
                          "World-frame terrain Engine retirement");
        call->terrain_engine = NULL;
    }
    if (call->translucent_engine != NULL) {
        if (call->translucent_lookup_owned &&
            mc_client_render_engine_retire_lookup(call->translucent_engine,
                                                  &call->translucent_texture_cleanup) !=
                MC_CLIENT_LOOKUP_OK)
            return result(error, MC_CLIENT_WORLD_FRAME_BUSY,
                          "World-frame translucent texture lookup retirement");
        call->translucent_lookup_owned = false;
        if (mc_client_render_engine_release(call->translucent_engine) != MC_CLIENT_RENDER_ENGINE_OK)
            return result(error, MC_CLIENT_WORLD_FRAME_BUSY,
                          "World-frame translucent Engine retirement");
        call->translucent_engine = NULL;
    }
    if (mc_client_render_global_cull_retire(&call->culling, &call->culling_cleanup) !=
        MC_CLIENT_FRUSTUM_OK) {
        if (call->culling != NULL)
            (void)mc_client_render_global_cull_inspect(call->culling, &call->culling_state);
        if (call->culler != NULL)
            (void)mc_client_source_frustum_inspect(call->culler, &call->culler_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame culling retirement");
    }
    call->state.culling = NULL;
    call->failure.culling = NULL;
    if (mc_client_source_frustum_retire(&call->culler, &call->culling_cleanup) !=
        MC_CLIENT_FRUSTUM_OK) {
        if (call->culler != NULL)
            (void)mc_client_source_frustum_inspect(call->culler, &call->culler_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame culler retirement");
    }
    call->state.culler = NULL;
    if (mc_client_render_global_sky_retire(&call->sky, &call->sky_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        if (call->sky != NULL)
            (void)mc_client_render_global_sky_inspect(call->sky, &call->sky_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame sky retirement");
    }
    sky_views_expired(call);
    call->state.sky = NULL;
    call->failure.sky = NULL;
#endif
    if (mc_client_entity_renderer_precipitation_retire(
            &call->precipitation, &call->precipitation_cleanup) != MC_CLIENT_PRECIPITATION_OK) {
        if (call->precipitation != NULL)
            (void)mc_client_entity_renderer_precipitation_inspect(call->precipitation,
                                                                  &call->precipitation_state);
        result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame precipitation retirement");
        if (error != NULL)
            error->precipitation = &call->precipitation_cleanup;
        return MC_CLIENT_WORLD_FRAME_BUSY;
    }
    call->state.precipitation = NULL;
    call->failure.precipitation = NULL;
    for (unsigned int i = 0; i < 6; ++i) {
        const mc_client_renderer_fog_state_error *failure = call->fog_setup_state[i].failure;
        if (mc_client_entity_renderer_fog_state_retire(&call->fog_setup[i],
                                                       &call->fog_setup_cleanup[i]) !=
            MC_CLIENT_RENDERER_FOG_STATE_OK) {
            if (call->fog_setup[i] != NULL)
                (void)mc_client_entity_renderer_fog_state_inspect(call->fog_setup[i],
                                                                  &call->fog_setup_state[i]);
            return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame fog setup retirement");
        }
        if (i == 0)
            call->state.sky_fog = NULL;
        else if (i == 1)
            call->state.terrain_fog = NULL;
        else if (i == 2)
            call->state.draw_fog = NULL;
        else if (i == 3)
            call->state.particle_fog = NULL;
        else if (i == 4)
            call->state.translucent_fog = NULL;
        else
            call->state.cloud_fog = NULL;
        call->fog_setup_state[i].failure = NULL;
        if (call->failure.fog_setup == failure)
            call->failure.fog_setup = NULL;
    }
    if (mc_client_entity_renderer_pick_retire(&call->pick) != MC_CLIENT_PICK_OK) {
        if (call->pick != NULL)
            (void)mc_client_entity_renderer_pick_inspect(call->pick, &call->pick_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame picker retirement");
    }
    call->state.picking = NULL;
    call->failure.picking = NULL;
    if (mc_client_entity_renderer_camera_retire(&call->camera_setup, &call->camera_cleanup) !=
        MC_CLIENT_RENDERER_CAMERA_OK) {
        if (call->camera_setup != NULL)
            (void)mc_client_entity_renderer_camera_inspect(call->camera_setup, &call->camera_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame camera retirement");
    }
    call->state.camera_setup = NULL;
    call->failure.camera = NULL;
    if (mc_client_entity_renderer_fog_retire(&call->fog, &call->fog_cleanup) !=
        MC_CLIENT_RENDERER_FOG_OK) {
        if (call->fog != NULL)
            (void)mc_client_entity_renderer_fog_inspect(call->fog, &call->fog_state);
        return result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame fog retirement");
    }
    call->state.fog = NULL;
    call->failure.fog = NULL;
    if (call->effects_occurrence != NULL) {
        const mc_effect_renderer_status released =
            mc_client_effect_renderer_release(call->effects_occurrence);
        if (released != MC_EFFECT_RENDERER_OK) {
            result(error, MC_CLIENT_WORLD_FRAME_BUSY,
                   "World-frame captured EffectRenderer retirement");
            if (error != NULL)
                error->effects = released;
            return MC_CLIENT_WORLD_FRAME_BUSY;
        }
        call->effects_occurrence = NULL;
        call->state.effects = NULL;
    }
#ifdef MC_SOURCE_HAS_RENDER_GLOBAL
    if (mc_client_render_global_release(&call->render_global_occurrence,
                                        &call->render_global_cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        result(error, MC_CLIENT_WORLD_FRAME_BUSY, "World-frame captured N retirement");
        if (error != NULL)
            error->entities = &call->render_global_cleanup;
        return MC_CLIENT_WORLD_FRAME_BUSY;
    }
    call->state.render_global = NULL;
#endif
    if (call->camera_held) {
        mc_entity_release(call->state.camera);
        call->camera_held = false;
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
