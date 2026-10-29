#ifndef MC_CORE_JAVA_URL_H
#define MC_CORE_JAVA_URL_H
#include "mc/world/storage/java_file.h"
#include <stdatomic.h>
typedef struct mc_java_uri mc_java_uri;
typedef struct mc_java_url mc_java_url;
typedef struct mc_java_http_input mc_java_http_input;
typedef struct mc_nbt_stream mc_java_url_data_input;

typedef enum mc_java_url_status {
    MC_JAVA_URL_OK,
    MC_JAVA_URL_ARGUMENT,
    MC_JAVA_URL_STATE,
    MC_JAVA_URL_BUSY,
    MC_JAVA_URL_LIMIT,
    MC_JAVA_URL_ALLOCATION,
    MC_JAVA_URL_SOURCE_MALFORMED,
    MC_JAVA_URL_SOURCE_IO,
    MC_JAVA_URL_SOURCE_NOT_FOUND,
    MC_JAVA_URL_PROVIDER,
    MC_JAVA_URL_NATIVE,
    MC_JAVA_URL_NATIVE_CANCELLED,
    MC_JAVA_URL_SOURCE_NULL
} mc_java_url_status;

typedef struct mc_java_url_error {
    mc_java_url_status status;
    const char *operation;
    const char *exception_class;
    int native_error;
    const char *source_class, *source_method;
    int source_pc;
    long response_code;
    bool source_exception, source_closed, consumed;
    mc_java_file_error file;
} mc_java_url_error;

typedef struct mc_java_url_view {
    const mc_world_info_name *text;
    const char *request;
    const char *native_file;
    const uint16_t *file_url_path;
    size_t file_url_length;
    bool http, file;
    unsigned int phase;
} mc_java_url_view;

/* CPU owners have no curl/XML/device edge. Nonfinal release consumes slots.
 * Errors are mandatory. Creation publishes a retained native candidate after
 * allocation even if construction fails; status alone acknowledges success.
 * File.toURI preserves getAbsoluteFile, getPath, then isDirectory order. */
/* Creation publishes an owned native candidate after allocation even if a
 * later source/native step fails. Only OK acknowledges construction; release
 * every returned candidate. RAW/failed URL instances cannot open streams.
 * URL/URI and HTTP occurrences require exclusive owner access; any pointer
 * observed from a view remains borrowed through the next mutation/retirement. */
mc_java_url_status mc_java_file_to_uri(mc_java_file *, mc_java_uri **, mc_java_url_error *);
mc_java_url_status mc_java_uri_to_url(mc_java_uri *, mc_java_url **, mc_java_url_error *);
mc_java_url_status mc_java_uri_release(mc_java_uri **, mc_java_url_error *);
mc_java_url_status mc_java_url_allocate(mc_java_url **, mc_java_url_error *);
mc_java_url_status mc_java_url_construct(mc_java_url *, mc_world_info_name *, mc_java_url_error *);
mc_java_url_status mc_java_url_create(mc_world_info_name *, mc_java_url **, mc_java_url_error *);
mc_java_url_status mc_java_url_resolve(mc_java_url *, mc_world_info_name *, mc_java_url **,
                                       mc_java_url_error *);
mc_java_url_status mc_java_url_retain(mc_java_url *, mc_java_url_error *);
mc_java_url_status mc_java_url_release(mc_java_url **, mc_java_url_error *);
bool mc_java_url_inspect(const mc_java_url *, mc_java_url_view *);

typedef struct mc_java_http_options {
    size_t max_buffer_bytes;
    uint64_t max_body_bytes;
    unsigned int native_operation_millis;
} mc_java_http_options;

typedef struct mc_java_http_state {
    size_t holds, reads, source_closes;
    uint64_t received, consumed;
    bool source_open, native_open, completed, active;
    long response_code;
    mc_java_url_error failure;
} mc_java_http_state;

mc_java_http_options mc_java_http_default_options(void);
/* Failed open publishes a retained native candidate separately from successful
 * source-open acknowledgement: *candidate receives ownership on allocation.
 * Caller must inspect status, never equate nonnull candidate with openStream
 * success. stop is native retirement only, not cz.c source cancellation. */
mc_java_url_status mc_java_http_open(mc_java_url *, const mc_java_http_options *,
                                     const atomic_bool *stop, mc_java_http_input **candidate,
                                     mc_java_url_error *);
mc_java_url_status mc_java_http_retain(mc_java_http_input *, mc_java_url_error *);
mc_java_url_status mc_java_http_read(mc_java_http_input *, uint8_t *, size_t, int32_t *,
                                     mc_java_url_error *);
mc_java_url_status mc_java_http_source_close(mc_java_http_input *, mc_java_url_error *);
mc_java_url_status mc_java_http_release(mc_java_http_input **, mc_java_url_error *);
bool mc_java_http_inspect(const mc_java_http_input *, mc_java_http_state *);

typedef struct mc_java_url_data_input_state {
    unsigned int phase;
    size_t holds;
    int last_store;
    const mc_java_http_input *in;
    const uint8_t *bytearr, *read_buffer;
    const uint16_t *chararr;
} mc_java_url_data_input_state;

mc_java_url_status mc_java_url_data_input_allocate(size_t native_bytes, mc_java_url_data_input **,
                                                   mc_java_url_error *);
mc_java_url_status mc_java_url_data_input_construct(mc_java_url_data_input *, mc_java_http_input *,
                                                    mc_java_url_error *);
mc_java_url_status mc_java_url_data_input_read(mc_java_url_data_input *, uint8_t *, int32_t,
                                               bool is_null, int32_t *, mc_java_url_error *);
mc_java_url_status mc_java_url_data_input_close(mc_java_url_data_input *, mc_java_url_error *);
mc_java_url_status mc_java_url_data_input_release(mc_java_url_data_input **, mc_java_url_error *);
bool mc_java_url_data_input_inspect(const mc_java_url_data_input *, mc_java_url_data_input_state *);
#endif
