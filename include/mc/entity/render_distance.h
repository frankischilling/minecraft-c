#ifndef MC_ENTITY_RENDER_DISTANCE_H
#define MC_ENTITY_RENDER_DISTANCE_H

#include "mc/entity/entity.h"
#include "mc/entity/vector/vector.h"

typedef enum mc_entity_render_distance_status {
    MC_ENTITY_RENDER_DISTANCE_OK = 0,
    MC_ENTITY_RENDER_DISTANCE_INVALID_ARGUMENT,
    MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL,
    MC_ENTITY_RENDER_DISTANCE_UNSUPPORTED_FP,
    MC_ENTITY_RENDER_DISTANCE_UNSUPPORTED_NUMERIC,
    MC_ENTITY_RENDER_DISTANCE_UNIMPLEMENTED_DEPENDENCY,
    MC_ENTITY_RENDER_DISTANCE_ENTITY_FAILED
} mc_entity_render_distance_status;

typedef struct mc_entity_render_distance_error {
    mc_entity_render_distance_status status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    int32_t pc, vector_pc, range_pc, bounds_pc;
    bool native_policy;
    mc_entity_status entity;
} mc_entity_render_distance_error;

/* Actual virtual client SN.a(BT) and reached a(D)/EQ.c closure. Current source
 * coordinates and the SAME nullable BT/bounds are read in bytecode order. All
 * inherited canonical native kinds and BY/VV/CF/LX's range overrides use their
 * real formula; C's own missing private-b body refuses at its reached field1,
 * without inspecting BT. Unknown/foreign native type provenance is explicit.
 * RAW/failed objects remain valid receivers; no constructor/READY/class use,
 * Math use, World operation, Random draw, hold or source store occurs.
 * Strict x86-64 floating controls are checked at the first reached arithmetic;
 * the comparison preserves JVM unordered=false semantics. Output changes only
 * on OK; optional error/output must be fresh and disjoint from each other and
 * all live/exclusive owners. Keep the actual Entity/BT/bounds storage alive. */
mc_entity_render_distance_status mc_entity_render_distance(mc_entity *,
                                                           const mc_entity_vector *nullable_eye,
                                                           bool *out,
                                                           mc_entity_render_distance_error *error);

#endif
