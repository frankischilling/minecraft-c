#ifndef MC_WORLD_RUNTIME_ENVIRONMENT_H
#define MC_WORLD_RUNTIME_ENVIRONMENT_H

#include "mc/world/runtime/construction.h"

/* Read the actual GAME World fields, including reached constructor prefixes.
 * A retired owner rejects. Inputs/outputs must be disjoint from live owner
 * storage, with exclusive access and the strict x86-64 floating environment.
 * These queries preserve out and runtime-owned diagnostics on failure, allocate nothing and
 * sample no clock. Interpolation accepts arbitrary float partial ticks.
 * Thunder includes a fresh rain interpolation after its own interpolation.
 * This exposes no tick evolution or constructor-time weather override. */
mc_world_storage_status mc_world_runtime_rain_strength(const mc_world_runtime *runtime,
                                                       float partial_tick, float *out,
                                                       mc_world_storage_error *error);
mc_world_storage_status mc_world_runtime_thunder_strength(const mc_world_runtime *runtime,
                                                          float partial_tick, float *out,
                                                          mc_world_storage_error *error);

/* Capture actual WorldProvider t, then read current Info time before dispatching
 * its actual celestial-angle method. The Overworld WorldProvider currently
 * accepts finite ticks; constant WorldProviders ignore their tick argument.
 * No detached time snapshot, wall-clock I, seed or caller-supplied angle is substituted. */
mc_world_storage_status mc_world_runtime_celestial_angle(const mc_world_runtime *runtime,
                                                         float partial_tick, float *out,
                                                         mc_world_storage_error *error);

/* Compute the actual a(float) int using the library float cosine lookup,
 * rain, then thunder (which reads rain again). Preserve both double attenuation
 * stages and the intervening float narrowing. This does not assign World f;
 * constructor initialization performs that conditional store separately. */
mc_world_storage_status mc_world_runtime_sky_subtraction(const mc_world_runtime *runtime,
                                                         float partial_tick, int32_t *out,
                                                         mc_world_storage_error *error);

#endif
