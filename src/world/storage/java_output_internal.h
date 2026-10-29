#ifndef MC_WORLD_STORAGE_JAVA_OUTPUT_INTERNAL_H
#define MC_WORLD_STORAGE_JAVA_OUTPUT_INTERNAL_H
#include "../../platform/files/byte_output.h"
#include "mc/world/storage/java_output.h"
#include <threads.h>

typedef struct mc_output_lock {
    mtx_t mutex;
    bool ready;
} mc_output_lock;

typedef struct mc_output_descriptor {
    FILE *file;
    mc_java_file_output *parent; /* Source attach identity, noncounted cycle. */
} mc_output_descriptor;

struct mc_java_file_output {
    mc_java_output_options options;
    mc_java_files *files;
    mc_java_file *argument;
    mc_world_info_name *path;
    mc_output_lock *lock;
    mc_output_descriptor *fd;
    mc_java_output_phase phase;
    size_t holds, bytes, writes, source_closes, native_closes;
    uint64_t committed;
    bool active, closed, source_close_reached, source_close_succeeded, cleanable;
    int last_store;
    mc_java_output_error failure;
};

struct mc_java_data_output {
    mc_java_output_options options;
    mc_java_file_output *out;
    mc_output_lock *lock;
    uint8_t *write_buffer, *bytearr;
    mc_java_output_phase phase;
    size_t holds, bytes, source_closes;
    int32_t written;
    bool active, closed;
    int last_store;
    const char *last_class;
    const char *last_method;
    mc_java_output_error failure;
};

mc_java_output_status mc_output_fail(mc_java_output_error *, mc_java_output_status, const char *,
                                     const char *, const char *, int, const char *, int, bool);
void mc_output_caller(mc_java_output_error *, const char *, const char *, const char *, int);
mc_java_output_status mc_output_file_error(mc_java_output_error *, mc_java_file_status,
                                           mc_java_file_error, int);
mc_java_output_status mc_output_lock_create(mc_output_lock **, size_t *, size_t,
                                            mc_java_output_error *, const char *, const char *);
void mc_output_lock_free(mc_output_lock **);
/* The real DOS writeLong retains its known nonnull eight-byte entry.
 * It and public nonnull counted spans share ONE actual write([BII) body.
 * General Java array/null/index overloads are not promised; no callback can
 * supply the FOS result. */
mc_java_output_status mc_output_file_write_long_bytes(mc_java_file_output *, const uint8_t[8],
                                                      mc_java_output_error *);
#endif
