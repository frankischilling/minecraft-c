#ifndef MC_CORE_JAVA_SOURCE_ENCODER_H
#define MC_CORE_JAVA_SOURCE_ENCODER_H
#include "mc/core/java/writer.h"

typedef struct mc_java_source_encoder mc_java_source_encoder;

/* A Java OutputStream void return is NOT a physical committed-byte count.
 * Normal PrintStream return may swallow I/O and consume all logical input
 * while fewer physical bytes reached the borrowed native sink. physical_bytes
 * includes older buffered bytes emitted by this operation, so may exceed the
 * current input length. Native failures may leave progress unknown: then
 * physical_bytes_known=false and physical_bytes is not an actual-count claim.
 * The owner separately retains first/secondary causes. */
typedef struct mc_java_source_write_result {
    bool source_returned, swallowed_io, physical_bytes_known;
    /* Inspection-only library fields. Callback values here are ignored.
     * A rejected outcome clears source_returned/swallowed_io; the rejected
     * reported flags, physical facts and raw swallowed cause remain visible. */
    bool outcome_rejected, rejected_source_returned, rejected_swallowed_io;
    size_t physical_bytes;
    mc_java_writer_error swallowed_error;
} mc_java_source_write_result;

typedef struct mc_java_source_encoder_output {
    void *context;
    mc_java_writer_status (*write)(void *, const uint8_t *, size_t, mc_java_source_write_result *,
                                   mc_java_writer_error *);
    mc_java_writer_status (*flush)(void *, mc_java_writer_error *);
    /* Native detach only, no source close/flush or process-handle close.
     * BUSY preserves context and permits retry; every other result detaches.
     * Borrowed context stays allocated until encoder retirement returns. */
    mc_java_writer_status (*retire)(void *, mc_java_writer_error *);
} mc_java_source_encoder_output;

/* Source callbacks use the existing status vocabulary. A normal return is
 * OK + source_returned=true; an IO throw is its non-OK status without a normal
 * source return. A swallowed IO retains its actual cause separately. Native
 * failures may stop after physical effects; they never grant source success.
 * Buffer-only flush preserves a pending high surrogate. Output/context and
 * error/state storage are live and disjoint. Context callbacks may not destroy
 * borrowed context; same-owner data/retirement reentry returns BUSY. */

/* Share the existing Writer encoding/storage kernel. Legacy Writer layout,
 * allocation charge and physical-commit callback contract stay byte-exact.
 * This new wrapper/context is separately allocated/charged. No parallel codec.
 * Charset uses existing real encoder modes; unsupported profiles stay typed. */
mc_java_writer_status mc_java_source_encoder_create(const mc_java_source_encoder_output *,
                                                    mc_java_reader_charset,
                                                    mc_java_source_encoder **out,
                                                    mc_java_writer_error *);
mc_java_writer_status mc_java_source_encoder_write(mc_java_source_encoder *, const uint16_t *,
                                                   size_t, mc_java_writer_error *);
/* StreamEncoder.flushBuffer: drain bytes only, preserve leftover surrogate,
 * no output.flush. Full flush remains a distinct operation. */
mc_java_writer_status mc_java_source_encoder_flush_buffer(mc_java_source_encoder *,
                                                          mc_java_writer_error *);
mc_java_writer_status mc_java_source_encoder_flush(mc_java_source_encoder *,
                                                   mc_java_writer_error *);
bool mc_java_source_encoder_read_state(const mc_java_source_encoder *, mc_java_writer_state *,
                                       mc_java_source_write_result *last_write);
mc_java_writer_status mc_java_source_encoder_retire(mc_java_source_encoder **,
                                                    mc_java_writer_error *);
#endif
