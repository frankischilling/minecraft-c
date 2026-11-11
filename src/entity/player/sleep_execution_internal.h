#ifndef MC_ENTITY_PLAYER_SLEEP_EXECUTION_INTERNAL_H
#define MC_ENTITY_PLAYER_SLEEP_EXECUTION_INTERNAL_H

#include "../../world/runtime/runtime_execution_internal.h"
#include "mc/entity/player/sleep.h"

/* Shares the retained GS wake body. The call owns its exact reached World
 * tokens and result until native close and checked retirement. The caller
 * owns the defining code and the execution for the entire call lifetime. */
mc_player_sleep_status
mc_player_wake_begin_in_execution(mc_entity *, const mc_player_sleep_context *,
                                  mc_entity_tick_execution *, mc_world_runtime_classes *,
                                  mc_world_runtime_execution_code, mc_player_sleep_call **,
                                  mc_player_sleep_error *);
mc_world_runtime_execution_result *mc_player_wake_execution_result(mc_player_sleep_call *);
bool mc_player_wake_execution_owns_world(const mc_player_sleep_call *,
                                         const mc_entity_tick_execution_world_token *);
bool mc_player_wake_execution_source_null(const mc_player_sleep_call *,
                                          mc_entity_tick_execution_site *);
typedef struct mc_world_notification_execution mc_world_notification_execution;
const mc_world_notification_execution *
mc_player_wake_execution_failure_observer(const mc_player_sleep_call *);
typedef struct mc_world_mutation_drop_execution mc_world_mutation_drop_execution;
const mc_world_mutation_drop_execution *
mc_player_wake_execution_failed_drop(const mc_player_sleep_call *);
mc_world_storage_status mc_player_wake_execution_native_close(mc_player_sleep_call *);

#endif
