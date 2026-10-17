#include "mc/world/chunk/data.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static mc_chunk_data data;
static mc_chunk_data saved_data;
static uint8_t blocks[MC_CHUNK_BLOCKS + 3u];
static uint8_t saved_blocks[sizeof(blocks)];
static uint8_t packed[MC_CHUNK_NIBBLE_BYTES + 3u];
static uint8_t saved_packed[sizeof(packed)];
static int32_t opacity[257];
static int32_t saved_opacity[257];

_Static_assert(MC_CHUNK_WIDTH == 16u, "normal chunk width");
_Static_assert(MC_CHUNK_HEIGHT == 128u, "normal chunk height");
_Static_assert(MC_CHUNK_BLOCKS == 32768u, "normal block count");
_Static_assert(MC_CHUNK_NIBBLE_BYTES == 16384u, "normal packed array size");
_Static_assert(MC_CHUNK_HEIGHTMAP_BYTES == 256u, "normal heightmap size");

static int test_initialization(void)
{
    memset(&data, 0xa5, sizeof(data));
    for (size_t i = 0; i < sizeof(blocks); ++i) {
        blocks[i] = (uint8_t)(i * 37u + 19u);
    }
    memcpy(saved_blocks, blocks, sizeof(blocks));

    CHECK(mc_chunk_data_init(&data, blocks, sizeof(blocks)));
    CHECK(data.blocks == blocks);
    CHECK(data.blocks_capacity == sizeof(blocks));
    CHECK(memcmp(blocks, saved_blocks, sizeof(blocks)) == 0);
    CHECK(data.minimum_height == 0);
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i) {
        CHECK(data.metadata[i] == 0);
        CHECK(data.skylight[i] == 0);
        CHECK(data.block_light[i] == 0);
    }
    for (size_t i = 0; i < MC_CHUNK_HEIGHTMAP_BYTES; ++i) {
        CHECK(data.heightmap[i] == 0);
    }

    /* Reinitializing caller-owned data clears retained light/metadata while
     * preserving its borrowed block storage and capacity suffix. */
    memset(data.metadata, 0x31, sizeof(data.metadata));
    memset(data.skylight, 0x72, sizeof(data.skylight));
    memset(data.block_light, 0xb4, sizeof(data.block_light));
    memset(data.heightmap, 0x65, sizeof(data.heightmap));
    data.minimum_height = 73;
    CHECK(mc_chunk_data_init(&data, blocks, sizeof(blocks)));
    CHECK(data.blocks == blocks);
    CHECK(data.blocks_capacity == sizeof(blocks));
    CHECK(memcmp(blocks, saved_blocks, sizeof(blocks)) == 0);
    CHECK(data.minimum_height == 0);
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i) {
        CHECK(data.metadata[i] == 0);
        CHECK(data.skylight[i] == 0);
        CHECK(data.block_light[i] == 0);
    }
    for (size_t i = 0; i < MC_CHUNK_HEIGHTMAP_BYTES; ++i) {
        CHECK(data.heightmap[i] == 0);
    }
    return 0;
}

static void save_state(void)
{
    memcpy(&saved_data, &data, sizeof(data));
    memcpy(saved_blocks, blocks, sizeof(blocks));
    memcpy(saved_opacity, opacity, sizeof(opacity));
}

static int check_state_unchanged(void)
{
    CHECK(memcmp(&data, &saved_data, sizeof(data)) == 0);
    CHECK(memcmp(blocks, saved_blocks, sizeof(blocks)) == 0);
    CHECK(memcmp(opacity, saved_opacity, sizeof(opacity)) == 0);
    return 0;
}

static int test_initialization_failures(void)
{
    memset(&data, 0xa5, sizeof(data));
    memset(blocks, 0x39, sizeof(blocks));
    save_state();
    CHECK(!mc_chunk_data_init(NULL, blocks, sizeof(blocks)));
    CHECK(check_state_unchanged() == 0);
    CHECK(!mc_chunk_data_init(&data, NULL, sizeof(blocks)));
    CHECK(check_state_unchanged() == 0);
    const size_t short_capacities[] = {0u, 1u, MC_CHUNK_BLOCKS - 1u};
    for (size_t i = 0; i < sizeof(short_capacities) / sizeof(short_capacities[0]); ++i) {
        CHECK(!mc_chunk_data_init(&data, blocks, short_capacities[i]));
        CHECK(check_state_unchanged() == 0);
    }
    return 0;
}

static int test_rebuild_failures(void)
{
    CHECK(mc_chunk_data_init(&data, blocks, sizeof(blocks)));
    memset(data.metadata, 0x27, sizeof(data.metadata));
    memset(data.skylight, 0xb6, sizeof(data.skylight));
    memset(data.block_light, 0xd1, sizeof(data.block_light));
    memset(data.heightmap, 0x53, sizeof(data.heightmap));
    data.minimum_height = 79;
    memset(opacity, 0, sizeof(opacity));
    save_state();
    CHECK(!mc_chunk_data_rebuild_height_skylight(NULL, opacity, 256u, true));
    CHECK(check_state_unchanged() == 0);
    CHECK(!mc_chunk_data_rebuild_height_skylight(&data, NULL, 256u, true));
    CHECK(check_state_unchanged() == 0);
    const size_t short_lengths[] = {0u, 1u, 255u};
    for (size_t i = 0; i < sizeof(short_lengths) / sizeof(short_lengths[0]); ++i) {
        CHECK(!mc_chunk_data_rebuild_height_skylight(&data, opacity, short_lengths[i], true));
        CHECK(check_state_unchanged() == 0);
    }

    /* A late invalid entry must also reject before changing any output,
     * including when skylight computation is disabled. */
    for (size_t id = 0; id < 256u; ++id) {
        opacity[id] = (id % 2u == 0) ? -1 : INT32_MIN;
        save_state();
        CHECK(!mc_chunk_data_rebuild_height_skylight(&data, opacity, 256u, true));
        CHECK(check_state_unchanged() == 0);
        CHECK(!mc_chunk_data_rebuild_height_skylight(&data, opacity, 256u, false));
        CHECK(check_state_unchanged() == 0);
        opacity[id] = 0;
    }

    data.blocks = NULL;
    save_state();
    CHECK(!mc_chunk_data_rebuild_height_skylight(&data, opacity, 256u, true));
    CHECK(check_state_unchanged() == 0);
    data.blocks = blocks;
    data.blocks_capacity = MC_CHUNK_BLOCKS - 1u;
    save_state();
    CHECK(!mc_chunk_data_rebuild_height_skylight(&data, opacity, 256u, true));
    CHECK(check_state_unchanged() == 0);
    return 0;
}

static int test_opacity_and_preservation(void)
{
    memset(blocks, 0, sizeof(blocks));
    blocks[MC_CHUNK_BLOCKS] = 0x7d;
    blocks[MC_CHUNK_BLOCKS + 1u] = 0x94;
    blocks[MC_CHUNK_BLOCKS + 2u] = 0xc2;
    CHECK(mc_chunk_data_init(&data, blocks, sizeof(blocks)));
    memset(data.metadata, 0x27, sizeof(data.metadata));
    memset(data.skylight, 0xb6, sizeof(data.skylight));
    memset(data.block_light, 0xd1, sizeof(data.block_light));
    for (size_t id = 0; id < 256u; ++id) {
        opacity[id] = INT32_MAX;
    }
    /* The extra entry is outside the admitted table prefix. */
    opacity[256] = INT32_MIN;
    save_state();
    CHECK(mc_chunk_data_rebuild_height_skylight(&data, opacity, 257u, true));
    CHECK(data.blocks == blocks);
    CHECK(data.blocks_capacity == sizeof(blocks));
    CHECK(memcmp(blocks, saved_blocks, sizeof(blocks)) == 0);
    CHECK(memcmp(opacity, saved_opacity, sizeof(opacity)) == 0);
    CHECK(memcmp(data.metadata, saved_data.metadata, sizeof(data.metadata)) == 0);
    CHECK(memcmp(data.block_light, saved_data.block_light, sizeof(data.block_light)) == 0);
    /* This C opacity-limit check is separate from vanilla reference fixtures.
     * An opacity above the initial light must preserve every sky nibble. */
    CHECK(memcmp(data.skylight, saved_data.skylight, sizeof(data.skylight)) == 0);

    memset(opacity, 0, 256u * sizeof(opacity[0]));
    save_state();
    CHECK(mc_chunk_data_rebuild_height_skylight(&data, opacity, 257u, false));
    CHECK(data.blocks == blocks);
    CHECK(data.blocks_capacity == sizeof(blocks));
    CHECK(memcmp(blocks, saved_blocks, sizeof(blocks)) == 0);
    CHECK(memcmp(opacity, saved_opacity, sizeof(opacity)) == 0);
    CHECK(memcmp(data.metadata, saved_data.metadata, sizeof(data.metadata)) == 0);
    CHECK(memcmp(data.block_light, saved_data.block_light, sizeof(data.block_light)) == 0);
    CHECK(memcmp(data.skylight, saved_data.skylight, sizeof(data.skylight)) == 0);
    save_state();
    CHECK(mc_chunk_data_rebuild_height_skylight(&data, opacity, 257u, false));
    CHECK(check_state_unchanged() == 0);
    return 0;
}

static int test_nibble_failures(void)
{
    memset(packed, 0xa7, sizeof(packed));
    memcpy(saved_packed, packed, sizeof(packed));
    uint8_t out = 0xc3;
    CHECK(!mc_chunk_nibbles_get(NULL, sizeof(packed), 0, 0, 0, &out));
    CHECK(!mc_chunk_nibbles_get(packed, sizeof(packed), 0, 0, 0, NULL));
    CHECK(!mc_chunk_nibbles_set(NULL, sizeof(packed), 0, 0, 0, INT32_MIN));
    CHECK(out == 0xc3);
    CHECK(memcmp(packed, saved_packed, sizeof(packed)) == 0);

    const size_t short_capacities[] = {0u, 1u, MC_CHUNK_NIBBLE_BYTES - 1u};
    for (size_t i = 0; i < sizeof(short_capacities) / sizeof(short_capacities[0]); ++i) {
        CHECK(!mc_chunk_nibbles_get(packed, short_capacities[i], 0, 0, 0, &out));
        CHECK(!mc_chunk_nibbles_set(packed, short_capacities[i], 0, 0, 0, 15));
        CHECK(out == 0xc3);
        CHECK(memcmp(packed, saved_packed, sizeof(packed)) == 0);
    }

    const int32_t bad_coordinates[][3] = {{-1, 0, 0},        {16, 0, 0},        {INT32_MIN, 0, 0},
                                          {INT32_MAX, 0, 0}, {0, -1, 0},        {0, 128, 0},
                                          {0, INT32_MIN, 0}, {0, INT32_MAX, 0}, {0, 0, -1},
                                          {0, 0, 16},        {0, 0, INT32_MIN}, {0, 0, INT32_MAX}};
    for (size_t i = 0; i < sizeof(bad_coordinates) / sizeof(bad_coordinates[0]); ++i) {
        const int32_t *position = bad_coordinates[i];
        CHECK(!mc_chunk_nibbles_get(packed, sizeof(packed), position[0], position[1], position[2],
                                    &out));
        CHECK(!mc_chunk_nibbles_set(packed, sizeof(packed), position[0], position[1], position[2],
                                    INT32_MAX));
        CHECK(out == 0xc3);
        CHECK(memcmp(packed, saved_packed, sizeof(packed)) == 0);
    }
    return 0;
}

int main(void)
{
    if (test_initialization() || test_initialization_failures() || test_rebuild_failures() ||
        test_opacity_and_preservation() || test_nibble_failures()) {
        return 1;
    }
    printf("chunk data contract checks: %u\n", checks);
    return 0;
}
