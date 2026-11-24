#ifndef MC_CLIENT_TEXTURE_EFFECTS_ORIENTATION_H
#define MC_CLIENT_TEXTURE_EFFECTS_ORIENTATION_H
#include "../../entity/entity_internal.h"
#include "../../world/runtime/spawn_internal.h"
#include "../application_state.h"
#include "effect_internal.h"

static inline bool effect_app(mc_texture_effect_owner *o, int32_t pc, client_application **out,
                              mc_texture_effect_error *e) {
    if (o->app == NULL) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                               "effect actual Minecraft field receiver",
                               mc_texture_effect_class(o->base.kind), "a", pc);
        return false;
    }
    if (o->app->constructor_phase == MC_CLIENT_APP_RETIRING ||
        o->app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RETIRED) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_INVALID_PHASE,
                               "effect native App lifetime admission", NULL, NULL, -1);
        return false;
    }
    *out = o->app;
    return true;
}

static inline bool effect_provider_flag(mc_texture_effect_owner *o, mc_world_runtime *world,
                                        int32_t world_pc, int32_t provider_pc, bool *out,
                                        mc_texture_effect_error *e) {
    const char *source = mc_texture_effect_class(o->base.kind);
    if (world == NULL) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                               "effect actual World.t receiver", source, "a", world_pc);
        return false;
    }
    mc_world_runtime_fields fields;
    e->world.status = mc_world_runtime_fields_get(world, &fields);
    if (e->world.status != MC_WORLD_STORAGE_OK) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_WORLD_FAILED,
                               "effect actual World.t native storage", source, "a", world_pc);
        return false;
    }
    if (fields.t == NULL) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                               "effect actual provider.c receiver", source, "a", provider_pc);
        return false;
    }
    mc_world_provider_fields provider;
    const mc_world_provider_status status = mc_world_provider_fields_get(fields.t, &provider);
    e->provider = status;
    if (status != MC_WORLD_PROVIDER_OK) {
        e->world.operation = mc_world_provider_status_string(status);
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_WORLD_FAILED, "effect provider native storage",
                               source, "a", provider_pc);
        return false;
    }
    *out = provider.c;
    return true;
}

static inline bool effect_damp(mc_texture_effect_owner *o, double target, int32_t sub_pc,
                               int32_t loop_pc, int32_t velocity_pc, int32_t decay_pc,
                               int32_t angle_pc, mc_texture_effect_error *e) {
    const char *source = mc_texture_effect_class(o->base.kind);
    if (!mc_texture_effect_fp(e, source, sub_pc))
        return false;
    double difference = ed_sub(target, o->angle);
    size_t iterations = 0;
    while (difference < -0x1.921fb54442d18p+1) {
        if (iterations++ >= o->options.max_angle_iterations) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_LIMIT,
                                   "effect native repeated-angle work bound", source, "a", loop_pc);
            return false;
        }
        difference = ed_add(difference, 0x1.921fb54442d18p+2);
    }
    while (difference >= 0x1.921fb54442d18p+1) {
        if (iterations++ >= o->options.max_angle_iterations) {
            mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_LIMIT,
                                   "effect native repeated-angle work bound", source, "a", loop_pc);
            return false;
        }
        difference = ed_sub(difference, 0x1.921fb54442d18p+2);
    }
    if (difference < -1.0)
        difference = -1.0;
    if (difference > 1.0)
        difference = 1.0;
    o->velocity = ed_add(o->velocity, ed_mul(difference, 0x1.999999999999ap-4));
    o->last_store_pc = velocity_pc;
    o->velocity = ed_mul(o->velocity, 0x1.999999999999ap-1);
    o->last_store_pc = decay_pc;
    o->angle = ed_add(o->angle, o->velocity);
    o->last_store_pc = angle_pc;
    return true;
}
#endif
