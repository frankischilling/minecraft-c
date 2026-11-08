#ifndef MC_WORLD_RUNTIME_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_EXECUTION_INTERNAL_H

#include "execution_acquisition_internal.h"

/* Actual defining Chunk code provenance, supplied by that code's caller.
 * Client lm.e uses receiver loads6/33; server hi.d has its own caller bundle.
 * Never derive this metadata from nullable/mutable Chunk.d or coordinator A.
 * No side check, class ensure or Source argument evaluation occurs here. */
typedef struct mc_world_runtime_execution_code {
    mc_entity_tick_execution_site tile_receiver_site, entity_receiver_site;
} mc_world_runtime_execution_code;

/* Ordinary VALUE continuations, never owners. Each actual attempt occupies its
 * already-reserved neutral loader slot. The exact execution/World owners stay
 * live and exclusive until completion/retirement; these values may be copied.
 * direct_failure is ONLY the returned direct loader under attempt.world. A
 * reached child is explicit, never inferred from a forwarded raw pointer. */
typedef struct mc_world_runtime_execution_pending {
    bool used, outcome_valid, child_ready, child_pending;
    mc_entity_tick_execution_loader_attempt attempt, child_attempt;
    mc_chunk_acquire_status actual_status;
    const mc_world_source_chunk_error *direct_failure;
    mc_entity_tick_execution_loader_selection child;
} mc_world_runtime_execution_pending;

typedef struct mc_world_runtime_execution_result {
    mc_world_storage_status ownership_status;
    mc_entity_tick_execution_error ownership_error;
    bool selection_ready, root_pending;
    mc_entity_tick_execution_loader_selection selection;
    mc_entity_tick_execution_loader_attempt root_attempt;
    mc_world_runtime_execution_pending pending[MC_ENTITY_TICK_EXECUTION_LOADERS];
} mc_world_runtime_execution_result;

void mc_world_runtime_execution_result_init(mc_world_runtime_execution_result *);
/* Latch only a current native owner error. Original Source cause/status lives
 * in the unchanged acquisition/activation/TileEntity error independently. */
void mc_world_runtime_execution_result_note(mc_world_runtime_execution_result *,
                                            mc_world_storage_status, const char *,
                                            mc_entity_tick_execution_site);
/* Forward ONLY the actually reached failed child result. No getter, owner
 * search, allocation, scope close or source classification occurs here. */
mc_world_storage_status
mc_world_runtime_execution_result_forward(mc_world_runtime_execution_result *,
                                          const mc_world_runtime_execution_result *);

/* Native-only completion of ONE actual returned attempt matching the exact
 * current World token. Caller first restores that scope by its real checked
 * unwind; this helper never pops a scope, calls Source or cancels a reservation.
 * First owner error remains latched even after a successful completion retry.
 * Child-pending relations are resolved only by their exact attempt identities.
 * Caller-driven structural-refusal unwind propagation remains an OPEN join. */
mc_world_storage_status
mc_world_runtime_execution_finish_pending(mc_entity_tick_execution *,
                                          mc_world_runtime_execution_result *);

/* One SAME actual GET/ACQUIRE. Required fresh disjoint result receives owner
 * bookkeeping separately from the returned actual acquisition status. A new
 * native preflight refusal invokes no getter/coordinator and preserves *out.
 * The caller already owns exact nullable A before source arguments. Successful
 * and failure-only error-output conventions match the two legacy entries.
 * A real result remains actual even if completion subsequently refuses; inspect
 * ownership_status/selection_ready and retain all pending VALUE continuations.
 * error may be NULL: pending direct views remain protected by their exact
 * unfinished World reservation, never by this result value itself.
 *
 * OPEN: contextful loading/import, generation/population descendants and
 * captured-provider calls. These genuine legacy children keep their existing
 * caller-held graph lifetime contract. A forwarded borrowed child diagnostic
 * is NOT adopted by searching ambient owners after return. This entry alone
 * does not close arbitrary cold acquisition ownership or full SL composition. */
mc_chunk_acquire_status mc_world_runtime_get_in_execution(mc_entity_tick_execution *,
                                                          mc_world_runtime *,
                                                          mc_world_runtime_execution_code, int32_t,
                                                          int32_t, mc_chunk_state **,
                                                          mc_world_runtime_acquisition_error *,
                                                          mc_world_runtime_execution_result *);
mc_chunk_acquire_status
mc_world_runtime_get_failure_in_execution(mc_entity_tick_execution *, mc_world_runtime *,
                                          mc_world_runtime_execution_code, int32_t, int32_t,
                                          mc_chunk_state **, mc_world_runtime_acquisition_error *,
                                          mc_world_runtime_execution_result *);
mc_chunk_acquire_status mc_world_runtime_acquire_in_execution(mc_entity_tick_execution *,
                                                              mc_world_runtime *,
                                                              mc_world_runtime_execution_code,
                                                              int32_t, int32_t, mc_chunk_state **,
                                                              mc_world_runtime_acquisition_error *,
                                                              mc_world_runtime_execution_result *);

#endif
