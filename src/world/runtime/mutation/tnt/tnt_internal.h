#ifndef MC_WORLD_RUNTIME_MUTATION_TNT_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_TNT_INTERNAL_H

#include "mc/world/runtime/mutations.h"

/* Actual client AH.c(World,III,int), including its remote guard and both flag
 * branches. The enclosing caller owns/enters this exact live World and holds
 * the evaluated Block and defining Registry/Entity graphs. A false preserves
 * reached construction, publication, Random and callback effects. Deferred TNT
 * ticks/explosion are not executed by this synchronous operation. */
bool mc_world_mutation_tnt_activate(mc_world_runtime *, const mc_block *, int32_t, int32_t, int32_t,
                                    int32_t, mc_world_mutation_error *);

#endif
