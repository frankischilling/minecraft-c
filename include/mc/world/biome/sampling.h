#ifndef MC_WORLD_BIOME_SAMPLING_H
#define MC_WORLD_BIOME_SAMPLING_H

#include "mc/world/biome/biome.h"
#include "mc/world/biome/registry.h"

typedef struct mc_biome_reference_array mc_biome_reference_array;

typedef struct mc_biome_sampling_error {
    mc_registry_error registry;
    mc_biome_status climate_status;
    /* Outer xv/ph direct sampler operation, separate from nested clinit PCs.
     * -1 is native admission; 331 is successful return. */
    int32_t sampler_pc;
    /* Reached local array, including an unpublished replacement on failure.
     * A diagnostic reference is not a successfully returned Java array. */
    const mc_biome_reference_array *array;
    size_t cells_completed;
    size_t next_index; /* iinc at 307 precedes the active use at 314. */
} mc_biome_sampling_error;

/* Direct square xv/ph sampler using actual shared Biome references. The
 * verified square domain requires size and size*size to fit INT32_MAX and the
 * native spans. A null/short input allocates an exact-sized reference array
 * FIRST, without initializing Biome. Larger inputs retain identity, length and
 * the unvisited suffix. Arrays are arena-held until registry destruction;
 * input must be NULL or an actual array previously obtained from this owner.
 *
 * Actual temperature, rainfall and auxiliary channel fills occur in order,
 * each retaining its completed effects. Each cell then stores a and b before
 * invoking the real Biome double lookup and storing its returned reference.
 * A later failure retains the earlier channels, processed cell and real class,
 * resource and profile effects. The result output itself is unchanged on
 * failure; error exposes the reached local prefix for diagnostics. Zero size
 * materializes arrays but performs no Biome active use. Numerical manager.d
 * is untouched. This is not the all-or-nothing numerical square API.
 *
 * Keep registry/profile/material/resource, manager and its memory owner live
 * through every operation/view. Exclusive access and disjoint output/error/
 * owner storage are required, including across synchronous resource callbacks.
 * Keep the captured x86-64 strict FP environment throughout: nearest-even,
 * gradual underflow with FTZ/DAZ off, and masked traps. Noise/cell arithmetic
 * precedes the later Biome lookup's reached FP check.
 * Ordinary registry reentry and destruction are BUSY for the entire call.
 * The native registry arena bound is not Java heap/GC or OOM timing. */
mc_registry_status mc_biome_sample_references(mc_registry_classes *classes,
                                              mc_biome_manager *manager, int32_t x, int32_t z,
                                              size_t size, mc_biome_reference_array *input,
                                              mc_biome_reference_array **out,
                                              mc_biome_sampling_error *error);

/* Borrowed readonly diagnostics; NULL array has length zero and NULL data.
 * An allocated empty array has a distinct nonnull data identity. A NULL cell
 * is an actual unfilled/null reference, never an enum projection. */
size_t mc_biome_reference_array_length(const mc_biome_reference_array *array);
const mc_biome *const *mc_biome_reference_array_data(const mc_biome_reference_array *array);

#endif
