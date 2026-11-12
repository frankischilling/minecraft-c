/* Terminal native ownership only, entered after every method observation and
 * WorldAccess retirement. This is not Source gameplay or queue processing. */
static bool rd_terminal_status(const char *stage, mc_world_storage_status status,
                               const mc_world_storage_error *error) {
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    fprintf(stderr, "REDSTONE_TERMINAL stage=%s status=%d operation=%s system=%" PRIu32 "\n", stage,
            (int)status, error->operation == NULL ? "(null)" : error->operation,
            error->system_error);
    return false;
}

static bool rd_terminal_chunks(mc_world_runtime *world, mc_world_store *store,
                               mc_world_provider *dimension, mc_chunk_state **chunks) {
    mc_chunk_state *owners[RD_CHUNKS];
    size_t unique = 0;
    for (size_t i = 0; i < RD_CHUNKS; ++i) {
        CHECK(chunks[i] != NULL);
        size_t j = 0;
        while (j < unique && owners[j] != chunks[i])
            ++j;
        if (j == unique)
            owners[unique++] = chunks[i];
    }
    mc_world_storage_error error = {0};
    /* The public context getter rejects the terminal phase latched below. */
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(world);
    CHECK(acquisition != NULL && acquisition->cache != NULL && acquisition->pending != NULL);
    mc_chunk_cache *cache = acquisition->cache;
    mc_chunk_pending *pending = acquisition->pending;
    CHECK(rd_terminal_status("entity_containers",
                             mc_world_runtime_retire_entity_containers(world, &error), &error));
    mc_chunk_provider_fields provider = {0};
    /* End owned indexing before borrowing/mutating the exported descriptors. */
    CHECK(rd_terminal_status("provider_export",
                             mc_chunk_provider_bind_fields(world->fields.v, &provider, &error),
                             &error));
    CHECK(provider.world == world && provider.store == store && provider.cache == cache &&
          provider.pending == pending);
    const size_t store_count = mc_world_store_chunk_count(store);
    if (!mc_chunk_cache_validate(cache) || !mc_chunk_pending_validate(pending) ||
        cache->count != unique || store_count != unique) {
        fprintf(stderr,
                "REDSTONE_TERMINAL stage=descriptors cache=%zu insertions=%zu pending=%zu "
                "store=%zu loaded_unique=%zu\n",
                cache->count, cache->insertion_count, pending->count, store_count, unique);
        return false;
    }
    /* Every detached occurrence must be one of the actual loaded Store owners. */
    for (size_t i = 0; i < cache->count; ++i) {
        size_t j = 0;
        while (j < unique && owners[j] != cache->entries[i].chunk)
            ++j;
        CHECK(j < unique);
    }
    for (size_t i = 0; i < cache->insertion_count; ++i) {
        size_t j = 0;
        while (j < unique && owners[j] != cache->insertions[i])
            ++j;
        CHECK(j < unique);
    }
    for (size_t i = 0; i < unique; ++i) {
        bool found = false;
        for (size_t j = 0; j < cache->count; ++j)
            found = found || cache->entries[j].chunk == owners[i];
        CHECK(found);
    }
    CHECK(mc_chunk_pending_clear(pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                              cache->insertion_capacity));
    CHECK(cache->count == 0 && cache->insertion_count == 0 && pending->count == 0);
    mc_world_provider_unbind(dimension);
    const mc_world_storage_status retired = mc_world_runtime_retire_world(world, &error);
    if (!rd_terminal_status("world_retire", retired, &error)) {
        fprintf(stderr, "REDSTONE_TERMINAL cache=%zu insertions=%zu pending=%zu store=%zu\n",
                cache->count, cache->insertion_count, pending->count,
                mc_world_store_chunk_count(store));
        return false;
    }
    for (size_t i = 0; i < unique; ++i) {
        bool aliases[RD_CHUNKS];
        for (size_t j = 0; j < RD_CHUNKS; ++j)
            aliases[j] = chunks[j] == owners[i];
        CHECK(rd_terminal_status("store_chunk_retire",
                                 mc_world_store_retire_chunk(store, owners[i], &error), &error));
        for (size_t j = 0; j < RD_CHUNKS; ++j)
            if (aliases[j])
                chunks[j] = NULL;
    }
    CHECK(mc_world_store_chunk_count(store) == 0);
    return true;
}

static bool rd_terminal_world_free(mc_world_runtime *world) {
    mc_world_storage_error error = {0};
    return rd_terminal_status("world_free", mc_world_runtime_free(world, &error), &error);
}
