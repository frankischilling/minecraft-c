#ifndef MC_WORLD_RUNTIME_ACTIVATION_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_ACTIVATION_EXECUTION_INTERNAL_H

#include "../../entity/tick/execution_internal.h"
#include "activation_internal.h"

/* Complete actual Entity-slice activation path only. Required stable nonnull
 * context may already own coordinator A. Capture actual nullable Chunk.d B
 * independently BEFORE slice41; enter its World only at invocation42; run the
 * one genuine entered batch/publication body and checked-pop exact B on every
 * path after push. A/B may differ, alias or be NULL; no Source field is rebound.
 *
 * evaluated_receiver_site names the actual defining Chunk code receiver load
 * (client lm.e()V pc33 / server hi.d()V pc33), supplied by that code's caller,
 * NEVER inferred from mutable/nullable B or the coordinator A.
 *
 * Failure closes B into retained fixed storage before returning typed views.
 * Checked pop refusal leaves B current/owned and records execution.cleanup;
 * callers recover its exact token through current_world after native fault
 * correction and retry only pop, never Source activation. An earlier Source
 * status/cause is preserved; cleanup after real Source success returns its
 * native refusal. The caller must not mistake still-current B for outer A.
 *
 * Kernel callback threading, TileEntity registration/dirty, loader projection
 * and full A-query/joint Store cleanup remain OPEN. No successful substitute
 * for those bodies is exposed here. This API invokes no alternate model. */
mc_world_storage_status mc_world_runtime_register_chunk_entities_in_execution(
    mc_entity_tick_execution *, mc_world_chunk *, size_t,
    mc_entity_tick_execution_site evaluated_receiver_site, mc_world_activation_error *);

#endif
