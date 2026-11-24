#ifndef MC_WORLD_BIOME_SKY_COLOR_INTERNAL_H
#define MC_WORLD_BIOME_SKY_COLOR_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

/* The unchanged JDK17 Color.HSBtoRGB arithmetic leaf; no Color allocation.
 * Native FP admission preserves out. All finite/nonfinite inputs are accepted
 * with Java f2i and wrapped RGB packing, without unit-interval validation. */
bool mc_java_color_hsb_to_rgb(float hue, float saturation, float brightness, int32_t *out);

#endif
