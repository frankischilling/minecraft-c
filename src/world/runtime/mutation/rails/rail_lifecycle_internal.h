#ifndef MC_WORLD_RUNTIME_RAIL_LIFECYCLE_INTERNAL_H
#define MC_WORLD_RUNTIME_RAIL_LIFECYCLE_INTERNAL_H

#include "mc/world/runtime/collision/boxes.h"
#include "mc/world/runtime/power/power.h"
#include "rail_connections_internal.h"

/* These finite current/deepest sites are diagnostics, not a source stack or
 * a traversal/recursion limit. Defining classes always come from the actual
 * receiver, independently of the nullable supplied World's namespace. */
/* The two private power searches are concrete source methods.
 * search is a(World,IIIIZI)Z; verify is
 * a(World,IIIZII)Z. A direct entry is fresh, publishes out only on completion,
 * and retains the complete reached child and source prefix on failure. */
typedef struct mc_rail_power_search_error {
    mc_world_mutation_error mutation;
    mc_rail_power_search_site search, verify, cause;
    mc_world_collision_error access;
    mc_world_power_error power;
    /* Explicit selection and actual innermost private failure origin. */
    uint32_t selected, origin;
} mc_rail_power_search_error;

mc_world_storage_status mc_rail_power_search_held(mc_world_runtime *world, const mc_block *block,
                                                  int32_t x, int32_t y, int32_t z, int32_t metadata,
                                                  bool forward, int32_t depth, bool *out,
                                                  mc_rail_power_search_error *error);
mc_world_storage_status mc_rail_power_verify_held(mc_world_runtime *world, const mc_block *block,
                                                  int32_t x, int32_t y, int32_t z, bool forward,
                                                  int32_t depth, int32_t axis, bool *out,
                                                  mc_rail_power_search_error *error);

typedef struct mc_rail_lifecycle_error {
    mc_world_mutation_error mutation;
    mc_rail_lifecycle_site added, shape, neighbor, cause;
    mc_world_collision_error metadata;
    mc_world_solid_support_error support;
    mc_world_nearby_error support_access;
    mc_world_power_error power;
    mc_rail_connections_error connections;
    mc_rail_power_search_error search;
    /* Explicit selection and actual innermost private failure origin. */
    uint32_t selected, origin;
} mc_rail_lifecycle_error;

/* Complete fresh defaults in writable, owner-disjoint caller storage. Uses an
 * immutable typed template and the existing child factories; no allocation or
 * mutable shared scratch. out is required and may contain prior diagnostics. */
void mc_world_mutation_rail_error_init(mc_rail_lifecycle_error *out);
mc_rail_lifecycle_error mc_world_mutation_rail_empty_error(void);

/* Concrete inherited PC_JJ bodies, including PH_JN inheritance. Caller has
 * selected this genuine compatible receiver body, keeps Block/classes
 * live/exclusive and holds/enters a nonnull GAME World through
 * reached getters/setters. No new World entry, binding, class use, READY gate,
 * kind dispatch or coordinate preflight is added. NULL World fails only at
 * the reached B1 read. Native receiver/defining-owner admission is unentered.
 *
 * error is required, disjoint from owners/arguments, and receives fresh data
 * even if poisoned on entry. Success is neutral; false is completion failure,
 * never an ordinary source boolean. Source mutations/entropy/initializers and
 * completed helper prefixes survive failure. Temporary graph owners retire on
 * every path without rolling back source effects. Failure publication retains
 * distinct current/deepest rail sites and full lower mutation children across
 * synchronous setter reentry. */
bool mc_world_mutation_rail_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_rail_lifecycle_error *error);
bool mc_world_mutation_rail_shape(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, bool force, mc_rail_lifecycle_error *error);
bool mc_world_mutation_rail_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, int32_t source_id,
                                     mc_rail_lifecycle_error *error);

#endif
