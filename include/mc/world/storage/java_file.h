#ifndef MC_WORLD_STORAGE_JAVA_FILE_H
#define MC_WORLD_STORAGE_JAVA_FILE_H
#include "mc/world/info/info.h"
#include <stddef.h>
#include <stdint.h>

typedef struct mc_java_files mc_java_files;
typedef struct mc_java_file mc_java_file;
typedef struct mc_java_file_array mc_java_file_array;
typedef struct mc_java_file_stream mc_java_file_stream;

typedef enum mc_java_file_status {
    MC_JAVA_FILE_OK,
    MC_JAVA_FILE_INVALID_ARGUMENT,
    MC_JAVA_FILE_STATE_ERROR,
    MC_JAVA_FILE_BUSY,
    MC_JAVA_FILE_LIMIT,
    MC_JAVA_FILE_ALLOCATION_FAILED,
    MC_JAVA_FILE_SOURCE_NULL_STRING,
    MC_JAVA_FILE_SOURCE_NULL_FILE,
    MC_JAVA_FILE_IO_ERROR,
    MC_JAVA_FILE_UNSUPPORTED_PROVIDER
} mc_java_file_status;

typedef enum mc_java_file_phase {
    MC_JAVA_FILE_RAW,
    MC_JAVA_FILE_CONSTRUCTING,
    MC_JAVA_FILE_READY,
    MC_JAVA_FILE_FAILED,
    MC_JAVA_FILE_RETIRING
} mc_java_file_phase;

typedef struct mc_java_file_error {
    mc_java_file_status status;
    const char *operation;
    int system_error;
} mc_java_file_error;

typedef struct mc_java_file_options {
    size_t max_path_units, max_string_bytes, max_entries;
} mc_java_file_options;

typedef struct mc_java_file_state {
    mc_java_file_phase phase;
    size_t holds, prefix_length;
    mc_world_info_name *path; /* exact nullable source String identity, borrowed */
    size_t exists_calls, mkdir_calls, canonicalize_calls;
    int last_native_error;
    bool active, has_failed_array;
} mc_java_file_state;

typedef struct mc_java_file_stream_state {
    mc_java_file_phase phase;
    /* native_open is an armed descriptor, not physical-close proof after a fault. */
    bool active, native_open, source_close_reached, source_close_succeeded;
    size_t holds, read_calls, native_read_calls, byte_read_calls;
    uint64_t native_bytes_read;
} mc_java_file_stream_state;

/* Shared native lifetime/literal scope, not a surrogate Java FileSystem body.
 * Native filename bridge currently requires UTF8-representable paths; POSIX
 * operations first admit the actual environment locale through the existing
 * native charset discovery and report the non-UTF8 provider as unfinished.
 * The native filename environment is borrowed unchanged from process startup
 * through the final namespace hold; Java startup-JNU/environment mutation is
 * a separate unqualified provider dependency.
 * All UTF16 units, including NUL/lone surrogates, survive construction and inspection;
 * any unfinished encoding is reported at reached native operation. No IO or
 * default-directory selection occurs here. Options use separate per-string,
 * per-path and enumeration bounds, not a process-wide heap budget. */
/* Namespace lifetime may overlap on distinct already-held occurrences. Its
 * counters/terminal flags are atomic; ordinary nonfinal retain/release has no
 * fallible mutex call. A legitimate worker is not rejected by unrelated
 * foreground construction. Only same-thread lazy-cache reentry reports BUSY.
 * The real mutex protects lazy default-parent/name publication. Entrants and
 * waiters must already have an external counted occurrence: leases cannot
 * rescue stale raw pointers. Same File/array/stream mutation stays exclusive;
 * Stats joins before foreground final File/name destruction. Generic concurrent
 * InfoName/Data operations are not admitted. Cache-unlock uncertainty keeps
 * the unique final namespace slot quarantined; nonfinal releases still consume
 * their occurrence without pretending final cleanup or source success. */
mc_java_file_options mc_java_file_default_options(void);
mc_java_file_status mc_java_files_create(const mc_java_file_options *options, mc_java_files **out,
                                         mc_java_file_error *error);
mc_java_file_status mc_java_files_retain(mc_java_files *files, mc_java_file_error *error);
mc_java_file_status mc_java_files_release(mc_java_files **occurrence, mc_java_file_error *error);
mc_java_file_status mc_java_file_allocate(mc_java_files *files, mc_java_file **out,
                                          mc_java_file_error *error);
/* Construct once; attempt latches FAILED and retains its native pending input.
 * child NULL is a source NPE. Parent NULL is valid and uses normalized child.
 * Immutable path ownership uses existing mc_world_info_name; no parallel
 * String wrapper, interning-by-content or early native encoding. */
mc_java_file_status mc_java_file_construct_string(mc_java_file *raw, mc_world_info_name *path,
                                                  mc_java_file_error *error);
mc_java_file_status mc_java_file_construct_child(mc_java_file *raw, mc_java_file *parent,
                                                 mc_world_info_name *child,
                                                 mc_java_file_error *error);
/* Actual File(String,String), sharing the canonical constructor. Child NULL
 * fails before reading parent. Null parent normalizes only child; empty parent
 * resolves default-parent BEFORE normalizing child; nonempty parent normalizes
 * parent BEFORE child. No temporary parent File or prejoined path surrogate.
 * Failed nonnull argument holds remain native-only, never source path fields. */
mc_java_file_status mc_java_file_construct_strings(mc_java_file *raw,
                                                   mc_world_info_name *nullable_parent,
                                                   mc_world_info_name *nonnull_child,
                                                   mc_java_file_error *error);
mc_java_file_status mc_java_file_retain(mc_java_file *file, mc_java_file_error *error);
/* Success consumes/nulls this occurrence even with other holds. BUSY/failure
 * preserves it; final resource cleanup is guarded and retryable. */
mc_java_file_status mc_java_file_release(mc_java_file **occurrence, mc_java_file_error *error);
mc_java_file_status mc_java_file_read_state(const mc_java_file *file, mc_java_file_state *out,
                                            mc_java_file_error *error);
mc_world_info_name *mc_java_file_path(const mc_java_file *file); /* borrowed */
mc_java_files *mc_java_file_namespace(const mc_java_file *file); /* borrowed native scope */
mc_java_file_status mc_java_file_get_name(mc_java_file *file, mc_world_info_name **owned,
                                          mc_java_file_error *error);
mc_java_file_status mc_java_file_exists(mc_java_file *file, bool *out, mc_java_file_error *error);
/* Independent current attribute calls, not a cached/grouped snapshot. Ordinary
 * provider absence/failure returns source false/0. Native encoding/budget/
 * unfinished-provider/resource-policy failures preserve out and typed error.
 * File has no source close; stream closure does not retire this immutable File.
 * READY/exclusive admission is identical to is_directory. */
mc_java_file_status mc_java_file_is_file(mc_java_file *file, bool *out, mc_java_file_error *error);
mc_java_file_status mc_java_file_length(mc_java_file *file, int64_t *out,
                                        mc_java_file_error *error);
mc_java_file_status mc_java_file_last_modified(mc_java_file *file, int64_t *out,
                                               mc_java_file_error *error);
mc_java_file_status mc_java_file_is_directory(mc_java_file *file, bool *out,
                                              mc_java_file_error *error);
/* Separate reached Java operations. mkdir makes one native attempt only.
 * Ordinary false is successful source completion; native bounds/providers
 * preserve out. rename borrows both same-namespace READY Files exclusively. */
mc_java_file_status mc_java_file_mkdir(mc_java_file *file, bool *out, mc_java_file_error *error);
mc_java_file_status mc_java_file_rename_to(mc_java_file *file, mc_java_file *destination, bool *out,
                                           mc_java_file_error *error);
mc_java_file_status mc_java_file_mkdirs(mc_java_file *file, bool *out, mc_java_file_error *error);
/* NULL is real File.listFiles failure/non-directory; nonnull empty array has a
 * distinct identity. OS enumeration order is retained. Native failure leaves
 * out unchanged and a failed candidate on File until clear_failure/release. */
mc_java_file_status mc_java_file_list(mc_java_file *file, mc_java_file_array **out,
                                      mc_java_file_error *error);
mc_java_file_status mc_java_file_clear_failure(mc_java_file *file, mc_java_file_error *error);
size_t mc_java_file_array_length(const mc_java_file_array *array);
mc_java_file *mc_java_file_array_at(const mc_java_file_array *array, size_t index); /* borrowed */
mc_java_file_status mc_java_file_array_retain(mc_java_file_array *array, mc_java_file_error *error);
mc_java_file_status mc_java_file_array_release(mc_java_file_array **occurrence,
                                               mc_java_file_error *error);

typedef struct mc_java_file_read {
    size_t count;
    bool eof;
} mc_java_file_read;

/* A real counted descriptor FileInputStream owner. Allocation/construct/open are separate so a
 * failed unpublished original child can be retained; no placeholder fd. */
mc_java_file_status mc_java_file_stream_allocate(mc_java_files *files, mc_java_file_stream **out,
                                                 mc_java_file_error *error);
mc_java_file_status mc_java_file_stream_construct(mc_java_file_stream *raw, mc_java_file *file,
                                                  mc_java_file_error *error);
/* Each call makes real unbuffered descriptor intake, retrying EINTR only.
 * No read-ahead, hidden stdio cursor or fake EOF. Input supports actual regular
 * files; unsupported providers remain distinct from a source IOException.
 * read/available require a constructed owner and exclusive access. */
mc_java_file_status mc_java_file_stream_retain(mc_java_file_stream *stream,
                                               mc_java_file_error *error);
mc_java_file_status mc_java_file_stream_read(mc_java_file_stream *stream, uint8_t *buffer,
                                             size_t capacity, mc_java_file_read *out,
                                             mc_java_file_error *error);
mc_java_file_status mc_java_file_stream_read_byte(mc_java_file_stream *stream, int *out,
                                                  mc_java_file_error *error);
mc_java_file_status mc_java_file_stream_available(mc_java_file_stream *stream, size_t *out,
                                                  mc_java_file_error *error);
mc_java_file_status mc_java_file_stream_source_close(mc_java_file_stream *stream,
                                                     mc_java_file_error *error);
mc_java_file_status mc_java_file_stream_release(mc_java_file_stream **occurrence,
                                                mc_java_file_error *error);
mc_java_file_status mc_java_file_stream_read_state(const mc_java_file_stream *stream,
                                                   mc_java_file_stream_state *out,
                                                   mc_java_file_error *error);
#endif
