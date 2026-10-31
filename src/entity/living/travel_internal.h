#ifndef MC_ENTITY_LIVING_TRAVEL_INTERNAL_H
#define MC_ENTITY_LIVING_TRAVEL_INTERNAL_H

#include "mc/entity/living/travel.h"

/* The caller holds the actual READY represented Living owner and defining
 * Registry. These entered source bodies add no public LocalPlayer admission,
 * owner copy, statistic, constructor or entropy binding. */
bool mc_living_travel_entered(mc_entity *, const mc_player_travel_context *, float strafe,
                              float forward, mc_player_travel_error *);
bool mc_living_ladder_entered(mc_entity *, const mc_player_travel_context *, bool *,
                              mc_player_travel_error *);
bool mc_living_jump_entered(mc_entity *, mc_player_travel_error *);
typedef struct mc_mob_tick_scope mc_mob_tick_scope;
bool mc_living_travel_mob_entered(mc_mob_tick_scope *, float strafe, float forward,
                                  mc_player_travel_error *);

#endif
