#ifndef MC_WORLD_RUNTIME_MUTATION_WORLD_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_WORLD_EXECUTION_INTERNAL_H

#include "../runtime_execution_internal.h"
#include "mc/world/runtime/mutations.h"
#include "notifications_execution_internal.h"

typedef struct mc_world_mutation_execution_call mc_world_mutation_execution_call;

/* The caller owns the evaluated World and the defining and external graphs.
 * Each recursive invocation borrows the same result and the exact token of its
 * World argument. Only reached fresh World operands get independent tokens. */
typedef struct mc_world_mutation_execution_context {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_world_runtime_execution_result *result;
    mc_entity_tick_execution_world_token world;
} mc_world_mutation_execution_context;

/* The slot starts empty. A reached call retains its actual child frames until
 * checked close and retirement. False preserves ordinary output. Raw Source
 * status and canonical native refusal are separate. Missing context refuses
 * before entering the shared body. */
bool mc_world_runtime_metadata_set_in_execution(const mc_world_mutation_execution_context *,
                                                mc_world_runtime *, int32_t, int32_t, int32_t,
                                                int32_t, mc_world_mutation_execution_call **,
                                                mc_world_storage_status *,
                                                mc_world_mutation_error *);
bool mc_world_runtime_block_set_in_execution(const mc_world_mutation_execution_context *,
                                             mc_world_runtime *, int32_t, int32_t, int32_t, int32_t,
                                             bool *, mc_world_mutation_execution_call **,
                                             mc_world_storage_status *, mc_world_mutation_error *);

/* The caller drains the actual canonical acquisition records before closing
 * only this call's recorded descendants. The initiating token remains borrowed.
 * A refusal preserves the live call and its remaining typed diagnostics. */
bool mc_world_mutation_execution_owns_world(const mc_world_mutation_execution_call *,
                                            const mc_entity_tick_execution_world_token *);
mc_world_storage_status
mc_world_mutation_execution_native_close(mc_world_mutation_execution_call *);
mc_world_storage_status mc_world_mutation_execution_retire(mc_world_mutation_execution_call **);
bool mc_world_mutation_execution_source_null(const mc_world_mutation_execution_call *,
                                             mc_entity_tick_execution_site *);
const mc_world_notification_execution *
mc_world_mutation_execution_failure_observer(const mc_world_mutation_execution_call *);

/* Shared descendants reuse the canonical result and retained call frames. */
mc_registry_classes *mc_world_mutation_execution_registry(const mc_world_mutation_execution_call *);
mc_world_runtime_execution_result *
mc_world_mutation_execution_result_of(const mc_world_mutation_execution_call *);
bool mc_world_mutation_execution_current(const mc_world_mutation_execution_call *,
                                         mc_world_runtime *, mc_entity_tick_execution_site);
bool mc_world_mutation_execution_get(mc_world_mutation_execution_call *, mc_world_runtime *,
                                     int32_t, int32_t, int32_t, bool, int32_t *,
                                     mc_world_mutation_error *, mc_entity_tick_execution_site);
bool mc_world_mutation_execution_remove(mc_world_mutation_execution_call *, mc_world_runtime *,
                                        int32_t, int32_t, int32_t, mc_world_mutation_error *,
                                        mc_entity_tick_execution_site);

bool mc_world_mutation_neighbors_in_execution(mc_world_mutation_execution_call *,
                                              mc_world_runtime *, int32_t, int32_t, int32_t,
                                              int32_t, mc_world_mutation_error *);
bool mc_world_mutation_block_neighbor_in_execution(mc_world_mutation_execution_call *,
                                                   mc_world_runtime *, const mc_block *, int32_t,
                                                   int32_t, int32_t, int32_t,
                                                   mc_world_mutation_error *);
bool mc_world_mutation_bed_neighbor_in_execution(mc_world_mutation_execution_call *,
                                                 mc_world_runtime *, const mc_block *, int32_t,
                                                 int32_t, int32_t, int32_t,
                                                 mc_world_mutation_error *);

bool mc_world_mutation_block_removed_in_execution(mc_world_runtime *, const mc_block *, int32_t,
                                                  int32_t, int32_t, mc_world_mutation_error *,
                                                  mc_world_runtime_execution_result *,
                                                  mc_entity_tick_execution_site);
bool mc_world_mutation_block_added_in_execution(mc_world_runtime *, const mc_block *, int32_t,
                                                int32_t, int32_t, mc_world_mutation_error *,
                                                mc_world_runtime_execution_result *,
                                                mc_entity_tick_execution_site);

#endif
