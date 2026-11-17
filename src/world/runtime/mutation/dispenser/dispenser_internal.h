#ifndef MC_WORLD_RUNTIME_MUTATION_DISPENSER_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_DISPENSER_INTERNAL_H

#include "../world_execution_internal.h"

/* The actual RW addition has returned before XQ.h begins. World is that
 * evaluated argument; a later Chunk.d read cannot replace its occurrence. */
bool mc_world_mutation_dispenser_orient(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                        int32_t, mc_world_mutation_error *,
                                        mc_world_mutation_execution_call *);

#endif
