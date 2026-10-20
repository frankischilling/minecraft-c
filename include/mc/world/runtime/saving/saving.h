#ifndef MC_WORLD_RUNTIME_SAVING_SAVING_H
#define MC_WORLD_RUNTIME_SAVING_SAVING_H

#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/saving/errors.h"

/* Write one exact committed GAME Chunk held by the actual World Store.
 * Timestamp is an explicit native region-transport input. The concrete runtime
 * lifecycle uses its real platform clock; this entry does not sample a clock.
 * Hold the World, selected Info and exact Chunk through both session checks,
 * source field/List traversal, live Tile build and region commit. Earlier q
 * changes survive failure; growth is published only after successful commit.
 * Empty live Entity slices are supported; the first Entity serializer is an
 * explicit unfinished boundary, never an opaque-record substitute.
 *
 * Source e/f/g, their a arrays, h, m and l are read as current projections.
 * Fixed native spans must remain live under exclusive access. Source owner
 * admission and buffered checked transport are not original stream/OOM or
 * caught-Exception parity. Incomplete/retired containers cannot become saveable
 * through native teardown. Outputs/errors must be disjoint from all owners. */
mc_world_storage_status mc_world_runtime_write_game_chunk(mc_world_runtime *world,
                                                          mc_chunk_state *state, uint32_t timestamp,
                                                          uint32_t *growth,
                                                          mc_world_game_save_error *error);

/* Each call installs an independent stack frame for actual save callbacks and
 * restores its predecessor after synchronous nested calls. Complete save errors
 * come directly from that attempt, never from ambient last_error. No selected
 * Chunk callback means save remains empty. Success clears the whole result. */
mc_chunk_lifecycle_status
mc_world_runtime_save_chunks_detailed(mc_world_runtime *world, bool force, bool *completed,
                                      mc_world_runtime_lifecycle_error *error);
mc_chunk_lifecycle_status mc_world_runtime_save_detailed(mc_world_runtime *world, bool force,
                                                         mc_world_runtime_lifecycle_error *error);
mc_chunk_lifecycle_status
mc_world_runtime_save_client_detailed(mc_world_runtime *world, int32_t pass, bool *completed,
                                      mc_world_runtime_lifecycle_error *error);
mc_chunk_lifecycle_status
mc_world_runtime_maintain_detailed(mc_world_runtime *world, bool *provider_result,
                                   mc_world_runtime_lifecycle_error *error);

#endif
