#include "torch_support_internal.h"

#include "../../block/registry/registry_internal.h"

mc_registry_status mc_world_torch_floor_fence(mc_registry_classes *classes, int32_t id, bool *out,
                                              mc_world_torch_fence_error *error) {
    *error = (mc_world_torch_fence_error){.registry = {.initializer_pc = -1,
                                                       .block_id = -1,
                                                       .constructor_pc = -1,
                                                       .active_initializer_pc = -1,
                                                       .item_argument = -1},
                                          .pc = 19};
    const mc_block *fence = NULL;
    if (mc_registry_block_static_get(classes, "ba", &fence, &error->registry) != MC_REGISTRY_OK)
        return error->registry.status;
    error->pc = 22;
    if (fence == NULL) {
        error->registry.status = MC_REGISTRY_NULL_REFERENCE;
        error->registry.message = "Torch floor live Fence field receiver";
        return error->registry.status;
    }
    *out = id == fence->fields.bn;
    error->pc = 33;
    return MC_REGISTRY_OK;
}
