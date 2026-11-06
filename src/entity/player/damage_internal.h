#ifndef MC_ENTITY_PLAYER_DAMAGE_INTERNAL_H
#define MC_ENTITY_PLAYER_DAMAGE_INTERNAL_H

#include "mc/entity/movement/effects.h"

/* The effect facade owns the genuine dc and has completed the GS.a prefix
 * through its actual statistic152. Enter the SAME inherited LS.a at158,
 * preserving virtual GS armor, inherited sounds and the next death frontier.
 * Registry/Math/source are borrowed exclusively and read only when reached.
 * Publish the Boolean only after LS and the containing GS return161. */
bool mc_player_living_damage_entered(mc_entity *, mc_registry_classes *, mc_math_classes *,
                                     mc_entity *source, int32_t amount, bool *,
                                     mc_entity_effect_error *);

/* Actual virtual GS.b(I)V reached by LS67/103. The caller owns live Player
 * storage; IX value/wear children, bO and inherited LS health subtraction run
 * in Source order. Failure preserves every completed child/store. */
bool mc_player_health_damage_entered(mc_entity *, mc_registry_classes *, int32_t amount,
                                     mc_entity_effect_error *);

#endif
