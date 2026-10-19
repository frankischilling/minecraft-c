#ifndef MC_WORLD_STORAGE_CHUNK_BUILD_INTERNAL_H
#define MC_WORLD_STORAGE_CHUNK_BUILD_INTERNAL_H

#include "mc/world/storage/chunk.h"

/* Build the existing eleven-field fresh schema directly from ACTUAL live
 * state/data, without constructing/copying a mc_stored_chunk owner. Requires
 * normal fixed data and no entity/tile records; actual generated children are
 * checked by the store dispatcher. Preserve existing scratch/result budgets,
 * exact field types/order and output-failure contracts. No state mutation. */
mc_stored_chunk_status mc_stored_chunk_build_fresh(const mc_chunk_state *state, int64_t world_time,
                                                   const mc_nbt_limits *limits,
                                                   mc_nbt_document **out,
                                                   mc_stored_chunk_error *error);

#endif
