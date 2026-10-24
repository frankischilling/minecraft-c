#ifndef MC_WORLD_RUNTIME_OBSERVERS_INTERNAL_H
#define MC_WORLD_RUNTIME_OBSERVERS_INTERNAL_H

#include "mc/world/runtime/observers.h"

#include <stddef.h>

struct mc_world_observer {
    mc_world_observer_callbacks callbacks;
    void *context;
    size_t memberships, active_holds;
};

/* The caller already holds the exact World. Reload current u at size/get,
 * validate only reached metadata and prove nonnull casts against the captured
 * typed occurrence. NULL passes cast without ownership adoption. has=false is
 * only ordinary loop termination; nonnull out owns one active hold, released
 * after actual argument evaluation and callback, including every failure path.
 * No callback, argument evaluation, iterator/snapshot or full-ledger preflight
 * runs here. Outputs are published only on success. */
bool mc_world_observer_next(mc_world_runtime *world, size_t index, unsigned int size_pc,
                            unsigned int get_pc, unsigned int cast_pc, mc_world_observer **out,
                            bool *has, unsigned int *failed_pc, mc_world_storage_error *error);
void mc_world_observer_release_held(mc_world_observer *observer);
mc_world_storage_error mc_world_observer_callback_error(mc_world_storage_status status,
                                                        mc_world_storage_error cause);

/* Native ownership admission only, never a source dispatch preflight. These
 * validate/carry the independently captured constructor u192 identity.
 * owned_valid never depends on replaceable source u. Retirement additionally
 * requires source u to be captured, NULL or a shaped empty borrowed List;
 * foreign empty Lists are never adopted, cleared or freed. Registration and
 * removal separately require current u to be the captured descriptor. Release
 * requires prior validation and no entered World call; it invokes no callback
 * and keeps the descriptor allocated for final World destruction. */
bool mc_world_observers_owned_valid(const mc_world_runtime *world);
bool mc_world_observers_retirement_valid(const mc_world_runtime *world);
void mc_world_observers_owned_release(mc_world_runtime *world);

#endif
