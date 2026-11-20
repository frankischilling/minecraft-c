#ifndef MC_WORLD_RUNTIME_MUTATION_PISTON_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_PISTON_INTERNAL_H

#include "mutation_internal.h"

/* Optional explicit code dependency through legacy setters/notifications. The
 * NULL wrapper never infers a namespace from mutable World/Chunk operands. */
mc_world_storage_status mc_world_runtime_piston_set(mc_world_runtime_classes *, mc_world_runtime *,
                                                    int32_t, int32_t, int32_t, int32_t, bool,
                                                    int32_t, bool, bool *,
                                                    mc_world_mutation_error *);
mc_world_storage_status mc_world_runtime_piston_metadata(mc_world_runtime_classes *,
                                                         mc_world_runtime *, int32_t, int32_t,
                                                         int32_t, int32_t,
                                                         mc_world_mutation_error *);
bool mc_world_mutation_neighbors_with_code(mc_world_runtime_classes *, mc_world_runtime *, int32_t,
                                           int32_t, int32_t, int32_t, mc_world_mutation_error *);
bool mc_world_mutation_block_added_with_code(mc_world_runtime_classes *, mc_world_runtime *,
                                             const mc_block *, int32_t, int32_t, int32_t,
                                             mc_world_mutation_error *);
bool mc_world_mutation_block_removed_with_code(mc_world_runtime_classes *, mc_world_runtime *,
                                               const mc_block *, int32_t, int32_t, int32_t,
                                               mc_world_mutation_error *);
bool mc_world_mutation_block_neighbor_with_code(mc_world_runtime_classes *, mc_world_runtime *,
                                                const mc_block *, int32_t, int32_t, int32_t,
                                                int32_t, mc_world_mutation_error *);
bool mc_world_mutation_chunk_piston_set(mc_world_runtime_classes *, mc_world_chunk *, int32_t,
                                        int32_t, int32_t, int32_t, bool, int32_t, bool *,
                                        mc_world_mutation_error *);

/* Explicit defining UK namespace, independently borrowed from the evaluated
 * World argument. NULL stays unavailable only when actual UT.a allocates UK.
 * The enclosing root keeps these classes/Registry/World graphs live and
 * exclusive throughout the synchronous closure and diagnostic drainage. */
bool mc_world_mutation_piston_base_added(mc_world_runtime_classes *, mc_world_runtime *,
                                         const mc_block *, int32_t, int32_t, int32_t,
                                         mc_world_mutation_error *);
bool mc_world_mutation_piston_base_neighbor(mc_world_runtime_classes *, mc_world_runtime *,
                                            const mc_block *, int32_t, int32_t, int32_t, int32_t,
                                            mc_world_mutation_error *);
bool mc_world_mutation_piston_head_removed(mc_world_runtime_classes *, mc_world_runtime *,
                                           const mc_block *, int32_t, int32_t, int32_t,
                                           mc_world_mutation_error *);
bool mc_world_mutation_piston_head_neighbor(mc_world_runtime_classes *, mc_world_runtime *,
                                            const mc_block *, int32_t, int32_t, int32_t, int32_t,
                                            mc_world_mutation_error *);
/* Exact synchronous FD.d(IIIII), distinct from the FD.c scheduler. */
bool mc_world_mutation_piston_world_event(mc_world_runtime_classes *, mc_registry_classes *,
                                          mc_world_runtime *, int32_t, int32_t, int32_t, int32_t,
                                          int32_t, mc_world_mutation_error *);

#endif
