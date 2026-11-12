#ifndef MC_WORLD_RUNTIME_MUTATION_CHUNK_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_CHUNK_EXECUTION_INTERNAL_H

#include "../lighting_execution_internal.h"
#include "mc/world/runtime/chunks.h"
#include "mc/world/runtime/mutations.h"
#include "notifications_execution_internal.h"

typedef enum mc_world_mutation_chunk_body {
    MC_WORLD_MUTATION_CHUNK_ID,
    MC_WORLD_MUTATION_CHUNK_INTERVAL,
    MC_WORLD_MUTATION_CHUNK_HEIGHT
} mc_world_mutation_chunk_body;

typedef struct mc_world_mutation_chunk_scope {
    mc_entity_tick_execution_world_token token;
    mc_entity_tick_execution_site site;
    mc_world_storage_status initial_close_status;
    bool created, attempted, failed, consumed;
} mc_world_mutation_chunk_scope;

/* One stable frame for the actual normal Chunk selected by FD's quiet setter.
 * The caller owns initiating_token and the defining code/resource graph. This
 * entry takes the exact Store/state lease and the passive Chunk binding's
 * ordinary World call. Each later Source d occurrence is independently
 * captured before its later operands, including a nullable World argument.
 *
 * Fresh disjoint frame/result storage stays live until checked native unwind.
 * The actual LM result and raw status stay independent of native continuation.
 * Refused admission preserves output/error/raw. A false after entry preserves
 * reached Source mutations and typed views without publishing changed.
 */
typedef struct mc_world_mutation_chunk_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_code code;
    mc_world_runtime_classes *code_classes;
    mc_registry_classes *registry;
    mc_entity_tick_execution_site caller_site;
    mc_entity_tick_execution_world_token initiating_token;
    mc_world_runtime_execution_result *owned;
    mc_world_runtime *initiating_world, *binding_world;
    mc_world_store *store;
    mc_chunk_state *state;
    mc_world_chunk *chunk;
    mc_world_light_execution light;
    mc_world_notification_execution height_observer;
    mc_world_chunk_error column;
    mc_world_mutation_chunk_scope scope;
    mc_entity_tick_execution_site source_null_site;
    size_t world_captures;
    bool entered, active, lease_held, binding_held, completed;
    bool source_returned, changed, source_null;
} mc_world_mutation_chunk_execution;

void mc_world_mutation_chunk_execution_init(mc_world_mutation_chunk_execution *,
                                            mc_entity_tick_execution *,
                                            mc_world_runtime_execution_code,
                                            mc_world_runtime_classes *, mc_registry_classes *,
                                            mc_entity_tick_execution_site,
                                            mc_entity_tick_execution_world_token,
                                            mc_world_runtime_execution_result *);
bool mc_world_mutation_chunk_block_in_execution(mc_world_mutation_chunk_execution *,
                                                mc_world_runtime *, mc_world_store *,
                                                mc_chunk_state *, mc_world_chunk *, int32_t,
                                                int32_t, int32_t, int32_t, bool *,
                                                mc_world_mutation_error *,
                                                mc_world_storage_status *);
bool mc_world_mutation_chunk_execution_owns_world(const mc_world_mutation_chunk_execution *,
                                                  const mc_entity_tick_execution_world_token *);
/* Drain actual canonical acquisition attempts/activation close records first.
 * Retry only this frame's exact current d token, then its Store/state lease
 * under the initiating token. A consumed token never authorizes another pop.
 * No Source retry, changed publication, or borrowed-view expiration occurs.
 */
mc_world_storage_status mc_world_mutation_chunk_native_close(mc_world_mutation_chunk_execution *);
const mc_world_notification_execution *
mc_world_mutation_chunk_failure_observer(const mc_world_mutation_chunk_execution *);

/* Shared same-body internals for the ID setter and column helpers. */
mc_entity_tick_execution_site
mc_world_mutation_chunk_site(const mc_world_mutation_chunk_execution *,
                             mc_world_mutation_chunk_body, unsigned int);
bool mc_world_mutation_chunk_capture(mc_world_mutation_chunk_execution *, mc_world_runtime *,
                                     mc_entity_tick_execution_site);
bool mc_world_mutation_chunk_close(mc_world_mutation_chunk_execution *, bool completed);
void mc_world_mutation_chunk_null(mc_world_mutation_chunk_execution *, mc_world_mutation_chunk_body,
                                  unsigned int);

bool mc_world_mutation_chunk_q(mc_world_runtime *, mc_registry_classes *, int32_t, unsigned int *,
                               unsigned int, unsigned int, int32_t *, mc_world_mutation_error *);
bool mc_world_mutation_chunk_submit(mc_world_mutation_chunk_execution *, mc_world_runtime *,
                                    mc_light_kind, const mc_light_bounds *,
                                    mc_world_mutation_chunk_body, unsigned int *, unsigned int,
                                    unsigned int, mc_world_light_error *, mc_world_storage_error *);
bool mc_world_mutation_submit_scoped(mc_world_mutation_chunk_execution *, mc_world_runtime *,
                                     mc_light_kind, const mc_light_bounds *,
                                     mc_world_mutation_chunk_body, unsigned int *, unsigned int,
                                     unsigned int, mc_world_mutation_error *);
bool mc_world_mutation_height_in_execution(mc_world_mutation_chunk_execution *, mc_world_chunk *,
                                           int32_t, int32_t, int32_t, mc_world_mutation_error *);
bool mc_world_mutation_column_in_execution(mc_world_mutation_chunk_execution *, mc_world_chunk *,
                                           int32_t, int32_t, mc_world_mutation_error *);
bool mc_world_runtime_chunk_column_in_execution(mc_world_mutation_chunk_execution *,
                                                mc_world_chunk *, int32_t, int32_t,
                                                mc_world_chunk_error *);

#endif
