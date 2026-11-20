#ifndef MC_WORLD_RUNTIME_ACTIVATION_TILE_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_ACTIVATION_TILE_EXECUTION_INTERNAL_H

#include "activation_internal.h"
#include "runtime_execution_internal.h"

/* Actual entered Chunk TileEntity registration, shared with NULL legacy.
 * The required stable context may already hold coordinator A. Capture the
 * genuine nullable Chunk.d B at lm.e/hi.d6 BEFORE l10/values13, and enter B only
 * at invocation18. code is the actual defining-code caller's immutable VALUE
 * bundle; never derive it from B, A, side fields or an unrelated error arm.
 * No Chunk/World/TileEntity source field is rebound and no Store admission is
 * weakened. The actual c/G Map addAll body currently acquires nothing and
 * returns an explicitly empty result. That is not a replacement for it.
 *
 * result is required fresh/disjoint and carries native owner errors beside
 * the unchanged first Source cause. A genuinely refused checked pop publishes
 * B's created token/actual preclose status/failed operand/LOCALS_DRAINED phase.
 * Parent atomically forwards its close/pending VALUEs before frame loss.
 * Caller retries that explicit native close only; no Source body replay.
 * Kernel sidecar propagation/loading/TileEntity projection joins are separate
 * dependencies. No owning context, World scope or Math context retires here. */
mc_world_storage_status mc_world_runtime_register_chunk_tile_entities_in_execution(
    mc_entity_tick_execution *, mc_world_chunk *, mc_world_runtime_execution_code,
    mc_world_activation_error *, mc_world_runtime_execution_result *);

#endif
