#ifndef MC_WORLD_RUNTIME_PLANTS_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_PLANTS_EXECUTION_INTERNAL_H

#include "blocks_execution_internal.h"
#include "mc/world/runtime/plants.h"
#include "queries_execution_internal.h"

/* SAME Flower survival body. The caller retains the defining code and actual
 * receiver graphs, the evaluated World and its exact token. This frame borrows
 * their canonical result without initializing it or beginning another scope.
 *
 * Fresh disjoint storage remains live through checked native drainage/close.
 * Failed query/block acquisition and typed details stay here until those owners
 * can retire. Clear this frame's borrowed views before retiring their owners.
 * No Source retry, scope close, pending completion or owner retirement occurs
 * here. Raw status and native continuation are independent; only true permits
 * the ordinary Boolean output to be used. Other plant kinds remain unjoined. */
typedef struct mc_world_plant_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_entity_tick_execution_world_token world;
    mc_entity_tick_execution_site receiver_site;
    mc_world_runtime_execution_result *owned;
    mc_world_access_failure_lease *lease;
    mc_world_query_execution query;
    mc_world_block_execution block;
    mc_world_plant_error detail;
    mc_world_storage_error block_cause;
    mc_world_storage_status query_status, block_status;
    bool entered, native_stopped, completed;
} mc_world_plant_execution;

void mc_world_plant_execution_init(mc_world_plant_execution *, mc_entity_tick_execution *,
                                   mc_world_runtime_classes *, mc_world_runtime_execution_code,
                                   mc_entity_tick_execution_world_token,
                                   mc_entity_tick_execution_site,
                                   mc_world_runtime_execution_result *);
bool mc_world_runtime_flower_survives_in_execution(mc_world_plant_execution *, mc_world_runtime *,
                                                   const mc_block *, int32_t, int32_t, int32_t,
                                                   bool *, mc_world_storage_status *);

#endif
