#ifndef MC_WORLD_STORAGE_CHUNK_BUILD_INTERNAL_H
#define MC_WORLD_STORAGE_CHUNK_BUILD_INTERNAL_H

#include "mc/world/storage/chunk.h"

/* Same actual decoded owner/document/fixed arrays as the generic decoder, but
 * defer ALL Entities/TileEntities semantic getter/cast checks to their reached
 * GAME coordinator phases. Source q starts false; opaque list provenance is
 * not a successful Entity factory or Chunk insertion. Generic decode remains
 * unchanged. This retains the existing fixed-array/native codec allocation
 * policy, not original missing-array repair or per-allocation failure timing. */
mc_stored_chunk_status mc_stored_chunk_decode_game(const uint8_t *input, size_t length,
                                                   const mc_nbt_limits *limits,
                                                   mc_stored_chunk **out, size_t *consumed,
                                                   mc_stored_chunk_error *error);

/* Passive ownership-shape admission. No normalization, allocation or source
 * access; useful before checked teardown of the actual retained payload. */
bool mc_stored_chunk_storage_valid(const mc_stored_chunk *chunk);

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
