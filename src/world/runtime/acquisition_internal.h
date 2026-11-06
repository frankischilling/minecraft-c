#ifndef MC_WORLD_RUNTIME_ACQUISITION_INTERNAL_H
#define MC_WORLD_RUNTIME_ACQUISITION_INTERNAL_H

#include "mc/world/runtime/acquisition_error.h"

/* Actual client OK.c on an explicitly captured provider. Its actual World
 * owner remains held by the enclosing caller. Pending removal occurs even on
 * hits; generation/population use that same provider receiver. No source-v
 * substitution or mutable cache export is performed. */
mc_chunk_acquire_status
mc_world_runtime_acquire_captured_provider(mc_chunk_provider *, int32_t, int32_t, mc_chunk_state **,
                                           mc_world_runtime_acquisition_error *);

mc_chunk_acquire_status mc_world_runtime_get_detailed(mc_world_runtime *runtime, int32_t x,
                                                      int32_t z, mc_chunk_state **out,
                                                      mc_world_runtime_acquisition_error *error);

/* The same single GET, admission and failure receipt. A successful call leaves
 * error untouched; callers may inspect it only after a non-OK result. This
 * keeps descendant diagnostic construction off scalar-query cache hits. */
mc_chunk_acquire_status mc_world_runtime_get_failure(mc_world_runtime *runtime, int32_t x,
                                                     int32_t z, mc_chunk_state **out,
                                                     mc_world_runtime_acquisition_error *error);

/* The ordinary block body, with the same failure-only diagnostic convention.
 * Every failed result supplies the complete acquisition/unreached baseline,
 * including failures before or after the actual provider invocation. */
mc_world_storage_status
mc_world_runtime_block_get_failure(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                   int32_t *out, mc_world_storage_error *error,
                                   mc_world_runtime_acquisition_error *acquisition);

/* The same convention and one-provider-call contract for World.e(III)I. */
mc_world_storage_status
mc_world_runtime_metadata_get_failure(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                      int32_t *out, mc_world_storage_error *error,
                                      mc_world_runtime_acquisition_error *acquisition);

#endif
