#include "mc/storage/region/writer.h"

#include "mc/storage/region/region.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "Region storage requires eight-bit bytes");

struct mc_region_writer {
    mc_region_writer_limits limits;
    uint8_t *image;
    uint8_t *free_sectors;
    size_t length;
    size_t capacity;
    uint32_t growth;
};

mc_region_writer_limits mc_region_writer_default_limits(void)
{
    const mc_region_writer_limits limits = {64u * 1024u * 1024u, 128u * 1024u * 1024u};
    return limits;
}

const char *mc_region_writer_status_string(mc_region_writer_status status)
{
    switch (status) {
        case MC_REGION_WRITER_OK:
            return "ok";
        case MC_REGION_WRITER_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_REGION_WRITER_INVALID_COORDINATE:
            return "invalid local coordinate";
        case MC_REGION_WRITER_INVALID_IMAGE:
            return "unsafe region allocation image";
        case MC_REGION_WRITER_RECORD_TOO_LARGE:
            return "region record requires 256 or more sectors";
        case MC_REGION_WRITER_LIMIT:
            return "resource limit";
        case MC_REGION_WRITER_ALLOCATION_FAILED:
            return "allocation failed";
    }
    return "unknown region writer status";
}

static uint32_t writer_read_u32(const uint8_t *input)
{
    return (uint32_t)input[0] << 24u | (uint32_t)input[1] << 16u | (uint32_t)input[2] << 8u |
           input[3];
}

static void writer_write_u32(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value >> 24u);
    output[1] = (uint8_t)(value >> 16u);
    output[2] = (uint8_t)(value >> 8u);
    output[3] = (uint8_t)value;
}

static bool writer_add_size(size_t left, size_t right, size_t *result)
{
    if (right > SIZE_MAX - left) {
        return false;
    }
    *result = left + right;
    return true;
}

static bool writer_allocation_size(size_t capacity, size_t *result)
{
    size_t bytes;
    return writer_add_size(sizeof(mc_region_writer), capacity, &bytes) &&
           writer_add_size(bytes, capacity / MC_REGION_SECTOR_BYTES, result);
}

static mc_region_writer_status writer_get_limits(const mc_region_writer_limits *requested,
                                                 mc_region_writer_limits *limits)
{
    *limits = requested == NULL ? mc_region_writer_default_limits() : *requested;
    if (limits->max_image_bytes > MC_REGION_WRITER_MAX_IMAGE_BYTES) {
        return MC_REGION_WRITER_INVALID_ARGUMENT;
    }
    return MC_REGION_WRITER_OK;
}

static bool writer_safe_image(const uint8_t *input, size_t length)
{
    if (length < MC_REGION_HEADER_BYTES || length % MC_REGION_SECTOR_BYTES != 0) {
        return false;
    }
    size_t sectors = length / MC_REGION_SECTOR_BYTES;
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        uint32_t location = writer_read_u32(input + 4u * i);
        if (location == 0) {
            continue;
        }
        size_t start = location >> 8u;
        size_t count = location & UINT32_C(255);
        if (start < 2 || count == 0 || start > sectors || count > sectors - start) {
            return false;
        }
        size_t end = start + count;
        for (size_t j = 0; j < i; ++j) {
            uint32_t previous = writer_read_u32(input + 4u * j);
            if (previous == 0) {
                continue;
            }
            size_t previous_start = previous >> 8u;
            size_t previous_end = previous_start + (previous & UINT32_C(255));
            if (start < previous_end && previous_start < end) {
                return false;
            }
        }
    }
    return true;
}

static mc_region_writer_status writer_allocate(const mc_region_writer_limits *limits, size_t length,
                                               mc_region_writer **out)
{
    size_t bytes;
    if (length > limits->max_image_bytes || !writer_allocation_size(length, &bytes) ||
        bytes > limits->max_allocation_bytes) {
        return MC_REGION_WRITER_LIMIT;
    }
    mc_region_writer *writer = malloc(sizeof(*writer));
    if (writer == NULL) {
        return MC_REGION_WRITER_ALLOCATION_FAILED;
    }
    uint8_t *image = malloc(length);
    if (image == NULL) {
        free(writer);
        return MC_REGION_WRITER_ALLOCATION_FAILED;
    }
    uint8_t *free_sectors = malloc(length / MC_REGION_SECTOR_BYTES);
    if (free_sectors == NULL) {
        free(image);
        free(writer);
        return MC_REGION_WRITER_ALLOCATION_FAILED;
    }
    writer->limits = *limits;
    writer->image = image;
    writer->free_sectors = free_sectors;
    writer->length = length;
    writer->capacity = length;
    writer->growth = 0;
    *out = writer;
    return MC_REGION_WRITER_OK;
}

mc_region_writer_status mc_region_writer_create(const mc_region_writer_limits *requested,
                                                mc_region_writer **out)
{
    if (out == NULL) {
        return MC_REGION_WRITER_INVALID_ARGUMENT;
    }
    mc_region_writer_limits limits;
    mc_region_writer_status status = writer_get_limits(requested, &limits);
    if (status != MC_REGION_WRITER_OK) {
        return status;
    }
    mc_region_writer *writer;
    status = writer_allocate(&limits, MC_REGION_HEADER_BYTES, &writer);
    if (status != MC_REGION_WRITER_OK) {
        return status;
    }
    memset(writer->image, 0, writer->length);
    memset(writer->free_sectors, 0, 2);
    writer->growth = MC_REGION_HEADER_BYTES;
    *out = writer;
    return MC_REGION_WRITER_OK;
}

mc_region_writer_status mc_region_writer_import(const uint8_t *input, size_t length,
                                                const mc_region_writer_limits *requested,
                                                mc_region_writer **out)
{
    if (out == NULL || (input == NULL && length != 0)) {
        return MC_REGION_WRITER_INVALID_ARGUMENT;
    }
    mc_region_writer_limits limits;
    mc_region_writer_status status = writer_get_limits(requested, &limits);
    if (status != MC_REGION_WRITER_OK) {
        return status;
    }
    if (length > limits.max_image_bytes) {
        return MC_REGION_WRITER_LIMIT;
    }
    if (!writer_safe_image(input, length)) {
        return MC_REGION_WRITER_INVALID_IMAGE;
    }
    mc_region_writer *writer;
    status = writer_allocate(&limits, length, &writer);
    if (status != MC_REGION_WRITER_OK) {
        return status;
    }
    memcpy(writer->image, input, length);
    memset(writer->free_sectors, 1, length / MC_REGION_SECTOR_BYTES);
    writer->free_sectors[0] = 0;
    writer->free_sectors[1] = 0;
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        uint32_t location = writer_read_u32(input + 4u * i);
        if (location != 0) {
            memset(writer->free_sectors + (location >> 8u), 0, location & UINT32_C(255));
        }
    }
    *out = writer;
    return MC_REGION_WRITER_OK;
}

/* The old extent is considered free during planning, without changing any
 * owned state. A short tail never combines with appended sectors. */
static size_t writer_find_run(const mc_region_writer *writer, size_t old_start, size_t old_count,
                              size_t required)
{
    size_t sectors = writer->length / MC_REGION_SECTOR_BYTES;
    size_t run = 0;
    for (size_t i = 2; i < sectors; ++i) {
        bool old_sector = i >= old_start && i - old_start < old_count;
        if (writer->free_sectors[i] != 0 || old_sector) {
            ++run;
            if (run == required) {
                return i + 1u - required;
            }
        } else {
            run = 0;
        }
    }
    return sectors;
}

static bool writer_growth_fits(const mc_region_writer *writer, size_t capacity)
{
    size_t current_bytes;
    size_t peak_bytes;
    return writer_allocation_size(writer->capacity, &current_bytes) &&
           writer_add_size(current_bytes, capacity, &peak_bytes) &&
           writer_add_size(peak_bytes, capacity / MC_REGION_SECTOR_BYTES, &peak_bytes) &&
           peak_bytes <= writer->limits.max_allocation_bytes;
}

/* Reserve both replacement buffers before committing. Capacity normally doubles
 * but falls back to the exact required size when the peak budget demands it. */
static mc_region_writer_status writer_reserve(const mc_region_writer *writer, size_t length,
                                              uint8_t **image, uint8_t **free_sectors,
                                              size_t *capacity)
{
    *image = writer->image;
    *free_sectors = writer->free_sectors;
    *capacity = writer->capacity;
    if (length <= writer->capacity) {
        return MC_REGION_WRITER_OK;
    }
    size_t maximum =
        writer->limits.max_image_bytes / MC_REGION_SECTOR_BYTES * MC_REGION_SECTOR_BYTES;
    size_t next = writer->capacity;
    while (next < length) {
        next = next > maximum - next ? maximum : next + next;
    }
    if (!writer_growth_fits(writer, next)) {
        next = length;
        if (!writer_growth_fits(writer, next)) {
            return MC_REGION_WRITER_LIMIT;
        }
    }
    uint8_t *new_image = malloc(next);
    if (new_image == NULL) {
        return MC_REGION_WRITER_ALLOCATION_FAILED;
    }
    uint8_t *new_free_sectors = malloc(next / MC_REGION_SECTOR_BYTES);
    if (new_free_sectors == NULL) {
        free(new_image);
        return MC_REGION_WRITER_ALLOCATION_FAILED;
    }
    memcpy(new_image, writer->image, writer->length);
    memcpy(new_free_sectors, writer->free_sectors, writer->length / MC_REGION_SECTOR_BYTES);
    *image = new_image;
    *free_sectors = new_free_sectors;
    *capacity = next;
    return MC_REGION_WRITER_OK;
}

static void writer_payload(uint8_t *image, size_t start, const uint8_t *payload, size_t length)
{
    uint8_t *record = image + start * MC_REGION_SECTOR_BYTES;
    writer_write_u32(record, (uint32_t)length + UINT32_C(1));
    record[4] = 2;
    if (length != 0) {
        memcpy(record + 5u, payload, length);
    }
}

mc_region_writer_status mc_region_writer_write_zlib(mc_region_writer *writer, int32_t x, int32_t z,
                                                    const uint8_t *payload, size_t length,
                                                    uint32_t timestamp)
{
    if (writer == NULL || (payload == NULL && length != 0)) {
        return MC_REGION_WRITER_INVALID_ARGUMENT;
    }
    if (x < 0 || x >= 32 || z < 0 || z >= 32) {
        return MC_REGION_WRITER_INVALID_COORDINATE;
    }
    if (length > MC_REGION_WRITER_MAX_ZLIB_BYTES) {
        return MC_REGION_WRITER_RECORD_TOO_LARGE;
    }
    size_t index = (size_t)x + 32u * (size_t)z;
    uint32_t old_location = writer_read_u32(writer->image + 4u * index);
    size_t old_start = old_location >> 8u;
    size_t old_count = old_location & UINT32_C(255);
    size_t required = (length + 5u) / MC_REGION_SECTOR_BYTES + 1u;
    bool rewrite = old_start != 0 && old_count == required;
    size_t start = rewrite ? old_start : writer_find_run(writer, old_start, old_count, required);
    size_t new_length = writer->length;
    bool grow = !rewrite && start == writer->length / MC_REGION_SECTOR_BYTES;
    if (grow) {
        size_t appended = required * MC_REGION_SECTOR_BYTES;
        if (appended > writer->limits.max_image_bytes - writer->length) {
            return MC_REGION_WRITER_LIMIT;
        }
        new_length += appended;
    }
    uint8_t *image;
    uint8_t *free_sectors;
    size_t capacity;
    mc_region_writer_status status =
        writer_reserve(writer, new_length, &image, &free_sectors, &capacity);
    if (status != MC_REGION_WRITER_OK) {
        return status;
    }

    /* There are no fallible operations after this point. Keep the successful
     * original header/payload ordering, without exposing partial I/O states. */
    if (!rewrite) {
        if (old_count != 0) {
            memset(free_sectors + old_start, 1, old_count);
        }
        if (grow) {
            memset(image + writer->length, 0, new_length - writer->length);
        } else {
            writer_write_u32(image + 4u * index, (uint32_t)start << 8u | (uint32_t)required);
        }
        memset(free_sectors + start, 0, required);
    }
    writer_payload(image, start, payload, length);
    if (grow) {
        writer_write_u32(image + 4u * index, (uint32_t)start << 8u | (uint32_t)required);
    }
    writer_write_u32(image + MC_REGION_SECTOR_BYTES + 4u * index, timestamp);
    if (image != writer->image) {
        free(writer->image);
        free(writer->free_sectors);
        writer->image = image;
        writer->free_sectors = free_sectors;
        writer->capacity = capacity;
    }
    writer->growth += (uint32_t)(new_length - writer->length);
    writer->length = new_length;
    return MC_REGION_WRITER_OK;
}

mc_region_writer_bytes mc_region_writer_get_bytes(const mc_region_writer *writer)
{
    const mc_region_writer_bytes bytes = {writer == NULL ? NULL : writer->image,
                                          writer == NULL ? 0 : writer->length};
    return bytes;
}

uint32_t mc_region_writer_consume_growth(mc_region_writer *writer)
{
    if (writer == NULL) {
        return 0;
    }
    uint32_t growth = writer->growth;
    writer->growth = 0;
    return growth;
}

void mc_region_writer_free(mc_region_writer *writer)
{
    if (writer != NULL) {
        free(writer->image);
        free(writer->free_sectors);
        free(writer);
    }
}
