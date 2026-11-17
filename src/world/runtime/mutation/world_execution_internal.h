#ifndef MC_WORLD_RUNTIME_MUTATION_WORLD_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_WORLD_EXECUTION_INTERNAL_H

#include "../presentation_execution_internal.h"
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

/* Actual retained presentation body, independent of the mutable enclosing
 * mutation projection. The exact World token can be RETAINED after its failed
 * checked pop; its producer receipt stays owned by that execution slot. */
typedef struct mc_world_mutation_presentation_failure {
    const mc_world_presentation_execution *execution;
    const mc_world_mutation_error *error;
    mc_world_storage_status status;
    mc_world_access_source_fault fault;
    bool particle;
} mc_world_mutation_presentation_failure;

bool mc_world_mutation_execution_failed_presentation(const mc_world_mutation_execution_call *,
                                                     mc_world_mutation_presentation_failure *);

/* Shared descendants reuse the canonical result and retained call frames. */
mc_registry_classes *mc_world_mutation_execution_registry(const mc_world_mutation_execution_call *);
mc_world_runtime_execution_result *
mc_world_mutation_execution_result_of(const mc_world_mutation_execution_call *);
bool mc_world_mutation_execution_current(const mc_world_mutation_execution_call *,
                                         mc_world_runtime *, mc_entity_tick_execution_site);
bool mc_world_mutation_execution_get(mc_world_mutation_execution_call *, mc_world_runtime *,
                                     int32_t, int32_t, int32_t, bool, int32_t *,
                                     mc_world_mutation_error *, mc_entity_tick_execution_site);
bool mc_world_mutation_execution_metadata(mc_world_mutation_execution_call *, mc_world_runtime *,
                                          int32_t, int32_t, int32_t, int32_t,
                                          mc_world_mutation_error *, mc_entity_tick_execution_site);
bool mc_world_mutation_execution_material(mc_world_mutation_execution_call *, mc_world_runtime *,
                                          int32_t, int32_t, int32_t, mc_material **,
                                          mc_world_mutation_error *, mc_entity_tick_execution_site);
typedef struct mc_world_material_execution mc_world_material_execution;
const mc_world_material_execution *
mc_world_mutation_execution_failed_material(const mc_world_mutation_execution_call *,
                                            mc_world_storage_status *);
/* The reached Chest/Furnace/Sign RW addition is joined. Its nullable World argument
 * reaches factory15 first and reports a successful factory's NULL invocation
 * through execution_source_null at RW18. Its complete live Tile child
 * remains in this mutation call while the one canonical result retains any
 * selected loader/pending/native-close work. Borrow expires at call retire. */
bool mc_world_mutation_execution_tile_added(mc_world_mutation_execution_call *, mc_world_runtime *,
                                            const mc_block *, int32_t, int32_t, int32_t,
                                            mc_world_mutation_error *);
bool mc_world_mutation_execution_sign_removed(mc_world_mutation_execution_call *,
                                              mc_world_runtime *, int32_t, int32_t, int32_t,
                                              mc_world_mutation_error *);
const mc_live_tile_entity_error *
mc_world_mutation_execution_failed_tile_added(const mc_world_mutation_execution_call *,
                                              mc_world_storage_status *);
const mc_live_tile_entity_error *
mc_world_mutation_execution_failed_tile_removed(const mc_world_mutation_execution_call *,
                                                mc_world_storage_status *);
typedef struct mc_live_tile_entity_query_execution_call mc_live_tile_entity_query_execution_call;
/* Borrow the actual failed nested lazy lookup while this mutation owns it.
 * Its complete addition diagnostics and capture site expire at retirement. */
const mc_live_tile_entity_query_execution_call *
mc_world_mutation_execution_failed_tile_removed_lookup(const mc_world_mutation_execution_call *);
bool mc_world_mutation_execution_set(mc_world_mutation_execution_call *, mc_world_runtime *,
                                     int32_t, int32_t, int32_t, int32_t, bool *,
                                     mc_world_mutation_error *, mc_entity_tick_execution_site);
/* Begin captures the actual World operand before later arguments. Every
 * accepted begin is paired with invoke or abort; neither repeats Source work.
 * Invoke closes this exact occurrence, retaining failed producers and any
 * refused close until native drainage. Abort is idempotent after consumption.
 * String/Math qualification uses the defining Registry/World graph, never the
 * unrelated MathHelper trig owner. All result/error/status storage is stable. */
bool mc_world_mutation_execution_presentation_begin(mc_world_mutation_execution_call *,
                                                    mc_world_runtime *, mc_world_mutation_error *,
                                                    mc_entity_tick_execution_site);
bool mc_world_mutation_execution_literal(mc_world_mutation_execution_call *, mc_registry_classes *,
                                         const char *, const mc_registry_string **,
                                         mc_world_mutation_error *, mc_entity_tick_execution_site);
bool mc_world_mutation_execution_math(mc_world_mutation_execution_call *, mc_java_random_profile *,
                                      double *, mc_world_mutation_error *,
                                      mc_entity_tick_execution_site);
bool mc_world_mutation_execution_sound(mc_world_mutation_execution_call *, mc_world_runtime *,
                                       double, double, double, const mc_registry_string *, float,
                                       float, mc_world_mutation_error *,
                                       mc_entity_tick_execution_site);
bool mc_world_mutation_execution_particle(mc_world_mutation_execution_call *, mc_world_runtime *,
                                          const mc_registry_string *, double, double, double,
                                          double, double, double, mc_world_mutation_error *,
                                          mc_entity_tick_execution_site);
mc_world_storage_status
mc_world_mutation_execution_presentation_abort(mc_world_mutation_execution_call *);
bool mc_world_mutation_execution_set_quiet(mc_world_mutation_execution_call *, mc_world_runtime *,
                                           int32_t, int32_t, int32_t, int32_t, int32_t, bool *,
                                           mc_world_mutation_error *,
                                           mc_entity_tick_execution_site);
bool mc_world_mutation_execution_observe_range(mc_world_mutation_execution_call *,
                                               mc_world_runtime *, int32_t, int32_t, int32_t,
                                               int32_t, int32_t, int32_t, mc_world_mutation_error *,
                                               mc_entity_tick_execution_site);
bool mc_world_mutation_execution_schedule(mc_world_mutation_execution_call *, mc_world_runtime *,
                                          int32_t, int32_t, int32_t, int32_t, int32_t,
                                          mc_world_mutation_error *, mc_entity_tick_execution_site);
bool mc_world_mutation_execution_added(const mc_world_mutation_execution_context *,
                                       mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                       int32_t, mc_world_mutation_execution_call **,
                                       mc_world_mutation_error *, mc_entity_tick_execution_site);
bool mc_world_mutation_execution_removed(const mc_world_mutation_execution_context *,
                                         mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                         int32_t, mc_world_mutation_execution_call **,
                                         mc_world_mutation_error *, mc_entity_tick_execution_site);
/* Lazy Chunk lookup supplies its independently held defining Registry and RW
 * factory classes. NULL remains an actual borrow until its reached use. These
 * bindings belong only to this virtual addition, never a mutable World/Block
 * inference or a change to the shared acquisition-code VALUE bundle. */
bool mc_world_mutation_execution_added_with_tile_bindings(
    const mc_world_mutation_execution_context *, mc_registry_classes *, mc_world_runtime_classes *,
    mc_world_runtime *, const mc_block *, int32_t, int32_t, int32_t,
    mc_world_mutation_execution_call **, mc_world_mutation_error *, mc_entity_tick_execution_site);
bool mc_world_mutation_block_added_with_call(mc_world_mutation_execution_call *, mc_world_runtime *,
                                             const mc_block *, int32_t, int32_t, int32_t,
                                             mc_world_mutation_error *);
bool mc_world_mutation_block_removed_with_call(mc_world_mutation_execution_call *,
                                               mc_world_runtime *, const mc_block *, int32_t,
                                               int32_t, int32_t, mc_world_mutation_error *);
typedef struct mc_world_solid_support_execution mc_world_solid_support_execution;
/* Retain the actual lower frame and raw C return before projection. A native
 * stop can accompany a non-OK raw return while support detail stays empty;
 * callers inspect that frame and the canonical result before projecting it.
 * False preserves the ordinary Boolean. Only a completed frame is discarded. */
bool mc_world_mutation_execution_support(mc_world_mutation_execution_call *, mc_world_runtime *,
                                         int32_t, int32_t, int32_t, bool *,
                                         mc_world_storage_status *,
                                         const mc_world_solid_support_execution **,
                                         mc_entity_tick_execution_site);
const mc_world_solid_support_execution *
mc_world_mutation_execution_failed_support(const mc_world_mutation_execution_call *,
                                           mc_world_storage_status *);
typedef struct mc_world_plant_execution mc_world_plant_execution;
bool mc_world_mutation_execution_plant_survives(mc_world_mutation_execution_call *,
                                                mc_world_runtime *, const mc_block *, int32_t,
                                                int32_t, int32_t, bool *, mc_world_storage_status *,
                                                const mc_world_plant_execution **,
                                                mc_entity_tick_execution_site);
const mc_world_plant_execution *
mc_world_mutation_execution_failed_plant(const mc_world_mutation_execution_call *,
                                         mc_world_storage_status *);
bool mc_world_mutation_execution_remove(mc_world_mutation_execution_call *, mc_world_runtime *,
                                        int32_t, int32_t, int32_t, mc_world_mutation_error *,
                                        mc_entity_tick_execution_site);
bool mc_world_mutation_execution_drop(mc_world_mutation_execution_call *, mc_world_runtime *,
                                      const mc_block *, int32_t, int32_t, int32_t, int32_t,
                                      mc_world_mutation_error *, mc_entity_tick_execution_site);
typedef struct mc_world_mutation_drop_execution mc_world_mutation_drop_execution;
const mc_world_mutation_drop_execution *
mc_world_mutation_execution_failed_drop(const mc_world_mutation_execution_call *);

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
bool mc_world_mutation_ladder_neighbor_in_execution(mc_world_mutation_execution_call *,
                                                    mc_world_runtime *, const mc_block *, int32_t,
                                                    int32_t, int32_t, int32_t,
                                                    mc_world_mutation_error *);
bool mc_world_mutation_sign_neighbor_in_execution(mc_world_mutation_execution_call *,
                                                  mc_world_runtime *, const mc_block *, int32_t,
                                                  int32_t, int32_t, int32_t,
                                                  mc_world_mutation_error *);
bool mc_world_mutation_torch_neighbor_in_execution(mc_world_mutation_execution_call *,
                                                   mc_world_runtime *, const mc_block *, int32_t,
                                                   int32_t, int32_t, int32_t,
                                                   mc_world_mutation_error *);
bool mc_world_mutation_plant_neighbor_in_execution(mc_world_mutation_execution_call *,
                                                   mc_world_runtime *, const mc_block *, int32_t,
                                                   int32_t, int32_t, int32_t,
                                                   mc_world_mutation_error *);
bool mc_world_mutation_falling_neighbor_in_execution(mc_world_mutation_execution_call *,
                                                     mc_world_runtime *, const mc_block *, int32_t,
                                                     int32_t, int32_t, int32_t,
                                                     mc_world_mutation_error *);
bool mc_world_mutation_fluid_added_in_execution(mc_world_mutation_execution_call *,
                                                mc_world_runtime *, const mc_block *, int32_t,
                                                int32_t, int32_t, mc_world_mutation_error *);
bool mc_world_mutation_fluid_neighbor_in_execution(mc_world_mutation_execution_call *,
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
