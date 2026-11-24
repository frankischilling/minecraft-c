#ifndef MC_ENTITY_DROP_EXECUTION_INTERNAL_H
#define MC_ENTITY_DROP_EXECUTION_INTERNAL_H

#include "../../world/runtime/entity_insertion_execution_internal.h"
#include "mc/entity/drop.h"

typedef struct mc_entity_drop_world_execution_view {
    mc_entity_tick_execution_world_token world;
    bool created, open, close_attempted, locals_drained, failed, consumed;
    bool native_close_attempted;
    mc_world_storage_status initial_close_status, native_close_status;
} mc_entity_drop_world_execution_view;

typedef struct mc_entity_drop_execution_view {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *result;
    const mc_world_entity_insert_execution *publication;
    mc_entity_drop_world_execution_view constructor_world, publication_world;
    mc_world_storage_status publication_status;
    bool publication_returned, completed;
} mc_entity_drop_execution_view;

/* SAME complete retained SN numeric/Stack bodies. Required fresh/disjoint
 * outer result receives VALUE observations only; the heap call owns the real
 * insertion frame/result. Inspect that stable private result for exact pending
 * acquisition/Chunk-close continuations. Forwarding acquires no close right.
 * Bool is Source/native continuation; raw Entity status and Source FD Boolean
 * (state.inserted) are separate. Math-only/native-only refusal may leave raw OK.
 * Missing execution/code/output refuses before Source, never selects legacy.
 *
 * Caller owns the stable exclusive execution, shared actual Math, defining
 * client runtime/Registry/Entity graph and full external provider/Store/import /
 * generation graph through checked unwind/retirement. code passes unchanged;
 * mutable actor World never selects defining code or another Math namespace.
 * Capture nullable constructor World at SN.a(Stack,F)5 before later arguments,
 * close after actual constructor work; capture fresh publication World35 before
 * Item38. NULL remains a Source value until genuine publication invoke39.
 * No eager class/Math ensure, Source retry, after-return receipt retain, rollback,
 * execution/Math retirement or automatic arbitrary descendant unwind occurs.
 */
bool mc_entity_drop_numeric_begin_in_execution(mc_entity_tick_execution *,
                                               mc_world_runtime_classes *,
                                               mc_world_runtime_execution_code,
                                               mc_registry_classes *, mc_entity *, int32_t id,
                                               int32_t count, float offset, mc_entity_drop_call **,
                                               mc_entity_status *, mc_entity_drop_error *,
                                               mc_world_runtime_execution_result *);
bool mc_entity_drop_stack_begin_in_execution(mc_entity_tick_execution *, mc_world_runtime_classes *,
                                             mc_world_runtime_execution_code, mc_registry_classes *,
                                             mc_entity *, mc_item_stack *, float offset,
                                             mc_entity_drop_call **, mc_entity_status *,
                                             mc_entity_drop_error *,
                                             mc_world_runtime_execution_result *);
bool mc_entity_drop_execution_inspect(mc_entity_drop_call *, mc_entity_drop_execution_view *);

/* Native-only close of this call's actual recorded tokens, publication first.
 * First finish only the real private result's explicit pending/Chunk capsules;
 * this helper refuses while they remain and never pops an unrecorded scope.
 * Initial closure saves its actual failed operand and drained-local proof
 * immediately before pop. Native retries use that exact operand. Matching
 * same-generation FREE/RETAINED is positive consumption; mismatch proves none.
 * Consumed non-OK closes are never repeated, including after slot reuse.
 * Preserve all typed views/owners until this succeeds; then expire borrowed
 * views before checked drop retirement and outer execution/Math retirement.
 * OK closes only these tokens, never promotes the original false continuation.
 */
mc_world_storage_status mc_entity_drop_native_close(mc_entity_drop_call *);

#endif
