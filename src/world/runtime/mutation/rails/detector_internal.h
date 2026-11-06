#ifndef MC_WORLD_RUNTIME_RAIL_DETECTOR_INTERNAL_H
#define MC_WORLD_RUNTIME_RAIL_DETECTOR_INTERNAL_H

#include "mc/world/runtime/collision/boxes.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/scheduling.h"

/* Retain complete consumed children privately and publish finite rail sites
 * with their full lower mutation diagnostics on failure. */
typedef struct mc_world_detector_error {
    mc_world_mutation_error mutation;
    mc_world_detector_site contact, tick, update;
    /* Deepest detector frame reached within this call; generic
     * mutation.cause_hook_* independently retains the concrete lower hook.
     * Neither projection is a recursive source stack or work limit. */
    mc_world_detector_site cause;
    /* Exact consumed children, in addition to their mutation projections. */
    mc_world_collision_error metadata;
    mc_world_schedule_error scheduled;
    /* Explicit selection and actual innermost private failure origin. */
    uint32_t selected, origin;
} mc_world_detector_error;

mc_world_detector_error mc_world_mutation_detector_empty_error(void);

/* Actual ph/jn bodies. The dispatcher supplies a nonnull actual detector
 * receiver and keeps its defining Registry/Entity classes live and exclusive.
 * No constructor/READY/class-active-use preflight is implied by invocation.
 * A nonnull World is the actual live exclusive GAME owner, with an enclosing
 * call entry held across these bodies; raw/constructing/failed prefixes are
 * allowed under that owner's native contract. No entry or persistent binding
 * is acquired here. World may be NULL until B1 (contact/tick) or query67 (f).
 * Entity and Random are ignored source arguments and may be NULL.
 *
 * error is required and disjoint from all owners/arguments. Each entry returns
 * a fresh neutral error on success; failure retains explicit caller frames,
 * complete real children and prior source effects. The bool is completion,
 * not the detector's powered state. update receives the caller's saved full
 * metadata and never reacquires it. Query List/bounds holds are released on
 * every path; source pool movement, acquisitions, writes, notifications and
 * schedule prefixes are not rolled back. There is no work or recursion cap. */
bool mc_world_mutation_detector_contact(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, mc_entity *entity,
                                        mc_world_detector_error *error);
bool mc_world_mutation_detector_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_java_random *random,
                                     mc_world_detector_error *error);
bool mc_world_mutation_detector_update(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, int32_t metadata,
                                       mc_world_detector_error *error);

#endif
