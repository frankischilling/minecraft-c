#ifndef MC_WORLD_LIGHTING_WORLD_H
#define MC_WORLD_LIGHTING_WORLD_H

#include "mc/world/cache/cache.h"
#include "mc/world/lighting/queue.h"

/* Caller-owned world context. enqueue_depth belongs to the runtime shared by
 * worlds, mirroring the original class-static counter. Own all mutable state
 * exclusively during calls. Cache and queue storage must be valid and disjoint.
 */
typedef struct mc_world_lighting {
    const mc_chunk_cache *cache;
    mc_light_queue *queue;
    int32_t *enqueue_depth;
    bool no_sky;
} mc_world_lighting;

typedef enum mc_light_submit_result {
    MC_LIGHT_SUBMIT_INVALID = 0,
    MC_LIGHT_SUBMIT_SKIPPED,
    MC_LIGHT_SUBMIT_MERGED,
    MC_LIGHT_SUBMIT_APPENDED,
    MC_LIGHT_SUBMIT_CLEARED,
    MC_LIGHT_SUBMIT_FULL
} mc_light_submit_result;

/* Full imported-state validation, including unique nonnull cache entries and
 * queue kinds. Restoring raw storage requires this before subsequent calls.
 */
bool mc_world_lighting_validate(const mc_world_lighting *world);

/* Original world height: asymmetric world border, signed chunk division and
 * unsigned height byte. Missing/blank chunks produce zero without loading.
 * A present NULL/invalid chunk returns false, leaving height unchanged.
 */
bool mc_world_height(const mc_chunk_cache *cache, int32_t x, int32_t z, int32_t *height);

/* Requires valid imported/live state. Implements no-sky, shared depth ==50,
 * Java-wrapped midpoint sums/truncating division, presence and blank gates,
 * then the ordered queue operation. Presence has no world X/Z border gate.
 * Restores shared depth on every result; no world or cache data is mutated.
 * FULL reports resource failure without dropping the request or changing the
 * queue. CLEARED requires the original diagnostic from the caller.
 */
mc_light_submit_result mc_world_light_submit(mc_world_lighting *world,
                                             const mc_light_region *region, bool coalesce);

#endif
