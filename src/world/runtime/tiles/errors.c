#include "mc/world/runtime/tiles/errors.h"

static mc_registry_error empty_registry_error(void)
{
    return (mc_registry_error){.status = MC_REGISTRY_OK,
                               .message = NULL,
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_class = NULL,
                               .constructor_pc = -1,
                               .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                               .active_initializer_class = NULL,
                               .active_initializer_pc = -1,
                               .item_argument = -1,
                               .format_error = {.status = MC_JAVA_FORMAT_OK}};
}

static mc_entity_error empty_entity_error(void)
{
    return (mc_entity_error){.status = MC_ENTITY_OK,
                             .operation = NULL,
                             .bytecode = 0,
                             .profile = MC_JAVA_RANDOM_PROFILE_OK,
                             .item = MC_ITEM_STACK_OK,
                             .world = MC_WORLD_STORAGE_OK};
}

mc_tile_error mc_tile_empty_error(void)
{
    return (mc_tile_error){.status = MC_TILE_OK,
                           .field = NULL,
                           .item_index = SIZE_MAX,
                           .item = {.status = MC_ITEM_STACK_OK,
                                    .field = NULL,
                                    .nbt = {.status = MC_NBT_OK, .offset = 0}},
                           .nbt = {.status = MC_NBT_OK, .offset = 0},
                           .source_bytecode = 0,
                           .registry = empty_registry_error(),
                           .constructor_bytecode = 0,
                           .initializer_bytecode = 0};
}

mc_live_tile_error mc_live_tiles_empty_error(void)
{
    return (mc_live_tile_error){
        .status = MC_WORLD_STORAGE_OK,
        .operation = NULL,
        .bytecode = 0,
        .chunk_bytecode = 0,
        .observer_bytecode = 0,
        .initializer_bytecode = 0,
        .cause = {.status = MC_WORLD_STORAGE_OK, .operation = NULL, .system_error = 0},
        .registry = empty_registry_error(),
        .tile = mc_tile_empty_error(),
        .entity = empty_entity_error(),
        .publication = {.status = MC_WORLD_STORAGE_OK,
                        .operation = NULL,
                        .bytecode = 0,
                        .presence_bytecode = 0,
                        .provider_bytecode = 0,
                        .chunk_bytecode = 0,
                        .observer_bytecode = 0,
                        .acquisition = MC_CHUNK_ACQUIRE_OK,
                        .cause = {.status = MC_WORLD_STORAGE_OK,
                                  .operation = NULL,
                                  .system_error = 0},
                        .entity = empty_entity_error()},
        .batch = {.index = SIZE_MAX},
        .caller_bytecode = 0,
        .hook_bytecode = 0,
        .remove_bytecode = 0,
        .tile_bytecode = 0,
        .inventory_bytecode = 0};
}

const char *mc_tile_status_string(mc_tile_status status)
{
    switch (status) {
        case MC_TILE_OK:
            return "ok";
        case MC_TILE_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_TILE_INVALID_KIND:
            return "wrong concrete tile kind";
        case MC_TILE_INVALID_PHASE:
            return "invalid construction phase";
        case MC_TILE_INVALID_FIELD:
            return "wrong tile field type";
        case MC_TILE_INVALID_SLOT:
            return "invalid physical chest slot";
        case MC_TILE_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_TILE_LIMIT:
            return "resource limit";
        case MC_TILE_WORLD_FAILED:
            return "concrete runtime operation failed";
        case MC_TILE_ITEM_FAILED:
            return "item stack operation failed";
        case MC_TILE_NBT_ERROR:
            return "invalid NBT result";
        case MC_TILE_STATE_ERROR:
            return "native tile state invariant failed";
    }
    return "unknown tile status";
}
