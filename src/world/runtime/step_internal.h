#ifndef MC_WORLD_RUNTIME_STEP_INTERNAL_H
#define MC_WORLD_RUNTIME_STEP_INTERNAL_H

#include "mc/world/runtime/step.h"

/* Exact shared entered fd.g body. The caller holds the live GAME World. */
bool mc_world_step_opaque_held(mc_world_runtime *, int32_t x, int32_t y, int32_t z, bool *out,
                               mc_world_step_error *, mc_world_step_source_failure *);
/* Exact private bs.i body, including its one captured World.r and lazy native
 * World entry. This does not perform the separate ore-activation method. */
bool mc_world_step_redstone_particles(mc_world_runtime *, const mc_block *, int32_t x, int32_t y,
                                      int32_t z, mc_world_step_error *,
                                      mc_world_step_source_failure *);

/* Exact private BS.h body, shared by walking and the clicked/activation
 * wrappers. The caller holds the World, evaluated Block and defining Registry
 * graphs across both children. Its outer bytecode remains caller-owned;
 * activation_bytecode records H's reached operation. The returned Source
 * witness is copied into error->source even when source is NULL. Each failed
 * child retains its actual diagnostic, including the WorldAccess failure.
 * No private lit flag, scheduling, phase gate or retry is added. */
bool mc_world_step_redstone_activate(mc_world_runtime *, const mc_block *, int32_t x, int32_t y,
                                     int32_t z, mc_world_step_error *,
                                     mc_world_step_source_failure *);

#endif
