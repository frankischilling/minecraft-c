#ifndef MC_STORAGE_COMPRESSION_COMPRESSION_H
#define MC_STORAGE_COMPRESSION_COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

typedef enum mc_compression_format {
    MC_COMPRESSION_GZIP,
    MC_COMPRESSION_ZLIB
} mc_compression_format;

typedef enum mc_compression_status {
    MC_COMPRESSION_OK = 0,
    MC_COMPRESSION_INVALID_ARGUMENT,
    MC_COMPRESSION_INVALID_STREAM,
    MC_COMPRESSION_TRUNCATED,
    MC_COMPRESSION_LIMIT,
    MC_COMPRESSION_ALLOCATION_FAILED,
    MC_COMPRESSION_BACKEND_ERROR
} mc_compression_status;

typedef struct mc_compression_limits {
    size_t max_input_bytes;
    size_t max_output_bytes;
    size_t max_allocation_bytes;
    size_t max_members;
} mc_compression_limits;

typedef struct mc_compression_buffer {
    uint8_t *data;
    size_t length;
} mc_compression_buffer;

/* Defaults: 64 MiB input/output, 128 MiB peak live allocations, 1024 members.
 * The allocation limit includes buffer capacity, this module's allocation
 * headers and zlib's private state; temporary growth counts both old and new
 * storage. System allocator overhead is outside this accounting. Zero byte
 * limits are valid; max_members must be at least one. */
mc_compression_limits mc_compression_default_limits(void);
const char *mc_compression_status_string(mc_compression_status status);

/* NULL limits selects defaults. Input may be NULL only when length is zero.
 * out is required and receives an owned buffer on success. A successful empty
 * buffer may have data == NULL. The caller must release any prior owned buffer
 * before reusing out. Failure leaves *out and *consumed unchanged.
 * Result/count storage must be mutually disjoint and must not overlap input or
 * limits. The input and limits remain borrowed only for the call.
 *
 * Decode validates each entire stream including checksums/trailers. Gzip
 * concatenates adjacent members starting with 1f8b; a recognized additional
 * member must be valid. Nonmagic trailing bytes are left unconsumed. Zlib stops
 * after its first stream. consumed is optional and reports the exact end of the
 * last member, independent of transport buffering. Preset dictionaries and raw
 * deflate are unsupported. This module neither parses NBT nor performs I/O. */
mc_compression_status mc_compression_decode(mc_compression_format format, const uint8_t *input,
                                            size_t length, const mc_compression_limits *limits,
                                            mc_compression_buffer *out, size_t *consumed);

/* Encode uses default deflate level 6 and one member. Gzip writes MTIME 0,
 * OS 255, no optional fields. Output is interoperable; compressed byte identity
 * with another compressor/runtime is not promised. Ownership and failure
 * contracts match decode. */
mc_compression_status mc_compression_encode(mc_compression_format format, const uint8_t *input,
                                            size_t length, const mc_compression_limits *limits,
                                            mc_compression_buffer *out);

/* NULL is accepted. A non-NULL buffer is reset to {NULL, 0}. */
void mc_compression_buffer_free(mc_compression_buffer *buffer);

#endif
