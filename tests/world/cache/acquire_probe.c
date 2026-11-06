#include "mc/world/cache/acquire.h"
#include "mc/world/generation/overworld.h"

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define MAX_IMPORTED 16u
#define MAX_OPERATIONS 16u
#define MAX_CHUNKS (MAX_IMPORTED + MAX_OPERATIONS)
#define MAX_ENTRIES 64u
#define MAX_QUEUE 32768u
#define MAX_MANAGER_VALUES 4096u
#define MAX_CALLBACKS 256u
#define MAX_OUTPUT_BYTES (64u * 1024u * 1024u)
#define BLOCK_CAPACITY (MC_CHUNK_BLOCKS + 3u)

_Static_assert(sizeof(double) == sizeof(uint64_t) && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "binary64 storage required");

/* Trusted adapter: real C generation and lighting, with a deliberately explicit
 * load-MISS boundary. Population features are not implemented here. All large
 * owners are static, including inspection copies which are never used as owners. */
static mc_overworld_generator generator;
static mc_biome_memory memory;
static mc_biome_manager manager;
static mc_math_trig trig, saved_trig;
static mc_terrain_overworld_state saved_terrain;
static mc_noise_simplex saved_temperature[4], saved_rainfall[4], saved_auxiliary[2];
static mc_biome_classifier saved_classifier;
static mc_overworld_generator_materials materials, saved_materials;
static int32_t opacity[256], saved_opacity[256];
static mc_overworld_chunk chunks[MAX_CHUNKS], saved_chunks[MAX_CHUNKS], bootstrap;
static uint8_t blocks[MAX_CHUNKS][BLOCK_CAPACITY], saved_blocks[MAX_CHUNKS][BLOCK_CAPACITY];
static uint8_t bootstrap_blocks[BLOCK_CAPACITY];
static mc_chunk_cache_entry entries[MAX_ENTRIES], saved_entries[MAX_ENTRIES];
static mc_chunk_state *insertions[MAX_ENTRIES], *saved_insertions[MAX_ENTRIES];
static int32_t pending_keys[MAX_ENTRIES], saved_pending[MAX_ENTRIES];
static mc_light_region regions[MAX_QUEUE], bootstrap_regions[MC_CHUNK_FINISH_QUERIES];
static mc_chunk_height_query queries[MAX_OPERATIONS][MC_CHUNK_FINISH_QUERIES + 1u];
static size_t finish_ids[MAX_OPERATIONS], finish_count;
static mc_biome_id saved_d[MAX_MANAGER_VALUES], saved_biomes[256];
static double saved_manager[3][MAX_MANAGER_VALUES];
static unsigned char saved_generator[sizeof(mc_overworld_generator)];
static unsigned char saved_manager_owner[sizeof(mc_biome_manager)];
static mc_biome_id *original_d;
static size_t original_d_length, chunk_count, imported_count, operation_count;
static mc_chunk_cache cache;
static mc_chunk_pending pending;
static mc_light_queue queue;
static mc_world_lighting lighting;
static mc_chunk_acquire_context acquisition;
static int64_t seed, world_time;
static int32_t depth, spawn_x, spawn_z;
static bool no_sky, dedicated, world_force, provider_force, callback_failed;
static bool runtime_failed;
static size_t output_bytes;
static bool output_failed;

typedef struct operation {
    uint32_t method;
    int32_t x, z;
} operation;

static operation operations[MAX_OPERATIONS];

typedef struct callback_record {
    const char *kind;
    int32_t x, z;
    size_t id, slice, map_count, list_count, pending_count;
    bool active, dirty, populated;
    int64_t time;
} callback_record;

static callback_record callbacks[MAX_CALLBACKS];
static size_t callback_count;

static void emit(const char *format, ...) {
    if (output_failed)
        return;
    char buffer[512];
    va_list arguments;
    va_start(arguments, format);
    int count = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (count < 0 || (size_t)count >= sizeof(buffer) ||
        (size_t)count > MAX_OUTPUT_BYTES - output_bytes) {
        output_failed = true;
        return;
    }
    output_bytes += (size_t)count;
    if (fwrite(buffer, 1, (size_t)count, stdout) != (size_t)count)
        output_failed = true;
}

static bool read_bytes(FILE *file, void *data, size_t count) {
    return fread(data, 1, count, file) == count;
}

/* Binary and raw-bit JSON helpers follow the project-owned overworld_probe.c. */
static bool read_u32(FILE *file, uint32_t *value) {
    uint8_t data[4];
    if (!read_bytes(file, data, sizeof(data)))
        return false;
    *value = (uint32_t)data[0] << 24u | (uint32_t)data[1] << 16u | (uint32_t)data[2] << 8u |
             (uint32_t)data[3];
    return true;
}

static bool read_i32(FILE *file, int32_t *value) {
    uint32_t raw;
    if (!read_u32(file, &raw))
        return false;
    *value = raw <= INT32_MAX ? (int32_t)raw : -1 - (int32_t)(UINT32_MAX - raw);
    return true;
}

static bool read_i64(FILE *file, int64_t *value) {
    uint32_t high, low;
    if (!read_u32(file, &high) || !read_u32(file, &low))
        return false;
    uint64_t raw = (uint64_t)high << 32u | low;
    *value = raw <= INT64_MAX ? (int64_t)raw : -1 - (int64_t)(UINT64_MAX - raw);
    return true;
}

static bool read_boolean(FILE *file, bool *value) {
    int byte = fgetc(file);
    if (byte != 0 && byte != 1)
        return false;
    *value = byte != 0;
    return true;
}

static bool read_doubles(FILE *file, mc_biome_double_buffer *buffer) {
    int32_t length;
    if (!read_i32(file, &length) || length < -1 || length > (int32_t)MAX_MANAGER_VALUES)
        return false;
    if (length == -1) {
        mc_biome_double_buffer_free(buffer);
        return true;
    }
    if (mc_biome_double_buffer_resize(&memory, buffer, (size_t)length) != MC_BIOME_OK) {
        runtime_failed = true;
        return false;
    }
    for (size_t i = 0; i < buffer->length; ++i) {
        int64_t raw;
        if (!read_i64(file, &raw))
            return false;
        uint64_t bits = (uint64_t)raw;
        memcpy(&buffer->data[i], &bits, sizeof(bits));
    }
    return true;
}

static bool read_biomes(FILE *file) {
    int32_t length;
    if (!read_i32(file, &length) || length < -1 || length > (int32_t)MAX_MANAGER_VALUES)
        return false;
    if (length == -1) {
        mc_biome_buffer_free(&manager.d);
        return true;
    }
    if (mc_biome_buffer_resize(&memory, &manager.d, (size_t)length) != MC_BIOME_OK) {
        runtime_failed = true;
        return false;
    }
    for (size_t i = 0; i < manager.d.length; ++i) {
        int value = fgetc(file);
        if (value < 'a' || value > 'm')
            return false;
        manager.d.data[i] = (mc_biome_id)(value - 'a' + (int)MC_BIOME_RAINFOREST);
    }
    return true;
}

static size_t chunk_id(const mc_chunk_state *state) {
    for (size_t i = 0; i < chunk_count; ++i) {
        if (state == &chunks[i].chunk)
            return i;
    }
    return SIZE_MAX;
}

static bool suffixes_retained(void) {
    for (size_t i = 0; i < chunk_count; ++i) {
        for (size_t j = MC_CHUNK_BLOCKS; j < BLOCK_CAPACITY; ++j) {
            if (blocks[i][j] != 0xa7)
                return false;
        }
    }
    for (size_t j = MC_CHUNK_BLOCKS; j < BLOCK_CAPACITY; ++j) {
        if (bootstrap_blocks[j] != 0xa7)
            return false;
    }
    return true;
}

static bool immutable_retained(void) {
    return memcmp(&generator.terrain, &saved_terrain, sizeof(saved_terrain)) == 0 &&
           memcmp(&trig, &saved_trig, sizeof(trig)) == 0 &&
           memcmp(&materials, &saved_materials, sizeof(materials)) == 0 &&
           memcmp(opacity, saved_opacity, sizeof(opacity)) == 0 &&
           memcmp(manager.temperature_layers, saved_temperature, sizeof(saved_temperature)) == 0 &&
           memcmp(manager.rainfall_layers, saved_rainfall, sizeof(saved_rainfall)) == 0 &&
           memcmp(manager.auxiliary_layers, saved_auxiliary, sizeof(saved_auxiliary)) == 0 &&
           memcmp(&manager.classifier, &saved_classifier, sizeof(saved_classifier)) == 0 &&
           manager.d.data == original_d && manager.d.length == original_d_length &&
           (original_d == NULL ||
            memcmp(original_d, saved_d, original_d_length * sizeof(*original_d)) == 0) &&
           generator.biome_manager == &manager && generator.trig == &trig &&
           generator.world_seed == seed && generator.initialized && manager.memory == &memory &&
           manager.initialized == 1;
}

static void save_payloads(void) {
    memcpy(saved_chunks, chunks, chunk_count * sizeof(*chunks));
    memcpy(saved_blocks, blocks, chunk_count * sizeof(*blocks));
}

static bool payloads_retained(size_t count) {
    return memcmp(saved_chunks, chunks, count * sizeof(*chunks)) == 0 &&
           memcmp(saved_blocks, blocks, count * sizeof(*blocks)) == 0;
}

static void save_generator(void) {
    memcpy(saved_generator, &generator, sizeof(generator));
    memcpy(saved_manager_owner, &manager, sizeof(manager));
    memcpy(saved_biomes, generator.biomes.data, sizeof(saved_biomes));
    const mc_biome_double_buffer *buffers[] = {&manager.a, &manager.b, &manager.c};
    for (size_t i = 0; i < 3u; ++i) {
        if (buffers[i]->data != NULL)
            memcpy(saved_manager[i], buffers[i]->data, buffers[i]->length * sizeof(double));
    }
}

static bool generator_retained(void) {
    if (memcmp(saved_generator, &generator, sizeof(generator)) != 0 ||
        memcmp(saved_manager_owner, &manager, sizeof(manager)) != 0 ||
        memcmp(saved_biomes, generator.biomes.data, sizeof(saved_biomes)) != 0)
        return false;
    const mc_biome_double_buffer *buffers[] = {&manager.a, &manager.b, &manager.c};
    for (size_t i = 0; i < 3u; ++i) {
        if (buffers[i]->data != NULL &&
            memcmp(saved_manager[i], buffers[i]->data, buffers[i]->length * sizeof(double)) != 0)
            return false;
    }
    return true;
}

static bool log_callback(const char *kind, int32_t x, int32_t z, const mc_chunk_state *chunk,
                         size_t slice) {
    if (callback_count == MAX_CALLBACKS || (chunk != NULL && chunk_id(chunk) == SIZE_MAX)) {
        callback_failed = true;
        return false;
    }
    callbacks[callback_count++] = (callback_record){kind,
                                                    x,
                                                    z,
                                                    chunk_id(chunk),
                                                    slice,
                                                    cache.count,
                                                    cache.insertion_count,
                                                    pending.count,
                                                    chunk != NULL && chunk->active,
                                                    chunk != NULL && chunk->dirty,
                                                    chunk != NULL && chunk->terrain_populated,
                                                    chunk == NULL ? 0 : chunk->last_save_time};
    return true;
}

static mc_chunk_load_result load_miss(void *owner, const mc_chunk_acquire_context *context,
                                      int32_t x, int32_t z, mc_chunk_state **loaded) {
    if (owner != &lighting || context != &acquisition || loaded == NULL || *loaded != NULL ||
        !log_callback("load", x, z, NULL, SIZE_MAX)) {
        callback_failed = true;
        return MC_CHUNK_LOAD_FATAL;
    }
    return MC_CHUNK_LOAD_MISS;
}

static bool generate_chunk(void *owner, const mc_chunk_acquire_context *context, int32_t x,
                           int32_t z, mc_chunk_state **generated) {
    if (owner != &generator || context != &acquisition || generated == NULL ||
        chunk_count == MAX_CHUNKS || finish_count == MAX_OPERATIONS ||
        !log_callback("generate_begin", x, z, NULL, SIZE_MAX))
        return false;
    const size_t prior_count = chunk_count;
    const mc_chunk_cache before_cache = cache;
    const mc_chunk_pending before_pending = pending;
    const int32_t before_depth = depth;
    save_payloads();
    memcpy(saved_entries, entries, cache.count * sizeof(*entries));
    memcpy(saved_insertions, insertions, cache.insertion_count * sizeof(*insertions));
    memcpy(saved_pending, pending_keys, pending.count * sizeof(*pending_keys));
    memset(queries[finish_count], 0xa5, sizeof(queries[finish_count]));
    mc_chunk_height_query suffix = queries[finish_count][MC_CHUNK_FINISH_QUERIES];
    mc_chunk_finish_trace trace = {queries[finish_count], MC_CHUNK_FINISH_QUERIES, 0};
    size_t aborts = SIZE_MAX;
    mc_overworld_generation_resources resources = {&lighting, &materials, opacity,
                                                   256u,      &trace,     &aborts};
    mc_overworld_generator_status status = mc_overworld_generator_generate(
        &generator, x, z, &resources, &chunks[chunk_count], blocks[chunk_count], BLOCK_CAPACITY);
    if (status != MC_OVERWORLD_GENERATOR_OK) {
        fprintf(stderr, "acquire probe Overworld generation: %s\n",
                mc_overworld_generator_status_string(status));
        return false;
    }
    if (memcmp(&cache, &before_cache, sizeof(cache)) != 0 ||
        memcmp(&pending, &before_pending, sizeof(pending)) != 0 || depth != before_depth ||
        memcmp(entries, saved_entries, cache.count * sizeof(*entries)) != 0 ||
        memcmp(insertions, saved_insertions, cache.insertion_count * sizeof(*insertions)) != 0 ||
        memcmp(pending_keys, saved_pending, pending.count * sizeof(*pending_keys)) != 0 ||
        !payloads_retained(prior_count) || trace.entries != queries[finish_count] ||
        trace.count != MC_CHUNK_FINISH_QUERIES || trace.capacity != MC_CHUNK_FINISH_QUERIES ||
        aborts != 0 ||
        memcmp(&queries[finish_count][MC_CHUNK_FINISH_QUERIES], &suffix, sizeof(suffix)) != 0 ||
        chunks[chunk_count].chunk.data != &chunks[chunk_count].data ||
        chunks[chunk_count].data.blocks != blocks[chunk_count] ||
        chunks[chunk_count].data.blocks_capacity != BLOCK_CAPACITY) {
        callback_failed = true;
        return false;
    }
    finish_ids[finish_count++] = chunk_count;
    *generated = &chunks[chunk_count++].chunk;
    return suffixes_retained() && immutable_retained() &&
           log_callback("generate_end", x, z, *generated, SIZE_MAX);
}

static bool register_tile_entities(void *owner, const mc_chunk_acquire_context *context,
                                   mc_chunk_state *chunk) {
    mc_chunk_state *mapped = NULL;
    if (owner != &lighting || context != &acquisition || !chunk->active ||
        cache.insertion_count == 0 || insertions[cache.insertion_count - 1u] != chunk ||
        !mc_chunk_cache_lookup(&cache, chunk->x, chunk->z, &mapped) || mapped != chunk) {
        callback_failed = true;
        return false;
    }
    return log_callback("tiles", chunk->x, chunk->z, chunk, SIZE_MAX);
}

static bool register_entities(void *owner, const mc_chunk_acquire_context *context,
                              mc_chunk_state *chunk, size_t slice) {
    if (owner != &lighting || context != &acquisition || !chunk->active || slice >= 8u) {
        callback_failed = true;
        return false;
    }
    return log_callback("entities", chunk->x, chunk->z, chunk, slice);
}

static bool unexpected_population(void *owner, const mc_chunk_acquire_context *context, int32_t x,
                                  int32_t z) {
    mc_chunk_state *target = NULL;
    if (owner != &generator || context != &acquisition ||
        !mc_chunk_cache_lookup(&cache, x, z, &target))
        callback_failed = true;
    (void)log_callback("populate", x, z, target, SIZE_MAX);
    fputs("acquire probe encountered population outside its recorded no-population scope\n",
          stderr);
    return false;
}

static bool bootstrap_generator(void) {
    mc_chunk_cache empty;
    mc_light_queue temporary;
    int32_t temporary_depth = 0;
    if (!mc_chunk_cache_init(&empty, NULL, 0, NULL, 0) ||
        !mc_light_queue_init(&temporary, bootstrap_regions, MC_CHUNK_FINISH_QUERIES))
        return false;
    mc_world_lighting world = {&empty, &temporary, &temporary_depth, no_sky};
    mc_overworld_generation_resources resources = {&world, &materials, opacity, 256u, NULL, NULL};
    memset(bootstrap_blocks, 0xa7, sizeof(bootstrap_blocks));
    return mc_overworld_generator_generate(&generator, 0, 0, &resources, &bootstrap,
                                           bootstrap_blocks,
                                           sizeof(bootstrap_blocks)) == MC_OVERWORLD_GENERATOR_OK &&
           empty.count == 0 && empty.insertion_count == 0 && temporary.count == 0 &&
           temporary_depth == 0;
}

static bool read_input(FILE *file) {
    char magic[4];
    uint32_t nc, ne, ni, np, nq, no;
    if (!read_bytes(file, magic, sizeof(magic)) || memcmp(magic, "MCA1", 4) != 0 ||
        !read_i64(file, &seed) || !read_boolean(file, &dedicated) ||
        !read_boolean(file, &world_force) || !read_boolean(file, &provider_force) ||
        !read_i32(file, &spawn_x) || !read_i32(file, &spawn_z) || !read_i64(file, &world_time) ||
        !read_i32(file, &depth) || !read_boolean(file, &no_sky) || !read_u32(file, &nc) ||
        !read_u32(file, &ne) || !read_u32(file, &ni) || !read_u32(file, &np) ||
        !read_u32(file, &nq) || !read_u32(file, &no) || nc == 0 || nc > MAX_IMPORTED ||
        ne > MAX_ENTRIES || ni > MAX_ENTRIES || np > MAX_ENTRIES || nq > MAX_QUEUE ||
        no > MAX_OPERATIONS)
        return false;
    if (mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) != MC_BIOME_OK ||
        mc_biome_manager_init(&manager, seed, &memory) != MC_BIOME_OK) {
        runtime_failed = true;
        return false;
    }
    for (size_t i = 0; i < 256u; ++i) {
        if (!read_i32(file, &opacity[i]) || opacity[i] < 0)
            return false;
    }
    for (size_t i = 0; i < MC_OVERWORLD_GENERATOR_BIOME_MATERIALS; ++i) {
        if (!read_bytes(file, &materials.by_biome[i].top, 1) ||
            !read_bytes(file, &materials.by_biome[i].filler, 1))
            return false;
    }
    if (!mc_math_trig_init(&trig) ||
        mc_overworld_generator_init(&generator, seed, &manager, &trig) !=
            MC_OVERWORLD_GENERATOR_OK ||
        !bootstrap_generator()) {
        runtime_failed = true;
        return false;
    }
    if (!read_doubles(file, &manager.a) || !read_doubles(file, &manager.b) ||
        !read_doubles(file, &manager.c) || !read_biomes(file))
        return false;
    memset(blocks, 0xa7, sizeof(blocks));
    for (size_t i = 0; i < nc; ++i) {
        mc_chunk_state *state = &chunks[i].chunk;
        mc_chunk_data *data = &chunks[i].data;
        if (!read_i32(file, &state->x) || !read_i32(file, &state->z) ||
            !read_boolean(file, &state->blank) || !read_boolean(file, &state->dirty) ||
            !read_boolean(file, &state->active) || !read_boolean(file, &state->terrain_populated) ||
            !read_boolean(file, &state->never_save) || !read_boolean(file, &state->has_entities) ||
            !read_i64(file, &state->last_save_time) || !read_i32(file, &data->minimum_height) ||
            !read_bytes(file, data->heightmap, sizeof(data->heightmap)))
            return false;
        if (!state->blank) {
            state->data = data;
            data->blocks = blocks[i];
            data->blocks_capacity = BLOCK_CAPACITY;
            if (!read_bytes(file, blocks[i], MC_CHUNK_BLOCKS) ||
                !read_bytes(file, data->metadata, sizeof(data->metadata)) ||
                !read_bytes(file, data->skylight, sizeof(data->skylight)) ||
                !read_bytes(file, data->block_light, sizeof(data->block_light)))
                return false;
        }
    }
    if (!chunks[0].chunk.blank ||
        !mc_chunk_cache_init(&cache, entries, MAX_ENTRIES, insertions, MAX_ENTRIES) ||
        !mc_chunk_pending_init(&pending, pending_keys, MAX_ENTRIES) ||
        !mc_light_queue_init(&queue, regions, MAX_QUEUE))
        return false;
    for (size_t i = 0; i < ne; ++i) {
        uint32_t id;
        if (!read_i32(file, &entries[i].key) || !read_u32(file, &id) || id >= nc)
            return false;
        entries[i].chunk = &chunks[id].chunk;
    }
    for (size_t i = 0; i < ni; ++i) {
        uint32_t id;
        if (!read_u32(file, &id) || id >= nc)
            return false;
        insertions[i] = &chunks[id].chunk;
    }
    for (size_t i = 0; i < np; ++i) {
        if (!read_i32(file, &pending_keys[i]))
            return false;
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
    for (size_t i = 0; i < no; ++i) {
        if (!read_u32(file, &operations[i].method) || operations[i].method > 3u ||
            !read_i32(file, &operations[i].x) || !read_i32(file, &operations[i].z))
            return false;
    }
    cache.count = ne;
    cache.insertion_count = ni;
    pending.count = np;
    queue.count = nq;
    imported_count = chunk_count = nc;
    operation_count = no;
    lighting = (mc_world_lighting){&cache, &queue, &depth, no_sky};
    acquisition = (mc_chunk_acquire_context){.cache = &cache,
                                             .pending = &pending,
                                             .fallback = &chunks[0].chunk,
                                             .world_time = &world_time,
                                             .world_force_load = &world_force,
                                             .provider_force_load = &provider_force,
                                             .spawn_x = &spawn_x,
                                             .spawn_z = &spawn_z,
                                             .policy = dedicated ? MC_CHUNK_ACQUIRE_DEDICATED
                                                                 : MC_CHUNK_ACQUIRE_BASE,
                                             .world = &lighting,
                                             .loader = &lighting,
                                             .provider = &generator,
                                             .load = load_miss,
                                             .generate = generate_chunk,
                                             .register_tile_entities = register_tile_entities,
                                             .register_entities = register_entities,
                                             .populate = unexpected_population};
    return fgetc(file) == EOF && !ferror(file) && mc_world_lighting_validate(&lighting) &&
           mc_chunk_acquire_validate(&acquisition);
}

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

static uint64_t double_bits(const double *value) {
    uint64_t raw;
    memcpy(&raw, value, sizeof(raw));
    return raw;
}

static void print_doubles(const double *values, size_t length) {
    if (values == NULL) {
        emit("null");
        return;
    }
    emit("{\"length\":%zu,\"values\":\"", length);
    for (size_t i = 0; i < length; ++i)
        emit("%016" PRIx64, double_bits(&values[i]));
    emit("\"}");
}

static void print_biomes(const mc_biome_id *values, size_t length) {
    if (values == NULL) {
        emit("null");
        return;
    }
    emit("{\"length\":%zu,\"values\":\"", length);
    for (size_t i = 0; i < length; ++i)
        emit("%c", mc_biome_field(values[i]));
    emit("\"}");
}

static void print_manager(void) {
    emit("{\"a\":");
    print_doubles(manager.a.data, manager.a.length);
    emit(",\"b\":");
    print_doubles(manager.b.data, manager.b.length);
    emit(",\"c\":");
    print_doubles(manager.c.data, manager.c.length);
    emit(",\"d\":");
    print_biomes(manager.d.data, manager.d.length);
    emit("}");
}

static void print_random(mc_java_random random) {
    emit("{\"state\":\"%012" PRIx64 "\",\"next_longs\":[", random.state);
    for (size_t i = 0; i < 8u; ++i)
        emit("%s\"%016" PRIx64 "\"", i ? "," : "", (uint64_t)mc_java_random_next_i64(&random));
    emit("]}");
}

static void print_noise(void) {
    static const char names[] = "klmnoabc";
    static const size_t counts[] = {16, 16, 8, 4, 4, 10, 16, 8};
    const mc_terrain_overworld_state *state = &generator.terrain;
    const mc_noise_perlin *channels[] = {state->low_layers,
                                         state->high_layers,
                                         state->mix_layers,
                                         state->surface_primary_layers,
                                         state->surface_secondary_layers,
                                         state->column_scale_layers,
                                         state->column_bias_layers,
                                         state->later_layers};
    emit("{");
    for (size_t channel = 0; channel < 8u; ++channel) {
        emit("%s\"%c\":[", channel ? "," : "", names[channel]);
        for (size_t layer = 0; layer < counts[channel]; ++layer) {
            const mc_noise_perlin *noise = &channels[channel][layer];
            emit("%s{\"offsets\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                 "\"],\"permutation\":[",
                 layer ? "," : "", double_bits(&noise->offset_x), double_bits(&noise->offset_y),
                 double_bits(&noise->offset_z));
            for (size_t i = 0; i < 512u; ++i)
                emit("%s%u", i ? "," : "", (unsigned int)noise->permutation[i]);
            emit("]}");
        }
        emit("]");
    }
    emit("}");
}

static void print_bytes(const char *name, const uint8_t *values, size_t length) {
    emit("\"%s\":{\"length\":%zu,\"values\":\"", name, length);
    for (size_t i = 0; i < length; ++i)
        emit("%02x", (unsigned int)values[i]);
    emit("\"}");
}

static void print_data(const mc_chunk_data *data) {
    emit("{");
    print_bytes("blocks", data->blocks, MC_CHUNK_BLOCKS);
    emit(",");
    print_bytes("metadata", data->metadata, sizeof(data->metadata));
    emit(",");
    print_bytes("skylight", data->skylight, sizeof(data->skylight));
    emit(",");
    print_bytes("block_light", data->block_light, sizeof(data->block_light));
    emit(",");
    print_bytes("heightmap", data->heightmap, sizeof(data->heightmap));
    emit(",\"minimum_height\":%" PRId32 "}", data->minimum_height);
}

static void print_generator(void) {
    emit("{\"noise\":");
    print_noise();
    emit(",\"scratch\":{");
    static const char names[] = "qrstdefgh";
    const double *scratch[] = {generator.density,   generator.sand,         generator.gravel,
                               generator.thickness, generator.mix,          generator.low,
                               generator.high,      generator.column_scale, generator.column_bias};
    static const size_t lengths[] = {425, 256, 256, 256, 425, 425, 425, 25, 25};
    for (size_t i = 0; i < 9u; ++i) {
        emit("%s\"%c\":", i ? "," : "", names[i]);
        print_doubles(scratch[i], lengths[i]);
    }
    emit("},\"biomes\":");
    print_biomes(generator.biomes.data, generator.biomes.length);
    emit(",\"materials\":\"");
    for (size_t i = 0; i < 256u; ++i)
        emit("%02x%02x", (unsigned int)generator.materials[i].top,
             (unsigned int)generator.materials[i].filler);
    emit("\",\"generator_random\":");
    print_random(generator.generator_random);
    emit(",\"cave_random\":");
    print_random(generator.cave_random);
    emit("}");
}

static void print_snapshot(void) {
    /* Map and pending have membership semantics, not Java bucket iteration.
     * Their projection alone is signed-key sorted; list and queue remain exact. */
    size_t order[MAX_ENTRIES];
    for (size_t i = 0; i < cache.count; ++i)
        order[i] = i;
    for (size_t i = 1; i < cache.count; ++i) {
        size_t j = i;
        while (j > 0 && entries[order[j - 1u]].key > entries[order[j]].key) {
            size_t swap = order[j];
            order[j] = order[j - 1u];
            order[--j] = swap;
        }
    }
    emit("{\"world\":{\"map\":[");
    for (size_t i = 0; i < cache.count; ++i)
        emit("%s[%" PRId32 ",%zu]", i ? "," : "", entries[order[i]].key,
             chunk_id(entries[order[i]].chunk));
    emit("],\"insertions\":[");
    for (size_t i = 0; i < cache.insertion_count; ++i)
        emit("%s%zu", i ? "," : "", chunk_id(insertions[i]));
    int32_t keys[MAX_ENTRIES];
    memcpy(keys, pending_keys, pending.count * sizeof(*keys));
    for (size_t i = 1; i < pending.count; ++i) {
        size_t j = i;
        while (j > 0 && keys[j - 1u] > keys[j]) {
            int32_t swap = keys[j];
            keys[j] = keys[j - 1u];
            keys[--j] = swap;
        }
    }
    emit("],\"pending\":[");
    for (size_t i = 0; i < pending.count; ++i)
        emit("%s%" PRId32, i ? "," : "", keys[i]);
    emit("],\"chunks\":[");
    for (size_t i = 0; i < chunk_count; ++i) {
        const mc_chunk_state *state = &chunks[i].chunk;
        emit("%s{\"id\":%zu,\"coordinates\":[%" PRId32 ",%" PRId32
             "],\"blank\":%s,\"flags\":{\"c\":%s,\"n\":%s,\"o\":%s,\"p\":%s,\"q\":%s},\"time\":\"%"
             "016" PRIx64 "\",\"data\":",
             i ? "," : "", i, state->x, state->z, boolean(state->blank), boolean(state->active),
             boolean(state->terrain_populated), boolean(state->dirty), boolean(state->never_save),
             boolean(state->has_entities), (uint64_t)state->last_save_time);
        if (state->blank)
            emit("null");
        else
            print_data(state->data);
        emit("}");
    }
    emit("],\"queue\":[");
    for (size_t i = 0; i < queue.count; ++i) {
        const mc_light_region *region = &regions[i];
        const mc_light_bounds *bounds = &region->bounds;
        emit("%s[%u,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]",
             i ? "," : "", (unsigned int)region->kind, bounds->min_x, bounds->min_y, bounds->min_z,
             bounds->max_x, bounds->max_y, bounds->max_z);
    }
    emit("],\"depth\":%" PRId32 "},\"generator\":", depth);
    print_generator();
    emit(",\"manager\":");
    print_manager();
    emit("}");
}

static void print_callbacks(void) {
    emit("[");
    for (size_t i = 0; i < callback_count; ++i) {
        const callback_record *record = &callbacks[i];
        emit("%s{\"kind\":\"%s\",\"x\":%" PRId32 ",\"z\":%" PRId32 ",\"id\":", i ? "," : "",
             record->kind, record->x, record->z);
        if (record->id == SIZE_MAX)
            emit("null");
        else
            emit("%zu", record->id);
        emit(",\"slice\":");
        if (record->slice == SIZE_MAX)
            emit("null");
        else
            emit("%zu", record->slice);
        emit(",\"map_count\":%zu,\"list_count\":%zu,\"pending_count\":%zu,\"active\":",
             record->map_count, record->list_count, record->pending_count);
        if (record->id == SIZE_MAX)
            emit("null,\"dirty\":null,\"populated\":null,\"last_save_time\":null}");
        else
            emit("%s,\"dirty\":%s,\"populated\":%s,\"last_save_time\":\"%016" PRIx64 "\"}",
                 boolean(record->active), boolean(record->dirty), boolean(record->populated),
                 (uint64_t)record->time);
    }
    emit("]");
}

static void print_finishes(size_t start) {
    emit("[");
    for (size_t i = start; i < finish_count; ++i) {
        emit("%s{\"id\":%zu,\"queries\":[", i != start ? "," : "", finish_ids[i]);
        for (size_t j = 0; j < MC_CHUNK_FINISH_QUERIES; ++j) {
            const mc_chunk_height_query *query = &queries[i][j];
            emit("%s[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]", j ? "," : "", query->x,
                 query->z, query->local_height, query->world_height);
        }
        emit("]}");
    }
    emit("]");
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fputs("usage: acquire_probe [MCA1-file]; otherwise reads binary stdin\n", stderr);
        return 2;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return 2;
#endif
    FILE *file = stdin;
    if (argc == 2) {
#ifdef _MSC_VER
        if (fopen_s(&file, argv[1], "rb") != 0)
            file = NULL;
#else
        file = fopen(argv[1], "rb");
#endif
    }
    if (file == NULL) {
        fputs("acquire probe cannot open MCA1 input\n", stderr);
        return 2;
    }
    int status = 2;
    bool valid = read_input(file);
    if (file != stdin && fclose(file) != 0)
        valid = false;
    if (!valid) {
        if (runtime_failed)
            status = 1;
        goto cleanup;
    }
    status = 1;
    original_d = manager.d.data;
    original_d_length = manager.d.length;
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
    if (!suffixes_retained() || generator.biomes.data == NULL || generator.biomes.length != 256u)
        goto cleanup;
    emit("{\"schema\":1,\"seed_hex\":\"%016" PRIx64 "\",\"initial\":", (uint64_t)seed);
    print_snapshot();
    emit(",\"operations\":[");
    bool complete = true;
    static const char *const names[] = {"b", "c", "d", "populate"};
    for (size_t i = 0; i < operation_count && !output_failed; ++i) {
        const operation *op = &operations[i];
        callback_count = 0;
        const size_t first_finish = finish_count, prior_count = chunk_count;
        save_payloads();
        save_generator();
        mc_chunk_state *out = NULL;
        mc_chunk_acquire_status acquired;
        if (op->method == 0u)
            acquired = mc_chunk_acquire_get(&acquisition, op->x, op->z, &out);
        else if (op->method == 1u)
            acquired = mc_chunk_acquire(&acquisition, op->x, op->z, &out);
        else if (op->method == 2u)
            acquired = mc_chunk_acquire_mark_drop(&acquisition, op->x, op->z);
        else
            acquired = mc_chunk_acquire_populate(&acquisition, op->x, op->z);
        if (callback_failed || !immutable_retained() || !suffixes_retained() ||
            (acquired == MC_CHUNK_ACQUIRE_OK && finish_count == first_finish &&
             (!payloads_retained(prior_count) || !generator_retained()))) {
            fputs("acquire probe owner/retention invariant failed\n", stderr);
            goto cleanup;
        }
        emit("%s{\"index\":%zu,\"method\":\"%s\",\"coordinates\":[%" PRId32 ",%" PRId32
             "],\"result\":",
             i ? "," : "", i, names[op->method], op->x, op->z);
        if (out == NULL)
            emit("null");
        else
            emit("%zu", chunk_id(out));
        emit(",\"status\":\"%s\",\"snapshot\":", mc_chunk_acquire_status_string(acquired));
        print_snapshot();
        emit(",\"callbacks\":");
        print_callbacks();
        emit(",\"finishes\":");
        print_finishes(first_finish);
        emit("}");
        if (acquired != MC_CHUNK_ACQUIRE_OK) {
            fprintf(stderr, "acquire probe operation %zu: %s\n", i,
                    mc_chunk_acquire_status_string(acquired));
            complete = false;
            break;
        }
    }
    emit("]}\n");
    if (complete && !output_failed && !ferror(stdout) && fflush(stdout) == 0)
        status = 0;
cleanup:
    mc_overworld_generator_free(&generator);
    mc_biome_manager_free(&manager);
    if (memory.live_allocation_bytes != 0) {
        fputs("acquire probe biome allocation accounting did not return to zero\n", stderr);
        status = 1;
    }
    if (output_failed)
        fprintf(stderr, "acquire probe output exceeds %u bytes or stream failed\n",
                MAX_OUTPUT_BYTES);
    if (status != 0)
        fprintf(stderr, "acquire probe failed (%d; imported=%zu)\n", status, imported_count);
    return status;
}
