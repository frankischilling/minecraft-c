#include "mc/storage/region/region.h"

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

static void put_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static void location(uint8_t *bytes, size_t index, uint32_t value) {
    put_u32(bytes + 4u * index, value);
}

static void timestamp(uint8_t *bytes, size_t index, uint32_t value) {
    put_u32(bytes + MC_REGION_SECTOR_BYTES + 4u * index, value);
}

static void record(uint8_t *bytes, size_t sector, uint32_t length, uint8_t selector) {
    put_u32(bytes + sector * MC_REGION_SECTOR_BYTES, length);
    bytes[sector * MC_REGION_SECTOR_BYTES + 4u] = selector;
}

static int expect_record_failure(const mc_region_view *view, int32_t x, int32_t z,
                                 mc_region_status expected) {
    uint8_t sentinel = 0xa5;
    mc_region_payload output = {&sentinel, 98765, 17};
    CHECK(mc_region_record(view, x, z, &output) == expected);
    CHECK(output.data == &sentinel && output.length == 98765 && output.selector == 17);
    CHECK(sentinel == 0xa5);
    return 0;
}

static int expect_open_failure(const uint8_t *bytes, size_t length, const mc_region_limits *limits,
                               mc_region_status expected) {
    mc_region_view output;
    mc_region_view saved;
    memset(&output, 0xa5, sizeof(output));
    memcpy(&saved, &output, sizeof(saved));
    CHECK(mc_region_open(bytes, length, limits, &output) == expected);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    return 0;
}

static int test_all_metadata(void) {
    uint8_t bytes[MC_REGION_HEADER_BYTES] = {0};
    uint8_t saved[MC_REGION_HEADER_BYTES];
    mc_region_view view;
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        uint32_t raw = i == 0 ? 0 : UINT32_C(0x80000001) + (uint32_t)i * UINT32_C(0x100);
        uint32_t time = UINT32_C(0x80000000) ^ ((uint32_t)i * UINT32_C(0x01010101));
        if (i == 1)
            time = UINT32_MAX;
        location(bytes, i, raw);
        timestamp(bytes, i, time);
    }
    memcpy(saved, bytes, sizeof(saved));
    CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
    CHECK(view.input == bytes && view.length == sizeof(bytes) && view.trailing_bytes == 0);
    CHECK(view.present_entries == MC_REGION_ENTRY_COUNT - 1u);
    CHECK(view.invalid_entries == MC_REGION_ENTRY_COUNT - 1u);
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        int32_t x = (int32_t)(i % 32u);
        int32_t z = (int32_t)(i / 32u);
        const mc_region_entry *entry = mc_region_entry_at(&view, x, z);
        uint32_t raw = i == 0 ? 0 : UINT32_C(0x80000001) + (uint32_t)i * UINT32_C(0x100);
        uint32_t time = UINT32_C(0x80000000) ^ ((uint32_t)i * UINT32_C(0x01010101));
        if (i == 1)
            time = UINT32_MAX;
        CHECK(entry == &view.entries[i]);
        CHECK(entry->location == raw && entry->timestamp == time);
        CHECK(entry->byte_offset == (uint64_t)(raw >> 8u) * MC_REGION_SECTOR_BYTES);
        CHECK(entry->extent_bytes == (uint64_t)(raw & 255u) * MC_REGION_SECTOR_BYTES);
        CHECK(entry->record_length == 0 && entry->selector == 0);
        if (i == 0) {
            CHECK(entry->issues == 0);
            CHECK(expect_record_failure(&view, x, z, MC_REGION_MISSING) == 0);
        } else {
            CHECK((entry->issues & MC_REGION_ISSUE_EXTENT_BOUNDS) != 0);
        }
    }
    CHECK(memcmp(bytes, saved, sizeof(bytes)) == 0);
    return 0;
}

static int test_coordinate_bounds(void) {
    uint8_t bytes[5u * MC_REGION_SECTOR_BYTES] = {0};
    mc_region_view view;
    location(bytes, 31, UINT32_C(0x201));
    location(bytes, 32, UINT32_C(0x301));
    location(bytes, 1023, UINT32_C(0x401));
    timestamp(bytes, 31, UINT32_C(0x12345678));
    timestamp(bytes, 32, UINT32_C(0x80000000));
    timestamp(bytes, 1023, UINT32_MAX);
    record(bytes, 2, 1, 1);
    record(bytes, 3, 1, 2);
    record(bytes, 4, 1, 1);
    CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
    CHECK(view.present_entries == 3 && view.invalid_entries == 0);
    CHECK(mc_region_entry_at(&view, 31, 0)->timestamp == UINT32_C(0x12345678));
    CHECK(mc_region_entry_at(&view, 0, 1)->timestamp == UINT32_C(0x80000000));
    CHECK(mc_region_entry_at(&view, 31, 31)->timestamp == UINT32_MAX);
    static const int32_t invalid[][2] = {{-1, 0},        {32, 0},        {0, -1},
                                         {0, 32},        {-1, 1},        {INT32_MIN, 0},
                                         {INT32_MAX, 0}, {0, INT32_MIN}, {0, INT32_MAX}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(mc_region_entry_at(&view, invalid[i][0], invalid[i][1]) == NULL);
        CHECK(expect_record_failure(&view, invalid[i][0], invalid[i][1],
                                    MC_REGION_INVALID_COORDINATE) == 0);
    }
    CHECK(mc_region_entry_at(NULL, 0, 0) == NULL);
    CHECK(expect_record_failure(NULL, 0, 0, MC_REGION_INVALID_ARGUMENT) == 0);
    CHECK(mc_region_record(&view, 0, 0, NULL) == MC_REGION_INVALID_ARGUMENT);
    CHECK(mc_region_entry_at(&view, 0, 0)->location == 0);
    CHECK(expect_record_failure(&view, 0, 0, MC_REGION_MISSING) == 0);
    return 0;
}

static int test_extents_and_overlaps(void) {
    uint8_t bytes[5u * MC_REGION_SECTOR_BYTES] = {0};
    mc_region_view view;

    static const struct {
        uint32_t location;
        uint32_t issue;
    } malformed[] = {{UINT32_C(0x200), MC_REGION_ISSUE_EMPTY_EXTENT},
                     {UINT32_C(0x001), MC_REGION_ISSUE_HEADER_SECTOR},
                     {UINT32_C(0x101), MC_REGION_ISSUE_HEADER_SECTOR},
                     {UINT32_C(0x501), MC_REGION_ISSUE_EXTENT_BOUNDS},
                     {UINT32_C(0x4ff), MC_REGION_ISSUE_EXTENT_BOUNDS},
                     {UINT32_C(0x80000001), MC_REGION_ISSUE_EXTENT_BOUNDS},
                     {UINT32_MAX, MC_REGION_ISSUE_EXTENT_BOUNDS}};

    for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
        memset(bytes, 0, sizeof(bytes));
        location(bytes, 0, malformed[i].location);
        timestamp(bytes, 0, UINT32_C(0xfedcba98));
        CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
        const mc_region_entry *entry = mc_region_entry_at(&view, 0, 0);
        CHECK(entry->location == malformed[i].location && entry->timestamp == UINT32_C(0xfedcba98));
        CHECK((entry->issues & malformed[i].issue) != 0);
        CHECK(view.present_entries == 1 && view.invalid_entries == 1);
        CHECK(entry->record_length == 0 && entry->selector == 0);
        CHECK(expect_record_failure(&view, 0, 0, MC_REGION_INVALID_RECORD) == 0);
    }
    memset(bytes, 0, sizeof(bytes));
    location(bytes, 0, UINT32_C(0x202));
    location(bytes, 1, UINT32_C(0x301));
    location(bytes, 2, UINT32_C(0x401));
    timestamp(bytes, 2, UINT32_C(0x89abcdef));
    record(bytes, 2, 1, 1);
    record(bytes, 3, 1, 2);
    record(bytes, 4, 1, 2);
    CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
    CHECK(view.present_entries == 3 && view.invalid_entries == 2);
    CHECK(view.entries[0].issues == MC_REGION_ISSUE_OVERLAP);
    CHECK(view.entries[1].issues == MC_REGION_ISSUE_OVERLAP);
    CHECK(view.entries[2].issues == 0 && view.entries[2].timestamp == UINT32_C(0x89abcdef));
    CHECK(expect_record_failure(&view, 0, 0, MC_REGION_INVALID_RECORD) == 0);
    CHECK(expect_record_failure(&view, 1, 0, MC_REGION_INVALID_RECORD) == 0);
    mc_region_payload output = {0};
    CHECK(mc_region_record(&view, 2, 0, &output) == MC_REGION_OK);
    CHECK(output.data == bytes + 4u * MC_REGION_SECTOR_BYTES + 5u && output.length == 0);
    /* Header aliases and out-of-file extents still participate in overlap diagnosis. */
    static const uint32_t invalid_ranges[] = {UINT32_C(0x102), UINT32_C(0x402)};
    static const uint32_t valid_ranges[] = {UINT32_C(0x201), UINT32_C(0x401)};
    for (size_t i = 0; i < 2; ++i) {
        memset(bytes, 0, sizeof(bytes));
        location(bytes, 0, invalid_ranges[i]);
        location(bytes, 1, valid_ranges[i]);
        CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
        CHECK((view.entries[0].issues & MC_REGION_ISSUE_OVERLAP) != 0);
        CHECK((view.entries[1].issues & MC_REGION_ISSUE_OVERLAP) != 0);
        CHECK((view.entries[0].issues &
               (i == 0 ? MC_REGION_ISSUE_HEADER_SECTOR : MC_REGION_ISSUE_EXTENT_BOUNDS)) != 0);
        CHECK(view.invalid_entries == 2);
    }
    return 0;
}

static int test_record_lengths_and_selectors(void) {
    uint8_t bytes[3u * MC_REGION_SECTOR_BYTES] = {0};
    uint8_t saved[sizeof(bytes)];
    mc_region_view view;
    location(bytes, 0, UINT32_C(0x201));

    static const struct {
        uint32_t length;
        uint32_t issue;
    } lengths[] = {{0, MC_REGION_ISSUE_INVALID_LENGTH},
                   {UINT32_MAX, MC_REGION_ISSUE_INVALID_LENGTH},
                   {UINT32_C(0x80000000), MC_REGION_ISSUE_INVALID_LENGTH},
                   {4093, MC_REGION_ISSUE_RECORD_BOUNDS},
                   {4096, MC_REGION_ISSUE_RECORD_BOUNDS},
                   {4097, MC_REGION_ISSUE_RECORD_BOUNDS},
                   {INT32_MAX, MC_REGION_ISSUE_RECORD_BOUNDS}};

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        record(bytes, 2, lengths[i].length, 2);
        memcpy(saved, bytes, sizeof(saved));
        CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
        CHECK(view.entries[0].record_length == lengths[i].length);
        CHECK(view.entries[0].issues == lengths[i].issue && view.entries[0].selector == 0);
        CHECK(expect_record_failure(&view, 0, 0, MC_REGION_INVALID_RECORD) == 0);
        CHECK(memcmp(bytes, saved, sizeof(bytes)) == 0);
    }
    static const uint32_t valid_lengths[] = {1, 4092};
    for (size_t i = 0; i < 2; ++i) {
        for (uint8_t selector = 1; selector <= 2; ++selector) {
            record(bytes, 2, valid_lengths[i], selector);
            memcpy(saved, bytes, sizeof(saved));
            CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
            CHECK(view.entries[0].issues == 0 && view.entries[0].selector == selector);
            mc_region_payload output = {0};
            CHECK(mc_region_record(&view, 0, 0, &output) == MC_REGION_OK);
            CHECK(output.data == bytes + 2u * MC_REGION_SECTOR_BYTES + 5u);
            CHECK(output.length == valid_lengths[i] - 1u && output.selector == selector);
            CHECK(memcmp(bytes, saved, sizeof(bytes)) == 0);
        }
    }
    static const uint8_t unknown[] = {0, 3, 127, 255};
    for (size_t i = 0; i < sizeof(unknown) / sizeof(unknown[0]); ++i) {
        record(bytes, 2, 1, unknown[i]);
        CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
        CHECK(view.entries[0].issues == MC_REGION_ISSUE_UNSUPPORTED_SELECTOR);
        CHECK(view.entries[0].selector == unknown[i]);
        CHECK(expect_record_failure(&view, 0, 0, MC_REGION_UNSUPPORTED_COMPRESSION) == 0);
    }
    record(bytes, 2, 4093, 255);
    CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
    CHECK(view.entries[0].issues == MC_REGION_ISSUE_RECORD_BOUNDS && view.entries[0].selector == 0);
    CHECK(expect_record_failure(&view, 0, 0, MC_REGION_INVALID_RECORD) == 0);
    return 0;
}

static int test_partial_sectors_and_limits(void) {
    uint8_t bytes[4u * MC_REGION_SECTOR_BYTES] = {0};
    uint8_t saved[sizeof(bytes)];
    mc_region_view view;
    location(bytes, 0, UINT32_C(0x201));
    record(bytes, 2, 1, 2);
    memcpy(saved, bytes, sizeof(saved));
    static const size_t partial[] = {1, 5, 17, MC_REGION_SECTOR_BYTES - 1u};
    for (size_t i = 0; i < sizeof(partial) / sizeof(partial[0]); ++i) {
        size_t length = MC_REGION_HEADER_BYTES + partial[i];
        CHECK(mc_region_open(bytes, length, NULL, &view) == MC_REGION_OK);
        CHECK(view.trailing_bytes == partial[i] && view.length == length);
        CHECK(view.entries[0].issues == MC_REGION_ISSUE_EXTENT_BOUNDS);
        CHECK(view.entries[0].record_length == 0 && view.entries[0].selector == 0);
        CHECK(expect_record_failure(&view, 0, 0, MC_REGION_INVALID_RECORD) == 0);
        CHECK(memcmp(bytes, saved, sizeof(bytes)) == 0);
    }
    CHECK(mc_region_open(bytes, 3u * MC_REGION_SECTOR_BYTES + 17u, NULL, &view) == MC_REGION_OK);
    CHECK(view.trailing_bytes == 17 && view.invalid_entries == 0);
    mc_region_payload output = {0};
    CHECK(mc_region_record(&view, 0, 0, &output) == MC_REGION_OK);
    mc_region_limits limits = mc_region_default_limits();
    CHECK(limits.max_input_bytes == 1024u * 1024u * 1024u);
    limits.max_input_bytes = sizeof(bytes) - 1u;
    CHECK(expect_open_failure(bytes, sizeof(bytes), &limits, MC_REGION_LIMIT) == 0);
    limits.max_input_bytes = sizeof(bytes);
    CHECK(mc_region_open(bytes, sizeof(bytes), &limits, &view) == MC_REGION_OK);
    limits.max_input_bytes = 0;
    CHECK(expect_open_failure(bytes, MC_REGION_HEADER_BYTES, &limits, MC_REGION_LIMIT) == 0);
    CHECK(expect_open_failure(NULL, 0, &limits, MC_REGION_TRUNCATED_HEADER) == 0);
    for (size_t length = 0; length < MC_REGION_HEADER_BYTES; ++length)
        CHECK(expect_open_failure(bytes, length, NULL, MC_REGION_TRUNCATED_HEADER) == 0);
    CHECK(expect_open_failure(NULL, 1, NULL, MC_REGION_INVALID_ARGUMENT) == 0);
    CHECK(mc_region_open(bytes, sizeof(bytes), NULL, NULL) == MC_REGION_INVALID_ARGUMENT);
    CHECK(memcmp(bytes, saved, sizeof(bytes)) == 0);
    for (int status = MC_REGION_OK; status <= MC_REGION_UNSUPPORTED_COMPRESSION; ++status)
        CHECK(mc_region_status_string((mc_region_status)status) != NULL);
    CHECK(mc_region_status_string((mc_region_status)99) != NULL);
    return 0;
}

static int test_multisector_records(void) {
    uint8_t bytes[5u * MC_REGION_SECTOR_BYTES] = {0};
    mc_region_view view;
    location(bytes, 0, UINT32_C(0x202));
    location(bytes, 1, UINT32_C(0x401));
    timestamp(bytes, 1, UINT32_C(0x89abcdef));
    const size_t max_payload = 2u * MC_REGION_SECTOR_BYTES - 5u;
    for (size_t i = 0; i < max_payload; ++i)
        bytes[2u * MC_REGION_SECTOR_BYTES + 5u + i] = (uint8_t)(i & 255u);
    record(bytes, 4, 1, 2);
    static const uint32_t lengths[] = {4093, 8188};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        record(bytes, 2, lengths[i], 1);
        CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
        CHECK(view.present_entries == 2 && view.invalid_entries == 0);
        CHECK(view.entries[0].extent_bytes == 2u * MC_REGION_SECTOR_BYTES);
        mc_region_payload output = {0};
        CHECK(mc_region_record(&view, 0, 0, &output) == MC_REGION_OK);
        CHECK(output.data == bytes + 2u * MC_REGION_SECTOR_BYTES + 5u);
        CHECK(output.length == lengths[i] - 1u && output.selector == 1);
        for (size_t index = 0; index < output.length; ++index)
            CHECK(output.data[index] == (uint8_t)(index & 255u));
        CHECK(view.entries[1].timestamp == UINT32_C(0x89abcdef) && view.entries[1].issues == 0);
        CHECK(mc_region_record(&view, 1, 0, &output) == MC_REGION_OK);
        CHECK(output.data == bytes + 4u * MC_REGION_SECTOR_BYTES + 5u);
        CHECK(output.length == 0 && output.selector == 2);
    }
    record(bytes, 2, 8189, 1);
    CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
    CHECK(view.entries[0].issues == MC_REGION_ISSUE_RECORD_BOUNDS);
    CHECK(view.entries[1].issues == 0 && view.entries[1].timestamp == UINT32_C(0x89abcdef));
    CHECK(expect_record_failure(&view, 0, 0, MC_REGION_INVALID_RECORD) == 0);
    return 0;
}

static int test_borrowed_payload(void) {
    uint8_t bytes[3u * MC_REGION_SECTOR_BYTES] = {0};
    uint8_t saved[sizeof(bytes)];
    static const uint8_t payload[] = {0, 0xff, 0x7f, 0x80, 0, 1};
    location(bytes, 0, UINT32_C(0x201));
    record(bytes, 2, (uint32_t)sizeof(payload) + 1u, 2);
    memcpy(bytes + 2u * MC_REGION_SECTOR_BYTES + 5u, payload, sizeof(payload));
    memcpy(saved, bytes, sizeof(saved));
    mc_region_view view;
    CHECK(mc_region_open(bytes, sizeof(bytes), NULL, &view) == MC_REGION_OK);
    CHECK(view.input == bytes && view.length == sizeof(bytes));
    mc_region_payload output = {0};
    CHECK(mc_region_record(&view, 0, 0, &output) == MC_REGION_OK);
    CHECK(output.data == bytes + 2u * MC_REGION_SECTOR_BYTES + 5u);
    CHECK(output.length == sizeof(payload) && output.selector == 2);
    CHECK(memcmp(output.data, payload, sizeof(payload)) == 0);
    CHECK(memcmp(bytes, saved, sizeof(bytes)) == 0);
    return 0;
}

static int test_bounded_mutations(void) {
    uint8_t original[3u * MC_REGION_SECTOR_BYTES] = {0};
    uint8_t mutated[sizeof(original)];
    uint8_t saved[sizeof(original)];
    mc_region_view view;
    location(original, 0, UINT32_C(0x201));
    timestamp(original, 0, UINT32_C(0x80000000));
    record(original, 2, 4, 2);
    static const size_t edges[] = {0,    1,    2,    3,    4,    4095, 4096, 4097,
                                   8191, 8192, 8193, 8194, 8195, 8196, 8197, 12287};
    static const uint8_t masks[] = {1, 0x80, 0xff};
    uint32_t state = UINT32_C(0x418ac25b);
    for (size_t round = 0; round < 272; ++round) {
        size_t index;
        if (round < sizeof(edges) / sizeof(edges[0])) {
            index = edges[round];
        } else {
            state ^= state << 13u;
            state ^= state >> 17u;
            state ^= state << 5u;
            index = (size_t)state % sizeof(original);
        }
        for (size_t mutation = 0; mutation < sizeof(masks) / sizeof(masks[0]); ++mutation) {
            memcpy(mutated, original, sizeof(mutated));
            mutated[index] ^= masks[mutation];
            memcpy(saved, mutated, sizeof(saved));
            CHECK(mc_region_open(mutated, sizeof(mutated), NULL, &view) == MC_REGION_OK);
            CHECK(view.input == mutated && view.length == sizeof(mutated) &&
                  view.trailing_bytes == 0);
            CHECK(view.invalid_entries <= view.present_entries &&
                  view.present_entries <= MC_REGION_ENTRY_COUNT);
            uint8_t sentinel = 0xa5;
            mc_region_payload output = {&sentinel, 98765, 17};
            mc_region_status status = mc_region_record(&view, 0, 0, &output);
            if (status == MC_REGION_OK) {
                uintptr_t begin = (uintptr_t)mutated;
                uintptr_t end = begin + sizeof(mutated);
                uintptr_t data = (uintptr_t)output.data;
                CHECK(data >= begin && data <= end);
                CHECK(output.length <= end - data);
                CHECK(output.selector == 1 || output.selector == 2);
            } else {
                CHECK(output.data == &sentinel && output.length == 98765 && output.selector == 17);
            }
            CHECK(sentinel == 0xa5);
            CHECK(memcmp(mutated, saved, sizeof(mutated)) == 0);
        }
    }
    return 0;
}

int main(void) {
    if (test_all_metadata() || test_coordinate_bounds() || test_extents_and_overlaps() ||
        test_record_lengths_and_selectors() || test_partial_sectors_and_limits() ||
        test_multisector_records() || test_borrowed_payload() || test_bounded_mutations())
        return 1;
    printf("Region: %u checks passed\n", checks);
    return 0;
}
