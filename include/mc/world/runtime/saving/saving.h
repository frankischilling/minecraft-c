#ifndef MC_WORLD_RUNTIME_SAVING_SAVING_H
#define MC_WORLD_RUNTIME_SAVING_SAVING_H

#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/saving/errors.h"

/* Write one exact committed GAME Chunk held by its actual World Store.
 * Source Stores use the canonical ld owner: first session outside the catch,
 * region stream16 before fresh root/Level24..44, serializer's second session,
 * genuine field/List traversal, plain NBT57, explicit close61, then actual
 * Info65/size74, late signed growth89 and Info store94. Timestamp is used only
 * by the unchanged legacy native region transport; source QJ samples its own
 * real clock at its reached commit423. No early source timestamp is sampled.
 *
 * Source Entity occurrences call actual Boolean SN.c, including duplicate,
 * dead and unnamed/Player false results; supported Item/FallingSand/Minecart
 * writers use semantic fields without storage overlays. Missing live Mob
 * writers remain explicit reached dependencies. Source Chest/Spawner/Piston
 * output uses fresh semantic fields/direct Stack values. Intermediate native
 * policy refuses unconsumed opaque Entity/TileEntity records before commit.
 * Legacy stores keep their separate provenance/opaque preservation composers.
 *
 * Earlier q changes survive failure. There is NO source serializer finally.
 * The ld owner retains partial output and input trees, admits native terminal
 * cleanup, and disables source continuation after failure. Native release ends
 * codec state before freeing borrowed input; it never finishes/commits it.
 * Complete per-attempt source/stream/tree causes are in source_chunk; legacy
 * composition/encoding fields describe only operations actually reached there.
 *
 * Native operation limits charge overlapping new record/list/fresh-root trees
 * and their scratch; concrete output/region and external owner graphs retain
 * separate budgets. Fixed native shapes and immutable live input spans are
 * admitted explicitly; Java mutable-NBT allocation/OOM/HashMap wire order and
 * complete Throwable/printing behavior are not claimed. All input/output/error
 * storage stays disjoint and owners remain exclusive through return. */
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
 * serialize only the first selected live Player via direct Entity data write
 * (never Boolean named-Entity save), save the actual SavedData named List,
 * then save cached Chunks. Canonical LocalPlayer emits the complete ordinary
 * Living/Inventory/Player/Score fields. Unknown Player subtypes remain explicit
 * dependencies. Named MapData values use actual dirty fields and names.
 * Empty Lists or a selected null Player omit retained Info.Player without mutating
 * it. Metadata uses its real clock and precedes Chunk SizeOnDisk growth.
 * Source metadata and Chunk output use their actual concrete owners; legacy
 * checked transport/unknown-field policies remain separate. Reached missing
 * Exception/IOException printing is an explicit dependency. Info-bound COMPOSITION has no live
 * Player List and retains its explicit unsupported metadata result. */
mc_chunk_lifecycle_status mc_world_runtime_save_detailed(mc_world_runtime *world, bool force,
                                                         mc_world_runtime_lifecycle_error *error);
/* Actual synchronous progress argument join for fd.a(Z,yb). Copy only the
 * canonical save context's progress owner/callback for this call; preserve its
 * same session/metadata/auxiliary/Chunk owners, entered-World hold and typed
 * per-attempt error. No runtime option/context is rebound. The concrete caller
 * must invoke its actual progress owner; this seam supplies no successful
 * callback, Loading screen or returned source result. NULL callback represents
 * the genuine null progress argument. Owner/error remain live and disjoint.
 * A nonnull callback accepts the actual mutable Chunk descriptor and therefore
 * permanently discards the source provider's optional private index before
 * dispatch, even if the save subsequently fails. */
mc_chunk_lifecycle_status
mc_world_runtime_save_with_progress_detailed(mc_world_runtime *, bool force, void *progress_owner,
                                             mc_world_save_progress_callback,
                                             mc_world_runtime_lifecycle_error *);
mc_chunk_lifecycle_status
mc_world_runtime_save_client_detailed(mc_world_runtime *world, int32_t pass, bool *completed,
                                      mc_world_runtime_lifecycle_error *error);
mc_chunk_lifecycle_status
mc_world_runtime_maintain_detailed(mc_world_runtime *world, bool *provider_result,
                                   mc_world_runtime_lifecycle_error *error);

#endif
