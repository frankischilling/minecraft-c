#ifndef MC_ENTITY_MOB_DAMAGE_INTERNAL_H
#define MC_ENTITY_MOB_DAMAGE_INTERNAL_H
#include "mc/entity/movement/effects.h"
/* Closed canonical client Zombie, NULL attacker only. The real effect facade
 * holds the actor; the caller holds the live Registry/World graph exclusively.
 * No admission of future Random/World/String/target fields occurs here.
 * Boolean out is published only after actual GZ return. Error is caller-owned,
 * contains complete value causes and borrows only the producer's live receipt.
 * No extra owned occurrence or cleanup obligation is created by this helper. */
bool mc_mob_zombie_null_damage_entered(mc_entity *, mc_registry_classes *, int32_t, bool *,
                                       mc_entity_effect_error *);
#endif
