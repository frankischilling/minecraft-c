#ifndef MC_BLOCK_REGISTRY_LEAF_COLOR_INTERNAL_H
#define MC_BLOCK_REGISTRY_LEAF_COLOR_INTERNAL_H

#include "world_color_ops_internal.h"

/* BK.b(Lxp;III)I's single source body. Each adapter operates on its actual
 * XP/namespace and owns diagnostics. In particular, channel must resolve XP.a
 * afresh: the sampling callback may replace the World's Dimension. */
static inline bool mc_leaf_world_color(const mc_block_world_color_ops *ops, void *context,
                                       int32_t x, int32_t y, int32_t z, int32_t *out) {
    int32_t metadata, value;
    if (!ops->metadata(context, x, y, z, 5, &metadata))
        return false;
    if ((metadata & 1) != 0) {
        if (!ops->constant(context, true, 20, &value))
            return false;
        ops->pc(context, 23);
    } else if ((metadata & 2) != 0) {
        if (!ops->constant(context, false, 32, &value))
            return false;
        ops->pc(context, 35);
    } else {
        double temperature, rainfall;
        if (!ops->climate(context, x, z, 37, 47) ||
            !ops->channel(context, false, 52, 57, 61, &temperature) ||
            !ops->channel(context, true, 65, 70, 74, &rainfall) ||
            !ops->lookup(context, temperature, rainfall, 81, &value))
            return false;
        ops->pc(context, 84);
    }
    *out = value;
    return true;
}

#endif
