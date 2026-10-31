#ifndef MC_WORLD_RUNTIME_SKY_H
#define MC_WORLD_RUNTIME_SKY_H

#include "mc/world/runtime/colors.h"

/* Client fd.b(F)F with actual t captured before fresh Info time, then concrete
 * Dimension dispatch. These diagnostics preserve source-null ordering and
 * native failures independently. No MathHelper active use is introduced. */
mc_world_storage_status mc_world_runtime_sky_angle(mc_world_runtime *, float partial_tick,
                                                   float *out, mc_world_color_error *);

/* Actual fd.g(F)F, reusing the canonical weather interpolation on live i/j/i.
 * No weather evolution, time read, MathHelper use or World snapshot. */
mc_world_storage_status mc_world_runtime_sky_rain_strength(mc_world_runtime *, float partial_tick,
                                                           float *out, mc_world_color_error *);

/* Actual fd.e(F)F: World angle first, source MathHelper cosine active use13,
 * comparison clamps, then separately rounded square and half at return45.
 * Use the SAME World-bound Math namespace. Output is unchanged on failure;
 * reached class effects and exact nested diagnostics remain. All three calls
 * admit live client GAME prefixes, retain their call dependencies, and require
 * exclusive actual owners and disjoint outputs/errors. */
mc_world_storage_status mc_world_runtime_star_brightness(mc_world_runtime *, float partial_tick,
                                                         mc_math_classes *, float *out,
                                                         mc_world_color_error *);

#endif
