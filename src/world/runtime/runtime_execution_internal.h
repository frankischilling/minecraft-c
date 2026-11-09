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

/* Only the TWO participating Chunk activation close helpers publish these
 * ordinary VALUE records. No World/receipt/loader occurrence is acquired.
 * Slot i names the exact created World token's fixed slot and generation;
 * successful closes and consumed-but-non-OK closes leave no record.
 * DRAINED means this activation's ordinary local active calls/temporary rights
 * have ended. Outer typed diagnostics still borrow the actual retained owners.
 * It is not permission to resume Source or to close an unrecorded descendant. */
typedef enum mc_world_runtime_execution_close_kind {
    MC_WORLD_RUNTIME_EXECUTION_CLOSE_NONE,
    MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_TILE,
    MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_ENTITY
} mc_world_runtime_execution_close_kind;

typedef enum mc_world_runtime_execution_close_phase {
    MC_WORLD_RUNTIME_EXECUTION_CLOSE_UNPUBLISHED,
    MC_WORLD_RUNTIME_EXECUTION_CLOSE_LOCALS_DRAINED
} mc_world_runtime_execution_close_phase;

typedef struct mc_world_runtime_execution_close {
    bool used, child_failed;
    mc_world_runtime_execution_close_kind kind;
    mc_world_runtime_execution_close_phase phase;
    mc_entity_tick_execution_world_token world;
    mc_world_storage_status source_status, refused_status;
    const char *operation;
    mc_entity_tick_execution_site site;
} mc_world_runtime_execution_close;

typedef struct mc_world_runtime_execution_result {
    mc_world_storage_status ownership_status;
    mc_entity_tick_execution_error ownership_error;
    bool selection_ready, root_pending;
    mc_entity_tick_execution_loader_selection selection;
    mc_entity_tick_execution_loader_attempt root_attempt;
    mc_world_runtime_execution_pending pending[MC_ENTITY_TICK_EXECUTION_LOADERS];
    mc_world_runtime_execution_close closes[MC_ENTITY_TICK_EXECUTION_WORLDS];
} mc_world_runtime_execution_result;

void mc_world_runtime_execution_result_init(mc_world_runtime_execution_result *);
/* Latch only a current native owner error. Original Source cause/status lives
 * in the unchanged acquisition/activation/TileEntity error independently. */
void mc_world_runtime_execution_result_note(mc_world_runtime_execution_result *,
                                            mc_world_storage_status, const char *,
                                            mc_entity_tick_execution_site);
/* Forward ONLY the actually reached failed child result. Required fresh,
 * disjoint operation results share ONE stable exclusive execution. Prevalidate
 * BOTH complete sparse pending/close ledgers, owner identity, exact conflicts
 * and active selection/root compatibility before changing ANY destination.
 * Equal VALUE copies are idempotent; an empty child never erases live records.
 * Invalid/conflicting input returns refusal with the destination unchanged;
 * caller must retain both inputs on such contract failure, not discard a live
 * child frame. Genuine same-execution fresh results cannot exhaust this ledger:
 * each close occupies only its already-existing fixed World slot.
 * No getter, owner search, allocation, pop or Source classification occurs. */
mc_world_storage_status
mc_world_runtime_execution_result_forward(mc_world_runtime_execution_result *,
                                          const mc_world_runtime_execution_result *);

/* Callee-only native close for the TWO Chunk activation helpers, after their
 * ordinary local calls/temporary rights drain. Perform ONE genuine checked
 * pop with the actual pre-close Source status/failed operand. Note its native
 * status immediately. Only if that pop refuses AND its exact created token is
 * still OPEN, publish a LOCALS_DRAINED capsule. The narrow post-pop VALUE check
 * reads identity/slot/generation/phase only, including during a temporarily
 * invalid native producer counter; it never dereferences a producer or mines
 * ambient diagnostics. A consumed non-OK pop keeps its native error but no
 * close history. NULL result preserves the old pop-only native route.
 * The operation/site are actual stable defining-code borrows, not inferred
 * from World or an older error. Caller supplies a valid fresh result contract. */
mc_world_storage_status mc_world_runtime_execution_close_scope(
    mc_entity_tick_execution *, mc_world_runtime_execution_result *,
    mc_world_runtime_execution_close_kind, const mc_entity_tick_execution_world_token *,
    mc_world_storage_status source_status, const char *operation, mc_entity_tick_execution_site);

/* Native-only retry of ONE EXPLICIT published capsule VALUE. continuation may
 * alias its exact slot in result, or be an equal copied value. Validate its
 * LOCALS_DRAINED kind/status/failed operand, exact owner/generation and stored
 * record; checked current_world must name this token. Refuse while this result
 * has a matching pending attempt: caller must first finish that actual returned
 * attempt via finish_pending. Never finish, pop a descendant, invoke Source,
 * resume the body, release typed diagnostics or retire owners automatically.
 * ONE actual checked pop follows. Refusal preserves the entire capsule; only
 * actual OK clears that slot. First native error and original Source status
 * remain latched. A copied stale capsule never authorizes a second close.
 * No capsule is supplied for unjoined Tile/internal/publication descendants. */
mc_world_storage_status
mc_world_runtime_execution_retry_close(mc_entity_tick_execution *,
                                       mc_world_runtime_execution_result *,
                                       const mc_world_runtime_execution_close *continuation);

/* Native-only completion of ONE actual returned attempt matching the exact
 * current World token. Caller first restores that scope by its real checked
 * unwind; this helper never pops a scope, calls Source or cancels a reservation.
 * First owner error remains latched even after a successful completion retry.
 * Child-pending relations are resolved only by their exact attempt identities.
 * Participating activation close capsules restore their own exact scope only
 * through retry_close; arbitrary unrecorded descendant unwind remains OPEN. */
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
