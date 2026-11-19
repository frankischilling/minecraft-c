#ifndef MC_WORLD_RUNTIME_BLOCK_RANDOM_DISPLAY_INTERNAL_H
#define MC_WORLD_RUNTIME_BLOCK_RANDOM_DISPLAY_INTERNAL_H

#include "mc/world/runtime/block_random_display.h"

/* Sampler-owned working diagnostics use the ordinary dispatcher and lifetime
 * rules. Every failure supplies the complete public record. Success initializes
 * the header and leaves the detail storage unchanged until a child is reached.
 * The sampler consumes this working record only after a failed status. */
mc_world_storage_status
mc_world_runtime_block_random_display_sample(mc_world_runtime *, const mc_block *, int32_t x,
                                             int32_t y, int32_t z, mc_java_random *,
                                             mc_world_random_display_block_error *);

#endif
