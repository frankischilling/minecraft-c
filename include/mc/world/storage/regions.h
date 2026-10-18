#ifndef MC_WORLD_STORAGE_REGIONS_H
#define MC_WORLD_STORAGE_REGIONS_H

#include "mc/storage/compression/compression.h"
#include "mc/storage/region/writer.h"
#include "mc/world/storage/status.h"

#define MC_WORLD_REGIONS_CACHE_SIZE 256u

typedef struct mc_world_regions mc_world_regions;

typedef struct mc_world_regions_limits {
    size_t max_cached_bytes;
    mc_region_writer_limits image;
    mc_compression_limits compression;
} mc_world_regions_limits;

typedef struct mc_world_regions_usage {
    size_t count;
    size_t bytes;
} mc_world_regions_usage;

/* Defaults: 128 MiB retained cache, 64 MiB region image/128 MiB writer peak,
 * and the default compression limits. Cache bytes include this owner, retained
 * image lengths and path strings. Writer/compression limits independently bound
 * temporary operations; they are not a shared process-allocation budget.
 * Temporary file-read/result copies add at most two configured image sizes;
 * native path scratch is bounded to 32768 UTF-8 bytes per path.
 */
mc_world_regions_limits mc_world_regions_default_limits(void);
mc_world_storage_status mc_world_regions_create(const mc_world_regions_limits *limits,
                                                mc_world_regions **out,
                                                mc_world_storage_error *error);

/* Explicit read/write transport for caller-selected dimension chunk roots.
 * Paths are NUL-terminated UTF-8, nonempty and bounded to32768 bytes. Windows
 * uses native wide APIs. The caller owns exclusive filesystem access and must
 * use one stable lexical path per physical root, without aliases. Coordinates
 * use signed floor division by32 and local low5bits across the full int32 range.
 *
 * Cache keys are complete lexical region paths. Live hits retain their image;
 * a miss at 256 entries discards the entire cache before opening another file.
 * Unlike Java SoftReferences, C retains entries until this explicit clear.
 * Reads may create region directories and fresh 8192-byte region files, matching
 * the original read/write region route. Existing malformed files are never
 * padded, repaired or overwritten during a read.
 *
 * read returns one owned decompressed record; free with
 * mc_compression_buffer_free. Missing slot returns MISSING and leaves out
 * unchanged. Gzip and zlib selectors are supported. No NBT schema is applied.
 * Neither stream nor region/compression byte identity is claimed.
 *
 * write compresses one raw record as zlib, preserves unrelated/dead region bytes
 * and uses explicit timestamp bits. Successful growth returns and clears the
 * region's accumulated byte growth, including a newly created header. There is
 * no world-time, metadata accounting or session-token check in this transport.
 *
 * Writes reserve memory and stage a sibling file before checked replacement.
 * A failed write does not intentionally replace the target or publish a changed
 * cached image. This differs from swallowed original I/O failures; no fsync,
 * power-loss durability or crash-safe recovery is promised. A failed replacement
 * may leave the owned temporary file if cleanup itself fails, reported as an
 * error. Only files created by the operation may be removed during cleanup.
 *
 * All calls require exclusive owner access. out/growth are required and remain
 * unchanged on failure. Result/error/limits and source storage must be disjoint.
 * Filesystem setup and cache clearing may occur before an error is returned.
 */
mc_world_storage_status mc_world_regions_read(mc_world_regions *regions, const char *chunk_root,
                                              int32_t x, int32_t z, mc_compression_buffer *out,
                                              mc_world_storage_error *error);
mc_world_storage_status mc_world_regions_write(mc_world_regions *regions, const char *chunk_root,
                                               int32_t x, int32_t z, const uint8_t *input,
                                               size_t length, uint32_t timestamp, uint32_t *growth,
                                               mc_world_storage_error *error);

/* Clears every retained image/path and growth counter. No implicit dirty work
 * exists: successful writes commit synchronously. Existing decompressed result
 * buffers remain independently owned. NULL clear/free are accepted; NULL usage
 * returns zero. Retained cache count/bytes are diagnostic, not disk usage. */
void mc_world_regions_clear(mc_world_regions *regions);
mc_world_regions_usage mc_world_regions_get_usage(const mc_world_regions *regions);
void mc_world_regions_free(mc_world_regions *regions);

#endif
