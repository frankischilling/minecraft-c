#include "block_queries_internal.h"
#include "runtime_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "collision/blocks_internal.h"
#include "mc/world/runtime/blocks.h"

static mc_world_solid_support_error empty_error(void) {
    return (mc_world_solid_support_error){.registry = {.initializer_pc = -1,
                                                       .block_id = -1,
                                                       .constructor_pc = -1,
                                                       .active_initializer_pc = -1,
                                                       .item_argument = -1}};
}

/* Native admission failures do not attribute a nonexistent Java operation
 * or change World diagnostics. */
static mc_world_storage_status admission(mc_world_solid_support_error *error,
                                         mc_world_storage_status status, const char *operation) {
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status registry_failure(mc_world_runtime *world,
                                                mc_world_solid_support_error *error,
                                                const mc_registry_error *cause,
                                                const char *operation) {
    error->registry = *cause;
    error->status =
        mc_world_runtime_registry_result(world, cause->status, cause, operation, &error->cause);
    error->operation = error->cause.operation;
    return error->status;
}

/* The selected virtual method is uu.d()/na.b(), not opacity uu.c()/na.a().
 * All 69 existing concrete kinds are resolved from both original declarations:
 * 33 inherit the base constant true, 35 select a constant false body, and only
 * ys/pw reads a live subtype field. No registry ID or final-property table is
 * involved, and a constructing slab still reads its current own b field. */
static mc_registry_status block_predicate(const mc_block *block, bool *out,
                                          mc_registry_error *error) {
    switch (block->kind) {
        case MC_BLOCK_BASE:
        case MC_BLOCK_STONE:
        case MC_BLOCK_GRASS:
        case MC_BLOCK_DIRT:
        case MC_BLOCK_SAND:
        case MC_BLOCK_GRAVEL:
        case MC_BLOCK_ORE:
        case MC_BLOCK_LOG:
        case MC_BLOCK_LEAVES:
        case MC_BLOCK_SPONGE:
        case MC_BLOCK_GLASS:
        case MC_BLOCK_DISPENSER:
        case MC_BLOCK_SANDSTONE:
        case MC_BLOCK_NOTE_BLOCK:
        case MC_BLOCK_WOOL:
        case MC_BLOCK_MINERAL_STORAGE:
        case MC_BLOCK_TNT:
        case MC_BLOCK_BOOKSHELF:
        case MC_BLOCK_OBSIDIAN:
        case MC_BLOCK_MOB_SPAWNER:
        case MC_BLOCK_CHEST:
        case MC_BLOCK_CRAFTING_TABLE:
        case MC_BLOCK_FURNACE:
        case MC_BLOCK_REDSTONE_ORE:
        case MC_BLOCK_ICE:
        case MC_BLOCK_SNOW_BLOCK:
        case MC_BLOCK_CLAY:
        case MC_BLOCK_JUKEBOX:
        case MC_BLOCK_PUMPKIN:
        case MC_BLOCK_NETHERRACK:
        case MC_BLOCK_SOUL_SAND:
        case MC_BLOCK_GLOWSTONE:
        case MC_BLOCK_LOCKED_CHEST:
            *out = true;
            return MC_REGISTRY_OK;

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
            *out = false;
            return MC_REGISTRY_OK;

        case MC_BLOCK_SLAB:
            *out = block->subtype.ys.b;
            return MC_REGISTRY_OK;

        case MC_BLOCK_KIND_COUNT:
        default:
            error->status = MC_REGISTRY_INVALID_ARGUMENT;
            error->message = "World solid-support native Block kind";
            return error->status;
    }
}

/* Caller holds this exact World for the complete source operation. The World
 * class binding already retains the Registry: the captured m reference and
 * its elements remain live across the acquiring getter's synchronous reentry.
 * No new borrow, array snapshot or coordinate preflight is introduced here. */
static mc_world_storage_status solid_support(mc_world_runtime *world, int32_t x, int32_t y,
                                             int32_t z, bool *out,
                                             mc_world_solid_support_error *error,
                                             mc_world_nearby_error *access) {
    error->bytecode = 0;
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error cause = empty_error().registry;
    if (classes == NULL) {
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "World solid-support Registry binding";
        return registry_failure(world, error, &cause, cause.message);
    }
    mc_registry_block_m_view array;
    if (mc_registry_block_m_get(classes, &array, &cause) != MC_REGISTRY_OK)
        return registry_failure(world, error, &cause, "World solid-support Block.m active use");

    error->bytecode = 7;
    int32_t id;
    mc_world_storage_status status;
    if (access == NULL) {
        status = mc_world_runtime_block_get(world, x, y, z, &id, &error->cause);
    } else {
        mc_world_collision_error lower = mc_world_collision_empty_error();
        status = mc_world_collision_block_value_held(world, x, y, z, false, &id, &lower);
        if (status != MC_WORLD_STORAGE_OK) {
            *access = lower.nearby;
            error->cause = lower.nearby.query.cause;
            error->registry = lower.nearby.query.registry;
        }
    }
    if (status != MC_WORLD_STORAGE_OK) {
        error->status = status;
        error->operation = error->cause.operation;
        world->last_error = error->cause;
        /* The public getter exposes only its storage cause. The held reader
         * supplies its own complete child above; neither path infers a newly
         * reached Registry cause from World.last_registry_error. */
        return status;
    }

    error->bytecode = 10;
    const mc_block *block;
    if (mc_registry_block_m_at(&array, id, &block, &cause) != MC_REGISTRY_OK)
        return registry_failure(world, error, &cause,
                                "World solid-support captured Block.m element");
    if (block == NULL) {
        *out = false;
        return MC_WORLD_STORAGE_OK;
    }

    error->bytecode = 22;
    const mc_material *material = mc_block_fields_get(block)->bA;
    error->bytecode = 25;
    if (material == NULL) {
        cause = empty_error().registry;
        cause.status = MC_REGISTRY_NULL_REFERENCE;
        cause.message = "World solid-support Material receiver";
        return registry_failure(world, error, &cause, cause.message);
    }
    if (!mc_material_h(material)) {
        *out = false;
        return MC_WORLD_STORAGE_OK;
    }

    error->bytecode = 33;
    bool value;
    cause = empty_error().registry;
    if (block_predicate(block, &value, &cause) != MC_REGISTRY_OK)
        return registry_failure(world, error, &cause, "World solid-support Block predicate");
    *out = value;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_world_solid_support_detailed_held(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                     bool *out, mc_world_solid_support_detail *detail) {
    mc_world_solid_support_detail frame = {.error = empty_error(),
                                           .access = mc_world_nearby_empty_error()};
    mc_world_storage_status status;
    if (world == NULL || out == NULL) {
        status = admission(&frame.error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "World solid-support held arguments");
    } else {
        status = solid_support(world, x, y, z, out, &frame.error, &frame.access);
        if (status == MC_WORLD_STORAGE_OK)
            frame = (mc_world_solid_support_detail){.error = empty_error(),
                                                    .access = mc_world_nearby_empty_error()};
    }
    if (detail != NULL)
        *detail = frame;
    return status;
}

mc_world_storage_status mc_world_runtime_solid_support_get(mc_world_runtime *world, int32_t x,
                                                           int32_t y, int32_t z, bool *out,
                                                           mc_world_solid_support_error *error) {
    mc_world_solid_support_error frame = empty_error();
    mc_world_storage_status status;
    if (world == NULL || out == NULL) {
        status =
            admission(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World solid-support arguments");
    } else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
               world->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        status = admission(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World solid-support phase");
    } else {
        status = mc_world_runtime_call_enter(world, &frame.cause);
        if (status != MC_WORLD_STORAGE_OK) {
            frame.status = status;
            frame.operation = frame.cause.operation;
        } else {
            status = solid_support(world, x, y, z, out, &frame, NULL);
            mc_world_runtime_call_leave(world);
            if (status == MC_WORLD_STORAGE_OK)
                frame = empty_error();
        }
    }
    if (error != NULL)
        *error = frame;
    return status;
}
