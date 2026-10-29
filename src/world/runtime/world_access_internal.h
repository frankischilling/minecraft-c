#ifndef MC_WORLD_RUNTIME_WORLD_ACCESS_INTERNAL_H
#define MC_WORLD_RUNTIME_WORLD_ACCESS_INTERNAL_H

#include "mc/world/runtime/world_access.h"

#include <stddef.h>

struct mc_world_access {
    mc_world_access_callbacks callbacks;
    mc_world_access_source_callbacks source_callbacks;
    void *context;
    size_t memberships, active_holds, receipt_holds;
};

struct mc_world_access_receipt {
    mc_world_access *receiver;
    const mc_client_render_global_error *render_global;
    const char *exception_class;
    size_t holds;
};

mc_world_storage_status mc_world_access_receipt_create(mc_world_access *,
                                                       const mc_client_render_global_error *,
                                                       mc_world_access_receipt **,
                                                       mc_world_access_failure *,
                                                       mc_world_storage_error *);
mc_world_storage_status mc_world_access_receipt_retire(mc_world_access_receipt **,
                                                       mc_world_storage_error *);

/* Exact native receipts for actual List.add/remove and terminal ownership.
 * Every added occurrence has a nonzero identity, including NULL. Removal is
 * still first-equal source removal and reports the actual removed identity.
 * Native detach/probe use only the independently captured owning descriptor,
 * never a replaceable foreign source List, and require exclusive quiescence. */
mc_world_storage_status mc_world_access_add_receipted(mc_world_runtime *, mc_world_access *,
                                                      uint64_t *, mc_world_storage_error *);
mc_world_storage_status mc_world_access_remove_receipted(mc_world_runtime *, mc_world_access *,
                                                         bool *, uint64_t *,
                                                         mc_world_storage_error *);
mc_world_storage_status mc_world_access_occurrence_present(const mc_world_runtime *,
                                                           const mc_world_access *, uint64_t,
                                                           bool *, mc_world_storage_error *);
mc_world_storage_status mc_world_access_detach_occurrence(mc_world_runtime *, mc_world_access *,
                                                          uint64_t, mc_world_storage_error *);

mc_world_storage_status mc_world_access_call_cell(mc_world_access *, mc_world_runtime *, int32_t,
                                                  int32_t, int32_t, mc_world_storage_error *,
                                                  mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_range(mc_world_access *, mc_world_runtime *, int32_t,
                                                   int32_t, int32_t, int32_t, int32_t, int32_t,
                                                   mc_world_storage_error *,
                                                   mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_sound(mc_world_access *, mc_world_runtime *,
                                                   const mc_registry_string *, double, double,
                                                   double, float, float, mc_world_storage_error *,
                                                   mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_particle(mc_world_access *, mc_world_runtime *,
                                                      const mc_registry_string *, double, double,
                                                      double, double, double, double,
                                                      mc_world_storage_error *,
                                                      mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_entity_added(mc_world_access *, mc_world_runtime *,
                                                          mc_entity *, mc_world_storage_error *,
                                                          mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_entity_removed(mc_world_access *, mc_world_runtime *,
                                                            mc_entity *, mc_world_storage_error *,
                                                            mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_tile_entity_dirty(mc_world_access *,
                                                               mc_world_runtime *, int32_t, int32_t,
                                                               int32_t, mc_tile_entity *,
                                                               mc_world_storage_error *,
                                                               mc_world_access_failure *);

/* The caller already holds the exact World. Reload current u at size/get,
 * validate only reached metadata and prove nonnull casts against the captured
 * typed occurrence. NULL passes cast without ownership adoption. has=false is
 * only ordinary loop termination; nonnull out owns one active hold, released
 * after actual argument evaluation and callback, including every failure path.
 * No callback, argument evaluation, iterator/snapshot or full-ledger preflight
 * runs here. Outputs are published only on success. */
bool mc_world_access_next(mc_world_runtime *world, size_t index, unsigned int size_pc,
                          unsigned int get_pc, unsigned int cast_pc, mc_world_access **out,
                          bool *has, unsigned int *failed_pc, mc_world_storage_error *error);
void mc_world_access_release_held(mc_world_access *world_access);
mc_world_storage_error mc_world_access_callback_error(mc_world_storage_status status,
                                                      mc_world_storage_error cause);

/* Native ownership admission only, never a source dispatch preflight. These
 * validate/carry the independently captured constructor u192 identity.
 * owned_valid never depends on replaceable source u. Retirement additionally
 * requires source u to be captured, NULL or a shaped empty borrowed List;
 * foreign empty Lists are never adopted, cleared or freed. Registration and
 * removal separately require current u to be the captured descriptor. Release
 * requires prior validation and no entered World call; it invokes no callback
 * and keeps the descriptor allocated for final World destruction. */
bool mc_world_accesses_owned_valid(const mc_world_runtime *world);
bool mc_world_accesses_retirement_valid(const mc_world_runtime *world);
void mc_world_accesses_owned_release(mc_world_runtime *world);

#endif
