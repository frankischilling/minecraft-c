#ifndef MC_WORLD_MATERIAL_MOVEMENT_INTERNAL_H
#define MC_WORLD_MATERIAL_MOVEMENT_INTERNAL_H
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include <emmintrin.h>

static inline float mc_material_fluid_height(int32_t metadata) {
    if (metadata >= 8)
        metadata = 0;
    const float level = (float)mc_java_i32_add(metadata, 1);
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(level), _mm_set_ss(9.0f)));
}
#endif
