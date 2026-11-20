#ifndef MC_WORLD_RUNTIME_SCHEDULING_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_SCHEDULING_EXECUTION_INTERNAL_H

#include "blocks_execution_internal.h"
#include "mc/world/runtime/scheduling.h"

/* Each caller owns its frame across synchronous descendants. Its contents are
 * defined on failure. A successful caller can discard or reuse the storage. */
mc_world_storage_status mc_world_runtime_schedule_in_place(mc_world_runtime *, int32_t, int32_t,
                                                           int32_t, int32_t, int32_t,
                                                           mc_world_schedule_error *);

/* Borrow the caller's initialized result and exact evaluated World token.
 * The same scheduling body constructs its actual record before presence/flag
 * branches. A failed invocation retains that local record and any scalar
 * acquisition until checked canonical drainage and frame retirement. No
 * initializer resets the borrowed result, begins a scope or retries Source.
 * Immediate virtual tick execution remains a reached, explicit native frontier. */
typedef struct mc_world_schedule_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_entity_tick_execution_world_token world;
    mc_entity_tick_execution_site receiver_site;
    mc_world_runtime_execution_result *owned;
    mc_scheduled_tick *record;
    mc_world_block_execution lower;
    mc_world_storage_error lower_cause;
    mc_world_storage_status lower_status, status;
    mc_world_schedule_error detail;
    bool entered, active, native_stopped, completed;
} mc_world_schedule_execution;

void mc_world_schedule_execution_init(mc_world_schedule_execution *, mc_entity_tick_execution *,
                                      mc_world_runtime_classes *, mc_world_runtime_execution_code,
                                      mc_entity_tick_execution_world_token,
                                      mc_entity_tick_execution_site,
                                      mc_world_runtime_execution_result *);
bool mc_world_runtime_schedule_in_execution(mc_world_schedule_execution *, mc_world_runtime *,
                                            int32_t, int32_t, int32_t, int32_t, int32_t,
                                            mc_world_storage_status *);
mc_world_storage_status mc_world_schedule_execution_retire(mc_world_schedule_execution **);

#endif
