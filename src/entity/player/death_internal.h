#ifndef MC_ENTITY_PLAYER_DEATH_INTERNAL_H
#define MC_ENTITY_PLAYER_DEATH_INTERNAL_H

#include "mc/entity/movement/effects.h"

bool mc_player_death_entered(mc_entity *, mc_registry_classes *, mc_math_classes *,
                             mc_entity *source, mc_entity_effect_error *);
bool mc_player_dead_entered(mc_entity *, mc_registry_classes *, mc_math_classes *,
                            mc_entity_effect_error *);
mc_entity_status mc_player_death_cleanup(mc_entity *, mc_entity_effect_error *);

#endif
