#ifndef MC_WORLD_RUNTIME_LIGHTING_INTERNAL_H
#define MC_WORLD_RUNTIME_LIGHTING_INTERNAL_H

#include "mc/world/runtime/lighting.h"

struct mc_world_light_region {
    mc_world_light_region_fields fields;
};

/* Final native World destruction only, with all calls/views/leases retired.
 * Free the actual C records/items and reset C storage, without freeing the C
 * identity, changing class A or invoking source callbacks/propagation. */
void mc_world_runtime_lighting_free(mc_world_runtime *runtime);

#endif
