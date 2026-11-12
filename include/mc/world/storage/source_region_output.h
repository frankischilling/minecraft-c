#ifndef MC_WORLD_STORAGE_SOURCE_REGION_OUTPUT_H
#define MC_WORLD_STORAGE_SOURCE_REGION_OUTPUT_H
#include "mc/world/storage/source_regions.h"

typedef struct mc_world_source_region_output mc_world_source_region_output;

typedef struct mc_world_source_region_output_error {
    mc_world_source_region_error region;
    mc_compressed_output_error stream;
} mc_world_source_region_output_error;

typedef struct mc_world_source_region_output_state {
    mc_java_output_phase phase;
    bool active, closed, finished, deflater_ended, commit_entered, commit_returned, oversized;
    const mc_world_source_regions *regions;
    const void *region_identity;
    const mc_java_random_access *random_access;
    const mc_java_file *region_file;
    const int32_t *offsets, *timestamps;
    const bool *free_sectors;
    size_t sectors, region_holds;
    int32_t growth;
    int32_t x, z, written, sector, required, timestamp;
    size_t owner_bytes, peak_owner_bytes, buffer_capacity, source_close_calls;
    size_t sectors_appended, free_updates;
    uint64_t raw_bytes;
    const uint8_t *compressed_bytes;
    size_t compressed_length;
    const mc_world_source_region_output_error *failure;
} mc_world_source_region_output_state;

/* Concrete rj.d/qj.b: SAME cached QJ, Hz with initial byte[8096], default
 * zlib-wrapped DeflaterOutputStream and outer DOS. NULL options supplies a
 * 1MiB native owner bound; compressed bytes also obey the shared region bound.
 * Construction returns its retained partial owner on failure. Native allocation
 * ceilings are policy, not Java heap/OOM timing. Serial, exclusive calls only;
 * caller roots/options/outputs/errors are live and disjoint. */
mc_java_output_options mc_world_source_region_output_default_options(void);
mc_world_source_status mc_world_source_regions_output(mc_world_source_regions *, mc_java_file *,
                                                      int32_t, int32_t,
                                                      const mc_java_output_options *,
                                                      mc_world_source_region_output **,
                                                      mc_world_source_region_output_error *);
/* Native acyclic-tree validation precedes primitive emission. Source callers
 * must already have assembled their actual fresh root/Level. No Chunk/Entity
 * serializer, session check, source finally or GZIP wrapper is invented here.
 * Input stays live through any later source close after a failed write, since
 * the actual deflater can retain a borrowed ByteArray span. Native release does
 * not read it. Supplied compound order and canonical Java NaNs are retained. */
mc_world_source_status
mc_world_source_region_output_write_compound(mc_world_source_region_output *, const mc_nbt_tag *,
                                             const mc_nbt_limits *,
                                             mc_world_source_region_output_error *);
/* Source outer close publishes closed, finishes zlib, ends default deflate and
 * invokes Hz.close -> SAME QJ ordered RAF commit. It does not close RAF. A
 * later source cache flush can close RAF while Hz remains alive. QJ's caught
 * IOException printing remains an explicit dependency; no successful catch is
 * fabricated. Repeated outer close returns without replaying commit. */
mc_world_source_status
mc_world_source_region_output_source_close(mc_world_source_region_output *,
                                           mc_world_source_region_output_error *);
/* Native cleanup only: no finish/flush/commit, timestamp or growth consumption.
 * Failed lower cleanup retains remaining slots; consumed slots stay consumed. */
mc_world_source_status mc_world_source_region_output_release(mc_world_source_region_output **,
                                                             mc_world_source_region_output_error *);
/* Passive spans are borrowed only until any region/output mutation or release. */
bool mc_world_source_region_output_inspect(const mc_world_source_region_output *,
                                           mc_world_source_region_output_state *);
/* Actual rj.b lookup followed by QJ.g read/reset; no World Info mutation. */
mc_world_source_status mc_world_source_regions_consume_growth(mc_world_source_regions *,
                                                              mc_java_file *, int32_t, int32_t,
                                                              int32_t *,
                                                              mc_world_source_region_error *);
#endif
