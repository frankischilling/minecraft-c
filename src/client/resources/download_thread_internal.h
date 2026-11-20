#ifndef MC_CLIENT_RESOURCES_DOWNLOAD_THREAD_INTERNAL_H
#define MC_CLIENT_RESOURCES_DOWNLOAD_THREAD_INTERNAL_H
#include "../application_state.h"
#include "download_thread.h"
#include <threads.h>

typedef struct download_frame {
    mc_java_file *directory;
    mc_java_file_array *array;
    mc_world_info_name *prefix, *name;
    size_t index, count;
    bool initialized;
} download_frame;

typedef struct download_call {
    mc_java_file *file, *parent;
    mc_world_info_name *key, *escaped, *prefix, *suffix, *entry_prefix;
    mc_java_uri *uri;
    mc_java_url *url;
    mc_java_http_input *input;
    mc_java_url_data_input *dis;
    uint8_t *buffer;
    mc_java_file_output *fos;
    mc_java_data_output *dos;
    mc_sound_pool_file_call *sound;
    mc_java_print_stream *print;
    mc_world_info_name *message;
} download_call;

struct mc_client_download_classes {
    mtx_t mutex;
    size_t holds;
    mc_java_files *files;
    mc_java_system_output *system;
    mc_world_info_name *literals[4];
    bool retiring, ready;
};

struct mc_client_source_cz {
    mtx_t mutex;
    atomic_size_t holds;
    atomic_bool c, stop, retiring;
    mc_client_download_classes *classes;
    mc_client_download_options options;
    client_application *b;
    mc_java_file *a, *pending_file;
    mc_world_info_name *thread_name;
    bool counted, thread_constructed, daemon, constructed, failed, started, completed, joined,
        run_returned;
    thrd_t thread;
    int constructor_pc, run_pc;
    size_t local_visits, registrations, writes;
    download_frame *frames;
    size_t depth;
    download_call call;
    mc_java_url *listing_url;
    mc_java_http_input *listing;
    mc_java_xml_factory *factory;
    mc_java_xml_builder *builder;
    mc_java_xml_document *document;
    mc_java_xml_list *contents, *selection;
    mc_client_download_error failure, caught, run_caught;
    size_t caught_count;
    atomic_bool worker_exited;
};

mc_client_download_status mc_download_fail(mc_client_download_error *, mc_client_download_status,
                                           const char *, const char *, const char *, int,
                                           const char *);
void mc_download_caller(mc_client_download_error *, const char *, const char *, const char *, int);
mc_client_download_status mc_download_literal(mc_client_download_classes *, unsigned int,
                                              mc_world_info_name **, mc_client_download_error *);
mc_client_download_status mc_download_call_retire(mc_client_download_thread *,
                                                  mc_client_download_error *);
mc_client_download_status mc_download_run(mc_client_download_thread *, mc_client_download_error *);
mc_client_download_status mc_download_scan(mc_client_download_thread *, mc_client_download_error *);
mc_client_download_status mc_download_register(mc_client_download_thread *, client_application *,
                                               mc_world_info_name *, mc_java_file *,
                                               mc_client_download_error *);
mc_client_download_status mc_download_file_error(mc_client_download_error *, mc_java_file_error,
                                                 int, const char *);
mc_client_download_status mc_download_name(const uint16_t *, size_t, mc_world_info_name **,
                                           mc_client_download_error *);
mc_client_download_status mc_download_concat(mc_nbt_string, mc_nbt_string, const char *,
                                             mc_world_info_name **, mc_client_download_error *);
#endif
