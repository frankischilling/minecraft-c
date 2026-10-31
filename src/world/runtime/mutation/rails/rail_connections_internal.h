#ifndef MC_WORLD_RUNTIME_RAIL_CONNECTIONS_INTERNAL_H
#define MC_WORLD_RUNTIME_RAIL_CONNECTIONS_INTERNAL_H

#include "mc/world/runtime/mutations.h"

/* The helper owns its private connectivity graph. A site is a finite
 * current/deepest diagnostic, not a source call stack or recursion/work limit. */
typedef struct mc_rail_connections_error {
    mc_world_mutation_error mutation;
    mc_rail_connection_site connection, cause_connection;
    mc_rail_connection_site predicate, accessor;
    /* Explicit selection and actual innermost private failure origin. */
    uint32_t selected, origin;
} mc_rail_connections_error;

typedef struct mc_rail_connections mc_rail_connections;

/* Caller keeps the actual defining Registry, nullable outer rail and a nonnull
 * supplied World live/exclusive; it holds/enters that World through source
 * getters and notifying setters. classes is the actual code owner, independent
 * of source outer and World arguments. outer may be NULL: vo/nq retains it but
 * never dereferences it. World may be NULL until an actual getter is reached.
 * No eager World entry/READY, coordinate, rail-kind or whole-table preflight.
 *
 * Each entry constructs fresh diagnostics. Ordinary outputs are published only
 * on completion. Source effects and earlier private-helper effects survive a
 * failure; no rollback occurs. Outputs/errors/owners are disjoint. create owns
 * a fresh helper and List; destroy retires its private native ownership only.
 * Every successfully created helper must be destroyed before the caller drops
 * its World/Registry lifetime. No persistent binding or callback is introduced.
 * Native allocations are charged to the supplied World's existing direct
 * allocation budget, with no source graph-size or recursion bound. */
mc_world_storage_status mc_rail_connections_create(mc_registry_classes *classes,
                                                   const mc_block *outer, mc_world_runtime *world,
                                                   int32_t x, int32_t y, int32_t z,
                                                   mc_rail_connections **out,
                                                   mc_rail_connections_error *error);
void mc_rail_connections_destroy(mc_rail_connections *connections);
mc_world_storage_status mc_rail_connections_reshape(mc_rail_connections *connections, bool powered,
                                                    bool force, mc_rail_connections_error *error);
mc_world_storage_status mc_rail_connections_degree(mc_rail_connections *connections, int32_t *out,
                                                   mc_rail_connections_error *error);

/* Actual PC_JJ static predicates; the caller supplies the defining owner.
 * Declaring-class active use precedes each body. Both compare live aH/U/V bn
 * fields in source order; presence first performs the actual supplied World
 * ID getter. These are not native kind/constant-ID predicates. */
mc_world_storage_status mc_rail_present_held(mc_registry_classes *classes, mc_world_runtime *world,
                                             int32_t x, int32_t y, int32_t z, bool *out,
                                             mc_rail_connections_error *error);
mc_world_storage_status mc_rail_is_id_held(mc_registry_classes *classes, int32_t id, bool *out,
                                           mc_rail_connections_error *error);

#endif
