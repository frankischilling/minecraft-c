#ifndef MC_ENTITY_PLAYER_DEATH_EXECUTION_INTERNAL_H
#define MC_ENTITY_PLAYER_DEATH_EXECUTION_INTERNAL_H

#include "death_internal.h"
#include "drop_execution_internal.h"

typedef struct mc_player_death_execution_view {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *result;
    mc_player_drop_call *drop;
    mc_player_drop_execution_view child;
    mc_player_drop_status drop_status;
    bool drop_returned, has_child, completed;
} mc_player_death_execution_view;

/* SAME actual retained GS.b(SN): inherited Living death, optional Notch Apple61,
 * all40 Inventory scatter68, source-nonnull IN cos91/sin118, statistic153.
 * No GS.K/Container close is reached or admitted by this entry. Existing K and
 * all public legacy interfaces keep their SAME bodies via NULL descriptor.
 * Required stable initialized effect scratch and fresh/disjoint outer result.
 * Missing context/code/outputs refuses before Source, never selects legacy.
 * Bool, raw Entity/Player statuses, Source FD Boolean and shared Math cause
 * remain independent. false/rawOK retains current Stack/drop/heap work and
 * stops before Inventory clear or any later fatal Source operation.
 *
 * Caller owns stable exclusive execution/shared Math, defining code/Registry /
 * Entity graph and full external provider/Store/import/generation/WorldAccess /
 * producer/statistic callback dependencies through exact unwind/retirement.
 * Scalar remote gate and literal FD status0 retain their actual World bindings;
 * no extra execution scopes or eager ensure is invented around those leaves.
 * No replay, rollback, ambient owner inference, execution/Math retirement or
 * arbitrary descendant close occurs. Outer result forwards VALUE only.
 */
bool mc_player_death_entered_in_execution(mc_entity_tick_execution *, mc_world_runtime_classes *,
                                          mc_world_runtime_execution_code, mc_entity *,
                                          mc_registry_classes *, mc_entity *source,
                                          mc_entity_status *, mc_entity_effect_error *,
                                          mc_world_runtime_execution_result *);

/* SAME pending owned fatal work only. result is original forwarded child VALUE;
 * child.result is the stable canonical PlayerDrop result for exact explicit
 * finish_pending/retry_close. Direct IN91/118 failure can leave no drop; its
 * actual Math cause remains in execution->math. After real descendants drain,
 * existing death/effect cleanup closes only this actual drop's recorded tokens
 * before retirement. Refusal preserves work/owners/typed views. Expire borrows
 * before successful cleanup and outer context/Math retirement. Cleanup never
 * promotes an original false continuation or resumes Source.
 */
bool mc_player_death_execution_inspect(mc_entity *, mc_player_death_execution_view *);

#endif
