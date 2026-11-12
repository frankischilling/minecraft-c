#ifndef MC_ENTITY_PLAYER_DAMAGE_ALERT_INTERNAL_H
#define MC_ENTITY_PLAYER_DAMAGE_ALERT_INTERNAL_H

#include "mc/entity/movement/effects.h"

bool mc_player_damage_alert_entered(mc_entity *, mc_entity *, mc_entity_effect_error *);
bool mc_player_damage_alert_inspect(const mc_entity *, mc_entity_player_alert_state *);
mc_entity_status mc_player_damage_alert_cleanup(mc_entity *, mc_entity_effect_error *);

#endif
