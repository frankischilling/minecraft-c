#ifndef MC_CLIENT_RENDER_ENGINE_IMAGE_DOWNLOAD_H
#define MC_CLIENT_RENDER_ENGINE_IMAGE_DOWNLOAD_H

#include "mc/client/render_engine/render_engine.h"
#include "mc/core/java/url.h"
#include "mc/entity/player/player.h"
#include "texture_lookup.h"

typedef struct mc_client_image_buffer mc_client_image_buffer;
typedef struct mc_client_image_download mc_client_image_download;
typedef struct mc_client_image_download_call mc_client_image_download_call;
typedef struct mc_client_image_download_native mc_client_image_download_native;
typedef struct mc_client_image_buffer_error mc_client_image_buffer_error;
typedef struct mc_client_image_io_error mc_client_image_io_error;

typedef enum mc_client_image_download_status {
    MC_CLIENT_IMAGE_DOWNLOAD_OK,
    MC_CLIENT_IMAGE_DOWNLOAD_ARGUMENT,
    MC_CLIENT_IMAGE_DOWNLOAD_STATE,
    MC_CLIENT_IMAGE_DOWNLOAD_BUSY,
    MC_CLIENT_IMAGE_DOWNLOAD_LIMIT,
    MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION,
    MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL,
    MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_EXCEPTION,
    MC_CLIENT_IMAGE_DOWNLOAD_NATIVE_THREAD,
    MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
    MC_CLIENT_IMAGE_DOWNLOAD_CLEANUP_FAILED
} mc_client_image_download_status;

typedef struct mc_client_image_download_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_client_image_download_site;

typedef struct mc_client_image_download_error {
    mc_client_image_download_status status;
    const char *operation, *exception_class;
    mc_client_image_download_site site, caller;
    bool native_policy, source_exception, source_returned;
    mc_java_url_error url;
    const mc_client_image_buffer_error *processor;
    const mc_client_image_io_error *image;
} mc_client_image_download_error;

typedef struct mc_client_image_download_state {
    /* Actual ek fields. a is an immutable image protected by this owner. */
    const mc_client_argb_image *a;
    int32_t b, c;
    bool d;
    bool thread_constructed, thread_started, thread_exited, thread_joined;
    bool native_stop_requested;
    bool worker_source_returned, print_stack_trace_pending;
    unsigned int constructor_pc, worker_pc;
    const mc_player_string *url;
    const mc_client_image_buffer *processor;
    const mc_client_image_download_error *worker_error, *cleanup_error;
    const mc_java_http_input *connection; /* Borrowed only once the worker has exited. */
} mc_client_image_download_state;

typedef struct mc_client_image_download_call_state {
    bool active, retiring, source_returned, map_published;
    unsigned int source_pc;
    const mc_client_render_engine *engine;
    const mc_player_string *url;
    const mc_client_image_buffer *processor;
    const mc_client_image_download *lookup, *candidate;
    const mc_client_image_download_error *failure, *cleanup;
} mc_client_image_download_call_state;

typedef struct mc_client_image_download_options {
    size_t max_entries, max_map_entries, max_active_workers, max_retained_calls, max_url_units;
} mc_client_image_download_options;

mc_client_image_download_options mc_client_image_download_default_options(void);
/* Native resource policy only, before any retained request/entry. Zero fields
 * keep their literal bounds. No source Map, thread, URL or image is created. */
mc_client_image_download_status
mc_client_render_image_downloads_configure(mc_client_render_engine *,
                                           const mc_client_image_download_options *);

/* Actual ji.a(String,nf), once. The same nullable String and concrete rr/NULL
 * processor are captured. New ek construction starts its real native worker
 * BEFORE publication in the actual h Map. Source return is the old lookup:
 * it is NULL on first insertion, independently of the map-owned new entry.
 * A returned call owns its exact completed/failed prefix; no replay occurs.
 * Worker completion and source worker return are independent of factory return.
 * Calls require the same exclusive Engine graph; no GL or completed image is
 * supplied by this factory. Native terminal retirement may stop/join workers;
 * source removal must not cancel or replace an existing running worker. */
mc_client_image_download_status
mc_client_image_download_request_begin(mc_client_render_engine *, mc_player_string *,
                                       mc_client_image_buffer *,
                                       mc_client_image_download_call **out);
bool mc_client_image_download_call_inspect(const mc_client_image_download_call *,
                                           mc_client_image_download_call_state *);
mc_client_image_download_status
mc_client_image_download_call_retire(mc_client_image_download_call **);
bool mc_client_image_download_inspect(const mc_client_image_download *,
                                      mc_client_image_download_state *);

typedef struct mc_client_download_texture_error {
    mc_client_image_download_error download;
    mc_client_texture_lookup_error lookup;
    mc_client_render_texture_error texture;
    bool fallback, returned;
    unsigned int source_pc;
} mc_client_download_texture_error;

/* Actual ji.a(String,String). Reads the real h map and synchronizes the worker
 * image publication. The same Engine and String arguments remain live and
 * exclusive. A failed fallback lookup belongs to the caller only if no lookup
 * was pending on entry. No worker/request is created by this query. */
mc_client_image_download_status
mc_client_render_download_texture(mc_client_render_engine *, mc_client_display *,
                                  const mc_player_string *url, const mc_player_string *fallback,
                                  mc_client_texture *, mc_client_download_texture_error *);
mc_client_image_download_status
mc_client_render_image_download_release(mc_client_render_engine *, mc_player_string *nullable_url,
                                        mc_client_image_download_error *);

/* Checked native terminal sidecar retirement. It neither invokes ji.c nor
 * changes source b counts. Once strict retirement admission succeeds, configure,
 * request and source release reject STATE permanently on this Engine, including
 * after the sidecar is empty/freed. A BUSY admission caused by foreign holds or
 * retained calls leaves admission open and requests no worker stop. Engine CPU
 * destruction requires terminal sidecar retirement to complete. */
mc_client_image_download_status
mc_client_render_image_downloads_retire(mc_client_render_engine *,
                                        mc_client_image_download_error *);

#endif
