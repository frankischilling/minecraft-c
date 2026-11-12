#ifndef MC_WORLD_RUNTIME_ENTITY_PUBLICATION_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_ENTITY_PUBLICATION_EXECUTION_INTERNAL_H

#include "entity_publication_internal.h"
#include "execution_children_internal.h"

/* Actual SAME World.a(List)/b(List) entered body. Required nonnull stable
 * execution owns the exact current nullable World scope and an unused node;
 * a nonnull World must already have its genuine entered call from the caller.
 * NULL/invalid context is a native refusal, NEVER legacy fallback. The legacy
 * entry calls the same core with NULL privately, preserving its old contract.
 *
 * Whole addAll still precedes live source size/get/cast and per-Entity hook34.
 * The actual base observer uses its real local owning WA slot and the SAME
 * node; capture occurs before temporary Entity/remaining-WA releases. Dedicated
 * ID/G.put remains only after base success. No get/acquire/kernel join occurs
 * in this entered body. Caller closes its scope; this child does not retire it.
 * Error output may be NULL; owned failures remain under execution regardless.
 * All owner/source/output storage is live, exclusive and mutually disjoint. */
mc_world_storage_status
mc_world_runtime_entity_batch_held_in_execution(mc_entity_tick_execution *, mc_world_runtime *,
                                                const mc_world_chunk_entity_list *, bool deferred,
                                                mc_world_entity_batch_error *);

#endif
