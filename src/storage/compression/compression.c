#include "mc/storage/compression/compression.h"

#include <zlib.h>

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* The payload needs the alignment of zlib state and the supported desktop C
 * scalar types. Do not depend on max_align_t, absent from some Windows CRTs. */
typedef union compression_allocation compression_allocation;

union compression_allocation {
    struct {
        compression_allocation *previous;
        compression_allocation *next;
        size_t bytes;
    } bookkeeping;

    z_stream stream_alignment;
    long double scalar_alignment;
    void *pointer_alignment;
};

typedef struct compression_allocator {
    compression_allocation *allocations;
    size_t live_bytes;
    size_t limit;
    mc_compression_status failure;
} compression_allocator;

typedef struct compression_output {
    uint8_t *data;
    size_t length;
    size_t capacity;
} compression_output;

mc_compression_limits mc_compression_default_limits(void) {
    const mc_compression_limits limits = {64u * 1024u * 1024u, 64u * 1024u * 1024u,
                                          128u * 1024u * 1024u, 1024u};
    return limits;
}

const char *mc_compression_status_string(mc_compression_status status) {
    switch (status) {
        case MC_COMPRESSION_OK:
            return "ok";
        case MC_COMPRESSION_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_COMPRESSION_INVALID_STREAM:
            return "invalid compressed stream";
        case MC_COMPRESSION_TRUNCATED:
            return "truncated compressed stream";
        case MC_COMPRESSION_LIMIT:
            return "resource limit";
        case MC_COMPRESSION_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_COMPRESSION_BACKEND_ERROR:
            return "compression backend error";
    }
    return "unknown compression status";
}

static void compression_allocation_failure(compression_allocator *allocator,
                                           mc_compression_status failure) {
    if (allocator->failure == MC_COMPRESSION_OK) {
        allocator->failure = failure;
    }
}

static void *compression_allocate(compression_allocator *allocator, size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) {
        compression_allocation_failure(allocator, MC_COMPRESSION_LIMIT);
        return NULL;
    }
    size_t payload_bytes = count * size;
    if (payload_bytes > SIZE_MAX - sizeof(compression_allocation)) {
        compression_allocation_failure(allocator, MC_COMPRESSION_LIMIT);
        return NULL;
    }
    size_t bytes = sizeof(compression_allocation) + payload_bytes;
    if (bytes > allocator->limit - allocator->live_bytes) {
        compression_allocation_failure(allocator, MC_COMPRESSION_LIMIT);
        return NULL;
    }
    compression_allocation *allocation = malloc(bytes);
    if (allocation == NULL) {
        compression_allocation_failure(allocator, MC_COMPRESSION_ALLOCATION_FAILED);
        return NULL;
    }
    allocation->bookkeeping.previous = NULL;
    allocation->bookkeeping.next = allocator->allocations;
    allocation->bookkeeping.bytes = bytes;
    if (allocator->allocations != NULL) {
        allocator->allocations->bookkeeping.previous = allocation;
    }
    allocator->allocations = allocation;
    allocator->live_bytes += bytes;
    void *payload = allocation + 1;
    memset(payload, 0, payload_bytes);
    return payload;
}

static void compression_detach(compression_allocator *allocator,
                               compression_allocation *allocation) {
    if (allocation->bookkeeping.previous != NULL) {
        allocation->bookkeeping.previous->bookkeeping.next = allocation->bookkeeping.next;
    } else {
        allocator->allocations = allocation->bookkeeping.next;
    }
    if (allocation->bookkeeping.next != NULL) {
        allocation->bookkeeping.next->bookkeeping.previous = allocation->bookkeeping.previous;
    }
    allocator->live_bytes -= allocation->bookkeeping.bytes;
    allocation->bookkeeping.previous = NULL;
    allocation->bookkeeping.next = NULL;
}

static void compression_release(compression_allocator *allocator, void *payload) {
    if (payload != NULL) {
        compression_allocation *allocation = (compression_allocation *)payload - 1;
        compression_detach(allocator, allocation);
        free(allocation);
    }
}

static void compression_release_all(compression_allocator *allocator) {
    while (allocator->allocations != NULL) {
        compression_allocation *allocation = allocator->allocations;
        compression_detach(allocator, allocation);
        free(allocation);
    }
}

static voidpf compression_zalloc(voidpf opaque, uInt count, uInt size) {
    return compression_allocate(opaque, count, size);
}

static void compression_zfree(voidpf opaque, voidpf payload) {
    compression_release(opaque, payload);
}

void mc_compression_buffer_free(mc_compression_buffer *buffer) {
    if (buffer != NULL) {
        if (buffer->data != NULL) {
            free((compression_allocation *)buffer->data - 1);
        }
        buffer->data = NULL;
        buffer->length = 0;
    }
}

static mc_compression_status compression_backend_status(int result,
                                                        const compression_allocator *allocator) {
    switch (result) {
        case Z_DATA_ERROR:
        case Z_NEED_DICT:
            return MC_COMPRESSION_INVALID_STREAM;
        case Z_MEM_ERROR:
            return allocator->failure == MC_COMPRESSION_OK ? MC_COMPRESSION_ALLOCATION_FAILED
                                                           : allocator->failure;
        default:
            return MC_COMPRESSION_BACKEND_ERROR;
    }
}

static bool compression_grow(compression_allocator *allocator, compression_output *output,
                             size_t maximum) {
    size_t capacity;
    if (output->capacity == 0) {
        capacity = 4096;
    } else if (output->capacity > SIZE_MAX / 2) {
        capacity = maximum;
    } else {
        capacity = output->capacity * 2;
    }
    if (capacity > maximum) {
        capacity = maximum;
    }
    /* Growth must fit with the old buffer still live. Use a smaller growth
     * step when the remaining allocation budget cannot fit the preferred one. */
    size_t available = allocator->limit - allocator->live_bytes;
    if (available <= sizeof(compression_allocation)) {
        compression_allocation_failure(allocator, MC_COMPRESSION_LIMIT);
        return false;
    }
    available -= sizeof(compression_allocation);
    if (capacity > available) {
        capacity = available;
    }
    if (capacity <= output->capacity) {
        compression_allocation_failure(allocator, MC_COMPRESSION_LIMIT);
        return false;
    }
    uint8_t *data = compression_allocate(allocator, capacity, sizeof(*data));
    if (data == NULL) {
        return false;
    }
    if (output->length != 0) {
        memcpy(data, output->data, output->length);
    }
    compression_release(allocator, output->data);
    output->data = data;
    output->capacity = capacity;
    return true;
}

static uInt compression_chunk(size_t length) {
    return length > UINT_MAX ? UINT_MAX : (uInt)length;
}

static mc_compression_status compression_run(mc_compression_format format, bool encode,
                                             const uint8_t *input, size_t length,
                                             const mc_compression_limits *requested_limits,
                                             mc_compression_buffer *out, size_t *consumed) {
    mc_compression_limits limits =
        requested_limits == NULL ? mc_compression_default_limits() : *requested_limits;
    if (out == NULL || (input == NULL && length != 0) || limits.max_members == 0 ||
        (format != MC_COMPRESSION_GZIP && format != MC_COMPRESSION_ZLIB)) {
        return MC_COMPRESSION_INVALID_ARGUMENT;
    }
    if (length > limits.max_input_bytes) {
        return MC_COMPRESSION_LIMIT;
    }
    compression_allocator allocator = {0};
    allocator.limit = limits.max_allocation_bytes;
    compression_output output = {0};
    z_stream stream = {0};
    stream.zalloc = compression_zalloc;
    stream.zfree = compression_zfree;
    stream.opaque = &allocator;
    int window_bits = format == MC_COMPRESSION_GZIP ? 31 : 15;
    int result = encode ? deflateInit2(&stream, 6, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY)
                        : inflateInit2(&stream, window_bits);
    if (result != Z_OK) {
        mc_compression_status status = compression_backend_status(result, &allocator);
        compression_release_all(&allocator);
        return status;
    }
    mc_compression_status status = MC_COMPRESSION_OK;
    gz_header header = {0};
    if (encode && format == MC_COMPRESSION_GZIP) {
        header.os = 255;
        result = deflateSetHeader(&stream, &header);
        if (result != Z_OK) {
            status = compression_backend_status(result, &allocator);
        }
    }
    size_t position = 0;
    size_t members = 1;
    while (status == MC_COMPRESSION_OK) {
        size_t remaining = length - position;
        uInt input_chunk = compression_chunk(remaining);
        stream.next_in = input_chunk == 0 ? Z_NULL : (Bytef *)(input + position);
        stream.avail_in = input_chunk;
        uint8_t overflow_byte = 0;
        bool at_output_limit = output.length == limits.max_output_bytes;
        if (at_output_limit) {
            /* A byte of scratch space lets an exact-limit stream finish its
             * checksum/trailer. Producing that byte proves the limit exceeded. */
            stream.next_out = &overflow_byte;
            stream.avail_out = 1;
        } else {
            if (output.length == output.capacity &&
                !compression_grow(&allocator, &output, limits.max_output_bytes)) {
                status = allocator.failure;
                break;
            }
            stream.next_out = output.data + output.length;
            stream.avail_out = compression_chunk(output.capacity - output.length);
        }
        uInt output_chunk = stream.avail_out;
        int flush = remaining <= UINT_MAX ? Z_FINISH : Z_NO_FLUSH;
        result = encode ? deflate(&stream, flush) : inflate(&stream, Z_NO_FLUSH);
        size_t input_used = (size_t)(input_chunk - stream.avail_in);
        size_t output_used = (size_t)(output_chunk - stream.avail_out);
        position += input_used;
        if (at_output_limit) {
            if (output_used != 0) {
                status = MC_COMPRESSION_LIMIT;
                break;
            }
        } else {
            output.length += output_used;
        }
        if (result == Z_STREAM_END) {
            if (!encode && format == MC_COMPRESSION_GZIP && length - position >= 2 &&
                input[position] == UINT8_C(0x1f) && input[position + 1] == UINT8_C(0x8b)) {
                if (members == limits.max_members) {
                    status = MC_COMPRESSION_LIMIT;
                    break;
                }
                ++members;
                result = inflateReset2(&stream, window_bits);
                if (result != Z_OK) {
                    status = compression_backend_status(result, &allocator);
                }
                continue;
            }
            break;
        }
        if (result != Z_OK && result != Z_BUF_ERROR) {
            status = compression_backend_status(result, &allocator);
            break;
        }
        if (input_used == 0 && output_used == 0) {
            status = !encode && position == length ? MC_COMPRESSION_TRUNCATED
                                                   : MC_COMPRESSION_BACKEND_ERROR;
            break;
        }
    }
    result = encode ? deflateEnd(&stream) : inflateEnd(&stream);
    if (status == MC_COMPRESSION_OK && result != Z_OK) {
        status = compression_backend_status(result, &allocator);
    }
    uint8_t *data = NULL;
    if (status == MC_COMPRESSION_OK && output.length != 0) {
        data = output.data;
        compression_detach(&allocator, (compression_allocation *)data - 1);
    }
    compression_release_all(&allocator);
    if (status != MC_COMPRESSION_OK) {
        return status;
    }
    out->data = data;
    out->length = output.length;
    if (consumed != NULL) {
        *consumed = position;
    }
    return MC_COMPRESSION_OK;
}

mc_compression_status mc_compression_decode(mc_compression_format format, const uint8_t *input,
                                            size_t length, const mc_compression_limits *limits,
                                            mc_compression_buffer *out, size_t *consumed) {
    return compression_run(format, false, input, length, limits, out, consumed);
}

mc_compression_status mc_compression_encode(mc_compression_format format, const uint8_t *input,
                                            size_t length, const mc_compression_limits *limits,
                                            mc_compression_buffer *out) {
    return compression_run(format, true, input, length, limits, out, NULL);
}
