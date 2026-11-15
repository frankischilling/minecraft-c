/* One fixed workload object links unchanged against baseline/candidate libs.
 * State snapshots, plane hashing, assertions, scopes and cleanup are unclocked.
 * Timed GET loops include coordinate generation, status and checksum folds. */
#define MC_WORLD_QUERIES_ENTRY mc_provider_scope_unused_queries_main
#include "world_queries.c"
#undef MC_WORLD_QUERIES_ENTRY

enum { PS_CALLS = 65536, PS_SAMPLES = 7, PS_MAX_CHUNKS = 256 };

static void ps_coordinates(unsigned int index, int32_t *x, int32_t *z) {
    /* Sparse real source records: no complete population-neighbor square. */
    *x = (int32_t)(index & 15u) * 2;
    *z = (int32_t)(index >> 4u) * 2;
}

static uint64_t ps_hash(uint64_t hash, const void *raw, size_t length) {
    const unsigned char *bytes = raw;
    for (size_t i = 0; i < length; ++i)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

static uint64_t ps_planes(source_save_fixture *s) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < mc_world_store_chunk_count(s->game.stores[0]); ++i) {
        mc_chunk_state *c = NULL;
        if (mc_world_store_chunk_at(s->game.stores[0], i, &c, NULL) != MC_WORLD_STORAGE_OK ||
            c == NULL || c->blank || c->data == NULL)
            return 0;
        const int64_t fields[] = {c->x,
                                  c->z,
                                  c->dirty,
                                  c->active,
                                  c->terrain_populated,
                                  c->never_save,
                                  c->has_entities,
                                  c->last_save_time,
                                  c->data->minimum_height};
        hash = ps_hash(hash, fields, sizeof fields);
        hash = ps_hash(hash, c->data->blocks, MC_CHUNK_BLOCKS);
        hash = ps_hash(hash, c->data->metadata, MC_CHUNK_NIBBLE_BYTES);
        hash = ps_hash(hash, c->data->skylight, MC_CHUNK_NIBBLE_BYTES);
        hash = ps_hash(hash, c->data->block_light, MC_CHUNK_NIBBLE_BYTES);
        hash = ps_hash(hash, c->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    }
    return hash;
}

static int ps_run(source_save_fixture *s, unsigned int count, unsigned int depth) {
    mc_world_runtime *world = s->game.worlds[0];
    mc_chunk_state *chunks[PS_MAX_CHUNKS] = {0};
    mc_world_runtime_acquisition_error *detail = malloc(sizeof(*detail));
    unsigned char *poison = malloc(sizeof(*detail));
    VERIFY(detail != NULL && poison != NULL);
    for (unsigned int i = 0; i < count; ++i) {
        int32_t x, z;
        ps_coordinates(i, &x, &z);
        VERIFY(mc_world_runtime_get_detailed(world, x, z, &chunks[i], detail) ==
                   MC_CHUNK_ACQUIRE_OK &&
               chunks[i] != NULL && chunks[i]->active && !chunks[i]->blank && chunks[i]->x == x &&
               chunks[i]->z == z && mc_world_store_owns_chunk(s->game.stores[0], chunks[i]));
    }
    mc_chunk_provider_fields fields;
    VERIFY(mc_chunk_provider_internal_fields(world->fields.v, &fields, NULL) ==
               MC_WORLD_STORAGE_OK &&
           fields.world == world && fields.cache == world->acquisition.cache &&
           mc_world_store_chunk_count(s->game.stores[0]) == count);
    mc_chunk_cache *cache = fields.cache;
    mc_chunk_pending *pending = fields.pending;
    VERIFY(mc_chunk_cache_validate(cache) && mc_chunk_pending_validate(pending) &&
           cache->count == count && cache->insertion_count == count);
    const uint64_t planes = ps_planes(s);
    VERIFY(planes != 0);
    unsigned char *chunk_states = malloc((size_t)count * sizeof(*chunks[0]));
    unsigned char *chunk_data = malloc((size_t)count * sizeof(*chunks[0]->data));
    VERIFY(chunk_states != NULL && chunk_data != NULL);
    for (unsigned int i = 0; i < count; ++i) {
        memcpy(chunk_states + (size_t)i * sizeof(*chunks[i]), chunks[i], sizeof(*chunks[i]));
        memcpy(chunk_data + (size_t)i * sizeof(*chunks[i]->data), chunks[i]->data,
               sizeof(*chunks[i]->data));
    }
    const size_t entry_bytes = cache->capacity * sizeof(*cache->entries);
    const size_t insertion_bytes = cache->insertion_capacity * sizeof(*cache->insertions);
    const size_t pending_bytes = pending->capacity * sizeof(*pending->keys);
    unsigned char *backing = malloc(entry_bytes + insertion_bytes + pending_bytes + 1);
    VERIFY(backing != NULL);
    memcpy(backing, cache->entries, entry_bytes);
    memcpy(backing + entry_bytes, cache->insertions, insertion_bytes);
    memcpy(backing + entry_bytes + insertion_bytes, pending->keys, pending_bytes);
    unsigned char world_fields[sizeof world->fields], random[sizeof *world->fields.r];
    unsigned char cache_fields[sizeof *cache], pending_fields[sizeof *pending];
    memcpy(world_fields, &world->fields, sizeof world_fields);
    memcpy(random, world->fields.r, sizeof random);
    memcpy(cache_fields, cache, sizeof cache_fields);
    memcpy(pending_fields, pending, sizeof pending_fields);
    const mc_world_storage_error last = world->last_error;
    unsigned char registry[sizeof world->last_registry_error];
    memcpy(registry, &world->last_registry_error, sizeof registry);
    const mc_java_random_profile_status random_status = world->last_random_status;
    const uint64_t unique = atomic_load(&s->game.profile->uniquifier);
    const unsigned int holder = atomic_load(&s->game.profile->holder_phase);
    mc_java_math_random_owner *const math = s->game.profile->math;
    const uint64_t math_state = math == NULL ? 0 : atomic_load(&math->state);
    const bool math_cached = math != NULL && math->have_next_next_gaussian;
    uint64_t math_gaussian = 0;
    if (math != NULL)
        memcpy(&math_gaussian, &math->next_next_gaussian, sizeof math_gaussian);
    const size_t active = world->active_calls, leases = world->local_leases;
    mc_world_runtime_acquisition_error *const outer = world->acquisition_error;
    mc_world_runtime_lifecycle_error *const lifecycle = world->lifecycle_error;
    VERIFY(active == 0 && leases == 0);
    mc_world_storage_error hold;
    VERIFY(mc_world_runtime_call_enter(world, &hold) == MC_WORLD_STORAGE_OK);
    for (unsigned int i = 0; i < depth; ++i)
        mc_chunk_provider_cache_mutation_begin(world->fields.v);
    /* Every key is qualified by exact identity before timing. Packed aliases
     * exercise the same original key rule without loading another Chunk. */
    memset(detail, 0xa5, sizeof(*detail));
    memcpy(poison, detail, sizeof(*detail));
    for (unsigned int i = 0; i < count; ++i) {
        int32_t x, z;
        mc_chunk_state *got = NULL;
        ps_coordinates(i, &x, &z);
        VERIFY(mc_world_runtime_get_failure(world, x, z, &got, detail) == MC_CHUNK_ACQUIRE_OK &&
               got == chunks[i] &&
               mc_world_runtime_get_failure(world, x + 32768, z, &got, detail) ==
                   MC_CHUNK_ACQUIRE_OK &&
               got == chunks[i] && memcmp(poison, detail, sizeof(*detail)) == 0);
    }
    mc_chunk_provider_cache_observation before, after;
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &before) && before.owned &&
           before.mutation_depth == depth && before.raw_exports == 0);
    uint64_t expected = UINT64_C(1469598103934665603);
    uint64_t expected_identity = UINT64_C(1469598103934665603);
    for (unsigned int i = 0; i < PS_CALLS; ++i) {
        const unsigned int point = (i * 73u + 17u) & (count - 1u);
        expected = bench_mix(bench_mix(expected, chunks[point]->x, i), chunks[point]->z, i);
        expected_identity = (expected_identity ^ ((uint64_t)(uintptr_t)chunks[point] + i)) *
                            UINT64_C(1099511628211);
    }
    for (unsigned int sample = 0; sample < PS_SAMPLES; ++sample) {
        uint64_t start, end, hz, end_hz;
        uint64_t hash = UINT64_C(1469598103934665603), identity = hash;
        unsigned int status = 0;
        mc_chunk_state *got = &world->fallback;
        VERIFY(bench_clock(&start, &hz));
        for (unsigned int i = 0; i < PS_CALLS; ++i) {
            const unsigned int point = (i * 73u + 17u) & (count - 1u);
            int32_t x, z;
            ps_coordinates(point, &x, &z);
            if ((i & 31u) == 0)
                x += 32768;
            status |= (unsigned int)mc_world_runtime_get_failure(world, x, z, &got, detail);
            hash = bench_mix(bench_mix(hash, got->x, i), got->z, i);
            identity = (identity ^ ((uint64_t)(uintptr_t)got + i)) * UINT64_C(1099511628211);
        }
        VERIFY(bench_clock(&end, &end_hz) && hz == end_hz && end > start && status == 0 &&
               hash == expected && identity == expected_identity &&
               memcmp(poison, detail, sizeof(*detail)) == 0);
        printf("PROVIDER_SCOPE_SAMPLE sample=%u calls=%u ticks=%" PRIu64 " frequency=%" PRIu64
               " checksum=%" PRIu64 " identity_guard=1\n",
               sample, PS_CALLS, end - start, hz, hash);
    }
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &after) && after.owned &&
           after.mutation_depth == depth && after.map_count == count &&
           after.insertion_count == count &&
           after.generic_gets == before.generic_gets + (uint64_t)PS_SAMPLES * PS_CALLS &&
           after.hits == before.hits && after.misses == before.misses &&
           after.rebuilds == before.rebuilds && after.raw_exports == before.raw_exports &&
           after.index_bytes == before.index_bytes && after.direct_bytes == before.direct_bytes &&
           world->active_calls == active + 1 && world->local_leases == leases &&
           world->acquisition_error == outer && world->lifecycle_error == lifecycle);
    VERIFY(memcmp(world_fields, &world->fields, sizeof world_fields) == 0 &&
           memcmp(random, world->fields.r, sizeof random) == 0 &&
           memcmp(cache_fields, cache, sizeof cache_fields) == 0 &&
           memcmp(pending_fields, pending, sizeof pending_fields) == 0 &&
           memcmp(backing, cache->entries, entry_bytes) == 0 &&
           memcmp(backing + entry_bytes, cache->insertions, insertion_bytes) == 0 &&
           memcmp(backing + entry_bytes + insertion_bytes, pending->keys, pending_bytes) == 0 &&
           world->last_error.status == last.status &&
           world->last_error.operation == last.operation &&
           world->last_error.system_error == last.system_error &&
           memcmp(registry, &world->last_registry_error, sizeof registry) == 0 &&
           world->last_random_status == random_status &&
           atomic_load(&s->game.profile->uniquifier) == unique &&
           atomic_load(&s->game.profile->holder_phase) == holder && s->game.profile->math == math &&
           (math == NULL || atomic_load(&math->state) == math_state) &&
           (math != NULL && math->have_next_next_gaussian) == math_cached &&
           mc_world_store_chunk_count(s->game.stores[0]) == count && ps_planes(s) == planes);
    uint64_t after_gaussian = 0;
    if (math != NULL)
        memcpy(&after_gaussian, &math->next_next_gaussian, sizeof after_gaussian);
    VERIFY(after_gaussian == math_gaussian);
    for (unsigned int i = 0; i < count; ++i)
        VERIFY(memcmp(chunk_states + (size_t)i * sizeof(*chunks[i]), chunks[i],
                      sizeof(*chunks[i])) == 0 &&
               memcmp(chunk_data + (size_t)i * sizeof(*chunks[i]->data), chunks[i]->data,
                      sizeof(*chunks[i]->data)) == 0 &&
               mc_world_store_owns_chunk(s->game.stores[0], chunks[i]));
    for (unsigned int i = depth; i != 0; --i) {
        mc_chunk_provider_cache_mutation_end(world->fields.v);
        VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &after) && after.owned &&
               after.mutation_depth == i - 1 &&
               after.rebuilds == before.rebuilds + (i == 1 ? 1u : 0u));
    }
    mc_world_runtime_call_leave(world);
    VERIFY(world->active_calls == active && world->local_leases == leases);
    printf("PROVIDER_SCOPE_INVARIANTS count=%u depth=%u calls=%u planes=%" PRIu64
           " checksum=%" PRIu64 " exact_identities=1 source_fields=1 random=1 cache=1 pending=1 "
           "diagnostics=1 sentinels=1 ownership=1 scopes=1 generic_dispatch=1\n",
           count, depth, PS_SAMPLES * PS_CALLS, planes, expected);
    free(chunk_data);
    free(chunk_states);
    free(backing);
    free(poison);
    free(detail);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 4);
    const unsigned int count = strcmp(argv[2], "8") == 0     ? 8u
                               : strcmp(argv[2], "64") == 0  ? 64u
                               : strcmp(argv[2], "256") == 0 ? 256u
                                                             : 0u;
    const unsigned int depth = strcmp(argv[3], "1") == 0 ? 1u : strcmp(argv[3], "2") == 0 ? 2u : 0u;
    VERIFY(count != 0 && depth != 0);
    printf("PROVIDER_SCOPE_CONTEXT count=%u depth=%u calls=%u samples=%u pointer_bits=%zu\n", count,
           depth, PS_CALLS, PS_SAMPLES, sizeof(void *) * CHAR_BIT);
    source_save_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(s, argv[1]) == 0 && bench_open(s) == 0 &&
           ps_run(s, count, depth) == 0 && bench_stop(s) == 0 && ss_finish(s) == 0);
    printf("PROVIDER_SCOPE_FINISH checks=%u cleanup=1 genuine_source_world=1\n", checks);
    free(s);
    return 0;
}
