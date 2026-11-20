#ifndef MC_WORLD_RUNTIME_MUTATION_FURNACE_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_FURNACE_INTERNAL_H

#include "../world_execution_internal.h"

/* The caller has completed the actual RW addition before private TC.h. The
 * supplied World is that evaluated argument, never a later Chunk.d load. */
bool mc_world_mutation_furnace_orient(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                      int32_t, mc_world_mutation_error *,
                                      mc_world_mutation_execution_call *);

/* Actual TC.a(Z,World,III). Its saved Tile occurrence survives both real Block
 * hooks and is validated/reinstalled after metadata restoration. No finally
 * repairs the class-static suppression field after a failed source prefix. */
bool mc_world_mutation_furnace_switch(mc_registry_classes *, mc_world_runtime *, bool, int32_t,
                                      int32_t, int32_t, mc_world_mutation_error *);

#endif
