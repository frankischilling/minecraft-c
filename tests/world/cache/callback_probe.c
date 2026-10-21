#include "mc/world/cache/acquire.h"
#include "mc/world/lighting/queue.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define MAX_CHUNKS 32u
#define MAX_ENTRIES 64u
#define MAX_QUEUE 32768u
#define MAX_OPERATIONS 16u
#define MAX_SCRIPTS 32u
#define MAX_CALLBACKS 256u
#define MAX_BYTES (64u * 1024u * 1024u)
#define BLOCK_CAPACITY (MC_CHUNK_BLOCKS + 3u)

/* Trusted callback-boundary replay, deliberately separate from acquire_probe.
 * Decoding, generation and population feature bodies are observed external
 * effects. Every complete callback entry state must match before importing its
 * effects. C alone performs publication, activation, gates and coordinator flags.
 * Payload owners are static/heap storage, never large Windows stack frames. */
typedef struct chunk_record {
    mc_chunk_state state;
    mc_chunk_data data;
    uint8_t blocks[BLOCK_CAPACITY];
} chunk_record;

typedef struct map_record {
    int32_t key;
    uint32_t id;
} map_record;

typedef struct world_record {
    int64_t time;
    bool world_force, provider_force, no_sky;
    int32_t spawn_x, spawn_z, depth;
    uint32_t chunk_count, map_count, list_count, pending_count, queue_count;
    chunk_record *chunks;
    map_record *map;
    uint32_t *list;
    int32_t *pending;
    mc_light_region *queue;
} world_record;

typedef struct operation {
    uint32_t method;
    int32_t x, z;
} operation;

typedef struct script {
    uint32_t kind, result, returned;
    int32_t x, z;
    world_record before, after;
} script;

typedef struct callback_record {
    const char *kind, *effect;
    int32_t x, z;
    size_t id, slice, script, map_count, list_count, pending_count;
    bool active, dirty, populated;
    int64_t time;
} callback_record;

static chunk_record chunks[MAX_CHUNKS];
static mc_chunk_cache_entry entries[MAX_ENTRIES];
static mc_chunk_state *insertions[MAX_ENTRIES];
static int32_t pending_keys[MAX_ENTRIES];
static mc_light_region regions[MAX_QUEUE];
static mc_chunk_cache cache;
static mc_chunk_pending pending;
static mc_chunk_acquire_context context, saved_context;
static world_record initial;
static operation operations[MAX_OPERATIONS];
static script scripts[MAX_SCRIPTS];
static callback_record callbacks[MAX_CALLBACKS];
static uint32_t operation_count, script_count;
static size_t chunk_count, queue_count, script_cursor, callback_count, input_bytes, output_bytes;
static int64_t seed, world_time;
static int32_t spawn_x, spawn_z, depth;
static bool dedicated, world_force, provider_force, no_sky;
static bool allocation_failed, callback_failed, output_failed;
static int world_owner, loader_owner, provider_owner;
static bool publication_known[MAX_CHUNKS];
static int32_t publication_x[MAX_CHUNKS], publication_z[MAX_CHUNKS];

static void emit(const char *format, ...) {
    if (output_failed)
        return;
    char buffer[512];
    va_list arguments;
    va_start(arguments, format);
    int count = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (count < 0 || (size_t)count >= sizeof(buffer) || (size_t)count > MAX_BYTES - output_bytes) {
        output_failed = true;
        return;
    }
    output_bytes += (size_t)count;
    if (fwrite(buffer, 1, (size_t)count, stdout) != (size_t)count)
        output_failed = true;
}

static bool read_bytes(FILE *file, void *data, size_t count) {
    if (count > MAX_BYTES - input_bytes)
        return false;
    input_bytes += count;
    return fread(data, 1, count, file) == count;
}

static bool read_u32(FILE *file, uint32_t *value) {
    uint8_t bytes[4];
    if (!read_bytes(file, bytes, sizeof(bytes)))
        return false;
    *value =
        (uint32_t)bytes[0] << 24u | (uint32_t)bytes[1] << 16u | (uint32_t)bytes[2] << 8u | bytes[3];
    return true;
}

static bool read_i32(FILE *file, int32_t *value) {
    uint32_t raw;
    if (!read_u32(file, &raw))
        return false;
    memcpy(value, &raw, sizeof(raw));
    return true;
}

static bool read_i64(FILE *file, int64_t *value) {
    uint8_t bytes[8];
    uint64_t raw = 0;
    if (!read_bytes(file, bytes, sizeof(bytes)))
        return false;
    for (size_t i = 0; i < sizeof(bytes); ++i)
        raw = (raw << 8u) | bytes[i];
    memcpy(value, &raw, sizeof(raw));
    return true;
}

static bool read_bool(FILE *file, bool *value) {
    uint8_t raw;
    if (!read_bytes(file, &raw, 1) || raw > 1u)
        return false;
    *value = raw != 0;
    return true;
}

static void *allocate(size_t count, size_t size) {
    if (count == 0)
        return NULL;
    void *result = calloc(count, size);
    if (result == NULL)
        allocation_failed = true;
    return result;
}

static void free_world(world_record *world) {
    free(world->chunks);
    free(world->map);
    free(world->list);
    free(world->pending);
    free(world->queue);
    memset(world, 0, sizeof(*world));
}

static bool read_chunk(FILE *file, chunk_record *chunk) {
    mc_chunk_state *state = &chunk->state;
    mc_chunk_data *data = &chunk->data;
    memset(chunk->blocks, 0xa7, sizeof(chunk->blocks));
    if (!read_i32(file, &state->x) || !read_i32(file, &state->z) ||
        !read_bool(file, &state->blank) || !read_bool(file, &state->dirty) ||
        !read_bool(file, &state->active) || !read_bool(file, &state->terrain_populated) ||
        !read_bool(file, &state->never_save) || !read_bool(file, &state->has_entities) ||
        !read_i64(file, &state->last_save_time) || !read_i32(file, &data->minimum_height) ||
        !read_bytes(file, data->heightmap, sizeof(data->heightmap)))
        return false;
    if (!state->blank) {
        state->data = data;
        data->blocks = chunk->blocks;
        data->blocks_capacity = BLOCK_CAPACITY;
        if (!read_bytes(file, chunk->blocks, MC_CHUNK_BLOCKS) ||
            !read_bytes(file, data->metadata, sizeof(data->metadata)) ||
            !read_bytes(file, data->skylight, sizeof(data->skylight)) ||
            !read_bytes(file, data->block_light, sizeof(data->block_light)))
            return false;
    }
    return true;
}

static bool read_world(FILE *file, world_record *world) {
    if (!read_i64(file, &world->time) || !read_bool(file, &world->world_force) ||
        !read_bool(file, &world->provider_force) || !read_i32(file, &world->spawn_x) ||
        !read_i32(file, &world->spawn_z) || !read_i32(file, &world->depth) ||
        !read_bool(file, &world->no_sky) || !read_u32(file, &world->chunk_count) ||
        !read_u32(file, &world->map_count) || !read_u32(file, &world->list_count) ||
        !read_u32(file, &world->pending_count) || !read_u32(file, &world->queue_count) ||
        world->chunk_count == 0 || world->chunk_count > MAX_CHUNKS ||
        world->map_count > MAX_ENTRIES || world->list_count > MAX_ENTRIES ||
        world->pending_count > MAX_ENTRIES || world->queue_count > MAX_QUEUE ||
        (!dedicated && world->provider_force))
        return false;
    world->chunks = allocate(world->chunk_count, sizeof(*world->chunks));
    world->map = allocate(world->map_count, sizeof(*world->map));
    world->list = allocate(world->list_count, sizeof(*world->list));
    world->pending = allocate(world->pending_count, sizeof(*world->pending));
    world->queue = allocate(world->queue_count, sizeof(*world->queue));
    if (allocation_failed)
        return false;
    for (size_t i = 0; i < world->chunk_count; ++i) {
        if (!read_chunk(file, &world->chunks[i]))
            return false;
    }
    if (!world->chunks[0].state.blank)
        return false;
    for (size_t i = 0; i < world->map_count; ++i) {
        if (!read_i32(file, &world->map[i].key) || !read_u32(file, &world->map[i].id) ||
            world->map[i].id >= world->chunk_count)
            return false;
        for (size_t j = 0; j < i; ++j) {
            if (world->map[j].key == world->map[i].key)
                return false;
        }
    }
    for (size_t i = 0; i < world->list_count; ++i) {
        if (!read_u32(file, &world->list[i]) || world->list[i] >= world->chunk_count)
            return false;
    }
    for (size_t i = 0; i < world->pending_count; ++i) {
        if (!read_i32(file, &world->pending[i]))
            return false;
        for (size_t j = 0; j < i; ++j) {
            if (world->pending[j] == world->pending[i])
                return false;
        }
    }
    for (size_t i = 0; i < world->queue_count; ++i) {
        uint32_t kind;
        mc_light_bounds *bounds = &world->queue[i].bounds;
        if (!read_u32(file, &kind) || kind > 1u || !read_i32(file, &bounds->min_x) ||
            !read_i32(file, &bounds->min_y) || !read_i32(file, &bounds->min_z) ||
            !read_i32(file, &bounds->max_x) || !read_i32(file, &bounds->max_y) ||
            !read_i32(file, &bounds->max_z))
            return false;
        world->queue[i].kind = (mc_light_kind)kind;
    }
    return true;
}

static bool identity_growth(const world_record *before, const world_record *after) {
    if (after->chunk_count < before->chunk_count)
        return false;
    for (size_t i = 0; i < before->chunk_count; ++i) {
        const mc_chunk_state *a = &before->chunks[i].state, *b = &after->chunks[i].state;
        if (a->x != b->x || a->z != b->z || a->blank != b->blank)
            return false;
    }
    return true;
}

static bool read_input(FILE *file) {
    char magic[4];
    if (!read_bytes(file, magic, sizeof(magic)) || memcmp(magic, "MCC1", 4) != 0 ||
        !read_i64(file, &seed) || !read_bool(file, &dedicated) ||
        !read_u32(file, &operation_count) || operation_count == 0 ||
        operation_count > MAX_OPERATIONS || !read_u32(file, &script_count) ||
        script_count > MAX_SCRIPTS || !read_world(file, &initial))
        return false;
    for (size_t i = 0; i < operation_count; ++i) {
        if (!read_u32(file, &operations[i].method) || operations[i].method > 3u ||
            !read_i32(file, &operations[i].x) || !read_i32(file, &operations[i].z))
            return false;
    }
    for (size_t i = 0; i < script_count; ++i) {
        script *item = &scripts[i];
        if (!read_u32(file, &item->kind) || item->kind > 2u || !read_i32(file, &item->x) ||
            !read_i32(file, &item->z) || !read_u32(file, &item->result) ||
            !read_u32(file, &item->returned) || !read_world(file, &item->before) ||
            !read_world(file, &item->after) || !identity_growth(&item->before, &item->after))
            return false;
        bool returns_chunk = item->kind == 1u || (item->kind == 0u && item->result == 0u);
        if ((item->kind == 0u ? item->result > 1u : item->result != 0u) ||
            (returns_chunk ? item->returned >= item->after.chunk_count
                           : item->returned != UINT32_MAX))
            return false;
        if (returns_chunk && (item->returned != item->before.chunk_count ||
                              item->after.chunk_count != item->before.chunk_count + 1u ||
                              item->after.chunks[item->returned].state.blank))
            return false;
    }
    return fgetc(file) == EOF && !ferror(file);
}

static size_t chunk_id(const mc_chunk_state *state) {
    if (state != NULL) {
        for (size_t i = 0; i < chunk_count; ++i) {
            if (state == &chunks[i].state)
                return i;
        }
    }
    return SIZE_MAX;
}

static bool suffixes_retained(void) {
    if (cache.entries != entries || cache.capacity != MAX_ENTRIES ||
        cache.insertions != insertions || cache.insertion_capacity != MAX_ENTRIES ||
        pending.keys != pending_keys || pending.capacity != MAX_ENTRIES ||
        cache.count > MAX_ENTRIES || cache.insertion_count > MAX_ENTRIES ||
        pending.count > MAX_ENTRIES || chunk_count > MAX_CHUNKS || queue_count > MAX_QUEUE)
        return false;
    for (size_t i = 0; i < MAX_CHUNKS; ++i) {
        for (size_t j = MC_CHUNK_BLOCKS; j < BLOCK_CAPACITY; ++j) {
            if (chunks[i].blocks[j] != 0xa7)
                return false;
        }
        if (i < chunk_count &&
            (chunks[i].state.blank ? chunks[i].state.data != NULL
                                   : (chunks[i].state.data != &chunks[i].data ||
                                      chunks[i].data.blocks != chunks[i].blocks ||
                                      chunks[i].data.blocks_capacity != BLOCK_CAPACITY)))
            return false;
    }
    return true;
}

static bool equal_chunk(const chunk_record *a, const chunk_record *b) {
    const mc_chunk_state *x = &a->state, *y = &b->state;
    if (x->x != y->x || x->z != y->z || x->blank != y->blank || x->dirty != y->dirty ||
        x->active != y->active || x->terrain_populated != y->terrain_populated ||
        x->never_save != y->never_save || x->has_entities != y->has_entities ||
        x->last_save_time != y->last_save_time ||
        a->data.minimum_height != b->data.minimum_height ||
        memcmp(a->data.heightmap, b->data.heightmap, sizeof(a->data.heightmap)) != 0)
        return false;
    return x->blank ||
           (memcmp(a->blocks, b->blocks, MC_CHUNK_BLOCKS) == 0 &&
            memcmp(a->data.metadata, b->data.metadata, sizeof(a->data.metadata)) == 0 &&
            memcmp(a->data.skylight, b->data.skylight, sizeof(a->data.skylight)) == 0 &&
            memcmp(a->data.block_light, b->data.block_light, sizeof(a->data.block_light)) == 0);
}

static bool matches(const world_record *world) {
    if (world->time != world_time || world->world_force != world_force ||
        world->provider_force != provider_force || world->spawn_x != spawn_x ||
        world->spawn_z != spawn_z || world->depth != depth || world->no_sky != no_sky ||
        world->chunk_count != chunk_count || world->map_count != cache.count ||
        world->list_count != cache.insertion_count || world->pending_count != pending.count ||
        world->queue_count != queue_count || !suffixes_retained())
        return false;
    for (size_t i = 0; i < chunk_count; ++i) {
        if (!equal_chunk(&chunks[i], &world->chunks[i]))
            return false;
    }
    for (size_t i = 0; i < world->map_count; ++i) {
        bool found = false;
        for (size_t j = 0; j < cache.count; ++j) {
            if (entries[j].key == world->map[i].key &&
                chunk_id(entries[j].chunk) == world->map[i].id)
                found = true;
        }
        if (!found)
            return false;
    }
    for (size_t i = 0; i < world->list_count; ++i) {
        if (chunk_id(insertions[i]) != world->list[i])
            return false;
    }
    for (size_t i = 0; i < world->pending_count; ++i) {
        if (!mc_chunk_pending_contains(&pending, world->pending[i]))
            return false;
    }
    for (size_t i = 0; i < world->queue_count; ++i) {
        if (regions[i].kind != world->queue[i].kind ||
            memcmp(&regions[i].bounds, &world->queue[i].bounds, sizeof(regions[i].bounds)) != 0)
            return false;
    }
    return true;
}

static bool import_world(const world_record *world) {
    for (size_t i = 0; i < world->chunk_count; ++i) {
        chunks[i] = world->chunks[i];
        if (!chunks[i].state.blank) {
            chunks[i].state.data = &chunks[i].data;
            chunks[i].data.blocks = chunks[i].blocks;
        }
    }
    chunk_count = world->chunk_count;
    world_time = world->time;
    world_force = world->world_force;
    provider_force = world->provider_force;
    spawn_x = world->spawn_x;
    spawn_z = world->spawn_z;
    depth = world->depth;
    no_sky = world->no_sky;
    for (size_t i = 0; i < world->map_count; ++i)
        entries[i] = (mc_chunk_cache_entry){world->map[i].key, &chunks[world->map[i].id].state};
    cache.count = world->map_count;
    for (size_t i = 0; i < world->list_count; ++i)
        insertions[i] = &chunks[world->list[i]].state;
    cache.insertion_count = world->list_count;
    if (world->pending_count != 0)
        memcpy(pending_keys, world->pending, world->pending_count * sizeof(*pending_keys));
    pending.count = world->pending_count;
    if (world->queue_count != 0)
        memcpy(regions, world->queue, world->queue_count * sizeof(*regions));
    queue_count = world->queue_count;
    return suffixes_retained() && mc_chunk_acquire_validate(&context) && matches(world);
}

static bool same_map(const world_record *a, const world_record *b) {
    if (a->map_count != b->map_count)
        return false;
    for (size_t i = 0; i < a->map_count; ++i) {
        bool found = false;
        for (size_t j = 0; j < b->map_count; ++j) {
            if (a->map[i].key == b->map[j].key && a->map[i].id == b->map[j].id)
                found = true;
        }
        if (!found)
            return false;
    }
    return true;
}

static bool same_pending(const world_record *a, const world_record *b) {
    if (a->pending_count != b->pending_count)
        return false;
    for (size_t i = 0; i < a->pending_count; ++i) {
        bool found = false;
        for (size_t j = 0; j < b->pending_count; ++j) {
            if (a->pending[i] == b->pending[j])
                found = true;
        }
        if (!found)
            return false;
    }
    return true;
}

static bool apply_effect(const world_record *before, const world_record *after) {
    if (!matches(before) || !identity_growth(before, after))
        return false;
    /* Preserve every unchanged coordinator field and every owner pointer.
     * Only actual observed body changes enter through this opaque seam. */
    for (size_t i = 0; i < after->chunk_count; ++i) {
        const chunk_record *a = i < before->chunk_count ? &before->chunks[i] : NULL;
        const chunk_record *b = &after->chunks[i];
        chunk_record *live = &chunks[i];
        if (a == NULL) {
            *live = *b;
            if (!live->state.blank) {
                live->state.data = &live->data;
                live->data.blocks = live->blocks;
            }
            continue;
        }
#define APPLY_FLAG(field)                                                                          \
    if (a->state.field != b->state.field)                                                          \
    live->state.field = b->state.field
        APPLY_FLAG(dirty);
        APPLY_FLAG(active);
        APPLY_FLAG(terrain_populated);
        APPLY_FLAG(never_save);
        APPLY_FLAG(has_entities);
        APPLY_FLAG(last_save_time);
#undef APPLY_FLAG
        if (a->data.minimum_height != b->data.minimum_height)
            live->data.minimum_height = b->data.minimum_height;
#define APPLY_ARRAY(field)                                                                         \
    if (memcmp(a->data.field, b->data.field, sizeof(a->data.field)) != 0)                          \
    memcpy(live->data.field, b->data.field, sizeof(live->data.field))
        APPLY_ARRAY(heightmap);
        if (!live->state.blank) {
            APPLY_ARRAY(metadata);
            APPLY_ARRAY(skylight);
            APPLY_ARRAY(block_light);
            if (memcmp(a->blocks, b->blocks, MC_CHUNK_BLOCKS) != 0)
                memcpy(live->blocks, b->blocks, MC_CHUNK_BLOCKS);
        }
#undef APPLY_ARRAY
    }
    chunk_count = after->chunk_count;
#define APPLY_WORLD(field, live)                                                                   \
    if (before->field != after->field)                                                             \
    live = after->field
    APPLY_WORLD(time, world_time);
    APPLY_WORLD(world_force, world_force);
    APPLY_WORLD(provider_force, provider_force);
    APPLY_WORLD(spawn_x, spawn_x);
    APPLY_WORLD(spawn_z, spawn_z);
    APPLY_WORLD(depth, depth);
    APPLY_WORLD(no_sky, no_sky);
#undef APPLY_WORLD
    if (!same_map(before, after)) {
        for (size_t i = 0; i < after->map_count; ++i)
            entries[i] = (mc_chunk_cache_entry){after->map[i].key, &chunks[after->map[i].id].state};
        cache.count = after->map_count;
    }
    if (before->list_count != after->list_count ||
        (before->list_count != 0 &&
         memcmp(before->list, after->list, before->list_count * sizeof(*before->list)) != 0)) {
        for (size_t i = 0; i < after->list_count; ++i)
            insertions[i] = &chunks[after->list[i]].state;
        cache.insertion_count = after->list_count;
    }
    if (!same_pending(before, after)) {
        if (after->pending_count != 0)
            memcpy(pending_keys, after->pending, after->pending_count * sizeof(*pending_keys));
        pending.count = after->pending_count;
    }
    if (before->queue_count != after->queue_count ||
        (before->queue_count != 0 &&
         memcmp(before->queue, after->queue, before->queue_count * sizeof(*before->queue)) != 0)) {
        if (after->queue_count != 0)
            memcpy(regions, after->queue, after->queue_count * sizeof(*regions));
        queue_count = after->queue_count;
    }
    return suffixes_retained() && mc_chunk_acquire_validate(&context) && matches(after);
}

static bool log_callback(const char *kind, const char *effect, int32_t x, int32_t z,
                         const mc_chunk_state *chunk, size_t slice, size_t script_index) {
    size_t id = chunk_id(chunk);
    if (callback_count == MAX_CALLBACKS || (chunk != NULL && id == SIZE_MAX)) {
        callback_failed = true;
        return false;
    }
    callbacks[callback_count++] = (callback_record){kind,
                                                    effect,
                                                    x,
                                                    z,
                                                    id,
                                                    slice,
                                                    script_index,
                                                    cache.count,
                                                    cache.insertion_count,
                                                    pending.count,
                                                    chunk != NULL && chunk->active,
                                                    chunk != NULL && chunk->dirty,
                                                    chunk != NULL && chunk->terrain_populated,
                                                    chunk == NULL ? 0 : chunk->last_save_time};
    return true;
}

static script *next_script(uint32_t kind, const mc_chunk_acquire_context *argument, int32_t x,
                           int32_t z) {
    if (argument != &context || script_cursor == script_count ||
        memcmp(&context, &saved_context, sizeof(context)) != 0) {
        callback_failed = true;
        return NULL;
    }
    script *item = &scripts[script_cursor];
    if (item->kind != kind || item->x != x || item->z != z || !matches(&item->before)) {
        fprintf(stderr,
                "callback probe entry/order mismatch: script %zu kind %u at %" PRId32 ",%" PRId32
                "\n",
                script_cursor, kind, x, z);
        callback_failed = true;
        return NULL;
    }
    ++script_cursor;
    return item;
}

static mc_chunk_load_result load_chunk(void *owner, const mc_chunk_acquire_context *argument,
                                       int32_t x, int32_t z, mc_chunk_state **loaded) {
    if (owner != &loader_owner || loaded == NULL || *loaded != NULL) {
        callback_failed = true;
        return MC_CHUNK_LOAD_FATAL;
    }
    script *item = next_script(0u, argument, x, z);
    size_t index = script_cursor - (item != NULL);
    if (item == NULL ||
        !log_callback("load_enter", "observed-original-replay", x, z, NULL, SIZE_MAX, index) ||
        !apply_effect(&item->before, &item->after)) {
        callback_failed = true;
        return MC_CHUNK_LOAD_FATAL;
    }
    *loaded = item->returned == UINT32_MAX ? NULL : &chunks[item->returned].state;
    if (*loaded != NULL) {
        publication_known[item->returned] = true;
        publication_x[item->returned] = x;
        publication_z[item->returned] = z;
    }
    if (!log_callback("load_return", "observed-original-replay", x, z, *loaded, SIZE_MAX, index))
        return MC_CHUNK_LOAD_FATAL;
    return item->result == 0u ? MC_CHUNK_LOAD_LOADED : MC_CHUNK_LOAD_MISS;
}

static bool generate_chunk(void *owner, const mc_chunk_acquire_context *argument, int32_t x,
                           int32_t z, mc_chunk_state **generated) {
    if (owner != &provider_owner || generated == NULL || *generated != NULL) {
        callback_failed = true;
        return false;
    }
    script *item = next_script(1u, argument, x, z);
    size_t index = script_cursor - (item != NULL);
    if (item == NULL ||
        !log_callback("generate_enter", "observed-original-replay", x, z, NULL, SIZE_MAX, index) ||
        !apply_effect(&item->before, &item->after)) {
        callback_failed = true;
        return false;
    }
    *generated = &chunks[item->returned].state;
    publication_known[item->returned] = true;
    publication_x[item->returned] = x;
    publication_z[item->returned] = z;
    return log_callback("generate_return", "observed-original-replay", x, z, *generated, SIZE_MAX,
                        index);
}

static bool populate_chunk(void *owner, const mc_chunk_acquire_context *argument, int32_t x,
                           int32_t z) {
    mc_chunk_state *target = NULL;
    if (owner != &provider_owner || argument != &context ||
        mc_chunk_acquire_get(argument, x, z, &target) != MC_CHUNK_ACQUIRE_OK ||
        !target->terrain_populated) {
        callback_failed = true;
        return false;
    }
    script *item = next_script(2u, argument, x, z);
    size_t index = script_cursor - (item != NULL);
    if (item == NULL ||
        !log_callback("populate_enter", "observed-original-replay", x, z, target, SIZE_MAX,
                      index) ||
        !apply_effect(&item->before, &item->after)) {
        callback_failed = true;
        return false;
    }
    return log_callback("populate_return", "observed-original-replay", x, z, target, SIZE_MAX,
                        index);
}

static bool register_tiles(void *owner, const mc_chunk_acquire_context *argument,
                           mc_chunk_state *chunk) {
    mc_chunk_state *mapped = NULL;
    const size_t id = chunk_id(chunk);
    if (owner != &world_owner || argument != &context || id == SIZE_MAX || !publication_known[id] ||
        !chunk->active || cache.insertion_count == 0 ||
        insertions[cache.insertion_count - 1u] != chunk ||
        !mc_chunk_cache_lookup(&cache, publication_x[id], publication_z[id], &mapped) ||
        mapped != chunk || memcmp(&context, &saved_context, sizeof(context)) != 0) {
        callback_failed = true;
        return false;
    }
    return log_callback("tiles", "coordinator", chunk->x, chunk->z, chunk, SIZE_MAX, SIZE_MAX);
}

static bool register_entities(void *owner, const mc_chunk_acquire_context *argument,
                              mc_chunk_state *chunk, size_t slice) {
    if (owner != &world_owner || argument != &context || !chunk->active || slice >= 8u ||
        memcmp(&context, &saved_context, sizeof(context)) != 0) {
        callback_failed = true;
        return false;
    }
    return log_callback("entities", "coordinator", chunk->x, chunk->z, chunk, slice, SIZE_MAX);
}

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

static void print_bytes(const char *name, const uint8_t *values, size_t count) {
    emit("\"%s\":{\"length\":%zu,\"values\":\"", name, count);
    for (size_t i = 0; i < count; ++i)
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

static void print_snapshot(void) {
    emit("{\"context\":{\"dedicated\":%s,\"world_time\":\"%016" PRIx64
         "\",\"world_force\":%s,\"provider_force\":%s,\"spawn\":[%" PRId32 ",%" PRId32
         "],\"no_sky\":%s},\"world\":{\"map\":[",
         boolean(dedicated), (uint64_t)world_time, boolean(world_force),
         dedicated ? boolean(provider_force) : "null", spawn_x, spawn_z, boolean(no_sky));
    size_t order[MAX_ENTRIES];
    for (size_t i = 0; i < cache.count; ++i)
        order[i] = i;
    for (size_t i = 1; i < cache.count; ++i) {
        size_t j = i;
        while (j != 0 && entries[order[j - 1u]].key > entries[order[j]].key) {
            size_t swap = order[j];
            order[j] = order[j - 1u];
            order[--j] = swap;
        }
    }
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
        while (j != 0 && keys[j - 1u] > keys[j]) {
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
        const mc_chunk_state *state = &chunks[i].state;
        emit("%s{\"id\":%zu,\"coordinates\":[%" PRId32 ",%" PRId32
             "],\"blank\":%s,\"flags\":{\"c\":%s,\"n\":%s,\"o\":%s,\"p\":%s,\"q\":%s},"
             "\"time\":\"%016" PRIx64 "\",\"data\":",
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
    for (size_t i = 0; i < queue_count; ++i) {
        const mc_light_region *region = &regions[i];
        const mc_light_bounds *b = &region->bounds;
        emit("%s[%u,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]",
             i ? "," : "", (unsigned int)region->kind, b->min_x, b->min_y, b->min_z, b->max_x,
             b->max_y, b->max_z);
    }
    emit("],\"depth\":%" PRId32 "}}", depth);
}

static void print_callbacks(void) {
    emit("[");
    for (size_t i = 0; i < callback_count; ++i) {
        const callback_record *record = &callbacks[i];
        emit("%s{\"kind\":\"%s\",\"effect\":\"%s\",\"x\":%" PRId32 ",\"z\":%" PRId32 ",\"id\":",
             i ? "," : "", record->kind, record->effect, record->x, record->z);
        if (record->id == SIZE_MAX)
            emit("null");
        else
            emit("%zu", record->id);
        emit(",\"slice\":");
        if (record->slice == SIZE_MAX)
            emit("null");
        else
            emit("%zu", record->slice);
        emit(",\"script\":");
        if (record->script == SIZE_MAX)
            emit("null");
        else
            emit("%zu", record->script);
        emit(",\"owner_matched\":true,\"context_matched\":true,\"map_count\":%zu,"
             "\"list_count\":%zu,\"pending_count\":%zu,\"active\":",
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

static bool initialize_context(void) {
    for (size_t i = 0; i < MAX_CHUNKS; ++i)
        memset(chunks[i].blocks, 0xa7, sizeof(chunks[i].blocks));
    if (!mc_chunk_cache_init(&cache, entries, MAX_ENTRIES, insertions, MAX_ENTRIES) ||
        !mc_chunk_pending_init(&pending, pending_keys, MAX_ENTRIES))
        return false;
    context = (mc_chunk_acquire_context){.cache = &cache,
                                         .pending = &pending,
                                         .fallback = &chunks[0].state,
                                         .world_time = &world_time,
                                         .world_force_load = &world_force,
                                         .provider_force_load = &provider_force,
                                         .spawn_x = &spawn_x,
                                         .spawn_z = &spawn_z,
                                         .policy = dedicated ? MC_CHUNK_ACQUIRE_DEDICATED
                                                             : MC_CHUNK_ACQUIRE_BASE,
                                         .world = &world_owner,
                                         .loader = &loader_owner,
                                         .provider = &provider_owner,
                                         .load = load_chunk,
                                         .generate = generate_chunk,
                                         .register_tiles = register_tiles,
                                         .register_entities = register_entities,
                                         .populate = populate_chunk};
    memcpy(&saved_context, &context, sizeof(context));
    return true;
}

/* Constructed diagnostic mode: metadata validity alone does not establish the
 * probe's expected storage owners. These valid alternative descriptors must
 * fail the same invariant used at every real replay callback and operation. */
static int audit_owners(void) {
    static mc_chunk_cache_entry alternate_entries[MAX_ENTRIES];
    static mc_chunk_state *alternate_list[MAX_ENTRIES];
    static int32_t alternate_pending[MAX_ENTRIES];
    static uint8_t alternate_blocks[BLOCK_CAPACITY];
    static mc_chunk_data alternate_data;
    alternate_data.blocks = alternate_blocks;
    alternate_data.blocks_capacity = BLOCK_CAPACITY;
    if (!initialize_context())
        return 1;
    chunks[0].state.blank = true;
    chunk_count = 1;
    if (!mc_chunk_acquire_validate(&context) || !suffixes_retained())
        return 1;
    const mc_chunk_cache saved_cache = cache;
    const mc_chunk_pending saved_pending = pending;
    unsigned int checks = 0;
#define AUDIT_OWNER(change)                                                                        \
    do {                                                                                           \
        change;                                                                                    \
        if (!mc_chunk_acquire_validate(&context) || suffixes_retained())                           \
            return 1;                                                                              \
        ++checks;                                                                                  \
        cache = saved_cache;                                                                       \
        pending = saved_pending;                                                                   \
    } while (0)
    AUDIT_OWNER(cache.entries = alternate_entries);
    AUDIT_OWNER(cache.capacity = MAX_ENTRIES - 1u);
    AUDIT_OWNER(cache.insertions = alternate_list);
    AUDIT_OWNER(cache.insertion_capacity = MAX_ENTRIES - 1u);
    AUDIT_OWNER(pending.keys = alternate_pending);
    AUDIT_OWNER(pending.capacity = MAX_ENTRIES - 1u);
    AUDIT_OWNER(chunks[0].state.data = &chunks[0].data);
    chunks[0].state.data = NULL;
    chunks[1].state.data = &chunks[1].data;
    chunks[1].data.blocks = chunks[1].blocks;
    chunks[1].data.blocks_capacity = BLOCK_CAPACITY;
    chunk_count = 2;
    AUDIT_OWNER(chunks[1].state.data = &alternate_data);
    chunks[1].state.data = &chunks[1].data;
    AUDIT_OWNER(chunks[1].data.blocks = alternate_blocks);
    chunks[1].data.blocks = chunks[1].blocks;
    AUDIT_OWNER(chunks[1].data.blocks_capacity = MC_CHUNK_BLOCKS);
#undef AUDIT_OWNER
    emit("{\"schema\":1,\"scope\":\"constructed-owner-guard\",\"checks\":%u,"
         "\"all_valid_descriptors_rejected\":true}\n",
         checks);
    return output_failed || fflush(stdout) != 0 ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fputs("usage: callback_probe [MCC1-file]; otherwise binary stdin\n", stderr);
        return 2;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return 2;
#endif
    if (argc == 2 && strcmp(argv[1], "--audit-owners") == 0)
        return audit_owners();
    FILE *file = stdin;
    if (argc == 2) {
#ifdef _MSC_VER
        if (fopen_s(&file, argv[1], "rb") != 0)
            file = NULL;
#else
        file = fopen(argv[1], "rb");
#endif
    }
    if (file == NULL)
        return 2;
    bool valid = read_input(file);
    if (file != stdin && fclose(file) != 0)
        valid = false;
    int status = allocation_failed ? 1 : 2;
    if (!valid)
        goto cleanup;
    if (!initialize_context())
        goto cleanup;
    if (!import_world(&initial))
        goto cleanup;
    status = 1;
    emit("{\"schema\":1,\"seed_hex\":\"%016" PRIx64
         "\",\"scope\":\"cache-callback-boundary\",\"initial\":",
         (uint64_t)seed);
    print_snapshot();
    emit(",\"operations\":[");
    bool complete = true;
    static const char *const names[] = {"b", "c", "d", "populate"};
    for (size_t i = 0; i < operation_count && !output_failed; ++i) {
        const operation *op = &operations[i];
        callback_count = 0;
        mc_chunk_state *out = NULL;
        mc_chunk_acquire_status acquired;
        if (op->method == 0u)
            acquired = mc_chunk_acquire_get(&context, op->x, op->z, &out);
        else if (op->method == 1u)
            acquired = mc_chunk_acquire(&context, op->x, op->z, &out);
        else if (op->method == 2u)
            acquired = mc_chunk_acquire_mark_drop(&context, op->x, op->z);
        else
            acquired = mc_chunk_acquire_populate(&context, op->x, op->z);
        if (callback_failed || !suffixes_retained() ||
            memcmp(&context, &saved_context, sizeof(context)) != 0 ||
            !mc_chunk_acquire_validate(&context) || (out != NULL && chunk_id(out) == SIZE_MAX)) {
            fputs("callback probe complete entry/owner/suffix invariant failed\n", stderr);
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
        emit("}");
        if (acquired != MC_CHUNK_ACQUIRE_OK) {
            complete = false;
            break;
        }
    }
    emit("]}\n");
    if (complete && script_cursor == script_count && !output_failed && !ferror(stdout) &&
        fflush(stdout) == 0)
        status = 0;
cleanup:
    free_world(&initial);
    for (size_t i = 0; i < MAX_SCRIPTS; ++i) {
        free_world(&scripts[i].before);
        free_world(&scripts[i].after);
    }
    if (status != 0)
        fprintf(stderr, "callback probe failed (%d; scripts=%zu/%u, input=%zu)\n", status,
                script_cursor, script_count, input_bytes);
    return status;
}
