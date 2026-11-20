/* Terminal native cleanup after every observation has been emitted/retired.
 * Unlike the earlier Item331 four-owner helper, collect the actual complete
 * cache/list owner union; DB_CHUNKS is an observation roster, not a cache cap.
 * No pending process, source callback, save, tick, RNG or clock is run here. */
static bool rd_terminal_world_free(mc_world_runtime *world) {
    mc_world_storage_error error = {0};
    const mc_world_storage_status status = mc_world_runtime_free(world, &error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "DB_TERMINAL world-free status=%d operation=%s\n", (int)status,
                error.operation == NULL ? "(null)" : error.operation);
    return status == MC_WORLD_STORAGE_OK;
}

static bool rd_terminal_chunks(mc_world_runtime *world, mc_world_store *store,
                               mc_world_provider *dimension, mc_chunk_state **canonical) {
    mc_world_storage_error error = {0};
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(world);
    CHECK(acquisition != NULL && acquisition->cache != NULL && acquisition->pending != NULL);
    mc_chunk_cache *cache = acquisition->cache;
    mc_chunk_pending *pending = acquisition->pending;
    CHECK(mc_world_runtime_retire_entity_containers(world, &error) == MC_WORLD_STORAGE_OK);
    /* This mutable descriptor exposure is terminal, after all source work. */
    mc_chunk_provider_fields provider = {0};
    CHECK(mc_chunk_provider_bind_fields(world->fields.v, &provider, &error) == MC_WORLD_STORAGE_OK);
    CHECK(provider.world == world && provider.store == store && provider.cache == cache &&
          provider.pending == pending && mc_chunk_cache_validate(cache) &&
          mc_chunk_pending_validate(pending));
    CHECK(cache->count <= SIZE_MAX - cache->insertion_count);
    const size_t capacity = cache->count + cache->insertion_count;
    CHECK(capacity != 0 && capacity <= SIZE_MAX / sizeof(mc_chunk_state *));
    mc_chunk_state **owners = malloc(capacity * sizeof(*owners));
    CHECK(owners != NULL);
    size_t unique = 0;
    for (size_t i = 0; i < capacity; ++i) {
        mc_chunk_state *candidate =
            i < cache->count ? cache->entries[i].chunk : cache->insertions[i - cache->count];
        CHECK(candidate != NULL && mc_world_store_owns_chunk(store, candidate));
        size_t j = 0;
        while (j < unique && owners[j] != candidate)
            ++j;
        if (j == unique)
            owners[unique++] = candidate;
    }
    CHECK(mc_world_store_chunk_count(store) == unique);
    size_t canonical_owner[RD_CHUNKS];
    for (size_t i = 0; i < RD_CHUNKS; ++i) {
        size_t j = 0;
        while (j < unique && owners[j] != canonical[i])
            ++j;
        CHECK(j < unique);
        canonical_owner[i] = j;
    }
    CHECK(mc_chunk_pending_clear(pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                              cache->insertion_capacity));
    mc_world_provider_unbind(dimension);
    CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < unique; ++i) {
        CHECK(mc_world_store_retire_chunk(store, owners[i], &error) == MC_WORLD_STORAGE_OK);
        for (size_t j = 0; j < RD_CHUNKS; ++j)
            if (canonical_owner[j] == i)
                canonical[j] = NULL;
        owners[i] = NULL;
    }
    free(owners);
    CHECK(mc_world_store_chunk_count(store) == 0);
    return true;
}
