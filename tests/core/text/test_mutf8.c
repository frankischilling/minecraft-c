#include "mc/core/text/mutf8.h"

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

static int test_known_bytes(void)
{
    /* DataOutputStream.writeUTF observations, with its two-byte prefix removed. */
    const uint16_t units[] = {0,      1,      0x7f,   0x80,   0x7ff, 0x800,
                              0xffff, 0xd800, 0xdc00, 0xd83d, 0xde00};
    const uint8_t expected[] = {0xc0, 0x80, 0x01, 0x7f, 0xc2, 0x80, 0xdf, 0xbf, 0xe0,
                                0xa0, 0x80, 0xef, 0xbf, 0xbf, 0xed, 0xa0, 0x80, 0xed,
                                0xb0, 0x80, 0xed, 0xa0, 0xbd, 0xed, 0xb8, 0x80};
    uint8_t encoded[sizeof(expected) + 1];
    uint16_t decoded[sizeof(units) / sizeof(units[0]) + 1];
    size_t count = 123;
    size_t written = 456;

    memset(encoded, 0x55, sizeof(encoded));
    for (size_t index = 0; index < sizeof(decoded) / sizeof(decoded[0]); ++index) {
        decoded[index] = UINT16_C(0x5555);
    }
    CHECK(mc_mutf8_encoded_length(units, sizeof(units) / sizeof(units[0]), &count) == MC_MUTF8_OK);
    CHECK(count == sizeof(expected));
    CHECK(mc_mutf8_encode(units, sizeof(units) / sizeof(units[0]), encoded, sizeof(encoded),
                          &written) == MC_MUTF8_OK);
    CHECK(written == count);
    CHECK(memcmp(encoded, expected, sizeof(expected)) == 0);
    CHECK(encoded[sizeof(expected)] == UINT8_C(0x55));
    CHECK(mc_mutf8_decoded_length(expected, sizeof(expected), &count) == MC_MUTF8_OK);
    CHECK(count == sizeof(units) / sizeof(units[0]));
    CHECK(mc_mutf8_decode(expected, sizeof(expected), decoded, sizeof(decoded) / sizeof(decoded[0]),
                          &written) == MC_MUTF8_OK);
    CHECK(written == count);
    CHECK(memcmp(decoded, units, sizeof(units)) == 0);
    CHECK(decoded[count] == UINT16_C(0x5555));
    return 0;
}

static int test_empty_and_arguments(void)
{
    const uint8_t input[] = {0x41};
    const uint16_t units[] = {0x41};
    uint8_t encoded[] = {0x55, 0x55};
    uint16_t decoded[] = {0x5555, 0x5555};
    size_t count = 123;

    CHECK(mc_mutf8_decoded_length(NULL, 0, &count) == MC_MUTF8_OK && count == 0);
    CHECK(mc_mutf8_encoded_length(NULL, 0, &count) == MC_MUTF8_OK && count == 0);
    CHECK(mc_mutf8_decode(NULL, 0, NULL, 0, &count) == MC_MUTF8_OK && count == 0);
    CHECK(mc_mutf8_encode(NULL, 0, NULL, 0, &count) == MC_MUTF8_OK && count == 0);
    CHECK(mc_mutf8_decode(input, 0, decoded, 2, &count) == MC_MUTF8_OK && count == 0);
    CHECK(mc_mutf8_encode(units, 0, encoded, 2, &count) == MC_MUTF8_OK && count == 0);
    CHECK(decoded[0] == UINT16_C(0x5555) && decoded[1] == UINT16_C(0x5555));
    CHECK(encoded[0] == UINT8_C(0x55) && encoded[1] == UINT8_C(0x55));
    count = 123;
    CHECK(mc_mutf8_decoded_length(NULL, 1, &count) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(count == 123);
    CHECK(mc_mutf8_encoded_length(NULL, 1, &count) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(count == 123);
    CHECK(mc_mutf8_decoded_length(input, 1, NULL) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(mc_mutf8_encoded_length(units, 1, NULL) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(mc_mutf8_decode(input, 1, decoded, 2, NULL) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(mc_mutf8_encode(units, 1, encoded, 2, NULL) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(mc_mutf8_decode(NULL, 1, decoded, 2, &count) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(mc_mutf8_encode(NULL, 1, encoded, 2, &count) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(mc_mutf8_decode(input, 1, NULL, 1, &count) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(mc_mutf8_encode(units, 1, NULL, 1, &count) == MC_MUTF8_INVALID_ARGUMENT);
    CHECK(count == 123);
    CHECK(decoded[0] == UINT16_C(0x5555) && decoded[1] == UINT16_C(0x5555));
    CHECK(encoded[0] == UINT8_C(0x55) && encoded[1] == UINT8_C(0x55));
    CHECK(mc_mutf8_encoded_length(units, SIZE_MAX / sizeof(units[0]) + 1, &count) ==
          MC_MUTF8_OVERFLOW);
    CHECK(mc_mutf8_encode(units, SIZE_MAX / sizeof(units[0]) + 1, encoded, 2, &count) ==
          MC_MUTF8_OVERFLOW);
    CHECK(count == 123);
    CHECK(encoded[0] == UINT8_C(0x55) && encoded[1] == UINT8_C(0x55));
    return 0;
}

static int test_capacity(void)
{
    const uint16_t input[] = {0x41, 0, 0xd800};
    const uint8_t bytes[] = {0x41, 0xc0, 0x80, 0xed, 0xa0, 0x80};
    uint8_t encoded[sizeof(bytes)];
    uint16_t decoded[sizeof(input) / sizeof(input[0])];
    size_t written = 123;

    for (size_t capacity = 0; capacity < sizeof(bytes); ++capacity) {
        memset(encoded, 0x55, sizeof(encoded));
        CHECK(mc_mutf8_encode(input, sizeof(input) / sizeof(input[0]), encoded, capacity,
                              &written) == MC_MUTF8_BUFFER_TOO_SMALL);
        CHECK(written == 123);
        for (size_t index = 0; index < sizeof(encoded); ++index) {
            CHECK(encoded[index] == UINT8_C(0x55));
        }
    }
    for (size_t capacity = 0; capacity < sizeof(input) / sizeof(input[0]); ++capacity) {
        for (size_t index = 0; index < sizeof(decoded) / sizeof(decoded[0]); ++index) {
            decoded[index] = UINT16_C(0x5555);
        }
        CHECK(mc_mutf8_decode(bytes, sizeof(bytes), decoded, capacity, &written) ==
              MC_MUTF8_BUFFER_TOO_SMALL);
        CHECK(written == 123);
        for (size_t index = 0; index < sizeof(decoded) / sizeof(decoded[0]); ++index) {
            CHECK(decoded[index] == UINT16_C(0x5555));
        }
    }
    CHECK(mc_mutf8_encode(input, sizeof(input) / sizeof(input[0]), encoded, sizeof(encoded),
                          &written) == MC_MUTF8_OK);
    CHECK(written == sizeof(bytes) && memcmp(encoded, bytes, sizeof(bytes)) == 0);
    CHECK(mc_mutf8_decode(bytes, sizeof(bytes), decoded, sizeof(decoded) / sizeof(decoded[0]),
                          &written) == MC_MUTF8_OK);
    CHECK(written == sizeof(input) / sizeof(input[0]) &&
          memcmp(decoded, input, sizeof(input)) == 0);
    return 0;
}

static int test_malformed(void)
{
    const struct {
        uint8_t bytes[8];
        size_t length;
    } cases[] = {{{0x80}, 1},
                 {{0xbf}, 1},
                 {{0xc0}, 1},
                 {{0xc2}, 1},
                 {{0xc2, 0x41}, 2},
                 {{0xc2, 0xc0}, 2},
                 {{0xe0}, 1},
                 {{0xe0, 0x80}, 2},
                 {{0xe0, 0x41, 0x80}, 3},
                 {{0xe0, 0x80, 0x41}, 3},
                 {{0xef, 0xbf, 0xff}, 3},
                 {{0xf0, 0x90, 0x80, 0x80}, 4},
                 {{0xff}, 1},
                 {{0x41, 0xc0}, 2},
                 {{0xc0, 0x80, 0x41, 0xe0, 0x80}, 5},
                 {{0xed, 0xa0, 0x80, 0x80}, 4}};

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint16_t output[] = {0x5555, 0x5555, 0x5555, 0x5555};
        size_t count = 123;
        CHECK(mc_mutf8_decoded_length(cases[index].bytes, cases[index].length, &count) ==
              MC_MUTF8_INVALID_ENCODING);
        CHECK(count == 123);
        CHECK(mc_mutf8_decode(cases[index].bytes, cases[index].length, output,
                              sizeof(output) / sizeof(output[0]),
                              &count) == MC_MUTF8_INVALID_ENCODING);
        CHECK(count == 123);
        for (size_t unit = 0; unit < sizeof(output) / sizeof(output[0]); ++unit) {
            CHECK(output[unit] == UINT16_C(0x5555));
        }
    }
    return 0;
}

static int test_decoder_forms(void)
{
    /* readUTF accepts raw NUL and non-shortest two-/three-byte forms. */
    for (uint32_t value = 0; value <= UINT32_C(0xffff); ++value) {
        uint8_t bytes[3] = {(uint8_t)(UINT32_C(0xe0) | (value >> 12)),
                            (uint8_t)(UINT32_C(0x80) | ((value >> 6) & UINT32_C(0x3f))),
                            (uint8_t)(UINT32_C(0x80) | (value & UINT32_C(0x3f)))};
        uint16_t output = UINT16_C(0x5555);
        size_t count = 123;
        CHECK(mc_mutf8_decoded_length(bytes, sizeof(bytes), &count) == MC_MUTF8_OK && count == 1);
        CHECK(mc_mutf8_decode(bytes, sizeof(bytes), &output, 1, &count) == MC_MUTF8_OK);
        CHECK(count == 1 && output == (uint16_t)value);
    }
    for (uint32_t value = 0; value <= UINT32_C(0x7ff); ++value) {
        uint8_t bytes[2] = {(uint8_t)(UINT32_C(0xc0) | (value >> 6)),
                            (uint8_t)(UINT32_C(0x80) | (value & UINT32_C(0x3f)))};
        uint16_t output = UINT16_C(0x5555);
        size_t count = 123;
        CHECK(mc_mutf8_decode(bytes, sizeof(bytes), &output, 1, &count) == MC_MUTF8_OK);
        CHECK(count == 1 && output == (uint16_t)value);
    }
    for (uint32_t value = 0; value <= UINT32_C(0xff); ++value) {
        uint8_t byte = (uint8_t)value;
        uint16_t output = UINT16_C(0x5555);
        size_t count = 123;
        if (value <= UINT32_C(0x7f)) {
            CHECK(mc_mutf8_decode(&byte, 1, &output, 1, &count) == MC_MUTF8_OK);
            CHECK(count == 1 && output == (uint16_t)value);
        } else {
            CHECK(mc_mutf8_decode(&byte, 1, &output, 1, &count) == MC_MUTF8_INVALID_ENCODING);
            CHECK(count == 123 && output == UINT16_C(0x5555));
        }
    }
    return 0;
}

static int test_all_code_units(void)
{
    static uint16_t input[65536];
    static uint16_t output[65536];
    static uint8_t bytes[65536 * 3];
    size_t encoded;
    size_t decoded;
    size_t written;

    for (uint32_t value = 0; value <= UINT32_C(0xffff); ++value) {
        uint8_t single[3] = {0x55, 0x55, 0x55};
        uint16_t unit = (uint16_t)value;
        uint16_t restored = UINT16_C(0x5555);
        size_t expected = value == 0                 ? 2
                          : value <= UINT32_C(0x7f)  ? 1
                          : value <= UINT32_C(0x7ff) ? 2
                                                     : 3;
        CHECK(mc_mutf8_encoded_length(&unit, 1, &encoded) == MC_MUTF8_OK && encoded == expected);
        CHECK(mc_mutf8_encode(&unit, 1, single, sizeof(single), &written) == MC_MUTF8_OK);
        CHECK(written == expected);
        CHECK(mc_mutf8_decoded_length(single, written, &decoded) == MC_MUTF8_OK && decoded == 1);
        CHECK(mc_mutf8_decode(single, written, &restored, 1, &decoded) == MC_MUTF8_OK);
        CHECK(decoded == 1 && restored == unit);
        for (size_t tail = written; tail < sizeof(single); ++tail) {
            CHECK(single[tail] == UINT8_C(0x55));
        }
        input[value] = unit;
    }
    /* Core payloads deliberately exceed the data-stream/NBT 65535-byte cap. */
    CHECK(mc_mutf8_encoded_length(input, 65536, &encoded) == MC_MUTF8_OK);
    CHECK(encoded == 194433);
    CHECK(mc_mutf8_encode(input, 65536, bytes, sizeof(bytes), &written) == MC_MUTF8_OK);
    CHECK(written == encoded);
    CHECK(mc_mutf8_decoded_length(bytes, written, &decoded) == MC_MUTF8_OK && decoded == 65536);
    CHECK(mc_mutf8_decode(bytes, written, output, 65536, &decoded) == MC_MUTF8_OK);
    CHECK(decoded == 65536 && memcmp(input, output, sizeof(input)) == 0);
    return 0;
}

int main(void)
{
    if (test_known_bytes() || test_empty_and_arguments() || test_capacity() || test_malformed() ||
        test_decoder_forms() || test_all_code_units()) {
        return 1;
    }
    printf("modified UTF-8: %u checks passed\n", checks);
    return 0;
}
