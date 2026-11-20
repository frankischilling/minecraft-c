#ifndef MC_WORLD_CACHE_KEY_INTERNAL_H
#define MC_WORLD_CACHE_KEY_INTERNAL_H

#include <stdint.h>

static inline uint32_t mc_chunk_cache_hash_key(int32_t key) {
    uint32_t hash = (uint32_t)key;
    hash ^= hash >> 16u;
    hash *= UINT32_C(0x7feb352d);
    hash ^= hash >> 15u;
    hash *= UINT32_C(0x846ca68b);
    return hash ^ (hash >> 16u);
}

#endif
