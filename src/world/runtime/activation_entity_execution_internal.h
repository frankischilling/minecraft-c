#ifndef MC_WORLD_RUNTIME_ACTIVATION_ENTITY_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_ACTIVATION_ENTITY_EXECUTION_INTERNAL_H

#include "activation_execution_internal.h"
#include "runtime_execution_internal.h"

/* SAME complete one-slice entered Entity body. Required fresh disjoint native
 * result selects this operation's own identity/push/capture/pop refusal when it occurs,
 * before outer acquisition completion can refuse for another reason. Source
 * status/cause/batch prefix remain in the unchanged activation error. No error
 * field grows and no ambient cleanup, status-number matching or old cause is
 * used to reconstruct this native outcome. Error may still be NULL.
 *
 * The actual defining-code caller supplies code.entity_receiver_site (lm.e33
 * or its actual server alias). It is never inferred from mutable Chunk.d/A.
 * No acquisition happens in this one-slice body; its result has no selected
 * loader/pending attempt. All such fields are initialized explicitly empty.
 * Parent forwards this native VALUE before completing its actual reservation.
 * Old public and old owned APIs retain their signatures and NULL selector. */
mc_world_storage_status mc_world_runtime_register_chunk_entities_detailed_in_execution(
    mc_entity_tick_execution *, mc_world_chunk *, size_t, mc_world_runtime_execution_code,
    mc_world_activation_error *, mc_world_runtime_execution_result *);

#endif
