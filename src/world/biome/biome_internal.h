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

/* Private sampling stages for an already constructed numerical manager.
 * sampling_valid checks the existing native owner/descriptor validity without
 * mutation, allocation, class use or callbacks. Descriptor/noise state keeps
 * biome.h's exclusive, live-owner and disjoint-buffer preconditions. */
bool mc_biome_manager_sampling_valid(const mc_biome_manager *manager);

/* Sample only the selected actual a/b/c channel over size*size. The same signed
 * dimension/product/span bounds as the public square sampler apply, including
 * zero size. A null/short selected array grows to the exact product; a retained
 * longer array keeps its identity/length and its WHOLE span is cleared by the
 * raw fill. Publish a replacement only after its successful fill, keeping old
 * plus new requests charged until then. Failure preserves this channel and
 * leaves earlier independently completed channels intact. No future-channel
 * allocations/preflight, cell processing, enum lookup or d write occur here.
 *
 * This staged contract differs from the public numerical square API, which
 * still preflights ALL needed buffers before any mutation and preserves all
 * outputs/state on failure. GAME callers own a/b/c sequencing and later real
 * class lookup; these helpers never turn numerical enum IDs into objects. */
mc_biome_status mc_biome_sample_channel_direct(mc_biome_manager *manager, int32_t x, int32_t z,
                                               size_t size, mc_biome_construction_channel channel);

/* Require a valid manager with live initialized channel samples and index in
 * bounds of a/b/c. Perform the original auxiliary/mix/clamps, then store a
 * before b. No allocation, failure, lookup/classification or d write. Caller
 * owns subsequent capture/lookup/output ordering and synchronous reentry. */
void mc_biome_sample_process_cell(mc_biome_manager *manager, size_t index);

#endif
