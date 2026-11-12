#include "mc/world/cache/lifecycle.h"

#include "mc/core/java/java.h"

#include <string.h>

const char *mc_chunk_lifecycle_status_string(mc_chunk_lifecycle_status status) {
    switch (status) {
        case MC_CHUNK_LIFECYCLE_OK:
            return "ok";
        case MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_CHUNK_LIFECYCLE_LIMIT:
            return "reference resource limit after possible partial effects";
        case MC_CHUNK_LIFECYCLE_CALLBACK_FAILED:
            return "callback failed after partial effects";
        case MC_CHUNK_LIFECYCLE_STATE_ERROR:
            return "downstream state error";
    }
    return "unknown chunk lifecycle status";
}

static bool chunk_valid(const mc_chunk_state *chunk) {
    return chunk != NULL && (chunk->blank || chunk->data != NULL);
}

bool mc_chunk_lifecycle_validate(const mc_chunk_lifecycle_context *context) {
    if (context == NULL || context->world_time == NULL ||
        !mc_chunk_cache_validate(context->cache) || !mc_chunk_pending_validate(context->pending) ||
        context->cache->capacity > SIZE_MAX / sizeof(*context->cache->entries) ||
        context->cache->insertion_capacity > SIZE_MAX / sizeof(*context->cache->insertions))
        return false;
    bool loader = context->save_main != NULL;
    if (loader != (context->save_extra != NULL) || loader != (context->flush != NULL) ||
        loader != (context->loader_maintenance != NULL))
        return false;
    if (context->references != NULL &&
        (context->references->retain == NULL || context->references->release == NULL))
        return false;
    if (context->policy == MC_CHUNK_ACQUIRE_DEDICATED)
        return context->saving_disabled != NULL;
    return context->policy == MC_CHUNK_ACQUIRE_BASE;
}

bool mc_chunk_lifecycle_saving_enabled(const mc_chunk_lifecycle_context *context) {
    return mc_chunk_lifecycle_validate(context) &&
           (context->policy == MC_CHUNK_ACQUIRE_BASE || !*context->saving_disabled);
}

bool mc_chunk_save_eligible(const mc_chunk_state *chunk, int64_t now, bool force) {
    if (chunk == NULL || chunk->blank || chunk->never_save)
        return false;
    return chunk->dirty ||
           (chunk->has_entities && (force ? now != chunk->last_save_time
                                          : now >= mc_java_i64_add(chunk->last_save_time, 600)));
}

static bool retain(const mc_chunk_lifecycle_context *context, mc_chunk_state *chunk) {
    return context->references == NULL ||
           context->references->retain(context->references->owner, chunk);
}

static void release(const mc_chunk_lifecycle_context *context, mc_chunk_state *chunk) {
    if (context->references != NULL)
        context->references->release(context->references->owner, chunk);
}

static mc_chunk_lifecycle_status checked(const mc_chunk_lifecycle_context *context,
                                         mc_chunk_callback_result result, bool catch_io,
                                         bool catch_exception) {
    if (!mc_chunk_lifecycle_validate(context))
        return MC_CHUNK_LIFECYCLE_STATE_ERROR;
    switch (result) {
        case MC_CHUNK_CALLBACK_OK:
            return MC_CHUNK_LIFECYCLE_OK;
        case MC_CHUNK_CALLBACK_IO_ERROR:
            return catch_io ? MC_CHUNK_LIFECYCLE_OK : MC_CHUNK_LIFECYCLE_CALLBACK_FAILED;
        case MC_CHUNK_CALLBACK_EXCEPTION:
            return catch_exception ? MC_CHUNK_LIFECYCLE_OK : MC_CHUNK_LIFECYCLE_CALLBACK_FAILED;
        case MC_CHUNK_CALLBACK_FATAL:
            return MC_CHUNK_LIFECYCLE_CALLBACK_FAILED;
    }
    return MC_CHUNK_LIFECYCLE_STATE_ERROR;
}

static mc_chunk_lifecycle_status save_main(const mc_chunk_lifecycle_context *context,
                                           mc_chunk_state *chunk) {
    if (context->save_main == NULL)
        return MC_CHUNK_LIFECYCLE_OK;
    chunk->last_save_time = *context->world_time;
    return checked(context, context->save_main(context->loader, context, chunk), true, false);
}

static mc_chunk_lifecycle_status save_extra(const mc_chunk_lifecycle_context *context,
                                            mc_chunk_state *chunk) {
    return context->save_extra == NULL
               ? MC_CHUNK_LIFECYCLE_OK
               : checked(context, context->save_extra(context->loader, context, chunk), true, true);
}

mc_chunk_lifecycle_status mc_chunk_lifecycle_save(const mc_chunk_lifecycle_context *context,
                                                  bool force, bool *completed) {
    if (completed == NULL || !mc_chunk_lifecycle_validate(context))
        return MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT;
    size_t attempted = 0;
    for (size_t index = 0; index < context->cache->insertion_count; ++index) {
        mc_chunk_state *chunk = context->cache->insertions[index];
        if (!retain(context, chunk))
            return MC_CHUNK_LIFECYCLE_LIMIT;
        mc_chunk_lifecycle_status status = MC_CHUNK_LIFECYCLE_OK;
        if (force && !chunk->never_save)
            status = save_extra(context, chunk);
        bool eligible = status == MC_CHUNK_LIFECYCLE_OK &&
                        mc_chunk_save_eligible(chunk, *context->world_time, force);
        if (eligible) {
            status = save_main(context, chunk);
            if (status == MC_CHUNK_LIFECYCLE_OK) {
                chunk->dirty = false;
                ++attempted;
            }
        }
        release(context, chunk);
        if (status != MC_CHUNK_LIFECYCLE_OK)
            return status;
        if (eligible && !force && attempted == 24) {
            *completed = false;
            return MC_CHUNK_LIFECYCLE_OK;
        }
    }
    if (force && context->flush != NULL) {
        mc_chunk_lifecycle_status status =
            checked(context, context->flush(context->loader, context), false, false);
        if (status != MC_CHUNK_LIFECYCLE_OK)
            return status;
    }
    *completed = true;
    return MC_CHUNK_LIFECYCLE_OK;
}

static mc_chunk_lifecycle_status detach(const mc_chunk_lifecycle_context *context,
                                        mc_chunk_state *chunk) {
    if (chunk->blank)
        return MC_CHUNK_LIFECYCLE_OK;
    chunk->active = false;
    mc_chunk_lifecycle_status status = checked(
        context, context->invalidate_tile_entities(context->world, context, chunk), false, false);
    for (size_t slice = 0; status == MC_CHUNK_LIFECYCLE_OK && slice < 8; ++slice)
        status = checked(context, context->defer_entities(context->world, context, chunk, slice),
                         false, false);
    return status;
}

mc_chunk_lifecycle_status mc_chunk_lifecycle_detach(const mc_chunk_lifecycle_context *context,
                                                    mc_chunk_state *chunk) {
    if (!mc_chunk_lifecycle_validate(context) || !chunk_valid(chunk) ||
        context->invalidate_tile_entities == NULL || context->defer_entities == NULL)
        return MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT;
    if (!retain(context, chunk))
        return MC_CHUNK_LIFECYCLE_LIMIT;
    mc_chunk_lifecycle_status status = detach(context, chunk);
    release(context, chunk);
    return status;
}

static size_t entry_index(const mc_chunk_cache *cache, int32_t key) {
    size_t index = 0;
    while (index < cache->count && cache->entries[index].key != key)
        ++index;
    return index;
}

static void unpublish(mc_chunk_cache *cache, int32_t key, mc_chunk_state *chunk) {
    size_t index = entry_index(cache, key);
    if (index != cache->count) {
        memmove(cache->entries + index, cache->entries + index + 1,
                (cache->count - index - 1) * sizeof(*cache->entries));
        --cache->count;
    }
    index = 0;
    while (index < cache->insertion_count && cache->insertions[index] != chunk)
        ++index;
    if (index != cache->insertion_count) {
        memmove(cache->insertions + index, cache->insertions + index + 1,
                (cache->insertion_count - index - 1) * sizeof(*cache->insertions));
        --cache->insertion_count;
    }
}

mc_chunk_lifecycle_status mc_chunk_lifecycle_maintain(const mc_chunk_lifecycle_context *context,
                                                      bool *provider_result) {
    if (provider_result == NULL || !mc_chunk_lifecycle_validate(context) ||
        context->pending->history == NULL || context->invalidate_tile_entities == NULL ||
        context->defer_entities == NULL || context->provider_maintenance == NULL)
        return MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT;
    if (context->policy != MC_CHUNK_ACQUIRE_DEDICATED || !*context->saving_disabled) {
        for (size_t iteration = 0; iteration < 100; ++iteration) {
            int32_t key = 0;
            mc_chunk_acquire_status selected = mc_chunk_pending_first(context->pending, &key);
            if (selected == MC_CHUNK_ACQUIRE_EMPTY)
                continue;
            if (selected != MC_CHUNK_ACQUIRE_OK)
                return MC_CHUNK_LIFECYCLE_STATE_ERROR;
            size_t index = entry_index(context->cache, key);
            if (index == context->cache->count)
                return MC_CHUNK_LIFECYCLE_STATE_ERROR;
            mc_chunk_state *chunk = context->cache->entries[index].chunk;
            if (!chunk_valid(chunk))
                return MC_CHUNK_LIFECYCLE_STATE_ERROR;
            if (!retain(context, chunk))
                return MC_CHUNK_LIFECYCLE_LIMIT;
            mc_chunk_lifecycle_status status = detach(context, chunk);
            if (status == MC_CHUNK_LIFECYCLE_OK)
                status = save_main(context, chunk);
            if (status == MC_CHUNK_LIFECYCLE_OK)
                status = save_extra(context, chunk);
            if (status == MC_CHUNK_LIFECYCLE_OK) {
                bool changed = false;
                if (mc_chunk_pending_remove(context->pending, key, &changed) != MC_CHUNK_ACQUIRE_OK)
                    status = MC_CHUNK_LIFECYCLE_STATE_ERROR;
                else
                    unpublish(context->cache, key, chunk);
            }
            release(context, chunk);
            if (status != MC_CHUNK_LIFECYCLE_OK)
                return status;
        }
        if (context->loader_maintenance != NULL) {
            mc_chunk_lifecycle_status status = checked(
                context, context->loader_maintenance(context->loader, context), false, false);
            if (status != MC_CHUNK_LIFECYCLE_OK)
                return status;
        }
    }
    bool result = false;
    mc_chunk_lifecycle_status status = checked(
        context, context->provider_maintenance(context->provider, context, &result), false, false);
    if (status == MC_CHUNK_LIFECYCLE_OK)
        *provider_result = result;
    return status;
}

bool mc_world_save_validate(const mc_world_save_context *context) {
    return context != NULL && mc_chunk_lifecycle_validate(context->chunks) &&
           context->check_session != NULL && context->save_metadata != NULL &&
           context->save_auxiliary != NULL;
}

static mc_chunk_lifecycle_status world_checked(const mc_world_save_context *context,
                                               mc_chunk_callback_result result) {
    if (!mc_world_save_validate(context))
        return MC_CHUNK_LIFECYCLE_STATE_ERROR;
    return checked(context->chunks, result, false, false);
}

static mc_chunk_lifecycle_status world_metadata(const mc_world_save_context *context) {
    mc_chunk_lifecycle_status status =
        world_checked(context, context->check_session(context->world, context));
    if (status == MC_CHUNK_LIFECYCLE_OK)
        status = world_checked(context, context->save_metadata(context->world, context));
    if (status == MC_CHUNK_LIFECYCLE_OK)
        status = world_checked(context, context->save_auxiliary(context->world, context));
    return status;
}

mc_chunk_lifecycle_status mc_world_lifecycle_save(const mc_world_save_context *context,
                                                  bool force) {
    if (!mc_world_save_validate(context))
        return MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT;
    if (!mc_chunk_lifecycle_saving_enabled(context->chunks))
        return MC_CHUNK_LIFECYCLE_OK;
    mc_chunk_lifecycle_status status = MC_CHUNK_LIFECYCLE_OK;
    if (context->report_progress != NULL)
        status = world_checked(
            context, context->report_progress(context->progress, context, MC_WORLD_SAVE_LEVEL));
    if (status == MC_CHUNK_LIFECYCLE_OK)
        status = world_metadata(context);
    if (status == MC_CHUNK_LIFECYCLE_OK && context->report_progress != NULL)
        status = world_checked(
            context, context->report_progress(context->progress, context, MC_WORLD_SAVE_CHUNKS));
    bool completed = false;
    if (status == MC_CHUNK_LIFECYCLE_OK)
        status = mc_chunk_lifecycle_save(context->chunks, force, &completed);
    return status;
}

mc_chunk_lifecycle_status mc_world_lifecycle_save_client(const mc_world_save_context *context,
                                                         int32_t pass, bool *completed) {
    if (completed == NULL || !mc_world_save_validate(context) ||
        context->chunks->policy != MC_CHUNK_ACQUIRE_BASE)
        return MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT;
    if (!mc_chunk_lifecycle_saving_enabled(context->chunks)) {
        *completed = true;
        return MC_CHUNK_LIFECYCLE_OK;
    }
    if (pass == 0) {
        mc_chunk_lifecycle_status status = world_metadata(context);
        if (status != MC_CHUNK_LIFECYCLE_OK)
            return status;
    }
    return mc_chunk_lifecycle_save(context->chunks, false, completed);
}
