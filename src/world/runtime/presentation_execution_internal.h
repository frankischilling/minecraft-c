#ifndef MC_WORLD_RUNTIME_PRESENTATION_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_PRESENTATION_EXECUTION_INTERNAL_H

#include "mc/world/runtime/presentation.h"
#include "runtime_execution_internal.h"

/* SAME coordinate sound/particle loops as the legacy entries. The caller owns
 * the exact evaluated nonnull World scope before later arguments; this helper
 * creates/closes no execution scope and performs no Math or String use.
 * Fresh stable frame/result/error/status storage is disjoint and remains live
 * through checked unwind. Init is for fresh storage only. The frame borrows
 * the caller's result; no callback, error or execution retains this frame.
 *
 * Every live-list reload checks the original exact token and its same IDLE
 * producer lease. Actual failing producers move to that lease before the held
 * receiver is released, and capture is noted immediately. Native continuation
 * is independent of raw Source status: raw OK/native false stops before the
 * next size/get/callback. Inspect the bool/result before Source classification.
 * callbacks_returned counts C invocation returns, including adapter refusals.
 * The caller owns exact native unwind, clearing typed borrows, and retirement.
 * No missing descriptor chooses a legacy fallback or post-return adoption.
 *
 * mutation entries share the already-entered bodies; runtime entries also pair
 * the ordinary World active call. Neither form closes the caller's scope.
 * Preflight refusal preserves error/status. Accepted runtime entries retain
 * the legacy error convention, including NULL-list/cast/invocation PCs.
 */
typedef struct mc_world_presentation_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *owned;
    mc_entity_tick_execution_site receiver_site;
    mc_entity_tick_execution_world_token world;
    mc_world_access_failure_lease *lease;
    mc_world_storage_status callback_status;
    size_t callbacks_returned;
    bool entered, completed;
} mc_world_presentation_execution;

void mc_world_presentation_execution_init(mc_world_presentation_execution *,
                                          mc_entity_tick_execution *, mc_entity_tick_execution_site,
                                          mc_world_runtime_execution_result *);
bool mc_world_mutation_sound_in_execution(mc_world_presentation_execution *, mc_world_runtime *,
                                          double, double, double, const mc_registry_string *, float,
                                          float, mc_world_mutation_error *,
                                          mc_world_storage_status *);
bool mc_world_mutation_particle_in_execution(mc_world_presentation_execution *, mc_world_runtime *,
                                             const mc_registry_string *, double, double, double,
                                             double, double, double, mc_world_mutation_error *,
                                             mc_world_storage_status *);
bool mc_world_runtime_sound_in_execution(mc_world_presentation_execution *, mc_world_runtime *,
                                         double, double, double, const mc_registry_string *, float,
                                         float, mc_world_mutation_error *,
                                         mc_world_storage_status *);
bool mc_world_runtime_particle_frame_in_execution(mc_world_presentation_execution *,
                                                  mc_world_runtime *, const mc_registry_string *,
                                                  double, double, double, double, double, double,
                                                  mc_world_mutation_error *,
                                                  mc_world_storage_status *);

/* The older detailed particle adapter keeps its actual raw status and supplied
 * initialized result. The older bare adapter has no native channel: a native-
 * only refusal is returned as its exact native status/cause, without inventing
 * a Source exception. Both adapters stop the same loop before later Source. */

#endif
