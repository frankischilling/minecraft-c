#ifndef MC_ENTITY_POSITION_INTERNAL_H
#define MC_ENTITY_POSITION_INTERNAL_H

#include "entity_internal.h"

/* Actual sn.e(DDD) arithmetic, after its x/y/z stores. No argument admission,
 * owner mutation, pointer dereference of aW, pool work or callback occurs.
 * Callers own strict FP admission and the reached bounds invocation. */
mc_entity_bounds mc_entity_source_position_values(const mc_entity *entity, double x, double y,
                                                  double z, float half_width, float height);

#endif
