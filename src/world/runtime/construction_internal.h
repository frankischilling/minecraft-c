#ifndef MC_WORLD_RUNTIME_CONSTRUCTION_INTERNAL_H
#define MC_WORLD_RUNTIME_CONSTRUCTION_INTERNAL_H

#include "mc/core/java/int_set.h"
#include "mc/world/runtime/lighting.h"

struct mc_world_active_chunks {
    void *entries;
    size_t count, capacity;
    mc_java_int_set *order;
};

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
    struct mc_entity_classes *entities;
    struct mc_tile_entity_classes *tile_entities;
    int32_t enqueue_depth;
    size_t live_worlds;
    bool initialized;
    mc_world_light_type light_types[2];
    const mc_world_light_type *light_statics[2];
    bool light_types_initialized;
    /* Actual lm/hi.a, default false once per shared class lifetime. */
    bool chunk_a;
    /* Separate qy/ki active-use lifetime, shared above World instances. */
    bool scheduled_initialized;
    mc_scheduled_tick_sequence scheduled_sequence;
    /* Actual client cq.b class lifetime, separate from each World.P. */
    bool spawning_initialized;
    mc_world_active_chunks spawning_chunks;
};

#endif
