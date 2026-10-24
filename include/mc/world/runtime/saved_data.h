#ifndef MC_WORLD_RUNTIME_SAVED_DATA_H
#define MC_WORLD_RUNTIME_SAVED_DATA_H

#include "mc/world/runtime/construction.h"

/* Actual fd/dj named-data and next-ID wrappers. Each call captures the current
 * GAME World.z and delegates directly to that same manager. No replacement
 * manager, independent cache, class check on a cache hit, session operation,
 * metadata save, clock or Random call is introduced. The manager's ownership,
 * caught-diagnostic and partial-mutation contracts apply unchanged. get returns
 * an owned value hold when nonnull; its output is preserved on escaping errors.
 *
 * The World and arguments require exclusive access through the call. Native
 * admission rejects COMPOSITION and retired owners; a missing source z reports
 * UNSUPPORTED_STATE. Output/error storage must be disjoint from the owners and
 * each other. Current MapData support does not implement map item/tracker or
 * packet updates. */
mc_world_storage_status mc_world_runtime_saved_data_get(mc_world_runtime *world,
                                                        const mc_saved_data_class *type,
                                                        mc_world_info_name *key,
                                                        mc_saved_data_value **out,
                                                        mc_world_storage_error *error);
mc_world_storage_status mc_world_runtime_saved_data_put(mc_world_runtime *world,
                                                        mc_world_info_name *key,
                                                        mc_saved_data_value *value,
                                                        mc_world_storage_error *error);
mc_world_storage_status mc_world_runtime_saved_data_next_id(mc_world_runtime *world,
                                                            const mc_nbt_string *key, int32_t *out,
                                                            mc_world_storage_error *error);

#endif
