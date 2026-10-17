#include "mc/world/generation/normal.h"

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define MAX_CHUNKS 512u
#define MAX_QUEUE 32768u
#define MAX_MANAGER_VALUES 4096u
#define CALLS 9u
#define BLOCK_CAPACITY (MC_CHUNK_BLOCKS + 3u)

_Static_assert(sizeof(double) == sizeof(uint64_t) && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "binary64 storage required");

/* Large owners are static so the trusted adapter also fits the MSVC default stack. */
static mc_normal_generator generator;
static mc_biome_memory memory;
static mc_biome_manager manager;
static mc_math_trig trig, saved_trig;
static mc_terrain_normal_state saved_terrain;
static mc_noise_simplex saved_temperature[4], saved_rainfall[4], saved_auxiliary[2];
static mc_biome_classifier saved_classifier;
static mc_normal_generator_materials materials, saved_materials;
static int32_t opacity[256], saved_opacity[256];
static mc_normal_chunk results[CALLS], saved_results[CALLS];
static uint8_t blocks[CALLS][BLOCK_CAPACITY], saved_blocks[CALLS][BLOCK_CAPACITY];
static mc_chunk_data cached[MAX_CHUNKS];
static mc_chunk_state chunks[MAX_CHUNKS], saved_chunks[MAX_CHUNKS];
static uint8_t saved_heights[MAX_CHUNKS][MC_CHUNK_HEIGHTMAP_BYTES];
static mc_chunk_cache_entry entries[MAX_CHUNKS], saved_entries[MAX_CHUNKS];
static mc_chunk_state *insertions[MAX_CHUNKS], *saved_insertions[MAX_CHUNKS];
static mc_light_region regions[MAX_QUEUE];
static mc_chunk_height_query queries[MC_CHUNK_FINISH_QUERIES];
static mc_biome_id saved_d[MAX_MANAGER_VALUES];
static const int32_t coordinates[CALLS][2] = {{1, 0},
                                              {0, 0},
                                              {1, 0},
                                              {-1, -1},
                                              {32768, 0},
                                              {2000000, 0},
                                              {-2000000, 0},
                                              {INT32_MIN + 8, 0},
                                              {INT32_MAX - 9, 0}};

static bool read_bytes(FILE *file, void *data, size_t count)
{
    return fread(data, 1, count, file) == count;
}

static bool read_u32(FILE *file, uint32_t *value)
{
    uint8_t data[4];
    if (!read_bytes(file, data, sizeof(data)))
        return false;
    *value = (uint32_t)data[0] << 24u | (uint32_t)data[1] << 16u | (uint32_t)data[2] << 8u |
             (uint32_t)data[3];
    return true;
}

static bool read_i32(FILE *file, int32_t *value)
{
    uint32_t raw;
    if (!read_u32(file, &raw))
        return false;
    *value = raw <= INT32_MAX ? (int32_t)raw : -1 - (int32_t)(UINT32_MAX - raw);
    return true;
}

static bool read_u64(FILE *file, uint64_t *value)
{
    uint32_t high, low;
    if (!read_u32(file, &high) || !read_u32(file, &low))
        return false;
    *value = (uint64_t)high << 32u | low;
    return true;
}

static bool read_boolean(FILE *file, bool *value)
{
    int byte = fgetc(file);
    if (byte != 0 && byte != 1)
        return false;
    *value = byte != 0;
    return true;
}

static bool read_doubles(FILE *file, mc_biome_double_buffer *buffer)
{
    int32_t length;
    if (!read_i32(file, &length) || length < -1 || length > (int32_t)MAX_MANAGER_VALUES)
        return false;
    if (length == -1)
        return true;
    if (mc_biome_double_buffer_resize(&memory, buffer, (size_t)length) != MC_BIOME_OK)
        return false;
    for (size_t i = 0; i < buffer->length; ++i) {
        uint64_t raw;
        if (!read_u64(file, &raw))
            return false;
        memcpy(&buffer->data[i], &raw, sizeof(raw));
    }
    return true;
}

static bool read_biomes(FILE *file)
{
    int32_t length;
    if (!read_i32(file, &length) || length < -1 || length > (int32_t)MAX_MANAGER_VALUES)
        return false;
    if (length == -1)
        return true;
    if (mc_biome_buffer_resize(&memory, &manager.d, (size_t)length) != MC_BIOME_OK)
        return false;
    for (size_t i = 0; i < manager.d.length; ++i) {
        int value = fgetc(file);
        if (value < 'a' || value > 'm')
            return false;
        manager.d.data[i] = (mc_biome_id)(value - 'a' + (int)MC_BIOME_RAINFOREST);
    }
    return true;
}

static bool read_input(FILE *file, int64_t *seed, mc_chunk_cache *cache, mc_light_queue *queue,
                       int32_t *depth, bool *no_sky, size_t *chunk_count)
{
    char magic[4];
    uint64_t raw_seed;
    uint32_t nc, ne, ni, nq;
    if (!read_bytes(file, magic, sizeof(magic)) || memcmp(magic, "MCG1", 4) != 0 ||
        !read_u64(file, &raw_seed) || !read_boolean(file, no_sky) || !read_i32(file, depth) ||
        !read_u32(file, &nc) || !read_u32(file, &ne) || !read_u32(file, &ni) ||
        !read_u32(file, &nq) || nc > MAX_CHUNKS || ne > MAX_CHUNKS || ni > MAX_CHUNKS ||
        nq > MAX_QUEUE - CALLS * MC_CHUNK_FINISH_QUERIES)
        return false;
    *seed = raw_seed <= INT64_MAX ? (int64_t)raw_seed : -1 - (int64_t)(UINT64_MAX - raw_seed);
    if (mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) != MC_BIOME_OK ||
        mc_biome_manager_init(&manager, *seed, &memory) != MC_BIOME_OK)
        return false;
    for (size_t i = 0; i < 256u; ++i) {
        if (!read_i32(file, &opacity[i]) || opacity[i] < 0)
            return false;
    }
    for (size_t i = 0; i < MC_NORMAL_GENERATOR_BIOME_MATERIALS; ++i) {
        if (!read_bytes(file, &materials.by_biome[i].top, 1) ||
            !read_bytes(file, &materials.by_biome[i].filler, 1))
            return false;
    }
    if (!read_doubles(file, &manager.a) || !read_doubles(file, &manager.b) ||
        !read_doubles(file, &manager.c) || !read_biomes(file))
        return false;
    for (size_t i = 0; i < nc; ++i) {
        chunks[i].data = &cached[i];
        if (!read_i32(file, &chunks[i].x) || !read_i32(file, &chunks[i].z) ||
            !read_boolean(file, &chunks[i].blank) || !read_boolean(file, &chunks[i].dirty) ||
            !read_bytes(file, cached[i].heightmap, MC_CHUNK_HEIGHTMAP_BYTES))
            return false;
        saved_chunks[i] = chunks[i];
        memcpy(saved_heights[i], cached[i].heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    }
    if (!mc_chunk_cache_init(cache, entries, MAX_CHUNKS, insertions, MAX_CHUNKS) ||
        !mc_light_queue_init(queue, regions, MAX_QUEUE))
        return false;
    for (size_t i = 0; i < ne; ++i) {
        uint32_t id;
        if (!read_i32(file, &entries[i].key) || !read_u32(file, &id) || id >= nc)
            return false;
        entries[i].chunk = &chunks[id];
    }
    for (size_t i = 0; i < ni; ++i) {
        uint32_t id;
        if (!read_u32(file, &id) || id >= nc)
            return false;
        insertions[i] = &chunks[id];
    }
    for (size_t i = 0; i < nq; ++i) {
        uint32_t kind;
        mc_light_bounds *bounds = &regions[i].bounds;
        if (!read_u32(file, &kind) || kind > 1u || !read_i32(file, &bounds->min_x) ||
            !read_i32(file, &bounds->min_y) || !read_i32(file, &bounds->min_z) ||
            !read_i32(file, &bounds->max_x) || !read_i32(file, &bounds->max_y) ||
            !read_i32(file, &bounds->max_z))
            return false;
        regions[i].kind = (mc_light_kind)kind;
    }
    cache->count = ne;
    cache->insertion_count = ni;
    queue->count = nq;
    *chunk_count = nc;
    return fgetc(file) == EOF && !ferror(file) && mc_chunk_cache_validate(cache);
}

static const char *json_boolean(bool value)
{
    return value ? "true" : "false";
}

static uint64_t double_bits(const double *value)
{
    uint64_t raw;
    memcpy(&raw, value, sizeof(raw));
    return raw;
}

static void print_doubles(const double *values, size_t length)
{
    if (values == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"length\":%zu,\"values\":\"", length);
    for (size_t i = 0; i < length; ++i)
        printf("%016" PRIx64, double_bits(&values[i]));
    fputs("\"}", stdout);
}

static void print_biomes(const mc_biome_id *values, size_t length)
{
    if (values == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"length\":%zu,\"values\":\"", length);
    for (size_t i = 0; i < length; ++i)
        putchar(mc_biome_field(values[i]));
    fputs("\"}", stdout);
}

static void print_manager(void)
{
    fputs("{\"a\":", stdout);
    print_doubles(manager.a.data, manager.a.length);
    fputs(",\"b\":", stdout);
    print_doubles(manager.b.data, manager.b.length);
    fputs(",\"c\":", stdout);
    print_doubles(manager.c.data, manager.c.length);
    fputs(",\"d\":", stdout);
    print_biomes(manager.d.data, manager.d.length);
    putchar('}');
}

static void print_random(mc_java_random random)
{
    printf("{\"state\":\"%012" PRIx64 "\",\"next_longs\":[", random.state);
    for (size_t i = 0; i < 8u; ++i)
        printf("%s\"%016" PRIx64 "\"", i ? "," : "", (uint64_t)mc_java_random_next_i64(&random));
    fputs("]}", stdout);
}

static void print_noise(void)
{
    static const char names[] = "klmnoabc";
    static const size_t counts[] = {16, 16, 8, 4, 4, 10, 16, 8};
    const mc_terrain_normal_state *state = &generator.terrain;
    const mc_noise_perlin *channels[] = {state->low_layers,
                                         state->high_layers,
                                         state->mix_layers,
                                         state->surface_primary_layers,
                                         state->surface_secondary_layers,
                                         state->column_scale_layers,
                                         state->column_bias_layers,
                                         state->later_layers};
    putchar('{');
    for (size_t channel = 0; channel < 8u; ++channel) {
        printf("%s\"%c\":[", channel ? "," : "", names[channel]);
        for (size_t layer = 0; layer < counts[channel]; ++layer) {
            const mc_noise_perlin *noise = &channels[channel][layer];
            printf("%s{\"offsets\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                   "\"],\"permutation\":[",
                   layer ? "," : "", double_bits(&noise->offset_x), double_bits(&noise->offset_y),
                   double_bits(&noise->offset_z));
            for (size_t i = 0; i < 512u; ++i)
                printf("%s%u", i ? "," : "", (unsigned int)noise->permutation[i]);
            fputs("]}", stdout);
        }
        putchar(']');
    }
    putchar('}');
}

static void print_bytes(const char *name, const uint8_t *values, size_t length)
{
    printf("\"%s\":{\"length\":%zu,\"values\":\"", name, length);
    for (size_t i = 0; i < length; ++i)
        printf("%02x", (unsigned int)values[i]);
    fputs("\"}", stdout);
}

static void print_call(size_t index, const mc_light_queue *queue, int32_t depth,
                       const bool reused[3], bool biomes_reused, bool context_retained,
                       bool prior_retained, bool immutable_retained, bool suffix_retained)
{
    const mc_normal_chunk *result = &results[index];
    const mc_chunk_data *data = &result->data;
    printf("%s{\"index\":%zu,\"coordinates\":[%" PRId32 ",%" PRId32 "],\"data\":{",
           index ? "," : "", index, result->chunk.x, result->chunk.z);
    print_bytes("blocks", blocks[index], MC_CHUNK_BLOCKS);
    putchar(',');
    print_bytes("metadata", data->metadata, sizeof(data->metadata));
    putchar(',');
    print_bytes("skylight", data->skylight, sizeof(data->skylight));
    putchar(',');
    print_bytes("block_light", data->block_light, sizeof(data->block_light));
    putchar(',');
    print_bytes("heightmap", data->heightmap, sizeof(data->heightmap));
    printf(",\"minimum_height\":%" PRId32 "},\"dirty\":%s,\"depth\":%" PRId32 ",\"queue\":[",
           data->minimum_height, json_boolean(result->chunk.dirty), depth);
    for (size_t i = 0; i < queue->count; ++i) {
        const mc_light_region *region = &queue->entries[i];
        const mc_light_bounds *b = &region->bounds;
        printf("%s[%d,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]",
               i ? "," : "", (int)region->kind, b->min_x, b->min_y, b->min_z, b->max_x, b->max_y,
               b->max_z);
    }
    fputs("],\"trace\":[", stdout);
    for (size_t i = 0; i < MC_CHUNK_FINISH_QUERIES; ++i) {
        const mc_chunk_height_query *query = &queries[i];
        printf("%s[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]", i ? "," : "", query->x,
               query->z, query->local_height, query->world_height);
    }
    fputs("],\"generator\":{\"scratch\":{", stdout);
    const char scratch_names[] = "qrstdefgh";
    const double *scratch[] = {generator.density,   generator.sand,         generator.gravel,
                               generator.thickness, generator.mix,          generator.low,
                               generator.high,      generator.column_scale, generator.column_bias};
    const size_t lengths[] = {425, 256, 256, 256, 425, 425, 425, 25, 25};
    for (size_t i = 0; i < 9u; ++i) {
        printf("%s\"%c\":", i ? "," : "", scratch_names[i]);
        print_doubles(scratch[i], lengths[i]);
    }
    fputs("},\"biomes\":", stdout);
    print_biomes(generator.biomes.data, generator.biomes.length);
    fputs(",\"materials\":\"", stdout);
    for (size_t i = 0; i < 256u; ++i)
        printf("%02x%02x", (unsigned int)generator.materials[i].top,
               (unsigned int)generator.materials[i].filler);
    fputs("\",\"generator_random\":", stdout);
    print_random(generator.generator_random);
    fputs(",\"cave_random\":", stdout);
    print_random(generator.cave_random);
    fputs("},\"manager\":", stdout);
    print_manager();
    printf(",\"manager_reused\":[%s,%s,%s],\"biomes_reused\":%s,\"context_retained\":%s,"
           "\"prior_retained\":%s,\"immutable_retained\":%s,\"suffix_retained\":%s}",
           json_boolean(reused[0]), json_boolean(reused[1]), json_boolean(reused[2]),
           json_boolean(biomes_reused), json_boolean(context_retained),
           json_boolean(prior_retained), json_boolean(immutable_retained),
           json_boolean(suffix_retained));
}

static bool immutable_retained(void)
{
    return memcmp(&generator.terrain, &saved_terrain, sizeof(saved_terrain)) == 0 &&
           memcmp(&trig, &saved_trig, sizeof(trig)) == 0 &&
           memcmp(&materials, &saved_materials, sizeof(materials)) == 0 &&
           memcmp(opacity, saved_opacity, sizeof(opacity)) == 0 &&
           memcmp(manager.temperature_layers, saved_temperature, sizeof(saved_temperature)) == 0 &&
           memcmp(manager.rainfall_layers, saved_rainfall, sizeof(saved_rainfall)) == 0 &&
           memcmp(manager.auxiliary_layers, saved_auxiliary, sizeof(saved_auxiliary)) == 0 &&
           memcmp(&manager.classifier, &saved_classifier, sizeof(saved_classifier)) == 0;
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (file == NULL)
        return 2;
    mc_chunk_cache cache;
    mc_light_queue queue;
    int64_t seed = 0;
    int32_t depth = 0;
    bool no_sky = false;
    size_t chunk_count = 0;
    int status = 2;
    bool read = read_input(file, &seed, &cache, &queue, &depth, &no_sky, &chunk_count);
    if (fclose(file) != 0 || !read)
        goto cleanup;
    mc_world_lighting world = {&cache, &queue, &depth, no_sky};
    status = 1;
    if (!mc_world_lighting_validate(&world) || !mc_math_trig_init(&trig) ||
        mc_normal_generator_init(&generator, seed, &manager, &trig) != MC_NORMAL_GENERATOR_OK)
        goto cleanup;
    const mc_chunk_cache saved_cache = cache;
    const mc_light_queue saved_queue = queue;
    const mc_world_lighting saved_world = world;
    const int32_t saved_depth = depth;
    mc_biome_id *const original_d = manager.d.data;
    const size_t original_d_length = manager.d.length;
    memcpy(saved_entries, entries, cache.count * sizeof(*entries));
    memcpy(saved_insertions, insertions, cache.insertion_count * sizeof(*insertions));
    if (original_d != NULL)
        memcpy(saved_d, original_d, original_d_length * sizeof(*original_d));
    memcpy(&saved_terrain, &generator.terrain, sizeof(saved_terrain));
    memcpy(&saved_trig, &trig, sizeof(trig));
    memcpy(&saved_materials, &materials, sizeof(materials));
    memcpy(saved_opacity, opacity, sizeof(opacity));
    memcpy(saved_temperature, manager.temperature_layers, sizeof(saved_temperature));
    memcpy(saved_rainfall, manager.rainfall_layers, sizeof(saved_rainfall));
    memcpy(saved_auxiliary, manager.auxiliary_layers, sizeof(saved_auxiliary));
    memcpy(&saved_classifier, &manager.classifier, sizeof(saved_classifier));
    memset(blocks, 0xa7, sizeof(blocks));
    mc_chunk_finish_trace trace = {queries, MC_CHUNK_FINISH_QUERIES, 0};
    size_t aborts = SIZE_MAX;
    mc_normal_generation_resources resources = {&world, &materials, opacity, 256, &trace, &aborts};
    if (generator.generator_random.state > UINT64_C(0xffffffffffff))
        goto cleanup;
    printf("{\"schema\":1,\"seed_hex\":\"%016" PRIx64 "\",\"constructor\":{\"noise\":",
           (uint64_t)seed);
    print_noise();
    fputs(",\"generator_random\":", stdout);
    print_random(generator.generator_random);
    fputs(",\"manager\":", stdout);
    print_manager();
    fputs("},\"calls\":[", stdout);
    for (size_t call = 0; call < CALLS; ++call) {
        const uintptr_t before[] = {(uintptr_t)manager.a.data, (uintptr_t)manager.b.data,
                                    (uintptr_t)manager.c.data};
        const uintptr_t before_biomes = (uintptr_t)generator.biomes.data;
        mc_normal_generator_status generated = mc_normal_generator_generate(
            &generator, coordinates[call][0], coordinates[call][1], &resources, &results[call],
            blocks[call], sizeof(blocks[call]));
        if (generated != MC_NORMAL_GENERATOR_OK) {
            fprintf(stderr, "generation call %zu: %s\n", call,
                    mc_normal_generator_status_string(generated));
            goto cleanup;
        }
        if (trace.entries != queries || trace.capacity != MC_CHUNK_FINISH_QUERIES ||
            trace.count != MC_CHUNK_FINISH_QUERIES || aborts != 0 || queue.count > MAX_QUEUE ||
            results[call].chunk.data != &results[call].data || results[call].chunk.blank ||
            !results[call].chunk.dirty || results[call].chunk.x != coordinates[call][0] ||
            results[call].chunk.z != coordinates[call][1] ||
            results[call].data.blocks != blocks[call] ||
            results[call].data.blocks_capacity != BLOCK_CAPACITY || manager.a.data == NULL ||
            manager.a.length < 256u || manager.a.length > MAX_MANAGER_VALUES ||
            manager.b.data == NULL || manager.b.length < 256u ||
            manager.b.length > MAX_MANAGER_VALUES || manager.c.data == NULL ||
            manager.c.length < 256u || manager.c.length > MAX_MANAGER_VALUES ||
            generator.biomes.data == NULL || generator.biomes.length != 256u ||
            generator.generator_random.state > UINT64_C(0xffffffffffff) ||
            generator.cave_random.state > UINT64_C(0xffffffffffff))
            goto cleanup;
        bool context =
            depth == saved_depth && cache.entries == saved_cache.entries &&
            cache.capacity == saved_cache.capacity && cache.count == saved_cache.count &&
            cache.insertions == saved_cache.insertions &&
            cache.insertion_capacity == saved_cache.insertion_capacity &&
            cache.insertion_count == saved_cache.insertion_count &&
            queue.entries == saved_queue.entries && queue.capacity == saved_queue.capacity &&
            world.cache == saved_world.cache && world.queue == saved_world.queue &&
            world.enqueue_depth == saved_world.enqueue_depth &&
            world.no_sky == saved_world.no_sky && generator.world_seed == seed &&
            generator.biome_manager == &manager && generator.trig == &trig &&
            generator.initialized && manager.memory == &memory && manager.initialized == 1 &&
            manager.d.data == original_d && manager.d.length == original_d_length &&
            (original_d == NULL ||
             memcmp(original_d, saved_d, original_d_length * sizeof(*original_d)) == 0) &&
            memcmp(entries, saved_entries, cache.count * sizeof(*entries)) == 0 &&
            memcmp(insertions, saved_insertions, cache.insertion_count * sizeof(*insertions)) == 0;
        for (size_t i = 0; i < chunk_count; ++i) {
            context = context && chunks[i].data == saved_chunks[i].data &&
                      chunks[i].x == saved_chunks[i].x && chunks[i].z == saved_chunks[i].z &&
                      chunks[i].blank == saved_chunks[i].blank &&
                      chunks[i].dirty == saved_chunks[i].dirty &&
                      memcmp(cached[i].heightmap, saved_heights[i], MC_CHUNK_HEIGHTMAP_BYTES) == 0;
        }
        bool prior = true;
        for (size_t i = 0; i < call; ++i) {
            prior = prior && memcmp(&results[i], &saved_results[i], sizeof(results[i])) == 0 &&
                    memcmp(blocks[i], saved_blocks[i], sizeof(blocks[i])) == 0;
        }
        bool suffix = true;
        for (size_t i = MC_CHUNK_BLOCKS; i < BLOCK_CAPACITY; ++i)
            suffix = suffix && blocks[call][i] == 0xa7;
        const bool immutable = immutable_retained();
        if (!context || !prior || !suffix || !immutable)
            goto cleanup;
        bool reused[] = {before[0] == (uintptr_t)manager.a.data,
                         before[1] == (uintptr_t)manager.b.data,
                         before[2] == (uintptr_t)manager.c.data};
        print_call(call, &queue, depth, reused, before_biomes == (uintptr_t)generator.biomes.data,
                   context, prior, immutable, suffix);
        /* Inspection snapshots are never used as owning descriptors or rollback state. */
        memcpy(&saved_results[call], &results[call], sizeof(results[call]));
        memcpy(saved_blocks[call], blocks[call], sizeof(blocks[call]));
    }
    fputs("]}\n", stdout);
    status = ferror(stdout) || fflush(stdout) != 0 ? 1 : 0;
cleanup:
    mc_normal_generator_free(&generator);
    mc_biome_manager_free(&manager);
    if (memory.live_allocation_bytes != 0) {
        fputs("biome allocation accounting did not return to zero\n", stderr);
        status = 1;
    }
    if (status != 0)
        fprintf(stderr, "normal probe failed (%d)\n", status);
    return status;
}
