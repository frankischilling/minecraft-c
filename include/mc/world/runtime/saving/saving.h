#ifndef MC_WORLD_RUNTIME_SAVING_SAVING_H
#define MC_WORLD_RUNTIME_SAVING_SAVING_H

#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/saving/errors.h"

/* Write one exact committed GAME Chunk held by the actual World Store.
 * Timestamp is an explicit native region-transport input. The concrete runtime
 * lifecycle uses its real platform clock; this entry does not sample a clock.
 * Hold the World, selected Info and exact Chunk through both session checks,
 * source field/List traversal, live Entity/Tile builds and region commit. Earlier q
 * changes survive failure; growth is published only after successful commit.
 * Every Item occurrence calls its actual Boolean serializer in slice/List
 * order, including duplicates and false/dead results. Other Entity types are
 * not implemented. Known records use current semantic fields and actual
 * Entity/Stack provenance. Unknown factory-NULL source records are preserved
 * by the Store's completed import journal, never placed in live containers.
 * Both completed live lists are required before lower composition/encoding.
 *
 * The NBT operation ceiling combines all temporary record nodes/trees, list
 * pointer scratch and independent list/composed/encoded outputs while live.
 * Each storage serializer additionally charges its own current Entity/Stack
 * graph. World/Store/container/class graphs and their native ceilings remain
 * separate; this is not a whole-World/process heap bound or Java OOM timing.
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
/* GAME full saves check the session, mutate the actual Info version to19132,
 * serialize its genuinely empty live Player List, save the actual SavedData
 * named List, then save cached Chunks. Nonempty Player/named Lists remain
 * unsupported. Empty Player Lists omit retained Info.Player without mutating
 * it. Metadata uses its real clock and precedes Chunk SizeOnDisk growth.
 * Native checked transport/unknown-field policies remain in effect; original
 * caught file failures are not qualified. Info-bound COMPOSITION has no live
 * Player List and retains its explicit unsupported metadata result. */
mc_chunk_lifecycle_status mc_world_runtime_save_detailed(mc_world_runtime *world, bool force,
                                                         mc_world_runtime_lifecycle_error *error);
mc_chunk_lifecycle_status
mc_world_runtime_save_client_detailed(mc_world_runtime *world, int32_t pass, bool *completed,
                                      mc_world_runtime_lifecycle_error *error);
mc_chunk_lifecycle_status
mc_world_runtime_maintain_detailed(mc_world_runtime *world, bool *provider_result,
                                   mc_world_runtime_lifecycle_error *error);

#endif
