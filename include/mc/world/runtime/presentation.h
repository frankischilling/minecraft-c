#ifndef MC_WORLD_RUNTIME_PRESENTATION_H
#define MC_WORLD_RUNTIME_PRESENTATION_H

#include "mc/world/runtime/mutations.h"

/* Actual fd/dj sound and particle entries. Each reads the current observer
 * list and dispatches in its source order. A genuinely empty list returns
 * normally; registered typed observers receive each reached synchronous call.
 * Iteration rereads the live list by index after each callback. Untyped nonnull
 * entries fail at cast; null receivers and missing callbacks fail at invocation.
 * These operations perform no Registry/String use, Random draw, coordinate
 * conversion, clipping, flag gate or implicit later work. The caller supplies
 * the actual already-evaluated arguments, including a nullable String.
 *
 * Nonnull name borrows its actual Registry-owned UTF-16 identity. Its owner
 * stays live/exclusive through the call; no retain, copy or interning occurs.
 * Hold the exact GAME World across synchronous calls. Raw/constructing/failed
 * prefixes are admitted; retired/composition owners reject. Require disjoint
 * error/owner/dependency storage. Native admission preserves earlier World
 * diagnostics; reached failure retains all earlier caller/source effects.
 * error may be NULL. These entries do not implement an audio/rendering engine. */
mc_world_storage_status mc_world_runtime_sound(mc_world_runtime *world, double x, double y,
                                               double z, const mc_registry_string *name,
                                               float volume, float pitch,
                                               mc_world_mutation_error *error);
mc_world_storage_status mc_world_runtime_particle(mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, double velocity_x,
                                                  double velocity_y, double velocity_z,
                                                  mc_world_mutation_error *error);

#endif
