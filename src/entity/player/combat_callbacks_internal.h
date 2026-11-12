#ifndef MC_ENTITY_PLAYER_COMBAT_CALLBACKS_INTERNAL_H
#define MC_ENTITY_PLAYER_COMBAT_CALLBACKS_INTERNAL_H

#include "mc/entity/movement/effects.h"
#include "mc/entity/player/player.h"

/* Entered actual GS.c(SN,I) / GS.a(LS). Caller holds the real READY LocalPlayer
 * and victim. No death work is owned here. Effects and child diagnostics survive
 * failure; the enclosing victim retains any pending death occurrence. */
bool mc_player_death_callback(mc_entity *player, mc_registry_classes *, mc_entity *victim,
                              int32_t score, bool score_callback, mc_player_site *site,
                              mc_entity_effect_error *);

#endif
