#ifndef MC_WORLD_RUNTIME_MOB_HURT_OBSERVATION_H
#define MC_WORLD_RUNTIME_MOB_HURT_OBSERVATION_H

#include "mc/entity/classes/classes.h"

/* Passive values copied from an actual Mob diagnostic, before the
 * tagged child can be reused. Identities are comparison-only, never owned or
 * dereferenced. Site strings are static source labels. No source getter,
 * callback, allocation or gameplay decision is introduced by observation. */
typedef struct mc_world_mob_hurt_receipt {
    bool present, returned, result, parent_returned;
    bool health_stored, statistic_returned, prior_pending;
    uintptr_t actor_identity, target_identity;
    mc_entity_class_kind actor_kind;
    int32_t damage, health_before, health_after, statistic_amount;
    const char *caller_class, *caller_method, *caller_descriptor;
    int32_t caller_pc, return_pc;
} mc_world_mob_hurt_receipt;

typedef struct mc_world_mob_hurt_observation {
    /* Reached caller hurt instructions, not fabricated callee entries. */
    uint64_t reached_calls, returned_calls, true_returns;
    bool saturated;
    mc_world_mob_hurt_receipt last;
} mc_world_mob_hurt_observation;

#endif
