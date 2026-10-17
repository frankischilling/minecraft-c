#include "mc/world/chunk/data.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool read_exact(const char *path, uint8_t *bytes, size_t length)
{
    FILE *stream = fopen(path, "rb");
    if (stream == NULL)
        return false;
    size_t count = fread(bytes, 1, length, stream);
    bool valid = count == length && fgetc(stream) == EOF && !ferror(stream);
    if (fclose(stream) != 0)
        valid = false;
    return valid;
}

static void print_bytes(const char *name, const uint8_t *bytes, size_t length)
{
    printf("\"%s\":{\"length\":%zu,\"values\":\"", name, length);
    for (size_t index = 0; index < length; ++index)
        printf("%02x", (unsigned)bytes[index]);
    printf("\"}");
}

static void snapshot(const char *name, const mc_chunk_data *data)
{
    printf("\"%s\":{", name);
    print_bytes("blocks", data->blocks, MC_CHUNK_BLOCKS);
    printf(",");
    print_bytes("metadata", data->metadata, MC_CHUNK_NIBBLE_BYTES);
    printf(",");
    print_bytes("skylight", data->skylight, MC_CHUNK_NIBBLE_BYTES);
    printf(",");
    print_bytes("block_light", data->block_light, MC_CHUNK_NIBBLE_BYTES);
    printf(",");
    print_bytes("heightmap", data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    printf(",\"minimum_height\":%" PRId32 "}", data->minimum_height);
}

int main(int argc, char **argv)
{
    static uint8_t blocks[MC_CHUNK_BLOCKS];
    static uint8_t initial[MC_CHUNK_BLOCKS];
    static uint8_t repeated[MC_CHUNK_BLOCKS];
    static uint8_t opacity_bytes[256u * 4u];
    static int32_t opacity[256];
    static int32_t opacity_before[256];
    static mc_chunk_data data;
    static uint8_t metadata_before[MC_CHUNK_NIBBLE_BYTES];
    static uint8_t block_light_before[MC_CHUNK_NIBBLE_BYTES];
    if (argc != 4 || !read_exact(argv[1], initial, sizeof(initial)) ||
        !read_exact(argv[2], repeated, sizeof(repeated)) ||
        !read_exact(argv[3], opacity_bytes, sizeof(opacity_bytes))) {
        fprintf(stderr, "usage: data_probe initial32768.bin repeat32768.bin opacity1024.i32be\n");
        return 1;
    }
    for (size_t index = 0; index < 256u; ++index) {
        size_t offset = index * 4u;
        uint32_t value =
            (uint32_t)opacity_bytes[offset] << 24u | (uint32_t)opacity_bytes[offset + 1u] << 16u |
            (uint32_t)opacity_bytes[offset + 2u] << 8u | (uint32_t)opacity_bytes[offset + 3u];
        if (value > INT32_MAX) {
            fprintf(stderr, "negative input opacity\n");
            return 1;
        }
        opacity[index] = (int32_t)value;
    }
    memcpy(opacity_before, opacity, sizeof(opacity));
    memcpy(blocks, initial, sizeof(blocks));
    memset(&data, 0xa5, sizeof(data));
    if (!mc_chunk_data_init(&data, blocks, sizeof(blocks)))
        return 1;
    memcpy(metadata_before, data.metadata, sizeof(metadata_before));
    memcpy(block_light_before, data.block_light, sizeof(block_light_before));
    printf("{\"schema\":1,");
    snapshot("before", &data);
    if (!mc_chunk_data_rebuild_height_skylight(&data, opacity, 256u, true))
        return 1;
    bool initial_unchanged = memcmp(blocks, initial, sizeof(blocks)) == 0;
    printf(",");
    snapshot("after", &data);
    memcpy(blocks, repeated, sizeof(blocks));
    printf(",");
    snapshot("repeat_before", &data);
    if (!mc_chunk_data_rebuild_height_skylight(&data, opacity, 256u, true))
        return 1;
    printf(",");
    snapshot("repeat_after", &data);
    bool repeat_unchanged = memcmp(blocks, repeated, sizeof(blocks)) == 0;
    bool metadata_unchanged = memcmp(data.metadata, metadata_before, sizeof(metadata_before)) == 0;
    bool block_light_unchanged =
        memcmp(data.block_light, block_light_before, sizeof(block_light_before)) == 0;
    bool opacity_unchanged = memcmp(opacity, opacity_before, sizeof(opacity)) == 0;
    bool input_retained = data.blocks == blocks && data.blocks_capacity == sizeof(blocks);
    printf(",\"controls\":{\"initial_blocks_unchanged\":%s,\"repeat_blocks_unchanged\":%s,"
           "\"metadata_unchanged\":%s,\"block_light_unchanged\":%s,\"opacity_unchanged\":%s,"
           "\"input_retained\":%s}}\n",
           initial_unchanged ? "true" : "false", repeat_unchanged ? "true" : "false",
           metadata_unchanged ? "true" : "false", block_light_unchanged ? "true" : "false",
           opacity_unchanged ? "true" : "false", input_retained ? "true" : "false");
    bool controls = initial_unchanged && repeat_unchanged && metadata_unchanged &&
                    block_light_unchanged && opacity_unchanged && input_retained;
    return controls && fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}
