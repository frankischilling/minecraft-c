#include "mc/world/runtime/tile_entities/errors.h"

static mc_registry_error empty_registry_error(void) {
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

static mc_entity_error empty_entity_error(void) {
    return (mc_entity_error){.status = MC_ENTITY_OK,
                             .operation = NULL,
                             .bytecode = 0,
                             .profile = MC_JAVA_RANDOM_PROFILE_OK,
                             .item = MC_ITEM_STACK_OK,
                             .world = MC_WORLD_STORAGE_OK};
}

void mc_tile_entity_error_init(mc_tile_entity_error *error) {
    static const mc_tile_entity_error zero = {0};
    if (error == NULL)
        return;
    *error = zero;
    error->item_index = SIZE_MAX;
    error->registry = empty_registry_error();
}

mc_tile_entity_error mc_tile_entity_empty_error(void) {
    mc_tile_entity_error error;
    mc_tile_entity_error_init(&error);
    return error;
}

void mc_live_tile_entities_error_init(mc_live_tile_entity_error *error) {
    static const mc_live_tile_entity_error zero = {0};
    if (error == NULL)
        return;
    *error = zero;
    error->registry = empty_registry_error();
    mc_tile_entity_error_init(&error->tile_entity);
    error->entity = empty_entity_error();
    error->publication.entity = empty_entity_error();
    mc_world_entity_batch_error_init(&error->batch);
    mc_world_entity_load_error_init(&error->load_entity);
}

mc_live_tile_entity_error mc_live_tile_entities_empty_error(void) {
    mc_live_tile_entity_error error;
    mc_live_tile_entities_error_init(&error);
    return error;
}

const char *mc_tile_entity_status_string(mc_tile_entity_status status) {
    switch (status) {
        case MC_TILE_ENTITY_OK:
            return "ok";
        case MC_TILE_ENTITY_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_TILE_ENTITY_INVALID_KIND:
            return "wrong concrete TileEntity kind";
        case MC_TILE_ENTITY_INVALID_PHASE:
            return "invalid construction phase";
        case MC_TILE_ENTITY_INVALID_FIELD:
            return "wrong TileEntity field type";
        case MC_TILE_ENTITY_INVALID_SLOT:
            return "invalid physical chest slot";
        case MC_TILE_ENTITY_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_TILE_ENTITY_LIMIT:
            return "resource limit";
        case MC_TILE_ENTITY_WORLD_FAILED:
            return "concrete runtime operation failed";
        case MC_TILE_ENTITY_ITEM_FAILED:
            return "item stack operation failed";
        case MC_TILE_ENTITY_NBT_ERROR:
            return "invalid NBT result";
        case MC_TILE_ENTITY_STATE_ERROR:
            return "native TileEntity state invariant failed";
        case MC_TILE_ENTITY_UNSUPPORTED_FP:
            return "unsupported floating environment";
    }
    return "unknown TileEntity status";
}
