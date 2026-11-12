#include "mc/storage/region/writer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define PROBE_INPUT_LIMIT (32u * 1024u * 1024u)
#define PROBE_OUTPUT_LIMIT (128u * 1024u * 1024u)
#define PROBE_WRITES 32u

typedef struct probe_reader {
    const uint8_t *bytes;
    size_t length, position;
} probe_reader;

typedef struct probe_write {
    int32_t x, z;
    uint32_t timestamp;
    const uint8_t *payload;
    size_t length;
} probe_write;

static bool read_span(probe_reader *reader, size_t length, const uint8_t **out) {
    if (length > reader->length - reader->position)
        return false;
    *out = reader->bytes + reader->position;
    reader->position += length;
    return true;
}

static bool read_u32(probe_reader *reader, uint32_t *out) {
    const uint8_t *bytes;
    if (!read_span(reader, 4, &bytes))
        return false;
    *out =
        (uint32_t)bytes[0] << 24u | (uint32_t)bytes[1] << 16u | (uint32_t)bytes[2] << 8u | bytes[3];
    return true;
}

static int32_t signed_bits(uint32_t bits) {
    return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}

static bool write_u32(uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)(value >> 24u), (uint8_t)(value >> 16u), (uint8_t)(value >> 8u),
                        (uint8_t)value};
    return fwrite(bytes, 1, sizeof(bytes), stdout) == sizeof(bytes);
}

static bool write_state(mc_region_writer *writer, mc_region_writer_status status, size_t *total) {
    mc_region_writer_bytes bytes = mc_region_writer_get_bytes(writer);
    if (bytes.length > PROBE_OUTPUT_LIMIT - 12u || *total > PROBE_OUTPUT_LIMIT - 12u - bytes.length)
        return false;
    *total += 12u + bytes.length;
    return write_u32((uint32_t)status) && write_u32(mc_region_writer_consume_growth(writer)) &&
           write_u32((uint32_t)bytes.length) &&
           (bytes.length == 0 || fwrite(bytes.data, 1, bytes.length, stdout) == bytes.length);
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1)
        return 2;
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return 2;
#endif
    uint8_t *input = malloc(PROBE_INPUT_LIMIT + 1u);
    if (input == NULL)
        return 2;
    size_t length = fread(input, 1, PROBE_INPUT_LIMIT + 1u, stdin);
    if (ferror(stdin) || length > PROBE_INPUT_LIMIT) {
        free(input);
        return 2;
    }
    probe_reader reader = {input, length, 0};
    const uint8_t *magic, *mode, *initial;
    uint32_t initial_length, count;
    if (!read_span(&reader, 4, &magic) || memcmp(magic, "MRW1", 4) != 0 ||
        !read_span(&reader, 1, &mode) || *mode > 1 || !read_u32(&reader, &initial_length) ||
        (*mode == 0 && initial_length != 0) || !read_span(&reader, initial_length, &initial) ||
        !read_u32(&reader, &count) || count > PROBE_WRITES) {
        free(input);
        return 2;
    }
    probe_write writes[PROBE_WRITES];
    for (size_t i = 0; i < count; ++i) {
        uint32_t x, z, payload_length;
        if (!read_u32(&reader, &x) || !read_u32(&reader, &z) ||
            !read_u32(&reader, &writes[i].timestamp) || !read_u32(&reader, &payload_length) ||
            !read_span(&reader, payload_length, &writes[i].payload)) {
            free(input);
            return 2;
        }
        writes[i].x = signed_bits(x);
        writes[i].z = signed_bits(z);
        writes[i].length = payload_length;
    }
    if (reader.position != reader.length) {
        free(input);
        return 2;
    }
    mc_region_writer *writer = NULL;
    mc_region_writer_status status =
        *mode == 0 ? mc_region_writer_create(NULL, &writer)
                   : mc_region_writer_import(initial, initial_length, NULL, &writer);
    size_t total = 8;
    bool success = fwrite("MRS1", 1, 4, stdout) == 4 &&
                   write_u32(status == MC_REGION_WRITER_OK ? count + 1u : 1u) &&
                   write_state(writer, status, &total);
    if (status == MC_REGION_WRITER_OK) {
        for (size_t i = 0; i < count && success; ++i) {
            status =
                mc_region_writer_write_zlib(writer, writes[i].x, writes[i].z, writes[i].payload,
                                            writes[i].length, writes[i].timestamp);
            success = write_state(writer, status, &total);
        }
    }
    mc_region_writer_free(writer);
    free(input);
    return success && fflush(stdout) == 0 ? 0 : 2;
}
