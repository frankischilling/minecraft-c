#include "internal.h"

#include <string.h>

/* Legacy hf checksum, unsigned arithmetic throughout. Source uses the
 * positive BigInteger's unpadded lowercase radix16 representation. */
static uint32_t rotate(uint32_t value, unsigned int count) {
    return (value << count) | (value >> (32u - count));
}

static void block(uint32_t state[4], const uint8_t bytes[64]) {
    static const uint32_t constants[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u,
        0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u,
        0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du,
        0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u, 0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
        0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u,
        0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
        0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u, 0xf4292244u,
        0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu,
        0xeb86d391u};
    static const unsigned int shifts[64] = {
        7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
        14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
        4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21};
    uint32_t words[16];
    for (size_t i = 0; i < 16; ++i)
        words[i] = (uint32_t)bytes[i * 4] | ((uint32_t)bytes[i * 4 + 1] << 8u) |
                   ((uint32_t)bytes[i * 4 + 2] << 16u) | ((uint32_t)bytes[i * 4 + 3] << 24u);
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (unsigned int i = 0; i < 64; ++i) {
        uint32_t value;
        unsigned int index;
        if (i < 16) {
            value = (b & c) | (~b & d);
            index = i;
        } else if (i < 32) {
            value = (d & b) | (~d & c);
            index = (5u * i + 1u) % 16u;
        } else if (i < 48) {
            value = b ^ c ^ d;
            index = (3u * i + 5u) % 16u;
        } else {
            value = c ^ (b | ~d);
            index = (7u * i) % 16u;
        }
        value += a + constants[i] + words[index];
        a = d;
        d = c;
        c = b;
        b += rotate(value, shifts[i]);
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static void digest(const uint8_t *bytes, size_t length, char out[33]) {
    uint32_t state[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    size_t offset = 0;
    while (length - offset >= 64) {
        block(state, bytes + offset);
        offset += 64;
    }
    uint8_t tail[128] = {0};
    const size_t remainder = length - offset;
    if (remainder != 0)
        memcpy(tail, bytes + offset, remainder);
    tail[remainder] = 0x80u;
    const size_t padded = remainder < 56 ? 64 : 128;
    const uint64_t bits = (uint64_t)length * UINT64_C(8);
    for (unsigned int i = 0; i < 8; ++i)
        tail[padded - 8u + i] = (uint8_t)(bits >> (i * 8u));
    block(state, tail);
    if (padded == 128)
        block(state, tail + 64);
    static const char digits[] = "0123456789abcdef";
    char full[32];
    for (size_t i = 0; i < 16; ++i) {
        const uint8_t value = (uint8_t)(state[i / 4u] >> ((i % 4u) * 8u));
        full[i * 2u] = digits[value >> 4u];
        full[i * 2u + 1u] = digits[value & 15u];
    }
    size_t first = 0;
    while (first < 31 && full[first] == '0')
        ++first;
    memcpy(out, full + first, 32u - first);
    out[32u - first] = '\0';
}

typedef struct byte_sink {
    stats_memory *memory;
    uint8_t *bytes;
    size_t length, capacity, maximum;
    mc_client_stats_error failure;
} byte_sink;

static mc_java_writer_status sink_write(void *context, const uint8_t *bytes, size_t length,
                                        size_t *committed, mc_java_writer_error *error) {
    byte_sink *sink = context;
    *committed = 0;
    if (length > sink->maximum || sink->length > sink->maximum - length) {
        stats_fail(&sink->failure, MC_CLIENT_STATS_LIMIT, "Stats checksum encoded byte extent",
                   NULL, NULL, NULL, 0);
        *error = (mc_java_writer_error){.status = MC_JAVA_WRITER_LIMIT};
        return MC_JAVA_WRITER_LIMIT;
    }
    const size_t needed = sink->length + length;
    if (needed > sink->capacity) {
        size_t capacity = sink->capacity == 0 ? 256 : sink->capacity;
        if (capacity > sink->maximum)
            capacity = sink->maximum;
        while (capacity < needed) {
            if (capacity > sink->maximum / 2u) {
                capacity = sink->maximum;
                break;
            }
            capacity *= 2u;
        }
        uint8_t *storage = stats_alloc(sink->memory, capacity, &sink->failure);
        if (storage == NULL) {
            *error = (mc_java_writer_error){.status = sink->failure.status == MC_CLIENT_STATS_LIMIT
                                                          ? MC_JAVA_WRITER_LIMIT
                                                          : MC_JAVA_WRITER_ALLOCATION_FAILED};
            return error->status;
        }
        if (sink->length != 0)
            memcpy(storage, sink->bytes, sink->length);
        stats_free(sink->bytes);
        sink->bytes = storage;
        sink->capacity = capacity;
    }
    memcpy(sink->bytes + sink->length, bytes, length);
    sink->length += length;
    *committed = length;
    *error = (mc_java_writer_error){0};
    return MC_JAVA_WRITER_OK;
}

static mc_java_writer_status sink_end(void *context, mc_java_writer_error *error) {
    (void)context;
    *error = (mc_java_writer_error){0};
    return MC_JAVA_WRITER_OK;
}

mc_client_stats_status stats_checksum(stats_memory *memory, size_t maximum,
                                      mc_java_reader_charset charset, mc_client_stats_string salt,
                                      mc_client_stats_string payload, char out[33],
                                      mc_client_stats_error *error) {
    stats_builder text = {.memory = memory, .maximum = maximum};
    mc_client_stats_status status = stats_string(&text, salt, error);
    if (status == MC_CLIENT_STATS_OK)
        status = stats_string(&text, payload, error);
    if (status != MC_CLIENT_STATS_OK) {
        stats_builder_clear(&text);
        return status;
    }
    byte_sink sink = {.memory = memory,
                      .maximum = maximum > SIZE_MAX / 4u ? SIZE_MAX : maximum * 4u};
    stats_error_clear(&sink.failure);
    const mc_java_writer_output output = {&sink, sink_write, sink_end, sink_end};
    const mc_java_writer_profile profile = {charset, MC_JAVA_WRITER_OUTPUT_STREAM,
                                            MC_JAVA_WRITER_LF};
    mc_java_writer *writer = NULL;
    mc_java_writer_error cause;
    mc_java_writer_status ws = mc_java_writer_create(&output, &profile, &writer, &cause);
    if (ws == MC_JAVA_WRITER_OK)
        ws = mc_java_writer_write(writer, text.units, text.length, &cause);
    if (ws == MC_JAVA_WRITER_OK)
        ws = mc_java_writer_close(writer, &cause);
    (void)mc_java_writer_retire(&writer, NULL, NULL);
    if (ws != MC_JAVA_WRITER_OK) {
        if (sink.failure.status != MC_CLIENT_STATS_OK)
            *error = sink.failure;
        else {
            error->writer = ws;
            stats_fail(error,
                       ws == MC_JAVA_WRITER_ALLOCATION_FAILED ? MC_CLIENT_STATS_ALLOCATION_FAILED
                       : ws == MC_JAVA_WRITER_LIMIT           ? MC_CLIENT_STATS_LIMIT
                                                              : MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
                       "Stats checksum String.getBytes charset", "hf", "a",
                       "(Ljava/lang/String;)Ljava/lang/String;", 30);
        }
        status = error->status;
    } else if (text.length > sink.length)
        status = stats_exception(error, MC_CLIENT_STATS_INDEX_OUT_OF_BOUNDS,
                                 "Stats checksum update uses String.length bytes", "hf", "a",
                                 "(Ljava/lang/String;)Ljava/lang/String;", 38);
    else
        digest(sink.bytes, text.length, out);
    stats_free(sink.bytes);
    stats_builder_clear(&text);
    return status;
}
