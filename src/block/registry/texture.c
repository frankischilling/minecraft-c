#include "registry_internal.h"

/* uu/na.a(I)I virtual dispatch over the actual closed Block classes. These
 * queries do not latch native failures or change game/class state. The caller
 * owns its source failure context; out must be disjoint from owner storage. */
mc_registry_status mc_registry_block_texture_side(mc_registry_classes *classes,
                                                  const mc_block *block, int32_t side,
                                                  int32_t *out) {
    if (classes == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (block == NULL)
        return MC_REGISTRY_NULL_REFERENCE;
    if (block->classes != classes || (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT)
        return MC_REGISTRY_INVALID_ARGUMENT;

    int32_t texture;
    switch (block->kind) {
        /* These fifty kinds inherit the actual uu/na body: read bm1/return4. */
        case MC_BLOCK_BASE:
        case MC_BLOCK_STONE:
        case MC_BLOCK_GRASS:
        case MC_BLOCK_DIRT:
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_SAND:
        case MC_BLOCK_GRAVEL:
        case MC_BLOCK_ORE:
        case MC_BLOCK_LOG:
        case MC_BLOCK_LEAVES:
        case MC_BLOCK_SPONGE:
        case MC_BLOCK_GLASS:
        case MC_BLOCK_BED:
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_COBWEB:
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_PISTON_HEAD:
        case MC_BLOCK_WOOL:
        case MC_BLOCK_MOVING_PISTON:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_OBSIDIAN:
        case MC_BLOCK_TORCH:
        case MC_BLOCK_FIRE:
        case MC_BLOCK_MOB_SPAWNER:
        case MC_BLOCK_REDSTONE:
        case MC_BLOCK_CROPS:
        case MC_BLOCK_FARMLAND:
        case MC_BLOCK_SIGN:
        case MC_BLOCK_DOOR:
        case MC_BLOCK_LADDER:
        case MC_BLOCK_LEVER:
        case MC_BLOCK_PRESSURE_PLATE:
        case MC_BLOCK_REDSTONE_ORE:
        case MC_BLOCK_REDSTONE_TORCH:
        case MC_BLOCK_BUTTON:
        case MC_BLOCK_SNOW_LAYER:
        case MC_BLOCK_ICE:
        case MC_BLOCK_SNOW_BLOCK:
        case MC_BLOCK_CLAY:
        case MC_BLOCK_SUGAR_CANE:
        case MC_BLOCK_FENCE:
        case MC_BLOCK_NETHERRACK:
        case MC_BLOCK_SOUL_SAND:
        case MC_BLOCK_GLOWSTONE:
        case MC_BLOCK_PORTAL:
        case MC_BLOCK_TRAPDOOR:
        /* pt/jr and l/j each declare their own independently read bm body. */
        case MC_BLOCK_NOTE_BLOCK:
        case MC_BLOCK_MINERAL_STORAGE:
            texture = block->fields.bm;
            break;

        /* Both fluid kinds select their rp/kv ancestor's a(I)I. */
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
            if (side == 0 || side == 1)
                texture = block->fields.bm;
            else
                texture = mc_java_i32_add(block->fields.bm, 1);
            break;

        case MC_BLOCK_DISPENSER:
            if (side == 1 || side == 0)
                texture = mc_java_i32_add(block->fields.bm, 17);
            else if (side == 3)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_SANDSTONE:
            if (side == 1)
                texture = mc_java_i32_sub(block->fields.bm, 16);
            else if (side == 0)
                texture = mc_java_i32_add(block->fields.bm, 16);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_SLAB:
            /* a(I)I passes metadata0 atPC2 to the actual a(II)I atPC3.
             * Its selected metadata0 branch returns6 for side<=1, else5. */
            texture = side <= 1 ? 6 : 5;
            break;

        case MC_BLOCK_TNT:
            if (side == 0)
                texture = mc_java_i32_add(block->fields.bm, 2);
            else if (side == 1)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_BOOKSHELF:
            texture = side <= 1 ? 4 : block->fields.bm;
            break;

        case MC_BLOCK_STAIRS: {
            /* getfield a1 retains the actual source before virtual call5. */
            const mc_block *source = block->subtype.ss.a;
            return mc_registry_block_texture_side(classes, source, side, out);
        }

        /* e/e and ks/gv independently declare the same face/bm arithmetic. */
        case MC_BLOCK_CHEST:
        case MC_BLOCK_LOCKED_CHEST:
            if (side == 1 || side == 0)
                texture = mc_java_i32_sub(block->fields.bm, 1);
            else if (side == 3)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_CRAFTING_TABLE:
            if (side == 1)
                texture = mc_java_i32_sub(block->fields.bm, 16);
            else if (side == 0) {
                /* Actual getstatic uu.y/na.y17, followed by virtual a(0)21.
                 * named[4] is that declaring field, not an m-slot shortcut. */
                const mc_block *source = classes->blocks.named[4];
                return mc_registry_block_texture_side(classes, source, 0, out);
            } else if (side == 2 || side == 4)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_FURNACE:
            if (side == 1 || side == 0)
                texture = mc_java_i32_add(block->fields.bm, 17);
            else if (side == 3)
                texture = mc_java_i32_sub(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_CACTUS:
            if (side == 1)
                texture = mc_java_i32_sub(block->fields.bm, 1);
            else if (side == 0)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_JUKEBOX:
            /* The source loads bm1 before choosing the increment5..13. */
            texture = block->fields.bm;
            texture = mc_java_i32_add(texture, side == 1 ? 1 : 0);
            break;

        case MC_BLOCK_PUMPKIN:
            if (side == 1 || side == 0)
                texture = block->fields.bm;
            else if (side == 3) {
                texture = mc_java_i32_add(block->fields.bm, 1);
                texture = mc_java_i32_add(texture, 16);
            } else
                texture = mc_java_i32_add(block->fields.bm, 16);
            break;

        case MC_BLOCK_CAKE:
            if (side == 1)
                texture = block->fields.bm;
            else if (side == 0)
                texture = mc_java_i32_add(block->fields.bm, 3);
            else
                texture = mc_java_i32_add(block->fields.bm, 1);
            break;

        case MC_BLOCK_REPEATER:
            /* a(I)I calls the actual a(II)I with metadata0. That body ignores
             * metadata and reads its declaring-class c only for faces0/1. */
            if (side == 0)
                texture = block->subtype.wo.c ? 99 : 115;
            else if (side == 1)
                texture = block->subtype.wo.c ? 147 : 131;
            else
                texture = 5;
            break;

        case MC_BLOCK_KIND_COUNT:
        default:
            return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = texture;
    return MC_REGISTRY_OK;
}
