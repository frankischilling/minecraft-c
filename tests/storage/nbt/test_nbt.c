#include "mc/storage/nbt/nbt.h"

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

static size_t decode_hex(const char *hex, uint8_t *bytes) {
    size_t length = strlen(hex) / 2u;
    for (size_t i = 0; i < length; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            char digit = hex[2u * i + j];
            unsigned int nibble;
            if (digit >= '0' && digit <= '9')
                nibble = (unsigned int)(digit - '0');
            else if (digit >= 'a' && digit <= 'f')
                nibble = (unsigned int)(digit - 'a') + 10u;
            else
                abort();
            value = value * 16u + nibble;
        }
        bytes[i] = (uint8_t)value;
    }
    return length;
}

static int expect_write(const mc_nbt_tag *tag, const char *hex) {
    uint8_t expected[1024];
    uint8_t actual[1024];
    size_t length = decode_hex(hex, expected);
    size_t measured = SIZE_MAX;
    size_t written = SIZE_MAX;
    mc_nbt_error error = {MC_NBT_INVALID_ARGUMENT, SIZE_MAX};
    CHECK(length <= sizeof(expected));
    CHECK(mc_nbt_measure(tag, NULL, &measured, &error) == MC_NBT_OK);
    CHECK(measured == length && error.status == MC_NBT_OK);
    memset(actual, 0xa5, sizeof(actual));
    CHECK(mc_nbt_write(tag, NULL, actual, sizeof(actual), &written, &error) == MC_NBT_OK);
    CHECK(written == measured && error.status == MC_NBT_OK);
    CHECK(memcmp(actual, expected, length) == 0);
    for (size_t i = length; i < sizeof(actual); ++i)
        CHECK(actual[i] == 0xa5);
    return 0;
}

static int expect_read_failure(const uint8_t *bytes, size_t length, const mc_nbt_limits *limits,
                               mc_nbt_status expected) {
    static const uint8_t end[] = {0};
    mc_nbt_document *existing = NULL;
    mc_nbt_document *result;
    size_t consumed = 98765;
    mc_nbt_error error = {MC_NBT_OK, SIZE_MAX};
    CHECK(mc_nbt_read(end, sizeof(end), NULL, &existing, NULL, NULL) == MC_NBT_OK);
    result = existing;
    mc_nbt_status status = mc_nbt_read(bytes, length, limits, &result, &consumed, &error);
    CHECK(status != MC_NBT_OK);
    if (expected != MC_NBT_OK)
        CHECK(status == expected);
    CHECK(error.status == status);
    CHECK(error.offset <= length);
    CHECK(result == existing && consumed == 98765);
    CHECK(mc_nbt_document_root(existing)->type == MC_NBT_END);
    mc_nbt_document_free(existing);
    return 0;
}

static int expect_write_failure(const mc_nbt_tag *tag, const mc_nbt_limits *limits,
                                mc_nbt_status expected) {
    uint8_t output[32];
    uint8_t saved[32];
    size_t written = 98765;
    size_t measured = 54321;
    mc_nbt_error error;
    memset(output, 0xa5, sizeof(output));
    memcpy(saved, output, sizeof(saved));
    mc_nbt_status status = mc_nbt_measure(tag, limits, &measured, &error);
    CHECK(status != MC_NBT_OK);
    if (expected != MC_NBT_OK)
        CHECK(status == expected);
    CHECK(error.status == status && measured == 54321);
    status = mc_nbt_write(tag, limits, output, sizeof(output), &written, &error);
    CHECK(status != MC_NBT_OK);
    if (expected != MC_NBT_OK)
        CHECK(status == expected);
    CHECK(error.status == status && written == 98765);
    CHECK(memcmp(output, saved, sizeof(output)) == 0);
    return 0;
}

static int test_signed_scalars(void) {
    static const struct {
        mc_nbt_type type;
        int64_t value;
        const char *hex;
    } fixtures[] = {
        {MC_NBT_BYTE, INT8_MIN, "01000080"},
        {MC_NBT_BYTE, INT8_MAX, "0100007f"},
        {MC_NBT_BYTE, -1, "010000ff"},
        {MC_NBT_SHORT, INT16_MIN, "0200008000"},
        {MC_NBT_SHORT, INT16_MAX, "0200007fff"},
        {MC_NBT_SHORT, -2, "020000fffe"},
        {MC_NBT_INT, INT32_MIN, "03000080000000"},
        {MC_NBT_INT, INT32_MAX, "0300007fffffff"},
        {MC_NBT_INT, INT32_C(0x12345678), "03000012345678"},
        {MC_NBT_INT, -2, "030000fffffffe"},
        {MC_NBT_LONG, INT64_MIN, "0400008000000000000000"},
        {MC_NBT_LONG, INT64_MAX, "0400007fffffffffffffff"},
        {MC_NBT_LONG, INT64_C(0x0123456789abcdef), "0400000123456789abcdef"},
        {MC_NBT_LONG, -2, "040000fffffffffffffffe"},
    };

    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        uint8_t bytes[16];
        size_t length = decode_hex(fixtures[i].hex, bytes);
        size_t consumed = SIZE_MAX;
        mc_nbt_document *document = NULL;
        mc_nbt_tag constructed = {0};
        constructed.type = fixtures[i].type;
        switch (constructed.type) {
            case MC_NBT_BYTE:
                constructed.value.byte_value = (int8_t)fixtures[i].value;
                break;
            case MC_NBT_SHORT:
                constructed.value.short_value = (int16_t)fixtures[i].value;
                break;
            case MC_NBT_INT:
                constructed.value.int_value = (int32_t)fixtures[i].value;
                break;
            case MC_NBT_LONG:
                constructed.value.long_value = fixtures[i].value;
                break;
            default:
                abort();
        }
        CHECK(expect_write(&constructed, fixtures[i].hex) == 0);
        CHECK(mc_nbt_read(bytes, length, NULL, &document, &consumed, NULL) == MC_NBT_OK);
        const mc_nbt_tag *tag = mc_nbt_document_root(document);
        CHECK(consumed == length && tag->type == fixtures[i].type && tag->name.length == 0);
        switch (tag->type) {
            case MC_NBT_BYTE:
                CHECK(tag->value.byte_value == fixtures[i].value);
                break;
            case MC_NBT_SHORT:
                CHECK(tag->value.short_value == fixtures[i].value);
                break;
            case MC_NBT_INT:
                CHECK(tag->value.int_value == fixtures[i].value);
                break;
            case MC_NBT_LONG:
                CHECK(tag->value.long_value == fixtures[i].value);
                break;
            default:
                abort();
        }
        CHECK(expect_write(tag, fixtures[i].hex) == 0);
        mc_nbt_document_free(document);
    }
    return 0;
}

static int test_float_bits(void) {
    static const struct {
        uint32_t bits;
        const char *input;
        const char *output;
    } floats[] = {
        {UINT32_C(0x00000000), "05000000000000", "05000000000000"},
        {UINT32_C(0x80000000), "05000080000000", "05000080000000"},
        {UINT32_C(0x00000001), "05000000000001", "05000000000001"},
        {UINT32_C(0x3f800000), "0500003f800000", "0500003f800000"},
        {UINT32_C(0x7f800000), "0500007f800000", "0500007f800000"},
        {UINT32_C(0xff800000), "050000ff800000", "050000ff800000"},
        {UINT32_C(0x7fc12345), "0500007fc12345", "0500007fc00000"},
        {UINT32_C(0xff800001), "050000ff800001", "0500007fc00000"},
    };

    static const struct {
        uint64_t bits;
        const char *input;
        const char *output;
    } doubles[] = {
        {UINT64_C(0), "0600000000000000000000", "0600000000000000000000"},
        {UINT64_C(0x8000000000000000), "0600008000000000000000", "0600008000000000000000"},
        {UINT64_C(1), "0600000000000000000001", "0600000000000000000001"},
        {UINT64_C(0x3ff0000000000000), "0600003ff0000000000000", "0600003ff0000000000000"},
        {UINT64_C(0x7ff0000000000000), "0600007ff0000000000000", "0600007ff0000000000000"},
        {UINT64_C(0xfff0000000000000), "060000fff0000000000000", "060000fff0000000000000"},
        {UINT64_C(0x7ff123456789abcd), "0600007ff123456789abcd", "0600007ff8000000000000"},
        {UINT64_C(0xfff8000000000001), "060000fff8000000000001", "0600007ff8000000000000"},
    };

    for (size_t i = 0; i < sizeof(floats) / sizeof(floats[0]); ++i) {
        uint8_t bytes[16];
        mc_nbt_document *document = NULL;
        size_t length = decode_hex(floats[i].input, bytes);
        mc_nbt_status status = mc_nbt_read(bytes, length, NULL, &document, NULL, NULL);
        if (status != MC_NBT_OK)
            fprintf(stderr, "float fixture %zu (%s): %s\n", i, floats[i].input,
                    mc_nbt_status_string(status));
        CHECK(status == MC_NBT_OK);
        const mc_nbt_tag *tag = mc_nbt_document_root(document);
        CHECK(tag->type == MC_NBT_FLOAT && tag->value.float_bits == floats[i].bits);
        CHECK(expect_write(tag, floats[i].output) == 0);
        mc_nbt_document_free(document);
    }
    for (size_t i = 0; i < sizeof(doubles) / sizeof(doubles[0]); ++i) {
        uint8_t bytes[16];
        mc_nbt_document *document = NULL;
        size_t length = decode_hex(doubles[i].input, bytes);
        CHECK(mc_nbt_read(bytes, length, NULL, &document, NULL, NULL) == MC_NBT_OK);
        const mc_nbt_tag *tag = mc_nbt_document_root(document);
        CHECK(tag->type == MC_NBT_DOUBLE && tag->value.double_bits == doubles[i].bits);
        CHECK(expect_write(tag, doubles[i].output) == 0);
        mc_nbt_document_free(document);
    }
    return 0;
}

static int test_text_and_names(void) {
    static const uint16_t units[] = {0,      1,      0x7f,   0x80,   0x7ff, 0x800,
                                     0xffff, 0xd800, 0xdc00, 0xd83d, 0xde00};
    static const char string_hex[] =
        "080000001ac080017fc280dfbfe0a080efbfbfeda080edb080eda0bdedb880";
    static const char name_hex[] =
        "03001ac080017fc280dfbfe0a080efbfbfeda080edb080eda0bdedb88012345678";
    uint8_t bytes[128];
    mc_nbt_document *document = NULL;
    mc_nbt_tag tag = {0};
    tag.type = MC_NBT_STRING;
    tag.value.string.units = units;
    tag.value.string.length = sizeof(units) / sizeof(units[0]);
    CHECK(expect_write(&tag, string_hex) == 0);
    size_t length = decode_hex(string_hex, bytes);
    CHECK(mc_nbt_read(bytes, length, NULL, &document, NULL, NULL) == MC_NBT_OK);
    const mc_nbt_tag *root = mc_nbt_document_root(document);
    CHECK(root->value.string.length == sizeof(units) / sizeof(units[0]));
    CHECK(memcmp(root->value.string.units, units, sizeof(units)) == 0);
    mc_nbt_document_free(document);
    document = NULL;
    tag.type = MC_NBT_INT;
    tag.name.units = units;
    tag.name.length = sizeof(units) / sizeof(units[0]);
    tag.value.int_value = INT32_C(0x12345678);
    CHECK(expect_write(&tag, name_hex) == 0);
    length = decode_hex(name_hex, bytes);
    CHECK(mc_nbt_read(bytes, length, NULL, &document, NULL, NULL) == MC_NBT_OK);
    root = mc_nbt_document_root(document);
    CHECK(root->name.length == sizeof(units) / sizeof(units[0]));
    CHECK(memcmp(root->name.units, units, sizeof(units)) == 0);
    CHECK(root->value.int_value == INT32_C(0x12345678));
    mc_nbt_document_free(document);
    tag = (mc_nbt_tag){0};
    tag.type = MC_NBT_STRING;
    CHECK(expect_write(&tag, "0800000000") == 0);
    tag.type = MC_NBT_END;
    tag.name.units = units;
    tag.name.length = sizeof(units) / sizeof(units[0]);
    CHECK(expect_write(&tag, "00") == 0);

    static const struct {
        const char *input;
        const char *output;
        uint16_t unit;
    } accepted[] = {
        {"080000000100", "0800000002c080", 0},      {"0800000002c080", "0800000002c080", 0},
        {"0800000002c181", "080000000141", 0x41},   {"0800000003e08080", "0800000002c080", 0},
        {"0800000003e08181", "080000000141", 0x41},
    };

    for (size_t i = 0; i < sizeof(accepted) / sizeof(accepted[0]); ++i) {
        document = NULL;
        length = decode_hex(accepted[i].input, bytes);
        CHECK(mc_nbt_read(bytes, length, NULL, &document, NULL, NULL) == MC_NBT_OK);
        root = mc_nbt_document_root(document);
        CHECK(root->value.string.length == 1 && root->value.string.units[0] == accepted[i].unit);
        CHECK(expect_write(root, accepted[i].output) == 0);
        mc_nbt_document_free(document);
    }
    return 0;
}

static int test_text_boundaries(void) {
    const size_t max_length = 65535;
    uint16_t *units = malloc((max_length + 1u) * sizeof(*units));
    uint8_t *bytes = malloc(max_length + 8u);
    uint8_t *written_bytes = malloc(max_length + 8u);
    mc_nbt_tag tag = {0};
    mc_nbt_document *document = NULL;
    size_t measured = 0;
    size_t written = 0;
    size_t consumed = 0;
    CHECK(units != NULL && bytes != NULL && written_bytes != NULL);
    for (size_t i = 0; i <= max_length; ++i)
        units[i] = 'a';
    tag.type = MC_NBT_STRING;
    tag.value.string = (mc_nbt_string){units, max_length};
    CHECK(mc_nbt_measure(&tag, NULL, &measured, NULL) == MC_NBT_OK);
    CHECK(measured == max_length + 5u);
    CHECK(mc_nbt_write(&tag, NULL, bytes, measured, &written, NULL) == MC_NBT_OK);
    CHECK(written == measured && bytes[0] == 8 && bytes[1] == 0 && bytes[2] == 0);
    CHECK(bytes[3] == 0xff && bytes[4] == 0xff);
    for (size_t i = 5; i < written; ++i)
        CHECK(bytes[i] == 'a');
    CHECK(mc_nbt_read(bytes, written, NULL, &document, &consumed, NULL) == MC_NBT_OK);
    CHECK(consumed == written);
    CHECK(mc_nbt_document_root(document)->value.string.length == max_length);
    CHECK(memcmp(mc_nbt_document_root(document)->value.string.units, units,
                 max_length * sizeof(*units)) == 0);
    CHECK(mc_nbt_write(mc_nbt_document_root(document), NULL, written_bytes, measured, &written,
                       NULL) == MC_NBT_OK);
    CHECK(memcmp(bytes, written_bytes, written) == 0);
    mc_nbt_document_free(document);
    document = NULL;
    tag.value.string.length = max_length + 1u;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_LENGTH) == 0);
    tag.type = MC_NBT_BYTE;
    tag.name = (mc_nbt_string){units, max_length};
    tag.value.byte_value = 42;
    CHECK(mc_nbt_measure(&tag, NULL, &measured, NULL) == MC_NBT_OK);
    CHECK(measured == max_length + 4u);
    CHECK(mc_nbt_write(&tag, NULL, bytes, measured, &written, NULL) == MC_NBT_OK);
    CHECK(bytes[0] == 1 && bytes[1] == 0xff && bytes[2] == 0xff);
    CHECK(bytes[written - 1u] == 42);
    CHECK(mc_nbt_read(bytes, written, NULL, &document, NULL, NULL) == MC_NBT_OK);
    CHECK(mc_nbt_document_root(document)->name.length == max_length);
    CHECK(memcmp(mc_nbt_document_root(document)->name.units, units, max_length * sizeof(*units)) ==
          0);
    mc_nbt_document_free(document);
    tag.name.length = max_length + 1u;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_LENGTH) == 0);
    for (size_t i = 0; i < 32768; ++i)
        units[i] = 0;
    tag = (mc_nbt_tag){0};
    tag.type = MC_NBT_STRING;
    tag.value.string = (mc_nbt_string){units, 32768};
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_LENGTH) == 0);
    free(written_bytes);
    free(bytes);
    free(units);
    return 0;
}

static int test_arrays_and_lists(void) {
    static const char *const fixtures[] = {
        "07000000000000",
        "07000000000004007f80ff",
        "0900000100000002ff7f",
        "0900000000000002",
        "090000090000000201000000018002000000010123",
        "0900000a00000002010001780100010001780200",
    };
    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        uint8_t bytes[128];
        size_t length = decode_hex(fixtures[i], bytes);
        size_t consumed = SIZE_MAX;
        mc_nbt_document *document = NULL;
        CHECK(mc_nbt_read(bytes, length, NULL, &document, &consumed, NULL) == MC_NBT_OK);
        CHECK(consumed == length);
        const mc_nbt_tag *root = mc_nbt_document_root(document);
        CHECK(expect_write(root, fixtures[i]) == 0);
        if (i == 1) {
            static const uint8_t expected[] = {0, 0x7f, 0x80, 0xff};
            CHECK(root->value.byte_array.length == sizeof(expected));
            CHECK(memcmp(root->value.byte_array.data, expected, sizeof(expected)) == 0);
        } else if (i == 2) {
            CHECK(root->value.list.length == 2 && root->value.list.element_type == MC_NBT_BYTE);
            CHECK(root->value.list.items[0]->value.byte_value == -1);
            CHECK(root->value.list.items[1]->value.byte_value == 127);
            CHECK(root->value.list.items[0]->name.length == 0);
        } else if (i == 3) {
            CHECK(root->value.list.length == 2 && root->value.list.element_type == MC_NBT_END);
            CHECK(root->value.list.items[0]->type == MC_NBT_END);
            CHECK(root->value.list.items[1]->type == MC_NBT_END);
        } else if (i == 4) {
            CHECK(root->value.list.items[0]->value.list.items[0]->value.byte_value == INT8_MIN);
            CHECK(root->value.list.items[1]->value.list.items[0]->value.short_value == 0x123);
        } else if (i == 5) {
            static const uint16_t x[] = {'x'};
            CHECK(mc_nbt_compound_get(root->value.list.items[0], x, 1)->value.byte_value == 1);
            CHECK(mc_nbt_compound_get(root->value.list.items[1], x, 1)->value.byte_value == 2);
        }
        mc_nbt_document_free(document);
    }
    for (unsigned int type = 0; type <= 255; ++type) {
        uint8_t bytes[] = {9, 0, 0, 0, 0, 0, 0, 0};
        bytes[3] = (uint8_t)type;
        mc_nbt_document *document = NULL;
        CHECK(mc_nbt_read(bytes, sizeof(bytes), NULL, &document, NULL, NULL) == MC_NBT_OK);
        CHECK(mc_nbt_document_root(document)->value.list.length == 0);
        CHECK(expect_write(mc_nbt_document_root(document), "0900000100000000") == 0);
        mc_nbt_document_free(document);
    }
    mc_nbt_tag child = {0};
    mc_nbt_tag list = {0};
    const mc_nbt_tag *items[] = {&child};
    child.type = MC_NBT_BYTE;
    child.name = (mc_nbt_string){NULL, 123};
    child.value.byte_value = -1;
    list.type = MC_NBT_LIST;
    list.value.list.element_type = MC_NBT_BYTE;
    list.value.list.items = items;
    list.value.list.length = 1;
    CHECK(expect_write(&list, "0900000100000001ff") == 0);
    return 0;
}

static int test_compounds_and_ownership(void) {
    static const char input_hex[] =
        "0a000172010001780103000178123456780800066675747572650001760100000700deadbeef";
    static const uint16_t x[] = {'x'};
    static const uint16_t future[] = {'f', 'u', 't', 'u', 'r', 'e'};
    uint8_t bytes[128];
    size_t length = decode_hex(input_hex, bytes);
    size_t consumed = 0;
    mc_nbt_document *document = NULL;
    CHECK(mc_nbt_read_compound(bytes, length, NULL, &document, &consumed, NULL) == MC_NBT_OK);
    CHECK(consumed == length - 4u);
    const mc_nbt_tag *root = mc_nbt_document_root(document);
    CHECK(root->type == MC_NBT_COMPOUND && root->name.length == 1 && root->name.units[0] == 'r');
    CHECK(root->value.compound.length == 3);
    const mc_nbt_tag *found = mc_nbt_compound_get(root, x, 1);
    CHECK(found != NULL && found->type == MC_NBT_INT);
    CHECK(found->value.int_value == INT32_C(0x12345678));
    found = mc_nbt_compound_get(root, future, sizeof(future) / sizeof(future[0]));
    CHECK(found != NULL && found->type == MC_NBT_STRING);
    CHECK(found->value.string.length == 1 && found->value.string.units[0] == 'v');
    CHECK(mc_nbt_compound_get(root, NULL, 0)->value.byte_value == 7);
    CHECK(mc_nbt_compound_get(root, NULL, 1) == NULL);
    CHECK(mc_nbt_compound_get(root, x, 0)->value.byte_value == 7);
    CHECK(mc_nbt_compound_get(found, x, 1) == NULL);
    CHECK(mc_nbt_compound_get(NULL, NULL, 0) == NULL);
    memset(bytes, 0xcc, sizeof(bytes));
    CHECK(root->name.units[0] == 'r');
    CHECK(mc_nbt_compound_get(root, x, 1)->value.int_value == INT32_C(0x12345678));
    CHECK(found->value.string.units[0] == 'v');
    /* Re-reading emitted bytes tests retained fields without imposing HashMap order. */
    size_t written = 0;
    CHECK(mc_nbt_write(root, NULL, bytes, sizeof(bytes), &written, NULL) == MC_NBT_OK);
    mc_nbt_document *second = NULL;
    CHECK(mc_nbt_read_compound(bytes, written, NULL, &second, NULL, NULL) == MC_NBT_OK);
    const mc_nbt_tag *second_root = mc_nbt_document_root(second);
    CHECK(second_root->value.compound.length == 3);
    CHECK(mc_nbt_compound_get(second_root, x, 1)->value.int_value == INT32_C(0x12345678));
    CHECK(mc_nbt_compound_get(second_root, future, 6)->value.string.units[0] == 'v');
    mc_nbt_document_free(second);
    mc_nbt_document_free(document);
    static const uint8_t empty_compound[] = {10, 0, 0, 0};
    document = NULL;
    CHECK(mc_nbt_read_compound(empty_compound, sizeof(empty_compound), NULL, &document, NULL,
                               NULL) == MC_NBT_OK);
    CHECK(mc_nbt_document_root(document)->value.compound.length == 0);
    CHECK(expect_write(mc_nbt_document_root(document), "0a000000") == 0);
    mc_nbt_document_free(document);
    return 0;
}

static int test_root_modes_and_arguments(void) {
    static const uint8_t end_with_tail[] = {0, 0xff, 0xff};
    static const uint8_t byte[] = {1, 0, 0, 1};
    mc_nbt_document *document = NULL;
    size_t consumed = 0;
    CHECK(mc_nbt_read(end_with_tail, sizeof(end_with_tail), NULL, &document, &consumed, NULL) ==
          MC_NBT_OK);
    CHECK(consumed == 1 && mc_nbt_document_root(document)->type == MC_NBT_END);
    mc_nbt_document *saved = document;
    mc_nbt_error error = {MC_NBT_OK, SIZE_MAX};
    consumed = 123;
    CHECK(mc_nbt_read_compound(byte, sizeof(byte), NULL, &document, &consumed, &error) ==
          MC_NBT_ROOT_TYPE);
    CHECK(document == saved && consumed == 123 && error.status == MC_NBT_ROOT_TYPE);
    CHECK(mc_nbt_read_compound(end_with_tail, sizeof(end_with_tail), NULL, &document, &consumed,
                               NULL) == MC_NBT_ROOT_TYPE);
    CHECK(document == saved && consumed == 123);
    CHECK(mc_nbt_read(byte, sizeof(byte), NULL, NULL, &consumed, NULL) == MC_NBT_INVALID_ARGUMENT);
    CHECK(mc_nbt_read(NULL, 1, NULL, &document, &consumed, NULL) == MC_NBT_INVALID_ARGUMENT);
    CHECK(document == saved && consumed == 123);
    mc_nbt_tag tag = {0};
    tag.type = MC_NBT_BYTE;
    uint8_t output[8];
    memset(output, 0xa5, sizeof(output));
    size_t written = 123;
    CHECK(mc_nbt_measure(NULL, NULL, &written, NULL) == MC_NBT_INVALID_ARGUMENT);
    CHECK(written == 123);
    CHECK(mc_nbt_measure(&tag, NULL, NULL, NULL) == MC_NBT_INVALID_ARGUMENT);
    CHECK(mc_nbt_write(NULL, NULL, output, sizeof(output), &written, NULL) ==
          MC_NBT_INVALID_ARGUMENT);
    CHECK(written == 123 && output[0] == 0xa5);
    CHECK(mc_nbt_document_root(NULL) == NULL);
    mc_nbt_document_free(NULL);
    mc_nbt_document_free(document);
    for (int status = MC_NBT_OK; status <= MC_NBT_ROOT_TYPE; ++status)
        CHECK(mc_nbt_status_string((mc_nbt_status)status) != NULL);
    CHECK(mc_nbt_status_string((mc_nbt_status)999) != NULL);
    return 0;
}

static int test_malformed_and_truncated(void) {
    static const struct {
        const char *hex;
        mc_nbt_status status;
    } malformed[] = {
        {"0b", MC_NBT_INVALID_TYPE},
        {"0c0000", MC_NBT_INVALID_TYPE},
        {"ff0000", MC_NBT_INVALID_TYPE},
        {"070000ffffffff", MC_NBT_INVALID_LENGTH},
        {"09000001ffffffff", MC_NBT_INVALID_LENGTH},
        {"0900000b00000001", MC_NBT_INVALID_TYPE},
        {"0a00000b", MC_NBT_INVALID_TYPE},
        {"080000000180", MC_NBT_INVALID_UTF},
        {"0800000002c241", MC_NBT_INVALID_UTF},
        {"0800000003e08041", MC_NBT_INVALID_UTF},
        {"0800000004f0908080", MC_NBT_INVALID_UTF},
        {"0100018000", MC_NBT_INVALID_UTF},
    };

    for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
        uint8_t bytes[64];
        size_t length = decode_hex(malformed[i].hex, bytes);
        CHECK(expect_read_failure(bytes, length, NULL, malformed[i].status) == 0);
    }
    static const char *const complete[] = {
        "00",
        "0100017880",
        "0200008123",
        "03000012345678",
        "0400000123456789abcdef",
        "050000ff800001",
        "0600007ff123456789abcd",
        "07000000000004007f80ff",
        "080002c0800006eda0bdedb880",
        "090000090000000201000000018002000000010123",
        "0a000109080001730002c0800a00016e01000178800000",
    };
    for (size_t i = 0; i < sizeof(complete) / sizeof(complete[0]); ++i) {
        uint8_t bytes[128];
        size_t length = decode_hex(complete[i], bytes);
        mc_nbt_document *document = NULL;
        size_t consumed = 0;
        CHECK(mc_nbt_read(bytes, length, NULL, &document, &consumed, NULL) == MC_NBT_OK);
        CHECK(consumed == length);
        mc_nbt_document_free(document);
        for (size_t cut = 0; cut < length; ++cut)
            CHECK(expect_read_failure(bytes, cut, NULL, MC_NBT_TRUNCATED) == 0);
    }
    return 0;
}

static int test_limits(void) {
    static const uint8_t byte[] = {1, 0, 0, 42};
    static const uint8_t list[] = {9, 0, 0, 1, 0, 0, 0, 2, 11, 12};
    static const uint8_t array[] = {7, 0, 0, 0, 0, 0, 3, 1, 2, 3};
    mc_nbt_limits limits = mc_nbt_default_limits();
    mc_nbt_limits defaults = limits;
    mc_nbt_document *document = NULL;
    CHECK(defaults.max_depth >= 1 && defaults.max_depth <= 128);
    CHECK(defaults.max_input_bytes > 0 && defaults.max_output_bytes > 0);
    CHECK(defaults.max_tags > 0 && defaults.max_elements > 0 && defaults.max_allocation_bytes > 0);
    limits.max_input_bytes = sizeof(byte) - 1u;
    CHECK(expect_read_failure(byte, sizeof(byte), &limits, MC_NBT_LIMIT) == 0);
    limits.max_input_bytes = sizeof(byte);
    CHECK(mc_nbt_read(byte, sizeof(byte), &limits, &document, NULL, NULL) == MC_NBT_OK);
    mc_nbt_tag borrowed = *mc_nbt_document_root(document);
    mc_nbt_document_free(document);
    limits = defaults;
    limits.max_output_bytes = sizeof(byte) - 1u;
    CHECK(expect_write_failure(&borrowed, &limits, MC_NBT_LIMIT) == 0);
    limits.max_output_bytes = sizeof(byte);
    size_t measured = 0;
    CHECK(mc_nbt_measure(&borrowed, &limits, &measured, NULL) == MC_NBT_OK && measured == 4);
    limits = defaults;
    limits.max_allocation_bytes = 1;
    CHECK(expect_read_failure(byte, sizeof(byte), &limits, MC_NBT_LIMIT) == 0);
    limits.max_allocation_bytes = 0;
    CHECK(expect_read_failure(byte, sizeof(byte), &limits, MC_NBT_LIMIT) == 0);
    limits = defaults;
    limits.max_tags = 0;
    CHECK(expect_read_failure(byte, sizeof(byte), &limits, MC_NBT_LIMIT) == 0);
    CHECK(expect_write_failure(&borrowed, &limits, MC_NBT_LIMIT) == 0);
    limits.max_tags = 2;
    CHECK(expect_read_failure(list, sizeof(list), &limits, MC_NBT_LIMIT) == 0);
    limits.max_tags = 3;
    document = NULL;
    CHECK(mc_nbt_read(list, sizeof(list), &limits, &document, NULL, NULL) == MC_NBT_OK);
    const mc_nbt_tag *root = mc_nbt_document_root(document);
    CHECK(mc_nbt_measure(root, &limits, &measured, NULL) == MC_NBT_OK);
    limits.max_tags = 2;
    CHECK(expect_write_failure(root, &limits, MC_NBT_LIMIT) == 0);
    limits = defaults;
    limits.max_elements = 1;
    CHECK(expect_read_failure(list, sizeof(list), &limits, MC_NBT_LIMIT) == 0);
    CHECK(expect_write_failure(root, &limits, MC_NBT_LIMIT) == 0);
    limits.max_elements = 2;
    CHECK(mc_nbt_measure(root, &limits, &measured, NULL) == MC_NBT_OK);
    mc_nbt_document_free(document);
    document = NULL;
    CHECK(mc_nbt_read(list, sizeof(list), &limits, &document, NULL, NULL) == MC_NBT_OK);
    mc_nbt_document_free(document);
    CHECK(expect_read_failure(array, sizeof(array), &limits, MC_NBT_LIMIT) == 0);
    limits.max_elements = 3;
    document = NULL;
    CHECK(mc_nbt_read(array, sizeof(array), &limits, &document, NULL, NULL) == MC_NBT_OK);
    limits.max_elements = 2;
    CHECK(expect_write_failure(mc_nbt_document_root(document), &limits, MC_NBT_LIMIT) == 0);
    mc_nbt_document_free(document);
    static const uint8_t duplicates[] = {10, 0, 0, 1, 0, 1, 'x', 1, 1, 0, 1, 'x', 2, 0};
    limits = defaults;
    limits.max_elements = 1;
    CHECK(expect_read_failure(duplicates, sizeof(duplicates), &limits, MC_NBT_LIMIT) == 0);
    limits = defaults;
    limits.max_tags = 2;
    CHECK(expect_read_failure(duplicates, sizeof(duplicates), &limits, MC_NBT_LIMIT) == 0);
    limits.max_tags = 3;
    document = NULL;
    CHECK(mc_nbt_read(duplicates, sizeof(duplicates), &limits, &document, NULL, NULL) == MC_NBT_OK);
    CHECK(mc_nbt_document_root(document)->value.compound.length == 1);
    mc_nbt_document_free(document);
    /* A large declared count must fail before allocation or payload traversal. */
    static const uint8_t large_array[] = {7, 0, 0, 0x7f, 0xff, 0xff, 0xff};
    static const uint8_t large_list[] = {9, 0, 0, 1, 0x7f, 0xff, 0xff, 0xff};
    CHECK(expect_read_failure(large_array, sizeof(large_array), NULL, MC_NBT_LIMIT) == 0);
    CHECK(expect_read_failure(large_list, sizeof(large_list), NULL, MC_NBT_LIMIT) == 0);
    /* Element bounds apply to each container, not the sum of all containers. */
    static const uint8_t nested[] = {9, 0, 0, 9, 0, 0, 0, 2, 1, 0, 0,
                                     0, 2, 1, 2, 1, 0, 0, 0, 2, 3, 4};
    limits = defaults;
    limits.max_elements = 2;
    document = NULL;
    CHECK(mc_nbt_read(nested, sizeof(nested), &limits, &document, NULL, NULL) == MC_NBT_OK);
    CHECK(mc_nbt_measure(mc_nbt_document_root(document), &limits, &measured, NULL) == MC_NBT_OK);
    mc_nbt_document_free(document);
    /* UTF-16 code units are governed by their encoded length, not container limits. */
    static const uint8_t text[] = {8, 0, 0, 0, 3, 'a', 'b', 'c'};
    limits.max_elements = 0;
    document = NULL;
    CHECK(mc_nbt_read(text, sizeof(text), &limits, &document, NULL, NULL) == MC_NBT_OK);
    CHECK(mc_nbt_measure(mc_nbt_document_root(document), &limits, &measured, NULL) == MC_NBT_OK);
    mc_nbt_document_free(document);
    static const uint16_t name_x[] = {'x'};
    static const uint16_t name_y[] = {'y'};
    mc_nbt_tag x = {0};
    mc_nbt_tag y = {0};
    mc_nbt_tag compound = {0};
    const mc_nbt_tag *entries[] = {&x, &y};
    x.type = MC_NBT_BYTE;
    x.name = (mc_nbt_string){name_x, 1};
    y.type = MC_NBT_BYTE;
    y.name = (mc_nbt_string){name_y, 1};
    compound.type = MC_NBT_COMPOUND;
    compound.value.compound.items = entries;
    compound.value.compound.length = 2;
    limits = defaults;
    limits.max_elements = 1;
    CHECK(expect_write_failure(&compound, &limits, MC_NBT_LIMIT) == 0);
    limits.max_elements = 2;
    CHECK(mc_nbt_measure(&compound, &limits, &measured, NULL) == MC_NBT_OK);
    limits.max_allocation_bytes = 0;
    CHECK(expect_write_failure(&compound, &limits, MC_NBT_LIMIT) == 0);
    /* Scalars require no writer scratch allocation. */
    CHECK(mc_nbt_measure(&borrowed, &limits, &measured, NULL) == MC_NBT_OK);
    return 0;
}

static int test_depth(void) {
    mc_nbt_tag tags[129] = {0};
    const mc_nbt_tag *links[128];
    uint8_t bytes[1024];
    size_t written = 0;
    mc_nbt_limits limits = mc_nbt_default_limits();
    for (size_t i = 0; i < 128; ++i) {
        links[i] = &tags[i + 1u];
        tags[i].type = MC_NBT_LIST;
        tags[i].value.list.element_type = i == 127 ? MC_NBT_BYTE : MC_NBT_LIST;
        tags[i].value.list.items = &links[i];
        tags[i].value.list.length = 1;
    }
    tags[128].type = MC_NBT_BYTE;
    tags[128].value.byte_value = 42;
    limits.max_depth = 128;
    CHECK(expect_write_failure(&tags[0], &limits, MC_NBT_LIMIT) == 0);
    CHECK(mc_nbt_write(&tags[1], &limits, bytes, sizeof(bytes), &written, NULL) == MC_NBT_OK);
    mc_nbt_document *document = NULL;
    CHECK(mc_nbt_read(bytes, written, &limits, &document, NULL, NULL) == MC_NBT_OK);
    const mc_nbt_tag *root = mc_nbt_document_root(document);
    for (size_t i = 0; i < 127; ++i) {
        CHECK(root->type == MC_NBT_LIST && root->value.list.length == 1);
        root = root->value.list.items[0];
    }
    CHECK(root->type == MC_NBT_BYTE && root->value.byte_value == 42);
    mc_nbt_document_free(document);
    limits.max_depth = 127;
    CHECK(expect_read_failure(bytes, written, &limits, MC_NBT_LIMIT) == 0);
    limits.max_depth = 1;
    CHECK(mc_nbt_write(&tags[128], &limits, bytes, sizeof(bytes), &written, NULL) == MC_NBT_OK);
    CHECK(expect_write_failure(&tags[127], &limits, MC_NBT_LIMIT) == 0);
    limits.max_depth = 0;
    CHECK(expect_write_failure(&tags[128], &limits, MC_NBT_INVALID_ARGUMENT) == 0);
    CHECK(expect_read_failure(bytes, written, &limits, MC_NBT_INVALID_ARGUMENT) == 0);
    limits.max_depth = 129;
    CHECK(expect_write_failure(&tags[128], &limits, MC_NBT_INVALID_ARGUMENT) == 0);
    CHECK(expect_read_failure(bytes, written, &limits, MC_NBT_INVALID_ARGUMENT) == 0);
    return 0;
}

static int test_writer_validation_and_capacity(void) {
    mc_nbt_tag tag = {0};
    tag.type = MC_NBT_INT;
    tag.value.int_value = 42;
    for (size_t capacity = 0; capacity < 7; ++capacity) {
        uint8_t output[16];
        uint8_t saved[16];
        size_t written = 123;
        mc_nbt_error error;
        memset(output, 0xa5, sizeof(output));
        memcpy(saved, output, sizeof(saved));
        CHECK(mc_nbt_write(&tag, NULL, output, capacity, &written, &error) ==
              MC_NBT_BUFFER_TOO_SMALL);
        CHECK(written == 123 && error.status == MC_NBT_BUFFER_TOO_SMALL);
        CHECK(memcmp(output, saved, sizeof(output)) == 0);
    }
    CHECK(expect_write(&tag, "0300000000002a") == 0);
    tag.type = (mc_nbt_type)11;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TYPE) == 0);
    tag.type = MC_NBT_BYTE;
    tag.name = (mc_nbt_string){NULL, 1};
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    tag.name.length = 0;
    tag.type = MC_NBT_STRING;
    tag.value.string = (mc_nbt_string){NULL, 1};
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    tag.type = MC_NBT_BYTE_ARRAY;
    tag.value.byte_array.data = NULL;
    tag.value.byte_array.length = 1;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    tag.type = MC_NBT_LIST;
    tag.value.list.element_type = MC_NBT_BYTE;
    tag.value.list.items = NULL;
    tag.value.list.length = 1;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    mc_nbt_tag a = {0};
    mc_nbt_tag b = {0};
    a.type = MC_NBT_BYTE;
    b.type = MC_NBT_SHORT;
    const mc_nbt_tag *items[] = {&a, &b};
    tag.value.list.items = items;
    tag.value.list.length = 2;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    tag.value.list.element_type = 11;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TYPE) == 0);
    tag.value.list.element_type = MC_NBT_BYTE;
    items[1] = NULL;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    items[1] = &b;
    tag.type = MC_NBT_COMPOUND;
    tag.value.compound.items = items;
    tag.value.compound.length = 2;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    b.type = MC_NBT_END;
    tag.value.compound.length = 1;
    items[0] = &b;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    tag.value.compound.items = NULL;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    /* A list cycle cannot become a valid serialized tree. */
    tag.type = MC_NBT_LIST;
    tag.value.list.element_type = MC_NBT_LIST;
    tag.value.list.items = items;
    tag.value.list.length = 1;
    items[0] = &tag;
    CHECK(expect_write_failure(&tag, NULL, MC_NBT_INVALID_TREE) == 0);
    /* Reusing an acyclic value in a list remains valid. */
    items[0] = &a;
    items[1] = &a;
    tag.value.list.element_type = MC_NBT_BYTE;
    tag.value.list.length = 2;
    CHECK(expect_write(&tag, "09000001000000020000") == 0);
    return 0;
}

static int test_bounded_mutations(void) {
    static const char *const seeds[] = {
        "00",
        "0100017880",
        "07000000000004007f80ff",
        "0800000006eda0bdedb880",
        "0900000100000002ff7f",
        "0900000000000002",
        "090000090000000201000000018002000000010123",
        "0a00000100017801080001730002c08000",
    };
    static const uint8_t masks[] = {1, 0x80, 0xff};
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_input_bytes = 128;
    limits.max_output_bytes = 1024;
    limits.max_allocation_bytes = 32768;
    limits.max_tags = 64;
    limits.max_elements = 64;
    limits.max_depth = 16;
    for (size_t seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
        uint8_t original[128];
        size_t length = decode_hex(seeds[seed], original);
        for (size_t index = 0; index < length; ++index) {
            for (size_t mutation = 0; mutation < sizeof(masks) / sizeof(masks[0]); ++mutation) {
                uint8_t mutated[128];
                uint8_t emitted[1024];
                mc_nbt_document *document = NULL;
                size_t consumed = SIZE_MAX;
                mc_nbt_error error = {MC_NBT_OK, SIZE_MAX};
                memcpy(mutated, original, length);
                mutated[index] ^= masks[mutation];
                mc_nbt_status status =
                    mc_nbt_read(mutated, length, &limits, &document, &consumed, &error);
                CHECK(error.status == status && error.offset <= length);
                if (status == MC_NBT_OK) {
                    size_t measured = 0;
                    size_t written = 0;
                    const mc_nbt_tag *root = mc_nbt_document_root(document);
                    CHECK(root != NULL && consumed <= length);
                    CHECK(mc_nbt_measure(root, &limits, &measured, NULL) == MC_NBT_OK);
                    CHECK(measured <= sizeof(emitted));
                    CHECK(mc_nbt_write(root, &limits, emitted, sizeof(emitted), &written, NULL) ==
                          MC_NBT_OK);
                    CHECK(written == measured);
                    mc_nbt_document *reparsed = NULL;
                    size_t reread = 0;
                    CHECK(mc_nbt_read(emitted, written, &limits, &reparsed, &reread, NULL) ==
                          MC_NBT_OK);
                    CHECK(reread == written);
                    uint8_t stable[1024];
                    size_t stable_length = 0;
                    CHECK(mc_nbt_write(mc_nbt_document_root(reparsed), &limits, stable,
                                       sizeof(stable), &stable_length, NULL) == MC_NBT_OK);
                    CHECK(stable_length == written && memcmp(stable, emitted, written) == 0);
                    mc_nbt_document_free(reparsed);
                } else {
                    CHECK(document == NULL && consumed == SIZE_MAX);
                }
                mc_nbt_document_free(document);
            }
        }
    }
    return 0;
}

int main(void) {
    if (test_signed_scalars() || test_float_bits() || test_text_and_names() ||
        test_text_boundaries() || test_arrays_and_lists() || test_compounds_and_ownership() ||
        test_root_modes_and_arguments() || test_malformed_and_truncated() || test_limits() ||
        test_depth() || test_writer_validation_and_capacity() || test_bounded_mutations())
        return 1;
    printf("NBT: %u checks passed\n", checks);
    return 0;
}
