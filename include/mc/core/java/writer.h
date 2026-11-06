#ifndef MC_CORE_JAVA_WRITER_H
#define MC_CORE_JAVA_WRITER_H

#include "mc/core/java/reader.h"

typedef struct mc_java_writer mc_java_writer;

typedef enum mc_java_writer_status {
    MC_JAVA_WRITER_OK = 0,
    MC_JAVA_WRITER_INVALID_ARGUMENT,
    MC_JAVA_WRITER_ALLOCATION_FAILED,
    MC_JAVA_WRITER_LIMIT,
    MC_JAVA_WRITER_IO_ERROR,
    MC_JAVA_WRITER_INTERRUPTED_IO,
    MC_JAVA_WRITER_UNSUPPORTED_CHARSET,
    MC_JAVA_WRITER_BUSY
} mc_java_writer_status;

typedef struct mc_java_writer_error {
    mc_java_writer_status status;
    int system_error;
    const char *operation;
    /* Secondary native cleanup after failed unbuffered file admission. The
     * primary admission cause remains above. Source-close suppression is
     * separately observable in mc_java_writer_state.secondary_close_error. */
    bool native_cleanup_failed;
    int native_cleanup_system_error;
} mc_java_writer_error;

typedef enum mc_java_writer_mode {
    MC_JAVA_WRITER_OUTPUT_STREAM,
    MC_JAVA_WRITER_PRINT
} mc_java_writer_mode;

typedef enum mc_java_writer_lines { MC_JAVA_WRITER_LF, MC_JAVA_WRITER_CRLF } mc_java_writer_lines;

typedef struct mc_java_writer_profile {
    /* Share the existing charset vocabulary/discovery, not Reader decoding.
     * DEFAULT resolves the native host profile without changing its locale.
     * A recorded Java file.encoding/defaultCharset must be passed explicitly. */
    mc_java_reader_charset charset;
    mc_java_writer_mode mode;
    mc_java_writer_lines lines;
} mc_java_writer_profile;

/* Real byte-output seam. Every callback is required and uses context.
 * write may return OK with 1..length bytes, or IO_ERROR/INTERRUPTED_IO with
 * 0..length bytes actually committed. Partial success is completed inside one
 * Java OutputStream.write operation. An error ends that operation immediately;
 * its count never advances StreamEncoder's flipped ByteBuffer position.
 * Callback failure may have changed the actual destination irreversibly.
 * close retires its sink resource even when reporting an error; the caller
 * retains context memory until native owner retirement completes. No callback
 * may reenter or release the same owner. Diagnostic operation strings supplied
 * by callbacks must remain live through retirement; retained causes borrow
 * those strings. Missing callbacks are rejected. */
typedef struct mc_java_writer_output {
    void *context;
    mc_java_writer_status (*write)(void *context, const uint8_t *bytes, size_t length,
                                   size_t *committed, mc_java_writer_error *error);
    mc_java_writer_status (*flush)(void *context, mc_java_writer_error *error);
    mc_java_writer_status (*close)(void *context, mc_java_writer_error *error);
} mc_java_writer_output;

typedef struct mc_java_writer_state {
    mc_java_reader_charset charset;
    bool encoder_closed;
    bool print_writer_out_present;
    bool trouble;
    /* A PrintWriter write/newline InterruptedIOException requests a Java
     * thread reinterrupt. This is an explicit native observation, not an OS
     * thread signal. Flush/close interrupted failures set trouble instead. */
    bool interrupted_write;
    bool source_close_reached;
    bool sink_close_attempted;
    bool native_file;
    mc_java_writer_error first_io_error;
    mc_java_writer_error secondary_close_error;
} mc_java_writer_state;

typedef struct mc_java_writer_retirement {
    bool source_close_reached;
    bool additional_sink_close;
} mc_java_writer_retirement;

/* One independently owned, fixed 8192-byte encoder. The output table is copied;
 * its context remains borrowed. Creation does no output/flush/close. Success
 * publishes out; failure leaves it unchanged. Native allocation failure is not
 * a model of Java OutOfMemoryError timing. UTF-8, windows-1252, ISO-8859-1 and
 * US-ASCII use replacement '?' for malformed/unmappable input, with one pending
 * high surrogate retained across write and flush calls. No BOM is emitted. */
mc_java_writer_status mc_java_writer_create(const mc_java_writer_output *output,
                                            const mc_java_writer_profile *profile,
                                            mc_java_writer **out, mc_java_writer_error *error);

/* The reached non-append FileWriter adapter: open/create/truncate before the
 * caller formats any setting. Binary, unbuffered stdio adds no byte buffering
 * or newline translation. Paths follow the existing UTF-8 native filename
 * convention, including strict conversion to Windows UTF-16. No parent mkdir,
 * temp file, backup, transaction, rename, fsync or rollback is added. Open paths
 * are bounded to 1 MiB; native path/error/sharing profiles need real-file
 * reference qualification. Invalid/native allocation admissions may precede
 * opening; their order does not claim Java exception/OOM scheduling. */
mc_java_writer_status mc_java_file_writer_open(const char *path,
                                               const mc_java_writer_profile *profile,
                                               mc_java_writer **out, mc_java_writer_error *error);

/* Counted UTF-16, including NUL and lone surrogates. NULL/zero is empty, not a
 * Java null String. Length above INT32_MAX is a native LIMIT before reads.
 * Direct mode propagates IO. Print mode swallows ordinary IO into trouble and
 * write/newline interruption into interrupted_write, as PrintWriter does.
 * Native argument/limit/callback-contract errors always propagate. No failure
 * rolls back output, buffer state or earlier source effects. */
mc_java_writer_status mc_java_writer_write(mc_java_writer *writer, const uint16_t *units,
                                           size_t length, mc_java_writer_error *error);

/* Print-mode String overloads; NULL view prints the four units "null".
 * println attempts the line separator after a swallowed text-write failure.
 * It never auto-flushes: this is PrintWriter(Writer), autoFlush=false. */
mc_java_writer_status mc_java_writer_print(mc_java_writer *writer,
                                           const mc_java_properties_string_view *text,
                                           mc_java_writer_error *error);
mc_java_writer_status mc_java_writer_println(mc_java_writer *writer,
                                             const mc_java_properties_string_view *text,
                                             mc_java_writer_error *error);
mc_java_writer_status mc_java_writer_flush(mc_java_writer *writer, mc_java_writer_error *error);
mc_java_writer_status mc_java_writer_close(mc_java_writer *writer, mc_java_writer_error *error);

/* Print-mode checkError flushes while its underlying reference is present;
 * GameSettings does not call this. It returns sticky trouble through out.
 * PrintWriter retains its reference after failed close although its underlying
 * encoder is already closed; the next close can clear that reference. */
mc_java_writer_status mc_java_writer_check_error(mc_java_writer *writer, bool *out,
                                                 mc_java_writer_error *error);
bool mc_java_writer_read_state(const mc_java_writer *writer, mc_java_writer_state *out);

/* Native cleanup is separate from source close. If source did not close the
 * sink, retire calls only sink.close: no encoding, flush, final replacement or
 * invented source finally. The close result always propagates here, even in
 * Print mode. The owner is freed and *writer=NULL after an attempted cleanup;
 * borrowed output context is never freed. A BUSY owner remains live. NULL
 * *writer is harmless. Retirement/error outputs may not alias owner storage. */
mc_java_writer_status mc_java_writer_retire(mc_java_writer **writer, mc_java_writer_retirement *out,
                                            mc_java_writer_error *error);

#endif
