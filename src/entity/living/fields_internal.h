#ifndef MC_ENTITY_LIVING_FIELDS_INTERNAL_H
#define MC_ENTITY_LIVING_FIELDS_INTERNAL_H

#include "mc/entity/player/player.h"

/* Borrow the represented embedded LS subobject. This is neither admission nor
 * a snapshot: the caller owns phase, lifetime and source ordering. */
mc_living_entity_fields *mc_entity_living_fields(mc_entity *);

/* Entered ls.c(I)V on represented live Living storage, in every phase. The
 * caller owns its occurrence and reached receiver admission. This performs
 * only the nonpositive-health return, Java-wrapped add, upper clamp and by
 * store, in source order. False means absent native Living storage. */
bool mc_entity_living_heal_source(mc_entity *, int32_t amount);

#endif
