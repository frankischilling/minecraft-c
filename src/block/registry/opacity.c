#include "registry_internal.h"

bool mc_registry_block_opaque(const mc_block *block) {
    if (block == NULL)
        return false;

    switch (block->kind) {
        /* These concrete classes inherit uu.c()/na.a(). */
        case MC_BLOCK_BASE:
        case MC_BLOCK_STONE:
        case MC_BLOCK_GRASS:
        case MC_BLOCK_DIRT:
        case MC_BLOCK_SAND:
        case MC_BLOCK_GRAVEL:
        case MC_BLOCK_ORE:
        case MC_BLOCK_LOG:
        case MC_BLOCK_SPONGE:
        case MC_BLOCK_DISPENSER:
        case MC_BLOCK_SANDSTONE:
        case MC_BLOCK_NOTE_BLOCK:
        case MC_BLOCK_WOOL:
        case MC_BLOCK_MINERAL_STORAGE:
        case MC_BLOCK_TNT:
        case MC_BLOCK_BOOKSHELF:
        case MC_BLOCK_OBSIDIAN:
        case MC_BLOCK_CHEST:
        case MC_BLOCK_CRAFTING_TABLE:
        case MC_BLOCK_FURNACE:
        case MC_BLOCK_REDSTONE_ORE:
        case MC_BLOCK_SNOW_BLOCK:
        case MC_BLOCK_CLAY:
        case MC_BLOCK_JUKEBOX:
        case MC_BLOCK_PUMPKIN:
        case MC_BLOCK_NETHERRACK:
        case MC_BLOCK_SOUL_SAND:
        case MC_BLOCK_GLOWSTONE:
        case MC_BLOCK_LOCKED_CHEST:
            return true;

        /* Each selected declaring body returns the constant false. */
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
        case MC_BLOCK_GLASS:
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
        case MC_BLOCK_MOB_SPAWNER:
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
        case MC_BLOCK_ICE:
        case MC_BLOCK_CACTUS:
        case MC_BLOCK_SUGAR_CANE:
        case MC_BLOCK_FENCE:
        case MC_BLOCK_PORTAL:
        case MC_BLOCK_CAKE:
        case MC_BLOCK_REPEATER:
        case MC_BLOCK_TRAPDOOR:
            return false;

        case MC_BLOCK_LEAVES:
            return !block->nr_b;
        case MC_BLOCK_SLAB:
            return block->subtype.ys.b;
        case MC_BLOCK_KIND_COUNT:
            break;
    }
    return false;
}
