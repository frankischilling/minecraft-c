#ifndef MC_WORLD_STORAGE_JAVA_FILE_INTERNAL_H
#define MC_WORLD_STORAGE_JAVA_FILE_INTERNAL_H
#include "../../platform/files/directory.h"
#include "mc/world/storage/java_file.h"
#include <stdio.h>

struct mc_java_files {
    size_t holds;
    mc_java_file_options options;
    mc_world_info_name *default_parent;
    bool active, retiring;
};

struct mc_java_file {
    mc_java_files *files;
    mc_java_file_state state;
    mc_world_info_name *pending_input, *pending_parent;
    mc_java_file_array *failed_array;
};

struct mc_java_file_array {
    mc_java_files *files;
    size_t holds, count, constructed;
    mc_java_file **items;
    mc_world_info_name **names;
    size_t names_count, names_capacity;
    bool active, retiring, source_array_allocated;
    mc_java_file *pending_file;
    void *issued_native_close_token; /* diagnostic only, no stale close rights */
    bool native_close_issued, native_close_failed;
};

struct mc_java_file_stream {
    mc_java_files *files;
    mc_java_file *file;
    int descriptor;
    size_t holds, read_calls, native_read_calls, byte_read_calls;
    uint64_t native_bytes_read;
    bool armed;
    mc_java_file_phase phase;
    bool active, source_close_reached, source_close_succeeded;
};

mc_java_file_status mc_file_result(mc_java_file_error *e, mc_java_file_status s, const char *op,
                                   int code);
mc_java_file_status mc_file_name_hold(mc_world_info_name *name, mc_java_file_error *e);
mc_java_file_status mc_file_ready(mc_java_file *file, mc_java_file_error *e);
bool mc_file_invalid(const mc_java_file *file);
mc_java_file_status mc_file_utf8(const mc_java_file *file, char **out, mc_java_file_error *e);
mc_java_file_status mc_file_native(const mc_java_file *file, mc_file_native_char **out,
                                   mc_java_file_error *e);
mc_java_file_status mc_file_io_result(mc_file_io_status s, const mc_file_io_error *cause,
                                      mc_java_file_error *e);
mc_java_file_status mc_file_construct_private_child(mc_java_file *raw, mc_java_file *parent,
                                                    mc_world_info_name *name,
                                                    mc_java_file_error *e);
mc_java_file_status mc_file_private_child(mc_java_files *files, mc_java_file *parent,
                                          mc_world_info_name *name, mc_java_file **out,
                                          mc_java_file_error *e);
#endif
