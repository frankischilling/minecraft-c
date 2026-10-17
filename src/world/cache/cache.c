#include "mc/world/cache/cache.h"

#include "mc/core/java/java.h"

static bool metadata_valid(const mc_chunk_cache *cache)
{
    return cache != NULL && cache->count <= cache->capacity &&
           cache->insertion_count <= cache->insertion_capacity &&
           (cache->capacity == 0 || cache->entries != NULL) &&
           (cache->insertion_capacity == 0 || cache->insertions != NULL);
}

static bool chunk_valid(const mc_chunk_state *chunk)
{
    return chunk != NULL && (chunk->blank || chunk->data != NULL);
}

int32_t mc_chunk_cache_key(int32_t x, int32_t z)
{
    uint32_t key = (x < 0 ? UINT32_C(0x80000000) : 0u) | (((uint32_t)x & UINT32_C(0x7fff)) << 16u) |
                   (z < 0 ? UINT32_C(0x8000) : 0u) | ((uint32_t)z & UINT32_C(0x7fff));
    return mc_java_i32_narrow((int64_t)key);
}

bool mc_chunk_cache_init(mc_chunk_cache *cache, mc_chunk_cache_entry *entries, size_t capacity,
                         mc_chunk_state **insertions, size_t insertion_capacity)
{
    if (cache == NULL || (capacity > 0 && entries == NULL) ||
        (insertion_capacity > 0 && insertions == NULL)) {
        return false;
    }
    *cache = (mc_chunk_cache){entries, capacity, 0, insertions, insertion_capacity, 0};
    return true;
}

bool mc_chunk_cache_validate(const mc_chunk_cache *cache)
{
    if (!metadata_valid(cache))
        return false;
    for (size_t i = 0; i < cache->count; ++i) {
        if (!chunk_valid(cache->entries[i].chunk))
            return false;
        for (size_t j = 0; j < i; ++j) {
            if (cache->entries[i].key == cache->entries[j].key)
                return false;
        }
    }
    for (size_t i = 0; i < cache->insertion_count; ++i) {
        if (!chunk_valid(cache->insertions[i]))
            return false;
    }
    return true;
}

bool mc_chunk_cache_lookup(const mc_chunk_cache *cache, int32_t x, int32_t z,
                           mc_chunk_state **chunk)
{
    if (!metadata_valid(cache) || chunk == NULL)
        return false;
    int32_t key = mc_chunk_cache_key(x, z);
    for (size_t i = 0; i < cache->count; ++i) {
        if (cache->entries[i].key == key) {
            *chunk = cache->entries[i].chunk;
            return true;
        }
    }
    return false;
}

bool mc_chunk_cache_contains(const mc_chunk_cache *cache, int32_t x, int32_t z)
{
    mc_chunk_state *chunk;
    return mc_chunk_cache_lookup(cache, x, z, &chunk);
}

bool mc_chunk_cache_publish(mc_chunk_cache *cache, int32_t x, int32_t z, mc_chunk_state *chunk)
{
    if (!mc_chunk_cache_validate(cache) || !chunk_valid(chunk) ||
        cache->insertion_count == cache->insertion_capacity) {
        return false;
    }
    int32_t key = mc_chunk_cache_key(x, z);
    size_t index = 0;
    while (index < cache->count && cache->entries[index].key != key)
        ++index;
    if (index == cache->count && cache->count == cache->capacity)
        return false;
    cache->entries[index] = (mc_chunk_cache_entry){key, chunk};
    if (index == cache->count)
        ++cache->count;
    cache->insertions[cache->insertion_count++] = chunk;
    return true;
}
