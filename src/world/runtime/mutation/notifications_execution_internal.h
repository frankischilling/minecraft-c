#ifndef MC_WORLD_RUNTIME_MUTATION_NOTIFICATIONS_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_NOTIFICATIONS_EXECUTION_INTERNAL_H

#include "../runtime_execution_internal.h"
#include "mc/world/runtime/mutations.h"

/* SAME live WorldAccess cell/range loops as the legacy mutation leaves.
 * The caller already owns the exact evaluated nonnull World scope before
 * later arguments, plus every actual callback/resource graph. Fresh stable
 * frame/result/error/status storage is disjoint and stays live/exclusive
 * through checked unwind. No scope is created or closed here.
 *
 * A callback's actual producer occurrence moves to that exact scope's lease
 * before the held receiver is released. Capture and native status are recorded
 * immediately, never reconstructed from an after-return receipt. Source error
 * and raw status remain separate from native continuation; raw OK can accompany
 * a native-only refusal after a completed callback. No later size/get/callback
 * runs after refusal. The loop always reloads the live list after ordinary
 * success, preserving removals/appends and unsorted supplied range endpoints.
 * callbacks_returned counts reached C invocation returns, not proof that a
 * Source callback body entered before any native adapter refusal.
 *
 * These leaves do not join recursive neighbors, block hooks, scheduling or
 * complete notifying metadata. Those callers must thread the same execution
 * explicitly; no missing context selects a legacy descendant here.
 */
typedef struct mc_world_notification_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *owned;
    mc_entity_tick_execution_site receiver_site;
    mc_entity_tick_execution_world_token world;
    mc_world_access_failure_lease *lease;
    mc_world_storage_status callback_status;
    size_t callbacks_returned;
    bool entered, range, completed;
} mc_world_notification_execution;

void mc_world_notification_execution_init(mc_world_notification_execution *,
                                          mc_entity_tick_execution *, mc_entity_tick_execution_site,
                                          mc_world_runtime_execution_result *);
/* The surrounding mutation owns the canonical result. Only initialize the
 * local leaf, then require a quiet result at actual notification admission. */
void mc_world_notification_execution_init_borrowed(mc_world_notification_execution *,
                                                   mc_entity_tick_execution *,
                                                   mc_entity_tick_execution_site,
                                                   mc_world_runtime_execution_result *);
bool mc_world_mutation_observe_cell_in_execution(mc_world_notification_execution *,
                                                 mc_world_runtime *, int32_t, int32_t, int32_t,
                                                 mc_world_mutation_error *,
                                                 mc_world_storage_status *);
bool mc_world_mutation_observe_range_in_execution(mc_world_notification_execution *,
                                                  mc_world_runtime *, int32_t, int32_t, int32_t,
                                                  int32_t, int32_t, int32_t,
                                                  mc_world_mutation_error *,
                                                  mc_world_storage_status *);
bool mc_world_mutation_observe_column_in_execution(mc_world_notification_execution *,
                                                   mc_world_runtime *, int32_t, int32_t, int32_t,
                                                   int32_t, mc_world_mutation_error *,
                                                   mc_world_storage_status *);

#endif
