#ifndef MC_WORLD_RUNTIME_ENTITIES_H
#define MC_WORLD_RUNTIME_ENTITIES_H

#include "mc/entity/entity.h"
#include "mc/world/chunk/object.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/entity_errors.h"

/* Bind the exact application Entity classes above all Worlds of this class.
 * Binding requires matching side/profile, no earlier World allocation, and no
 * existing Entity binding. It retains one native class hold without class
 * initialization, ID consumption or Random/Math activity. WorldClasses releases
 * that hold at destruction; the application profile still outlives both owners.
 * An absent binding stays an explicit dependency at reached Entity allocation.
 * Binding the second of Registry/Entity classes also joins Registry to the same
 * canonical Entity owner, retaining its one independent Registry hold. A
 * conflicting pair fails without publishing the WorldClasses binding. Release
 * WorldClasses, then Registry, then Entity classes; passive Class cells never
 * initialize a target Entity or consume IDs/entropy. */
mc_entity_status mc_world_runtime_classes_bind_entities(mc_world_runtime_classes *classes,
                                                        mc_entity_classes *entities);

/* Actual base World.b(Entity)Z. A missing nonplayer chunk completes false;
 * successful publication appends the same Entity to the actual Chunk slice and
 * World.b before invoking the concrete WorldAccess hook. Duplicates own separate
 * native holds. It does not change Entity.world or precheck source membership.
 *
 * This runtime publishes actual Item, FallingSand, Minecart, LocalPlayer and Mob owners.
 * A real LocalPlayer bypasses presence, appends lowercase World.d, updates J
 * through fd.y, then enters the same Chunk/b/WorldAccess sequence. It retains
 * d/J even when a later getter, Chunk operation or callback fails. The live WorldAccess
 * loop dispatches entity_added after both publications, before a dedicated
 * World's Entity-index insertion. Reached failures retain those publications.
 * NULL Entity is admitted until its reached source field read. A nonnull Entity
 * must belong to this World's bound application EntityClasses, independently
 * of its nullable/different construction World. This is native type admission,
 * not class initialization or an additional source World field operation.
 *
 * Keep owners exclusive/live. Outputs/errors must be disjoint from all owners,
 * dependencies and each other. Failure preserves out and all reached effects;
 * error names the World/Chunk/WorldAccess PCs and deepest native cause. */
mc_world_storage_status mc_world_runtime_insert_entity(mc_world_runtime *world, mc_entity *entity,
                                                       bool *out, mc_world_entity_error *error);

/* Actual dedicated dp.a(int): first matching node in its real G index, or
 * NULL when absent/present-null. The output borrows that occurrence's hold;
 * retain it before a later index mutation if it must live independently.
 * Reject base Worlds, preserve out on failure, and consume no Entity ID,
 * Random, callbacks or Chunk operation. */
mc_world_storage_status mc_world_runtime_entity_by_id(mc_world_runtime *world, int32_t id,
                                                      mc_entity **out,
                                                      mc_world_entity_error *error);

/* Direct normal Chunk.a(Entity)V through its exact store-owned header. Hold and
 * lease that identity through the call. q is set before NULL/mismatch failures;
 * ordered Entity membership stores precede the selected List append. A reached
 * wrong-location diagnostic remains unsupported. No World.b/WorldAccess operation
 * is performed by this direct Chunk call. */
mc_world_storage_status mc_world_runtime_chunk_add_entity(mc_world_chunk *chunk, mc_entity *entity,
                                                          mc_world_entity_error *error);

typedef struct mc_world_terminal_limits {
    size_t max_entries, max_bytes; /* Native scratch only; zero keeps its literal value. */
} mc_world_terminal_limits;

typedef struct mc_world_terminal_inventory {
    size_t entities, tile_entities, entity_occurrences, tile_entity_occurrences;
    size_t expected_world_bindings;
    bool piston_discard_required;
} mc_world_terminal_inventory;

mc_world_terminal_limits mc_world_runtime_terminal_default_limits(void);

/* Additional, nonmutating admission before terminal World destruction. The
 * first supported scope is one READY CLIENT GAME World, with canonical typed
 * containers/provider and no active calls, leases, WorldAccess registrations
 * or unsupported child-owner graph. Count every actual occurrence by identity,
 * including duplicate Lists/maps/slices, and each distinct object's ONE World
 * backedge. Canonical Mob look and Creature target fields contribute their
 * actual owning occurrences, including aliases and cycles. Unimplemented path
 * owners and active Mob calls refuse. External Entity/Tile/Provider holds or unexplained direct
 * World bindings refuse before any owner, source phase, container, cache or provider mutation.
 * Separately bounded scratch allocations never charge source ledgers. No source/class use,
 * source/user callback, I/O, retain/release or cleanup runs. The genuine constructor-owned active
 * Chunk coordinate Set may be nonempty; its descriptor, keys and traversal membership are validated
 * without a source clear. Its records own no World/Chunk/Entity occurrences and survive until final
 * native free alongside C/E/F. NULL limits select defaults; error is optional. World and output are
 * required. Output changes only on success. Owners/errors/output must be disjoint and the entire
 * graph exclusive/live. Success reserves nothing: keep it unchanged through the actual
 * cleanup/commit, and audit afresh after any mutation or checked cleanup refusal. This does not
 * authorize SaveHandler or App teardown. Old retire_entity_containers retains its broader separate
 * native contract. */
mc_world_storage_status mc_world_runtime_terminal_admit(mc_world_runtime *world,
                                                        const mc_world_terminal_limits *limits,
                                                        mc_world_terminal_inventory *out,
                                                        mc_world_storage_error *error);

/* Explicit terminal native teardown, separate from game removal/save/unload.
 * First retire borrowed views/calls/leases. Admission validates the complete
 * typed World.b/D/d/c/G/R ledgers and every bound Chunk slice/embedded TileEntity Map
 * before releasing any hold. This includes each nearby-result occurrence and
 * every actual dedicated Entity-index occurrence. The latter owner is released
 * and subclass G becomes NULL without a source clear/removal callback.
 * Success terminates source operations and releases each actual occurrence;
 * it leaves gameplay q/membership/World bindings untouched. Admitted Mob target
 * graphs are pinned before commit and consumed after source admission ends;
 * an externally held Mob owning nonnull target fields refuses without clearing
 * its fields. Retired containers
 * cannot serialize even when empty. Retire native cache/pending descriptors,
 * then World contexts and checked store owners; externally held Entities/TileEntities keep
 * final World.free BUSY until their real construction bindings are released.
 * Failure before admission preserves every owner. No callback or save runs. */
mc_world_storage_status mc_world_runtime_retire_entity_containers(mc_world_runtime *world,
                                                                  mc_world_storage_error *error);

#endif
