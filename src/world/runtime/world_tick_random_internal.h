#ifndef MC_WORLD_RUNTIME_WORLD_TICK_RANDOM_INTERNAL_H
#define MC_WORLD_RUNTIME_WORLD_TICK_RANDOM_INTERNAL_H

#include "mc/world/runtime/random_tick.h"

/* Actual current World.P Set.clear, shared by fd.n and native source-reentry
 * fixtures. The canonical owned Set, source modCount wrap and native allocation
 * policy apply; no World/Chunk/Player/RNG state is synthesized. */
mc_world_storage_status mc_world_random_tick_active_set_clear(mc_world_runtime *,
                                                              mc_world_random_tick_error *);

#endif
