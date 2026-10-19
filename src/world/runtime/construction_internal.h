#ifndef MC_WORLD_RUNTIME_CONSTRUCTION_INTERNAL_H
#define MC_WORLD_RUNTIME_CONSTRUCTION_INTERNAL_H

#include "mc/world/runtime/lighting.h"

struct mc_world_reference_list {
    void **items;
    size_t count, capacity;
};

struct mc_world_light_type {
    mc_world_runtime_classes *classes;
    mc_world_light_type_fields fields;
};

struct mc_world_runtime_classes {
    mc_world_info_side side;
    mc_java_random_profile *profile;
    const mc_math_trig *trig;
    mc_registry_classes *registry;
    int32_t enqueue_depth;
    size_t live_worlds;
    bool initialized;
    mc_world_light_type light_types[2];
    const mc_world_light_type *light_statics[2];
    bool light_types_initialized;
};

#endif
