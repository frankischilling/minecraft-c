#include "mutation_internal.h"
#include "plants/plants_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../chunk_provider_internal.h"
#include "../presence_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

static bool hook_return(mc_world_mutation_error *error) {
    error->hook_class = NULL;
    error->hook_bytecode = 0;
    return true;
}

static void hook_stage(const mc_block *block, const char *client, const char *server,
                       unsigned int pc, mc_world_mutation_error *error) {
    error->hook_class = block->classes->options.side == MC_REGISTRY_CLIENT ? client : server;
    error->hook_bytecode = pc;
}

static bool receiver(mc_world_runtime *world, const mc_block *block,
                     mc_world_mutation_error *error) {
    if (block != NULL)
        return true;
    error->hook_class = NULL;
    error->hook_bytecode = 0;
    return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Block hook receiver is null");
}

static bool unavailable(mc_world_runtime *world, const mc_block *block, const char *client,
                        const char *server, const char *operation, mc_world_mutation_error *error) {
    hook_stage(block, client, server, 0, error);
    return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
}

static bool storage_fail(mc_world_runtime *world, mc_world_mutation_error *error,
                         mc_world_storage_error cause) {
    error->status = cause.status;
    error->operation = cause.operation;
    error->cause = cause;
    world->last_error = cause;
    return false;
}

static bool tile_entity_hook(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                             int32_t z, bool added, mc_world_mutation_error *error) {
    mc_live_tile_entity_error detail = mc_live_tile_entities_empty_error();
    const mc_world_storage_status status =
        added ? mc_live_tile_entities_block_added(world, block, x, y, z, &detail)
              : mc_live_tile_entities_block_removed(world, block, x, y, z, &detail);
    if (status == MC_WORLD_STORAGE_OK)
        return hook_return(error);
    /* Keep the reached concrete hook frame beside its whole typed child. */
    hook_stage(block, block->kind == MC_BLOCK_CHEST && !added ? "e" : "rw",
               block->kind == MC_BLOCK_CHEST && !added ? "e" : "lb",
               detail.hook_bytecode != 0 ? detail.hook_bytecode : detail.bytecode, error);
    error->tile_entity = detail;
    error->registry = detail.registry;
    error->entity = detail.entity;
    error->publication = detail.publication;
    return storage_fail(world, error, detail.cause);
}

/* fd/dj.a(IIIIII)Z. These are nonacquiring presence calls, not World block
 * reads. Shifted endpoints cannot increment-wrap; reversed ranges stay empty.
 * All six source shifts remain in their original order, including unused Y. */
static bool region_present(mc_world_runtime *world, int32_t min_x, int32_t min_y, int32_t min_z,
                           int32_t max_x, int32_t max_y, int32_t max_z, bool *out,
                           mc_world_mutation_error *error) {
    mc_world_presence_error frame = {0};
    mc_world_storage_status status = mc_world_runtime_region_present_held(
        world, min_x, min_y, min_z, max_x, max_y, max_z, out, &frame);
    error->presence_bytecode = frame.bytecode;
    error->provider_bytecode = frame.provider_bytecode;
    return status == MC_WORLD_STORAGE_OK || storage_fail(world, error, frame.cause);
}

/* vg/nk and bk/ar.b(World,III)V. Their loop variables are small offsets,
 * whereas every absolute coordinate addition/subtraction is a wrapped int.
 * Log skips an already-set bit; leaf still reaches an equal metadata write. */
static bool mark_leaves(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                        int32_t z, bool log, mc_world_mutation_error *error) {
    const int32_t radius = log ? 4 : 1;
    const int32_t margin = radius + 1;
    hook_stage(block, log ? "vg" : "bk", log ? "nk" : "ar", 36, error);
    bool present;
    if (!region_present(world, mc_java_i32_sub(x, margin), mc_java_i32_sub(y, margin),
                        mc_java_i32_sub(z, margin), mc_java_i32_add(x, margin),
                        mc_java_i32_add(y, margin), mc_java_i32_add(z, margin), &present, error))
        return false;
    if (!present)
        return hook_return(error);

    for (int32_t dx = -radius; dx <= radius; ++dx) {
        for (int32_t dy = -radius; dy <= radius; ++dy) {
            for (int32_t dz = -radius; dz <= radius; ++dz) {
                const int32_t cell_x = mc_java_i32_add(x, dx);
                const int32_t cell_y = mc_java_i32_add(y, dy);
                const int32_t cell_z = mc_java_i32_add(z, dz);
                int32_t id;
                mc_world_storage_error cause = {0};
                error->hook_bytecode = 92;
                if (mc_world_runtime_block_get(world, cell_x, cell_y, cell_z, &id, &cause) !=
                    MC_WORLD_STORAGE_OK)
                    return storage_fail(world, error, cause);

                /* The getstatic follows the acquiring getter, on every cell.
                 * Keep the actual nullable live object through getfield102. */
                const mc_block *leaves;
                mc_registry_error registry = mc_world_mutation_empty_registry();
                error->hook_bytecode = 99;
                if (mc_registry_block_static_get(block->classes, "L", &leaves, &registry) !=
                    MC_REGISTRY_OK)
                    return mc_world_mutation_registry_fail(world, error, &registry,
                                                           "Block removal live L active use");
                error->hook_bytecode = 102;
                if (leaves == NULL)
                    return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                  "Block removal live L receiver is null");
                if (id != leaves->fields.bn)
                    continue;

                int32_t metadata;
                error->hook_bytecode = 122;
                if (mc_world_runtime_metadata_get(world, cell_x, cell_y, cell_z, &metadata,
                                                  &cause) != MC_WORLD_STORAGE_OK)
                    return storage_fail(world, error, cause);
                if (log && (metadata & 8) != 0)
                    continue;
                bool ordinary;
                error->hook_bytecode = log ? 154u : 146u;
                if (mc_world_runtime_metadata_set_quiet(world, cell_x, cell_y, cell_z, metadata | 8,
                                                        &ordinary, &cause) != MC_WORLD_STORAGE_OK)
                    return storage_fail(world, error, cause);
                /* Original boolean result is discarded, including false. */
            }
        }
    }
    return hook_return(error);
}

bool mc_world_mutation_block_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_world_mutation_error *error) {
    if (!receiver(world, block, error))
        return false;
    switch (block->kind) {
        case MC_BLOCK_BASE:
        case MC_BLOCK_STONE:
        case MC_BLOCK_GRASS:
        case MC_BLOCK_DIRT:
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
        case MC_BLOCK_SAND:
        case MC_BLOCK_GRAVEL:
        case MC_BLOCK_ORE:
        case MC_BLOCK_GLASS:
        case MC_BLOCK_SANDSTONE:
        case MC_BLOCK_BED:
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_COBWEB:
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_WOOL:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_MINERAL_STORAGE:
        case MC_BLOCK_SLAB:
        case MC_BLOCK_TNT:
        case MC_BLOCK_BOOKSHELF:
        case MC_BLOCK_OBSIDIAN:
        case MC_BLOCK_TORCH:
        case MC_BLOCK_FIRE:
        case MC_BLOCK_CRAFTING_TABLE:
        case MC_BLOCK_CROPS:
        case MC_BLOCK_FARMLAND:
        case MC_BLOCK_DOOR:
        case MC_BLOCK_LADDER:
        case MC_BLOCK_REDSTONE_ORE:
        case MC_BLOCK_SNOW_LAYER:
        case MC_BLOCK_ICE:
        case MC_BLOCK_SNOW_BLOCK:
        case MC_BLOCK_CACTUS:
        case MC_BLOCK_CLAY:
        case MC_BLOCK_SUGAR_CANE:
        case MC_BLOCK_FENCE:
        case MC_BLOCK_PUMPKIN:
        case MC_BLOCK_NETHERRACK:
        case MC_BLOCK_SOUL_SAND:
        case MC_BLOCK_GLOWSTONE:
        case MC_BLOCK_PORTAL:
        case MC_BLOCK_CAKE:
        case MC_BLOCK_REPEATER:
        case MC_BLOCK_LOCKED_CHEST:
        case MC_BLOCK_TRAPDOOR:
            return hook_return(error); /* uu/na.b: return0. */
        case MC_BLOCK_LOG:
            return mark_leaves(world, block, x, y, z, true, error);
        case MC_BLOCK_LEAVES:
            return mark_leaves(world, block, x, y, z, false, error);
        case MC_BLOCK_NOTE_BLOCK:
        case MC_BLOCK_SIGN:
            return unavailable(world, block, "rw", "lb", "Block TileEntity removal hook", error);
        case MC_BLOCK_MOB_SPAWNER:
            return tile_entity_hook(world, block, x, y, z, false, error);
        case MC_BLOCK_SPONGE:
            return unavailable(world, block, "xf", "ov", "Block sponge removal hook", error);
        case MC_BLOCK_DISPENSER:
            return unavailable(world, block, "xq", "pa", "Block dispenser removal hook", error);
        case MC_BLOCK_PISTON_HEAD:
            return unavailable(world, block, "h", "g", "Block removal hook", error);
        case MC_BLOCK_MOVING_PISTON:
            return mc_world_mutation_piston_removed(world, block, x, y, z, error);
        case MC_BLOCK_STAIRS:
            return unavailable(world, block, "ss", "lu", "Block removal hook", error);
        case MC_BLOCK_CHEST:
            return tile_entity_hook(world, block, x, y, z, false, error);
        case MC_BLOCK_REDSTONE:
            return mc_world_mutation_redstone_removed(world, block, x, y, z, error);
        case MC_BLOCK_FURNACE:
            return unavailable(world, block, "tc", "lz", "Block furnace removal hook", error);
        case MC_BLOCK_LEVER:
            return unavailable(world, block, "xr", "pd", "Block removal hook", error);
        case MC_BLOCK_PRESSURE_PLATE:
            return mc_world_mutation_pressure_removed(world, block, x, y, z, error);
        case MC_BLOCK_REDSTONE_TORCH:
            return unavailable(world, block, "db", "by", "Block removal hook", error);
        case MC_BLOCK_BUTTON:
            return unavailable(world, block, "oi", "iz", "Block removal hook", error);
        case MC_BLOCK_JUKEBOX:
            return unavailable(world, block, "fo", "dt", "Block removal hook", error);
        default:
            return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Block removal native kind");
    }
}

bool mc_world_mutation_block_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                   int32_t y, int32_t z, mc_world_mutation_error *error) {
    (void)x;
    (void)y;
    (void)z;
    if (!receiver(world, block, error))
        return false;
    switch (block->kind) {
        case MC_BLOCK_BASE:
        case MC_BLOCK_STONE:
        case MC_BLOCK_GRASS:
        case MC_BLOCK_DIRT:
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_ORE:
        case MC_BLOCK_LOG:
        case MC_BLOCK_LEAVES:
        case MC_BLOCK_GLASS:
        case MC_BLOCK_SANDSTONE:
        case MC_BLOCK_BED:
        case MC_BLOCK_COBWEB:
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_PISTON_HEAD:
        case MC_BLOCK_WOOL:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_MINERAL_STORAGE:
        case MC_BLOCK_BOOKSHELF:
        case MC_BLOCK_OBSIDIAN:
        case MC_BLOCK_CRAFTING_TABLE:
        case MC_BLOCK_CROPS:
        case MC_BLOCK_FARMLAND:
        case MC_BLOCK_DOOR:
        case MC_BLOCK_LADDER:
        case MC_BLOCK_LEVER:
        case MC_BLOCK_REDSTONE_ORE:
        case MC_BLOCK_BUTTON:
        case MC_BLOCK_SNOW_LAYER:
        case MC_BLOCK_ICE:
        case MC_BLOCK_SNOW_BLOCK:
        case MC_BLOCK_CACTUS:
        case MC_BLOCK_CLAY:
        case MC_BLOCK_SUGAR_CANE:
        case MC_BLOCK_FENCE:
        case MC_BLOCK_NETHERRACK:
        case MC_BLOCK_SOUL_SAND:
        case MC_BLOCK_GLOWSTONE:
        case MC_BLOCK_PORTAL:
        case MC_BLOCK_CAKE:
        case MC_BLOCK_LOCKED_CHEST:
        case MC_BLOCK_TRAPDOOR:
            return hook_return(error); /* uu/na.c: return0. */
        case MC_BLOCK_MOVING_PISTON:
            return hook_return(error); /* ut/mz.c is itself return0. */
        case MC_BLOCK_PUMPKIN:
            /* fc/di.c invokes the proved base body6, then returns9. */
            hook_stage(block, "fc", "di", 6, error);
            return hook_return(error);
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
            return mc_world_mutation_fluid_added(world, block, x, y, z, error);
        case MC_BLOCK_SAND:
        case MC_BLOCK_GRAVEL:
            return mc_world_mutation_falling_added(world, block, x, y, z, error);
        case MC_BLOCK_NOTE_BLOCK:
        case MC_BLOCK_MOB_SPAWNER:
        case MC_BLOCK_CHEST:
        case MC_BLOCK_SIGN:
        case MC_BLOCK_JUKEBOX:
            return tile_entity_hook(world, block, x, y, z, true, error);
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
            return mc_world_mutation_rail_added_hook(world, block, x, y, z, error);
        case MC_BLOCK_SPONGE:
            return unavailable(world, block, "xf", "ov", "Block sponge addition hook", error);
        case MC_BLOCK_DISPENSER:
            return unavailable(world, block, "xq", "pa", "Block dispenser addition hook", error);
        case MC_BLOCK_PISTON_BASE:
            return unavailable(world, block, "jq", "gi", "Block addition hook", error);
        case MC_BLOCK_SLAB:
            return unavailable(world, block, "ys", "pw", "Block slab addition hook", error);
        case MC_BLOCK_TNT:
            return unavailable(world, block, "ah", "t", "Block addition hook", error);
        case MC_BLOCK_TORCH:
            return unavailable(world, block, "vm", "np", "Block addition hook", error);
        case MC_BLOCK_FIRE:
            return unavailable(world, block, "yq", "pt", "Block fire addition hook", error);
        case MC_BLOCK_STAIRS:
            return unavailable(world, block, "ss", "lu", "Block addition hook", error);
        case MC_BLOCK_REDSTONE:
            return mc_world_mutation_redstone_added(world, block, x, y, z, error);
        case MC_BLOCK_FURNACE:
            return unavailable(world, block, "tc", "lz", "Block furnace addition hook", error);
        case MC_BLOCK_PRESSURE_PLATE:
            return hook_return(error); /* bv/bc.c is itself return0. */
        case MC_BLOCK_REDSTONE_TORCH:
            return unavailable(world, block, "db", "by", "Block addition hook", error);
        case MC_BLOCK_REPEATER:
            return unavailable(world, block, "wo", "oi", "Block addition hook", error);
        default:
            return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Block addition native kind");
    }
}

bool mc_world_mutation_block_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t source_id,
                                      mc_world_mutation_error *error) {
    (void)x;
    (void)y;
    (void)z;
    (void)source_id;
    if (!receiver(world, block, error))
        return false;
    switch (block->kind) {
        case MC_BLOCK_BASE:
        case MC_BLOCK_STONE:
        case MC_BLOCK_GRASS:
        case MC_BLOCK_DIRT:
        case MC_BLOCK_ORE:
        case MC_BLOCK_LOG:
        case MC_BLOCK_LEAVES:
        case MC_BLOCK_SPONGE:
        case MC_BLOCK_GLASS:
        case MC_BLOCK_SANDSTONE:
        case MC_BLOCK_COBWEB:
        case MC_BLOCK_WOOL:
        case MC_BLOCK_MINERAL_STORAGE:
        case MC_BLOCK_SLAB:
        case MC_BLOCK_BOOKSHELF:
        case MC_BLOCK_OBSIDIAN:
        case MC_BLOCK_MOB_SPAWNER:
        case MC_BLOCK_STAIRS:
        case MC_BLOCK_CHEST:
        case MC_BLOCK_CRAFTING_TABLE:
        case MC_BLOCK_FURNACE:
        case MC_BLOCK_REDSTONE_ORE:
        case MC_BLOCK_ICE:
        case MC_BLOCK_SNOW_BLOCK:
        case MC_BLOCK_CLAY:
        case MC_BLOCK_JUKEBOX:
        case MC_BLOCK_FENCE:
        case MC_BLOCK_PUMPKIN:
        case MC_BLOCK_NETHERRACK:
        case MC_BLOCK_SOUL_SAND:
        case MC_BLOCK_GLOWSTONE:
        case MC_BLOCK_LOCKED_CHEST:
            return hook_return(error); /* uu/na.b(World,IIII): return0. */
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_CROPS:
            return mc_world_mutation_plant_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
            return mc_world_mutation_fluid_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_SAND:
        case MC_BLOCK_GRAVEL:
            return mc_world_mutation_falling_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
            return mc_world_mutation_rail_neighbor_hook(world, block, x, y, z, source_id, error);
        case MC_BLOCK_DISPENSER:
            return unavailable(world, block, "xq", "pa", "Block dispenser neighbor hook", error);
        case MC_BLOCK_NOTE_BLOCK:
            return unavailable(world, block, "pt", "jr", "Block neighbor hook", error);
        case MC_BLOCK_BED:
            return unavailable(world, block, "ve", "nj", "Block neighbor hook", error);
        case MC_BLOCK_PISTON_BASE:
            return unavailable(world, block, "jq", "gi", "Block neighbor hook", error);
        case MC_BLOCK_PISTON_HEAD:
            return unavailable(world, block, "h", "g", "Block neighbor hook", error);
        case MC_BLOCK_MOVING_PISTON:
            return mc_world_mutation_piston_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_TNT:
            return unavailable(world, block, "ah", "t", "Block neighbor hook", error);
        case MC_BLOCK_TORCH:
            return unavailable(world, block, "vm", "np", "Block neighbor hook", error);
        case MC_BLOCK_FIRE:
            return unavailable(world, block, "yq", "pt", "Block fire neighbor hook", error);
        case MC_BLOCK_REDSTONE:
            return mc_world_mutation_redstone_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_FARMLAND:
            return mc_plant_farmland_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_SIGN:
            return unavailable(world, block, "uj", "mt", "Block sign neighbor hook", error);
        case MC_BLOCK_DOOR:
            return unavailable(world, block, "le", "hc", "Block neighbor hook", error);
        case MC_BLOCK_LADDER:
            return unavailable(world, block, "dp", "cg", "Block neighbor hook", error);
        case MC_BLOCK_LEVER:
            return unavailable(world, block, "xr", "pd", "Block neighbor hook", error);
        case MC_BLOCK_PRESSURE_PLATE:
            return mc_world_mutation_pressure_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_REDSTONE_TORCH:
            return unavailable(world, block, "db", "by", "Block neighbor hook", error);
        case MC_BLOCK_BUTTON:
            return unavailable(world, block, "oi", "iz", "Block neighbor hook", error);
        case MC_BLOCK_SNOW_LAYER:
            return mc_world_mutation_snow_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_CACTUS:
        case MC_BLOCK_SUGAR_CANE:
            return mc_world_mutation_plant_neighbor(world, block, x, y, z, source_id, error);
        case MC_BLOCK_PORTAL:
            return unavailable(world, block, "ak", "z", "Block neighbor hook", error);
        case MC_BLOCK_CAKE:
            return unavailable(world, block, "um", "mw", "Block neighbor hook", error);
        case MC_BLOCK_REPEATER:
            return unavailable(world, block, "wo", "oi", "Block neighbor hook", error);
        case MC_BLOCK_TRAPDOOR:
            return unavailable(world, block, "oq", "jd", "Block neighbor hook", error);
        default:
            return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Block neighbor native kind");
    }
}
