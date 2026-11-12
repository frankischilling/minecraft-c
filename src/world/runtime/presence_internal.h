#ifndef MC_WORLD_RUNTIME_PRESENCE_INTERNAL_H
#define MC_WORLD_RUNTIME_PRESENCE_INTERNAL_H

#include "mc/world/runtime/runtime.h"

typedef struct mc_world_presence_error {
    unsigned int bytecode, provider_bytecode;
    mc_world_storage_error cause;
} mc_world_presence_error;

/* Actual fd/dj.a(IIIIII)Z. Caller holds the live GAME World across this body.
 * No acquisition, Registry use or coordinate-domain preflight occurs here.
 * Every iteration reloads current provider v through its presence operation.
 * Ordinary false is separate from native failure; out is unchanged on failure.
 * Success clears the completed frame, including the ordinary false branches. */
mc_world_storage_status mc_world_runtime_region_present_held(mc_world_runtime *world, int32_t min_x,
                                                             int32_t min_y, int32_t min_z,
                                                             int32_t max_x, int32_t max_y,
                                                             int32_t max_z, bool *out,
                                                             mc_world_presence_error *error);

#endif
