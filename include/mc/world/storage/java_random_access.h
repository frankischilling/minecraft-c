#ifndef MC_WORLD_STORAGE_JAVA_RANDOM_ACCESS_H
#define MC_WORLD_STORAGE_JAVA_RANDOM_ACCESS_H
#include "mc/world/storage/java_file.h"

typedef struct mc_java_random_access mc_java_random_access;

typedef struct mc_java_random_access_state {
    mc_java_file_phase phase;
    mc_java_file *file;
    bool active, native_open, close_reached, close_succeeded;
    size_t read_calls, write_calls, seek_calls;
    uint64_t bytes_read, bytes_written;
} mc_java_random_access_state;

/* Separate NEW and actual RandomAccessFile(File,"rw"). The same immutable
 * File and namespace are held through descriptor retirement. No UTF8 path
 * proxy, buffered stdio cursor or whole-file image is used. Exclusive calls. */
mc_java_file_status mc_java_random_access_allocate(mc_java_random_access **, mc_java_file_error *);
mc_java_file_status mc_java_random_access_construct(mc_java_random_access *, mc_java_file *,
                                                    mc_java_file_error *);
mc_java_file_status mc_java_random_access_length(mc_java_random_access *, int64_t *,
                                                 mc_java_file_error *);
mc_java_file_status mc_java_random_access_seek(mc_java_random_access *, int64_t,
                                               mc_java_file_error *);
mc_java_file_status mc_java_random_access_read(mc_java_random_access *, uint8_t *, size_t,
                                               mc_java_file_read *, mc_java_file_error *);
mc_java_file_status mc_java_random_access_write(mc_java_random_access *, const uint8_t *, size_t,
                                                mc_java_file_error *);
mc_java_file_status mc_java_random_access_close(mc_java_random_access *, mc_java_file_error *);
mc_java_file_status mc_java_random_access_release(mc_java_random_access **, mc_java_file_error *);
bool mc_java_random_access_inspect(const mc_java_random_access *, mc_java_random_access_state *);
#endif
