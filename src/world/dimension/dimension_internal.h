#ifndef MC_WORLD_DIMENSION_INTERNAL_H
#define MC_WORLD_DIMENSION_INTERNAL_H

#include "mc/world/dimension/dimension.h"

/* The actual virtual light-table operation, also called by final bind after
 * biome setup. Requires a READY owner with its constructor-created f storage
 * and the strict FP environment. Fills that same array in source index order;
 * no World, biome, flags, source PCs or clock are read or changed. A standalone
 * call does not stand in for successful bind/setup. Admission failure preserves
 * the array and all other owner fields. */
mc_world_dimension_status
mc_world_dimension_initialize_light_table(mc_world_dimension *dimension,
                                          mc_world_dimension_error *error);

#endif
