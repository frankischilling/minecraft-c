/* Optional genuine EW getter benchmark. Construction, assertions, hashes,
 * failure injection, captures and cleanup are outside timed calls. */
#define MC_WORLD_QUERIES_ENTRY mc_ew_cpu_unused_world_benchmark
#include "world_queries.c"
#undef MC_WORLD_QUERIES_ENTRY
#include "chunk_cache_metadata_adapter.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/chunk_cache_internal.h"

enum { EW_METADATA_CPU_CALLS = 65536, EW_METADATA_CPU_POINTS = 1024, EW_METADATA_CPU_SAMPLES = 7 };

typedef struct ew_cpu_point {
    int32_t x, y, z, expected;
} ew_cpu_point;

static uint64_t ew_hash(uint64_t h, const void *raw, size_t length) {
    const unsigned char *p = raw;
    for (size_t i = 0; i < length; ++i)
        h = (h ^ p[i]) * UINT64_C(1099511628211);
    return h;
}

static uint64_t ew_planes(source_save_fixture *s) {
    uint64_t h = UINT64_C(1469598103934665603);
    const size_t count = mc_world_store_chunk_count(s->game.stores[0]);
    for (size_t i = 0; i < count; ++i) {
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
                                  c->last_save_time};
        h = ew_hash(h, fields, sizeof(fields));
        h = ew_hash(h, c->data->blocks, MC_CHUNK_BLOCKS);
        h = ew_hash(h, c->data->metadata, MC_CHUNK_NIBBLE_BYTES);
        h = ew_hash(h, c->data->skylight, MC_CHUNK_NIBBLE_BYTES);
        h = ew_hash(h, c->data->block_light, MC_CHUNK_NIBBLE_BYTES);
        h = ew_hash(h, c->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    }
    return h;
}

static bool ew_text_equal(const char *a, const char *b) {
    return a == b || (a != NULL && b != NULL && strcmp(a, b) == 0);
}

static bool ew_provider_equal(const mc_chunk_provider_cache_observation *a,
                              const mc_chunk_provider_cache_observation *b) {
    return a->owned == b->owned && a->map_count == b->map_count &&
           a->insertion_count == b->insertion_count && a->index_capacity == b->index_capacity &&
           a->index_bytes == b->index_bytes && a->mutation_depth == b->mutation_depth &&
           a->direct_bytes == b->direct_bytes && a->hits == b->hits && a->misses == b->misses &&
           a->generic_gets == b->generic_gets && a->raw_exports == b->raw_exports &&
           a->rebuilds == b->rebuilds;
}

#include "chunk_cache_error_equal.inc"

static int ew_control(const ew_cpu_adapter *a, const char *name, mc_client_chunk_cache *cache,
                      int32_t x, int32_t y, int32_t z, bool null_out,
                      mc_client_chunk_cache_status wanted, int32_t wanted_value) {
    unsigned char *owner_before = cache == NULL ? NULL : malloc(sizeof(*cache));
    VERIFY(cache == NULL || owner_before != NULL);
    if (cache != NULL)
        memcpy(owner_before, cache, sizeof(*cache));
    mc_client_chunk_cache_error *reference = malloc(sizeof(*reference));
    mc_client_chunk_cache_error *actual = malloc(sizeof(*actual));
    unsigned char *sentinel = malloc(sizeof(*actual));
    VERIFY(reference != NULL && actual != NULL && sentinel != NULL);
    memset(reference, 0x5a, sizeof(*reference));
    memset(actual, 0xa5, sizeof(*actual));
    memcpy(sentinel, actual, sizeof(*actual));
    int32_t public_value = 777, value = 777;
    const mc_client_chunk_cache_status public_status =
        mc_client_chunk_cache_metadata(cache, x, y, z, null_out ? NULL : &public_value, reference);
    VERIFY(cache == NULL || memcmp(owner_before, cache, sizeof(*cache)) == 0);
    const mc_client_chunk_cache_status status =
        a->get(cache, x, y, z, null_out ? NULL : &value, actual);
    VERIFY(cache == NULL || memcmp(owner_before, cache, sizeof(*cache)) == 0);

    VERIFY(status == public_status && status == wanted && value == public_value &&
           value == wanted_value &&
           (status != MC_CLIENT_CHUNK_CACHE_OK || a->full_success_error
                ? ew_error_equal(actual, reference)
                : memcmp(actual, sentinel, sizeof(*actual)) == 0));
    if (strcmp(name, "entered-point") != 0)
        printf("EW_METADATA_CPU_CONTROL name=%s status=%d value=%" PRId32
               " method=%s descriptor=%s pc=%u child=%u native=%d nested=%zu "
               "operation=%s exact-public-failure=%d\n",
               name, (int)status, value, reference->method == NULL ? "NULL" : reference->method,
               reference->descriptor == NULL ? "NULL" : reference->descriptor, reference->bytecode,
               reference->child_bytecode, (int)reference->native_policy, reference->nested_count,
               reference->operation == NULL ? "NULL" : reference->operation,
               status != MC_CLIENT_CHUNK_CACHE_OK);
    free(owner_before);
    free(sentinel);
    free(actual);
    free(reference);
    return 0;
}

/* Same genuine EW object and complete public-error comparator as the shared
 * benchmark. All authored corruption/restoration is outside timed calls. */
static int ew_controls(const ew_cpu_adapter *a, mc_client_chunk_cache *cache) {
    VERIFY(ew_control(a, "null-cache", NULL, 0, 64, 0, false,
                      MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT, 777) == 0 &&
           ew_control(a, "null-out", cache, 0, 64, 0, true, MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT,
                      777) == 0 &&
           ew_control(a, "null-out-before-y", cache, 0, -1, 0, true,
                      MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT, 777) == 0);
    cache->state.active = true;
    VERIFY(ew_control(a, "active", cache, 0, 64, 0, false, MC_CLIENT_CHUNK_CACHE_BUSY, 777) == 0 &&
           ew_control(a, "active-before-y", cache, 0, -1, 0, false, MC_CLIENT_CHUNK_CACHE_BUSY,
                      777) == 0);
    cache->state.active = false;
    cache->state.retiring = true;
    VERIFY(ew_control(a, "retiring", cache, 0, 64, 0, false, MC_CLIENT_CHUNK_CACHE_BUSY, 777) == 0);
    cache->state.retiring = false;
    mc_world_chunk *chunk = cache->occurrences[4].object;
    VERIFY(chunk != NULL && chunk->e != NULL && chunk->e->a != NULL);
    mc_world_chunk_nibble *n = chunk->e;
    const size_t index = 64u / 2u;
    const int32_t expected = n->a[index] & 15u;
    mc_chunk_data *data = chunk->data;
    chunk->data = NULL;
    VERIFY(ew_control(a, "data-unread", cache, 0, 64, 0, false, MC_CLIENT_CHUNK_CACHE_OK,
                      expected) == 0);
    chunk->data = data;
    chunk->e = NULL;
    VERIFY(ew_control(a, "null-nibble", cache, 0, 64, 0, false, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                      777) == 0);
    const bool blank = cache->occurrences[4].state->blank;
    cache->occurrences[4].state->blank = true;
    VERIFY(ew_control(a, "blank-before-null-nibble", cache, 0, 64, 0, false,
                      MC_CLIENT_CHUNK_CACHE_OK, 0) == 0);
    cache->occurrences[4].state->blank = blank;
    chunk->e = n;
    uint8_t *backing = n->a;
    n->a = NULL;
    VERIFY(ew_control(a, "null-nibble-backing", cache, 0, 64, 0, false,
                      MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 777) == 0);
    n->a = backing;
    const size_t length = n->length;
    n->length = index;
    VERIFY(ew_control(a, "short-nibble-backing", cache, 0, 64, 0, false,
                      MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, 777) == 0);
    n->length = index + 1u;
    VERIFY(ew_control(a, "last-nibble-even", cache, 0, 64, 0, false, MC_CLIENT_CHUNK_CACHE_OK,
                      expected) == 0 &&
           ew_control(a, "last-nibble-odd", cache, 0, 65, 0, false, MC_CLIENT_CHUNK_CACHE_OK,
                      n->a[index] >> 4u) == 0);
    n->length = length;
    mc_chunk_state *const *const *rows = cache->state.c;
    cache->state.c = NULL;
    VERIFY(ew_control(a, "null-outer", cache, 0, 64, 0, false, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                      777) == 0 &&
           ew_control(a, "y-before-null", cache, 0, -1, 0, false, MC_CLIENT_CHUNK_CACHE_OK, 0) ==
               0);
    cache->state.c = rows;
    static const int32_t positions[][3] = {
        {-17, 64, 0},       {32, 64, 0},        {0, 64, -17},       {0, 64, 32},
        {INT32_MIN, 64, 0}, {INT32_MAX, 64, 0}, {0, 64, INT32_MIN}, {0, 64, INT32_MAX}};
    for (size_t i = 0; i < sizeof(positions) / sizeof(*positions); ++i)
        VERIFY(ew_control(a, "signed-array-index", cache, positions[i][0], positions[i][1],
                          positions[i][2], false, MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, 777) == 0);
    VERIFY(
        ew_control(a, "y-low", cache, 0, INT32_MIN, 0, false, MC_CLIENT_CHUNK_CACHE_OK, 0) == 0 &&
        ew_control(a, "y-high", cache, 0, INT32_MAX, 0, false, MC_CLIENT_CHUNK_CACHE_OK, 0) == 0);
    mc_client_chunk_cache *prefix = NULL;
    VERIFY(mc_client_chunk_cache_create(NULL, 0, 0, 0, 15, 127, 15, &prefix, NULL) ==
               MC_CLIENT_CHUNK_CACHE_SOURCE_NULL &&
           prefix != NULL &&
           ew_control(a, "failed-constructor-null-cell", prefix, 0, 64, 0, false,
                      MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 777) == 0 &&
           mc_client_chunk_cache_retire(&prefix, NULL) == MC_CLIENT_CHUNK_CACHE_OK);
    return 0;
}

static int ew_benchmark(source_save_fixture *s, bool measure) {
    const ew_cpu_adapter *a = ew_cpu_adapter_get();
    VERIFY(a != NULL && a->get != NULL);
    mc_world_runtime *world = s->game.worlds[0];
    mc_client_chunk_cache *cache = NULL;
    VERIFY(mc_client_chunk_cache_create(world, -16, 0, -16, 31, 127, 31, &cache, NULL) ==
               MC_CLIENT_CHUNK_CACHE_OK &&
           cache != NULL && cache->state.acquired == 9 && cache->state.leased == 9 &&
           cache->state.returned && cache->cell_count == 9);
    /* Authored finite CPU inputs, before snapshots/timing: all 16 nibbles
     * vary across the actual captured metadata backing; no fake getter. */
    for (size_t cell = 0; cell < cache->cell_count; ++cell) {
        mc_world_chunk_nibble *n = cache->occurrences[cell].object->e;
        VERIFY(n != NULL && n->a != NULL && n->length == MC_CHUNK_NIBBLE_BYTES);
        for (size_t byte = 0; byte < n->length; ++byte)
            n->a[byte] = (uint8_t)(byte * 37u + cell * 13u);
    }
    const uint64_t before_control_planes = ew_planes(s);
    unsigned char *before_control_world = malloc(sizeof(*world));
    VERIFY(before_control_world != NULL);
    memcpy(before_control_world, world, sizeof(*world));
    ew_cpu_point *points = malloc(EW_METADATA_CPU_POINTS * sizeof(*points));
    int32_t *values = malloc(EW_METADATA_CPU_CALLS * sizeof(*values));
    mc_client_chunk_cache_error *error = malloc(sizeof(*error));
    unsigned char *error_sentinel = malloc(sizeof(*error));
    VERIFY(points != NULL && values != NULL && error != NULL && error_sentinel != NULL);
    for (unsigned int i = 0; i < EW_METADATA_CPU_POINTS; ++i) {
        ew_cpu_point *p = &points[i];
        const unsigned int cx = (i * 5u) % 3u, cz = (i * 7u / 3u) % 3u;
        p->x = ((int32_t)cx - 1) * 16 + (int32_t)((i * 13u) & 15u);
        p->z = ((int32_t)cz - 1) * 16 + (int32_t)((i * 11u) & 15u);
        p->y = 8 + (int32_t)((i * 17u) % 104u);
        const size_t cell = ((size_t)((uint32_t)p->x & 15u) << 11u) |
                            ((size_t)((uint32_t)p->z & 15u) << 7u) | (uint32_t)p->y;
        const mc_world_chunk_nibble *n = cache->occurrences[cx * 3u + cz].object->e;
        p->expected = (int32_t)((n->a[cell / 2u] >> (((uint32_t)p->y & 1u) * 4u)) & 15u);
        VERIFY(ew_control(a, "entered-point", cache, p->x, p->y, p->z, false,
                          MC_CLIENT_CHUNK_CACHE_OK, p->expected) == 0);
    }
    VERIFY(ew_controls(a, cache) == 0);
    VERIFY(ew_planes(s) == before_control_planes &&
           memcmp(before_control_world, world, sizeof(*world)) == 0);
    free(before_control_world);

    const uint64_t planes = ew_planes(s);
    const uint64_t coordinates =
        ew_hash(UINT64_C(1469598103934665603), points, EW_METADATA_CPU_POINTS * sizeof(*points));
    mc_client_chunk_cache snapshot;
    memcpy(&snapshot, cache, sizeof(snapshot));
    mc_client_chunk_cache_occurrence occurrences[9];
    memcpy(occurrences, cache->occurrences, sizeof(occurrences));
    unsigned char *world_snapshot = malloc(sizeof(*world));
    VERIFY(world_snapshot != NULL && planes != 0);
    memcpy(world_snapshot, world, sizeof(*world));
    const size_t bindings = world->persistent_bindings, active = world->active_calls,
                 leases = world->local_leases;
    mc_chunk_provider_cache_observation before, after;
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &before));
    const uint64_t unique = atomic_load(&s->game.profile->uniquifier);
    unsigned char random[sizeof(*world->fields.r)];
    memcpy(random, world->fields.r, sizeof(random));
    printf("EW_METADATA_CPU_CONTEXT adapter=%s success-error=%s error-bytes=%zu calls=%u points=%u "
           "samples=%u chunks=9 planes=%" PRIu64 " coordinates=%" PRIu64
           " public-init-per-call=1 public-copy-per-call=1 public-nominal-bytes-per-sample=%zu\n",
           a->name, a->full_success_error ? "full" : "untouched", sizeof(*error),
           EW_METADATA_CPU_CALLS, EW_METADATA_CPU_POINTS, measure ? EW_METADATA_CPU_SAMPLES : 0u,
           planes, coordinates, (size_t)EW_METADATA_CPU_CALLS * sizeof(*error) * 2u);
    for (unsigned int sample = 0; measure && sample < EW_METADATA_CPU_SAMPLES; ++sample) {
        memset(error, 0xa5, sizeof(*error));
        memcpy(error_sentinel, error, sizeof(*error));
        uint64_t start, end, hz, end_hz;
        const bool started = bench_clock(&start, &hz);
        mc_client_chunk_cache_status status = MC_CLIENT_CHUNK_CACHE_OK;
        unsigned int completed = 0;
        for (; completed < EW_METADATA_CPU_CALLS; ++completed) {
            const ew_cpu_point *p = &points[completed % EW_METADATA_CPU_POINTS];
            status = a->get(cache, p->x, p->y, p->z, &values[completed], error);
            if (status != MC_CLIENT_CHUNK_CACHE_OK)
                break;
        }
        const bool ended = bench_clock(&end, &end_hz);
        VERIFY(started && ended && status == MC_CLIENT_CHUNK_CACHE_OK &&
               completed == EW_METADATA_CPU_CALLS && end > start && hz == end_hz);
        for (unsigned int i = 0; i < EW_METADATA_CPU_CALLS; ++i)
            VERIFY(values[i] == points[i % EW_METADATA_CPU_POINTS].expected);
        VERIFY(a->full_success_error || memcmp(error, error_sentinel, sizeof(*error)) == 0);
        const uint64_t result =
            ew_hash(UINT64_C(1469598103934665603), values, EW_METADATA_CPU_CALLS * sizeof(*values));
        VERIFY(ew_planes(s) == planes && memcmp(cache, &snapshot, sizeof(snapshot)) == 0 &&
               memcmp(cache->occurrences, occurrences, sizeof(occurrences)) == 0 &&
               memcmp(world, world_snapshot, sizeof(*world)) == 0 &&
               world->persistent_bindings == bindings && world->active_calls == active &&
               world->local_leases == leases &&
               memcmp(world->fields.r, random, sizeof(random)) == 0 &&
               atomic_load(&s->game.profile->uniquifier) == unique &&
               mc_chunk_provider_cache_observe(world->fields.v, &after) &&
               ew_provider_equal(&before, &after));
        printf("EW_METADATA_CPU_SAMPLE sample=%u ticks=%" PRIu64 " frequency=%" PRIu64
               " calls=%u result=%" PRIu64 " planes=%" PRIu64 " owner-same=1 "
               "world-same=1 random-same=1 holds-same=1 acquisition-gets=0\n",
               sample, end - start, hz, completed, result, planes);
    }
    VERIFY(mc_client_chunk_cache_retire(&cache, NULL) == MC_CLIENT_CHUNK_CACHE_OK &&
           cache == NULL && world->persistent_bindings + 1u == bindings);
    free(world_snapshot);
    free(error_sentinel);
    free(error);
    free(values);
    free(points);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && (strcmp(argv[2], "measure") == 0 || strcmp(argv[2], "controls") == 0));
    source_save_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(s, argv[1]) == 0 && bench_open(s) == 0);
    VERIFY(ew_benchmark(s, strcmp(argv[2], "measure") == 0) == 0 && bench_stop(s) == 0 &&
           ss_finish(s) == 0);
    printf("EW_METADATA_CPU_FINISH checks=%u cleanup=1 genuine-world=1 genuine-ew=1 gl=0\n",
           checks);
    free(s);
    return 0;
}
