#ifndef MC_WORLD_RUNTIME_BRIGHTNESS_H
#define MC_WORLD_RUNTIME_BRIGHTNESS_H

#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/queries.h"

typedef enum mc_world_brightness_exception {
    MC_WORLD_BRIGHTNESS_SOURCE_NONE,
    MC_WORLD_BRIGHTNESS_SOURCE_NULL_DIMENSION,
    MC_WORLD_BRIGHTNESS_SOURCE_NULL_TABLE,
    MC_WORLD_BRIGHTNESS_SOURCE_INDEX
} mc_world_brightness_exception;

typedef struct mc_world_brightness_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode; /* fd.c(III)F; normal return15, child invocation11. */
    bool native_policy, query_reached;
    int32_t light;
    mc_world_brightness_exception source_exception;
    mc_world_provider_status provider;
    mc_world_storage_error cause;
    mc_world_query_error query;
} mc_world_brightness_error;

/* Actual client fd.c(III)F: capture actual World.t at1 and its f array at4,
 * invoke actual fd.n(III)I at11, index the captured array at14, return15.
 * No dimension lookup after the query, arithmetic, clamping or FP gate occurs.
 * The captured provider and GAME World have native holds throughout the query,
 * including synchronous Registry/chunk reentry and replacement of World.t.
 * The same constructor-owned float[16] remains the array identity. NULL t fails
 * at4 before the query; NULL f fails at14 only after the query returns. Bounds
 * failure at14 preserves prior query effects without an unsafe native access.
 * RAW/constructing/failed client GAME Worlds are admitted; retirement,
 * COMPOSITION and the unqualified server namespace are native admission errors.
 * Failure preserves out; complete child diagnostics and reached light survive.
 * Success reports return15. No source field or prior World error is cleared.
 * Keep the whole graph live/exclusive and out/error disjoint from owners. */
mc_world_storage_status mc_world_runtime_brightness(mc_world_runtime *world, int32_t x, int32_t y,
                                                    int32_t z, float *out,
                                                    mc_world_brightness_error *error);

#endif
