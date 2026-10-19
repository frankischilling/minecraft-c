#ifndef MC_WORLD_BIOME_INTERNAL_H
#define MC_WORLD_BIOME_INTERNAL_H

#include "mc/world/biome/biome.h"

typedef enum mc_biome_construction_channel {
    MC_BIOME_CONSTRUCT_TEMPERATURE,
    MC_BIOME_CONSTRUCT_RAINFALL,
    MC_BIOME_CONSTRUCT_AUXILIARY
} mc_biome_construction_channel;

/* Private stages for an unpublished, zero-initialized manager. Construct each
 * channel once, in the declared order, then finish once with a live memory
 * owner. These functions do not obtain a World seed or invoke callbacks.
 * Until finish succeeds, memory remains NULL and initialized remains zero.
 * The caller owns admission, partial cleanup and final publication. */
mc_biome_status mc_biome_manager_construct_channel(mc_biome_manager *manager, int64_t seed,
                                                   mc_biome_construction_channel channel);
mc_biome_status mc_biome_manager_finish_construction(mc_biome_manager *manager,
                                                     mc_biome_memory *memory);

#endif
