#ifndef MC_WORLD_RUNTIME_CHUNK_TICK_INTERNAL_H
#define MC_WORLD_RUNTIME_CHUNK_TICK_INTERNAL_H

#include "mc/world/runtime/chunk_tick.h"

/* Same exclusive, live, disjoint arguments and checked raw read as the public
 * accessor. Every call reloads the actual current data/blocks array and preserves
 * admission, source index/NULL failures, capture cursor/read counts and holds.
 * On success only the caller error is untouched. A nonnull error receives the
 * complete diagnostic on failure. Pre-admission rejection preserves the capture;
 * an entered read failure latches its failure and World error as the public API.
 * No array cache, Blank virtual shortcut, repair or callback is supplied. */
mc_world_storage_status mc_world_chunk_tick_raw_block_failure(mc_world_chunk_tick_capture *,
                                                              int32_t x, int32_t y, int32_t z,
                                                              int32_t *out,
                                                              mc_world_chunk_tick_error *error);

#endif
