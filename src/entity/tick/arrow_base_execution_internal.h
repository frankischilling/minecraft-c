#ifndef MC_ENTITY_TICK_ARROW_BASE_EXECUTION_INTERNAL_H
#define MC_ENTITY_TICK_ARROW_BASE_EXECUTION_INTERNAL_H

#include "../../world/runtime/material_movement_execution_internal.h"
#include "execution_internal.h"

typedef enum mc_tick_arrow_base_child {
    MC_TICK_ARROW_BASE_NO_CHILD,
    MC_TICK_ARROW_BASE_ENVIRONMENT,
    MC_TICK_ARROW_BASE_SOUND,
    MC_TICK_ARROW_BASE_PARTICLE,
    MC_TICK_ARROW_BASE_REMOTE
} mc_tick_arrow_base_child;

/* Caller-stable private frame. These fields are borrowed pointers and VALUE
 * continuations, never copied owners. The real execution/Math, defining code
 * graph, actual Arrow, result and typed error remain live/exclusive/disjoint.
 * Initialize once for one genuine Base invocation. No Source retry/re-entry.
 * Actual code_classes/chunk code are supplied by defining code, not Entity.world.
 * The scanner's current same-code Native qualification remains unchanged.
 *
 * child identifies the reached operation/argument prefix, not a classifier.
 * child_returned witnesses only an actual lower C entry return. A scan receipt
 * is not an active failure tag after a later Registry/Math/pool failure.
 * body_entered/body_completed are continuation observations, not Java exception
 * classifications. Native-only stops may leave the Source error status OK.
 * Inspect owned first and retain every typed active arm without guessing a cause.
 *
 * world_open publishes only this Base call's actual created token. On a refused
 * close, this frame/result plus execution retain it. The parent may use its
 * existing exact-token native unwind, including real activation close capsules;
 * no ambient scope search, Source replay or context/Math retirement occurs here.
 * close_attempted prevents another automatic checked pop after a refused one.
 * Clear borrowed typed views before actual receipt/World/loader retirement.
 */
typedef struct mc_tick_arrow_base_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_world_runtime_execution_result *owned;
    mc_world_material_scan_query scan;
    mc_entity_tick_execution_world_token world;
    mc_entity_tick_execution_site receiver_site;
    mc_tick_arrow_base_child child;
    mc_world_storage_status child_status;
    bool world_open, close_attempted, child_returned, body_entered, body_completed;
} mc_tick_arrow_base_execution;

void mc_tick_arrow_base_execution_init(mc_tick_arrow_base_execution *, mc_entity_tick_execution *,
                                       mc_world_runtime_classes *, mc_world_runtime_execution_code,
                                       mc_world_runtime_execution_result *);

/* SAME admitted Arrow SN.U body, including real inherited ai/damage/K calls.
 * Required initialized frame and fresh stable error; no public error layout grows.
 * Caller owns the defining Registry/runtime/Entity/Math and complete external
 * provider/Store/import graph. Each actual dynamic World is captured at its
 * Source load BEFORE bounds/literal/Random arguments and popped exactly once
 * after ordinary locals drain. A failed checked close remains explicit above.
 */
bool mc_tick_arrow_base_body_in_execution(mc_entity *, mc_registry_classes *,
                                          mc_tick_arrow_base_execution *,
                                          const mc_entity_tick_limits *, mc_entity_tick_error *);

/* Necessary private cross-file seam into the SINGLE existing liquid_probe.
 * Caller already admits this same genuine held READY Arrow; no new retain,
 * eager Registry use, substitute Material or second water/lava body is added. */
bool mc_entity_environment_liquid_in_tick_execution(mc_entity *, mc_registry_classes *, bool water,
                                                    mc_tick_arrow_base_execution *,
                                                    const mc_world_material_movement_limits *,
                                                    bool *, mc_entity_environment_error *);

/* Internal actual-load/actual-close helpers shared by the two existing files.
 * NULL is used ONLY by their legacy callers, never a missing required-context
 * owned entry. Helpers neither infer nor retire a caller's scope/Math. */
bool mc_tick_arrow_base_world_begin(mc_tick_arrow_base_execution *, mc_world_runtime *,
                                    mc_entity_tick_execution_site, mc_tick_arrow_base_child);
bool mc_tick_arrow_base_world_end(mc_tick_arrow_base_execution *, bool completed);

#endif
