#include "chunk_reference_fixtures.h"
#include "mc/world/chunk/data.h"

#include <stdio.h>
#include <string.h>

#define ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))

static unsigned int checks;
static unsigned int snapshots;
static unsigned int complete_arrays;
static unsigned int rebuilds;
static const char *case_name = "fixture";
static const char *stage_name = "fixture";

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s/%s: failed: %s\n", __FILE__, __LINE__, case_name,           \
                    stage_name, #condition);                                                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static mc_chunk_data data;
static uint8_t blocks[MC_CHUNK_BLOCKS];
static uint8_t expected[MC_CHUNK_BLOCKS];
static int32_t opacity[256];

_Static_assert(MC_CHUNK_BLOCKS == 32768u, "observed block count");
_Static_assert(MC_CHUNK_NIBBLE_BYTES == 16384u, "observed packed array size");
_Static_assert(MC_CHUNK_HEIGHTMAP_BYTES == 256u, "observed heightmap size");
_Static_assert(ARRAY_COUNT(chunk_reference_cases) == 5u, "five paired original cases");

/* The decoder expands captured bytes. It contains no height or light rules. */
static bool decode_buffer(size_t identifier, uint8_t *target, size_t length)
{
    if (identifier >= ARRAY_COUNT(chunk_reference_buffers)) {
        return false;
    }
    const chunk_reference_buffer *buffer = &chunk_reference_buffers[identifier];
    if (buffer->length != length || buffer->run_count == 0u ||
        buffer->first_run > ARRAY_COUNT(chunk_reference_runs) ||
        buffer->run_count > ARRAY_COUNT(chunk_reference_runs) - buffer->first_run) {
        return false;
    }
    size_t used = 0u;
    for (size_t i = 0u; i < buffer->run_count; ++i) {
        const chunk_reference_run *run = &chunk_reference_runs[buffer->first_run + i];
        if (run->count == 0u || run->count > length - used) {
            return false;
        }
        memset(target + used, run->value, run->count);
        used += run->count;
    }
    return used == length;
}

static int check_snapshot(const chunk_reference_snapshot *snapshot, size_t table)
{
    CHECK(data.blocks == blocks);
    CHECK(data.blocks_capacity == sizeof(blocks));
    CHECK(data.minimum_height == snapshot->minimum_height);
    CHECK(memcmp(opacity, chunk_reference_opacity[table], sizeof(opacity)) == 0);

    const uint8_t *actual[] = {blocks, data.metadata, data.skylight, data.block_light,
                               data.heightmap};
    const size_t lengths[] = {sizeof(blocks), sizeof(data.metadata), sizeof(data.skylight),
                              sizeof(data.block_light), sizeof(data.heightmap)};
    for (size_t i = 0u; i < ARRAY_COUNT(actual); ++i) {
        CHECK(decode_buffer(snapshot->buffers[i], expected, lengths[i]));
        CHECK(memcmp(actual[i], expected, lengths[i]) == 0);
        ++complete_arrays;
    }
    ++snapshots;
    return 0;
}

static int test_reference_case(const chunk_reference_case *fixture)
{
    case_name = fixture->name;
    stage_name = "before";
    CHECK(fixture->opacity < ARRAY_COUNT(chunk_reference_opacity));
    memcpy(opacity, chunk_reference_opacity[fixture->opacity], sizeof(opacity));
    CHECK(decode_buffer(fixture->stages[0].buffers[0], blocks, sizeof(blocks)));
    memset(&data, 0xa5, sizeof(data));
    CHECK(mc_chunk_data_init(&data, blocks, sizeof(blocks)));
    CHECK(check_snapshot(&fixture->stages[0], fixture->opacity) == 0);

    stage_name = "after";
    CHECK(mc_chunk_data_rebuild_height_skylight(&data, opacity, ARRAY_COUNT(opacity), true));
    ++rebuilds;
    CHECK(check_snapshot(&fixture->stages[1], fixture->opacity) == 0);

    /* Replay the original second input on the same retained chunk storage. */
    stage_name = "repeat_before";
    CHECK(decode_buffer(fixture->stages[2].buffers[0], blocks, sizeof(blocks)));
    CHECK(check_snapshot(&fixture->stages[2], fixture->opacity) == 0);

    stage_name = "repeat_after";
    CHECK(mc_chunk_data_rebuild_height_skylight(&data, opacity, ARRAY_COUNT(opacity), true));
    ++rebuilds;
    CHECK(check_snapshot(&fixture->stages[3], fixture->opacity) == 0);
    return 0;
}

int main(void)
{
    for (size_t i = 0u; i < ARRAY_COUNT(chunk_reference_cases); ++i) {
        CHECK(test_reference_case(&chunk_reference_cases[i]) == 0);
    }
    printf("chunk original fixture checks: %u (%zu cases, %u stages, %u arrays, %u rebuilds)\n",
           checks, ARRAY_COUNT(chunk_reference_cases), snapshots, complete_arrays, rebuilds);
    return 0;
}
