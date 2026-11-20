#ifndef MC_WORLD_TORCH_SUPPORT_INTERNAL_H
#define MC_WORLD_TORCH_SUPPORT_INTERNAL_H

#include "mc/block/registry/runtime.h"

typedef struct mc_world_torch_fence_error {
    mc_registry_error registry;
    unsigned int pc;
} mc_world_torch_fence_error;

/* VM.h's reached live UU.ba load and bn comparison. The caller has already
 * completed World.h(false) and the separate fresh World ID query. The code
 * Registry is independent of that World; neither namespace is substituted. */
mc_registry_status mc_world_torch_floor_fence(mc_registry_classes *, int32_t fresh_id, bool *out,
                                              mc_world_torch_fence_error *);

#endif
