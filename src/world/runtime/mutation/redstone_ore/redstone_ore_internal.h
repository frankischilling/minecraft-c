#ifndef MC_WORLD_RUNTIME_MUTATION_REDSTONE_ORE_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_REDSTONE_ORE_INTERNAL_H

#include "mc/world/runtime/mutations.h"

/* Actual ordinary client BS.a(World,III,Random). The caller holds its concrete
 * evaluated Block and defining Registry graph. World and Random may be NULL:
 * Random is never read, and a nonmatching own/live-lit ID returns before any
 * World operation. A matched branch captures the live unlit bn before invoking
 * the notifying setter. The setter Boolean is discarded, without scheduling.
 * The caller supplies fresh, disjoint error storage and retains the actual
 * World/producer graph through any failed nested mutation diagnostic. */
bool mc_world_mutation_redstone_ore_tick(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                         int32_t, mc_java_random *, mc_world_mutation_error *);

#endif
