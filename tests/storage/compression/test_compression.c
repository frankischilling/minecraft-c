#include "mc/storage/compression/compression.h"

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

/* Fixed independent fixtures generated with Python's gzip/zlib over synthetic
 * bytes. Optional header CRC16 is the low 16 bits of CRC32 over that header.
 * Compressed byte identity is not required of the project encoder. */
static const uint8_t plain[] = {0, 0x7f, 0x80, 0xff, 's', 't', 'o', 'r', 'a', 'g', 'e', 0};
static const char gzip_hex[] = "1f8b08000000000002ff63a86ff85f5c925f94989eca00005adb852f0c000000";
static const char zlib_hex[] = "789c63a86ff85f5c925f94989eca0000229304f4";
static const char gzip_empty_hex[] = "1f8b08000000000002ff03000000000000000000";
static const char zlib_empty_hex[] = "789c030000000001";
static const char gzip_optional_hex[] =
    "1f8b081e0000000000ff0300007fff66697874757265006f7074696f6e616c206669656c647300afc8"
    "63a86ff85f5c925f94989eca00005adb852f0c000000";

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

static int expect_decoded(mc_compression_format format, const uint8_t *bytes, size_t length,
                          const uint8_t *expected, size_t expected_length, size_t expected_consumed,
                          const mc_compression_limits *limits) {
    mc_compression_buffer output = {0};
    size_t consumed = SIZE_MAX;
    CHECK(mc_compression_decode(format, bytes, length, limits, &output, &consumed) ==
          MC_COMPRESSION_OK);
    CHECK(consumed == expected_consumed && output.length == expected_length);
    if (expected_length != 0) {
        CHECK(output.data != NULL);
        CHECK(memcmp(output.data, expected, expected_length) == 0);
    }
    mc_compression_buffer_free(&output);
    CHECK(output.data == NULL && output.length == 0);
    mc_compression_buffer_free(&output);
    return 0;
}

static int expect_decode_failure(mc_compression_format format, const uint8_t *bytes, size_t length,
                                 const mc_compression_limits *limits,
                                 mc_compression_status expected) {
    uint8_t sentinel[] = {1, 2, 3, 4};
    mc_compression_buffer output = {sentinel, sizeof(sentinel)};
    size_t consumed = 98765;
    mc_compression_status status =
        mc_compression_decode(format, bytes, length, limits, &output, &consumed);
    CHECK(status != MC_COMPRESSION_OK);
    if (expected != MC_COMPRESSION_OK)
        CHECK(status == expected);
    CHECK(output.data == sentinel && output.length == sizeof(sentinel) && consumed == 98765);
    CHECK(sentinel[0] == 1 && sentinel[1] == 2 && sentinel[2] == 3 && sentinel[3] == 4);
    return 0;
}

static int expect_encode_failure(mc_compression_format format, const uint8_t *bytes, size_t length,
                                 const mc_compression_limits *limits,
                                 mc_compression_status expected) {
    uint8_t sentinel[] = {1, 2, 3, 4};
    mc_compression_buffer output = {sentinel, sizeof(sentinel)};
    mc_compression_status status = mc_compression_encode(format, bytes, length, limits, &output);
    CHECK(status != MC_COMPRESSION_OK);
    if (expected != MC_COMPRESSION_OK)
        CHECK(status == expected);
    CHECK(output.data == sentinel && output.length == sizeof(sentinel));
    CHECK(sentinel[0] == 1 && sentinel[1] == 2 && sentinel[2] == 3 && sentinel[3] == 4);
    return 0;
}

static int test_fixed_fixtures(void) {
    static const struct {
        mc_compression_format format;
        const char *hex;
        int empty;
    } fixtures[] = {
        {MC_COMPRESSION_GZIP, gzip_hex, 0},          {MC_COMPRESSION_ZLIB, zlib_hex, 0},
        {MC_COMPRESSION_GZIP, gzip_optional_hex, 0}, {MC_COMPRESSION_GZIP, gzip_empty_hex, 1},
        {MC_COMPRESSION_ZLIB, zlib_empty_hex, 1},
    };

    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        uint8_t bytes[128];
        size_t length = decode_hex(fixtures[i].hex, bytes);
        CHECK(expect_decoded(fixtures[i].format, bytes, length, plain,
                             fixtures[i].empty ? 0 : sizeof(plain), length, NULL) == 0);
        mc_compression_buffer output = {0};
        CHECK(mc_compression_decode(fixtures[i].format, bytes, length, NULL, &output, NULL) ==
              MC_COMPRESSION_OK);
        mc_compression_buffer_free(&output);
    }
    return 0;
}

static int test_large_roundtrip_and_ownership(void) {
    const size_t length = 262161;
    uint8_t *expected = malloc(length);
    CHECK(expected != NULL);
    uint32_t state = UINT32_C(0x7a35b19d);
    for (size_t i = 0; i < length; ++i) {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        expected[i] = (uint8_t)(state & UINT32_C(0xff));
    }
    for (int selected = 0; selected < 2; ++selected) {
        mc_compression_format format = selected == 0 ? MC_COMPRESSION_GZIP : MC_COMPRESSION_ZLIB;
        uint8_t *borrowed = malloc(length);
        CHECK(borrowed != NULL);
        memcpy(borrowed, expected, length);
        mc_compression_buffer compressed = {0};
        CHECK(mc_compression_encode(format, borrowed, length, NULL, &compressed) ==
              MC_COMPRESSION_OK);
        memset(borrowed, 0xa5, length);
        free(borrowed);
        CHECK(compressed.data != NULL && compressed.length > 0);
        if (format == MC_COMPRESSION_GZIP) {
            CHECK(compressed.length >= 18 && compressed.data[0] == 0x1f &&
                  compressed.data[1] == 0x8b);
            CHECK(compressed.data[2] == 8 && compressed.data[3] == 0);
            CHECK(compressed.data[4] == 0 && compressed.data[5] == 0 && compressed.data[6] == 0 &&
                  compressed.data[7] == 0 && compressed.data[9] == 255);
        }
        mc_compression_buffer decoded = {0};
        size_t consumed = 0;
        CHECK(mc_compression_decode(format, compressed.data, compressed.length, NULL, &decoded,
                                    &consumed) == MC_COMPRESSION_OK);
        CHECK(consumed == compressed.length && decoded.length == length);
        memset(compressed.data, 0xcc, compressed.length);
        mc_compression_buffer_free(&compressed);
        CHECK(memcmp(decoded.data, expected, length) == 0);
        mc_compression_buffer_free(&decoded);
        CHECK(mc_compression_encode(format, NULL, 0, NULL, &compressed) == MC_COMPRESSION_OK);
        CHECK(expect_decoded(format, compressed.data, compressed.length, NULL, 0, compressed.length,
                             NULL) == 0);
        mc_compression_buffer_free(&compressed);
    }
    free(expected);
    return 0;
}

static int test_concatenation_and_trailing(void) {
    uint8_t bytes[512];
    size_t first = decode_hex("1f8b08000000000002ffcb48cdc9c9070086a6103605000000", bytes);
    size_t second =
        decode_hex("1f8b08000000000002ff5328cf2fca490100cb423b4a06000000", bytes + first);
    static const uint8_t hello_world[] = "hello world";
    size_t length = first + second;
    CHECK(expect_decoded(MC_COMPRESSION_GZIP, bytes, length, hello_world, sizeof(hello_world) - 1u,
                         length, NULL) == 0);
    bytes[length] = 0;
    bytes[length + 1u] = 0x1f;
    bytes[length + 2u] = 0x8b;
    CHECK(expect_decoded(MC_COMPRESSION_GZIP, bytes, length + 3u, hello_world,
                         sizeof(hello_world) - 1u, length, NULL) == 0);
    bytes[length] = 0x1f;
    bytes[length + 1u] = 0x8b;
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length + 2u, NULL,
                                MC_COMPRESSION_TRUNCATED) == 0);
    /* An unrecognized one-byte suffix is not a complete next-member magic. */
    CHECK(expect_decoded(MC_COMPRESSION_GZIP, bytes, length + 1u, hello_world,
                         sizeof(hello_world) - 1u, length, NULL) == 0);
    mc_compression_limits limits = mc_compression_default_limits();
    limits.max_members = 1;
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length, &limits,
                                MC_COMPRESSION_LIMIT) == 0);
    limits.max_members = 2;
    limits.max_output_bytes = sizeof(hello_world) - 1u;
    CHECK(expect_decoded(MC_COMPRESSION_GZIP, bytes, length, hello_world, sizeof(hello_world) - 1u,
                         length, &limits) == 0);
    limits.max_output_bytes--;
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length, &limits,
                                MC_COMPRESSION_LIMIT) == 0);
    size_t empty = decode_hex(gzip_empty_hex, bytes);
    memcpy(bytes + empty, bytes, empty);
    memcpy(bytes + 2u * empty, bytes, empty);
    limits = mc_compression_default_limits();
    limits.max_output_bytes = 0;
    limits.max_members = 3;
    CHECK(expect_decoded(MC_COMPRESSION_GZIP, bytes, 3u * empty, NULL, 0, 3u * empty, &limits) ==
          0);
    limits.max_members = 2;
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, 3u * empty, &limits,
                                MC_COMPRESSION_LIMIT) == 0);
    size_t zlib_length = decode_hex(zlib_hex, bytes);
    memcpy(bytes + zlib_length, bytes, zlib_length);
    CHECK(expect_decoded(MC_COMPRESSION_ZLIB, bytes, 2u * zlib_length, plain, sizeof(plain),
                         zlib_length, NULL) == 0);
    bytes[zlib_length] = 0xff;
    CHECK(expect_decoded(MC_COMPRESSION_ZLIB, bytes, zlib_length + 1u, plain, sizeof(plain),
                         zlib_length, NULL) == 0);
    return 0;
}

static int test_malformed_and_truncated(void) {
    static const struct {
        mc_compression_format format;
        const char *hex;
    } fixtures[] = {{MC_COMPRESSION_GZIP, gzip_hex},
                    {MC_COMPRESSION_GZIP, gzip_optional_hex},
                    {MC_COMPRESSION_ZLIB, zlib_hex},
                    {MC_COMPRESSION_GZIP, gzip_empty_hex},
                    {MC_COMPRESSION_ZLIB, zlib_empty_hex}};

    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        uint8_t bytes[128];
        size_t length = decode_hex(fixtures[i].hex, bytes);
        for (size_t cut = 0; cut < length; ++cut)
            CHECK(expect_decode_failure(fixtures[i].format, bytes, cut, NULL,
                                        MC_COMPRESSION_TRUNCATED) == 0);
    }
    uint8_t bytes[128];
    size_t length = decode_hex(gzip_hex, bytes);
    bytes[0] = 0;
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length, NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    length = decode_hex(gzip_hex, bytes);
    bytes[2] = 0;
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length, NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    length = decode_hex(gzip_hex, bytes);
    bytes[3] = 0xe0;
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length, NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    for (size_t trailer = 0; trailer < 8; ++trailer) {
        length = decode_hex(gzip_hex, bytes);
        bytes[length - 8u + trailer] ^= 1u;
        CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length, NULL,
                                    MC_COMPRESSION_INVALID_STREAM) == 0);
    }
    length = decode_hex(gzip_optional_hex, bytes);
    bytes[39] ^= 1u; /* FHCRC low byte; header otherwise remains well formed. */
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length, NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    length = decode_hex(zlib_hex, bytes);
    bytes[1] ^= 1u; /* CMF/FLG check must be a multiple of 31. */
    CHECK(expect_decode_failure(MC_COMPRESSION_ZLIB, bytes, length, NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    for (size_t trailer = 0; trailer < 4; ++trailer) {
        length = decode_hex(zlib_hex, bytes);
        bytes[length - 4u + trailer] ^= 1u;
        CHECK(expect_decode_failure(MC_COMPRESSION_ZLIB, bytes, length, NULL,
                                    MC_COMPRESSION_INVALID_STREAM) == 0);
    }
    length = decode_hex("78bb16c004374b81b3140a122b73f2135300454e0741", bytes);
    CHECK(expect_decode_failure(MC_COMPRESSION_ZLIB, bytes, length, NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    length = decode_hex(gzip_hex, bytes);
    CHECK(expect_decode_failure(MC_COMPRESSION_ZLIB, bytes, length, NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    length = decode_hex(zlib_hex, bytes);
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, bytes, length, NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    static const uint8_t raw_deflate[] = {3, 0};
    CHECK(expect_decode_failure(MC_COMPRESSION_ZLIB, raw_deflate, sizeof(raw_deflate), NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, raw_deflate, sizeof(raw_deflate), NULL,
                                MC_COMPRESSION_INVALID_STREAM) == 0);
    return 0;
}

static int test_byte_limits(void) {
    for (int selected = 0; selected < 2; ++selected) {
        mc_compression_format format = selected == 0 ? MC_COMPRESSION_GZIP : MC_COMPRESSION_ZLIB;
        uint8_t bytes[128];
        size_t length = decode_hex(selected == 0 ? gzip_hex : zlib_hex, bytes);
        mc_compression_limits limits = mc_compression_default_limits();
        limits.max_input_bytes = length - 1u;
        CHECK(expect_decode_failure(format, bytes, length, &limits, MC_COMPRESSION_LIMIT) == 0);
        limits.max_input_bytes = length;
        limits.max_output_bytes = sizeof(plain);
        CHECK(expect_decoded(format, bytes, length, plain, sizeof(plain), length, &limits) == 0);
        limits.max_output_bytes = sizeof(plain) - 1u;
        CHECK(expect_decode_failure(format, bytes, length, &limits, MC_COMPRESSION_LIMIT) == 0);
        limits.max_output_bytes = 0;
        CHECK(expect_decode_failure(format, bytes, length, &limits, MC_COMPRESSION_LIMIT) == 0);
        length = decode_hex(selected == 0 ? gzip_empty_hex : zlib_empty_hex, bytes);
        CHECK(expect_decoded(format, bytes, length, NULL, 0, length, &limits) == 0);
        limits.max_input_bytes = 0;
        CHECK(expect_decode_failure(format, bytes, length, &limits, MC_COMPRESSION_LIMIT) == 0);
        limits = mc_compression_default_limits();
        limits.max_input_bytes = sizeof(plain) - 1u;
        CHECK(expect_encode_failure(format, plain, sizeof(plain), &limits, MC_COMPRESSION_LIMIT) ==
              0);
        limits.max_input_bytes = sizeof(plain);
        mc_compression_buffer encoded = {0};
        CHECK(mc_compression_encode(format, plain, sizeof(plain), &limits, &encoded) ==
              MC_COMPRESSION_OK);
        size_t encoded_length = encoded.length;
        mc_compression_buffer_free(&encoded);
        limits.max_output_bytes = encoded_length - 1u;
        CHECK(expect_encode_failure(format, plain, sizeof(plain), &limits, MC_COMPRESSION_LIMIT) ==
              0);
        limits.max_output_bytes = encoded_length;
        CHECK(mc_compression_encode(format, plain, sizeof(plain), &limits, &encoded) ==
              MC_COMPRESSION_OK);
        CHECK(encoded.length == encoded_length);
        mc_compression_buffer_free(&encoded);
        limits.max_output_bytes = 0;
        CHECK(expect_encode_failure(format, NULL, 0, &limits, MC_COMPRESSION_LIMIT) == 0);
        limits = mc_compression_default_limits();
        limits.max_input_bytes = 0;
        CHECK(mc_compression_encode(format, NULL, 0, &limits, &encoded) == MC_COMPRESSION_OK);
        mc_compression_buffer_free(&encoded);
    }
    return 0;
}

static int test_allocation_limits(void) {
    for (int selected = 0; selected < 2; ++selected) {
        mc_compression_format format = selected == 0 ? MC_COMPRESSION_GZIP : MC_COMPRESSION_ZLIB;
        uint8_t bytes[128];
        size_t length = decode_hex(selected == 0 ? gzip_hex : zlib_hex, bytes);
        mc_compression_limits limits = mc_compression_default_limits();
        limits.max_allocation_bytes = 0;
        CHECK(expect_decode_failure(format, bytes, length, &limits, MC_COMPRESSION_LIMIT) == 0);
        CHECK(expect_encode_failure(format, plain, sizeof(plain), &limits, MC_COMPRESSION_LIMIT) ==
              0);
        /* Discover a platform-specific peak without assuming zlib's private layout. */
        for (int encode = 0; encode < 2; ++encode) {
            size_t lower = 0;
            size_t upper = mc_compression_default_limits().max_allocation_bytes;
            while (upper - lower > 1u) {
                size_t candidate = lower + (upper - lower) / 2u;
                mc_compression_buffer output = {0};
                limits.max_allocation_bytes = candidate;
                mc_compression_status status;
                if (encode)
                    status = mc_compression_encode(format, plain, sizeof(plain), &limits, &output);
                else
                    status = mc_compression_decode(format, bytes, length, &limits, &output, NULL);
                CHECK(status == MC_COMPRESSION_OK || status == MC_COMPRESSION_LIMIT);
                if (status == MC_COMPRESSION_OK)
                    upper = candidate;
                else
                    lower = candidate;
                mc_compression_buffer_free(&output);
            }
            CHECK(upper > 0);
            limits.max_allocation_bytes = upper - 1u;
            if (encode)
                CHECK(expect_encode_failure(format, plain, sizeof(plain), &limits,
                                            MC_COMPRESSION_LIMIT) == 0);
            else
                CHECK(expect_decode_failure(format, bytes, length, &limits, MC_COMPRESSION_LIMIT) ==
                      0);
            limits.max_allocation_bytes = upper;
            if (encode) {
                mc_compression_buffer output = {0};
                CHECK(mc_compression_encode(format, plain, sizeof(plain), &limits, &output) ==
                      MC_COMPRESSION_OK);
                mc_compression_buffer_free(&output);
            } else {
                CHECK(expect_decoded(format, bytes, length, plain, sizeof(plain), length,
                                     &limits) == 0);
            }
        }
    }
    return 0;
}

static int test_invalid_arguments(void) {
    mc_compression_limits limits = mc_compression_default_limits();
    CHECK(limits.max_input_bytes == 64u * 1024u * 1024u);
    CHECK(limits.max_output_bytes == 64u * 1024u * 1024u);
    CHECK(limits.max_allocation_bytes == 128u * 1024u * 1024u && limits.max_members == 1024);
    CHECK(expect_decode_failure((mc_compression_format)99, plain, sizeof(plain), NULL,
                                MC_COMPRESSION_INVALID_ARGUMENT) == 0);
    CHECK(expect_encode_failure((mc_compression_format)99, plain, sizeof(plain), NULL,
                                MC_COMPRESSION_INVALID_ARGUMENT) == 0);
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, NULL, 1, NULL,
                                MC_COMPRESSION_INVALID_ARGUMENT) == 0);
    CHECK(expect_encode_failure(MC_COMPRESSION_GZIP, NULL, 1, NULL,
                                MC_COMPRESSION_INVALID_ARGUMENT) == 0);
    CHECK(mc_compression_decode(MC_COMPRESSION_GZIP, plain, sizeof(plain), NULL, NULL, NULL) ==
          MC_COMPRESSION_INVALID_ARGUMENT);
    CHECK(mc_compression_encode(MC_COMPRESSION_GZIP, plain, sizeof(plain), NULL, NULL) ==
          MC_COMPRESSION_INVALID_ARGUMENT);
    limits.max_members = 0;
    CHECK(expect_decode_failure(MC_COMPRESSION_GZIP, plain, sizeof(plain), &limits,
                                MC_COMPRESSION_INVALID_ARGUMENT) == 0);
    CHECK(expect_encode_failure(MC_COMPRESSION_GZIP, plain, sizeof(plain), &limits,
                                MC_COMPRESSION_INVALID_ARGUMENT) == 0);
    mc_compression_buffer_free(NULL);
    for (int status = MC_COMPRESSION_OK; status <= MC_COMPRESSION_BACKEND_ERROR; ++status)
        CHECK(mc_compression_status_string((mc_compression_status)status) != NULL);
    CHECK(mc_compression_status_string((mc_compression_status)99) != NULL);
    return 0;
}

static int test_allocation_growth(void) {
    const size_t length = 131073;
    uint8_t *payload = malloc(length);
    CHECK(payload != NULL);
    memset(payload, 0xa5, length);
    for (int selected = 0; selected < 2; ++selected) {
        mc_compression_format format = selected == 0 ? MC_COMPRESSION_GZIP : MC_COMPRESSION_ZLIB;
        mc_compression_buffer compressed = {0};
        CHECK(mc_compression_encode(format, payload, length, NULL, &compressed) ==
              MC_COMPRESSION_OK);
        mc_compression_limits limits = mc_compression_default_limits();
        size_t lower = 0;
        size_t upper = limits.max_allocation_bytes;
        while (upper - lower > 1u) {
            mc_compression_buffer decoded = {0};
            size_t candidate = lower + (upper - lower) / 2u;
            limits.max_allocation_bytes = candidate;
            mc_compression_status status = mc_compression_decode(
                format, compressed.data, compressed.length, &limits, &decoded, NULL);
            CHECK(status == MC_COMPRESSION_OK || status == MC_COMPRESSION_LIMIT);
            if (status == MC_COMPRESSION_OK)
                upper = candidate;
            else
                lower = candidate;
            mc_compression_buffer_free(&decoded);
        }
        CHECK(upper > 0);
        limits.max_allocation_bytes = upper - 1u;
        CHECK(expect_decode_failure(format, compressed.data, compressed.length, &limits,
                                    MC_COMPRESSION_LIMIT) == 0);
        /* This budget supports initialization and a short output; the large
         * stream therefore fails after decoding can begin and output grows. */
        uint8_t short_stream[128];
        size_t short_length = decode_hex(selected == 0 ? gzip_hex : zlib_hex, short_stream);
        CHECK(expect_decoded(format, short_stream, short_length, plain, sizeof(plain), short_length,
                             &limits) == 0);
        limits.max_allocation_bytes = upper;
        CHECK(expect_decoded(format, compressed.data, compressed.length, payload, length,
                             compressed.length, &limits) == 0);
        mc_compression_buffer_free(&compressed);
    }
    free(payload);
    return 0;
}

static int test_bounded_mutations(void) {
    static const struct {
        mc_compression_format format;
        const char *hex;
    } seeds[] = {{MC_COMPRESSION_GZIP, gzip_hex},
                 {MC_COMPRESSION_GZIP, gzip_optional_hex},
                 {MC_COMPRESSION_GZIP, gzip_empty_hex},
                 {MC_COMPRESSION_ZLIB, zlib_hex},
                 {MC_COMPRESSION_ZLIB, zlib_empty_hex}};

    static const uint8_t masks[] = {1, 0x80, 0xff};
    mc_compression_limits limits = mc_compression_default_limits();
    limits.max_input_bytes = 128;
    limits.max_output_bytes = 4096;
    limits.max_allocation_bytes = 1024u * 1024u;
    limits.max_members = 4;
    for (size_t seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
        uint8_t original[128];
        size_t length = decode_hex(seeds[seed].hex, original);
        for (size_t index = 0; index < length; ++index) {
            for (size_t mutation = 0; mutation < sizeof(masks) / sizeof(masks[0]); ++mutation) {
                uint8_t mutated[128];
                uint8_t sentinel = 0xa5;
                mc_compression_buffer output = {&sentinel, 1};
                size_t consumed = SIZE_MAX;
                memcpy(mutated, original, length);
                mutated[index] ^= masks[mutation];
                mc_compression_status status = mc_compression_decode(
                    seeds[seed].format, mutated, length, &limits, &output, &consumed);
                if (status == MC_COMPRESSION_OK) {
                    CHECK(consumed <= length && output.length <= limits.max_output_bytes);
                    mc_compression_buffer encoded = {0};
                    mc_compression_buffer decoded = {0};
                    CHECK(mc_compression_encode(seeds[seed].format, output.data, output.length,
                                                NULL, &encoded) == MC_COMPRESSION_OK);
                    CHECK(mc_compression_decode(seeds[seed].format, encoded.data, encoded.length,
                                                NULL, &decoded, NULL) == MC_COMPRESSION_OK);
                    CHECK(decoded.length == output.length);
                    if (output.length != 0)
                        CHECK(memcmp(decoded.data, output.data, output.length) == 0);
                    mc_compression_buffer_free(&decoded);
                    mc_compression_buffer_free(&encoded);
                    mc_compression_buffer_free(&output);
                } else {
                    CHECK(output.data == &sentinel && output.length == 1 && consumed == SIZE_MAX);
                }
                CHECK(sentinel == 0xa5);
            }
        }
    }
    return 0;
}

int main(void) {
    if (test_fixed_fixtures() || test_large_roundtrip_and_ownership() ||
        test_concatenation_and_trailing() || test_malformed_and_truncated() || test_byte_limits() ||
        test_allocation_limits() || test_invalid_arguments() || test_allocation_growth() ||
        test_bounded_mutations())
        return 1;
    printf("Compression: %u checks passed\n", checks);
    return 0;
}
