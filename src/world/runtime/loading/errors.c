#include "mc/world/runtime/loading/entity_errors.h"

void mc_world_entity_load_error_init(mc_world_entity_load_error *error) {
    static const mc_world_entity_load_error zero = {0};
    if (error == NULL)
        return;
    *error = zero;
    error->index = SIZE_MAX;
    mc_entity_nbt_error_init(&error->factory);
    mc_entity_nbt_error_init(&error->provenance);
}

mc_world_entity_load_error mc_world_entity_load_empty_error(void) {
    mc_world_entity_load_error error;
    mc_world_entity_load_error_init(&error);
    return error;
}
