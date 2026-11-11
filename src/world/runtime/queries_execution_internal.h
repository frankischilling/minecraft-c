#ifndef MC_WORLD_RUNTIME_QUERIES_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_QUERIES_EXECUTION_INTERNAL_H

#include "mc/world/runtime/queries.h"
#include "runtime_execution_internal.h"

/* SAME plant-light and sky-visible bodies. The caller already owns the exact
 * evaluated World scope and supplies its actual defining Chunk classes/code.
 * It keeps all World, provider, Store/import and defining-code graphs live.
 * This frame borrows the initialized canonical result; init never resets it.
 *
 * Fresh frame/status/out storage is disjoint and stable. Missing context or a
 * nonquiet result refuses before body entry, preserving ordinary outputs and
 * detail. Only an actually reached getter uses the owned acquisition kernel.
 * A raw OK return does not authorize payload/output continuation after native
 * refusal. Full failed acquisition and query detail remain here independently
 * of canonical ownership status until the caller drains the exact recorded
 * native work and clears expiring borrows. A quiet completed frame may be
 * reinitialized. These leaves never create/close a scope or replay Source.
 */
typedef struct mc_world_query_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_entity_tick_execution_site receiver_site;
    mc_world_runtime_execution_result *owned;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_query_error detail;
    mc_chunk_acquire_status get_status;
    mc_world_storage_status status;
    bool entered, get_entered, get_returned, native_stopped, completed;
} mc_world_query_execution;

void mc_world_query_execution_init(mc_world_query_execution *, mc_entity_tick_execution *,
                                   mc_world_runtime_classes *, mc_world_runtime_execution_code,
                                   mc_entity_tick_execution_site,
                                   mc_world_runtime_execution_result *);
bool mc_world_runtime_plant_light_in_execution(mc_world_query_execution *, mc_world_runtime *,
                                               int32_t, int32_t, int32_t, int32_t *,
                                               mc_world_storage_status *);
bool mc_world_runtime_sky_visible_in_execution(mc_world_query_execution *, mc_world_runtime *,
                                               int32_t, int32_t, int32_t, bool *,
                                               mc_world_storage_status *);

#endif
