#ifndef MC_WORLD_PROVIDER_INTERNAL_H
#define MC_WORLD_PROVIDER_INTERNAL_H

#include "mc/world/dimension/world_provider.h"

/* The actual virtual light-table operation, also called by final bind after
 * biome setup. Requires a READY owner with its constructor-created f storage
 * and the strict FP environment. Fills that same array in source index order;
 * no World, biome, flags, source PCs or clock are read or changed. A standalone
 * call does not stand in for successful bind/setup. Admission failure preserves
 * the array and all other owner fields. */
mc_world_provider_status mc_world_provider_initialize_light_table(mc_world_provider *dimension,
                                                                  mc_world_provider_error *error);

#endif
