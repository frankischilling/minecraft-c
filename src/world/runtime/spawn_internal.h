#ifndef MC_WORLD_RUNTIME_SPAWN_INTERNAL_H
#define MC_WORLD_RUNTIME_SPAWN_INTERNAL_H
#include "mc/world/runtime/construction.h"

typedef struct mc_world_spawn_error {
    mc_world_storage_status world;
    mc_world_info_status info;
    unsigned int pc;
    bool fields_failed, info_was_null, info_getter_entered;
} mc_world_spawn_error;

/* Shared arithmetic-free fd.u argument tail only. Caller already owns the
 * actual pending new br0. x is reloaded for each of ei.c/d/e at8/15/22;
 * br's stores remain after all three argument evaluations. */
bool mc_world_source_spawn_values(mc_world_runtime *, int32_t values[3], mc_world_spawn_error *);
#endif
