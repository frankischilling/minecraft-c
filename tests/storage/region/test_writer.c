#include "mc/storage/region/region.h"
#include "mc/storage/region/writer.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
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

static void put_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static uint32_t get_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] << 24u | (uint32_t)bytes[1] << 16u | (uint32_t)bytes[2] << 8u |
           bytes[3];
}

/* Fixed expected locations are supplied by each case, not selected by a second
 * allocator. Only the bytes written by that known commit are changed here. */
static void expected_record(uint8_t *image, size_t index, size_t sector, uint32_t count,
                            const uint8_t *payload, size_t length, uint32_t timestamp)
{
    put_u32(image + 4u * index, (uint32_t)sector << 8u | count);
    put_u32(image + MC_REGION_SECTOR_BYTES + 4u * index, timestamp);
    put_u32(image + sector * MC_REGION_SECTOR_BYTES, (uint32_t)length + UINT32_C(1));
    image[sector * MC_REGION_SECTOR_BYTES + 4u] = 2;
    if (length != 0) {
        memcpy(image + sector * MC_REGION_SECTOR_BYTES + 5u, payload, length);
    }
}

static int expect_image(const mc_region_writer *writer, const uint8_t *expected, size_t length)
{
    mc_region_writer_bytes bytes = mc_region_writer_get_bytes(writer);
    CHECK(bytes.data != NULL && bytes.length == length);
    CHECK(memcmp(bytes.data, expected, length) == 0);
    return 0;
}

static int expect_write_failure(mc_region_writer *writer, int32_t x, int32_t z,
                                const uint8_t *payload, size_t length,
                                mc_region_writer_status expected)
{
    mc_region_writer_bytes before = mc_region_writer_get_bytes(writer);
    uint8_t *saved = malloc(before.length);
    CHECK(saved != NULL);
    memcpy(saved, before.data, before.length);
    CHECK(mc_region_writer_write_zlib(writer, x, z, payload, length, UINT32_C(0x12345678)) ==
          expected);
    mc_region_writer_bytes after = mc_region_writer_get_bytes(writer);
    CHECK(after.data == before.data && after.length == before.length);
    CHECK(memcmp(after.data, saved, before.length) == 0);
    free(saved);
    return 0;
}

static int expect_import_failure(const uint8_t *input, size_t length,
                                 const mc_region_writer_limits *limits, mc_region_writer *sentinel,
                                 mc_region_writer_status expected)
{
    mc_region_writer *result = sentinel;
    uint8_t *saved = NULL;
    if (input != NULL && length != 0) {
        saved = malloc(length);
        CHECK(saved != NULL);
        memcpy(saved, input, length);
    }
    CHECK(mc_region_writer_import(input, length, limits, &result) == expected);
    CHECK(result == sentinel);
    if (saved != NULL) {
        CHECK(memcmp(saved, input, length) == 0);
    }
    free(saved);
    return 0;
}

static int test_creation_nulls_and_status(void)
{
    mc_region_writer_limits limits = mc_region_writer_default_limits();
    CHECK(limits.max_image_bytes == 64u * 1024u * 1024u);
    CHECK(limits.max_allocation_bytes == 128u * 1024u * 1024u);
    CHECK(MC_REGION_WRITER_MAX_IMAGE_BYTES == UINT32_C(0x7ffff000));
    CHECK(MC_REGION_WRITER_MAX_ZLIB_BYTES == 1044474u);
    static const struct {
        mc_region_writer_status status;
        const char *text;
    } statuses[] = {
        {MC_REGION_WRITER_OK, "ok"},
        {MC_REGION_WRITER_INVALID_ARGUMENT, "invalid argument"},
        {MC_REGION_WRITER_INVALID_COORDINATE, "invalid local coordinate"},
        {MC_REGION_WRITER_INVALID_IMAGE, "unsafe region allocation image"},
        {MC_REGION_WRITER_RECORD_TOO_LARGE, "region record requires 256 or more sectors"},
        {MC_REGION_WRITER_LIMIT, "resource limit"},
        {MC_REGION_WRITER_ALLOCATION_FAILED, "allocation failed"}};
    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); ++i) {
        CHECK(strcmp(mc_region_writer_status_string(statuses[i].status), statuses[i].text) == 0);
    }
    CHECK(strcmp(mc_region_writer_status_string((mc_region_writer_status)999),
                 "unknown region writer status") == 0);
    CHECK(mc_region_writer_create(NULL, NULL) == MC_REGION_WRITER_INVALID_ARGUMENT);
    CHECK(mc_region_writer_import(NULL, 0, NULL, NULL) == MC_REGION_WRITER_INVALID_ARGUMENT);
    mc_region_writer_bytes empty = mc_region_writer_get_bytes(NULL);
    CHECK(empty.data == NULL && empty.length == 0);
    CHECK(mc_region_writer_consume_growth(NULL) == 0);
    mc_region_writer_free(NULL);
    CHECK(mc_region_writer_write_zlib(NULL, 0, 0, NULL, 0, 0) == MC_REGION_WRITER_INVALID_ARGUMENT);

    uint8_t zeros[MC_REGION_HEADER_BYTES] = {0};
    mc_region_writer *writer = NULL;
    CHECK(mc_region_writer_create(NULL, &writer) == MC_REGION_WRITER_OK);
    CHECK(expect_image(writer, zeros, sizeof(zeros)) == 0);
    mc_region_writer_bytes initial = mc_region_writer_get_bytes(writer);
    CHECK(mc_region_writer_consume_growth(writer) == MC_REGION_HEADER_BYTES);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    CHECK(mc_region_writer_get_bytes(writer).data == initial.data);
    CHECK(expect_write_failure(writer, 0, 0, NULL, 1, MC_REGION_WRITER_INVALID_ARGUMENT) == 0);
    CHECK(expect_import_failure(NULL, 0, NULL, writer, MC_REGION_WRITER_INVALID_IMAGE) == 0);
    CHECK(expect_import_failure(NULL, 1, NULL, writer, MC_REGION_WRITER_INVALID_ARGUMENT) == 0);
    CHECK(mc_region_writer_import(zeros, sizeof(zeros), NULL, NULL) ==
          MC_REGION_WRITER_INVALID_ARGUMENT);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    mc_region_writer_free(writer);
    return 0;
}

static int test_coordinates_and_empty_payload(void)
{
    uint8_t expected[5u * MC_REGION_SECTOR_BYTES] = {0};
    const uint8_t payload[] = {0xde, 0xad, 0x00, 0xff};
    mc_region_writer *writer = NULL;
    CHECK(mc_region_writer_create(NULL, &writer) == MC_REGION_WRITER_OK);
    CHECK(mc_region_writer_write_zlib(writer, 31, 0, payload, sizeof(payload), 0) ==
          MC_REGION_WRITER_OK);
    expected_record(expected, 31, 2, 1, payload, sizeof(payload), 0);
    CHECK(expect_image(writer, expected, 3u * MC_REGION_SECTOR_BYTES) == 0);
    CHECK(mc_region_writer_write_zlib(writer, 0, 1, NULL, 0, UINT32_C(0x80000000)) ==
          MC_REGION_WRITER_OK);
    expected_record(expected, 32, 3, 1, NULL, 0, UINT32_C(0x80000000));
    CHECK(expect_image(writer, expected, 4u * MC_REGION_SECTOR_BYTES) == 0);
    CHECK(mc_region_writer_write_zlib(writer, 31, 31, payload, 0, UINT32_MAX) ==
          MC_REGION_WRITER_OK);
    expected_record(expected, 1023, 4, 1, payload, 0, UINT32_MAX);
    CHECK(expect_image(writer, expected, sizeof(expected)) == 0);
    static const int32_t invalid[][2] = {{-1, 0},        {32, 0},        {0, -1},
                                         {0, 32},        {-1, 1},        {INT32_MIN, 0},
                                         {INT32_MAX, 0}, {0, INT32_MIN}, {0, INT32_MAX}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(expect_write_failure(writer, invalid[i][0], invalid[i][1], payload, sizeof(payload),
                                   MC_REGION_WRITER_INVALID_COORDINATE) == 0);
    }
    CHECK(mc_region_writer_consume_growth(writer) == sizeof(expected));
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    mc_region_writer_free(writer);
    return 0;
}

static int test_boundaries_and_maximum(void)
{
    const size_t payload_length = MC_REGION_WRITER_MAX_ZLIB_BYTES;
    const size_t image_length = 257u * MC_REGION_SECTOR_BYTES;
    uint8_t *payload = malloc(payload_length + 1u);
    uint8_t *expected = calloc(image_length, 1);
    CHECK(payload != NULL && expected != NULL);
    for (size_t i = 0; i <= payload_length; ++i) {
        payload[i] = (uint8_t)(i ^ (i >> 8u));
    }
    mc_region_writer *writer = NULL;
    CHECK(mc_region_writer_create(NULL, &writer) == MC_REGION_WRITER_OK);
    CHECK(expect_write_failure(writer, 0, 0, payload, payload_length + 1u,
                               MC_REGION_WRITER_RECORD_TOO_LARGE) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == MC_REGION_HEADER_BYTES);
    CHECK(mc_region_writer_write_zlib(writer, 0, 0, payload, 4090, 1) == MC_REGION_WRITER_OK);
    expected_record(expected, 0, 2, 1, payload, 4090, 1);
    CHECK(expect_image(writer, expected, 3u * MC_REGION_SECTOR_BYTES) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == MC_REGION_SECTOR_BYTES);
    CHECK(mc_region_writer_write_zlib(writer, 1, 0, payload, 4091, 2) == MC_REGION_WRITER_OK);
    expected_record(expected, 1, 3, 2, payload, 4091, 2);
    CHECK(expect_image(writer, expected, 5u * MC_REGION_SECTOR_BYTES) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 2u * MC_REGION_SECTOR_BYTES);
    mc_region_writer_free(writer);

    memset(expected, 0, image_length);
    CHECK(mc_region_writer_create(NULL, &writer) == MC_REGION_WRITER_OK);
    CHECK(mc_region_writer_write_zlib(writer, 31, 31, payload, payload_length,
                                      UINT32_C(0x80000001)) == MC_REGION_WRITER_OK);
    expected_record(expected, 1023, 2, 255, payload, payload_length, UINT32_C(0x80000001));
    CHECK(expect_image(writer, expected, image_length) == 0);
    CHECK(expect_write_failure(writer, 31, 31, payload, payload_length + 1u,
                               MC_REGION_WRITER_RECORD_TOO_LARGE) == 0);
    CHECK(expect_write_failure(writer, 0, 0, payload, SIZE_MAX,
                               MC_REGION_WRITER_RECORD_TOO_LARGE) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == image_length);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    mc_region_writer_free(writer);
    free(expected);
    free(payload);
    return 0;
}

static int test_rewrite_reuse_and_growth(void)
{
    const size_t expected_length = 14u * MC_REGION_SECTOR_BYTES;
    uint8_t *expected = calloc(expected_length, 1);
    uint8_t *payload = malloc(12283);
    CHECK(expected != NULL && payload != NULL);
    for (size_t i = 0; i < 12283; ++i) {
        payload[i] = (uint8_t)(0xa5u ^ i);
    }
    static const struct {
        int32_t x;
        size_t length;
        size_t sector;
        uint32_t count;
        size_t file_sectors;
        uint32_t growth;
    } operations[] = {{0, 4090, 2, 1, 3, 4096},    {1, 4091, 3, 2, 5, 8192},
                      {2, 8190, 5, 3, 8, 12288},   {1, 7, 3, 1, 8, 0},
                      {3, 0, 4, 1, 8, 0},          {0, 1000, 2, 1, 8, 0},
                      {2, 12283, 8, 4, 12, 16384}, {4, 4091, 5, 2, 12, 0},
                      {5, 0, 7, 1, 12, 0},         {6, 4091, 12, 2, 14, 8192}};
    mc_region_writer *writer = NULL;
    CHECK(mc_region_writer_create(NULL, &writer) == MC_REGION_WRITER_OK);
    CHECK(mc_region_writer_consume_growth(writer) == MC_REGION_HEADER_BYTES);
    for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); ++i) {
        uint32_t timestamp = UINT32_C(0xf0000000) + (uint32_t)i;
        CHECK(mc_region_writer_write_zlib(writer, operations[i].x, 0, payload, operations[i].length,
                                          timestamp) == MC_REGION_WRITER_OK);
        expected_record(expected, (size_t)operations[i].x, operations[i].sector,
                        operations[i].count, payload, operations[i].length, timestamp);
        CHECK(expect_image(writer, expected, operations[i].file_sectors * MC_REGION_SECTOR_BYTES) ==
              0);
        CHECK(mc_region_writer_consume_growth(writer) == operations[i].growth);
        CHECK(mc_region_writer_consume_growth(writer) == 0);
    }
    /* Shrink B frees sector4. Rewrite A remains at2 even though later holes
     * exist. Growing C cannot combine its freed three-sector tail with one new
     * sector: it appends all four sectors at8, retaining old payload bytes. */
    CHECK(get_u32(expected + 4u * 2u) == UINT32_C(0x804));
    mc_region_writer_free(writer);

    /* Same-size overwrite bypasses earlier free space and retains the old
     * allocation and every byte after the new shorter payload. */
    uint8_t imported[10u * MC_REGION_SECTOR_BYTES];
    memset(imported, 0xa7, sizeof(imported));
    memset(imported, 0, MC_REGION_HEADER_BYTES);
    expected_record(imported, 0, 4, 1, payload, 1000, 10);
    CHECK(mc_region_writer_import(imported, sizeof(imported), NULL, &writer) ==
          MC_REGION_WRITER_OK);
    expected_record(imported, 0, 4, 1, payload, 1, 11);
    CHECK(mc_region_writer_write_zlib(writer, 0, 0, payload, 1, 11) == MC_REGION_WRITER_OK);
    CHECK(expect_image(writer, imported, sizeof(imported)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    mc_region_writer_free(writer);

    /* A free prefix precedes the old run. Releasing the old run joins that
     * prefix into the first adequate gap, ahead of another sufficient gap. */
    memset(imported, 0xa7, sizeof(imported));
    memset(imported, 0, MC_REGION_HEADER_BYTES);
    expected_record(imported, 0, 3, 2, payload, 4091, 17);
    expected_record(imported, 1, 5, 1, payload, 1, 18);
    CHECK(mc_region_writer_import(imported, sizeof(imported), NULL, &writer) ==
          MC_REGION_WRITER_OK);
    expected_record(imported, 0, 2, 3, payload, 8190, 19);
    CHECK(mc_region_writer_write_zlib(writer, 0, 0, payload, 8190, 19) == MC_REGION_WRITER_OK);
    CHECK(expect_image(writer, imported, sizeof(imported)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    mc_region_writer_free(writer);

    /* A one-sector free tail is insufficient: append a whole two-sector run. */
    uint8_t tail[6u * MC_REGION_SECTOR_BYTES] = {0};
    expected_record(tail, 0, 2, 1, payload, 1, 23);
    CHECK(mc_region_writer_import(tail, 4u * MC_REGION_SECTOR_BYTES, NULL, &writer) ==
          MC_REGION_WRITER_OK);
    expected_record(tail, 1, 4, 2, payload, 4091, 24);
    CHECK(mc_region_writer_write_zlib(writer, 1, 0, payload, 4091, 24) == MC_REGION_WRITER_OK);
    CHECK(expect_image(writer, tail, sizeof(tail)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 2u * MC_REGION_SECTOR_BYTES);
    mc_region_writer_free(writer);
    free(payload);
    free(expected);
    return 0;
}

static int test_import_preservation_and_rejections(void)
{
    uint8_t input[9u * MC_REGION_SECTOR_BYTES];
    uint8_t saved[sizeof(input)];
    memset(input, 0xa7, sizeof(input));
    memset(input, 0, MC_REGION_HEADER_BYTES);
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        put_u32(input + MC_REGION_SECTOR_BYTES + 4u * i, UINT32_C(0x80000000) + (uint32_t)i);
    }
    for (size_t i = 0; i < 5; ++i) {
        put_u32(input + 4u * i, (uint32_t)(i + 2u) << 8u | UINT32_C(1));
    }
    put_u32(input + 2u * MC_REGION_SECTOR_BYTES, 1);
    input[2u * MC_REGION_SECTOR_BYTES + 4u] = 99;
    put_u32(input + 3u * MC_REGION_SECTOR_BYTES, UINT32_MAX);
    put_u32(input + 4u * MC_REGION_SECTOR_BYTES, 4096);
    put_u32(input + 5u * MC_REGION_SECTOR_BYTES, 0);
    put_u32(input + 6u * MC_REGION_SECTOR_BYTES, 2);
    input[6u * MC_REGION_SECTOR_BYTES + 4u] = 1;
    memcpy(saved, input, sizeof(saved));
    mc_region_view view;
    CHECK(mc_region_open(input, sizeof(input), NULL, &view) == MC_REGION_OK);
    CHECK(view.invalid_entries == 4 && view.entries[4].issues == 0);
    mc_region_writer *writer = NULL;
    CHECK(mc_region_writer_import(input, sizeof(input), NULL, &writer) == MC_REGION_WRITER_OK);
    CHECK(mc_region_writer_get_bytes(writer).data != input);
    CHECK(expect_image(writer, saved, sizeof(saved)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    input[8u * MC_REGION_SECTOR_BYTES] ^= 0xff;
    CHECK(expect_image(writer, saved, sizeof(saved)) == 0);
    memcpy(input, saved, sizeof(input));
    expected_record(saved, 3, 5, 1, NULL, 0, UINT32_MAX);
    CHECK(mc_region_writer_write_zlib(writer, 3, 0, NULL, 0, UINT32_MAX) == MC_REGION_WRITER_OK);
    CHECK(expect_image(writer, saved, sizeof(saved)) == 0);
    expected_record(saved, 5, 7, 1, NULL, 0, 123);
    CHECK(mc_region_writer_write_zlib(writer, 5, 0, NULL, 0, 123) == MC_REGION_WRITER_OK);
    CHECK(expect_image(writer, saved, sizeof(saved)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 0);

    static const uint32_t unsafe[] = {UINT32_C(0x001), UINT32_C(0x101), UINT32_C(0x200),
                                      UINT32_C(0x901), UINT32_C(0x8ff), UINT32_C(0x80000001),
                                      UINT32_MAX};
    for (size_t i = 0; i < sizeof(unsafe) / sizeof(unsafe[0]); ++i) {
        memset(input, 0, MC_REGION_HEADER_BYTES);
        put_u32(input, unsafe[i]);
        CHECK(expect_import_failure(input, sizeof(input), NULL, writer,
                                    MC_REGION_WRITER_INVALID_IMAGE) == 0);
    }
    static const uint32_t overlaps[][2] = {{UINT32_C(0x201), UINT32_C(0x201)},
                                           {UINT32_C(0x202), UINT32_C(0x301)},
                                           {UINT32_C(0x204), UINT32_C(0x301)}};
    for (size_t i = 0; i < sizeof(overlaps) / sizeof(overlaps[0]); ++i) {
        memset(input, 0, MC_REGION_HEADER_BYTES);
        put_u32(input, overlaps[i][0]);
        put_u32(input + 4u, overlaps[i][1]);
        CHECK(expect_import_failure(input, sizeof(input), NULL, writer,
                                    MC_REGION_WRITER_INVALID_IMAGE) == 0);
    }
    memset(input, 0, MC_REGION_HEADER_BYTES);
    static const size_t lengths[] = {0, 1, 4095, 4096, 8191, 8193, sizeof(input) - 1u};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        CHECK(expect_import_failure(input, lengths[i], NULL, writer,
                                    MC_REGION_WRITER_INVALID_IMAGE) == 0);
    }
    CHECK(expect_image(writer, saved, sizeof(saved)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    mc_region_writer_free(writer);
    return 0;
}

/* Find the opaque owner's minimum creation budget rather than depending on its
 * private struct layout or host pointer width. Tests below check the current
 * one-byte-per-sector free-map accounting against this live baseline. */
static int minimum_creation_budget(size_t *budget)
{
    size_t lower = 0;
    size_t upper = 65536;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2u;
        mc_region_writer_limits limits = {MC_REGION_HEADER_BYTES, middle};
        mc_region_writer *writer = NULL;
        mc_region_writer_status status = mc_region_writer_create(&limits, &writer);
        CHECK(status == MC_REGION_WRITER_OK || status == MC_REGION_WRITER_LIMIT);
        if (status == MC_REGION_WRITER_OK) {
            mc_region_writer_free(writer);
            upper = middle;
        } else {
            CHECK(writer == NULL);
            lower = middle + 1u;
        }
    }
    CHECK(lower > MC_REGION_HEADER_BYTES + 2u && lower < 65536);
    *budget = lower;
    return 0;
}

static int test_limits_and_failure_atomicity(void)
{
    uint8_t expected[3u * MC_REGION_SECTOR_BYTES] = {0};
    uint8_t payload[4091];
    memset(payload, 0x5a, sizeof(payload));
    mc_region_writer_limits limits = {sizeof(expected), 128u * 1024u * 1024u};
    mc_region_writer *writer = NULL;
    CHECK(mc_region_writer_create(&limits, &writer) == MC_REGION_WRITER_OK);
    CHECK(mc_region_writer_write_zlib(writer, 0, 0, payload, 4090, 7) == MC_REGION_WRITER_OK);
    expected_record(expected, 0, 2, 1, payload, 4090, 7);
    CHECK(expect_write_failure(writer, 0, 0, payload, sizeof(payload), MC_REGION_WRITER_LIMIT) ==
          0);
    /* A failed resize must not release A's allocation for this following miss. */
    CHECK(expect_write_failure(writer, 1, 0, NULL, 0, MC_REGION_WRITER_LIMIT) == 0);
    CHECK(expect_image(writer, expected, sizeof(expected)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == sizeof(expected));
    CHECK(mc_region_writer_write_zlib(writer, 0, 0, NULL, 0, 8) == MC_REGION_WRITER_OK);
    expected_record(expected, 0, 2, 1, NULL, 0, 8);
    CHECK(expect_image(writer, expected, sizeof(expected)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 0);

    mc_region_writer *output = writer;
    limits.max_image_bytes = (size_t)MC_REGION_WRITER_MAX_IMAGE_BYTES + 1u;
    CHECK(mc_region_writer_create(&limits, &output) == MC_REGION_WRITER_INVALID_ARGUMENT);
    CHECK(output == writer);
    CHECK(expect_import_failure(expected, sizeof(expected), &limits, writer,
                                MC_REGION_WRITER_INVALID_ARGUMENT) == 0);
    limits.max_image_bytes = 0;
    CHECK(mc_region_writer_create(&limits, &output) == MC_REGION_WRITER_LIMIT);
    CHECK(output == writer);
    CHECK(expect_import_failure(expected, sizeof(expected), &limits, writer,
                                MC_REGION_WRITER_LIMIT) == 0);
    limits.max_image_bytes = MC_REGION_HEADER_BYTES - 1u;
    CHECK(mc_region_writer_create(&limits, &output) == MC_REGION_WRITER_LIMIT);
    CHECK(output == writer);
    limits.max_image_bytes = sizeof(expected);
    limits.max_allocation_bytes = 0;
    CHECK(mc_region_writer_create(&limits, &output) == MC_REGION_WRITER_LIMIT);
    CHECK(output == writer);
    CHECK(expect_import_failure(expected, sizeof(expected), &limits, writer,
                                MC_REGION_WRITER_LIMIT) == 0);
    CHECK(expect_image(writer, expected, sizeof(expected)) == 0);
    mc_region_writer_free(writer);

    size_t minimum = 0;
    CHECK(minimum_creation_budget(&minimum) == 0);
    limits.max_image_bytes = 4u * MC_REGION_SECTOR_BYTES;
    limits.max_allocation_bytes = minimum - 1u;
    writer = NULL;
    CHECK(mc_region_writer_create(&limits, &writer) == MC_REGION_WRITER_LIMIT);
    CHECK(writer == NULL);
    limits.max_allocation_bytes = minimum;
    CHECK(mc_region_writer_create(&limits, &writer) == MC_REGION_WRITER_OK);
    CHECK(expect_write_failure(writer, 0, 0, NULL, 0, MC_REGION_WRITER_LIMIT) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == MC_REGION_HEADER_BYTES);
    mc_region_writer_free(writer);

    /* Import needs one more free-map byte for each additional image sector. */
    limits.max_allocation_bytes = minimum + MC_REGION_SECTOR_BYTES;
    CHECK(expect_import_failure(expected, sizeof(expected), &limits, NULL,
                                MC_REGION_WRITER_LIMIT) == 0);
    ++limits.max_allocation_bytes;
    CHECK(mc_region_writer_import(expected, sizeof(expected), &limits, &writer) ==
          MC_REGION_WRITER_OK);
    CHECK(expect_image(writer, expected, sizeof(expected)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    mc_region_writer_free(writer);

    /* Existing buffers plus both replacements count toward the peak. The exact
     * first growth fits only at this threshold; geometric spare capacity must
     * not prevent a write whose exact reservation fits. */
    limits.max_allocation_bytes = minimum + sizeof(expected) + 3u - 1u;
    CHECK(mc_region_writer_create(&limits, &writer) == MC_REGION_WRITER_OK);
    CHECK(expect_write_failure(writer, 0, 0, NULL, 0, MC_REGION_WRITER_LIMIT) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == MC_REGION_HEADER_BYTES);
    mc_region_writer_free(writer);
    ++limits.max_allocation_bytes;
    CHECK(mc_region_writer_create(&limits, &writer) == MC_REGION_WRITER_OK);
    CHECK(mc_region_writer_write_zlib(writer, 0, 0, NULL, 0, 19) == MC_REGION_WRITER_OK);
    memset(expected, 0, sizeof(expected));
    expected_record(expected, 0, 2, 1, NULL, 0, 19);
    CHECK(expect_image(writer, expected, sizeof(expected)) == 0);
    CHECK(expect_write_failure(writer, 1, 0, NULL, 0, MC_REGION_WRITER_LIMIT) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == sizeof(expected));
    CHECK(mc_region_writer_write_zlib(writer, 0, 0, payload, 1, 20) == MC_REGION_WRITER_OK);
    expected_record(expected, 0, 2, 1, payload, 1, 20);
    CHECK(expect_image(writer, expected, sizeof(expected)) == 0);
    CHECK(mc_region_writer_consume_growth(writer) == 0);
    mc_region_writer_free(writer);
    return 0;
}

int main(void)
{
    if (test_creation_nulls_and_status() != 0 || test_coordinates_and_empty_payload() != 0 ||
        test_boundaries_and_maximum() != 0 || test_rewrite_reuse_and_growth() != 0 ||
        test_import_preservation_and_rejections() != 0 ||
        test_limits_and_failure_atomicity() != 0) {
        return 1;
    }
    printf("region writer: %u checks passed\n", checks);
    return 0;
}
