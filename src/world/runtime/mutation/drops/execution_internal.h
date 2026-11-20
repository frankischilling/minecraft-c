#ifndef MC_WORLD_MUTATION_DROPS_EXECUTION_INTERNAL_H
#define MC_WORLD_MUTATION_DROPS_EXECUTION_INTERNAL_H

#include "../../../../entity/drop/execution_internal.h"
#include "../../entity_insertion_execution_internal.h"
#include "../world_execution_internal.h"

/* Same Block probability/spawn bodies, with the actual caller's defining
 * graph, evaluated World and canonical result. The caller keeps all external
 * owners live. One reached Item owns its constructor/publication occurrences;
 * false retains their actual frames and the caller's local Item/Stack holds.
 * No Source retry or legacy diagnostic capture is performed at retirement. */
typedef struct mc_world_mutation_drop_execution {
    mc_world_mutation_execution_context context;
    mc_entity *entity;
    mc_item_stack *stack;
    mc_world_entity_insert_execution insertion;
    mc_world_entity_error publication;
    mc_entity_drop_world_execution_view constructor_scope, publication_scope;
    mc_entity_tick_execution_site constructor_site, publication_site;
    mc_world_storage_status publication_status;
    bool entered, active, completed, publication_returned;
} mc_world_mutation_drop_execution;

bool mc_world_mutation_drop_in_execution(const mc_world_mutation_execution_context *,
                                         mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                         int32_t, int32_t, mc_world_mutation_drop_execution **,
                                         mc_world_mutation_error *, mc_world_storage_status *);
bool mc_world_mutation_drop_capture(mc_world_mutation_drop_execution *, mc_world_runtime *,
                                    mc_entity_tick_execution_site,
                                    mc_entity_drop_world_execution_view *);
bool mc_world_mutation_drop_close(mc_world_mutation_drop_execution *,
                                  mc_entity_drop_world_execution_view *,
                                  mc_entity_tick_execution_site, bool);
bool mc_world_mutation_drop_owns_world(const mc_world_mutation_drop_execution *,
                                       const mc_entity_tick_execution_world_token *);
mc_world_storage_status mc_world_mutation_drop_native_close(mc_world_mutation_drop_execution *);
mc_world_storage_status mc_world_mutation_drop_retire(mc_world_mutation_drop_execution **);
bool mc_world_mutation_drop_result_quiet(const mc_world_runtime_execution_result *);

#endif
