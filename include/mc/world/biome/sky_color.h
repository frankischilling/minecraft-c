#ifndef MC_WORLD_BIOME_SKY_COLOR_H
#define MC_WORLD_BIOME_SKY_COLOR_H

#include "mc/world/biome/registry.h"

typedef struct mc_biome_sky_color_error {
    mc_registry_status status;
    const char *operation;
    unsigned int bytecode, color_bytecode;
    bool native_policy, color_allocated, color_constructed, source_returned;
} mc_biome_sky_color_error;

/* Actual client kd.a(F)I on this canonical Biome. Divide by3, source comparison
 * clamps (NaN passes both), binary32 HSB arguments, translated JDK17 conversion,
 * transient Color(int) value construction/getRGB, return44. Actual ry.a(F)I
 * instead returns12632319 at2 without FP or Color allocation. No instance
 * field/class initializer is read or run by either source body.
 * The temporary Color allocation precedes HSB conversion, as in getHSBColor.
 * Its identity never escapes. No substitute Biome or sampled temperature is
 * constructed. Requires exclusive live Registry/Biome and disjoint outputs;
 * strict x86-64 FP controls. Ordinary out is unchanged on any failure. */
mc_registry_status mc_world_biome_sky_color(const mc_biome *, float temperature, int32_t *out,
                                            mc_biome_sky_color_error *);

#endif
