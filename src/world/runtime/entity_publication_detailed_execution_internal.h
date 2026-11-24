#ifndef MC_WORLD_RUNTIME_ENTITY_PUBLICATION_DETAILED_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_ENTITY_PUBLICATION_DETAILED_EXECUTION_INTERNAL_H

#include "entity_publication_execution_internal.h"
#include "runtime_execution_internal.h"

/* SAME entered held batch/observer body. Borrow the caller's initialized native
 * result without resetting a prior first owner error. Select this operation's
 * actual context admission/receipt-capture result at its return site, before
 * Entity/WA release and before a later activation pop or outer completion.
 * Source status/cause/publication prefix and all old APIs remain unchanged.
 * receiver_site is the defining caller's already-captured World site VALUE;
 * no mutable current World/site is inspected after a callback to reconstruct it.
 * No acquisition, context close or retirement is introduced here. */
mc_world_storage_status mc_world_runtime_entity_batch_held_detailed_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, const mc_world_chunk_entity_list *, bool,
    mc_entity_tick_execution_site receiver_site, mc_world_entity_batch_error *,
    mc_world_runtime_execution_result *);

#endif
