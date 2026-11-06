#ifndef MC_CORE_JAVA_PRINT_STREAM_H
#define MC_CORE_JAVA_PRINT_STREAM_H

#include "mc/core/java/source_encoder.h"

typedef struct mc_java_print_stream mc_java_print_stream;
typedef struct mc_java_system_output mc_java_system_output;

typedef enum mc_java_print_status {
    MC_JAVA_PRINT_OK,
    MC_JAVA_PRINT_INVALID_ARGUMENT,
    MC_JAVA_PRINT_INVALID_PHASE,
    MC_JAVA_PRINT_BUSY,
    MC_JAVA_PRINT_ALLOCATION_FAILED,
    MC_JAVA_PRINT_LIMIT,
    MC_JAVA_PRINT_NATIVE_FAILURE,
    MC_JAVA_PRINT_UNSUPPORTED_CHARSET,
    MC_JAVA_PRINT_THREAD_UNIMPLEMENTED
} mc_java_print_status;

typedef enum mc_java_print_phase {
    MC_JAVA_PRINT_RAW,
    MC_JAVA_PRINT_CONSTRUCTING,
    MC_JAVA_PRINT_READY,
    MC_JAVA_PRINT_FAILED,
    MC_JAVA_PRINT_RETIRING
} mc_java_print_phase;

typedef struct mc_java_print_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_java_print_site;

typedef struct mc_java_print_error {
    mc_java_print_status status;
    const char *operation;
    mc_java_print_site site;
    /* Finite reached base path: println -> writeln -> nested byte write.
     * These are caller invokes/pending native children, not Java Throwables. */
    mc_java_print_site frames[3];
    size_t frame_count;
    int system_error;
    bool native_policy, source_returned, interrupted_io;
    mc_java_writer_error encoder;
} mc_java_print_error;

typedef struct mc_java_print_profile {
    mc_java_reader_charset charset;
    bool stdout_encoding_present, fallback_default, native_character_device;
    unsigned int stdout_code_page, default_code_page;
} mc_java_print_profile;

typedef struct mc_java_print_state {
    mc_java_print_phase phase;
    size_t holds, byte_count, text_count;
    bool auto_flush, trouble, closing, out_present, char_out_present, text_out_present;
    bool active, interrupted_thread_pending, physical_bytes_known;
    /* physical_bytes is a total only while physical_bytes_known is true;
     * after unknown progress it retains a known subtotal, not a total claim. */
    size_t physical_bytes, byte_write_returns, println_returns;
    mc_java_print_profile profile;
    mc_java_writer_error first_io;
    mc_java_print_error failure;
    mc_java_writer_state encoder;
    mc_java_source_write_result last_write;
} mc_java_print_state;

/* Native allocation is separate from source construction. Failed construction
 * keeps its actual pending encoder/text buffer in this counted owner. These
 * native allocation admissions do not claim Java OOM timing. Output/error
 * storage is mandatory, disjoint and live through callbacks. */
mc_java_print_status mc_java_print_stream_allocate(mc_java_print_stream **, mc_java_print_error *);
/* System.newPrintStream's supported native stdout/default-charset profile,
 * autoFlush=true, real BufferedOutputStream128, text8192 and exact G encoder.
 * No runtime JDK/JAR lookup, locale mutation or fixed UTF8 fallback. A valid
 * unsupported native charset remains unsupported, rather than falling back.
 * Command-line file.encoding/provider overrides and Java subclasses are not
 * admitted. Captures, but never owns/closes, the actual process stdout handle;
 * that handle/fd and its native environment must outlive the stream. */
mc_java_print_status mc_java_print_stream_construct_stdout(mc_java_print_stream *,
                                                           mc_java_print_error *);
/* Actual base PrintStream(OutputStream,boolean,Charset) over the SAME native
 * stdout/buffer pipeline. Explicit charset must be one of the real G codecs;
 * DEFAULT is not an explicit Charset. This does not alter the System profile. */
mc_java_print_status mc_java_print_stream_construct_encoded_stdout(mc_java_print_stream *,
                                                                   mc_java_reader_charset,
                                                                   bool auto_flush,
                                                                   mc_java_print_error *);
mc_java_print_status mc_java_print_stream_retain(mc_java_print_stream *, mc_java_print_error *);
/* NULL text is Java null -> "null"; nonnull zero length is empty. Base-class
 * println uses String.valueOf, writeln, BufferedWriter8192, buffer-only encoder
 * flush and actual autoFlush. Ordinary IOException is swallowed into trouble;
 * source return is separate from physical byte progress. A missing Java thread
 * interrupt child is explicit, never a fabricated stream field or OS signal.
 * Public same-owner data/final-release reentry is BUSY; real private nested
 * encoder->PrintStream.write dispatch uses the held recursive source monitor. */
mc_java_print_status mc_java_print_stream_println(mc_java_print_stream *,
                                                  const mc_java_properties_string_view *,
                                                  mc_java_print_error *);
/* Actual PrintStream.flush flushes its byte output, not the text encoder. */
mc_java_print_status mc_java_print_stream_flush(mc_java_print_stream *, mc_java_print_error *);
bool mc_java_print_stream_inspect(mc_java_print_stream *, mc_java_print_state *);
/* Drop one occurrence. Nonfinal drops work during an active source call.
 * Final native detach does not source-close/flush/encode, close process stdout
 * or consume pending surrogate. BUSY preserves the slot; all other attempted
 * final detach results consume it. RETIRING permits retry, never new holds. */
mc_java_print_status mc_java_print_stream_release(mc_java_print_stream **, mc_java_print_error *);

typedef struct mc_java_system_output_state {
    mc_java_print_phase phase;
    size_t holds;
    const mc_java_print_stream *out, *pending, *pending_old;
    mc_java_print_error failure;
} mc_java_system_output_state;

/* One canonical System.out field per caller-owned native class namespace.
 * This is the selected output bootstrap, not all System.initPhase1. No hidden
 * singleton or per-call default stream is constructed. A failed candidate is
 * retained separately and never published as the field. */
mc_java_print_status mc_java_system_output_allocate(mc_java_system_output **,
                                                    mc_java_print_error *);
mc_java_print_status mc_java_system_output_construct(mc_java_system_output *,
                                                     mc_java_print_error *);
mc_java_print_status mc_java_system_output_retain(mc_java_system_output *, mc_java_print_error *);
/* Fresh actual field read and counted occurrence, including actual NULL.
 * On failure, result unchanged. Caller releases each captured occurrence. */
mc_java_print_status mc_java_system_output_get(mc_java_system_output *, mc_java_print_stream **,
                                               mc_java_print_error *);
/* Native System.setOut0-style field publication, not Java permission checks.
 * Retain new, publish, then retire old; a failed old detach stays in pending_old
 * with the new field visible. No rollback or discarded occurrence. */
mc_java_print_status mc_java_system_output_bind(mc_java_system_output *, mc_java_print_stream *,
                                                mc_java_print_error *);
bool mc_java_system_output_inspect(mc_java_system_output *, mc_java_system_output_state *);
mc_java_print_status mc_java_system_output_release(mc_java_system_output **, mc_java_print_error *);

#endif
