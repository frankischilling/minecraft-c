#ifndef MC_WORLD_DIMENSION_SKY_H
#define MC_WORLD_DIMENSION_SKY_H

#include "mc/core/math/classes.h"
#include "mc/world/dimension/world_provider.h"

typedef struct mc_world_provider_sky_error {
    mc_world_provider_status status;
    const char *operation, *source_class;
    unsigned int bytecode, stores;
    bool native_policy, source_returned;
    mc_world_provider_error cause;
    mc_math_class_error math;
} mc_world_provider_sky_error;

/* Actual virtual xa.a(FF)[F. Overworld/Nether evaluate the actual MathHelper
 * namespace and conditionally fill/return the SAME constructor-owned h[4].
 * AY returns NULL without reading h, Math or FP state. The second argument is
 * unused by both original bodies. No World, replacement array or color cache
 * is consulted. Allocated prefixes are supported, including source-null h at
 * the first reached fastore92. Out is unchanged on failure; reached Math and
 * h stores remain. Success returns a borrowed array or ordinary NULL. Keep
 * THIS Dimension held until the last use of a returned array, including
 * across rebinds; a subsequent call may overwrite its elements. Calls require
 * exclusive live owners, the actual shared Math namespace, disjoint outputs
 * and strict FP controls only where arithmetic is reached. */
mc_world_provider_status mc_world_provider_sunrise_color(mc_world_provider *, float angle,
                                                         float partial_tick, mc_math_classes *,
                                                         const float **out,
                                                         mc_world_provider_sky_error *);

/* Actual xa.c()Z, inherited by RH/WD; AY.c()Z returns false. No field, Math,
 * floating arithmetic or construction-phase test is part of either body. */
mc_world_provider_status mc_world_provider_horizon_tint(mc_world_provider *, bool *out,
                                                        mc_world_provider_sky_error *);

#endif
