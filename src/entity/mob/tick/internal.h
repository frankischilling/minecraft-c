#ifndef MC_ENTITY_MOB_TICK_INTERNAL_H
#define MC_ENTITY_MOB_TICK_INTERNAL_H
#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../../../world/runtime/construction_internal.h"
#include "../../../world/runtime/runtime_internal.h"
#include "../../living/fields_internal.h"
#include "../../player/tick_math_internal.h"
#include "../internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/entity/mob/tick.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/environment.h"
#include "mc/world/runtime/queries.h"

typedef struct mc_mob_tick_scope {
    mc_entity *entity;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_mob_tick_error *error;
    const mc_entity_tick_limits *environment_limits;
    const mc_player_action_context *action_context;
    const mc_player_action_limits *action_limits;
    size_t work, max_work;
    bool hold_transferred;
} mc_mob_tick_scope;

bool mc_mob_tick_behavior(mc_mob_tick_scope *);
bool mc_entity_mob_environment_entered(mc_mob_tick_scope *, mc_entity_tick_error *);
bool mc_entity_mob_action_entered(mc_mob_tick_scope *, mc_player_action_error *);

void mt_at(mc_mob_tick_scope *, const char *, const char *, const char *, unsigned int);
bool mt_fail(mc_mob_tick_scope *, mc_entity_status, const char *);
bool mt_null(mc_mob_tick_scope *, const char *);
bool mt_fp(mc_mob_tick_scope *);
bool mt_math(mc_mob_tick_scope *);
void mt_registry_classify(mc_mob_tick_scope *, const mc_registry_error *);
bool mt_brightness_fail(mc_mob_tick_scope *, const char *);
bool mt_work(mc_mob_tick_scope *);
bool mt_int(mc_mob_tick_scope *, int32_t, int32_t *);
bool mt_float(mc_mob_tick_scope *, float *);
bool mt_world(mc_mob_tick_scope *, mc_world_runtime **);
bool mt_nearest(mc_mob_tick_scope *, double, mc_entity **);
bool mt_brightness(mc_mob_tick_scope *, float *);
bool mt_sound(mc_mob_tick_scope *, const char *, float, float);
bool mt_literal(mc_mob_tick_scope *, const char *, const mc_registry_string **);
bool mt_sound_prepared(mc_mob_tick_scope *, mc_world_runtime *, const mc_registry_string *, float,
                       float);
bool mt_particle_prepared(mc_mob_tick_scope *, mc_world_runtime *, const mc_registry_string *,
                          double, double, double, double, double, double);
bool mt_particle(mc_mob_tick_scope *, const char *, double, double, double, double, double, double);
bool mt_look(mc_mob_tick_scope *, mc_entity *, float, float);
bool mt_eye(mc_mob_tick_scope *, mc_entity *, float *);
bool mt_visible(mc_mob_tick_scope *, mc_entity *, bool *);
bool mt_watch_byte(mc_mob_tick_scope *, int32_t, int8_t *);
bool mt_watch_byte_set(mc_mob_tick_scope *, int32_t, int8_t);
bool mt_watch_int_set(mc_mob_tick_scope *, int32_t, int32_t);
bool mt_watch_string(mc_mob_tick_scope *, int32_t, const mc_player_string **);
bool mt_target_get(mc_mob_tick_scope *, bool, mc_entity **);
bool mt_target_store(mc_mob_tick_scope *, bool, mc_entity *);
bool mt_water(mc_mob_tick_scope *, bool *);
bool mt_lava(mc_mob_tick_scope *, bool *);
bool mt_kill(mc_mob_tick_scope *);
bool mt_living_w(mc_mob_tick_scope *);
bool mt_concrete_w(mc_mob_tick_scope *);
bool mt_concrete_o(mc_mob_tick_scope *);
bool mt_creature(mc_mob_tick_scope *);
bool mt_slime_behavior(mc_mob_tick_scope *);
bool mt_squid_behavior(mc_mob_tick_scope *);
bool mt_wolf_behavior(mc_mob_tick_scope *);
bool mt_despawn(mc_mob_tick_scope *);
bool mt_base_behavior(mc_mob_tick_scope *);
bool mt_wolf_tail(mc_mob_tick_scope *);
bool mt_wolf_action_tail(mc_mob_tick_scope *);
bool mt_squid_action_tail(mc_mob_tick_scope *);
bool mt_chicken_action_tail(mc_mob_tick_scope *);

static inline mc_living_entity_fields *mt_fields(mc_mob_tick_scope *s) {
    return mc_entity_living_fields(s->entity);
}

static inline double mt_sqrt(double x) {
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(x)));
}
#endif
