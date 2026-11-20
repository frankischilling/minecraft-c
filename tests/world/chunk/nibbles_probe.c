#include "mc/world/chunk/data.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WRITE_COUNT = 16 };

static const int32_t requests[WRITE_COUNT][4] = {
    {0, 0, 0, 0},     {0, 1, 0, 15},     {15, 126, 15, 16},      {15, 127, 15, 31},
    {0, 126, 15, -1}, {0, 127, 15, -16}, {15, 0, 0, INT32_MIN},  {15, 1, 0, INT32_MAX},
    {3, 0, 11, 15},   {3, 1, 11, 0},     {11, 126, 3, 31},       {11, 127, 3, 16},
    {1, 64, 14, -16}, {1, 65, 14, -1},   {14, 62, 1, INT32_MAX}, {14, 63, 1, INT32_MIN},
};

typedef struct observation {
    uint8_t fresh[MC_CHUNK_NIBBLE_BYTES];
    uint8_t prepared[MC_CHUNK_NIBBLE_BYTES];
    uint8_t initial_getters[MC_CHUNK_BLOCKS];
    uint8_t writes[WRITE_COUNT][MC_CHUNK_NIBBLE_BYTES];
    uint8_t observed[WRITE_COUNT];
    uint8_t final_packed[MC_CHUNK_NIBBLE_BYTES];
    uint8_t final_getters[MC_CHUNK_BLOCKS];
} observation;

static bool read_all(const uint8_t *packed, uint8_t *values) {
    size_t cursor = 0;
    for (int32_t x = 0; x < 16; ++x) {
        for (int32_t z = 0; z < 16; ++z) {
            for (int32_t y = 0; y < 128; ++y) {
                if (!mc_chunk_nibbles_get(packed, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                          &values[cursor]) ||
                    values[cursor] > 15)
                    return false;
                ++cursor;
            }
        }
    }
    return cursor == MC_CHUNK_BLOCKS;
}

static bool observe(mc_chunk_data *data, uint8_t *blocks, observation *result) {
    if (!mc_chunk_data_init(data, blocks, MC_CHUNK_BLOCKS))
        return false;
    uint8_t *packed = data->metadata;
    memcpy(result->fresh, packed, MC_CHUNK_NIBBLE_BYTES);
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i) {
        if (result->fresh[i] != 0)
            return false;
        /* Prepare the same bytes as the original observer; do not calculate access results. */
        packed[i] = (uint8_t)(i * 73u + (i >> 3u) * 19u + (i >> 8u) * 151u + 0xa7u);
    }
    memcpy(result->prepared, packed, MC_CHUNK_NIBBLE_BYTES);
    if (!read_all(packed, result->initial_getters) ||
        memcmp(packed, result->prepared, MC_CHUNK_NIBBLE_BYTES) != 0)
        return false;
    for (size_t i = 0; i < WRITE_COUNT; ++i) {
        const int32_t *request = requests[i];
        if (!mc_chunk_nibbles_set(packed, MC_CHUNK_NIBBLE_BYTES, request[0], request[1], request[2],
                                  request[3]))
            return false;
        memcpy(result->writes[i], packed, MC_CHUNK_NIBBLE_BYTES);
        if (!mc_chunk_nibbles_get(packed, MC_CHUNK_NIBBLE_BYTES, request[0], request[1], request[2],
                                  &result->observed[i]) ||
            result->observed[i] > 15 ||
            memcmp(packed, result->writes[i], MC_CHUNK_NIBBLE_BYTES) != 0)
            return false;
    }
    memcpy(result->final_packed, packed, MC_CHUNK_NIBBLE_BYTES);
    return read_all(packed, result->final_getters) &&
           memcmp(packed, result->final_packed, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           data->metadata == packed && data->blocks == blocks &&
           data->blocks_capacity == MC_CHUNK_BLOCKS;
}

static void print_hex(const uint8_t *bytes, size_t count) {
    static const char digits[] = "0123456789abcdef";
    putchar('"');
    for (size_t i = 0; i < count; ++i) {
        putchar(digits[bytes[i] >> 4u]);
        putchar(digits[bytes[i] & 15u]);
    }
    putchar('"');
}

static void print_record(const observation *result) {
    printf("{\"schema\":1,\"side\":\"c\",\"constructor_cells\":%u,\"packed_length\":%u,"
           "\"fresh_packed\":",
           MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES);
    print_hex(result->fresh, MC_CHUNK_NIBBLE_BYTES);
    printf(",\"prepared_packed\":");
    print_hex(result->prepared, MC_CHUNK_NIBBLE_BYTES);
    printf(",\"getter_order\":\"X/Z/Y\",\"initial_getters\":");
    print_hex(result->initial_getters, MC_CHUNK_BLOCKS);
    printf(",\"writes\":[");
    for (size_t i = 0; i < WRITE_COUNT; ++i) {
        const int32_t *request = requests[i];
        printf("%s{\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"z\":%" PRId32 ",\"value\":%" PRId32
               ",\"observed\":%u,\"packed\":",
               i == 0 ? "" : ",", request[0], request[1], request[2], request[3],
               (unsigned)result->observed[i]);
        print_hex(result->writes[i], MC_CHUNK_NIBBLE_BYTES);
        putchar('}');
    }
    printf("],\"final_packed\":");
    print_hex(result->final_packed, MC_CHUNK_NIBBLE_BYTES);
    printf(",\"final_getters\":");
    print_hex(result->final_getters, MC_CHUNK_BLOCKS);
    printf(",\"controls\":{\"fresh_zero_checked\":true,\"array_identity_unchanged\":true,"
           "\"initial_getters_readonly\":true,\"written_getters_readonly\":true,"
           "\"final_getters_readonly\":true}}\n");
}

int main(void) {
    _Static_assert(MC_CHUNK_BLOCKS == 32768u && MC_CHUNK_NIBBLE_BYTES == 16384u,
                   "The reference scenario requires the normal original chunk shape.");
    mc_chunk_data *data = malloc(sizeof(*data));
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    observation *result = malloc(sizeof(*result));
    int status = EXIT_FAILURE;
    if (data == NULL || blocks == NULL || result == NULL) {
        fputs("nibbles observation allocation failed\n", stderr);
    } else if (!observe(data, blocks, result)) {
        fputs("nibbles observation control failed\n", stderr);
    } else {
        print_record(result);
        if (fflush(stdout) == 0 && !ferror(stdout))
            status = EXIT_SUCCESS;
    }
    free(result);
    free(blocks);
    free(data);
    return status;
}
