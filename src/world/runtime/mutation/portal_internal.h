#ifndef MC_WORLD_RUNTIME_MUTATION_PORTAL_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_PORTAL_INTERNAL_H

#include "mc/world/runtime/mutations.h"

/* Actual client AK creation and neighbor bodies. The enclosing caller keeps
 * the live World, concrete Portal and defining Registry graph exclusive.
 * Creation's ordinary bool is separate from continuation; failure preserves
 * it and all reached writes, callbacks and World.o stores. Each live static
 * and World read is performed at its original occurrence. No frame-size
 * approximation, saved flag restoration or missing-context fallback occurs. */
bool mc_world_mutation_portal_create(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                     int32_t, bool *, mc_world_mutation_error *);
bool mc_world_mutation_portal_neighbor(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                       int32_t, int32_t, mc_world_mutation_error *);

#endif
