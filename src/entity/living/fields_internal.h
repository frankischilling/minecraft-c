#ifndef MC_ENTITY_LIVING_FIELDS_INTERNAL_H
#define MC_ENTITY_LIVING_FIELDS_INTERNAL_H

#include "mc/entity/player/player.h"

/* Borrow the represented embedded LS subobject. This is neither admission nor
 * a snapshot: the caller owns phase, lifetime and source ordering. */
mc_living_entity_fields *mc_entity_living_fields(mc_entity *);

#endif
