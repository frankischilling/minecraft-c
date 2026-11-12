#include "mc/world/cache/cache.h"

#include "mc/core/java/java.h"

static bool metadata_valid(const mc_chunk_cache *cache) {
    return cache != NULL && cache->count <= cache->capacity &&
           cache->insertion_count <= cache->insertion_capacity &&
           (cache->capacity == 0 || cache->entries != NULL) &&
           (cache->insertion_capacity == 0 || cache->insertions != NULL);
}

static bool chunk_valid(const mc_chunk_state *chunk) {
    return chunk != NULL && (chunk->blank || chunk->data != NULL);
}

int32_t mc_chunk_cache_key(int32_t x, int32_t z) {
    uint32_t key = (x < 0 ? UINT32_C(0x80000000) : 0u) | (((uint32_t)x & UINT32_C(0x7fff)) << 16u) |
                   (z < 0 ? UINT32_C(0x8000) : 0u) | ((uint32_t)z & UINT32_C(0x7fff));
    return mc_java_i32_narrow((int64_t)key);
}

bool mc_chunk_cache_init(mc_chunk_cache *cache, mc_chunk_cache_entry *entries, size_t capacity,
                         mc_chunk_state **insertions, size_t insertion_capacity) {
    if (cache == NULL || (capacity > 0 && entries == NULL) ||
        (insertion_capacity > 0 && insertions == NULL)) {
        return false;
    }
    *cache = (mc_chunk_cache){entries, capacity, 0, insertions, insertion_capacity, 0};
    return true;
}

static bool compact_entries_valid(const mc_chunk_cache *cache) {
    /* Validate caller-mutable entries on every call. Store indices in bounded
     * scratch space so no allocation, cache mutation or stale validity flag is
     * needed. At most half the slots are occupied; all int keys remain valid. */
    uint16_t seen[1024] = {0};
    for (size_t i = 0; i < cache->count; ++i) {
        if (!chunk_valid(cache->entries[i].chunk))
            return false;
        uint32_t hash = (uint32_t)cache->entries[i].key;
        hash ^= hash >> 16u;
        hash *= UINT32_C(0x7feb352d);
        hash ^= hash >> 15u;
        hash *= UINT32_C(0x846ca68b);
        hash ^= hash >> 16u;
        size_t slot = hash & 1023u;
        while (seen[slot] != 0u) {
            if (cache->entries[seen[slot] - 1u].key == cache->entries[i].key)
                return false;
            slot = (slot + 1u) & 1023u;
        }
        seen[slot] = (uint16_t)(i + 1u);
    }
    return true;
}

/* Keep the fallback body separate from the blocked validator on supported
 * compilers. This layout variant leaves validation policy and work unchanged. */
#if defined(_MSC_VER)
#define MC_CACHE_VALIDATION_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define MC_CACHE_VALIDATION_NOINLINE __attribute__((noinline))
#else
#define MC_CACHE_VALIDATION_NOINLINE
#endif

static MC_CACHE_VALIDATION_NOINLINE bool pairwise_entries_valid(const mc_chunk_cache *cache) {
    for (size_t i = 0; i < cache->count; ++i) {
        if (!chunk_valid(cache->entries[i].chunk))
            return false;
        for (size_t j = 0; j < i; ++j) {
            if (cache->entries[i].key == cache->entries[j].key)
                return false;
        }
    }
    return true;
}

#undef MC_CACHE_VALIDATION_NOINLINE

static size_t blocked_key_slot(int32_t key) {
    uint32_t hash = (uint32_t)key;
    hash ^= hash >> 16u;
    hash *= UINT32_C(0x7feb352d);
    hash ^= hash >> 15u;
    hash *= UINT32_C(0x846ca68b);
    hash ^= hash >> 16u;
    return hash & 1023u;
}

/* Pure validation of live caller-owned entries. Each 512-entry batch owns
 * only local ordinal+1 words; no cache state or caller object is modified.
 * Passive future-key reads may be reordered; there are no Source callbacks. */
static bool blocked_entries_valid(const mc_chunk_cache *cache) {
    for (size_t begin = 0; begin < cache->count;) {
        uint16_t seen[1024] = {0};
        const size_t remaining = cache->count - begin;
        const size_t width = remaining > 512u ? 512u : remaining;
        const size_t end = begin + width; /* end <= count, without rounded-up overflow. */
        for (size_t i = begin; i < end; ++i) {
            if (!chunk_valid(cache->entries[i].chunk))
                return false;
            size_t slot = blocked_key_slot(cache->entries[i].key);
            size_t probes = 0;
            while (seen[slot] != 0u) {
                const size_t prior = begin + (size_t)(seen[slot] - 1u);
                if (cache->entries[prior].key == cache->entries[i].key)
                    return false;
                if (++probes >= 64u)
                    return pairwise_entries_valid(cache);
                slot = (slot + 1u) & 1023u;
            }
            seen[slot] = (uint16_t)(i - begin + 1u); /* 1..512, independent of count. */
        }
        for (size_t i = end; i < cache->count; ++i) {
            size_t slot = blocked_key_slot(cache->entries[i].key);
            size_t probes = 0;
            while (seen[slot] != 0u) {
                const size_t prior = begin + (size_t)(seen[slot] - 1u);
                if (cache->entries[prior].key == cache->entries[i].key)
                    return false;
                if (++probes >= 64u)
                    return pairwise_entries_valid(cache);
                slot = (slot + 1u) & 1023u;
            }
        }
        begin = end;
    }
    return true;
}

bool mc_chunk_cache_validate(const mc_chunk_cache *cache) {
    if (!metadata_valid(cache))
        return false;
    if (cache->count > 8u && cache->count <= 512u) {
        if (!compact_entries_valid(cache))
            return false;
    } else if (cache->count > 512u) {
        if (!blocked_entries_valid(cache))
            return false;
    } else {
        for (size_t i = 0; i < cache->count; ++i) {
            if (!chunk_valid(cache->entries[i].chunk))
                return false;
            for (size_t j = 0; j < i; ++j) {
                if (cache->entries[i].key == cache->entries[j].key)
                    return false;
            }
        }
    }
    for (size_t i = 0; i < cache->insertion_count; ++i) {
        if (!chunk_valid(cache->insertions[i]))
            return false;
    }
    return true;
}

bool mc_chunk_cache_lookup(const mc_chunk_cache *cache, int32_t x, int32_t z,
                           mc_chunk_state **chunk) {
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

bool mc_chunk_cache_contains(const mc_chunk_cache *cache, int32_t x, int32_t z) {
    mc_chunk_state *chunk;
    return mc_chunk_cache_lookup(cache, x, z, &chunk);
}

bool mc_chunk_cache_publish(mc_chunk_cache *cache, int32_t x, int32_t z, mc_chunk_state *chunk) {
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
