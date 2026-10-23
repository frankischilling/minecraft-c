#ifndef MC_WORLD_RUNTIME_ENTITY_PUBLICATION_INTERNAL_H
#define MC_WORLD_RUNTIME_ENTITY_PUBLICATION_INTERNAL_H

#include "runtime_internal.h"

/* Validate native ownership of the actual constructor-created b/D Lists,
 * independently of replaceable source projections. NULL typed occurrences own
 * no hold. Never reinterpret an unknown void* collection as Entity ownership. */
bool mc_world_runtime_entity_list_valid(const mc_world_runtime *world);
/* Infallible after complete quiescent admission. No source callback, World
 * binding rewrite, serialization, dirty/q or Entity membership store occurs. */
void mc_world_runtime_entity_list_release(mc_world_runtime *world);

/* Actual World.a(List) / b(List), caller already holding the genuine World and
 * readable actual Chunk slice through this synchronous call. No Entity phase,
 * construction World/class, coordinate/provider or relocation gate is added.
 * Source input is typed Entity/NULL; arbitrary untyped values are unsupported.
 * Destination getfield1/addAll5 precedes input size15/get26/known cast31 and
 * observer34. Append the WHOLE batch before observers; retain each nonnull
 * occurrence separately. Native allocation/hold preadmission is transactional,
 * not Java ArrayList/OOM timing. Unknown replacement destinations admit only
 * the existing finite empty-source no-op. NULL destinations/inputs fail at5.
 * Deferred=true appends D only and returns11; processing/removal is separate.
 * No source membership/q/phase mutation, Chunk operation or callback is added.
 * The base observer's empty-list return is supported; nonempty base dispatch
 * remains an explicit reached frontier retaining the complete append prefix.
 * A dedicated override enters its actual G.put only AFTER base success and
 * the current Entity-ID read. Deferred append never invokes that override.
 * Input/error must be disjoint from owners/ledgers/each other. */
mc_world_storage_status mc_world_runtime_entity_batch_held(mc_world_runtime *world,
                                                           const mc_world_chunk_entity_list *source,
                                                           bool deferred,
                                                           mc_world_entity_batch_error *error);

#endif
