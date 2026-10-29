#include "../../../entity/player/player_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "../../application_state.h"
#include "../../display/camera_surface.h"
#include "../../world/render_global_internal.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/entity/hit_result.h"
#include "mc/world/runtime/blocks.h"
#include "renderer_camera_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct mc_client_renderer_camera_call {
    mc_client_renderer_camera_state state;
    mc_client_renderer_camera_error failure;
    mc_math_classes *math;
    mc_entity *captures[8];
    size_t capture_count;
    mc_entity_vector *vectors[24];
    size_t vector_count;
    mc_world_raytrace_call *rays[8];
    mc_world_raytrace_state ray_states[8];
};

static const char *descriptor(const char *method) {
    return strcmp(method, "a") == 0 ? "(FI)V" : strcmp(method, "d") == 0 ? "(F)F" : "(F)V";
}

static bool fail(mc_client_renderer_camera_call *c, mc_client_renderer_camera_status status,
                 const char *operation, const char *method, const char *descriptor, unsigned int pc,
                 bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = (mc_client_graphics_site){"px", method, descriptor, (int32_t)pc};
    c->failure.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool fp(mc_client_renderer_camera_call *c, const char *method, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_CLIENT_RENDERER_CAMERA_UNSUPPORTED_FP, "camera native floating environment",
                method, descriptor(method), pc, true);
}

static client_application *app_at(mc_client_renderer_camera_call *c, const char *method,
                                  unsigned int pc) {
    client_application *app = c->state.owner->fields.j;
    if (app == NULL)
        fail(c, MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL, "actual camera null App receiver", method,
             strcmp(method, "a") == 0   ? "(FI)V"
             : strcmp(method, "d") == 0 ? "(F)F"
                                        : "(F)V",
             pc, false);
    return app;
}

static bool settings(mc_client_renderer_camera_call *c, const char *method, unsigned int app_pc,
                     unsigned int pc, mc_game_settings_state *out) {
    client_application *app = app_at(c, method, app_pc);
    if (app == NULL)
        return false;
    if (!mc_game_settings_read_state(app->settings, out))
        return fail(c,
                    app->settings == NULL ? MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL
                                          : MC_CLIENT_RENDERER_CAMERA_NATIVE_FAILED,
                    "actual camera Settings fields", method, descriptor(method), pc,
                    app->settings != NULL);
    return true;
}

static bool capture(mc_client_renderer_camera_call *c, mc_entity *entity) {
    if (entity == NULL)
        return true;
    c->failure.entity = mc_entity_retain(entity);
    if (c->failure.entity != MC_ENTITY_OK)
        return false;
    c->captures[c->capture_count++] = entity;
    return true;
}

static mc_entity *view(mc_client_renderer_camera_call *c, const char *method) {
    client_application *app = app_at(c, method, 4);
    if (app == NULL)
        return NULL;
    mc_entity *entity = app->i;
    if (!capture(c, entity)) {
        fail(c, MC_CLIENT_RENDERER_CAMERA_NATIVE_FAILED, "camera captured view occurrence", method,
             descriptor(method), 7, true);
        return NULL;
    }
    return entity;
}

static bool local(mc_client_renderer_camera_call *c, mc_entity *entity, const char *method,
                  unsigned int pc) {
    if (entity == NULL)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL, "actual camera null view fields",
                    method, descriptor(method), pc, false);
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        entity->local_player == NULL)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_DEPENDENCY, "camera concrete Living field owner",
                    method, descriptor(method), pc, true);
    return true;
}

static bool gl(mc_client_renderer_camera_call *c, bool ok, const char *method, unsigned int pc) {
    if (!ok)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_NATIVE_FAILED, c->failure.display.operation,
                    method, strcmp(method, "a") == 0 ? "(FI)V" : "(F)V", pc, true);
    ++c->state.transforms;
    return true;
}

static bool translate(mc_client_renderer_camera_call *c, const char *method, unsigned int pc,
                      float x, float y, float z) {
    return gl(c,
              mc_client_display_source_translate(c->state.owner->services.display, x, y, z,
                                                 &c->failure.display),
              method, pc);
}

static bool rotate(mc_client_renderer_camera_call *c, const char *method, unsigned int pc,
                   float angle, float x, float y, float z) {
    return gl(c,
              mc_client_display_source_rotate(c->state.owner->services.display, angle, x, y, z,
                                              &c->failure.display),
              method, pc);
}

static float convert(int32_t value) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

static float narrow(double value) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
}

static float lerp(float old, float current, float partial) {
    return at_fadd(old, at_fmul(at_fsub(current, old), partial));
}

static double position(double old, double current, float partial) {
    return at_add(old, at_mul(at_sub(current, old), (double)partial));
}

static bool math_at(mc_client_renderer_camera_call *c, const char *method, unsigned int pc) {
    if (mc_math_classes_ensure(c->math, &c->failure.math) != MC_MATH_CLASS_OK) {
        c->failure.child =
            (mc_client_graphics_site){"in", "<clinit>", "()V", c->failure.math.source_pc};
        return fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED, "camera MathHelper active use",
                    method, "(F)V", pc, true);
    }
    return fp(c, method, pc);
}

static bool fov(mc_client_renderer_camera_call *c, float *out) {
    mc_entity *entity = view(c, "d");
    if (c->failure.status != MC_CLIENT_RENDERER_CAMERA_OK)
        return false;
    client_application *app = c->state.owner->fields.j;
    mc_material *water = NULL;
    if (mc_registry_material_get(app->registry, MC_MATERIAL_g, &water, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED, "camera actual water Material", "d",
                    "(F)F", 12, true);
    if (entity == NULL)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL, "actual FOV null immersion receiver",
                    "d", "(F)F", 15, false);
    bool submerged;
    c->failure.entity = mc_entity_inside_material(entity, water, &submerged, &c->failure.material);
    if (c->failure.entity != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED, "camera actual eye immersion", "d",
                    "(F)F", 15,
                    c->failure.material.world.status != MC_WORLD_STORAGE_INVALID_ARGUMENT);
    if (!local(c, entity, "d", 25))
        return false;
    mc_living_entity_fields *living = &entity->local_player->fields.living;
    float value = submerged ? 60.0f : 70.0f;
    if (living->Y <= 0) {
        if (!fp(c, "d", 35))
            return false;
        const float death = at_fadd(convert(living->ad), c->state.partial);
        value = at_fdiv(
            value,
            at_fadd(at_fmul(at_fsub(1.0f, at_fdiv(500.0f, at_fadd(death, 500.0f))), 2.0f), 1.0f));
    }
    if (!fp(c, "d", 62))
        return false;
    mc_client_entity_renderer_fields *f = &c->state.owner->fields;
    *out = at_fadd(at_fadd(value, f->v_to_C[5]),
                   at_fmul(at_fsub(f->v_to_C[4], f->v_to_C[5]), c->state.partial));
    return true;
}

static bool hurt(mc_client_renderer_camera_call *c) {
    mc_entity *entity = view(c, "e");
    if (c->failure.status != MC_CLIENT_RENDERER_CAMERA_OK || !local(c, entity, "e", 9) ||
        !fp(c, "e", 12))
        return false;
    const mc_living_entity_fields *living = &entity->local_player->fields.living;
    float amount = at_fsub(convert(living->aa), c->state.partial);
    if (living->Y <= 0) {
        const float death = at_fadd(convert(living->ad), c->state.partial);
        if (!rotate(c, "e", 46, at_fsub(40.0f, at_fdiv(8000.0f, at_fadd(death, 200.0f))), 0, 0, 1))
            return false;
    }
    if (amount < 0)
        return true;
    if (!fp(c, "e", 63))
        return false;
    amount = at_fdiv(amount, convert(living->ab));
    amount = at_fmul(at_fmul(at_fmul(at_fmul(amount, amount), amount), amount), 3.1415927f);
    if (!math_at(c, "e", 74))
        return false;
    amount = mc_math_sin(mc_math_classes_table(c->math), amount);
    const float direction = living->ac;
    return rotate(c, "e", 90, -direction, 0, 1, 0) && fp(c, "e", 96) &&
           rotate(c, "e", 101, at_fmul(-amount, 14.0f), 0, 0, 1) &&
           rotate(c, "e", 109, direction, 0, 1, 0);
}

static bool bob(mc_client_renderer_camera_call *c) {
    mc_entity *entity = view(c, "f");
    if (c->failure.status != MC_CLIENT_RENDERER_CAMERA_OK)
        return false;
    if (entity != NULL && !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PLAYER))
        return fail(c, MC_CLIENT_RENDERER_CAMERA_DEPENDENCY,
                    "camera concrete remote Player bobbing fields", "f", "(F)V", 28, true);
    if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return true; /* Actual instanceof gs fails, including NULL. */
    if (!fp(c, "f", 31))
        return false;
    const mc_entity_base_fields *base = &entity->base;
    const mc_player_fields *p = &entity->local_player->fields.player;
    const mc_living_entity_fields *living = &entity->local_player->fields.living;
    const float phase = -at_fadd(base->bj, at_fmul(at_fsub(base->bj, base->bi), c->state.partial));
    const float amplitude = lerp(p->h, p->i, c->state.partial);
    const float sway = lerp(living->af, living->ag, c->state.partial);
    if (!math_at(c, "f", 87))
        return false;
    const mc_math_trig *trig = mc_math_classes_table(c->math);
    const float angle = at_fmul(phase, 3.1415927f);
    if (!translate(c, "f", 112, at_fmul(at_fmul(mc_math_sin(trig, angle), amplitude), 0.5f),
                   -fabsf(at_fmul(mc_math_cos(trig, angle), amplitude)), 0))
        return false;
    if (!fp(c, "f", 115))
        return false;
    return rotate(c, "f", 132, at_fmul(at_fmul(mc_math_sin(trig, angle), amplitude), 3.0f), 0, 0,
                  1) &&
           fp(c, "f", 135) &&
           rotate(c, "f", 158,
                  at_fmul(fabsf(at_fmul(mc_math_cos(trig, at_fsub(angle, 0.2f)), amplitude)), 5.0f),
                  1, 0, 0) &&
           rotate(c, "f", 166, sway, 1, 0, 0);
}

static bool pooled(mc_client_renderer_camera_call *c, double x, double y, double z, unsigned int pc,
                   mc_entity_vector **out) {
    mc_entity_classes *classes = c->state.owner->services.models->entities;
    c->failure.entity = mc_entity_vector_pool_get(classes, x, y, z, out, &c->failure.vector);
    if (c->failure.entity != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED, "camera actual pooled Vec3", "g",
                    "(F)V", pc, true);
    c->vectors[c->vector_count++] = *out;
    return true;
}

static bool third_person(mc_client_renderer_camera_call *c, mc_entity *entity,
                         const mc_game_settings_state *options, double x, double y, double z) {
    mc_client_entity_renderer_fields *f = &c->state.owner->fields;
    c->state.third_person = true;
    c->state.distance = (double)lerp(f->u, f->t, c->state.partial);
    if (options->values.debug_camera) {
        const float yaw = lerp(f->v_to_C[1], f->v_to_C[0], c->state.partial);
        const float pitch = lerp(f->v_to_C[3], f->v_to_C[2], c->state.partial);
        return translate(c, "g", 365, 0, 0, narrow(-c->state.distance)) &&
               rotate(c, "g", 373, pitch, 1, 0, 0) && rotate(c, "g", 381, yaw, 0, 1, 0);
    }
    const float yaw = entity->base.yaw, pitch = entity->base.aT;
    if (!math_at(c, "g", 407))
        return false;
    const mc_math_trig *trig = mc_math_classes_table(c->math);
    const float yr = at_fmul(at_fdiv(yaw, 180.0f), 3.1415927f);
    const float pr = at_fmul(at_fdiv(pitch, 180.0f), 3.1415927f);
    const double dx =
        at_mul((double)at_fmul(-mc_math_sin(trig, yr), mc_math_cos(trig, pr)), c->state.distance);
    const double dz =
        at_mul((double)at_fmul(mc_math_cos(trig, yr), mc_math_cos(trig, pr)), c->state.distance);
    const double dy = at_mul((double)-mc_math_sin(trig, pr), c->state.distance);
    for (unsigned int index = 0; index < 8; ++index) {
        if (!fp(c, "g", 527))
            return false;
        const float ox = at_fmul(convert((int32_t)(index & 1u) * 2 - 1), 0.1f);
        const float oy = at_fmul(convert((int32_t)((index >> 1) & 1u) * 2 - 1), 0.1f);
        const float oz = at_fmul(convert((int32_t)((index >> 2) & 1u) * 2 - 1), 0.1f);
        client_application *app = app_at(c, "g", 548);
        if (app == NULL)
            return false;
        mc_world_runtime *world = app->active_world; /* Captured before both bt.b factories. */
        mc_entity_vector *start = NULL, *end = NULL;
        if (!pooled(c, at_add(x, (double)ox), at_add(y, (double)oy), at_add(z, (double)oz), 569,
                    &start) ||
            !pooled(c, at_add(at_add(at_sub(x, dx), (double)ox), (double)oz),
                    at_add(at_sub(y, dy), (double)oy), at_add(at_sub(z, dz), (double)oz), 603,
                    &end))
            return false;
        if (world == NULL)
            return fail(c, MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL,
                        "actual third-person null World ray receiver", "g", "(F)V", 606, false);
        ++c->state.ray_calls;
        const mc_world_raytrace_status status =
            mc_world_raytrace_begin(world, start, end, &c->rays[index]);
        if (c->rays[index] != NULL)
            (void)mc_world_raytrace_inspect(c->rays[index], &c->ray_states[index]);
        if (status != MC_WORLD_RAYTRACE_OK) {
            c->failure.ray = c->ray_states[index].failure;
            return fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED, "camera actual World ray", "g",
                        "(F)V", 606, c->failure.ray == NULL || c->failure.ray->native_policy);
        }
        if (c->ray_states[index].result != NULL) {
            mc_entity_hit_result_view hit;
            if (mc_entity_hit_result_inspect(c->ray_states[index].result, &hit) != MC_ENTITY_HIT_OK)
                return fail(c, MC_CLIENT_RENDERER_CAMERA_NATIVE_FAILED, "camera actual hit fields",
                            "g", "(F)V", 618, true);
            mc_entity_vector *origin = NULL;
            if (!pooled(c, x, y, z, 627, &origin))
                return false;
            double distance;
            if (mc_entity_vector_distance(c->state.owner->services.models->entities, hit.fields.f,
                                          origin, &distance,
                                          &c->failure.geometry) != MC_ENTITY_GEOMETRY_OK)
                return fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED, "camera hit Vec3 distance",
                            "g", "(F)V", 630, c->failure.geometry.native_policy);
            if (!fp(c, "g", 639))
                return false;
            if (distance < c->state.distance)
                c->state.distance = distance;
        }
    }
    if (!fp(c, "g", 659))
        return false;
    return rotate(c, "g", 663, at_fsub(entity->base.aT, pitch), 1, 0, 0) && fp(c, "g", 672) &&
           rotate(c, "g", 676, at_fsub(entity->base.yaw, yaw), 0, 1, 0) && fp(c, "g", 684) &&
           translate(c, "g", 685, 0, 0, narrow(-c->state.distance)) && fp(c, "g", 694) &&
           rotate(c, "g", 698, at_fsub(yaw, entity->base.yaw), 0, 1, 0) && fp(c, "g", 707) &&
           rotate(c, "g", 711, at_fsub(pitch, entity->base.aT), 1, 0, 0);
}

static bool bed_query(mc_client_renderer_camera_call *c, mc_entity *entity, bool metadata,
                      int32_t *out) {
    client_application *app = app_at(c, "g", metadata ? 179 : 135);
    if (app == NULL)
        return false;
    mc_world_runtime *world = app->active_world;
    if (!math_at(c, "g", metadata ? 185 : 141))
        return false;
    const int32_t x = mc_math_floor_double(entity->base.x);
    const int32_t y = mc_math_floor_double(entity->base.y);
    const int32_t z = mc_math_floor_double(entity->base.z);
    if (world == NULL)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL,
                    "actual sleeping camera null World receiver", "g", "(F)V", metadata ? 203 : 159,
                    false);
    const mc_world_storage_status status =
        metadata ? mc_world_runtime_metadata_get(world, x, y, z, out, &c->failure.world)
                 : mc_world_runtime_block_get(world, x, y, z, out, &c->failure.world);
    return status == MC_WORLD_STORAGE_OK ||
           fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED, c->failure.world.operation, "g", "(F)V",
                metadata ? 203 : 159, true);
}

static bool orient(mc_client_renderer_camera_call *c) {
    mc_entity *entity = view(c, "g");
    if (c->failure.status != MC_CLIENT_RENDERER_CAMERA_OK || !local(c, entity, "g", 9) ||
        !fp(c, "g", 14))
        return false;
    mc_entity_base_fields *base = &entity->base;
    float offset = at_fsub(base->offset, 1.62f);
    const double x = position(base->aJ, base->x, c->state.partial);
    const double y = at_sub(position(base->aK, base->y, c->state.partial), (double)offset);
    const double z = position(base->aL, base->z, c->state.partial);
    mc_client_entity_renderer_fields *f = &c->state.owner->fields;
    if (!rotate(c, "g", 95, lerp(f->v_to_C[7], f->v_to_C[6], c->state.partial), 0, 0, 1))
        return false;
    if (mc_player_is_sleeping(entity, &c->state.sleeping, &c->failure.player) != MC_PLAYER_OK)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED, c->failure.player.operation, "g",
                    "(F)V", 99, c->failure.player.status != MC_PLAYER_NULL_REFERENCE);
    mc_game_settings_state options;
    if (c->state.sleeping) {
        if (!fp(c, "g", 106))
            return false;
        offset = narrow(at_add((double)offset, 1.0));
        if (!translate(c, "g", 115, 0, 0.3f, 0) || !settings(c, "g", 122, 125, &options))
            return false;
        if (!options.values.debug_camera) {
            int32_t id;
            if (!bed_query(c, entity, false, &id))
                return false;
            client_application *app = c->state.owner->fields.j;
            const mc_block *bed = NULL;
            if (mc_registry_block_static_get(app->registry, "T", &bed, &c->failure.registry) !=
                MC_REGISTRY_OK)
                return fail(c, MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED,
                            "camera actual bed static active use", "g", "(F)V", 166, true);
            const mc_block_fields *fields = mc_block_fields_get(bed);
            if (fields == NULL)
                return fail(c, MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL,
                            "camera actual null bed Block", "g", "(F)V", 169, false);
            if (id == fields->bn) {
                int32_t value;
                if (!bed_query(c, entity, true, &value) || !fp(c, "g", 219) ||
                    !rotate(c, "g", 223, convert(mc_java_i32_mul(value & 3, 90)), 0, 1, 0))
                    return false;
            }
            if (!fp(c, "g", 238) ||
                !rotate(c, "g", 249, at_fadd(lerp(base->aU, base->yaw, c->state.partial), 180.0f),
                        0, -1, 0) ||
                !fp(c, "g", 253) ||
                !rotate(c, "g", 272, lerp(base->aV, base->aT, c->state.partial), -1, 0, 0))
                return false;
        }
    } else {
        if (!settings(c, "g", 282, 285, &options))
            return false;
        if (options.values.third_person_view) {
            if (!fp(c, "g", 299) || !third_person(c, entity, &options, x, y, z))
                return false;
        } else if (!translate(c, "g", 721, 0, 0, -0.1f))
            return false;
    }
    if (!settings(c, "g", 728, 731, &options))
        return false;
    if (!options.values.debug_camera) {
        if (!fp(c, "g", 746) ||
            !rotate(c, "g", 756, lerp(base->aV, base->aT, c->state.partial), 1, 0, 0) ||
            !fp(c, "g", 765) ||
            !rotate(c, "g", 781, at_fadd(lerp(base->aU, base->yaw, c->state.partial), 180.0f), 0, 1,
                    0))
            return false;
    }
    if (!translate(c, "g", 787, 0, offset, 0) || !fp(c, "g", 802))
        return false;
    c->state.eye_offset = offset;
    c->state.cloud_x = position(base->aJ, base->x, c->state.partial);
    c->state.cloud_y = at_sub(position(base->aK, base->y, c->state.partial), (double)offset);
    c->state.cloud_z = position(base->aL, base->z, c->state.partial);
    client_application *app = app_at(c, "g", 855);
    if (app == NULL)
        return false;
    if (app->g == NULL)
        return fail(c, MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL,
                    "actual cloud-fog null RenderGlobal receiver", "g", "(F)V", 865, false);
    /* Actual n.a(DDDF) is literal false0. No World/render query is added. */
    f->D = false;
    c->state.D_stored = true;
    return true;
}

static bool body(mc_client_renderer_camera_call *c) {
    mc_game_settings_state options;
    mc_client_source_px *owner = c->state.owner;
    mc_client_display *display = owner->services.display;
    if (!settings(c, "a", 8, 11, &options) || !fp(c, "a", 15))
        return false;
    owner->fields.k = convert(mc_java_i32_shr(256, options.values.render_distance));
    c->state.far_distance = owner->fields.k;
    c->state.k_stored = true;
    if (!gl(c, mc_client_display_source_matrix_mode(display, 5889, &c->failure.display), "a", 22) ||
        !gl(c, mc_client_display_source_load_identity(display, &c->failure.display), "a", 25))
        return false;
    c->state.projection_reset = true;
    if (!settings(c, "a", 35, 38, &options))
        return false;
    if (options.values.anaglyph) {
        if (!fp(c, "a", 50) || !translate(c, "a", 55,
                                          at_fmul(convert(mc_java_i32_neg(mc_java_i32_sub(
                                                      mc_java_i32_mul(c->state.eye, 2), 1))),
                                                  0.07f),
                                          0, 0))
            return false;
    }
    c->state.zoomed = owner->fields.E != 1.0;
    if (c->state.zoomed) {
        if (!fp(c, "a", 71) ||
            !translate(c, "a", 79, narrow(owner->fields.F), narrow(-owner->fields.G), 0) ||
            !gl(c,
                mc_client_display_source_scale(display, owner->fields.E, owner->fields.E, 1.0,
                                               &c->failure.display),
                "a", 91))
            return false;
    }
    if (!fov(c, &c->state.fov))
        return false;
    c->state.fov_returned = true;
    client_application *app = app_at(c, "a", c->state.zoomed ? 103 : 139);
    if (app == NULL)
        return false;
    c->state.width = app->d;
    app = app_at(c, "a", c->state.zoomed ? 111 : 147);
    if (app == NULL || !fp(c, "a", c->state.zoomed ? 114 : 150))
        return false;
    c->state.height = app->e;
    if (!gl(c,
            mc_client_display_source_perspective(
                display, c->state.fov, at_fdiv(convert(c->state.width), convert(c->state.height)),
                0.05f, at_fmul(owner->fields.k, 2.0f), &c->failure.display),
            "a", c->state.zoomed ? 124 : 160) ||
        !gl(c, mc_client_display_source_matrix_mode(display, 5888, &c->failure.display), "a",
            166) ||
        !gl(c, mc_client_display_source_load_identity(display, &c->failure.display), "a", 169))
        return false;
    c->state.modelview_reset = true;
    if (!settings(c, "a", 176, 179, &options))
        return false;
    if (options.values.anaglyph &&
        (!fp(c, "a", 190) ||
         !translate(c, "a", 196,
                    at_fmul(convert(mc_java_i32_sub(mc_java_i32_mul(c->state.eye, 2), 1)), 0.1f), 0,
                    0)))
        return false;
    if (!hurt(c))
        return false;
    c->state.hurt_returned = true;
    if (!settings(c, "a", 208, 211, &options))
        return false;
    if (options.values.view_bobbing && !bob(c))
        return false;
    c->state.bob_returned = true;
    app = app_at(c, "a", 226);
    if (app == NULL)
        return false;
    mc_entity *player = app->h;
    if (!capture(c, player))
        return fail(c, MC_CLIENT_RENDERER_CAMERA_NATIVE_FAILED, "camera portal Player occurrence",
                    "a", "(FI)V", 225, true);
    if (!local(c, player, "a", 229) || !fp(c, "a", 252))
        return false;
    c->state.portal = lerp(player->local_player->fields.player.C,
                           player->local_player->fields.player.B, c->state.partial);
    if (c->state.portal > 0) {
        const float portal = c->state.portal;
        float scale =
            at_fsub(at_fdiv(5.0f, at_fadd(at_fmul(portal, portal), 5.0f)), at_fmul(portal, 0.04f));
        scale = at_fmul(scale, scale);
        const float angle = at_fmul(at_fadd(convert(owner->fields.l), c->state.partial), 20.0f);
        if (!rotate(c, "a", 304, angle, 0, 1, 1) || !fp(c, "a", 309) ||
            !gl(c,
                mc_client_display_source_scale(display, (double)at_fdiv(1.0f, scale), 1, 1,
                                               &c->failure.display),
                "a", 313) ||
            !fp(c, "a", 320) ||
            !rotate(c, "a", 330,
                    at_fmul(-at_fadd(convert(owner->fields.l), c->state.partial), 20.0f), 0, 1, 1))
            return false;
        c->state.portal_applied = true;
    }
    if (!orient(c))
        return false;
    c->state.orient_returned = true;
    c->state.returned = true;
    c->state.source_pc = 338;
    return true;
}

static mc_client_renderer_camera_status result(mc_client_renderer_camera_error *e,
                                               mc_client_renderer_camera_status status,
                                               const char *operation) {
    if (e != NULL)
        *e = (mc_client_renderer_camera_error){.status = status,
                                               .operation = operation,
                                               .native_policy =
                                                   status != MC_CLIENT_RENDERER_CAMERA_OK};
    return status;
}

mc_client_renderer_camera_status
entity_renderer_camera_entered(mc_client_source_px *owner, mc_math_classes *math, float partial,
                               int32_t eye, mc_client_renderer_camera_call **out,
                               mc_client_renderer_camera_error *error) {
    if (owner == NULL || out == NULL || *out != NULL || math == NULL)
        return result(error, MC_CLIENT_RENDERER_CAMERA_ARGUMENT, "camera entered arguments");
    if (!owner->active || owner->phase != MC_CLIENT_ENTITY_RENDERER_READY)
        return result(error, MC_CLIENT_RENDERER_CAMERA_BUSY, "camera requires active parent PX");
    mc_client_renderer_camera_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return result(error, MC_CLIENT_RENDERER_CAMERA_ALLOCATION,
                      "camera retained call allocation");
    c->state.owner = owner;
    c->state.partial = partial;
    c->state.eye = eye;
    c->state.error = &c->failure;
    *out = c;
    if (mc_math_classes_retain(math, &c->failure.math) != MC_MATH_CLASS_OK) {
        fail(c, MC_CLIENT_RENDERER_CAMERA_NATIVE_FAILED, "camera MathHelper native occurrence", "a",
             "(FI)V", 0, true);
    } else {
        c->math = math;
        c->state.active = true;
        (void)body(c);
        c->state.active = false;
    }
    if (error != NULL)
        *error = c->failure;
    return c->failure.status;
}

bool mc_client_entity_renderer_camera_inspect(const mc_client_renderer_camera_call *c,
                                              mc_client_renderer_camera_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_renderer_camera_status
mc_client_entity_renderer_camera_retire(mc_client_renderer_camera_call **slot,
                                        mc_client_renderer_camera_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_RENDERER_CAMERA_ARGUMENT, "camera cleanup slot");
    mc_client_renderer_camera_call *c = *slot;
    if (c == NULL)
        return result(error, MC_CLIENT_RENDERER_CAMERA_OK, NULL);
    if (c->state.active)
        return result(error, MC_CLIENT_RENDERER_CAMERA_BUSY, "camera active cleanup");
    for (size_t i = 0; i < 8; ++i)
        if (mc_world_raytrace_retire(&c->rays[i]) != MC_WORLD_RAYTRACE_OK)
            return result(error, MC_CLIENT_RENDERER_CAMERA_BUSY, "camera ray cleanup");
    c->failure.ray = NULL;
    while (c->vector_count != 0) {
        mc_entity_vector *vector = c->vectors[c->vector_count - 1];
        if (mc_entity_vector_release(c->state.owner->services.models->entities, vector) !=
            MC_ENTITY_OK)
            return result(error, MC_CLIENT_RENDERER_CAMERA_BUSY, "camera Vec3 cleanup");
        c->vectors[--c->vector_count] = NULL;
    }
    mc_math_class_error math_cleanup;
    if (mc_math_classes_release(&c->math, &math_cleanup) != MC_MATH_CLASS_OK) {
        const mc_client_renderer_camera_status status =
            result(error, MC_CLIENT_RENDERER_CAMERA_BUSY, "camera MathHelper cleanup");
        if (error != NULL)
            error->math = math_cleanup;
        return status;
    }
    while (c->capture_count != 0)
        mc_entity_release(c->captures[--c->capture_count]);
    free(c);
    *slot = NULL;
    return result(error, MC_CLIENT_RENDERER_CAMERA_OK, NULL);
}
