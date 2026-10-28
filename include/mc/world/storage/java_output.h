#ifndef MC_WORLD_STORAGE_JAVA_OUTPUT_H
#define MC_WORLD_STORAGE_JAVA_OUTPUT_H
#include "mc/world/storage/java_file.h"

typedef struct mc_java_file_output mc_java_file_output;
typedef struct mc_java_data_output mc_java_data_output;

typedef enum mc_java_output_status {
    MC_JAVA_OUTPUT_OK,
    MC_JAVA_OUTPUT_ARGUMENT,
    MC_JAVA_OUTPUT_STATE,
    MC_JAVA_OUTPUT_BUSY,
    MC_JAVA_OUTPUT_LIMIT,
    MC_JAVA_OUTPUT_ALLOCATION,
    MC_JAVA_OUTPUT_SOURCE_NULL,
    MC_JAVA_OUTPUT_SOURCE_IO,
    MC_JAVA_OUTPUT_NATIVE_PROVIDER,
    MC_JAVA_OUTPUT_NATIVE_FAILURE
} mc_java_output_status;

typedef enum mc_java_output_phase {
    MC_JAVA_OUTPUT_RAW,
    MC_JAVA_OUTPUT_CONSTRUCTING,
    MC_JAVA_OUTPUT_READY,
    MC_JAVA_OUTPUT_FAILED,
    MC_JAVA_OUTPUT_RETIRING
} mc_java_output_phase;

typedef struct mc_java_output_site {
    const char *class_name, *method, *descriptor;
    int pc;
} mc_java_output_site;

typedef struct mc_java_output_cause {
    mc_java_output_status status;
    mc_java_output_site site;
    const char *operation;
    const char *exception_class; /* Only evidenced concrete source exceptions, else NULL. */
    int system_error;
    bool source_exception, consumed;
} mc_java_output_cause;

typedef struct mc_java_output_error {
    mc_java_output_status status;
    mc_java_output_cause cause, suppressed;
    mc_java_output_site callers[4];
    size_t caller_count;
    mc_java_output_site suppressed_callers[4];
    size_t suppressed_caller_count;
    mc_java_file_error file;
    mc_java_file_error suppressed_file;
    size_t committed;
    bool cleanup_failed;
    int cleanup_system_error;
} mc_java_output_error;

typedef struct mc_java_output_options {
    size_t max_owner_bytes;
    uint64_t max_written_bytes; /* Concrete FOS budget; DOS uses owner budget only. */
} mc_java_output_options;

typedef struct mc_java_file_output_state {
    mc_java_output_phase phase;
    size_t holds, owner_bytes, native_write_calls, source_close_calls, native_close_calls;
    uint64_t bytes_committed;
    const mc_java_file *argument;
    const mc_world_info_name *path;
    const void *close_lock, *descriptor;
    const void *pending_close_lock; /* Native failed candidate, never a source field. */
    const mc_java_file_output *descriptor_parent;
    /* native_open is an armed close right, not proof of physical close after
     * a fault. source_close_succeeded describes the method result (including
     * its already-closed return), never a new OS-close acknowledgement. */
    bool active, native_open, source_closed, source_close_reached, source_close_succeeded;
    bool cleanable_registered;
    int last_store_pc;
    const char *last_store_method;
    const mc_java_output_error *failure;
} mc_java_file_output_state;

typedef struct mc_java_data_output_state {
    mc_java_output_phase phase;
    size_t holds, owner_bytes, source_close_calls;
    const mc_java_file_output *out;
    const void *close_lock;
    const void *pending_close_lock;
    const uint8_t *write_buffer, *bytearr;
    int32_t written;
    bool active, closed;
    int last_store_pc;
    const char *last_store_class;
    const char *last_store_method;
    const mc_java_output_error *failure;
} mc_java_data_output_state;

/* Concrete non-append FOS and DOS over that nullable concrete FOS only. No
 * OutputStream callbacks, text encoder, channel API or arbitrary stream subclass.
 * Native allocation is cold; constructors preserve actual JDK17 source stores.
 * Default null SecurityManager and no FileChannel are the admitted profile.
 * File/provider/native environment remains live and exclusive through retirement.
 * Arguments/results/errors/slots must be disjoint. These serial owners reject
 * synchronous reentry; arbitrary concurrent operations are outside admission. */
mc_java_output_options mc_java_output_default_options(void);
mc_java_output_status mc_java_file_output_allocate(mc_java_files *, const mc_java_output_options *,
                                                   mc_java_file_output **, mc_java_output_error *);
mc_java_output_status mc_java_file_output_construct(mc_java_file_output *, mc_java_file *,
                                                    mc_java_output_error *);
mc_java_output_status mc_java_file_output_retain(mc_java_file_output *, mc_java_output_error *);
mc_java_output_status mc_java_file_output_flush(mc_java_file_output *, mc_java_output_error *);
mc_java_output_status mc_java_file_output_source_close(mc_java_file_output *,
                                                       mc_java_output_error *);
mc_java_output_status mc_java_file_output_release(mc_java_file_output **, mc_java_output_error *);
bool mc_java_file_output_inspect(const mc_java_file_output *, mc_java_file_output_state *);
mc_java_output_status mc_java_data_output_allocate(const mc_java_output_options *,
                                                   mc_java_data_output **, mc_java_output_error *);
mc_java_output_status mc_java_data_output_construct(mc_java_data_output *, mc_java_file_output *,
                                                    mc_java_output_error *);
mc_java_output_status mc_java_data_output_retain(mc_java_data_output *, mc_java_output_error *);
mc_java_output_status mc_java_data_output_write_long(mc_java_data_output *, int64_t,
                                                     mc_java_output_error *);
mc_java_output_status mc_java_data_output_flush(mc_java_data_output *, mc_java_output_error *);
mc_java_output_status mc_java_data_output_source_close(mc_java_data_output *,
                                                       mc_java_output_error *);
mc_java_output_status mc_java_data_output_release(mc_java_data_output **, mc_java_output_error *);
bool mc_java_data_output_inspect(const mc_java_data_output *, mc_java_data_output_state *);
/* Release success always consumes/nulls this occurrence, including nonfinal.
 * BUSY/failed cleanup preserves it. Final retirement closes only a still armed
 * native handle, with no source flush/close credit. A close error consumes that
 * native handle but retains its owner/cause for allocation-free retry; the next
 * retry cannot close a reused descriptor. Source close does not consume holds.
 * Inspect failure pointers borrow the same owner until successful final release.
 * ThreadDeath/general Throwable services are not reached by this concrete FOS
 * profile; native policy faults never become fabricated caught Java Throwables. */
#endif
