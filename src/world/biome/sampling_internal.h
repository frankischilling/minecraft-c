#ifndef MC_WORLD_BIOME_SAMPLING_INTERNAL_H
#define MC_WORLD_BIOME_SAMPLING_INTERNAL_H

#include "mc/world/biome/sampling.h"

/* Actual fixed hd/et inherited arrays and declaring fields, embedded once in
 * its concrete retained Dimension manager. numerical_d is projection-only;
 * actual source d belongs to that manager's shared reference-array field. */
typedef struct mc_biome_fixed_storage {
    mc_biome_memory memory;
    mc_biome_double_buffer a, b, c;
    mc_biome_buffer numerical_d;
    const mc_biome *e;
    double f, g;
} mc_biome_fixed_storage;

/* Private real-array operations. Require an already entered class owner and
 * actual same-owner input; allocator links the arena/provenance once without
 * Biome active use. No guessed opaque layout may be supplied by callers. */
mc_registry_status mc_registry_biome_array_prepare(mc_registry_classes *classes,
                                                   mc_biome_reference_array *input, size_t length,
                                                   mc_biome_reference_array **out);
mc_registry_status mc_registry_biome_array_fill(mc_registry_classes *classes,
                                                mc_biome_reference_array *array, size_t length,
                                                const mc_biome *value);

/* Concrete fixed direct body over its actual embedded storage. Allocates
 * output19; if and ONLY if a is null/short, allocate/publish a49/51 then fresh
 * b60/62 separately. Fill output with e76, b with g93, then a with f110.
 * Retain actual partial publications and earlier fills on later failure;
 * do not repair b merely because a is adequate. Direct leaves c/d untouched.
 * Source x/z are unused. Zero/rectangular extents keep the checked native
 * domain. The owner/fields/arrays and dependencies must remain live/exclusive.
 * This ordinary call owns its whole registry entry, without Biome active use.
 * Diagnostic cells_completed is zero before output fill and length after it;
 * next_index is always zero because fixed has no noise_backed307 loop. */
mc_registry_status mc_registry_biome_fixed_sample(mc_registry_classes *classes,
                                                  mc_biome_fixed_storage *storage, int32_t x,
                                                  int32_t z, size_t width, size_t height,
                                                  mc_biome_reference_array *input,
                                                  mc_biome_reference_array **out,
                                                  mc_biome_sampling_error *error);

#endif
