#ifndef MC_WORLD_RUNTIME_POWER_POWER_H
#define MC_WORLD_RUNTIME_POWER_POWER_H

#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/collision/entities.h"

typedef struct mc_world_power_site {
    bool entered;
    unsigned int bytecode;
    int32_t x, y, z, side;
} mc_world_power_site;

typedef struct mc_world_block_power_site {
    bool entered;
    const char *declaring_class, *method;
    unsigned int bytecode;
} mc_world_block_power_site;

typedef struct mc_world_power_error {
    mc_world_storage_status status;
    const char *operation;
    mc_world_storage_error cause;
    const char *world_class;
    mc_world_power_site weak_neighbors, weak_at, strong_neighbors, strong_at;
    mc_world_block_power_site capability, strong, weak, wire_connectivity;
    mc_world_solid_support_error support;
    mc_world_nearby_error access;
    mc_registry_error registry;
} mc_world_power_error;

/* Actual fd.s/dj.r(III)Z. Ask below, above, Z-, Z+, X-, X+ in source order
 * with side arguments0..5 and Java wrapping coordinates, returning on the first
 * true result. Each directional weak query first executes actual solid support;
 * a supported block asks its six strong neighbors, otherwise it acquires its
 * current ID, then captures live Block.m and invokes the selected weak output.
 * Strong queries likewise acquire ID before Block.m and concrete dispatch.
 * No ID mask, future-neighbor validation, power-table shortcut or work cap is
 * added. Reached concrete dependencies fail explicitly, preserving effects.
 *
 * Keep live GAME owners exclusive; raw/constructing/failed prefixes are admitted
 * and retirement rejects. The World call holds its actual class/Registry binding
 * across synchronous initialization/acquisition. This operation owns no result
 * lease, persistent binding, bounds, List, Entity or entropy stream. Initialization
 * and acquisition may still change their real owners and consume entropy.
 *
 * out is required; error may be NULL. Arguments/results/errors and owners must
 * be disjoint. Failure preserves out and all completed source effects, retaining
 * distinct reached World/Block frames and full actual reader/support descendants.
 * entered distinguishes PC0 from an unentered/null receiver body. Neighborhood
 * side is diagnostic -1; directional side is the actual source argument. These
 * fixed sites are a diagnostic projection, not a recursive source stack.
 * Native admission changes only its error; success returns a neutral error and
 * preserves earlier World diagnostics. No floating-environment gate is needed. */
mc_world_storage_status mc_world_runtime_is_powered(mc_world_runtime *world, int32_t x, int32_t y,
                                                    int32_t z, bool *out,
                                                    mc_world_power_error *error);

#endif
