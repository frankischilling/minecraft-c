#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/lighting.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/biome/biome_internal.h"
#include "../../../src/world/runtime/generation/generation_internal.h"
#include "../../../src/world/runtime/provider_internal.h"
#include "../../../src/world/storage/files.h"
#include "../../../src/world/storage/generated_internal.h"
#include "../../support/java_random.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define ARTIFACT_BYTES (MC_CHUNK_BLOCKS + 3u * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES)
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Constructed native World/store contracts. The binary artifact is observed
 * output, independently checked against the retained original fixture by the
 * parent driver. No queue trace, palette, constructor entropy or expected
 * terrain is manufactured here. Every provider cache remains genuinely empty. */
static unsigned int checks;
static mc_math_trig trig;
static uint8_t first_snapshot[ARTIFACT_BYTES];
static uint8_t climate_snapshot[3u * 256u * 8u];
static uint8_t registry_snapshot[256u * 4u + 13u * 2u];
_Static_assert(ARTIFACT_BYTES == 82176u, "complete observed Chunk arrays");
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct byte_stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} byte_stream;

typedef struct resource_source {
    byte_stream streams[3];
    mc_registry_resources api;
    mc_registry_classes *registry;
    mc_world_runtime *world;
    mc_world_dimension *dimension;
    mc_world_dimension_biome *captured;
    size_t opens, closes, guarded;
    bool fail, rebind, rebound, bad;
} resource_source;

typedef struct retained_chunk {
    mc_chunk_state *state;
    mc_world_store *store;
} retained_chunk;

typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *stores[4];
    mc_world_info_name *name;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry[3];
    mc_world_runtime_classes *classes[3];
    resource_source resources[3];
    mc_world_runtime *worlds[6];
    mc_world_dimension *dimensions[6];
    mc_world_provider *providers[6];
    mc_world_provider_fields fields[6];
    retained_chunk children[16];
    mc_world_store_reservation *reservation;
    const mc_biome **changed_table;
    const mc_biome *old_cell;
    size_t changed_index;
} fixture;

static uint64_t bits(double value)
{
    uint64_t out;
    _Static_assert(sizeof(out) == sizeof(value), "binary64 required");
    memcpy(&out, &value, sizeof(out));
    return out;
}

static uint64_t profile_u(const fixture *f)
{
    return atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
}

static bool zeros(const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; ++i)
        if (data[i] != 0)
            return false;
    return true;
}

static bool same_registry_cause(const mc_registry_error *a, const mc_registry_error *b)
{
    return a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static bool remember(fixture *f, mc_world_store *store, mc_chunk_state *state)
{
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state == state)
            return f->children[i].store == store;
        if (f->children[i].state == NULL) {
            f->children[i] = (retained_chunk){state, store};
            return true;
        }
    }
    return false;
}

/* Remember actual ownership before any following assertion can fail. Error
 * diagnostics expose a child only when the entry's checked retirement failed. */
static mc_world_storage_status generate(fixture *f, size_t world, int32_t x, int32_t z,
                                        mc_chunk_state **out, mc_world_generation_error *error)
{
    const mc_world_storage_status status =
        mc_world_provider_generate_game(f->providers[world], x, z, out, error);
    if (status == MC_WORLD_STORAGE_OK) {
        if (!remember(f, f->fields[world].store, *out))
            f->resources[0].bad = true;
    } else if (error->retained_child != NULL) {
        if (error->retained_store == NULL ||
            !remember(f, error->retained_store, error->retained_child))
            f->resources[0].bad = true;
    }
    return status;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
    byte_stream *s = context;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17)
        take = 17;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out)
{
    resource_source *s = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || s->opens >= COUNT(names) ||
        strcmp(name, names[s->opens]) != 0) {
        s->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const size_t index = s->opens++;
    if (s->fail || s->rebind) {
        /* Actual synchronous callback, while generation holds its World and
         * sampling holds its original receiver. Retirement must not detach it. */
        if (s->world == NULL ||
            mc_world_runtime_retire_world(s->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_free(s->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_registry_classes_free(s->registry, NULL) != MC_REGISTRY_BUSY)
            s->bad = true;
        ++s->guarded;
    }
    if (s->fail)
        return MC_REGISTRY_IO_FAILED;
    if (s->rebind && !s->rebound) {
        mc_world_dimension_fields before, after;
        mc_world_biome_game_fields game;
        if (mc_world_dimension_fields_get(s->dimension, &before) != MC_WORLD_DIMENSION_OK ||
            before.a != s->world || before.b != s->captured ||
            mc_world_dimension_bind(s->dimension, s->world, NULL) != MC_WORLD_DIMENSION_OK ||
            mc_world_dimension_fields_get(s->dimension, &after) != MC_WORLD_DIMENSION_OK ||
            after.a != s->world || after.b == before.b ||
            mc_world_dimension_biome_game_fields_get(after.b, &game) != MC_WORLD_DIMENSION_OK ||
            game.registry != NULL || game.d != NULL)
            s->bad = true;
        s->rebound = true;
    }
    *out = (mc_java_properties_input){&s->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input)
{
    resource_source *s = context;
    if (input == NULL || input->context != &s->streams[2] || input->read != read_bytes) {
        s->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++s->closes;
    s->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input)
{
    resource_source *s = context;
    if (input == NULL || input->read != read_bytes)
        s->bad = true;
    return false;
}

static bool open_store(fixture *f, size_t slot, const char *root, const char *suffix, size_t chunks,
                       size_t bytes)
{
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const int n = snprintf(path, sizeof(path), "%s%s", root, suffix);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 127,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = chunks;
    options.limits.nbt.max_allocation_bytes = bytes;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool create_world(fixture *f, size_t slot, size_t registry, size_t store)
{
    bool ok = false;
    mc_world_runtime_fields world;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 16;
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->classes[registry], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[store], .name = f->name, .seed = 23, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_fields_get(f->worlds[slot], &world) == MC_WORLD_STORAGE_OK &&
          world.v != NULL);
    f->providers[slot] = world.v;
    CHECK(mc_world_provider_bind_fields(world.v, &f->fields[slot], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    const mc_world_provider_fields *p = &f->fields[slot];
    CHECK(p->world == f->worlds[slot] && p->store == f->stores[store] && p->generation != NULL &&
          p->generator != NULL && p->generator->origin == MC_NORMAL_GENERATOR_PROVIDER &&
          p->generator->cave_range == 8 && p->manager == p->generator->biome_manager &&
          p->generation->v == NULL && p->generation->q.data == NULL &&
          p->generation->d.data == NULL && p->generation->e.data == NULL &&
          p->generation->f.data == NULL && p->generation->g.data == NULL &&
          p->generation->h.data == NULL && p->generation->r.length == 256 &&
          p->generation->s.length == 256 && p->generation->t.length == 256 &&
          mc_chunk_cache_validate(p->cache) && p->cache->count == 0 &&
          p->cache->insertion_count == 0);
    ok = true;
cleanup:
    return ok;
}

static void snapshot(const mc_chunk_data *data, uint8_t *out)
{
    memcpy(out, data->blocks, MC_CHUNK_BLOCKS);
    out += MC_CHUNK_BLOCKS;
    memcpy(out, data->metadata, MC_CHUNK_NIBBLE_BYTES);
    out += MC_CHUNK_NIBBLE_BYTES;
    memcpy(out, data->skylight, MC_CHUNK_NIBBLE_BYTES);
    out += MC_CHUNK_NIBBLE_BYTES;
    memcpy(out, data->block_light, MC_CHUNK_NIBBLE_BYTES);
    out += MC_CHUNK_NIBBLE_BYTES;
    memcpy(out, data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
}

static bool write_observation(const char *root, const char *leaf, const uint8_t *observed,
                              size_t length)
{
    bool ok = false;
    char *path = NULL;
    mc_ws_output output = {0};
    CHECK(mc_ws_path_join(root, leaf, &path, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_output_open(path, length, &output, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_output_write(&output, observed, length, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(output.remaining == 0);
    CHECK(mc_ws_output_close(&output, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    if (output.file != NULL && mc_ws_output_close(&output, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    free(path);
    return ok;
}

static bool scratch_ready(const mc_world_provider_generation_fields *g)
{
    return g->q.data != NULL && g->q.length == 425 && g->d.data != NULL && g->d.length == 425 &&
           g->e.data != NULL && g->e.length == 425 && g->f.data != NULL && g->f.length == 425 &&
           g->g.data != NULL && g->g.length == 25 && g->h.data != NULL && g->h.length == 25;
}

static bool check_success(fixture *f, const char *root)
{
    bool ok = false, leased = false;
    mc_chunk_state *a = NULL, *b = NULL, *c = NULL;
    mc_biome_reference_array *warm = NULL;
    mc_world_generation_error error;
    mc_world_chunk_fields fields;
    mc_world_runtime_fields world;
    mc_world_light_list_view queue;
    mc_world_biome_game_fields game;
    mc_world_dimension_fields dimension;
    mc_world_dimension_biome_fields manager;
    mc_level_info info;
    mc_registry_block_view blocks;
    mc_biome_registry_view biomes;
    mc_world_provider_fields *p = &f->fields[0];
    CHECK(mc_world_runtime_read_info(f->worlds[0], &info, NULL) == MC_WORLD_STORAGE_OK &&
          info.seed == 23);
    CHECK(mc_world_dimension_fields_get(f->dimensions[0], &dimension) == MC_WORLD_DIMENSION_OK &&
          dimension.a == f->worlds[0] && dimension.b == p->biome && !dimension.e &&
          mc_world_dimension_biome_fields_get(dimension.b, &manager) == MC_WORLD_DIMENSION_OK &&
          !manager.fixed && manager.normal == p->manager);
    CHECK(mc_world_dimension_biome_sample(p->biome, 0, 0, 16, 16, NULL, &warm, NULL) ==
              MC_WORLD_DIMENSION_OK &&
          warm != NULL);
    CHECK(f->resources[0].opens == 3 && f->resources[0].closes == 1 && !f->resources[0].bad);
    CHECK(mc_world_dimension_biome_game_fields_get(p->biome, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == NULL);
    CHECK(p->generation->v == NULL && p->manager->a.length == 256 && p->manager->b.length == 256 &&
          p->manager->c.length == 256 && p->manager->d.data == NULL);
    CHECK(mc_registry_classes_inspect_blocks(f->registry[0], &blocks, NULL) == MC_REGISTRY_OK &&
          blocks.phase == MC_REGISTRY_READY && blocks.q != NULL &&
          mc_biome_registry_inspect(f->registry[0], &biomes, NULL) == MC_REGISTRY_OK &&
          biomes.phase == MC_REGISTRY_READY);
    for (size_t i = 0; i < 256; ++i) {
        const uint32_t raw = (uint32_t)blocks.q[i];
        for (size_t byte = 0; byte < 4; ++byte)
            registry_snapshot[i * 4u + byte] = (uint8_t)(raw >> (24u - (unsigned int)byte * 8u));
    }
    const mc_biome *const actual[] = {biomes.a, biomes.b, biomes.c, biomes.d, biomes.e,
                                      biomes.f, biomes.g, biomes.h, biomes.i, biomes.j,
                                      biomes.k, biomes.l, biomes.m};
    for (size_t i = 0; i < COUNT(actual); ++i) {
        const mc_biome_fields *const fields_now = mc_biome_fields_get(actual[i]);
        CHECK(fields_now != NULL);
        registry_snapshot[1024u + 2u * i] = (uint8_t)fields_now->p;
        registry_snapshot[1024u + 2u * i + 1u] = (uint8_t)fields_now->q;
    }
    CHECK(write_observation(root, "provider-generation-registry.bin", registry_snapshot,
                            sizeof(registry_snapshot)));
    const double *const channels[] = {p->manager->a.data, p->manager->b.data, p->manager->c.data};
    for (size_t channel = 0; channel < COUNT(channels); ++channel) {
        for (size_t i = 0; i < 256; ++i) {
            const uint64_t raw = bits(channels[channel][i]);
            for (size_t byte = 0; byte < 8; ++byte)
                climate_snapshot[(channel * 256u + i) * 8u + byte] =
                    (uint8_t)(raw >> (56u - (unsigned int)byte * 8u));
        }
    }
    /* Only observed inputs are exported; the driver admits the original data
     * comparison only after all three actual input channels match its fixture. */
    CHECK(write_observation(root, "provider-generation-climate.bin", climate_snapshot,
                            sizeof(climate_snapshot)));
    CHECK(mc_world_runtime_fields_get(f->worlds[0], &world) == MC_WORLD_STORAGE_OK);
    const mc_java_random saved_world = *world.r;
    mc_java_random_reseed(&p->generator->generator_random, 7);
    mc_java_random_reseed(&p->generator->cave_random, 11);
    (void)mc_java_random_next_gaussian(&p->generator->generator_random);
    (void)mc_java_random_next_gaussian(&p->generator->cave_random);
    const uint64_t generator_cache = bits(p->generator->generator_random.next_next_gaussian);
    const uint64_t cave_cache = bits(p->generator->cave_random.next_next_gaussian);
    CHECK(p->generator->generator_random.have_next_next_gaussian &&
          p->generator->cave_random.have_next_next_gaussian);
    const uint64_t u = profile_u(f);
    CHECK(generate(f, 0, 1, 0, &a, &error) == MC_WORLD_STORAGE_OK);
    CHECK(a != NULL && error.status == MC_WORLD_STORAGE_OK && error.retained_child == NULL &&
          error.retained_store == NULL && error.cleanup.status == MC_WORLD_STORAGE_OK);
    mc_world_chunk *const object = mc_world_store_generated_chunk(f->stores[0], a);
    CHECK(object != NULL && mc_world_chunk_state(object) == a &&
          mc_world_store_owns_chunk(f->stores[0], a));
    CHECK(mc_world_chunk_fields_get(object, &fields) == MC_WORLD_STORAGE_OK &&
          fields.d == f->worlds[0] && fields.b == a->data->blocks &&
          fields.h == a->data->heightmap && fields.i == 66 && fields.j == 1 && fields.k == 0 &&
          fields.o && !fields.c && !fields.n && !fields.p && !fields.q && fields.r == 0);
    CHECK(a->x == 1 && a->z == 0 && !a->blank && a->dirty && !a->active &&
          a->data->blocks_capacity == MC_CHUNK_BLOCKS &&
          zeros(a->data->metadata, MC_CHUNK_NIBBLE_BYTES) &&
          zeros(a->data->block_light, MC_CHUNK_NIBBLE_BYTES));
    CHECK(p->generation->v != NULL && mc_biome_reference_array_length(p->generation->v) == 256 &&
          scratch_ready(p->generation) && p->manager->a.length == 256 &&
          p->manager->b.length == 256 && p->manager->c.length == 256 &&
          p->manager->d.data == NULL &&
          mc_world_dimension_biome_game_fields_get(p->biome, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == NULL);
    CHECK(mc_world_runtime_light_list_get(f->worlds[0], &queue, NULL) == MC_WORLD_STORAGE_OK &&
          queue.count == 0);
    CHECK(p->cache->count == 0 && p->cache->insertion_count == 0 && profile_u(f) == u &&
          mc_test_java_random_equal(world.r, &saved_world));
    /* Literal observed seed23/(1,0) continuations from normal_fixtures.json,
     * SHA cd712382a736a394e6d30a11e776b9b585b051faecf32da4bc0a96bd7492b000.
     * Existing fixture comparison separately checks all observed array bytes. */
    CHECK(p->generator->generator_random.state == UINT64_C(0x92b432c8f60c) &&
          p->generator->cave_random.state == UINT64_C(0x15a523df41a5));
    CHECK(bits(p->generator->generator_random.next_next_gaussian) == generator_cache &&
          !p->generator->generator_random.have_next_next_gaussian &&
          bits(p->generator->cave_random.next_next_gaussian) == cave_cache &&
          !p->generator->cave_random.have_next_next_gaussian);
    mc_java_random continuation = p->generator->generator_random;
    CHECK((uint64_t)mc_java_random_next_i64(&continuation) == UINT64_C(0x413767af1401f6b7));
    continuation = p->generator->cave_random;
    CHECK((uint64_t)mc_java_random_next_i64(&continuation) == UINT64_C(0x8a8175257de3a530));
    snapshot(a->data, first_snapshot);
    CHECK(write_observation(root, "provider-generation-first.bin", first_snapshot,
                            sizeof(first_snapshot)));
    const mc_biome_reference_array *const v = p->generation->v;
    const double *const q = p->generation->q.data;
    const double *const d = p->generation->d.data;
    CHECK(generate(f, 0, 0, 0, &b, &error) == MC_WORLD_STORAGE_OK && b != a && b->data != a->data);
    CHECK(generate(f, 0, 1, 0, &c, &error) == MC_WORLD_STORAGE_OK && c != a && c != b &&
          c->data != a->data && c->data->blocks != a->data->blocks);
    CHECK(p->generation->v == v && p->generation->q.data == q && p->generation->d.data == d &&
          p->cache->count == 0 && p->cache->insertion_count == 0 &&
          mc_world_store_chunk_count(f->stores[0]) == 3);
    /* Same-kernel repeat consistency and earlier-result retention, not a new
     * original observation or a fabricated queue/trace expectation. */
    uint8_t current[ARTIFACT_BYTES];
    snapshot(c->data, current);
    CHECK(memcmp(current, first_snapshot, sizeof(current)) == 0);
    snapshot(a->data, current);
    CHECK(memcmp(current, first_snapshot, sizeof(current)) == 0 && profile_u(f) == u);
    CHECK(mc_world_store_lease_chunk(f->stores[0], a, NULL) == MC_WORLD_STORAGE_OK);
    leased = true;
    CHECK(mc_world_store_retire_chunk(f->stores[0], a, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(f->stores[0], a, NULL) == MC_WORLD_STORAGE_OK);
    leased = false;
    CHECK(mc_world_store_has_generated_for_world(f->stores[0], f->worlds[0]) &&
          mc_world_runtime_free(f->worlds[0], NULL) == MC_WORLD_STORAGE_BUSY);
    ok = true;
cleanup:
    if (leased)
        (void)mc_world_store_release_chunk(f->stores[0], a, NULL);
    return ok;
}

static bool clean_failure(const mc_world_generation_error *error)
{
    return error->retained_child == NULL && error->retained_store == NULL &&
           error->cleanup.status == MC_WORLD_STORAGE_OK;
}

static bool check_array_captures(void)
{
    bool ok = false;
    mc_biome_memory memory = {0};
    mc_biome_double_buffer array = {0};
    mc_biome_double_reference first = {0}, second = {0}, empty = {0};
    CHECK(mc_biome_memory_init(&memory, SIZE_MAX) == MC_BIOME_OK);
    CHECK(mc_biome_double_capture(&array, &empty) == MC_BIOME_OK && empty.data == NULL &&
          empty.length == 0 && empty.allocation == NULL && memory.live_allocation_bytes == 0);
    mc_biome_double_release(&empty);
    CHECK(mc_biome_double_buffer_resize(&memory, &array, 4) == MC_BIOME_OK);
    array.data[0] = 0.25;
    array.data[3] = -0.0;
    const size_t old_charge = memory.live_allocation_bytes;
    const double *old = array.data;
    CHECK(old_charge > 4u * sizeof(double));
    CHECK(mc_biome_double_capture(&array, &first) == MC_BIOME_OK && first.data == old &&
          first.length == 4);
    CHECK(mc_biome_double_capture(&array, &second) == MC_BIOME_OK && second.data == old &&
          second.allocation == first.allocation && memory.live_allocation_bytes == old_charge);
    CHECK(mc_biome_double_buffer_resize(&memory, &array, 8) == MC_BIOME_OK && array.data != old);
    const size_t both = memory.live_allocation_bytes;
    const size_t new_charge = both - old_charge;
    CHECK(new_charge > 8u * sizeof(double) && first.data[0] == 0.25 &&
          bits(second.data[3]) == UINT64_C(0x8000000000000000));
    CHECK(mc_biome_memory_init(&memory, SIZE_MAX) == MC_BIOME_INVALID_ARGUMENT &&
          memory.live_allocation_bytes == both);
    mc_biome_double_buffer_free(&array);
    CHECK(array.data == NULL && array.length == 0 && memory.live_allocation_bytes == old_charge &&
          first.data[0] == 0.25 && bits(second.data[3]) == UINT64_C(0x8000000000000000));
    mc_biome_double_release(&first);
    CHECK(first.data == NULL && first.length == 0 && first.allocation == NULL &&
          memory.live_allocation_bytes == old_charge && second.data[0] == 0.25);
    CHECK(mc_biome_memory_init(&memory, both) == MC_BIOME_INVALID_ARGUMENT);
    mc_biome_double_release(&second);
    CHECK(second.data == NULL && second.length == 0 && second.allocation == NULL &&
          memory.live_allocation_bytes == 0);
    mc_biome_double_release(&second);
    CHECK(mc_biome_memory_init(&memory, both - 1u) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&memory, &array, 4) == MC_BIOME_OK);
    array.data[0] = 0.5;
    old = array.data;
    CHECK(mc_biome_double_capture(&array, &first) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&memory, &array, 8) == MC_BIOME_LIMIT &&
          array.data == old && array.length == 4 && array.data[0] == 0.5 && first.data == old &&
          first.length == 4 && first.data[0] == 0.5 && memory.live_allocation_bytes == old_charge &&
          memory.max_allocation_bytes == both - 1u);
    mc_biome_double_buffer_free(&array);
    CHECK(first.data[0] == 0.5 && memory.live_allocation_bytes == old_charge);
    mc_biome_double_release(&first);
    CHECK(memory.live_allocation_bytes == 0);
    CHECK(mc_biome_memory_init(&memory, both) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&memory, &array, 4) == MC_BIOME_OK);
    CHECK(mc_biome_double_capture(&array, &first) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&memory, &array, 8) == MC_BIOME_OK &&
          memory.live_allocation_bytes == both && first.length == 4 && array.length == 8);
    mc_biome_double_release(&first);
    CHECK(memory.live_allocation_bytes == new_charge);
    mc_biome_double_buffer_free(&array);
    CHECK(memory.live_allocation_bytes == 0);
    ok = true;
cleanup:
    mc_biome_double_release(&empty);
    mc_biome_double_release(&first);
    mc_biome_double_release(&second);
    mc_biome_double_buffer_free(&array);
    return ok;
}

static bool check_limits(fixture *f, const char *root)
{
    bool ok = false;
    mc_chunk_state *const sentinel = f->children[0].state;
    mc_chunk_state *out = sentinel;
    mc_world_generation_error error;
    const mc_world_chunk *const first = mc_world_store_generated_chunk(f->stores[0], sentinel);
    const size_t bytes = mc_world_chunk_allocation_bytes(first);
    CHECK(bytes > MC_CHUNK_BLOCKS);
    CHECK(open_store(f, 1, root, "/count", 1, mc_nbt_default_limits().max_allocation_bytes));
    CHECK(open_store(f, 2, root, "/short", 1, bytes - 1));
    CHECK(open_store(f, 3, root, "/exact", 1, bytes));
    CHECK(create_world(f, 3, 0, 1) && create_world(f, 4, 0, 2) && create_world(f, 5, 0, 3));
    CHECK(mc_world_store_reserve_chunk(f->stores[1], 0, &f->reservation, NULL) ==
          MC_WORLD_STORAGE_OK);
    mc_normal_generator *const generator = f->fields[3].generator;
    mc_java_random_reseed(&generator->generator_random, 19);
    (void)mc_java_random_next_gaussian(&generator->generator_random);
    const uint64_t payload = bits(generator->generator_random.next_next_gaussian);
    mc_java_random expected;
    mc_java_random_seed(&expected,
                        INT64_C(341873128712)); /* Actual source j.setSeed argument at (1,0). */
    CHECK(generate(f, 3, 1, 0, &out, &error) == MC_WORLD_STORAGE_LIMIT && out == sentinel &&
          clean_failure(&error));
    CHECK(generator->generator_random.state == expected.state &&
          bits(generator->generator_random.next_next_gaussian) == payload &&
          !generator->generator_random.have_next_next_gaussian &&
          f->fields[3].generation->v == NULL && f->fields[3].generation->q.data == NULL &&
          mc_world_store_chunk_count(f->stores[1]) == 0 &&
          mc_world_store_reservation_count(f->stores[1]) == 1);
    CHECK(mc_world_store_cancel_reservation(f->stores[1], f->reservation, NULL) ==
          MC_WORLD_STORAGE_OK);
    f->reservation = NULL;
    CHECK(generate(f, 3, 1, 0, &out, &error) == MC_WORLD_STORAGE_OK && out != sentinel);
    mc_chunk_state *const occupied = out;
    out = sentinel;
    CHECK(generate(f, 3, 0, 0, &out, &error) == MC_WORLD_STORAGE_LIMIT && out == sentinel &&
          clean_failure(&error));
    CHECK(mc_world_store_chunk_count(f->stores[1]) == 1 &&
          mc_world_store_owns_chunk(f->stores[1], occupied));
    CHECK(generate(f, 4, 1, 0, &out, &error) == MC_WORLD_STORAGE_LIMIT && out == sentinel &&
          clean_failure(&error));
    CHECK(mc_world_store_chunk_count(f->stores[2]) == 0 && f->fields[4].generation->v == NULL &&
          f->fields[4].generator->generator_random.state == expected.state);
    CHECK(generate(f, 5, 1, 0, &out, &error) == MC_WORLD_STORAGE_OK && out != sentinel);
    CHECK(mc_world_chunk_allocation_bytes(mc_world_store_generated_chunk(f->stores[3], out)) ==
          bytes);
    out = sentinel;
    const uint64_t u = profile_u(f);
    CHECK(mc_world_provider_generate_game(NULL, 0, 0, &out, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out == sentinel);
    CHECK(mc_world_provider_generate_game(f->providers[0], 0, 0, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(profile_u(f) == u);
    ok = true;
cleanup:
    return ok;
}

static bool check_resource_failure(fixture *f)
{
    bool ok = false;
    mc_chunk_state *const sentinel = f->children[0].state;
    mc_chunk_state *out = sentinel;
    mc_world_generation_error error, first;
    CHECK(create_world(f, 1, 1, 0));
    resource_source *s = &f->resources[1];
    s->world = f->worlds[1];
    const size_t count = mc_world_store_chunk_count(f->stores[0]);
    CHECK(generate(f, 1, 1, 0, &out, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out == sentinel);
    CHECK(error.bytecode == 65 && error.registry.status == MC_REGISTRY_IO_FAILED &&
          error.biome.sampling.registry.status == MC_REGISTRY_IO_FAILED && clean_failure(&error));
    CHECK(s->opens == 1 && s->closes == 0 && s->guarded == 1 && !s->bad &&
          mc_world_store_chunk_count(f->stores[0]) == count && f->fields[1].generation->v == NULL &&
          f->fields[1].generation->q.data == NULL && f->fields[1].generation->d.data == NULL &&
          error.biome.sampling.array != NULL && error.biome.sampling.cells_completed == 0);
    first = error;
    CHECK(generate(f, 1, 1, 0, &out, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out == sentinel);
    CHECK(error.registry.status == MC_REGISTRY_FAILED_INITIALIZATION && error.bytecode == 65 &&
          same_registry_cause(&error.registry, &first.registry) &&
          same_registry_cause(&error.biome.sampling.registry, &first.biome.sampling.registry) &&
          clean_failure(&error));
    CHECK(s->opens == 1 && s->guarded == 1 && mc_world_store_chunk_count(f->stores[0]) == count);
    ok = true;
cleanup:
    return ok;
}

static bool check_wrapped_scheduler(fixture *f)
{
    bool ok = false;
    mc_chunk_state *out = f->children[0].state;
    mc_world_generation_error error;
    const uint64_t u = profile_u(f);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 23);
    (void)mc_java_random_next_i64(&seeded);
    (void)mc_java_random_next_i64(&seeded);
    const uint64_t payload = bits(f->fields[0].generator->cave_random.next_next_gaussian);
    /* At INT_MIN the wrapped lower outer endpoint exceeds its upper endpoint:
     * no neighbor body, but the actual seed and two longs are already reached. */
    CHECK(generate(f, 0, INT32_MIN, 0, &out, &error) == MC_WORLD_STORAGE_OK &&
          out != f->children[0].state && out->x == INT32_MIN && out->z == 0);
    CHECK(f->fields[0].generator->cave_random.state == seeded.state &&
          bits(f->fields[0].generator->cave_random.next_next_gaussian) == payload &&
          !f->fields[0].generator->cave_random.have_next_next_gaussian);
    const size_t count = mc_world_store_chunk_count(f->stores[0]);
    out = f->children[0].state;
    /* Wrapped outer loop would continue forever; its inner Z range is empty.
     * This exercises the explicit finite-work policy without heavy neighbors. */
    CHECK(generate(f, 0, INT32_MAX - 8, INT32_MIN, &out, &error) == MC_WORLD_STORAGE_LIMIT &&
          out == f->children[0].state && error.bytecode == 119 &&
          error.cave_scheduler_bytecode == 123 && clean_failure(&error));
    CHECK(mc_world_store_chunk_count(f->stores[0]) == count &&
          scratch_ready(f->fields[0].generation) &&
          f->fields[0].generator->cave_random.state == seeded.state &&
          bits(f->fields[0].generator->cave_random.next_next_gaussian) == payload &&
          !f->fields[0].generator->cave_random.have_next_next_gaussian && profile_u(f) == u &&
          f->fields[0].cache->count == 0 && f->fields[0].cache->insertion_count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool check_late_climate(fixture *f)
{
    bool ok = false;
    mc_chunk_state *out = f->children[0].state;
    mc_world_generation_error error;
    mc_world_dimension_fields dimension;
    mc_world_dimension_biome_fields fields;
    CHECK(create_world(f, 2, 2, 0));
    resource_source *s = &f->resources[2];
    s->world = f->worlds[2];
    s->dimension = f->dimensions[2];
    s->captured = f->fields[2].biome;
    const size_t count = mc_world_store_chunk_count(f->stores[0]);
    CHECK(generate(f, 2, 1, 0, &out, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out == f->children[0].state);
    CHECK(error.bytecode == 93 && error.base_bytecode == 44 && error.density_bytecode == 292 &&
          clean_failure(&error));
    CHECK(s->rebound && !s->bad && s->opens == 3 && s->closes == 1 && s->guarded == 3 &&
          mc_world_store_chunk_count(f->stores[0]) == count && f->fields[2].generation->v != NULL &&
          f->fields[2].generation->q.data == NULL);
    const mc_world_provider_generation_fields *g = f->fields[2].generation;
    CHECK(g->d.data != NULL && g->d.length == 425 && g->e.data != NULL && g->e.length == 425 &&
          g->f.data != NULL && g->f.length == 425 && g->g.data != NULL && g->g.length == 25 &&
          g->h.data != NULL && g->h.length == 25);
    /* Original seed23/(1,0) raw noise endpoints, independent of the later
     * climate receiver. Publication alone does not prove these fills ran. */
    CHECK(bits(g->g.data[0]) == UINT64_C(0xc069913ffe0a9c83) &&
          bits(g->g.data[24]) == UINT64_C(0xc066c3096c48c25b) &&
          bits(g->h.data[0]) == UINT64_C(0x40b4b3d5c6e70711) &&
          bits(g->h.data[24]) == UINT64_C(0x40b9236800a194f4) &&
          bits(g->d.data[0]) == UINT64_C(0x4045ade85e27de6b) &&
          bits(g->d.data[424]) == UINT64_C(0xc0376ea0e2c8ede0) &&
          bits(g->e.data[0]) == UINT64_C(0xc0d8c6c7a0740b86) &&
          bits(g->e.data[424]) == UINT64_C(0x40afe4c939e806b7) &&
          bits(g->f.data[0]) == UINT64_C(0x40c48609ec30baf5) &&
          bits(g->f.data[424]) == UINT64_C(0x40b661412be07694));
    CHECK(mc_world_dimension_fields_get(f->dimensions[2], &dimension) == MC_WORLD_DIMENSION_OK &&
          dimension.b != f->fields[2].biome && dimension.a == f->worlds[2] &&
          mc_world_dimension_biome_fields_get(dimension.b, &fields) == MC_WORLD_DIMENSION_OK &&
          fields.a->data == NULL && fields.b->data == NULL);
    CHECK(f->fields[2].manager->a.data != NULL && f->fields[2].manager->b.data != NULL &&
          f->fields[2].cache->count == 0 && f->fields[2].cache->insertion_count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool check_null_biome(fixture *f)
{
    bool ok = false;
    mc_world_provider_fields *p = &f->fields[0];
    mc_biome_reference_array *sample = NULL;
    mc_biome_registry_view view;
    mc_chunk_state *out = f->children[0].state;
    mc_world_generation_error error;
    CHECK(mc_world_dimension_biome_sample(p->biome, 16, 0, 16, 16, p->generation->v, &sample,
                                          NULL) == MC_WORLD_DIMENSION_OK &&
          sample == p->generation->v);
    CHECK(mc_biome_registry_inspect(f->registry[0], &view, NULL) == MC_REGISTRY_OK &&
          view.x_length == 4096);
    const double temperature = p->manager->a.data[0], rainfall = p->manager->b.data[0];
    CHECK(temperature >= 0.0 && temperature <= 1.0 && rainfall >= 0.0 && rainfall <= 1.0);
    const size_t index =
        (size_t)(int32_t)(temperature * 63.0) + (size_t)(int32_t)(rainfall * 63.0) * 64u;
    CHECK(index < view.x_length && view.x[index] != NULL);
    /* Authorized exclusive native control of ONE real mutable Registry table
     * element. This is not original-observed mutation or a fabricated palette. */
    f->changed_table = (const mc_biome **)view.x;
    f->changed_index = index;
    f->old_cell = view.x[index];
    f->changed_table[index] = NULL;
    const size_t count = mc_world_store_chunk_count(f->stores[0]);
    const mc_java_random cave = p->generator->cave_random;
    mc_java_random expected = p->generator->generator_random;
    mc_java_random_reseed(&expected, mc_terrain_normal_chunk_seed(1, 0));
    (void)mc_java_random_next_double(&expected);
    (void)mc_java_random_next_double(&expected);
    (void)mc_java_random_next_double(&expected);
    CHECK(generate(f, 0, 1, 0, &out, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out == f->children[0].state);
    CHECK(error.bytecode == 104 && error.surface_bytecode == 282 && clean_failure(&error) &&
          scratch_ready(p->generation) &&
          mc_biome_reference_array_data(p->generation->v)[0] == NULL &&
          mc_test_java_random_equal(&cave, &p->generator->cave_random) &&
          mc_test_java_random_equal(&expected, &p->generator->generator_random) &&
          mc_world_store_chunk_count(f->stores[0]) == count && p->cache->count == 0);
    f->changed_table[index] = f->old_cell;
    f->changed_table = NULL;
    CHECK(generate(f, 0, 1, 0, &out, &error) == MC_WORLD_STORAGE_OK &&
          out != f->children[0].state &&
          mc_biome_reference_array_data(p->generation->v)[0] == f->old_cell);
    ok = true;
cleanup:
    if (f->changed_table != NULL) {
        f->changed_table[f->changed_index] = f->old_cell;
        f->changed_table = NULL;
    }
    return ok;
}

static bool check_retirement(fixture *f)
{
    bool ok = false;
    mc_world_runtime_construction state;
    mc_world_dimension_fields dimension;
    CHECK(mc_world_runtime_retire_world(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[0], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_RETIRED && !state.contexts_bound &&
          !state.cache_registered);
    CHECK(mc_world_dimension_fields_get(f->dimensions[0], &dimension) == MC_WORLD_DIMENSION_OK &&
          dimension.a == NULL &&
          mc_world_store_has_generated_for_world(f->stores[0], f->worlds[0]));
    /* With the Dimension binding and contexts retired, this refusal exercises
     * the generated-child guard rather than the earlier binding guard. */
    CHECK(mc_world_runtime_free(f->worlds[0], NULL) == MC_WORLD_STORAGE_BUSY);
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f)
{
    if (f == NULL)
        return true;
    if (f->changed_table != NULL)
        f->changed_table[f->changed_index] = f->old_cell;
    if (f->reservation != NULL && mc_world_store_cancel_reservation(f->stores[1], f->reservation,
                                                                    NULL) != MC_WORLD_STORAGE_OK)
        return false;
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state != NULL &&
            mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) !=
                MC_WORLD_STORAGE_OK)
            return false;
        f->children[i] = (retained_chunk){0};
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] != NULL &&
            (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
             mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK))
            return false;
        f->worlds[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_dimension_release(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->classes); ++i)
        if (mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    for (size_t i = 0; i < COUNT(f->registry); ++i)
        if (mc_registry_classes_free(f->registry[i], NULL) != MC_REGISTRY_OK)
            return false;
    mc_world_info_name_release(f->name);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        mc_world_store_free(f->stores[i]);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool decode_path(const char *hex, char *out, size_t capacity)
{
    const size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2 * i + j];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                              : 16u;
            if (digit > 15u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        out[i] = (char)(unsigned char)value;
    }
    out[n / 2u] = '\0';
    return true;
}

int main(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'G', 'e', 'n'};
    if (argc != 3 || strcmp(argv[1], "world-provider-generation") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(open_store(f, 0, root, "", 16, mc_nbt_default_limits().max_allocation_bytes));
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->registry); ++i) {
        resource_source *s = &f->resources[i];
        s->streams[0] = (byte_stream){.bytes = language, .length = sizeof(language) - 1};
        s->streams[1] = (byte_stream){.bytes = language};
        s->streams[2] = (byte_stream){.bytes = language};
        s->fail = i == 1;
        s->rebind = i == 2;
        s->api = (mc_registry_resources){s, open_resource, close_resource, resource_ready};
        const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                             &s->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[i], NULL) == MC_REGISTRY_OK);
        s->registry = f->registry[i];
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[i], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[i], f->registry[i], NULL) ==
              MC_REGISTRY_OK);
    }
    CHECK(create_world(f, 0, 0, 0));
    CHECK(check_array_captures());
    CHECK(check_success(f, root));
    CHECK(check_limits(f, root));
    CHECK(check_wrapped_scheduler(f));
    CHECK(check_resource_failure(f));
    CHECK(check_late_climate(f));
    CHECK(check_null_biome(f));
    CHECK(check_retirement(f));
    for (size_t i = 0; i < COUNT(f->resources); ++i)
        CHECK(!f->resources[i].bad);
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("provider generation fixture retirement refused; live dependencies retained\n",
              stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u constructed GAME provider generation checks (world-provider-generation)\n",
               checks) < 0)
        result = 1;
    return result;
}
