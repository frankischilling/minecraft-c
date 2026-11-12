#ifndef MC_WORLD_RUNTIME_BLOCK_NORMAL_INTERNAL_H
#define MC_WORLD_RUNTIME_BLOCK_NORMAL_INTERNAL_H
#include "../../block/registry/registry_internal.h"

/* Exact declaring client body selected by the same closed normal predicate.
 * This passive identity read performs no source active use or virtual call. */
static inline const char *mc_world_source_block_normal_declaring(const mc_block *block) {
    switch (block->kind) {
        case MC_BLOCK_PORTAL:
            return "ak";
        case MC_BLOCK_PRESSURE_PLATE:
            return "bv";
        case MC_BLOCK_LADDER:
            return "dp";
        case MC_BLOCK_PISTON_HEAD:
            return "h";
        case MC_BLOCK_PISTON_BASE:
            return "jq";
        case MC_BLOCK_SNOW_LAYER:
            return "jr";
        case MC_BLOCK_FENCE:
            return "jw";
        case MC_BLOCK_DOOR:
            return "le";
        case MC_BLOCK_BUTTON:
            return "oi";
        case MC_BLOCK_TRAPDOOR:
            return "oq";
        case MC_BLOCK_CACTUS:
            return "or";
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_RAIL:
            return "pc";
        case MC_BLOCK_SUGAR_CANE:
            return "ri";
        case MC_BLOCK_COBWEB:
            return "rn";
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
            return "rp";
        case MC_BLOCK_REDSTONE:
            return "sm";
        case MC_BLOCK_STAIRS:
            return "ss";
        case MC_BLOCK_SIGN:
            return "uj";
        case MC_BLOCK_CAKE:
            return "um";
        case MC_BLOCK_MOVING_PISTON:
            return "ut";
        case MC_BLOCK_BED:
            return "ve";
        case MC_BLOCK_FARMLAND:
            return "vl";
        case MC_BLOCK_REDSTONE_TORCH:
        case MC_BLOCK_TORCH:
            return "vm";
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_CROPS:
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_FLOWER:
            return "wb";
        case MC_BLOCK_REPEATER:
            return "wo";
        case MC_BLOCK_LEVER:
            return "xr";
        case MC_BLOCK_FIRE:
            return "yq";
        case MC_BLOCK_SLAB:
            return "ys";
        default:
            return "uu";
    }
}

/* Actual uu.d()Z closed dispatch on a proven nonnull canonical Block.
 * No Registry active use, World query or opaque-array substitution occurs. */
static inline bool mc_world_source_block_normal(const mc_block *block) {
    /* Complete client uu.d()Z dispatch: constant-false declaring bodies and
     * their descendants, exact ys.b, otherwise inherited uu literal true. */
    switch (block->kind) {
        case MC_BLOCK_SLAB:
            return block->subtype.ys.b;
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
        case MC_BLOCK_BED:
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_COBWEB:
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_PISTON_HEAD:
        case MC_BLOCK_MOVING_PISTON:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_TORCH:
        case MC_BLOCK_FIRE:
        case MC_BLOCK_STAIRS:
        case MC_BLOCK_REDSTONE:
        case MC_BLOCK_CROPS:
        case MC_BLOCK_FARMLAND:
        case MC_BLOCK_SIGN:
        case MC_BLOCK_DOOR:
        case MC_BLOCK_LADDER:
        case MC_BLOCK_LEVER:
        case MC_BLOCK_PRESSURE_PLATE:
        case MC_BLOCK_REDSTONE_TORCH:
        case MC_BLOCK_BUTTON:
        case MC_BLOCK_SNOW_LAYER:
        case MC_BLOCK_CACTUS:
        case MC_BLOCK_SUGAR_CANE:
        case MC_BLOCK_FENCE:
        case MC_BLOCK_PORTAL:
        case MC_BLOCK_CAKE:
        case MC_BLOCK_REPEATER:
        case MC_BLOCK_TRAPDOOR:
            return false;
        default:
            return true;
    }
}

#endif
