#ifndef MC_WORLD_RUNTIME_LIGHTING_INTERNAL_H
#define MC_WORLD_RUNTIME_LIGHTING_INTERNAL_H

#include "mc/world/runtime/lighting.h"

struct mc_world_light_region {
    mc_world_light_region_fields fields;
};

/* Current actual C List.clear with one native World call hold. The caller
 * supplies canonical owned lighting storage and excludes borrowed region
 * views. Release queued region owners in place, preserving the C identity,
 * pointer capacity, class A and propagation state. No callback or drain runs. */
mc_world_storage_status mc_world_runtime_light_list_clear(mc_world_runtime *,
                                                          mc_world_storage_error *);

/* Final native World destruction only, with all calls/views/leases retired.
 * Free the actual C records/items and reset C storage, without freeing the C
 * identity, changing class A or invoking source callbacks/propagation. */
void mc_world_runtime_lighting_free(mc_world_runtime *runtime);

#endif
