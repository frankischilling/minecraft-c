#ifndef MC_WORLD_RUNTIME_WORLD_ACCESS_H
#define MC_WORLD_RUNTIME_WORLD_ACCESS_H

#include "mc/world/runtime/construction.h"
#include "mc/world/storage/status.h"

typedef struct mc_world_access mc_world_access;
typedef struct mc_entity mc_entity;
typedef struct mc_tile_entity mc_tile_entity;

/* Native adapters for the seven presently reached pm/jp method slots. Each
 * callback receives the actual World and already-evaluated source arguments.
 * Returning non-OK stops that source loop without rolling back earlier work.
 * error carries a static operation string and native system_error; an absent
 * operation is diagnosed as "WorldAccess callback". Missing slots remain
 * explicit reached failures, never successful no-ops. */
typedef struct mc_world_access_callbacks {
    mc_world_storage_status (*cell)(void *context, mc_world_runtime *world, int32_t x, int32_t y,
                                    int32_t z, mc_world_storage_error *error);
    mc_world_storage_status (*range)(void *context, mc_world_runtime *world, int32_t min_x,
                                     int32_t min_y, int32_t min_z, int32_t max_x, int32_t max_y,
                                     int32_t max_z, mc_world_storage_error *error);
    mc_world_storage_status (*sound)(void *context, mc_world_runtime *world,
                                     const mc_registry_string *name, double x, double y, double z,
                                     float volume, float pitch, mc_world_storage_error *error);
    mc_world_storage_status (*particle)(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, double velocity_x, double velocity_y,
                                        double velocity_z, mc_world_storage_error *error);
    mc_world_storage_status (*entity_added)(void *context, mc_world_runtime *world,
                                            mc_entity *entity, mc_world_storage_error *error);
    mc_world_storage_status (*tile_entity_dirty)(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z, mc_tile_entity *tile_entity,
                                                 mc_world_storage_error *error);
    mc_world_storage_status (*entity_removed)(void *context, mc_world_runtime *world,
                                              mc_entity *entity, mc_world_storage_error *error);
} mc_world_access_callbacks;

/* Copies the table and borrows context until successful free. The table may
 * contain missing slots and context may be NULL. No callback, World access,
 * Registry active use or Random operation runs here. Allocation failure
 * preserves out. Free(NULL) succeeds; free is BUSY while any World occurrence
 * or current callback holds this identity. A failed free leaves it live.
 * Callers keep context and callback code live until free succeeds. */
mc_world_storage_status mc_world_access_create(const mc_world_access_callbacks *callbacks,
                                               void *context, mc_world_access **out,
                                               mc_world_storage_error *error);
mc_world_storage_status mc_world_access_free(mc_world_access *world_access,
                                             mc_world_storage_error *error);

/* Actual World.u registration: append each duplicate or NULL occurrence in
 * order on either side. Removal implements only the researched client entry;
 * it removes the first identical native handle (or first NULL), and preserves
 * out on failure. Server removal is explicitly unavailable.
 *
 * Require exclusive access to the World and handle, including when shared by
 * Worlds. Synchronous callback reentry may add/remove registrations. Dispatch
 * uses the current index and live list on every iteration: it neither snapshots
 * membership nor skips mutations. The current WorldAccess stays held until its
 * callback returns, so self-removal cannot make an in-progress free succeed.
 * Keep callback arguments and disjoint error storage live through the call.
 *
 * GAME RAW/constructing/failed prefixes with the reached constructor-created u
 * are admitted; retired/composition Worlds reject. Native limits are distinct
 * from Java allocation-failure timing. Terminal World retirement releases every
 * valid owned occurrence without invoking callbacks, including on the server. */
mc_world_storage_status mc_world_runtime_add_world_access(mc_world_runtime *world,
                                                          mc_world_access *world_access,
                                                          mc_world_storage_error *error);
mc_world_storage_status mc_world_runtime_remove_world_access(mc_world_runtime *world,
                                                             mc_world_access *world_access,
                                                             bool *out,
                                                             mc_world_storage_error *error);

#endif
