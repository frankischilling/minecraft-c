#ifndef MC_ENTITY_PLAYER_DAMAGE_WAKE_INTERNAL_H
#define MC_ENTITY_PLAYER_DAMAGE_WAKE_INTERNAL_H

#include "mc/entity/movement/effects.h"

bool mc_player_damage_wake_entered(mc_entity *, mc_registry_classes *, mc_entity_effect_error *);
bool mc_player_damage_wake_inspect(const mc_entity *, mc_entity_player_wake_state *);
mc_entity_status mc_player_damage_wake_cleanup(mc_entity *, mc_entity_effect_error *);

#endif
