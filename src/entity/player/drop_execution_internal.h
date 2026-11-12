#ifndef MC_ENTITY_PLAYER_DROP_EXECUTION_INTERNAL_H
#define MC_ENTITY_PLAYER_DROP_EXECUTION_INTERNAL_H

#include "../../world/runtime/entity_insertion_execution_internal.h"
#include "mc/entity/player/drop.h"

typedef struct mc_player_drop_execution_context {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
} mc_player_drop_execution_context;

typedef struct mc_player_drop_world_execution_view {
    mc_entity_tick_execution_world_token world;
    bool created, open, close_attempted, locals_drained, failed, consumed;
    bool native_close_attempted;
    mc_world_storage_status initial_close_status, native_close_status;
} mc_player_drop_world_execution_view;

typedef struct mc_player_drop_execution_view {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *result;
    const mc_world_entity_insert_execution *publication;
    mc_world_runtime *constructor_value, *publication_value;
    mc_player_drop_world_execution_view constructor_world, publication_world;
    mc_world_storage_status publication_status;
    bool publication_returned, completed;
} mc_player_drop_execution_view;

/* SAME selected/Stack bodies. Required fresh/disjoint outer result receives
 * VALUE observations only; the retained call owns the canonical insertion /
 * result. Bool, raw Player status, Source FD Boolean and actual shared Math
 * cause are independent. Missing context/code/outputs never selects legacy.
 * NULL Stack returns4 without World/Math/statistic work. Capture nullable
 * GS.a(Stack,Z) World10 before coordinates/eyeheight/Stack/constructor36; close
 * after actual construction. Capture fresh GS.a(Item) World1 before Item4 /
 * invoke5 (outer315), close before statistic319/323 and return326. Ordinary
 * FD false is discarded; a native/Math false stops without statistic or replay.
 * IN83/97 or132/146/168/181/203 uses the same execution->math; Java Math
 * constructor/perturbation operations retain their independent actual bodies.
 *
 * Caller owns stable exclusive execution/Math, defining code/Registry/Entity
 * graph and full external provider/Store/import/generation/WorldAccess callback
 * and producer graph until exact unwind/retirement. No ambient World/Chunk
 * inference, eager ensure, cold-path substitution, after-return receipt retain,
 * automatic arbitrary descendant close or execution/Math retirement occurs.
 */
bool mc_local_player_drop_selected_begin_in_execution(
    mc_entity_tick_execution *, mc_world_runtime_classes *, mc_world_runtime_execution_code,
    mc_registry_classes *, mc_entity *, mc_player_drop_call **, mc_player_drop_status *,
    mc_player_drop_error *, mc_world_runtime_execution_result *);
bool mc_local_player_drop_stack_begin_in_execution(
    mc_entity_tick_execution *, mc_world_runtime_classes *, mc_world_runtime_execution_code,
    mc_registry_classes *, mc_entity *, mc_item_stack *, bool scatter, mc_player_drop_call **,
    mc_player_drop_status *, mc_player_drop_error *, mc_world_runtime_execution_result *);
bool mc_local_player_drop_execution_inspect(mc_player_drop_call *, mc_player_drop_execution_view *);

/* Only this call's exact recorded tokens, publication first. Initial close
 * records actual drained-local proof/failed operand before pop. Native retry
 * preserves those observations and never repeats a proved consumed non-OK
 * close, including after slot reuse. Matching same-generation FREE/RETAINED
 * proves consumption; mismatch does not. Explicit canonical pending/Chunk
 * close continuations must drain first; no arbitrary descendant is popped.
 * Preserve call/frame/typed views through refusal; expire borrows before
 * checked call retirement and subsequent outer execution/Math retirement.
 * OK never promotes a previous false begin or claims outer quiescence.
 */
mc_world_storage_status mc_local_player_drop_native_close(mc_player_drop_call *);

#endif
