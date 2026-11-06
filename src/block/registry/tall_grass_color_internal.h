#ifndef MC_BLOCK_REGISTRY_TALL_GRASS_COLOR_INTERNAL_H
#define MC_BLOCK_REGISTRY_TALL_GRASS_COLOR_INTERNAL_H

#include "mc/core/java/java.h"
#include "world_color_ops_internal.h"

/* RU.b(Lxp;III)I. This hash is the wrapped sum used for climate coordinates,
 * independently of CV's different crossed-plant geometry jitter. */
static inline bool mc_tall_grass_world_color(const mc_block_world_color_ops *ops, void *context,
                                             int32_t x, int32_t y, int32_t z, int32_t *out) {
    int32_t metadata, value;
    if (!ops->metadata(context, x, y, z, 5, &metadata))
        return false;
    if (metadata == 0) {
        value = 16777215;
        ops->pc(context, 19);
    } else {
        int64_t hash = mc_java_i32_add(
            mc_java_i32_add(mc_java_i32_mul(x, 3129871), mc_java_i32_mul(z, 6129781)), y);
        hash = mc_java_i64_add(mc_java_i64_mul(mc_java_i64_mul(hash, hash), 42317861),
                               mc_java_i64_mul(hash, 11));
        x = mc_java_i32_narrow(mc_java_i64_add(x, mc_java_i64_shr(hash, 14) & 31));
        /* RU stores an adjusted local Y at79, then never reads that local. */
        z = mc_java_i32_narrow(mc_java_i64_add(z, mc_java_i64_shr(hash, 24) & 31));
        double temperature, rainfall;
        if (!ops->climate(context, x, z, 98, 108) ||
            !ops->channel(context, false, 113, 118, 122, &temperature) ||
            !ops->channel(context, true, 126, 131, 135, &rainfall) ||
            !ops->lookup(context, temperature, rainfall, 142, &value))
            return false;
        ops->pc(context, 145);
    }
    *out = value;
    return true;
}

#endif
