#ifndef MC_ENTITY_MOB_REMOVAL_EXECUTION_INTERNAL_H
#define MC_ENTITY_MOB_REMOVAL_EXECUTION_INTERNAL_H

#include "../../world/runtime/entity_insertion_execution_internal.h"
#include "removal_internal.h"

typedef struct mc_slime_removal_world_view {
    mc_entity_tick_execution_world_token world;
    bool created, open, close_attempted, locals_drained, failed, consumed;
    bool native_close_attempted;
    mc_world_storage_status initial_close_status, native_close_status;
} mc_slime_removal_world_view;

typedef struct mc_slime_removal_execution_view {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *result; /* Canonical stable current child result. */
    const mc_world_entity_insert_execution *publication;
    mc_entity *current_child;
    mc_world_runtime *constructor_value, *publication_value;
    mc_slime_removal_world_view constructor_world, publication_world;
    mc_world_storage_status publication_status;
    bool publication_returned, publication_continued, completed;
    bool constructor_held, publication_held, gate_held; /* Passive own-occurrence flags. */
} mc_slime_removal_execution_view;

/* SAME body as legacy. Required stable execution/shared Math, defining code
 * and complete external World/provider/Store/import/callback graph. Capture
 * nullable constructor World68 and fresh publication World124 BEFORE later
 * arguments. Constructors retain independent Java Math/private Random bodies;
 * only the reached FD/LM insertion uses the actual shared execution Math.
 * Bool continuation, raw Entity status, raw World return and discarded FD Bool
 * are separate. ANY false/rawOK retains current child and stops before iinc133
 * or inherited SN.K140. Outer result receives VALUE only, never close rights.
 * Current insertion/result borrows expire when that iteration is consumed;
 * inspect only the actually pending invocation for its canonical close rights. */
bool mc_slime_removal_entered_in_execution(mc_entity_tick_execution *, mc_world_runtime_classes *,
                                           mc_world_runtime_execution_code, mc_entity *,
                                           mc_entity_status *, mc_entity_effect_error *,
                                           mc_world_runtime_execution_result *);
bool mc_slime_removal_execution_inspect(mc_entity *, mc_slime_removal_execution_view *);

/* First finish ONLY inspect.result's recorded pending/Chunk continuations.
 * Close publication then constructor using their saved initial operands;
 * never pop an arbitrary current descendant or reclose consumed tokens.
 * A canonical Slime with no pending work succeeds without touching execution;
 * a null or non-Slime receiver refuses. */
mc_world_storage_status mc_slime_removal_native_close(mc_entity *);

#endif
