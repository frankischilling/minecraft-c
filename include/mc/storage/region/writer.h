#ifndef MC_STORAGE_REGION_WRITER_H
#define MC_STORAGE_REGION_WRITER_H

#include <stddef.h>
#include <stdint.h>

#define MC_REGION_WRITER_MAX_IMAGE_BYTES UINT32_C(0x7ffff000)
#define MC_REGION_WRITER_MAX_ZLIB_BYTES 1044474u

typedef struct mc_region_writer mc_region_writer;

typedef enum mc_region_writer_status {
    MC_REGION_WRITER_OK = 0,
    MC_REGION_WRITER_INVALID_ARGUMENT,
    MC_REGION_WRITER_INVALID_COORDINATE,
    MC_REGION_WRITER_INVALID_IMAGE,
    MC_REGION_WRITER_RECORD_TOO_LARGE,
    MC_REGION_WRITER_LIMIT,
    MC_REGION_WRITER_ALLOCATION_FAILED
} mc_region_writer_status;

typedef struct mc_region_writer_limits {
    size_t max_image_bytes;
    size_t max_allocation_bytes;
} mc_region_writer_limits;

typedef struct mc_region_writer_bytes {
    const uint8_t *data;
    size_t length;
} mc_region_writer_bytes;

/* Defaults: 64 MiB image, 128 MiB peak live allocations. The allocation budget
 * includes the owner object, image and free-sector capacities, and simultaneous
 * old/new buffers during growth. System allocator overhead and borrowed inputs
 * are excluded. Zero budgets are valid; a max_image_bytes above the absolute
 * maximum is an invalid argument. The maximum keeps successful images inside
 * the originals' nonoverflowing signed file-length/sector-seek domain. */
mc_region_writer_limits mc_region_writer_default_limits(void);
const char *mc_region_writer_status_string(mc_region_writer_status status);

/* NULL limits selects defaults. out is required and receives a fresh opaque
 * owner on success; release any prior owner before reusing out. Failure leaves
 * *out unchanged. Result storage must not overlap inputs or limits. The owner
 * requires exclusive access, must not be copied, and exposes no mutable bytes.
 * All calls are synchronous, without callbacks, compression, clocks or I/O. */
mc_region_writer_status mc_region_writer_create(const mc_region_writer_limits *limits,
                                                mc_region_writer **out);

/* Copies an existing image and preserves every byte, including malformed record
 * lengths/selectors and stale/unallocated bytes. Requires at least 8192 bytes,
 * sector alignment, and allocation-safe location extents: starts >= 2, nonzero
 * counts, bounds within the image, and no overlap. Payload validity is separate
 * and remains diagnosed by the unchanged read-only region API.
 *
 * Existing short/nonaligned files and unsafe allocation tables are rejected;
 * original malformed-file constructor writes/partial initialization are not
 * reproduced. A zero-length input is invalid, not implicit creation. Input is
 * borrowed only for this call and may be NULL only when length is zero. */
mc_region_writer_status mc_region_writer_import(const uint8_t *input, size_t length,
                                                const mc_region_writer_limits *limits,
                                                mc_region_writer **out);

/* Writes opaque, already-compressed zlib bytes with selector 2; stream validity
 * is the caller's responsibility. Both coordinates must be local 0..31. NULL
 * payload is accepted only for length zero. Payload must be disjoint from all
 * storage owned by the writer, including spans borrowed from this owner.
 *
 * Safe-domain successful writes follow the original allocation policy: sectors
 * = (length + 5) / 4096 + 1, even at exact boundaries; 256 or more sectors return
 * RECORD_TOO_LARGE. Same-size records retain their allocation. Otherwise the
 * old run is freed before first-fit search; if no whole run fits, append the
 * entire required run. New sectors are zeroed; reused suffixes and freed bytes
 * are retained. Images never shrink. Timestamp bits are supplied explicitly,
 * independently of world time; no clock or timestamp conversion is hidden.
 *
 * Every failure preserves all image bytes, capacity, allocation state, growth
 * and borrowed spans. All required memory is reserved before commit. This
 * atomic in-memory failure contract does not reproduce partial original I/O
 * failures and makes no filesystem durability or crash-consistency promise. */
mc_region_writer_status mc_region_writer_write_zlib(mc_region_writer *writer, int32_t x, int32_t z,
                                                    const uint8_t *payload, size_t length,
                                                    uint32_t timestamp);

/* NULL returns {NULL, 0}. Otherwise the entire owned image is borrowed and
 * immutable. Discard every borrowed span/view before a successful write or
 * free; failed writes and growth-counter consumption leave spans valid. */
mc_region_writer_bytes mc_region_writer_get_bytes(const mc_region_writer *writer);

/* Returns and resets accumulated image growth, in bytes. Fresh creation starts
 * at 8192, import at zero; only appended sectors add growth. NULL returns zero.
 * No image data or allocation changes, and consuming zero is allowed. */
uint32_t mc_region_writer_consume_growth(mc_region_writer *writer);

/* NULL is accepted. All owned storage is released; discard the caller's pointer
 * and every borrowed span afterward. No save, close or external I/O occurs. */
void mc_region_writer_free(mc_region_writer *writer);

#endif
