#include "mc/entity/nbt/errors.h"

mc_entity_nbt_error mc_entity_nbt_empty_error(void) {
    return (mc_entity_nbt_error){.status = MC_ENTITY_NBT_OK,
                                 .index = SIZE_MAX,
                                 .construction = {.status = MC_ENTITY_OK,
                                                  .profile = MC_JAVA_RANDOM_PROFILE_OK,
                                                  .item = MC_ITEM_STACK_OK,
                                                  .world = MC_WORLD_STORAGE_OK},
                                 .stack = {.status = MC_ITEM_STACK_OK, .nbt = {MC_NBT_OK, 0}},
                                 .nbt = {MC_NBT_OK, 0},
                                 .inventory = {.status = MC_MINECART_INVENTORY_OK}};
}

const char *mc_entity_nbt_status_string(mc_entity_nbt_status status) {
    switch (status) {
        case MC_ENTITY_NBT_OK:
            return "ok";
        case MC_ENTITY_NBT_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_ENTITY_NBT_NULL_REFERENCE:
            return "null source reference";
        case MC_ENTITY_NBT_WRONG_TAG:
            return "wrong source tag type";
        case MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS:
            return "source List index out of bounds";
        case MC_ENTITY_NBT_LIMIT:
            return "resource limit";
        case MC_ENTITY_NBT_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_ENTITY_NBT_ENTITY_FAILED:
            return "Entity ownership failed";
        case MC_ENTITY_NBT_STACK_FAILED:
            return "ItemStack operation failed";
        case MC_ENTITY_NBT_NBT_FAILED:
            return "NBT operation failed";
        case MC_ENTITY_NBT_UNSUPPORTED_FP:
            return "unsupported floating environment";
        case MC_ENTITY_NBT_UNSUPPORTED_TYPE:
            return "unsupported concrete Entity type";
        case MC_ENTITY_NBT_REGISTRY_FAILED:
            return "EntityList operation failed";
    }
    return "unknown Entity NBT status";
}
