#include "mc/world/chunk/finish.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Constructed contract cases; these do not establish original-game parity. */
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_chunk_data data[2];
    uint8_t blocks[2][MC_CHUNK_BLOCKS + 3u];
    mc_chunk_state chunks[3];
    mc_chunk_cache_entry entries[8];
    mc_chunk_state *insertions[16];
    mc_chunk_cache cache;
    mc_light_region regions[2048];
    mc_light_queue queue;
    int32_t depth;
    mc_world_lighting world;
    mc_chunk_height_query queries[MC_CHUNK_FINISH_QUERIES + 2u];
    mc_chunk_finish_trace trace;
    int32_t opacity[257];
    size_t aborts;
} fixture;

static fixture f, saved;

static int reset_fixture(void)
{
    memset(&f, 0, sizeof(f));
    for (size_t i = 0; i < 2u; ++i) {
        memset(f.blocks[i] + MC_CHUNK_BLOCKS, 0xa7, 3u);
        CHECK(mc_chunk_data_init(&f.data[i], f.blocks[i], sizeof(f.blocks[i])));
        f.chunks[i] = (mc_chunk_state){.data = &f.data[i]};
    }
    f.chunks[2] = (mc_chunk_state){.x = 99, .z = -99, .blank = true};
    CHECK(mc_chunk_cache_init(&f.cache, f.entries, 8u, f.insertions, 16u));
    CHECK(mc_light_queue_init(&f.queue, f.regions, 2048u));
    f.world = (mc_world_lighting){&f.cache, &f.queue, &f.depth, false};
    memset(f.queries, 0xa5, sizeof(f.queries));
    f.trace = (mc_chunk_finish_trace){f.queries, MC_CHUNK_FINISH_QUERIES + 2u, 17u};
    f.opacity[1] = 255;
    f.opacity[256] = -91; /* Ignored suffix. */
    f.aborts = 123u;
    return 0;
}

static int test_cache(void)
{
    CHECK(reset_fixture() == 0);
    CHECK(mc_chunk_cache_validate(&f.cache));
    CHECK(mc_chunk_cache_key(0, 0) == 0);
    CHECK(mc_chunk_cache_key(-1, 0) == -65536);
    CHECK(mc_chunk_cache_key(0, -1) == 65535);
    CHECK(mc_chunk_cache_key(INT_MAX, INT_MAX) == 2147450879);
    CHECK(mc_chunk_cache_key(INT_MIN, INT_MIN) == -2147450880);
    CHECK(mc_chunk_cache_key(32768, 0) == 0);
    CHECK(mc_chunk_cache_key(-32769, -32769) == mc_chunk_cache_key(-1, -1));
    memcpy(&saved, &f, sizeof(f));
    CHECK(!mc_chunk_cache_init(&f.cache, NULL, 1u, f.insertions, 16u));
    CHECK(memcmp(&f, &saved, sizeof(f)) == 0);
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
    CHECK(mc_chunk_cache_publish(&f.cache, 32768, 0, &f.chunks[1]));
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[1]));
    CHECK(f.cache.count == 1u && f.cache.insertion_count == 3u);
    CHECK(f.entries[0].chunk == &f.chunks[1]);
    CHECK(f.insertions[0] == &f.chunks[0] && f.insertions[1] == &f.chunks[1] &&
          f.insertions[2] == &f.chunks[1]);
    mc_chunk_state *found = &f.chunks[2];
    CHECK(mc_chunk_cache_lookup(&f.cache, 65536, 0, &found) && found == &f.chunks[1]);
    CHECK(!mc_chunk_cache_lookup(&f.cache, 1, 1, &found) && found == &f.chunks[1]);
    CHECK(mc_chunk_cache_contains(&f.cache, 32768, 0));
    f.cache.capacity = 1u;
    memcpy(&saved, &f, sizeof(f));
    CHECK(!mc_chunk_cache_publish(&f.cache, 1, 0, &f.chunks[0]));
    CHECK(memcmp(&f, &saved, sizeof(f)) == 0);
    f.cache.insertion_capacity = 3u;
    memcpy(&saved, &f, sizeof(f));
    CHECK(!mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
    CHECK(memcmp(&f, &saved, sizeof(f)) == 0);
    f.cache.capacity = 8u;
    f.entries[1] = f.entries[0];
    f.cache.count = 2u;
    CHECK(!mc_chunk_cache_validate(&f.cache));
    f.cache.count = 1u;
    f.entries[0].chunk = NULL;
    CHECK(!mc_chunk_cache_validate(&f.cache));
    f.entries[0].chunk = &f.chunks[2];
    CHECK(mc_chunk_cache_validate(&f.cache));
    return 0;
}

static int test_world_height(void)
{
    CHECK(reset_fixture() == 0);
    f.data[0].heightmap[255] = 254u;
    f.data[0].heightmap[0] = 231u;
    CHECK(mc_chunk_cache_publish(&f.cache, -1, -1, &f.chunks[0]));
    CHECK(mc_chunk_cache_publish(&f.cache, -2000000, 0, &f.chunks[0]));
    CHECK(mc_chunk_cache_publish(&f.cache, 2000000, 0, &f.chunks[0]));
    CHECK(mc_chunk_cache_publish(&f.cache, 0, -2000000, &f.chunks[0]));
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 2000000, &f.chunks[0]));
    CHECK(mc_chunk_cache_publish(&f.cache, 2, 2, &f.chunks[2]));
    static const int32_t cases[][3] = {{-1, -1, 254},     {-32000000, 0, 231}, {0, -32000000, 231},
                                       {32000000, 0, 0},  {0, 32000000, 231},  {-32000001, 0, 0},
                                       {0, -32000001, 0}, {0, 32000001, 0},    {32, 32, 0},
                                       {800, 800, 0}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        int32_t height = -77;
        CHECK(mc_world_height(&f.cache, cases[i][0], cases[i][1], &height));
        CHECK(height == cases[i][2]);
    }
    f.entries[0].chunk = NULL;
    int32_t height = 73;
    CHECK(!mc_world_height(&f.cache, -1, -1, &height) && height == 73);
    CHECK(!mc_world_height(NULL, 0, 0, &height) && height == 73);
    CHECK(!mc_world_height(&f.cache, 0, 0, NULL));
    return 0;
}

static int test_admission(void)
{
    CHECK(reset_fixture() == 0);
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
    mc_light_region region = {MC_LIGHT_SKY, {-1, -200, 0, 0, 300, 0}};
    /* (-1+0)/2 truncates to zero, selecting chunk zero rather than minus one. */
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(f.queue.count == 1u && f.regions[0].bounds.min_y == -200);
    f.world.no_sky = true;
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_SKIPPED);
    region.kind = MC_LIGHT_BLOCK;
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_APPENDED);
    f.depth = 49;
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_SKIPPED);
    CHECK(f.depth == 49 && f.queue.count == 2u);
    mc_world_lighting other = f.world; /* A second world borrows the same depth owner. */
    f.depth = 50;
    CHECK(mc_world_light_submit(&other, &region, false) == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(f.depth == 50);
    f.depth = INT_MAX;
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(f.depth == INT_MAX);
    CHECK(reset_fixture() == 0);
    CHECK(mc_chunk_cache_publish(&f.cache, -1, 0, &f.chunks[0]));
    region = (mc_light_region){MC_LIGHT_SKY, {INT_MAX, 0, 0, INT_MAX, 1, 0}};
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(f.depth == 0); /* Wrapped midpoint is -1, not INT_MAX. */
    CHECK(reset_fixture() == 0);
    CHECK(mc_chunk_cache_publish(&f.cache, 2000000, 0, &f.chunks[0]));
    region = (mc_light_region){MC_LIGHT_SKY, {32000000, 0, 0, 32000000, 1, 0}};
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(mc_chunk_cache_publish(&f.cache, 2000000, 0, &f.chunks[2]));
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_SKIPPED);
    region.bounds.min_x = region.bounds.max_x = 1600;
    CHECK(mc_world_light_submit(&f.world, &region, false) == MC_LIGHT_SUBMIT_SKIPPED);
    return 0;
}

static int test_finish_preflight(void)
{
    for (unsigned int failure = 0; failure < 5u; ++failure) {
        CHECK(reset_fixture() == 0);
        memset(f.data[0].skylight, 0x37, sizeof(f.data[0].skylight));
        if (failure == 0u)
            f.trace.capacity = MC_CHUNK_FINISH_QUERIES - 1u;
        else if (failure == 1u)
            f.opacity[255] = -1;
        else if (failure == 2u)
            f.queue.capacity = MC_CHUNK_FINISH_QUERIES - 1u;
        else if (failure == 3u)
            f.chunks[0].blank = true;
        else
            f.data[0].blocks_capacity = MC_CHUNK_BLOCKS - 1u;
        memcpy(&saved, &f, sizeof(f));
        CHECK(
            !mc_chunk_finish_normal(&f.chunks[0], &f.world, f.opacity, 257u, &f.trace, &f.aborts));
        CHECK(memcmp(&f, &saved, sizeof(f)) == 0);
    }
    return 0;
}

static int test_finish_order_and_preservation(void)
{
    CHECK(reset_fixture() == 0);
    f.chunks[0].x = f.chunks[0].z = -1;
    memset(f.data[0].metadata, 0x35, sizeof(f.data[0].metadata));
    memset(f.data[0].block_light, 0x57, sizeof(f.data[0].block_light));
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_finish_normal(&f.chunks[0], &f.world, f.opacity, 257u, &f.trace, &f.aborts));
    CHECK(f.trace.count == 1024u && f.chunks[0].dirty && f.aborts == 0u);
    CHECK(f.queue.count == 0u && f.depth == 0);
    for (size_t i = 0; i < 1024u; ++i) {
        static const int32_t offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        int32_t column_x = (int32_t)(i / 64u) - 16;
        int32_t column_z = (int32_t)((i / 4u) % 16u) - 16;
        CHECK(f.queries[i].x == column_x + offsets[i % 4u][0]);
        CHECK(f.queries[i].z == column_z + offsets[i % 4u][1]);
        CHECK(f.queries[i].local_height == 0 && f.queries[i].world_height == 0);
    }
    CHECK(memcmp(f.queries + 1024u, saved.queries + 1024u, 2u * sizeof(f.queries[0])) == 0);
    CHECK(memcmp(f.data[0].metadata, saved.data[0].metadata, sizeof(f.data[0].metadata)) == 0);
    CHECK(memcmp(f.data[0].block_light, saved.data[0].block_light, sizeof(f.data[0].block_light)) ==
          0);
    CHECK(memcmp(f.blocks, saved.blocks, sizeof(f.blocks)) == 0);
    CHECK(memcmp(&f.cache, &saved.cache, sizeof(f.cache)) == 0);
    CHECK(memcmp(f.entries, saved.entries, sizeof(f.entries)) == 0);
    CHECK(memcmp(f.insertions, saved.insertions, sizeof(f.insertions)) == 0);
    return 0;
}

static int test_finish_visibility_and_queue(void)
{
    for (unsigned int published = 0; published < 2u; ++published) {
        CHECK(reset_fixture() == 0);
        for (size_t column = 0; column < 256u; ++column)
            f.blocks[0][column * 128u] = 1u;
        f.world.no_sky = true;
        memset(f.data[0].skylight, 0x93, sizeof(f.data[0].skylight));
        if (published != 0u)
            CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
        memcpy(&saved, &f, sizeof(f));
        CHECK(mc_chunk_finish_normal(&f.chunks[0], &f.world, f.opacity, 257u, &f.trace, &f.aborts));
        CHECK(f.chunks[0].dirty && f.data[0].minimum_height == 1);
        CHECK(f.queries[0].world_height == 0 && f.queries[0].local_height == 1);
        CHECK(f.queries[1].world_height == (published != 0u ? 1 : 0));
        CHECK(f.queries[68].world_height == (published != 0u ? 1 : 0));
        CHECK(f.queue.count == 0u);
        CHECK(memcmp(f.data[0].skylight, saved.data[0].skylight, sizeof(f.data[0].skylight)) == 0);
    }
    CHECK(reset_fixture() == 0);
    f.blocks[0][0] = 1u; /* Only column (0,0) differs from the cached old object. */
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[1]));
    f.regions[0] = (mc_light_region){MC_LIGHT_BLOCK, {-9, 7, -9, -9, 8, -9}};
    f.queue.count = 1u;
    mc_light_region baseline = f.regions[0];
    CHECK(mc_chunk_finish_normal(&f.chunks[0], &f.world, f.opacity, 257u, &f.trace, &f.aborts));
    CHECK(f.queue.count == 2u && memcmp(&baseline, &f.regions[0], sizeof(baseline)) == 0);
    CHECK(f.regions[1].kind == MC_LIGHT_SKY);
    CHECK(f.regions[1].bounds.min_x == 0 && f.regions[1].bounds.max_x == 1);
    CHECK(f.regions[1].bounds.min_y == 0 && f.regions[1].bounds.max_y == 1);
    CHECK(f.regions[1].bounds.min_z == 0 && f.regions[1].bounds.max_z == 1);
    CHECK(!f.chunks[1].dirty && f.data[1].heightmap[0] == 0);
    return 0;
}

int main(void)
{
    if (test_cache() || test_world_height() || test_admission() || test_finish_preflight() ||
        test_finish_order_and_preservation() || test_finish_visibility_and_queue()) {
        return 1;
    }
    printf("chunk finishing contract: %u checks passed\n", checks);
    return 0;
}
