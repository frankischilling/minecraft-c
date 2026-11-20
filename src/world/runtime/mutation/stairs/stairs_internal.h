#ifndef MC_WORLD_RUNTIME_MUTATION_STAIRS_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_STAIRS_INTERNAL_H

#include "mc/world/runtime/mutations.h"

/* Client SS delegates through the actual captured a reference. The enclosing
 * caller keeps its defining Registry and the wrapped Block graph live and
 * exclusive. NULL World/Random are admitted until the actual child uses them.
 * Neighbor resolution is UU's inherited empty body, never a wrapped call.
 * A failed actual child preserves its effects and deepest diagnostic beside
 * the reached SS invocation. No wrapped-kind palette or recursion cap occurs. */
bool mc_world_mutation_stairs_added(mc_world_runtime *, const mc_block *, int32_t, int32_t, int32_t,
                                    mc_world_mutation_error *);
bool mc_world_mutation_stairs_removed(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                      int32_t, mc_world_mutation_error *);
bool mc_world_mutation_stairs_neighbor(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                       int32_t, int32_t, mc_world_mutation_error *);
bool mc_world_mutation_stairs_tick(mc_world_runtime *, const mc_block *, int32_t, int32_t, int32_t,
                                   mc_java_random *, mc_world_mutation_error *);

#endif
