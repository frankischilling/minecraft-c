#ifndef MC_WORLD_MATERIAL_MOVEMENT_INTERNAL_H
#define MC_WORLD_MATERIAL_MOVEMENT_INTERNAL_H
#include "../../core/math/execution_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/material_movement.h"
#include <emmintrin.h>

/* Same run/scan, with actual reached IN active use in the caller-owned private
 * context. NULL execution is reserved for unchanged legacy callers. No new
 * public diagnostic member, owner inference or successful missing dependency. */
mc_world_storage_status mc_world_runtime_material_accelerate_in_execution(
    mc_world_runtime *, const mc_entity_bounds *, mc_material *, mc_entity *,
    const mc_world_material_movement_limits *, bool *, mc_world_material_movement_error *,
    mc_math_execution *);
mc_world_storage_status mc_world_runtime_contains_material_in_execution(
    mc_world_runtime *, const mc_entity_bounds *, mc_material *,
    const mc_world_material_movement_limits *, bool *, mc_world_material_movement_error *,
    mc_math_execution *);

static inline float mc_material_fluid_height(int32_t metadata) {
    if (metadata >= 8)
        metadata = 0;
    const float level = (float)mc_java_i32_add(metadata, 1);
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(level), _mm_set_ss(9.0f)));
}
#endif
