#include "mutation_internal.h"
#include "plants/plants_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "mc/core/java/java.h"

static bool tick_empty(mc_world_mutation_error *error) {
    error->hook_class = NULL;
    error->hook_bytecode = 0;
    return true;
}

static bool tick_unavailable(mc_world_runtime *world, const mc_block *block, const char *client,
                             const char *server, mc_world_mutation_error *error) {
    error->hook_class = block->classes->options.side == MC_REGISTRY_CLIENT ? client : server;
    error->hook_bytecode = 0;
    return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Block tick body is not implemented");
}

/* uu/na.a(World,III,Random)V virtual resolution. The empty base does not
 * dereference World or Random. Concrete fluid/falling overrides execute their
 * actual bodies; remaining overrides stop at their declaring entry until
 * those concrete bodies are implemented. */
bool mc_world_mutation_block_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error) {
    (void)x;
    (void)y;
    (void)z;
    (void)random;
    if (block == NULL) {
        error->hook_class = NULL;
        error->hook_bytecode = 0;
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Block tick receiver is null");
    }
    switch (block->kind) {
        case MC_BLOCK_BASE:
        case MC_BLOCK_STONE:
        case MC_BLOCK_DIRT:
        case MC_BLOCK_ORE:
        case MC_BLOCK_LOG:
        case MC_BLOCK_SPONGE:
        case MC_BLOCK_GLASS:
        case MC_BLOCK_SANDSTONE:
        case MC_BLOCK_NOTE_BLOCK:
        case MC_BLOCK_BED:
        case MC_BLOCK_RAIL:
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_COBWEB:
        case MC_BLOCK_PISTON_HEAD:
        case MC_BLOCK_WOOL:
        case MC_BLOCK_MOVING_PISTON:
        case MC_BLOCK_MINERAL_STORAGE:
        case MC_BLOCK_SLAB:
        case MC_BLOCK_TNT:
        case MC_BLOCK_BOOKSHELF:
        case MC_BLOCK_OBSIDIAN:
        case MC_BLOCK_MOB_SPAWNER:
        case MC_BLOCK_CHEST:
        case MC_BLOCK_REDSTONE:
        case MC_BLOCK_CRAFTING_TABLE:
        case MC_BLOCK_FURNACE:
        case MC_BLOCK_SIGN:
        case MC_BLOCK_DOOR:
        case MC_BLOCK_LADDER:
        case MC_BLOCK_LEVER:
        case MC_BLOCK_CLAY:
        case MC_BLOCK_JUKEBOX:
        case MC_BLOCK_FENCE:
        case MC_BLOCK_PUMPKIN:
        case MC_BLOCK_NETHERRACK:
        case MC_BLOCK_SOUL_SAND:
        case MC_BLOCK_GLOWSTONE:
        case MC_BLOCK_PORTAL:
        case MC_BLOCK_CAKE:
        case MC_BLOCK_TRAPDOOR:
            return tick_empty(error);
        case MC_BLOCK_GRASS:
            return mc_world_mutation_grass_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_SAPLING:
            return mc_world_mutation_plant_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
            return mc_world_mutation_fluid_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_SAND:
        case MC_BLOCK_GRAVEL:
            return mc_world_mutation_falling_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_LEAVES:
            return mc_world_mutation_leaves_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_DISPENSER:
            return tick_unavailable(world, block, "xq", "pa", error);
        case MC_BLOCK_DETECTOR_RAIL:
            return mc_world_mutation_detector_tick_hook(world, block, x, y, z, random, error);
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
            return mc_world_mutation_plant_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_TORCH:
            return tick_unavailable(world, block, "vm", "np", error);
        case MC_BLOCK_FIRE:
            return tick_unavailable(world, block, "yq", "pt", error);
        case MC_BLOCK_STAIRS:
            return tick_unavailable(world, block, "ss", "lu", error);
        case MC_BLOCK_CROPS:
            return mc_world_mutation_plant_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_FARMLAND:
            return mc_plant_farmland_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_PRESSURE_PLATE:
            return mc_world_mutation_pressure_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_REDSTONE_ORE:
            return tick_unavailable(world, block, "bs", "az", error);
        case MC_BLOCK_REDSTONE_TORCH:
            return tick_unavailable(world, block, "db", "by", error);
        case MC_BLOCK_BUTTON:
            return tick_unavailable(world, block, "oi", "iz", error);
        case MC_BLOCK_SNOW_LAYER:
            return mc_world_mutation_snow_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_ICE:
            return mc_world_mutation_ice_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_SNOW_BLOCK:
            return mc_world_mutation_snow_block_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_CACTUS:
        case MC_BLOCK_SUGAR_CANE:
            return mc_world_mutation_plant_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_REPEATER:
            return tick_unavailable(world, block, "wo", "oi", error);
        case MC_BLOCK_LOCKED_CHEST:
            return tick_unavailable(world, block, "ks", "gv", error);
        default:
            error->hook_class = NULL;
            error->hook_bytecode = 0;
            return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Unknown concrete Block tick dispatch");
    }
}
