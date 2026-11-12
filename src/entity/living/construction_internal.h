#ifndef MC_ENTITY_LIVING_CONSTRUCTION_INTERNAL_H
#define MC_ENTITY_LIVING_CONSTRUCTION_INTERNAL_H
#include "mc/entity/player/player.h"

/* Actual LS body after its SN superclass. Destination is the real embedded LS
 * subobject of a concrete Player or Mob; neither a second owner nor a mirror.
 * The same shared Math profile supplies all four production draws. */
mc_entity_status mc_entity_construct_living(mc_entity *, mc_living_entity_fields *,
                                            unsigned int *source_pc, mc_entity_error *);
#endif
