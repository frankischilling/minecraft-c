#ifndef MC_WORLD_STORAGE_COMPRESSED_OUTPUT_H
#define MC_WORLD_STORAGE_COMPRESSED_OUTPUT_H

#include "mc/storage/nbt/nbt.h"
#include "mc/world/storage/java_output.h"

typedef struct mc_compressed_output mc_compressed_output;

typedef struct mc_compressed_output_error {
    mc_java_output_error output;
    mc_nbt_error nbt;
    int zlib_status;
} mc_compressed_output_error;

typedef struct mc_compressed_output_state {
    mc_java_output_phase phase;
    bool active, closed, header_written, finished, file_closed;
    bool gzip_closed, deflater_ended, data_output_constructed;
    const mc_java_file_output *file;
    int32_t written_count;
    uint64_t raw_bytes, compressed_bytes;
    size_t owner_bytes, peak_owner_bytes, source_close_calls, gzip_write_calls;
    size_t utf_buffer_bytes;
    uint32_t crc;
    const mc_compressed_output_error *failure;
} mc_compressed_output_state;

/* One serial concrete DataOutputStream/GZIPOutputStream/FOS composition.
 * Allocation is cold. Construct retains the actual FOS, creates raw default
 * deflate with a 512-byte buffer and writes the JDK17 ten-byte header before
 * constructing the outer DataOutputStream fields. No arbitrary OutputStream
 * subclass, source Throwable identity or whole-file compression buffer exists.
 * Native owner/zlib/UTF allocations share max_owner_bytes; the separately held
 * FOS keeps its own transport budget. Slots/results/errors are disjoint. */
/* NULL options uses the ordinary Java-output defaults, including their 64KiB
 * owner bound. That bound cannot construct a default zlib deflater here;
 * callers admitting compressed output must supply an adequate explicit bound.
 * A source handler uses its independently configured owner bound (default1MiB). */
mc_java_output_status mc_compressed_output_allocate(const mc_java_output_options *,
                                                    mc_compressed_output **,
                                                    mc_compressed_output_error *);
mc_java_output_status mc_compressed_output_construct(mc_compressed_output *, mc_java_file_output *,
                                                     mc_compressed_output_error *);
/* Native acyclic-tree safety validation precedes emission, after the already
 * reached constructor/header. The bounded encoder then streams the actual NBT
 * primitive writes, canonical Java NaNs, modified UTF8 and empty-list Byte
 * type. Compound traversal keeps supplied order; Java HashMap byte identity,
 * arbitrary malformed source NBT and Java heap/OOM timing are not claimed.
 * Input/limits stay immutable and live throughout this exclusive call. A
 * failed stream write can leave its current borrowed NBT array on Deflater;
 * keep the tree/array storage live through the enclosing source finally-close
 * or checked native release. No later source close may use expired input. */
mc_java_output_status mc_compressed_output_write_compound(mc_compressed_output *,
                                                          const mc_nbt_tag *, const mc_nbt_limits *,
                                                          mc_compressed_output_error *);
/* Actual outer Filter.close publishes closed before flush, then finishes the
 * GZIP stream and closes the real FOS. A source finish failure ends deflate but
 * does not source-close FOS. Repeated outer close returns without retrying any
 * failed child. Native failures do not invent source catch/finally execution.
 * The enclosing as.a write-failure finally belongs to its actual caller. */
mc_java_output_status mc_compressed_output_source_close(mc_compressed_output *,
                                                        mc_compressed_output_error *);
/* Checked native retirement ends deflate and releases the real FOS occurrence
 * without source finish/flush/close. Failed lower retirement retains this owner
 * and its remaining slots. Earlier consumed slots stay consumed on retry. */
mc_java_output_status mc_compressed_output_release(mc_compressed_output **,
                                                   mc_compressed_output_error *);
bool mc_compressed_output_inspect(const mc_compressed_output *, mc_compressed_output_state *);

#endif
