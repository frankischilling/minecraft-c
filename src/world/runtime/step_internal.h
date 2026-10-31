#ifndef MC_WORLD_RUNTIME_STEP_INTERNAL_H
#define MC_WORLD_RUNTIME_STEP_INTERNAL_H

#include "mc/world/runtime/step.h"

typedef enum mc_world_step_source_failure_kind {
    MC_WORLD_STEP_SOURCE_NONE,
    MC_WORLD_STEP_SOURCE_NULL_WORLD,
    MC_WORLD_STEP_SOURCE_NULL_RANDOM,
    MC_WORLD_STEP_SOURCE_REGISTRY,
    MC_WORLD_STEP_SOURCE_QUERY,
    MC_WORLD_STEP_SOURCE_PRESENTATION
} mc_world_step_source_failure_kind;

typedef struct mc_world_step_source_failure {
    mc_world_step_source_failure_kind kind;
    mc_registry_status registry_status;
} mc_world_step_source_failure;

/* Exact shared entered fd.g body. The caller holds the live GAME World. */
bool mc_world_step_opaque_held(mc_world_runtime *, int32_t x, int32_t y, int32_t z, bool *out,
                               mc_world_step_error *, mc_world_step_source_failure *);
/* Exact private bs.i body, including its one captured World.r and lazy native
 * World entry. This does not perform the separate ore-activation method. */
bool mc_world_step_redstone_particles(mc_world_runtime *, const mc_block *, int32_t x, int32_t y,
                                      int32_t z, mc_world_step_error *,
                                      mc_world_step_source_failure *);

#endif
