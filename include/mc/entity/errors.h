#ifndef MC_ENTITY_ERRORS_H
#define MC_ENTITY_ERRORS_H

#include "mc/core/java/random_profile.h"
#include "mc/item/stack/stack.h"
#include "mc/world/storage/status.h"

#include <stdint.h>

typedef enum mc_entity_status {
    MC_ENTITY_OK = 0,
    MC_ENTITY_INVALID_ARGUMENT,
    MC_ENTITY_INVALID_PHASE,
    MC_ENTITY_LIMIT,
    MC_ENTITY_ALLOCATION_FAILED,
    MC_ENTITY_PROFILE_FAILED,
    MC_ENTITY_WORLD_FAILED,
    MC_ENTITY_ITEM_FAILED,
    MC_ENTITY_CLASS_FAILED_STATUS,
    MC_ENTITY_BUSY,
    MC_ENTITY_STATE_ERROR,
    MC_ENTITY_UNSUPPORTED_FP
} mc_entity_status;

typedef struct mc_entity_error {
    mc_entity_status status;
    const char *operation; /* Static diagnostic naming the reached constructor. */
    uint16_t bytecode;
    mc_java_random_profile_status profile;
    mc_item_stack_status item;
    mc_world_storage_status world;
} mc_entity_error;

#endif
