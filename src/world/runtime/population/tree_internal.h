#ifndef MC_WORLD_RUNTIME_POPULATION_TREE_INTERNAL_H
#define MC_WORLD_RUNTIME_POPULATION_TREE_INTERNAL_H

#include "mc/world/runtime/population/trees.h"

/* The production owner definition is shared by the implementation and
 * private state inspections. Keep the public owner opaque. */
struct tree_context;
struct mc_world_tree_big_oak {
    mc_java_random random;
    struct tree_context *context;      /* Transient native frame, never a World adapter. */
    mc_world_runtime_classes *classes; /* Application-lifetime native borrow. */
    mc_world_runtime *world;           /* Actual retained source c. */
    int32_t base[3];
    int32_t height_limit, trunk_height;
    double height_attenuation, unused_h, branch_slope, width_scale, leaf_density;
    int32_t trunk_size, height_random_limit, leaf_distance;
    int32_t (*nodes)[4];
    int32_t node_count;
    size_t allocation_bytes, max_allocation_bytes;
    bool entered;
};

#endif
