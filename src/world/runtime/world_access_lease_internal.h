#ifndef MC_WORLD_RUNTIME_WORLD_ACCESS_LEASE_INTERNAL_H
#define MC_WORLD_RUNTIME_WORLD_ACCESS_LEASE_INTERNAL_H

#include "mc/world/runtime/world_access_error.h"

typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_registry_string mc_registry_string;
typedef struct mc_entity mc_entity;
typedef struct mc_tile_entity mc_tile_entity;

typedef enum mc_world_access_failure_lease_phase {
    MC_WORLD_ACCESS_FAILURE_LEASE_IDLE,
    MC_WORLD_ACCESS_FAILURE_LEASE_ARMED,
    MC_WORLD_ACCESS_FAILURE_LEASE_HELD
} mc_world_access_failure_lease_phase;

/* Native ownership node, supplied in stable precharged caller storage. The
 * representation is private: use init/inspect/release, never copy or move it.
 * HELD owns the current invocation's EXISTING receiver active occurrence and
 * guards its actual returned receipt independently from receipt.holds. */
typedef struct mc_world_access_failure_lease {
    const struct mc_world_access_failure_lease *identity;
    mc_world_access_failure_lease_phase phase;
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    struct mc_world_access_failure_lease *previous, *next;
    mc_world_access_failure failure;
    mc_world_storage_error callback_error;
    mc_world_storage_status callback_status;
} mc_world_access_failure_lease;

/* Initialize exactly once in fresh storage before any source invocation.
 * IDLE successful retirement is reusable without reinitializing. Calling init
 * on a live node would discard rights and is prohibited by the caller contract. */
void mc_world_access_failure_lease_init(mc_world_access_failure_lease *);
/* Returns only the actual protected same-invocation view. It borrows this
 * HELD node and expires when checked release succeeds. False preserves out. */
bool mc_world_access_failure_lease_inspect(const mc_world_access_failure_lease *,
                                           const mc_world_access_failure **out);
/* Clear all outer borrowed copies before releasing their owner. This clears
 * the node's view before unlinking its receipt guard and dropping its one
 * transferred active right. Structural refusal consumes nothing; retry the
 * SAME node after correction. IDLE release is idempotent. No callback runs. */
mc_world_storage_status mc_world_access_failure_lease_release(mc_world_access_failure_lease *,
                                                              mc_world_storage_error *cleanup);

/* Each held_slot is the actual owning occurrence from mc_world_access_next.
 * Nonnull node must be fresh IDLE stable storage, admitted before the callback.
 * Every valid concrete failure returned by a source callback, including native
 * LIMIT/ALLOCATION with no Throwable class, transfers that occurrence and sets
 * *held_slot=NULL BEFORE
 * the enclosing dispatcher's normal release_held. Success/native-adapter
 * failure/value-only null receiver leaves it with the caller. A NULL node
 * selects the unchanged legacy invocation contract. The caller always releases
 * the remaining held_slot exactly once. No World/Registry/Entity owner is
 * acquired by this layer; their source argument lifetimes remain caller-owned.
 * All owner/node/output storage is mutually disjoint and access is exclusive. */
mc_world_storage_status
mc_world_access_call_cell_with_lease(mc_world_access **held_slot, mc_world_runtime *, int32_t,
                                     int32_t, int32_t, mc_world_access_failure_lease *,
                                     mc_world_storage_error *, mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_range_with_lease(
    mc_world_access **held_slot, mc_world_runtime *, int32_t, int32_t, int32_t, int32_t, int32_t,
    int32_t, mc_world_access_failure_lease *, mc_world_storage_error *, mc_world_access_failure *);
mc_world_storage_status
mc_world_access_call_sound_with_lease(mc_world_access **held_slot, mc_world_runtime *,
                                      const mc_registry_string *, double, double, double, float,
                                      float, mc_world_access_failure_lease *,
                                      mc_world_storage_error *, mc_world_access_failure *);
mc_world_storage_status
mc_world_access_call_particle_with_lease(mc_world_access **held_slot, mc_world_runtime *,
                                         const mc_registry_string *, double, double, double, double,
                                         double, double, mc_world_access_failure_lease *,
                                         mc_world_storage_error *, mc_world_access_failure *);
mc_world_storage_status
mc_world_access_call_entity_added_with_lease(mc_world_access **held_slot, mc_world_runtime *,
                                             mc_entity *, mc_world_access_failure_lease *,
                                             mc_world_storage_error *, mc_world_access_failure *);
mc_world_storage_status
mc_world_access_call_entity_removed_with_lease(mc_world_access **held_slot, mc_world_runtime *,
                                               mc_entity *, mc_world_access_failure_lease *,
                                               mc_world_storage_error *, mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_sky_changed_with_lease(mc_world_access **held_slot,
                                                                    mc_world_runtime *,
                                                                    mc_world_access_failure_lease *,
                                                                    mc_world_storage_error *,
                                                                    mc_world_access_failure *);
mc_world_storage_status mc_world_access_call_tile_entity_dirty_with_lease(
    mc_world_access **held_slot, mc_world_runtime *, int32_t, int32_t, int32_t, mc_tile_entity *,
    mc_world_access_failure_lease *, mc_world_storage_error *, mc_world_access_failure *);
mc_world_storage_status
mc_world_access_call_auxiliary_with_lease(mc_world_access **held_slot, mc_world_runtime *,
                                          mc_entity *, int32_t, int32_t, int32_t, int32_t, int32_t,
                                          mc_world_access_failure_lease *, mc_world_storage_error *,
                                          mc_world_access_failure *);

#endif
