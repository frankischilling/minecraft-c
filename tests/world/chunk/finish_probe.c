#include "mc/world/chunk/finish.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHUNKS 512u
#define MAX_QUEUE 32768u

static mc_chunk_data measured, cached[MAX_CHUNKS];
static uint8_t blocks[MC_CHUNK_BLOCKS], repeated[MC_CHUNK_BLOCKS];
static mc_chunk_state target, chunks[MAX_CHUNKS];
static mc_chunk_state saved_chunks[MAX_CHUNKS];
static bool is_target[MAX_CHUNKS];
static uint8_t saved_heights[MAX_CHUNKS][MC_CHUNK_HEIGHTMAP_BYTES];
static mc_chunk_cache_entry entries[MAX_CHUNKS], saved_entries[MAX_CHUNKS];
static mc_chunk_state *insertions[MAX_CHUNKS], *saved_insertions[MAX_CHUNKS];
static mc_light_region regions[MAX_QUEUE];
static mc_chunk_height_query queries[MC_CHUNK_FINISH_QUERIES];
static int32_t opacity[256];

static bool bytes(FILE *file, void *data, size_t count) {
    return fread(data, 1, count, file) == count;
}

static bool u32(FILE *file, uint32_t *value) {
    uint8_t data[4];
    if (!bytes(file, data, sizeof(data)))
        return false;
    *value =
        (uint32_t)data[0] << 24u | (uint32_t)data[1] << 16u | (uint32_t)data[2] << 8u | data[3];
    return true;
}

static bool i32(FILE *file, int32_t *value) {
    uint32_t raw;
    if (!u32(file, &raw))
        return false;
    *value = raw <= INT32_MAX ? (int32_t)raw : -1 - (int32_t)(UINT32_MAX - raw);
    return true;
}

static bool boolean(FILE *file, bool *value) {
    int byte = fgetc(file);
    if (byte != 0 && byte != 1)
        return false;
    *value = byte != 0;
    return true;
}

static bool read_input(FILE *file, mc_chunk_cache *cache, mc_light_queue *queue, int32_t *depth,
                       bool *no_sky, size_t *chunk_count) {
    char magic[4];
    uint32_t nc, ne, ni, nq;
    if (!bytes(file, magic, sizeof(magic)) || memcmp(magic, "MCF1", 4) != 0 ||
        !i32(file, &target.x) || !i32(file, &target.z) || !boolean(file, no_sky) ||
        !i32(file, depth) || !u32(file, &nc) || !u32(file, &ne) || !u32(file, &ni) ||
        !u32(file, &nq) || nc > MAX_CHUNKS || ne > MAX_CHUNKS || ni > MAX_CHUNKS ||
        nq > MAX_QUEUE - MC_CHUNK_FINISH_QUERIES) {
        return false;
    }
    target.data = &measured;
    target.blank = false;
    if (!boolean(file, &target.dirty) || !mc_chunk_data_init(&measured, blocks, sizeof(blocks)) ||
        !bytes(file, blocks, sizeof(blocks)) || !bytes(file, repeated, sizeof(repeated)) ||
        !bytes(file, measured.metadata, sizeof(measured.metadata)) ||
        !bytes(file, measured.skylight, sizeof(measured.skylight)) ||
        !bytes(file, measured.block_light, sizeof(measured.block_light)) ||
        !bytes(file, measured.heightmap, sizeof(measured.heightmap)) ||
        !i32(file, &measured.minimum_height)) {
        return false;
    }
    for (size_t i = 0; i < 256u; ++i) {
        if (!i32(file, &opacity[i]) || opacity[i] < 0)
            return false;
    }
    for (size_t i = 0; i < nc; ++i) {
        chunks[i].data = &cached[i];
        if (!i32(file, &chunks[i].x) || !i32(file, &chunks[i].z) ||
            !boolean(file, &chunks[i].blank) || !boolean(file, &chunks[i].dirty) ||
            !boolean(file, &is_target[i]) ||
            !bytes(file, cached[i].heightmap, MC_CHUNK_HEIGHTMAP_BYTES)) {
            return false;
        }
        if (is_target[i]) {
            if (chunks[i].blank || chunks[i].x != target.x || chunks[i].z != target.z ||
                chunks[i].dirty != target.dirty ||
                memcmp(cached[i].heightmap, measured.heightmap, MC_CHUNK_HEIGHTMAP_BYTES) != 0)
                return false;
        }
        saved_chunks[i] = chunks[i];
        memcpy(saved_heights[i], cached[i].heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    }
    if (!mc_chunk_cache_init(cache, entries, MAX_CHUNKS, insertions, MAX_CHUNKS) ||
        !mc_light_queue_init(queue, regions, MAX_QUEUE))
        return false;
    for (size_t i = 0; i < ne; ++i) {
        uint32_t id;
        if (!i32(file, &entries[i].key) || !u32(file, &id) || id >= nc)
            return false;
        entries[i].chunk = is_target[id] ? &target : &chunks[id];
    }
    for (size_t i = 0; i < ni; ++i) {
        uint32_t id;
        if (!u32(file, &id) || id >= nc)
            return false;
        insertions[i] = is_target[id] ? &target : &chunks[id];
    }
    for (size_t i = 0; i < nq; ++i) {
        uint32_t kind;
        mc_light_bounds *bounds = &regions[i].bounds;
        if (!u32(file, &kind) || kind > 1u || !i32(file, &bounds->min_x) ||
            !i32(file, &bounds->min_y) || !i32(file, &bounds->min_z) ||
            !i32(file, &bounds->max_x) || !i32(file, &bounds->max_y) || !i32(file, &bounds->max_z))
            return false;
        regions[i].kind = (mc_light_kind)kind;
    }
    cache->count = ne;
    cache->insertion_count = ni;
    queue->count = nq;
    *chunk_count = nc;
    return fgetc(file) == EOF && !ferror(file) && mc_chunk_cache_validate(cache);
}

static void print_bytes(const char *name, const uint8_t *value, size_t count) {
    printf("\"%s\":{\"length\":%zu,\"values\":\"", name, count);
    for (size_t i = 0; i < count; ++i)
        printf("%02x", (unsigned)value[i]);
    printf("\"}");
}

static void stage(const char *name, const mc_light_queue *queue, int32_t depth, bool with_trace) {
    printf("\"%s\":{\"data\":{", name);
    print_bytes("blocks", blocks, sizeof(blocks));
    printf(",");
    print_bytes("metadata", measured.metadata, sizeof(measured.metadata));
    printf(",");
    print_bytes("skylight", measured.skylight, sizeof(measured.skylight));
    printf(",");
    print_bytes("block_light", measured.block_light, sizeof(measured.block_light));
    printf(",");
    print_bytes("heightmap", measured.heightmap, sizeof(measured.heightmap));
    printf(",\"minimum_height\":%" PRId32 "},\"dirty\":%s,\"depth\":%" PRId32 ",\"queue\":[",
           measured.minimum_height, target.dirty ? "true" : "false", depth);
    for (size_t i = 0; i < queue->count; ++i) {
        const mc_light_region *r = &queue->entries[i];
        printf("%s[%d,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]",
               i ? "," : "", (int)r->kind, r->bounds.min_x, r->bounds.min_y, r->bounds.min_z,
               r->bounds.max_x, r->bounds.max_y, r->bounds.max_z);
    }
    printf("],\"trace\":[");
    if (with_trace) {
        for (size_t i = 0; i < MC_CHUNK_FINISH_QUERIES; ++i) {
            const mc_chunk_height_query *q = &queries[i];
            printf("%s[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]", i ? "," : "", q->x, q->z,
                   q->local_height, q->world_height);
        }
    }
    printf("]}");
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (file == NULL)
        return 2;
    mc_chunk_cache cache;
    mc_light_queue queue;
    int32_t depth = 0;
    bool no_sky;
    size_t chunk_count = 0;
    bool read = read_input(file, &cache, &queue, &depth, &no_sky, &chunk_count);
    if (fclose(file) != 0 || !read)
        return 2;
    memcpy(saved_entries, entries, cache.count * sizeof(*entries));
    memcpy(saved_insertions, insertions, cache.insertion_count * sizeof(*insertions));
    int32_t saved_depth = depth;
    mc_world_lighting world = {&cache, &queue, &depth, no_sky};
    mc_chunk_cache saved_cache = cache;
    mc_light_queue saved_queue = queue;
    mc_world_lighting saved_world = world;
    mc_chunk_state saved_target = target;
    uint8_t *saved_blocks = measured.blocks;
    size_t saved_blocks_capacity = measured.blocks_capacity;
    mc_chunk_finish_trace trace = {queries, MC_CHUNK_FINISH_QUERIES, 0};
    size_t aborts = SIZE_MAX;
    printf("{\"schema\":1,");
    stage("before", &queue, depth, false);
    if (!mc_chunk_finish_normal(&target, &world, opacity, 256, &trace, &aborts) ||
        trace.count != MC_CHUNK_FINISH_QUERIES || aborts != 0)
        return 1;
    printf(",");
    stage("after", &queue, depth, true);
    memcpy(blocks, repeated, sizeof(blocks));
    printf(",");
    stage("repeat_before", &queue, depth, false);
    if (!mc_chunk_finish_normal(&target, &world, opacity, 256, &trace, &aborts) ||
        trace.count != MC_CHUNK_FINISH_QUERIES || aborts != 0)
        return 1;
    printf(",");
    stage("repeat_after", &queue, depth, true);
    bool unchanged =
        depth == saved_depth && cache.entries == saved_cache.entries &&
        cache.capacity == saved_cache.capacity && cache.count == saved_cache.count &&
        cache.insertions == saved_cache.insertions &&
        cache.insertion_capacity == saved_cache.insertion_capacity &&
        cache.insertion_count == saved_cache.insertion_count &&
        queue.entries == saved_queue.entries && queue.capacity == saved_queue.capacity &&
        world.cache == saved_world.cache && world.queue == saved_world.queue &&
        world.enqueue_depth == saved_world.enqueue_depth && world.no_sky == saved_world.no_sky &&
        target.data == saved_target.data && target.x == saved_target.x &&
        target.z == saved_target.z && target.blank == saved_target.blank &&
        measured.blocks == saved_blocks && measured.blocks_capacity == saved_blocks_capacity &&
        memcmp(saved_entries, entries, saved_cache.count * sizeof(*entries)) == 0 &&
        memcmp(saved_insertions, insertions, saved_cache.insertion_count * sizeof(*insertions)) ==
            0;
    /* Non-target cached heights are immutable input to both passes. */
    for (size_t i = 0; i < chunk_count; ++i) {
        unchanged = unchanged && chunks[i].data == saved_chunks[i].data &&
                    chunks[i].x == saved_chunks[i].x && chunks[i].z == saved_chunks[i].z &&
                    chunks[i].blank == saved_chunks[i].blank &&
                    chunks[i].dirty == saved_chunks[i].dirty &&
                    memcmp(saved_heights[i], cached[i].heightmap, MC_CHUNK_HEIGHTMAP_BYTES) == 0;
    }
    printf(",\"context_retained\":%s}\n", unchanged ? "true" : "false");
    return unchanged && fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}
