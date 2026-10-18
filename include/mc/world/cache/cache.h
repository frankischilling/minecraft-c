#ifndef MC_WORLD_CACHE_CACHE_H
#define MC_WORLD_CACHE_CACHE_H

#include "mc/world/chunk/state.h"

typedef struct mc_chunk_cache_entry {
    int32_t key;
    mc_chunk_state *chunk;
} mc_chunk_cache_entry;

/* Caller-owned unique packed-key entries and a separate insertion list.
 * Preserve object lifetimes; arrays, cache and chunk objects must be disjoint.
 * The insertion list can contain repeated identities and overwritten values.
 * Directly restored baselines must pass validate before use. NULL map values
 * are unsupported: original lookups can then load/generate and mutate state.
 * This storage interface does not load or run lifecycle callbacks; acquire.h
 * provides the separate synchronous nonnull acquisition coordinator.
 */
typedef struct mc_chunk_cache {
    mc_chunk_cache_entry *entries;
    size_t capacity, count;
    mc_chunk_state **insertions;
    size_t insertion_capacity, insertion_count;
} mc_chunk_cache;

int32_t mc_chunk_cache_key(int32_t x, int32_t z);
bool mc_chunk_cache_init(mc_chunk_cache *cache, mc_chunk_cache_entry *entries, size_t capacity,
                         mc_chunk_state **insertions, size_t insertion_capacity);
bool mc_chunk_cache_validate(const mc_chunk_cache *cache);

/* Requires validated live entries. Returns presence separately from value;
 * missing entries leave *chunk unchanged. Neither function loads or generates.
 */
bool mc_chunk_cache_lookup(const mc_chunk_cache *cache, int32_t x, int32_t z,
                           mc_chunk_state **chunk);
bool mc_chunk_cache_contains(const mc_chunk_cache *cache, int32_t x, int32_t z);

/* Explicit post-generation publication: replace/insert the packed key, then
 * append the same object to the ordered insertion list. Do not publish before
 * finishing a new chunk. Replacement retains the map entry's position and the
 * old object in the insertion list. Aliased coordinates use the same key.
 * Invalid inputs or insufficient storage leave every byte/count unchanged.
 * No pending-set, loader, generation, lifecycle or population calls occur.
 */
bool mc_chunk_cache_publish(mc_chunk_cache *cache, int32_t x, int32_t z, mc_chunk_state *chunk);

#endif
