#include "../mob/tick/internal.h"
#include "tick_internal.h"

bool mc_tick_suffocating_entered(mc_entity *entity, bool *out, mc_entity_tick_error *e,
                                 mc_mob_tick_scope *scope) {
    e->source_class = "gs";
    e->source_method = "L()Z";
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
        entity->local_player->fields.player.u) {
        *out = false;
        return true;
    }
    e->source_class = "sn";
    for (int32_t index = 0; index < 8; ++index) {
        e->suffocation_bytecode = 16;
        if (!mc_tick_fp(e, 16))
            return false;
        const float ox = mc_tick_fmul(
            mc_tick_fmul(mc_tick_fsub(mc_tick_i2f(index % 2), .5f), entity->base.width), .9f);
        const float oy = mc_tick_fmul(mc_tick_fsub(mc_tick_i2f((index >> 1) % 2), .5f), .1f);
        const float oz = mc_tick_fmul(
            mc_tick_fmul(mc_tick_fsub(mc_tick_i2f((index >> 2) % 2), .5f), entity->base.width),
            .9f);
        if (scope != NULL) {
            mt_at(scope, "sn", "L", "()Z", 65);
            if (!mt_math(scope))
                return false;
        }
        const int32_t x = mc_math_floor_double(mc_tick_dadd(entity->base.x, mc_tick_widen(ox)));
        /* The actual w virtual is reached again inside EACH source sample. */
        float eye = .12f;
        if (mc_entity_has_mob(entity))
            eye = mc_tick_fmul(entity->base.height,
                               mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_WOLF) ? .8f : .85f);
        const int32_t y = mc_math_floor_double(
            mc_tick_dadd(mc_tick_dadd(entity->base.y, mc_tick_widen(eye)), mc_tick_widen(oy)));
        const int32_t z = mc_math_floor_double(mc_tick_dadd(entity->base.z, mc_tick_widen(oz)));
        mc_world_runtime *world = entity->base.world;
        bool solid;
        e->suffocation_bytecode = 111;
        if (world == NULL)
            return mc_tick_null(e, "Suffocation null World h receiver", 111);
        if (!mc_tick_world(e, mc_world_runtime_solid_support_get(world, x, y, z, &solid, &e->solid),
                           "Suffocation actual World h", 111))
            return false;
        if (solid) {
            *out = true;
            return true;
        }
    }
    *out = false;
    return true;
}

bool mc_tick_suffocating(mc_entity *entity, bool *out, mc_entity_tick_error *e) {
    return mc_tick_suffocating_entered(entity, out, e, NULL);
}

mc_entity_status mc_entity_tick_suffocating(mc_entity *entity, bool *out,
                                            mc_entity_tick_error *error) {
    mc_entity_tick_error frame;
    mc_entity_tick_error_init(&frame);
    if (out == NULL)
        mc_tick_fail(&frame, MC_ENTITY_INVALID_ARGUMENT, "Suffocation output", 0);
    else if (mc_tick_enter(entity, &frame)) {
        bool value;
        if (mc_tick_suffocating(entity, &value, &frame))
            *out = value;
        mc_entity_release(entity);
    }
    return mc_tick_finish(&frame, error);
}
