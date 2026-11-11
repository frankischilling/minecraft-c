#ifndef MC_WORLD_RUNTIME_LIGHTING_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_LIGHTING_EXECUTION_INTERNAL_H

#include "acquisition_internal.h"
#include "mc/world/runtime/lighting.h"
#include "runtime_execution_internal.h"

/* The same World height and light-submit bodies as lighting.h. The caller
 * owns the exact evaluated World scope before later arguments and keeps the
 * actual defining code, provider, Store and resource graphs live. The caller
 * attributes an invocation on a null World at its own Source site.
 *
 * The fresh frame, result and outputs are disjoint. Only the actually reached
 * getter uses the owned acquisition kernel. Presence and all earlier guards
 * retain their ordinary behavior. A false continuation can accompany a raw
 * OK getter return: the body reports the GET site and leaves gameplay output
 * untouched before any Chunk payload read or light queue work.
 *
 * Keep incomplete frames and their typed acquisition views stable until the
 * caller drains the exact recorded native work and clears expiring borrows.
 * A completed quiet call permits reinitialization. These leaves create and
 * close no World scope and never retry Source work.
 */
typedef struct mc_world_light_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_code code;
    mc_entity_tick_execution_site receiver_site;
    mc_world_runtime_execution_result *owned;
    mc_world_runtime_acquisition_error acquisition;
    mc_chunk_acquire_status get_status;
    bool entered, get_entered, get_returned, native_stopped, completed;
} mc_world_light_execution;

void mc_world_light_execution_init(mc_world_light_execution *, mc_entity_tick_execution *,
                                   mc_world_runtime_execution_code, mc_entity_tick_execution_site,
                                   mc_world_runtime_execution_result *);
bool mc_world_runtime_height_in_execution(mc_world_light_execution *, mc_world_runtime *, int32_t,
                                          int32_t, int32_t *, mc_world_light_error *,
                                          mc_world_storage_status *);
bool mc_world_runtime_light_submit_in_execution(mc_world_light_execution *, mc_world_runtime *,
                                                const mc_world_light_type *,
                                                const mc_light_bounds *, bool,
                                                mc_light_submit_result *, mc_world_light_error *,
                                                mc_world_storage_status *);

/* Shared native plumbing for these two translation units. */
bool mc_world_light_execution_enter(mc_world_light_execution *, mc_world_runtime *, const void *,
                                    const mc_world_light_error *, const mc_world_storage_status *);
bool mc_world_light_execution_finish(mc_world_light_execution *, mc_world_storage_status,
                                     mc_world_storage_status *);
mc_chunk_acquire_status mc_world_light_get_chunk(mc_world_light_execution *, mc_world_runtime *,
                                                 int32_t, int32_t, mc_chunk_state **,
                                                 mc_world_storage_error *,
                                                 mc_world_population_context *);

#endif
