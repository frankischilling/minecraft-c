#include "mc/world/cache/acquire.h"

#include "mc/core/java/java.h"

#include <string.h>

const char *mc_chunk_acquire_status_string(mc_chunk_acquire_status status)
{
    switch (status) {
        case MC_CHUNK_ACQUIRE_OK:
            return "ok";
        case MC_CHUNK_ACQUIRE_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_CHUNK_ACQUIRE_LIMIT:
            return "resource limit";
        case MC_CHUNK_ACQUIRE_PARTIAL_LIMIT:
            return "resource limit after partial effects";
        case MC_CHUNK_ACQUIRE_CALLBACK_FAILED:
            return "callback failed after partial effects";
        case MC_CHUNK_ACQUIRE_STATE_ERROR:
            return "downstream state error";
    }
    return "unknown chunk acquisition status";
}

static bool pending_metadata_valid(const mc_chunk_pending *pending)
{
    return pending != NULL && pending->capacity <= SIZE_MAX / sizeof(*pending->keys) &&
           pending->count <= pending->capacity && (pending->capacity == 0 || pending->keys != NULL);
}

bool mc_chunk_pending_init(mc_chunk_pending *pending, int32_t *keys, size_t capacity)
{
    if (pending == NULL || capacity > SIZE_MAX / sizeof(*keys) || (capacity != 0 && keys == NULL)) {
        return false;
    }
    *pending = (mc_chunk_pending){keys, capacity, 0};
    return true;
}

bool mc_chunk_pending_validate(const mc_chunk_pending *pending)
{
    if (!pending_metadata_valid(pending))
        return false;
    for (size_t i = 0; i < pending->count; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (pending->keys[i] == pending->keys[j])
                return false;
        }
    }
    return true;
}

bool mc_chunk_pending_contains(const mc_chunk_pending *pending, int32_t key)
{
    if (!pending_metadata_valid(pending))
        return false;
    for (size_t i = 0; i < pending->count; ++i) {
        if (pending->keys[i] == key)
            return true;
    }
    return false;
}

static void pending_remove(mc_chunk_pending *pending, int32_t key)
{
    for (size_t i = 0; i < pending->count; ++i) {
        if (pending->keys[i] == key) {
            memmove(pending->keys + i, pending->keys + i + 1,
                    (pending->count - i - 1u) * sizeof(*pending->keys));
            --pending->count;
            return;
        }
    }
}

static bool chunk_valid(const mc_chunk_state *chunk)
{
    return chunk != NULL && (chunk->blank || chunk->data != NULL);
}

bool mc_chunk_acquire_validate(const mc_chunk_acquire_context *context)
{
    if (context == NULL || context->cache == NULL ||
        context->cache->capacity > SIZE_MAX / sizeof(*context->cache->entries) ||
        context->cache->insertion_capacity > SIZE_MAX / sizeof(*context->cache->insertions) ||
        !mc_chunk_cache_validate(context->cache) || !mc_chunk_pending_validate(context->pending) ||
        !chunk_valid(context->fallback) || context->world_time == NULL ||
        context->register_tiles == NULL || context->register_entities == NULL ||
        (context->generate == NULL) != (context->populate == NULL)) {
        return false;
    }
    if (context->policy == MC_CHUNK_ACQUIRE_DEDICATED) {
        return context->world_force_load != NULL && context->provider_force_load != NULL &&
               context->spawn_x != NULL && context->spawn_z != NULL;
    }
    return context->policy == MC_CHUNK_ACQUIRE_BASE;
}

static size_t entry_index(const mc_chunk_cache *cache, int32_t key)
{
    size_t index = 0;
    while (index < cache->count && cache->entries[index].key != key)
        ++index;
    return index;
}

static bool publication_room(const mc_chunk_cache *cache, int32_t key)
{
    return cache->insertion_count < cache->insertion_capacity &&
           (entry_index(cache, key) < cache->count || cache->count < cache->capacity);
}

static mc_chunk_acquire_status callback_result(const mc_chunk_acquire_context *context,
                                               bool success)
{
    if (!mc_chunk_acquire_validate(context))
        return MC_CHUNK_ACQUIRE_STATE_ERROR;
    return success ? MC_CHUNK_ACQUIRE_OK : MC_CHUNK_ACQUIRE_CALLBACK_FAILED;
}

static mc_chunk_acquire_status activate(const mc_chunk_acquire_context *context,
                                        mc_chunk_state *chunk)
{
    if (chunk->blank)
        return MC_CHUNK_ACQUIRE_OK;
    /* Original pre-activation is a no-op. Activation itself sets active first,
     * then registers tiles and all eight slices, including empty collections. */
    chunk->active = true;
    mc_chunk_acquire_status status =
        callback_result(context, context->register_tiles(context->world, context, chunk));
    if (status != MC_CHUNK_ACQUIRE_OK)
        return status;
    for (size_t slice = 0; slice < 8u; ++slice) {
        status = callback_result(context,
                                 context->register_entities(context->world, context, chunk, slice));
        if (status != MC_CHUNK_ACQUIRE_OK)
            return status;
    }
    return MC_CHUNK_ACQUIRE_OK;
}

mc_chunk_acquire_status mc_chunk_acquire_get(const mc_chunk_acquire_context *context, int32_t x,
                                             int32_t z, mc_chunk_state **out)
{
    if (out == NULL || !mc_chunk_acquire_validate(context))
        return MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
    mc_chunk_state *chunk;
    if (mc_chunk_cache_lookup(context->cache, x, z, &chunk)) {
        *out = chunk;
        return MC_CHUNK_ACQUIRE_OK;
    }
    if (context->policy == MC_CHUNK_ACQUIRE_DEDICATED && !*context->world_force_load &&
        !*context->provider_force_load) {
        *out = context->fallback;
        return MC_CHUNK_ACQUIRE_OK;
    }
    return mc_chunk_acquire(context, x, z, out);
}

mc_chunk_acquire_status mc_chunk_acquire_populate(const mc_chunk_acquire_context *context,
                                                  int32_t x, int32_t z)
{
    if (!mc_chunk_acquire_validate(context))
        return MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
    mc_chunk_state *chunk;
    mc_chunk_acquire_status status = mc_chunk_acquire_get(context, x, z, &chunk);
    if (status != MC_CHUNK_ACQUIRE_OK || chunk->terrain_populated)
        return status;
    chunk->terrain_populated = true;
    if (context->populate == NULL)
        return MC_CHUNK_ACQUIRE_OK;
    status = callback_result(context, context->populate(context->provider, context, x, z));
    if (status != MC_CHUNK_ACQUIRE_OK)
        return status;
    if (!chunk->blank)
        chunk->dirty = true;
    return MC_CHUNK_ACQUIRE_OK;
}

static mc_chunk_acquire_status population_gates(const mc_chunk_acquire_context *context,
                                                mc_chunk_state *returned, int32_t x, int32_t z)
{
    int32_t west = mc_java_i32_sub(x, 1), east = mc_java_i32_add(x, 1);
    int32_t north = mc_java_i32_sub(z, 1), south = mc_java_i32_add(z, 1);
    mc_chunk_acquire_status status;
    mc_chunk_state *candidate;
    /* Keep the original live short-circuit order, including repeated presence
     * checks. Earlier population callbacks can change every later predicate. */
    if (!returned->terrain_populated && mc_chunk_cache_contains(context->cache, east, south) &&
        mc_chunk_cache_contains(context->cache, x, south) &&
        mc_chunk_cache_contains(context->cache, east, z)) {
        status = mc_chunk_acquire_populate(context, x, z);
        if (status != MC_CHUNK_ACQUIRE_OK)
            return status;
    }
    if (mc_chunk_cache_contains(context->cache, west, z)) {
        status = mc_chunk_acquire_get(context, west, z, &candidate);
        if (status != MC_CHUNK_ACQUIRE_OK)
            return status;
        if (!candidate->terrain_populated && mc_chunk_cache_contains(context->cache, west, south) &&
            mc_chunk_cache_contains(context->cache, x, south) &&
            mc_chunk_cache_contains(context->cache, west, z)) {
            status = mc_chunk_acquire_populate(context, west, z);
            if (status != MC_CHUNK_ACQUIRE_OK)
                return status;
        }
    }
    if (mc_chunk_cache_contains(context->cache, x, north)) {
        status = mc_chunk_acquire_get(context, x, north, &candidate);
        if (status != MC_CHUNK_ACQUIRE_OK)
            return status;
        if (!candidate->terrain_populated && mc_chunk_cache_contains(context->cache, east, north) &&
            mc_chunk_cache_contains(context->cache, x, north) &&
            mc_chunk_cache_contains(context->cache, east, z)) {
            status = mc_chunk_acquire_populate(context, x, north);
            if (status != MC_CHUNK_ACQUIRE_OK)
                return status;
        }
    }
    if (mc_chunk_cache_contains(context->cache, west, north)) {
        status = mc_chunk_acquire_get(context, west, north, &candidate);
        if (status != MC_CHUNK_ACQUIRE_OK)
            return status;
        if (!candidate->terrain_populated && mc_chunk_cache_contains(context->cache, west, north) &&
            mc_chunk_cache_contains(context->cache, x, north) &&
            mc_chunk_cache_contains(context->cache, west, z)) {
            status = mc_chunk_acquire_populate(context, west, north);
            if (status != MC_CHUNK_ACQUIRE_OK)
                return status;
        }
    }
    return MC_CHUNK_ACQUIRE_OK;
}

mc_chunk_acquire_status mc_chunk_acquire(const mc_chunk_acquire_context *context, int32_t x,
                                         int32_t z, mc_chunk_state **out)
{
    if (out == NULL || !mc_chunk_acquire_validate(context))
        return MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
    int32_t key = mc_chunk_cache_key(x, z);
    size_t index = entry_index(context->cache, key);
    if (index == context->cache->count && !publication_room(context->cache, key))
        return MC_CHUNK_ACQUIRE_LIMIT;
    pending_remove(context->pending, key);
    if (index < context->cache->count) {
        *out = context->cache->entries[index].chunk;
        return MC_CHUNK_ACQUIRE_OK;
    }
    mc_chunk_state *chunk = NULL;
    if (context->load != NULL) {
        mc_chunk_load_result loaded = context->load(context->loader, context, x, z, &chunk);
        if (!mc_chunk_acquire_validate(context))
            return MC_CHUNK_ACQUIRE_STATE_ERROR;
        switch (loaded) {
            case MC_CHUNK_LOAD_LOADED:
                if (!chunk_valid(chunk))
                    return MC_CHUNK_ACQUIRE_STATE_ERROR;
                /* The live time read follows the complete load callback. */
                chunk->last_save_time = *context->world_time;
                break;
            case MC_CHUNK_LOAD_MISS:
            case MC_CHUNK_LOAD_CAUGHT_ERROR:
                chunk = NULL;
                break;
            case MC_CHUNK_LOAD_FATAL:
                return MC_CHUNK_ACQUIRE_CALLBACK_FAILED;
            default:
                return MC_CHUNK_ACQUIRE_STATE_ERROR;
        }
    }
    if (chunk == NULL) {
        if (context->generate == NULL) {
            chunk = context->fallback;
        } else {
            mc_chunk_acquire_status status = callback_result(
                context, context->generate(context->provider, context, x, z, &chunk));
            if (status != MC_CHUNK_ACQUIRE_OK)
                return status;
            if (!chunk_valid(chunk))
                return MC_CHUNK_ACQUIRE_STATE_ERROR;
        }
    }
    if (!publication_room(context->cache, key))
        return MC_CHUNK_ACQUIRE_PARTIAL_LIMIT;
    /* Callback reentrancy may have inserted this same key. Retain the outer
     * result and replace unconditionally; keep every intervening list append. */
    index = entry_index(context->cache, key);
    context->cache->entries[index] = (mc_chunk_cache_entry){key, chunk};
    if (index == context->cache->count)
        ++context->cache->count;
    context->cache->insertions[context->cache->insertion_count++] = chunk;
    mc_chunk_acquire_status status = activate(context, chunk);
    if (status != MC_CHUNK_ACQUIRE_OK)
        return status;
    status = population_gates(context, chunk, x, z);
    if (status == MC_CHUNK_ACQUIRE_LIMIT)
        return MC_CHUNK_ACQUIRE_PARTIAL_LIMIT;
    if (status == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT)
        return MC_CHUNK_ACQUIRE_STATE_ERROR;
    if (status != MC_CHUNK_ACQUIRE_OK)
        return status;
    *out = chunk;
    return MC_CHUNK_ACQUIRE_OK;
}

mc_chunk_acquire_status mc_chunk_acquire_mark_drop(const mc_chunk_acquire_context *context,
                                                   int32_t x, int32_t z)
{
    if (!mc_chunk_acquire_validate(context) || context->policy != MC_CHUNK_ACQUIRE_DEDICATED)
        return MC_CHUNK_ACQUIRE_INVALID_ARGUMENT;
    int32_t dx = mc_java_i32_sub(mc_java_i32_add(mc_java_i32_mul(x, 16), 8), *context->spawn_x);
    int32_t dz = mc_java_i32_sub(mc_java_i32_add(mc_java_i32_mul(z, 16), 8), *context->spawn_z);
    if (dx >= -128 && dx <= 128 && dz >= -128 && dz <= 128)
        return MC_CHUNK_ACQUIRE_OK;
    int32_t key = mc_chunk_cache_key(x, z);
    if (mc_chunk_pending_contains(context->pending, key))
        return MC_CHUNK_ACQUIRE_OK;
    if (context->pending->count == context->pending->capacity)
        return MC_CHUNK_ACQUIRE_LIMIT;
    context->pending->keys[context->pending->count++] = key;
    return MC_CHUNK_ACQUIRE_OK;
}
