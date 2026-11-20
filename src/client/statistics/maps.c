#include "internal.h"

#include <limits.h>

bool stats_statistic_valid(mc_registry_classes *registry, const mc_statistic *statistic) {
    return statistic == NULL || statistic->classes == registry;
}

mc_client_stats_status stats_map_make(mc_registry_classes *registry, stats_memory *memory,
                                      const mc_client_stats_options *options,
                                      mc_client_stats_map **out, mc_client_stats_error *error) {
    if (memory->registry == NULL) {
        mc_registry_error cause;
        const mc_registry_status status = mc_registry_classes_retain(
            registry, MC_REGISTRY_CLIENT, registry->options.random_profile, &cause);
        if (status != MC_REGISTRY_OK) {
            error->registry = status;
            return stats_fail(error, MC_CLIENT_STATS_REGISTRY_FAILED, "Stats Map Registry hold",
                              NULL, NULL, NULL, 0);
        }
        memory->registry = registry;
    }
    if (memory->registry != registry)
        return stats_fail(error, MC_CLIENT_STATS_INVALID_ARGUMENT,
                          "Stats Map shared Registry identity", NULL, NULL, NULL, 0);
    if (!stats_memory_retain(memory)) {
        return stats_fail(error, MC_CLIENT_STATS_LIMIT, "Stats Map memory hold", NULL, NULL, NULL,
                          0);
    }
    mc_client_stats_map *map = stats_alloc(memory, sizeof(*map), error);
    if (map == NULL) {
        stats_memory_release(memory);
        return error->status;
    }
    *map = (mc_client_stats_map){
        .holds = 1, .memory = memory, .registry = registry, .options = *options};
    if (mtx_init(&map->mutex, mtx_plain) != thrd_success) {
        stats_free(map);
        stats_memory_release(memory);
        return stats_fail(error, MC_CLIENT_STATS_THREAD_FAILED, "Stats Map native mutex", NULL,
                          NULL, NULL, 0);
    }
    *out = map;
    return MC_CLIENT_STATS_OK;
}

mc_client_stats_status mc_client_stats_map_create(mc_registry_classes *registry,
                                                  const mc_client_stats_options *input,
                                                  mc_client_stats_map **out,
                                                  mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (registry == NULL || out == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats Map native arguments", NULL,
                   NULL, NULL, 0);
    else {
        const mc_client_stats_options options = stats_options(input);
        stats_memory *memory = stats_memory_create(options.limits.max_allocation_bytes);
        if (memory == NULL)
            stats_fail(&error, MC_CLIENT_STATS_ALLOCATION_FAILED, "Stats Map native memory owner",
                       NULL, NULL, NULL, 0);
        else {
            (void)stats_map_make(registry, memory, &options, out, &error);
            stats_memory_release(memory);
        }
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_map_retain(mc_client_stats_map *map,
                                                  mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (map == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats Map retain receiver", NULL,
                   NULL, NULL, 0);
    else {
        mtx_lock(&map->mutex);
        if (map->holds == SIZE_MAX)
            stats_fail(&error, MC_CLIENT_STATS_LIMIT, "Stats Map native hold limit", NULL, NULL,
                       NULL, 0);
        else
            ++map->holds;
        mtx_unlock(&map->mutex);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

void mc_client_stats_map_retire(mc_client_stats_map **owner) {
    if (owner == NULL || *owner == NULL)
        return;
    mc_client_stats_map *map = *owner;
    *owner = NULL;
    mtx_lock(&map->mutex);
    const bool last = --map->holds == 0;
    mtx_unlock(&map->mutex);
    if (!last)
        return;
    stats_entry *entry = map->head;
    while (entry != NULL) {
        stats_entry *next = entry->next;
        stats_free(entry);
        entry = next;
    }
    stats_memory *memory = map->memory;
    mtx_destroy(&map->mutex);
    stats_free(map);
    stats_memory_release(memory);
}

size_t mc_client_stats_map_size(const mc_client_stats_map *input) {
    if (input == NULL)
        return 0;
    mc_client_stats_map *map = (mc_client_stats_map *)input;
    mtx_lock(&map->mutex);
    const size_t count = map->count;
    mtx_unlock(&map->mutex);
    return count;
}

static stats_entry *find(const mc_client_stats_map *map, const mc_statistic *key) {
    for (stats_entry *entry = map->head; entry != NULL; entry = entry->next)
        if (entry->key == key)
            return entry;
    return NULL;
}

static mc_client_stats_status put(mc_client_stats_map *map, const mc_statistic *key, int32_t value,
                                  bool add, mc_client_stats_error *error) {
    if (map == NULL || !stats_statistic_valid(map->registry, key))
        return stats_fail(error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats Map exact key namespace",
                          NULL, NULL, NULL, 0);
    mtx_lock(&map->mutex);
    if (map->readers != 0) {
        mtx_unlock(&map->mutex);
        return stats_fail(error, MC_CLIENT_STATS_BUSY, "Stats Map active codec/worker read", NULL,
                          NULL, NULL, 0);
    }
    stats_entry *entry = find(map, key);
    if (entry != NULL) {
        if (add) {
            const uint32_t sum = (uint32_t)entry->value + (uint32_t)value;
            entry->value =
                sum <= INT32_MAX ? (int32_t)sum : INT32_MIN + (int32_t)(sum - UINT32_C(0x80000000));
        } else
            entry->value = value;
        mtx_unlock(&map->mutex);
        return MC_CLIENT_STATS_OK;
    }
    if (map->count >= map->options.limits.max_map_entries || map->count >= (size_t)INT32_MAX) {
        mtx_unlock(&map->mutex);
        return stats_fail(error, MC_CLIENT_STATS_LIMIT, "Stats Map native entry bound", NULL, NULL,
                          NULL, 0);
    }
    entry = stats_alloc(map->memory, sizeof(*entry), error);
    if (entry == NULL) {
        mtx_unlock(&map->mutex);
        return error->status;
    }
    *entry = (stats_entry){key, value, NULL};
    if (map->tail != NULL)
        map->tail->next = entry;
    else
        map->head = entry;
    map->tail = entry;
    ++map->count;
    mtx_unlock(&map->mutex);
    return MC_CLIENT_STATS_OK;
}

mc_client_stats_status mc_client_stats_map_put(mc_client_stats_map *map, const mc_statistic *key,
                                               int32_t value, mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    (void)put(map, key, value, false, &error);
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status stats_map_add(mc_client_stats_map *map, const mc_statistic *key,
                                     int32_t value, mc_client_stats_error *error) {
    return put(map, key, value, true, error);
}

mc_client_stats_status mc_client_stats_map_get(const mc_client_stats_map *input,
                                               const mc_statistic *key, bool *present,
                                               int32_t *value, mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (input == NULL || present == NULL || value == NULL ||
        !stats_statistic_valid(input->registry, key))
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats Map get arguments", NULL, NULL,
                   NULL, 0);
    else {
        mc_client_stats_map *map = (mc_client_stats_map *)input;
        mtx_lock(&map->mutex);
        const stats_entry *entry = find(map, key);
        *present = entry != NULL;
        *value = entry == NULL ? 0 : entry->value;
        mtx_unlock(&map->mutex);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status mc_client_stats_map_entry(const mc_client_stats_map *input, size_t index,
                                                 const mc_statistic **key, int32_t *value,
                                                 mc_client_stats_error *out_error) {
    mc_client_stats_error error;
    stats_error_clear(&error);
    if (input == NULL || key == NULL || value == NULL)
        stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats Map entry arguments", NULL,
                   NULL, NULL, 0);
    else {
        mc_client_stats_map *map = (mc_client_stats_map *)input;
        mtx_lock(&map->mutex);
        const stats_entry *entry = map->head;
        for (size_t i = 0; i < index && entry != NULL; ++i)
            entry = entry->next;
        if (entry == NULL)
            stats_fail(&error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats Map native entry index",
                       NULL, NULL, NULL, 0);
        else {
            *key = entry->key;
            *value = entry->value;
        }
        mtx_unlock(&map->mutex);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}

mc_client_stats_status stats_map_lease(const mc_client_stats_map *input,
                                       mc_client_stats_error *error) {
    mc_client_stats_map *map = (mc_client_stats_map *)input;
    mtx_lock(&map->mutex);
    if (map->readers == SIZE_MAX) {
        mtx_unlock(&map->mutex);
        return stats_fail(error, MC_CLIENT_STATS_LIMIT, "Stats Map native read count", NULL, NULL,
                          NULL, 0);
    }
    ++map->readers;
    mtx_unlock(&map->mutex);
    return MC_CLIENT_STATS_OK;
}

void stats_map_unlease(const mc_client_stats_map *input) {
    mc_client_stats_map *map = (mc_client_stats_map *)input;
    mtx_lock(&map->mutex);
    --map->readers;
    mtx_unlock(&map->mutex);
}

mc_client_stats_status stats_map_clone(const mc_client_stats_map *map, mc_client_stats_map **out,
                                       mc_client_stats_error *error) {
    mc_client_stats_status status = stats_map_lease(map, error);
    if (status != MC_CLIENT_STATS_OK)
        return status;
    mc_client_stats_map *copy = NULL;
    status = stats_map_make(map->registry, map->memory, &map->options, &copy, error);
    for (const stats_entry *entry = map->head; status == MC_CLIENT_STATS_OK && entry != NULL;
         entry = entry->next)
        status = put(copy, entry->key, entry->value, false, error);
    stats_map_unlease(map);
    if (status != MC_CLIENT_STATS_OK)
        mc_client_stats_map_retire(&copy);
    else
        *out = copy;
    return status;
}
