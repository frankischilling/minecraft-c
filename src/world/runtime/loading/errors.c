#include "mc/world/runtime/loading/entity_errors.h"

mc_world_entity_load_error mc_world_entity_load_empty_error(void)
{
    return (mc_world_entity_load_error){.index = SIZE_MAX,
                                        .factory = mc_entity_nbt_empty_error(),
                                        .provenance = mc_entity_nbt_empty_error()};
}
