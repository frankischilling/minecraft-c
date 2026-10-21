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
 * World.b before invoking the concrete observer hook. Duplicates own separate
 * native holds. It does not change Entity.world or precheck source membership.
 *
 * This runtime constructs actual Item and FallingSand Entities; player
 * sleep/tracker descendants remain separate implementations. Empty observer lists
 * complete; unsupported reached nonempty dispatch retains both publications.
 * NULL Entity is admitted until its reached source field read. A nonnull Entity
 * must belong to this World's bound application EntityClasses, independently
 * of its nullable/different construction World. This is native type admission,
 * not class initialization or an additional source World field operation.
 *
 * Keep owners exclusive/live. Outputs/errors must be disjoint from all owners,
 * dependencies and each other. Failure preserves out and all reached effects;
 * error names the World/Chunk/observer PCs and deepest native cause. */
mc_world_storage_status mc_world_runtime_insert_entity(mc_world_runtime *world, mc_entity *entity,
                                                       bool *out, mc_world_entity_error *error);

/* Direct normal Chunk.a(Entity)V through its exact store-owned header. Hold and
 * lease that identity through the call. q is set before NULL/mismatch failures;
 * ordered Entity membership stores precede the selected List append. A reached
 * wrong-location diagnostic remains unsupported. No World.b/observer operation
 * is performed by this direct Chunk call. */
mc_world_storage_status mc_world_runtime_chunk_add_entity(mc_world_chunk *chunk, mc_entity *entity,
                                                          mc_world_entity_error *error);

/* Explicit terminal native teardown, separate from game removal/save/unload.
 * First retire borrowed views/calls/leases. Admission validates the complete
 * typed World.b/D/c/G/R ledgers and every bound Chunk slice/embedded Tile Map
 * before releasing any hold. This includes each nearby-result occurrence.
 * Success terminates source operations and releases each actual occurrence;
 * it leaves gameplay q/membership/World bindings untouched. Retired containers
 * cannot serialize even when empty. Retire native cache/pending descriptors,
 * then World contexts and checked store owners; externally held Entities/Tiles keep
 * final World.free BUSY until their real construction bindings are released.
 * Failure before admission preserves every owner. No callback or save runs. */
mc_world_storage_status mc_world_runtime_retire_entity_containers(mc_world_runtime *world,
                                                                  mc_world_storage_error *error);

#endif
