#ifndef MC_STORAGE_REGION_REGION_H
#define MC_STORAGE_REGION_REGION_H

#include <stddef.h>
#include <stdint.h>

#define MC_REGION_ENTRY_COUNT 1024u
#define MC_REGION_SECTOR_BYTES 4096u
#define MC_REGION_HEADER_BYTES 8192u

typedef enum mc_region_status {
    MC_REGION_OK = 0,
    MC_REGION_INVALID_ARGUMENT,
    MC_REGION_TRUNCATED_HEADER,
    MC_REGION_LIMIT,
    MC_REGION_INVALID_COORDINATE,
    MC_REGION_MISSING,
    MC_REGION_INVALID_RECORD,
    MC_REGION_UNSUPPORTED_COMPRESSION
} mc_region_status;

typedef enum mc_region_issue {
    MC_REGION_ISSUE_HEADER_SECTOR = 1u << 0,
    MC_REGION_ISSUE_EMPTY_EXTENT = 1u << 1,
    MC_REGION_ISSUE_EXTENT_BOUNDS = 1u << 2,
    MC_REGION_ISSUE_OVERLAP = 1u << 3,
    MC_REGION_ISSUE_INVALID_LENGTH = 1u << 4,
    MC_REGION_ISSUE_RECORD_BOUNDS = 1u << 5,
    MC_REGION_ISSUE_UNSUPPORTED_SELECTOR = 1u << 6
} mc_region_issue;

typedef struct mc_region_limits {
    size_t max_input_bytes;
} mc_region_limits;

typedef struct mc_region_entry {
    uint32_t location;
    uint32_t timestamp;
    uint32_t issues;
    uint32_t record_length; /* Raw signed-on-wire bits, zero if not inspected. */
    uint64_t byte_offset;
    uint64_t extent_bytes;
    uint8_t selector; /* Only inspected when location and length are valid. */
} mc_region_entry;

typedef struct mc_region_view {
    const uint8_t *input;
    size_t length;
    size_t trailing_bytes; /* Incomplete final sector; never padded or repaired. */
    size_t present_entries;
    size_t invalid_entries;
    mc_region_entry entries[MC_REGION_ENTRY_COUNT];
} mc_region_view;

typedef struct mc_region_payload {
    const uint8_t *data;
    size_t length;
    uint8_t selector; /* 1 = gzip, 2 = zlib. */
} mc_region_payload;

/* The module allocates no heap memory. Default input limit is 1 GiB. */
mc_region_limits mc_region_default_limits(void);
const char *mc_region_status_string(mc_region_status status);

/* NULL limits selects defaults. Input may be NULL only when length is zero.
 * Requires a complete 8192-byte header.
 * Successful open retains all raw location/timestamp bits and diagnoses each
 * present record independently. Malformed records and partial final sectors
 * are reported in the view rather than making open fail. Overlap marks both
 * affected entries, including overlaps with otherwise invalid extents.
 * This is a borrowed, immutable view: input must remain valid and unchanged
 * until the view and all payloads are discarded. Do not modify view fields.
 * out is required and must not overlap input or limits. Failure leaves *out
 * unchanged. No creation, repair, decompression, NBT, schema or file I/O occurs. */
mc_region_status mc_region_open(const uint8_t *input, size_t length, const mc_region_limits *limits,
                                mc_region_view *out);

/* Only views from successful open may be used. Both local coordinates must
 * be in 0..31; there is no masking, global coordinate or filename mapping.
 * Returns NULL for a NULL view/invalid coordinates, otherwise metadata even
 * for missing/malformed records. location == 0 denotes missing. */
const mc_region_entry *mc_region_entry_at(const mc_region_view *view, int32_t x, int32_t z);

/* Selects a fully validated, nonoverlapping record. Distinguishes missing,
 * invalid and unsupported selectors. *out is unchanged on failure. Its storage
 * must not overlap the view or input. Zero-length payloads are valid spans;
 * decompression/root parsing are separate operations with their own budgets. */
mc_region_status mc_region_record(const mc_region_view *view, int32_t x, int32_t z,
                                  mc_region_payload *out);

#endif
