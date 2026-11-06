/* Private implementation of the source provider's owned map adapter. No raw
 * caller-owned cache is admitted here. The optional index never certifies a
 * caller-mutable descriptor: a raw export destroys it for this owner lifetime. */
#include "../cache/acquire_internal.h"
#include "mc/world/storage/source_handler.h"
#include <string.h>

typedef struct mc_source_owned_cache {
    mc_chunk_cache_entry *entries;
    mc_chunk_state **insertions;
    size_t capacity, insertion_capacity, count, insertion_count;
    size_t slots_count, mutation_depth;
    uint32_t *slots; /* Same-allocation tail. Entry index+1; zero is empty. */
} mc_source_owned_cache;

static void owned_cache_discard(mc_chunk_provider *provider) {
    if (provider == NULL || provider->owned_cache == NULL)
        return;
    free(provider->owned_cache);
    provider->owned_cache = NULL;
    if (provider->owned_cache_bytes <= provider->direct_bytes)
        provider->direct_bytes -= provider->owned_cache_bytes;
    provider->owned_cache_bytes = 0;
}

static size_t owned_cache_hash(int32_t key, size_t mask) {
    uint32_t value = (uint32_t)key;
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return (size_t)value & mask;
}

static bool owned_cache_rebuild(mc_chunk_provider *provider) {
    mc_source_owned_cache *owner = provider->owned_cache;
    mc_chunk_cache *cache = &provider->cache;
    if (owner == NULL || cache->entries != owner->entries ||
        cache->insertions != owner->insertions || cache->capacity != owner->capacity ||
        cache->insertion_capacity != owner->insertion_capacity || !mc_chunk_cache_validate(cache))
        return false;
    memset(owner->slots, 0, owner->slots_count * sizeof(*owner->slots));
    for (size_t i = 0; i < cache->count; ++i) {
        size_t slot = owned_cache_hash(cache->entries[i].key, owner->slots_count - 1);
        while (owner->slots[slot] != 0)
            slot = (slot + 1) & (owner->slots_count - 1);
        owner->slots[slot] = (uint32_t)(i + 1);
    }
    owner->count = cache->count;
    owner->insertion_count = cache->insertion_count;
    ++provider->cache_rebuilds;
    return true;
}

static void owned_cache_create_optional(mc_chunk_provider *provider) {
    /* This adapter belongs only to source-constructed provider storage. A
     * baseline composition keeps its caller-owned validator unchanged. No
     * required allocation is displaced; inability to fit retains baseline. */
    if (provider->max_chunks == 0 || !mc_world_store_is_source(provider->store) ||
        provider->world->construction.entry > MC_WORLD_RUNTIME_CLIENT_COPY ||
        provider->max_chunks > UINT32_MAX / 2u ||
        provider->direct_bytes > provider->max_direct_bytes)
        return;
    size_t slots = 2;
    while (slots < provider->max_chunks * 2u) {
        if (slots > SIZE_MAX / 2u)
            return;
        slots *= 2u;
    }
    if (slots > (SIZE_MAX - sizeof(mc_source_owned_cache)) / sizeof(uint32_t))
        return;
    const size_t bytes = sizeof(mc_source_owned_cache) + slots * sizeof(uint32_t);
    if (bytes > provider->max_direct_bytes - provider->direct_bytes)
        return;
    mc_source_owned_cache *owner = calloc(1, bytes);
    if (owner == NULL)
        return;
    *owner = (mc_source_owned_cache){.entries = provider->entries,
                                     .insertions = provider->insertions,
                                     .capacity = provider->max_chunks,
                                     .insertion_capacity = provider->max_chunks,
                                     .slots_count = slots,
                                     .slots = (uint32_t *)(void *)(owner + 1)};
    provider->owned_cache = owner;
    provider->owned_cache_bytes = bytes;
    provider->direct_bytes += bytes;
    if (!owned_cache_rebuild(provider))
        owned_cache_discard(provider);
}

void mc_chunk_provider_cache_export(mc_chunk_provider *provider) {
    if (provider == NULL)
        return;
    ++provider->cache_raw_exports;
    owned_cache_discard(provider);
    /* No operation recreates the owner after construction. External aliases
     * may remain mutable indefinitely, including after a callback returns. */
}

void mc_chunk_provider_cache_mutation_begin(mc_chunk_provider *provider) {
    if (provider == NULL || provider->owned_cache == NULL)
        return;
    if (provider->owned_cache->mutation_depth == SIZE_MAX) {
        owned_cache_discard(provider);
        return;
    }
    ++provider->owned_cache->mutation_depth;
}

void mc_chunk_provider_cache_mutation_end(mc_chunk_provider *provider) {
    if (provider == NULL || provider->owned_cache == NULL)
        return; /* Export inside a callback is irreversible. */
    mc_source_owned_cache *owner = provider->owned_cache;
    if (owner->mutation_depth == 0) {
        owned_cache_discard(provider);
        return;
    }
    --owner->mutation_depth;
    if (owner->mutation_depth == 0 && !owned_cache_rebuild(provider))
        owned_cache_discard(provider);
}

bool mc_chunk_provider_cache_get(mc_chunk_provider *provider,
                                 const mc_chunk_acquire_context *context, int32_t x, int32_t z,
                                 mc_chunk_state **out, mc_chunk_acquire_status *status) {
    if (provider == NULL)
        return false;
    mc_source_owned_cache *owner = provider->owned_cache;
    if (owner == NULL) {
        ++provider->cache_generic_gets;
        return false;
    }
    const bool mutating = owner->mutation_depth != 0;
    const mc_chunk_cache *cache = &provider->cache;
    if (context == NULL || context->cache != cache || context->world != provider->world ||
        cache->entries != owner->entries || cache->insertions != owner->insertions ||
        cache->capacity != owner->capacity ||
        cache->insertion_capacity != owner->insertion_capacity || cache->count > cache->capacity ||
        cache->insertion_count > cache->insertion_capacity ||
        (!mutating &&
         (cache->count != owner->count || cache->insertion_count != owner->insertion_count))) {
        owned_cache_discard(provider);
        ++provider->cache_generic_gets;
        return false;
    }
    if (mutating)
        ++provider->cache_generic_gets;
    if (out == NULL || !mc_chunk_acquire_validate_without_cache(context)) {
        *status = MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
        return true;
    }
    const int32_t key = mc_chunk_cache_key(x, z);
    if (mutating) {
        /* Only this implementation owns the backing, but its index snapshot
         * may precede a completed publish/unpublish or synchronous reentry.
         * Read the live bounded map in ordinary lookup order instead. Raw
         * exports already discard this owner permanently. A miss still enters
         * the unchanged coordinator; no callback or pending edit occurs here. */
        for (size_t index = 0; index < cache->count; ++index) {
            const mc_chunk_cache_entry *entry = &cache->entries[index];
            if (entry->key != key)
                continue;
            if (entry->chunk == NULL || (!entry->chunk->blank && entry->chunk->data == NULL)) {
                *status = MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
                return true;
            }
            *out = entry->chunk;
            *status = MC_CHUNK_ACQUIRE_OK;
            return true;
        }
        return false;
    }
    size_t slot = owned_cache_hash(key, owner->slots_count - 1);
    for (size_t inspected = 0; inspected < owner->slots_count; ++inspected) {
        const uint32_t word = owner->slots[slot];
        if (word == 0) {
            ++provider->cache_misses;
            ++provider->cache_generic_gets;
            return false;
        }
        const size_t index = (size_t)word - 1;
        if (index >= cache->count) {
            owned_cache_discard(provider);
            ++provider->cache_generic_gets;
            return false;
        }
        const mc_chunk_cache_entry *entry = &cache->entries[index];
        if (entry->key == key) {
            if (entry->chunk == NULL || (!entry->chunk->blank && entry->chunk->data == NULL)) {
                *status = MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
                return true;
            }
            *out = entry->chunk;
            *status = MC_CHUNK_ACQUIRE_OK;
            ++provider->cache_hits;
            return true;
        }
        slot = (slot + 1) & (owner->slots_count - 1);
    }
    owned_cache_discard(provider);
    ++provider->cache_generic_gets;
    return false;
}

static bool owned_cache_contains(const mc_chunk_provider *provider, int32_t x, int32_t z,
                                 bool *out) {
    const mc_source_owned_cache *owner = provider->owned_cache;
    if (owner == NULL || owner->mutation_depth != 0)
        return false;
    const mc_chunk_cache *cache = &provider->cache;
    if (cache->entries != owner->entries || cache->insertions != owner->insertions ||
        cache->capacity != owner->capacity ||
        cache->insertion_capacity != owner->insertion_capacity || cache->count != owner->count ||
        cache->insertion_count != owner->insertion_count)
        return false;
    /* Presence is packed-key membership only. Unlike GET it does not inspect
     * values, pending or acquisition bindings, and changes no diagnostics or
     * counters. Unknown/stale native index state falls back to live lookup. */
    const int32_t key = mc_chunk_cache_key(x, z);
    size_t slot = owned_cache_hash(key, owner->slots_count - 1);
    for (size_t inspected = 0; inspected < owner->slots_count; ++inspected) {
        const uint32_t word = owner->slots[slot];
        if (word == 0) {
            *out = false;
            return true;
        }
        const size_t index = (size_t)word - 1;
        if (index >= cache->count)
            return false;
        if (cache->entries[index].key == key) {
            *out = true;
            return true;
        }
        slot = (slot + 1) & (owner->slots_count - 1);
    }
    return false;
}

bool mc_chunk_provider_cache_observe(const mc_chunk_provider *provider,
                                     mc_chunk_provider_cache_observation *out) {
    if (provider == NULL || out == NULL)
        return false;
    const mc_source_owned_cache *owner = provider->owned_cache;
    *out = (mc_chunk_provider_cache_observation){
        .owned = owner != NULL,
        .map_count = provider->cache.count,
        .insertion_count = provider->cache.insertion_count,
        .index_capacity = owner == NULL ? 0 : owner->slots_count,
        .index_bytes = provider->owned_cache_bytes,
        .mutation_depth = owner == NULL ? 0 : owner->mutation_depth,
        .direct_bytes = provider->direct_bytes,
        .hits = provider->cache_hits,
        .misses = provider->cache_misses,
        .generic_gets = provider->cache_generic_gets,
        .raw_exports = provider->cache_raw_exports,
        .rebuilds = provider->cache_rebuilds};
    return true;
}

mc_world_storage_status mc_chunk_provider_cache_eligible(const mc_chunk_provider *provider,
                                                         int64_t now, bool force, size_t *out,
                                                         mc_world_storage_error *error) {
    if (provider == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "owned cache scalar observer arguments");
    const mc_chunk_cache *cache = &provider->cache;
    if (!mc_chunk_cache_validate(cache))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "owned cache scalar observer metadata");
    size_t count = 0;
    for (size_t i = 0; i < cache->insertion_count; ++i)
        if (mc_chunk_save_eligible(cache->insertions[i], now, force))
            ++count;
    *out = count;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}
