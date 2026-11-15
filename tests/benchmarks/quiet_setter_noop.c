/* Fixed-work resident quiet-setter opportunity. Each actual setter
 * receives the existing ID/metadata and must return false without changing
 * Source state. This measures no-op calls, not natural population or FPS. */
#define MC_PROVIDER_CACHE_SCOPE_ENTRY quiet_scope_unused_main
#include "provider_cache_scope.c"
#undef MC_PROVIDER_CACHE_SCOPE_ENTRY
#include "../../src/world/runtime/lighting_internal.h"
#include "../../src/world/runtime/mutation/mutation_internal.h"

/* Only this layout header sends its diagnostic rows to this sink.
 * Public Source/helper printf calls retain their original binding. */
static int qn_layout_sink(const char *format, ...) {
    (void)format;
    return 0;
}

#define QN_LAYOUT_REPORT qn_layout_sink
#include "quiet_setter_diagnostics.h"
#undef QN_LAYOUT_REPORT

enum { QN_CALLS = 8192, QN_SAMPLES = 7 };

typedef struct quiet_point {
    int32_t x, z, id, metadata;
} quiet_point;

/* Compare every byte except positions proven padding in the reviewed partial
 * layout. UNKNOWN includes unsupported children/union payloads and is compared. */
static bool qn_diagnostic_equal(const unit_layout *layout, const void *left, const void *right) {
    const unsigned char *a = left, *b = right;
    for (size_t i = 0; i < layout->size; ++i)
        if (layout->mask[i] != UNIT_PADDING && a[i] != b[i])
            return false;
    return true;
}

static uint64_t qn_diagnostic_hash(const unit_layout *layout, const void *raw) {
    const unsigned char *bytes = raw;
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < layout->size; ++i)
        if (layout->mask[i] != UNIT_PADDING)
            hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

static int quiet_run(source_save_fixture *s, unsigned int count, unsigned int depth,
                     bool metadata) {
    mc_world_runtime *world = s->game.worlds[0];
    mc_chunk_state *chunks[PS_MAX_CHUNKS] = {0};
    quiet_point points[PS_MAX_CHUNKS];
    mc_world_mutation_error *detail = malloc(sizeof(*detail));
    mc_world_mutation_error *expected_detail = malloc(sizeof(*expected_detail));
    VERIFY(detail != NULL && expected_detail != NULL);
    /* The reviewed mask is built outside every measured interval. Only the
     * exact 1,650 proven-padding positions are excluded, never UNKNOWN. */
    unit_layout diagnostic_layout = {.size = sizeof(*detail), .valid = 1};
    diagnostic_layout.mask = calloc(diagnostic_layout.size, sizeof(*diagnostic_layout.mask));
    diagnostic_layout.context = calloc(diagnostic_layout.size, sizeof(*diagnostic_layout.context));
    diagnostic_layout.member = calloc(diagnostic_layout.size, sizeof(*diagnostic_layout.member));
    VERIFY(diagnostic_layout.mask != NULL && diagnostic_layout.context != NULL &&
           diagnostic_layout.member != NULL);
    unit_prepare_layout(&diagnostic_layout);
    size_t diagnostic_named = 0, diagnostic_padding = 0, diagnostic_unknown = 0;
    for (size_t i = 0; i < diagnostic_layout.size; ++i) {
        diagnostic_named += diagnostic_layout.mask[i] == UNIT_NAMED;
        diagnostic_padding += diagnostic_layout.mask[i] == UNIT_PADDING;
        diagnostic_unknown += diagnostic_layout.mask[i] == UNIT_UNKNOWN;
    }
    const uint64_t diagnostic_layout_hash =
        ps_hash(UINT64_C(1469598103934665603), diagnostic_layout.mask, diagnostic_layout.size);
    VERIFY(diagnostic_layout.valid && diagnostic_layout.size == 24296 && diagnostic_named == 6298 &&
           diagnostic_padding == 1650 && diagnostic_unknown == 16348 &&
           diagnostic_named + diagnostic_padding + diagnostic_unknown == diagnostic_layout.size &&
           diagnostic_layout_hash == UINT64_C(8267465906294600077));
    /* Finite two-poison representation guard: all 22,646 retained bytes must
     * match, including every UNKNOWN byte. No result bytes are rewritten. */
    memset(expected_detail, 0xa5, sizeof(*expected_detail));
    memset(detail, 0x5a, sizeof(*detail));
    mc_world_mutation_error_init(expected_detail);
    mc_world_mutation_error_init(detail);
    VERIFY(qn_diagnostic_equal(&diagnostic_layout, expected_detail, detail));
    for (unsigned int i = 0; i < count; ++i) {
        int32_t x, z;
        ps_coordinates(i, &x, &z);
        VERIFY(
            mc_world_runtime_get_detailed(world, x, z, &chunks[i], NULL) == MC_CHUNK_ACQUIRE_OK &&
            chunks[i] != NULL && !chunks[i]->blank && chunks[i]->active && chunks[i]->data != NULL);
        const unsigned int lx = i & 15u, lz = (i * 7u) & 15u;
        const size_t offset = ((size_t)lx * 16u + lz) * 128u + 64u;
        points[i] = (quiet_point){.x = x * 16 + (int32_t)lx,
                                  .z = z * 16 + (int32_t)lz,
                                  .id = chunks[i]->data->blocks[offset],
                                  .metadata = chunks[i]->data->metadata[offset / 2u] & 15u};
    }
    /* Warm both setter bodies before snapshots and all observations. */
    for (unsigned int i = 0; i < count; ++i) {
        const quiet_point p = points[i];
        bool changed = true;
        memset(detail, 0x5a, sizeof(*detail));
        VERIFY(mc_world_runtime_block_set_quiet(world, p.x, 64, p.z, p.id, &changed, detail) ==
                   MC_WORLD_STORAGE_OK &&
               !changed && qn_diagnostic_equal(&diagnostic_layout, expected_detail, detail));
        changed = true;
        memset(detail, 0xa5, sizeof(*detail));
        VERIFY(mc_world_runtime_block_metadata_set_quiet(world, p.x, 64, p.z, p.id, p.metadata,
                                                         &changed, detail) == MC_WORLD_STORAGE_OK &&
               !changed && qn_diagnostic_equal(&diagnostic_layout, expected_detail, detail));
    }
    const uint64_t planes = ps_planes(s);
    VERIFY(planes != 0);
    unsigned char *states = malloc((size_t)count * sizeof(*chunks[0]));
    unsigned char *data = malloc((size_t)count * sizeof(*chunks[0]->data));
    VERIFY(states != NULL && data != NULL);
    for (unsigned int i = 0; i < count; ++i) {
        memcpy(states + (size_t)i * sizeof(*chunks[i]), chunks[i], sizeof(*chunks[i]));
        memcpy(data + (size_t)i * sizeof(*chunks[i]->data), chunks[i]->data,
               sizeof(*chunks[i]->data));
    }
    const mc_chunk_cache *cache = world->acquisition.cache;
    const mc_chunk_pending *pending = world->acquisition.pending;
    const size_t eb = cache->capacity * sizeof(*cache->entries);
    const size_t ib = cache->insertion_capacity * sizeof(*cache->insertions);
    const size_t pb = pending->capacity * sizeof(*pending->keys);
    unsigned char *backing = malloc(eb + ib + pb + 1);
    VERIFY(backing != NULL);
    memcpy(backing, cache->entries, eb);
    memcpy(backing + eb, cache->insertions, ib);
    memcpy(backing + eb + ib, pending->keys, pb);
    unsigned char wf[sizeof world->fields], rng[sizeof *world->fields.r];
    unsigned char cf[sizeof *cache], pf[sizeof *pending];
    unsigned char re[sizeof world->last_registry_error], we[sizeof world->last_error];
    memcpy(wf, &world->fields, sizeof wf);
    memcpy(rng, world->fields.r, sizeof rng);
    memcpy(cf, cache, sizeof cf);
    memcpy(pf, pending, sizeof pf);
    memcpy(re, &world->last_registry_error, sizeof re);
    memcpy(we, &world->last_error, sizeof we);
    const size_t active = world->active_calls, leases = world->local_leases;
    const uint64_t unique = atomic_load(&s->game.profile->uniquifier);
    const unsigned int holder = atomic_load(&s->game.profile->holder_phase);
    mc_java_math_random_owner *const math = s->game.profile->math;
    const uint64_t math_state = math == NULL ? 0 : atomic_load(&math->state);
    const bool math_cached = math != NULL && math->have_next_next_gaussian;
    uint64_t math_gaussian = 0;
    if (math != NULL)
        memcpy(&math_gaussian, &math->next_next_gaussian, sizeof math_gaussian);
    unsigned char classes[sizeof *world->classes], light_list[sizeof *world->fields.C];
    memcpy(classes, world->classes, sizeof classes);
    VERIFY(world->fields.C != NULL && world->fields.C->count <= world->fields.C->capacity);
    memcpy(light_list, world->fields.C, sizeof light_list);
    const size_t light_count = world->fields.C->count;
    const size_t light_bytes = world->fields.C->capacity * sizeof(*world->fields.C->items);
    unsigned char *light_items = malloc(light_bytes + 1);
    mc_world_light_region_fields *regions = calloc(light_count + 1, sizeof(*regions));
    VERIFY(light_items != NULL && regions != NULL);
    if (light_bytes != 0)
        memcpy(light_items, world->fields.C->items, light_bytes);
    for (size_t i = 0; i < light_count; ++i) {
        const mc_world_light_region *region = world->fields.C->items[i];
        VERIFY(region != NULL);
        regions[i] = region->fields;
    }
    mc_world_runtime_acquisition_error *const outer = world->acquisition_error;
    mc_world_runtime_lifecycle_error *const life = world->lifecycle_error;
    const mc_java_random_profile_status random_status = world->last_random_status;
    mc_world_storage_error hold;
    VERIFY(mc_world_runtime_call_enter(world, &hold) == MC_WORLD_STORAGE_OK);
    for (unsigned int i = 0; i < depth; ++i)
        mc_chunk_provider_cache_mutation_begin(world->fields.v);
    mc_chunk_provider_cache_observation before, after;
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &before) && before.owned &&
           before.raw_exports == 0 && before.mutation_depth == depth);
    uint64_t expected = UINT64_C(1469598103934665603);
    for (unsigned int i = 0; i < QN_CALLS; ++i)
        expected = bench_mix(expected, 0, i);
    for (unsigned int sample = 0; sample < QN_SAMPLES; ++sample) {
        uint64_t start, end, hz, ehz, hash = UINT64_C(1469598103934665603);
        unsigned int status = 0, changes = 0;
        bool changed = true;
        memset(detail, 0x5a, sizeof(*detail));
        VERIFY(bench_clock(&start, &hz));
        for (unsigned int i = 0; i < QN_CALLS; ++i) {
            const quiet_point p = points[(i * 73u + 17u) & (count - 1u)];
            const int32_t x = p.x + ((i & 31u) == 0 ? 524288 : 0);
            changed = true;
            status |=
                (unsigned int)(metadata ? mc_world_runtime_block_metadata_set_quiet(
                                              world, x, 64, p.z, p.id, p.metadata, &changed, detail)
                                        : mc_world_runtime_block_set_quiet(world, x, 64, p.z, p.id,
                                                                           &changed, detail));
            changes |= changed ? 1u : 0u;
            hash = bench_mix(hash, changed ? 1 : 0, i);
        }
        VERIFY(bench_clock(&end, &ehz) && hz == ehz && end > start && status == 0 && changes == 0 &&
               hash == expected && detail->status == MC_WORLD_STORAGE_OK &&
               detail->operation == NULL && detail->bytecode == 0 &&
               qn_diagnostic_equal(&diagnostic_layout, expected_detail, detail));
        printf("QUIET_NOOP_SAMPLE sample=%u calls=%u ticks=%" PRIu64 " frequency=%" PRIu64
               " checksum=%" PRIu64 " changes=%u\n",
               sample, QN_CALLS, end - start, hz, hash, changes);
    }
    const uint64_t calls = (uint64_t)QN_CALLS * QN_SAMPLES;
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &after) && after.owned &&
           after.mutation_depth == depth && after.hits == before.hits + (depth ? 0 : calls) &&
           after.generic_gets == before.generic_gets + (depth ? calls : 0) &&
           after.misses == before.misses && after.rebuilds == before.rebuilds &&
           after.raw_exports == before.raw_exports && after.map_count == before.map_count &&
           after.insertion_count == before.insertion_count &&
           after.direct_bytes == before.direct_bytes && after.index_bytes == before.index_bytes &&
           memcmp(wf, &world->fields, sizeof wf) == 0 &&
           memcmp(rng, world->fields.r, sizeof rng) == 0 && memcmp(cf, cache, sizeof cf) == 0 &&
           memcmp(pf, pending, sizeof pf) == 0 && memcmp(backing, cache->entries, eb) == 0 &&
           memcmp(backing + eb, cache->insertions, ib) == 0 &&
           memcmp(backing + eb + ib, pending->keys, pb) == 0 &&
           memcmp(re, &world->last_registry_error, sizeof re) == 0 &&
           memcmp(we, &world->last_error, sizeof we) == 0 &&
           world->last_random_status == random_status && world->active_calls == active + 1 &&
           world->local_leases == leases && world->acquisition_error == outer &&
           world->lifecycle_error == life && atomic_load(&s->game.profile->uniquifier) == unique &&
           atomic_load(&s->game.profile->holder_phase) == holder && s->game.profile->math == math &&
           (math == NULL || atomic_load(&math->state) == math_state) &&
           (math != NULL && math->have_next_next_gaussian) == math_cached &&
           memcmp(classes, world->classes, sizeof classes) == 0 &&
           memcmp(light_list, world->fields.C, sizeof light_list) == 0 && ps_planes(s) == planes &&
           mc_world_store_chunk_count(s->game.stores[0]) == count);
    uint64_t math_after = 0;
    if (math != NULL)
        memcpy(&math_after, &math->next_next_gaussian, sizeof math_after);
    VERIFY(math_gaussian == math_after &&
           (light_bytes == 0 || memcmp(light_items, world->fields.C->items, light_bytes) == 0));
    for (size_t i = 0; i < light_count; ++i) {
        const mc_world_light_region *region = world->fields.C->items[i];
        VERIFY(region != NULL && region->fields.a == regions[i].a &&
               memcmp(&region->fields.bounds, &regions[i].bounds, sizeof regions[i].bounds) == 0);
    }
    for (unsigned int i = 0; i < count; ++i)
        VERIFY(memcmp(states + (size_t)i * sizeof(*chunks[i]), chunks[i], sizeof(*chunks[i])) ==
                   0 &&
               memcmp(data + (size_t)i * sizeof(*chunks[i]->data), chunks[i]->data,
                      sizeof(*chunks[i]->data)) == 0 &&
               mc_world_store_owns_chunk(s->game.stores[0], chunks[i]));
    for (unsigned int i = depth; i != 0; --i)
        mc_chunk_provider_cache_mutation_end(world->fields.v);
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &after) && after.owned &&
           after.mutation_depth == 0 && after.rebuilds == before.rebuilds + (depth ? 1 : 0));
    mc_world_runtime_call_leave(world);
    VERIFY(world->active_calls == active && world->local_leases == leases);
    printf("QUIET_NOOP_INVARIANTS count=%u depth=%u metadata=%u calls=%" PRIu64 " planes=%" PRIu64
           " checksum=%" PRIu64 " diagnostic=%" PRIu64 " diagnostic_compared_bytes=%zu"
           " diagnostic_excluded_padding=%zu diagnostic_named_compared=%zu"
           " diagnostic_unknown_compared=%zu diagnostic_layout=%" PRIu64
           " source_state=1 scopes=1 ownership=1\n",
           count, depth, metadata ? 1u : 0u, calls, planes, expected,
           qn_diagnostic_hash(&diagnostic_layout, expected_detail),
           diagnostic_layout.size - diagnostic_padding, diagnostic_padding, diagnostic_named,
           diagnostic_unknown, diagnostic_layout_hash);
    free(diagnostic_layout.member);
    free(diagnostic_layout.context);
    free(diagnostic_layout.mask);
    free(backing);
    free(regions);
    free(light_items);
    free(data);
    free(states);
    free(detail);
    free(expected_detail);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 5);
    const unsigned int count = strcmp(argv[2], "8") == 0     ? 8u
                               : strcmp(argv[2], "256") == 0 ? 256u
                                                             : 0u;
    const unsigned int depth = strcmp(argv[3], "0") == 0 ? 0u : strcmp(argv[3], "1") == 0 ? 1u : 2u;
    const bool metadata = strcmp(argv[4], "metadata") == 0;
    VERIFY(count != 0 && depth < 2 && (metadata || strcmp(argv[4], "block") == 0));
    printf("QUIET_NOOP_CONTEXT count=%u depth=%u metadata=%u calls=%u samples=%u "
           "mutation_error_bytes=%zu acquisition_error_bytes=%zu\n",
           count, depth, metadata ? 1u : 0u, QN_CALLS, QN_SAMPLES, sizeof(mc_world_mutation_error),
           sizeof(mc_world_runtime_acquisition_error));
    source_save_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(s, argv[1]) == 0 && bench_open(s) == 0 &&
           quiet_run(s, count, depth, metadata) == 0 && bench_stop(s) == 0 && ss_finish(s) == 0);
    printf("QUIET_NOOP_FINISH checks=%u cleanup=1 genuine_source_world=1\n", checks);
    free(s);
    return 0;
}
