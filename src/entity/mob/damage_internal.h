#ifndef MC_ENTITY_MOB_DAMAGE_INTERNAL_H
#define MC_ENTITY_MOB_DAMAGE_INTERNAL_H
#include "mc/entity/movement/effects.h"
/* Closed canonical client Zombie/Skeleton/Creeper, NULL attacker only. These actual
 * receivers inherit GZ -> LS damage; their sound overrides stay distinct.
 * The real effect facade
 * holds the actor; the caller holds the live Registry/World graph exclusively.
 * No admission of future Random/World/String/target fields occurs here.
 * Boolean out is published only after actual GZ return. Error is caller-owned,
 * contains complete value causes and borrows only the producer's live receipt.
 * No extra owned occurrence or cleanup obligation is created by this helper. */
bool mc_mob_monster_null_damage_entered(mc_entity *, mc_registry_classes *, int32_t, bool *,
                                        mc_entity_effect_error *);

/* Canonical client Sheep/Pig/Cow/Chicken. Sheep DL.a invokes symbolic BG.a,
 * resolving inherited LS directly; the other three inherit LS.a. The effect
 * facade already holds the receiver. A nonnull source is admitted/retained
 * only at actual LS144, then kept through later callbacks and released before
 * return. Remote/dead/repeated branches never inspect source or Math.
 * Math is the actual caller-supplied class namespace, borrowed/exclusive and
 * activated only at the reached knockback sqrt. No World/trig inference,
 * substitute source, retry cap, damage rollback or completed death is added. */
bool mc_mob_living_damage_entered(mc_entity *, mc_registry_classes *, mc_math_classes *,
                                  mc_entity *source, int32_t amount, bool *,
                                  mc_entity_effect_error *);
#endif
