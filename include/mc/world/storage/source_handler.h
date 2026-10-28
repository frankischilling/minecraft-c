#ifndef MC_WORLD_STORAGE_SOURCE_HANDLER_H
#define MC_WORLD_STORAGE_SOURCE_HANDLER_H
#include "mc/core/java/logging.h"
#include "mc/world/storage/gzip_input.h"
#include "mc/world/storage/java_output.h"
#include "mc/world/storage/store.h"

typedef struct mc_world_store_source_classes mc_world_store_source_classes;

typedef enum mc_world_source_status {
    MC_WORLD_SOURCE_OK,
    MC_WORLD_SOURCE_ARGUMENT,
    MC_WORLD_SOURCE_STATE,
    MC_WORLD_SOURCE_BUSY,
    MC_WORLD_SOURCE_LIMIT,
    MC_WORLD_SOURCE_ALLOCATION,
    MC_WORLD_SOURCE_FILE,
    MC_WORLD_SOURCE_OUTPUT,
    MC_WORLD_SOURCE_CLOCK,
    MC_WORLD_SOURCE_LOGGING,
    MC_WORLD_SOURCE_SOURCE_EXCEPTION,
    MC_WORLD_SOURCE_UNIMPLEMENTED,
    MC_WORLD_SOURCE_LATCHED
} mc_world_source_status;

typedef enum mc_world_source_phase {
    MC_WORLD_SOURCE_RAW,
    MC_WORLD_SOURCE_CONSTRUCTING,
    MC_WORLD_SOURCE_READY,
    MC_WORLD_SOURCE_FAILED,
    MC_WORLD_SOURCE_RETIRING
} mc_world_source_phase;

typedef struct mc_world_source_site {
    const char *class_name, *method, *descriptor;
    int pc;
} mc_world_source_site;

typedef struct mc_world_source_error {
    mc_world_source_status status, cause_status;
    const char *operation, *exception_class;
    mc_world_source_site frames[6];
    mc_world_source_site cause_site, prior_site;
    size_t frame_count;
    bool native_policy, source_exception, caught_io, diagnostic_frontier;
    mc_java_file_error file;
    mc_java_output_error output, finally_close;
    mc_java_logging_error logging;
    mc_gzip_error gzip, gzip_close;
    mc_nbt_stream_error input, input_close;
    mc_world_info_error info;
    bool caught_exception, session_mismatch;
    mc_world_info_status string_status;
    int platform_clock_status;
} mc_world_source_error;

typedef struct mc_world_source_options {
    size_t max_owner_bytes, max_literal_bytes, max_allocations;
    mc_java_output_options output;
} mc_world_source_options;

typedef struct mc_world_source_class_state {
    mc_world_source_phase phase;
    size_t holds, handlers, literal_allocations;
    bool active;
    int last_store_pc;
    const mc_java_files *files;
    const mc_java_logging *logging;
    const mc_java_logger *source_a, *pending_logger;
    const mc_world_info_name *minecraft, *players, *data, *session_lock;
    const mc_world_source_error *failure, *cleanup;
} mc_world_source_class_state;

typedef struct mc_world_source_state {
    mc_world_source_phase phase;
    size_t holds, allocations;
    bool active, e_stored, session_entered, session_file_stored, output_stored;
    bool write_returned, close_returned, d_returned, fm_returned, mx_returned;
    int last_store_pc, last_close_pc;
    int64_t source_e;
    mc_java_file *source_b, *source_c, *source_d;
    mc_java_file *pending_file, *session_file;
    mc_java_file_output *pending_fos;
    mc_java_data_output *pending_dos;
    const mc_world_store_source_classes *classes;
    const mc_world_source_error *failure, *cleanup;
} mc_world_source_state;

typedef enum mc_world_source_input_call {
    MC_WORLD_SOURCE_INPUT_NONE,
    MC_WORLD_SOURCE_INPUT_METADATA,
    MC_WORLD_SOURCE_INPUT_SESSION
} mc_world_source_input_call;

/* Borrowed view of one real retained source-call prefix. Slots below own their
 * actual native occurrences inside Store. No independent File/stream/Info may
 * be released through this copied view; use its real retain API first. The
 * DataInput reader is uniquely owned: only its passive state query is allowed
 * through this view, not parse/read/close/release or a fabricated extra hold. */
typedef struct mc_world_source_input_state {
    mc_world_source_input_call call;
    mc_java_file *candidate, *file;
    mc_java_file_stream *stream;
    mc_gzip_input *gzip;
    mc_nbt_stream *reader;
    mc_nbt_document *document;
    mc_world_info_data *data;
    mc_world_info *info;
    bool old_file, exists, input_constructed, parsed, closed, token_read, token_matches;
    bool source_returned, output_transferred;
    int last_pc, close_pc;
    int64_t observed_token;
} mc_world_source_input_state;

bool mc_world_store_source_input_inspect(const mc_world_store *, mc_world_source_input_state *);
/* Explicit native call-prefix retirement for repeated calls and enclosing
 * callers. No source close/finally is credited here. BUSY/failure preserves
 * remaining slots, consumed lower occurrences stay NULL; cleanup retries.
 * A new metadata/session call first performs this same bounded retirement. */
mc_world_source_status mc_world_store_source_input_clear(mc_world_store *, mc_world_source_error *);

mc_world_source_options mc_world_store_source_default_options(void);
/* Cold concrete context: SAME Files and CS service, no logger activation,
 * clock, source literals, directories, regions, Registry, GL or audio. Cold
 * create requires its shared File/service namespaces quiescent and exclusive;
 * serial exclusive calls only, with synchronous reentry rejected before mutation.
 * All error/out/slot storage is live, disjoint and not an owner view. */
mc_world_source_status mc_world_store_source_classes_create(mc_java_files *, mc_java_logging *,
                                                            const mc_world_source_options *,
                                                            mc_world_store_source_classes **,
                                                            mc_world_source_error *);
mc_world_source_status mc_world_store_source_classes_retain(mc_world_store_source_classes *,
                                                            mc_world_source_error *);
mc_world_source_status mc_world_store_source_classes_release(mc_world_store_source_classes **,
                                                             mc_world_source_error *);
mc_world_source_status mc_world_store_source_classes_retire(mc_world_store_source_classes **,
                                                            mc_world_source_error *);
bool mc_world_store_source_classes_inspect(const mc_world_store_source_classes *,
                                           mc_world_source_class_state *);
/* Actual hi new0 activates fm, then allocates the SAME opaque Store. No
 * instance clock or File before construct. Failure preserves out; completed
 * fm class initialization is not rolled back on a later allocation failure. */
mc_world_source_status mc_world_store_source_allocate(mc_world_store_source_classes *,
                                                      mc_world_store **, mc_world_source_error *);
/* Actual mx->fm constructor. Caller holds nullable parent/folder through call;
 * File(parent,folder)18 owns its reached inputs, so NULL folder fails after e8
 * and NEW File12. READY only follows actual d70/fm90/mx7. Native failures and
 * reached IOException.printStackTrace57 retain honest failed prefixes. */
mc_world_source_status mc_world_store_source_construct(mc_world_store *, mc_java_file *,
                                                       mc_world_info_name *, bool,
                                                       mc_world_source_error *);
mc_world_source_status mc_world_store_source_retain(mc_world_store *, mc_world_source_error *);
mc_world_source_status mc_world_store_source_release(mc_world_store **, mc_world_source_error *);
mc_world_source_status mc_world_store_source_retire(mc_world_store **, mc_world_source_error *);
bool mc_world_store_source_inspect(const mc_world_store *, mc_world_source_state *);
bool mc_world_store_is_source(const mc_world_store *);
/* Actual fm.c and fm.b use only real incremental stream owners. Metadata
 * returns a fresh Info occurrence or actual NULL; failure preserves out and
 * the retained child prefix. Source exceptions requiring printStackTrace and
 * the absent us/Throwable owner stop at their reached typed dependency.
 * No legacy whole-file transport or diagnostic World is substituted. */
mc_world_source_status mc_world_store_source_read_info(mc_world_store *, mc_world_info **,
                                                       mc_world_source_error *);
mc_world_source_status mc_world_store_source_check_session(mc_world_store *,
                                                           mc_world_source_error *);
mc_world_source_status mc_world_store_source_loader(mc_world_store *, mc_world_source_error *);
/* Successful release consumes/nulls this occurrence, including nonfinal.
 * BUSY/failure preserves it; final cleanup is allocation-free and retryable.
 * Native retirement never deletes session.lock, flushes shared regions, samples
 * a clock, claims source finally or resets source class initialization. External
 * File/stream occurrences remain independent; borrowed inspection views do not.
 * Every future World/loader consumer must own a real Store occurrence. Legacy
 * void free refuses source owners; checked free delegates counted release. */
#endif
