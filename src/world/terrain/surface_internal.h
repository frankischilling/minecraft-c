#ifndef MC_WORLD_TERRAIN_SURFACE_INTERNAL_H
#define MC_WORLD_TERRAIN_SURFACE_INTERNAL_H

#include "mc/world/terrain/surface.h"

typedef enum mc_terrain_surface_channel {
    MC_TERRAIN_SURFACE_SAND = 0,
    MC_TERRAIN_SURFACE_GRAVEL,
    MC_TERRAIN_SURFACE_THICKNESS
} mc_terrain_surface_channel;

/* One fixed noise request only; no future span/material/Registry admission. */
bool mc_terrain_normal_surface_channel(const mc_terrain_normal_state *state,
                                       mc_terrain_surface_channel channel, int32_t x, int32_t z,
                                       mc_terrain_double_span out);

static inline bool mc_terrain_surface_patch(double noise, double uniform, double threshold)
{
    return noise + uniform * 0.2 > threshold;
}

static inline double mc_terrain_surface_thickness_base(double noise)
{
    return noise / 3.0 + 3.0;
}

static inline int32_t mc_terrain_surface_thickness_finish(double base, double uniform)
{
    return mc_java_double_to_i32(base + uniform * 0.25);
}

#endif
