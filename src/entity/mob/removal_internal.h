#ifndef MC_ENTITY_MOB_REMOVAL_INTERNAL_H
#define MC_ENTITY_MOB_REMOVAL_INTERNAL_H

#include "mc/entity/mob/removal.h"
#include "mc/entity/movement/effects.h"

/* Actual entered UW.K, with a held canonical READY client Slime. The nullable
 * output is a projection; pending work owns all failed prefixes even with
 * NULL output. One synchronous shared body, never Source retry or rollback. */
bool mc_slime_removal_entered(mc_entity *, mc_entity_effect_error *);

/* Native-only cleanup. Active work, actual pending/Chunk closes, unresolved
 * scope rights or checked release refusal preserve the SAME work. Published
 * children stay in their actual World/Chunk containers. Self-release is last. */
mc_entity_status mc_slime_removal_cleanup(mc_entity *, mc_entity_effect_error *);

#endif
