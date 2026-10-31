#ifndef MC_ENTITY_MOVEMENT_EFFECTS_INTERNAL_H
#define MC_ENTITY_MOVEMENT_EFFECTS_INTERNAL_H
#include "mc/entity/entity.h"
/* Actual SN.ai()V store2/return5. Caller owns canonical admission/lifetime;
 * no facade, allocation, Source World access or diagnostic reset is inserted. */
void mc_entity_effect_attacked_entered(mc_entity *);
#endif
