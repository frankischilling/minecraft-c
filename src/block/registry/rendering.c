#include "mc/block/registry/rendering.h"
#include "registry_internal.h"

_Static_assert(MC_BLOCK_KIND_COUNT == 69, "audit new Block rendering descendants");

/* Client uu descendants and cv callers, pinned in the private renderer
 * research. Rendering dispatch reads live subclass state, never an ID table. */
static bool valid(const mc_block *block) {
    return block != NULL && block->classes != NULL &&
           (unsigned int)block->kind < (unsigned int)MC_BLOCK_KIND_COUNT;
}

mc_registry_status mc_block_render_describe(const mc_block *block,
                                            mc_block_render_description *out) {
    if (!valid(block) || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_block_render_description value = {0};
    switch (block->kind) {
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_COBWEB:
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_SUGAR_CANE:
            value.type = 1;
            break;
        case MC_BLOCK_TORCH:
        case MC_BLOCK_REDSTONE_TORCH:
            value.type = 2;
            break;
        case MC_BLOCK_FIRE:
            value.type = 3;
            break;
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
            value.type = 4;
            value.fluid = true;
            value.pass = block->fields.bA ==
                                 mc_materials_get(block->classes->options.materials, MC_MATERIAL_g)
                             ? 1
                             : 0;
            break;
        case MC_BLOCK_REDSTONE:
            value.type = 5;
            break;
        case MC_BLOCK_CROPS:
            value.type = 6;
            break;
        case MC_BLOCK_DOOR:
            value.type = 7;
            break;
        case MC_BLOCK_LADDER:
            value.type = 8;
            break;
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
            value.type = 9;
            break;
        case MC_BLOCK_STAIRS:
            value.type = 10;
            break;
        case MC_BLOCK_FENCE:
            value.type = 11;
            break;
        case MC_BLOCK_LEVER:
            value.type = 12;
            break;
        case MC_BLOCK_CACTUS:
            value.type = 13;
            value.cactus = true;
            break;
        case MC_BLOCK_BED:
            value.type = 14;
            break;
        case MC_BLOCK_REPEATER:
            value.type = 15;
            break;
        case MC_BLOCK_PISTON_BASE:
            value.type = 16;
            break;
        case MC_BLOCK_PISTON_HEAD:
            value.type = 17;
            break;
        case MC_BLOCK_SIGN:
        case MC_BLOCK_MOVING_PISTON:
            value.type = -1;
            break;
        case MC_BLOCK_ICE:
        case MC_BLOCK_PORTAL:
            value.pass = 1;
            break;
        default:
            break;
    }
    value.grass = block->kind == MC_BLOCK_GRASS;
    value.tall_grass = block->kind == MC_BLOCK_TALL_GRASS;
    *out = value;
    return MC_REGISTRY_OK;
}

bool mc_block_render_opaque(const mc_block *block) {
    return valid(block) && mc_registry_block_opaque(block);
}

mc_registry_status mc_block_render_finish_fluid(mc_block *block) {
    if (!valid(block) ||
        (block->kind != MC_BLOCK_FLOWING_FLUID && block->kind != MC_BLOCK_STILL_FLUID))
        return MC_REGISTRY_INVALID_ARGUMENT;
    block->fields.bt = 0.0;
    block->fields.bw = 1.0;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_block_render_bounds(mc_block *block, float x0, float y0, float z0, float x1,
                                          float y1, float z1) {
    if (!valid(block))
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_block_bounds(block, x0, y0, z0, x1, y1, z1);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_block_render_fancy_leaves(mc_registry_classes *classes, bool fancy) {
    if (classes == NULL || classes->blocks_phase != MC_REGISTRY_READY)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_block *block = classes->blocks.named[17]; /* uu.L, field u..bl */
    /* Use the actual class receiver, not the numerical slot as a replacement
     * for a missing static field. L is cell17 in this declaration sequence. */
    return mc_block_render_fancy_leaves_receiver(block, fancy);
}

mc_registry_status mc_block_render_fancy_leaves_receiver(mc_block *block, bool fancy) {
    if (!valid(block) || block->kind != MC_BLOCK_LEAVES)
        return MC_REGISTRY_NULL_REFERENCE;
    block->nr_b = fancy;
    block->fields.bm = mc_java_i32_add(block->subtype.bk.c, fancy ? 0 : 1);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_block_render_update_bounds(mc_block *block, const mc_block_render_access *a,
                                                 int32_t x, int32_t y, int32_t z) {
    if (!valid(block) || a == NULL || a->metadata == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    int32_t m;
    switch (block->kind) {
        case MC_BLOCK_STAIRS:
            mc_registry_block_bounds(block, 0, 0, 0, 1, 1, 1);
            break;
        case MC_BLOCK_PORTAL:
            if (a->block == NULL)
                return MC_REGISTRY_INVALID_ARGUMENT;
            if (a->block(a->owner, x - 1, y, z) == block->fields.bn ||
                a->block(a->owner, x + 1, y, z) == block->fields.bn)
                mc_registry_block_bounds(block, 0, 0, .375f, 1, 1, .625f);
            else
                mc_registry_block_bounds(block, .375f, 0, 0, .625f, 1, 1);
            break;
        case MC_BLOCK_SNOW_LAYER:
            m = a->metadata(a->owner, x, y, z) & 7;
            mc_registry_block_bounds(block, 0, 0, 0, 1, (float)(2 * (1 + m)) / 16.0f, 1);
            break;
        case MC_BLOCK_PRESSURE_PLATE:
            m = a->metadata(a->owner, x, y, z);
            mc_registry_block_bounds(block, .0625f, 0, .0625f, .9375f, m == 1 ? .03125f : .0625f,
                                     .9375f);
            break;
        case MC_BLOCK_BUTTON: {
            m = a->metadata(a->owner, x, y, z);
            float thickness = (m & 8) != 0 ? .0625f : .125f;
            switch (m & 7) {
                case 1:
                    mc_registry_block_bounds(block, 0, .375f, .3125f, thickness, .625f, .6875f);
                    break;
                case 2:
                    mc_registry_block_bounds(block, 1 - thickness, .375f, .3125f, 1, .625f, .6875f);
                    break;
                case 3:
                    mc_registry_block_bounds(block, .3125f, .375f, 0, .6875f, .625f, thickness);
                    break;
                case 4:
                    mc_registry_block_bounds(block, .3125f, .375f, 1 - thickness, .6875f, .625f, 1);
                    break;
                default:
                    break; /* Original invalid metadata retains prior bounds. */
            }
            break;
        }
        case MC_BLOCK_CAKE:
            m = a->metadata(a->owner, x, y, z);
            mc_registry_block_bounds(block, (float)(1 + m * 2) / 16.0f, 0, .0625f, .9375f, .5f,
                                     .9375f);
            break;
        case MC_BLOCK_TRAPDOOR:
            m = a->metadata(a->owner, x, y, z);
            mc_registry_block_bounds(block, 0, 0, 0, 1, .1875f, 1);
            if ((m & 4) != 0) {
                switch (m & 3) {
                    case 0:
                        mc_registry_block_bounds(block, 0, 0, .8125f, 1, 1, 1);
                        break;
                    case 1:
                        mc_registry_block_bounds(block, 0, 0, 0, 1, 1, .1875f);
                        break;
                    case 2:
                        mc_registry_block_bounds(block, .8125f, 0, 0, 1, 1, 1);
                        break;
                    case 3:
                        mc_registry_block_bounds(block, 0, 0, 0, .1875f, 1, 1);
                        break;
                }
            }
            break;
        /* These specialized callbacks belong to geometry families that the
         * mesher rejects explicitly before treating any shape as common. */
        case MC_BLOCK_DOOR:
        case MC_BLOCK_LEVER:
        case MC_BLOCK_SIGN:
        case MC_BLOCK_MOVING_PISTON:
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_PISTON_HEAD:
            return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        default:
            break; /* Inherited uu.a(xp,xyz) is an empty body. */
    }
    return MC_REGISTRY_OK;
}

mc_registry_status mc_block_render_texture_metadata(const mc_block *block, int32_t side, int32_t m,
                                                    int32_t *out) {
    if (!valid(block) || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    int32_t t = block->fields.bm;
    switch (block->kind) {
        case MC_BLOCK_SAPLING:
            t = (m & 3) == 1 ? 63 : (m & 3) == 2 ? 79 : t;
            break;
        case MC_BLOCK_LOG:
            t = side <= 1 ? 21 : m == 1 ? 116 : m == 2 ? 117 : 20;
            break;
        case MC_BLOCK_LEAVES:
            t = (m & 3) == 1 ? t + 80 : t;
            break;
        case MC_BLOCK_TALL_GRASS:
            t = m == 2 ? t + 17 : m == 0 ? t + 16 : t;
            break;
        case MC_BLOCK_WOOL:
            if (m != 0) {
                int32_t n = ~(m & 15);
                t = 113 + ((n & 8) >> 3) + (n & 7) * 16;
            }
            break;
        case MC_BLOCK_SLAB:
            t = m == 0   ? (side <= 1 ? 6 : 5)
                : m == 1 ? (side == 0   ? 208
                            : side == 1 ? 176
                                        : 192)
                : m == 2 ? 4
                : m == 3 ? 16
                         : 6;
            break;
        case MC_BLOCK_FARMLAND:
            t = side == 1 ? t - (m > 0 ? 1 : 0) : 2;
            break;
        case MC_BLOCK_PUMPKIN: {
            int32_t front = t + 17 + (block->subtype.fc.a ? 1 : 0);
            t = side <= 1 ? t
                : ((m == 2 && side == 2) || (m == 3 && side == 5) || (m == 0 && side == 3) ||
                   (m == 1 && side == 4))
                    ? front
                    : t + 16;
            break;
        }
        case MC_BLOCK_CROPS:
            t += m < 0 ? 7 : m;
            break;
        case MC_BLOCK_CAKE:
            t += side == 0 ? 3 : side == 1 ? 0 : (m > 0 && side == 4) ? 2 : 1;
            break;
        case MC_BLOCK_STAIRS:
            return mc_block_render_texture_metadata(block->subtype.ss.a, side, m, out);
        /* These metadata methods are consumed by still-unimplemented specialized
         * geometry, so no invented inherited result is published. */
        case MC_BLOCK_BED:
        case MC_BLOCK_DOOR:
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_PISTON_HEAD:
            return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        default:
            return mc_registry_block_texture_side(block->classes, block, side, out);
    }
    *out = t;
    return MC_REGISTRY_OK;
}

static int32_t chest_texture(const mc_block *b, const mc_block_render_access *a, int32_t x,
                             int32_t y, int32_t z, int32_t side, bool locked) {
    int32_t t = b->fields.bm;
    if (side <= 1)
        return t - 1;
    int32_t north = a->block(a->owner, x, y, z - 1);
    int32_t south = a->block(a->owner, x, y, z + 1);
    int32_t west = a->block(a->owner, x - 1, y, z);
    int32_t east = a->block(a->owner, x + 1, y, z);
    const bool *o = b->classes->blocks.o;
    if (!locked && (north == b->fields.bn || south == b->fields.bn)) {
        if (side == 2 || side == 3)
            return t;
        int32_t offset = north == b->fields.bn ? -1 : 0;
        int32_t zz = north == b->fields.bn ? z - 1 : z + 1;
        int32_t nw = a->block(a->owner, x - 1, y, zz);
        int32_t ne = a->block(a->owner, x + 1, y, zz);
        if (side == 4)
            offset = -1 - offset;
        int32_t front = 5;
        if ((o[west] || o[nw]) && !o[east] && !o[ne])
            front = 5;
        if ((o[east] || o[ne]) && !o[west] && !o[nw])
            front = 4;
        return (side == front ? t + 16 : t + 32) + offset;
    }
    if (!locked && (west == b->fields.bn || east == b->fields.bn)) {
        if (side == 4 || side == 5)
            return t;
        int32_t offset = west == b->fields.bn ? -1 : 0;
        int32_t xx = west == b->fields.bn ? x - 1 : x + 1;
        int32_t nw = a->block(a->owner, xx, y, z - 1);
        int32_t sw = a->block(a->owner, xx, y, z + 1);
        if (side == 3)
            offset = -1 - offset;
        int32_t front = 3;
        if ((o[north] || o[nw]) && !o[south] && !o[sw])
            front = 3;
        if ((o[south] || o[sw]) && !o[north] && !o[nw])
            front = 2;
        return (side == front ? t + 16 : t + 32) + offset;
    }
    int32_t front = 3;
    if (o[north] && !o[south])
        front = 3;
    if (o[south] && !o[north])
        front = 2;
    if (o[west] && !o[east])
        front = 5;
    if (o[east] && !o[west])
        front = 4;
    return side == front ? t + 1 : t;
}

mc_registry_status mc_block_render_texture(const mc_block *b, const mc_block_render_access *a,
                                           int32_t x, int32_t y, int32_t z, int32_t side,
                                           int32_t *out) {
    if (!valid(b) || a == NULL || out == NULL || a->metadata == NULL || a->material == NULL ||
        a->block == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    int32_t t;
    switch (b->kind) {
        case MC_BLOCK_GRASS: {
            if (side == 1)
                t = 0;
            else if (side == 0)
                t = 2;
            else {
                mc_material *above = a->material(a->owner, x, y + 1, z);
                t = above == mc_materials_get(b->classes->options.materials, MC_MATERIAL_t) ||
                            above == mc_materials_get(b->classes->options.materials, MC_MATERIAL_u)
                        ? 68
                        : 3;
            }
            break;
        }
        case MC_BLOCK_DISPENSER:
            if (side <= 1)
                t = b->fields.bm + 17;
            else
                t = side == a->metadata(a->owner, x, y, z) ? b->fields.bm + 1 : b->fields.bm;
            break;
        case MC_BLOCK_FURNACE:
            if (side <= 1)
                t = b->fields.bm + 17;
            else if (side != a->metadata(a->owner, x, y, z))
                t = b->fields.bm;
            else
                t = b->fields.bm + (b->subtype.tc.b ? 16 : -1);
            break;
        case MC_BLOCK_CHEST:
        case MC_BLOCK_LOCKED_CHEST:
            t = chest_texture(b, a, x, y, z, side, b->kind == MC_BLOCK_LOCKED_CHEST);
            break;
        default:
            return mc_block_render_texture_metadata(b, side, a->metadata(a->owner, x, y, z), out);
    }
    *out = t;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_block_render_color(const mc_block *b, const mc_block_render_access *a,
                                         int32_t x, int32_t y, int32_t z, uint32_t *out) {
    if (!valid(b) || a == NULL || out == NULL || a->metadata == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    const uint32_t *map = a->grass_colors;
    size_t count = a->grass_color_count;
    if (b->kind != MC_BLOCK_GRASS && b->kind != MC_BLOCK_LEAVES && b->kind != MC_BLOCK_TALL_GRASS) {
        *out = 0xffffff;
        return MC_REGISTRY_OK;
    }
    if (b->kind == MC_BLOCK_LEAVES) {
        int32_t m = a->metadata(a->owner, x, y, z);
        if ((m & 1) != 0) {
            *out = 0x619961;
            return MC_REGISTRY_OK;
        }
        if ((m & 2) != 0) {
            *out = 8431445;
            return MC_REGISTRY_OK;
        }
        map = a->foliage_colors;
        count = a->foliage_color_count;
    } else if (b->kind == MC_BLOCK_TALL_GRASS) {
        if (a->metadata(a->owner, x, y, z) == 0) {
            *out = 0xffffff;
            return MC_REGISTRY_OK;
        }
        int64_t n = mc_java_i32_add(
            mc_java_i32_add(mc_java_i32_mul(x, 3129871), mc_java_i32_mul(z, 6129781)), y);
        n = mc_java_i64_add(mc_java_i64_mul(mc_java_i64_mul(n, n), 42317861),
                            mc_java_i64_mul(n, 11));
        x = mc_java_i32_narrow(mc_java_i64_add(x, mc_java_i64_shr(n, 14) & 31));
        z = mc_java_i32_narrow(mc_java_i64_add(z, mc_java_i64_shr(n, 24) & 31));
    }
    if (map == NULL || count < 65536 || a->climate == NULL)
        return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
    double temperature, rain;
    mc_registry_status status = a->climate(a->owner, x, z, &temperature, &rain);
    if (status != MC_REGISTRY_OK)
        return status;
    rain *= temperature;
    int32_t u = mc_java_double_to_i32((1.0 - temperature) * 255.0);
    int32_t v = mc_java_double_to_i32((1.0 - rain) * 255.0);
    int32_t index = mc_java_i32_shl(v, 8) | u;
    if (index < 0 || (size_t)index >= count)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    *out = map[index] & UINT32_C(0xffffff);
    return MC_REGISTRY_OK;
}

static bool base_side(const mc_block *b, const mc_block_render_access *a, int32_t x, int32_t y,
                      int32_t z, int32_t side) {
    const mc_block_fields *f = &b->fields;
    if ((side == 0 && f->bt > 0) || (side == 1 && f->bw < 1) || (side == 2 && f->bu > 0) ||
        (side == 3 && f->bx < 1) || (side == 4 && f->bs > 0) || (side == 5 && f->bv < 1))
        return true;
    return !a->opaque(a->owner, x, y, z);
}

mc_registry_status mc_block_render_side(const mc_block *b, const mc_block_render_access *a,
                                        int32_t x, int32_t y, int32_t z, int32_t side, bool *out) {
    if (!valid(b) || a == NULL || out == NULL || a->block == NULL || a->material == NULL ||
        a->opaque == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    bool value;
    if (b->kind == MC_BLOCK_FLOWING_FLUID || b->kind == MC_BLOCK_STILL_FLUID) {
        mc_material *neighbor = a->material(a->owner, x, y, z);
        if (neighbor == b->fields.bA ||
            neighbor == mc_materials_get(b->classes->options.materials, MC_MATERIAL_s))
            value = false;
        else
            value = side == 1 || base_side(b, a, x, y, z, side);
    } else if (b->kind == MC_BLOCK_PORTAL) {
        if (a->block(a->owner, x, y, z) == b->fields.bn)
            value = false;
        else {
            bool west = a->block(a->owner, x - 1, y, z) == b->fields.bn &&
                        a->block(a->owner, x - 2, y, z) != b->fields.bn;
            bool east = a->block(a->owner, x + 1, y, z) == b->fields.bn &&
                        a->block(a->owner, x + 2, y, z) != b->fields.bn;
            bool north = a->block(a->owner, x, y, z - 1) == b->fields.bn &&
                         a->block(a->owner, x, y, z - 2) != b->fields.bn;
            bool south = a->block(a->owner, x, y, z + 1) == b->fields.bn &&
                         a->block(a->owner, x, y, z + 2) != b->fields.bn;
            value = ((west || east) && (side == 4 || side == 5)) ||
                    ((north || south) && (side == 2 || side == 3));
        }
    } else if (b->kind == MC_BLOCK_LEAVES && !b->nr_b &&
               a->block(a->owner, x, y, z) == b->fields.bn)
        value = false;
    else if (b->kind == MC_BLOCK_GLASS && !b->jp_a && a->block(a->owner, x, y, z) == b->fields.bn)
        value = false;
    else if (b->kind == MC_BLOCK_ICE && !b->jp_a && a->block(a->owner, x, y, z) == b->fields.bn)
        value = false;
    else if (b->kind == MC_BLOCK_SNOW_LAYER && side == 1)
        value = true;
    else
        value = base_side(b, a, x, y, z, b->kind == MC_BLOCK_ICE ? 1 - side : side);
    *out = value;
    return MC_REGISTRY_OK;
}
