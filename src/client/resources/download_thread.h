#ifndef MC_CLIENT_RESOURCES_DOWNLOAD_THREAD_H
#define MC_CLIENT_RESOURCES_DOWNLOAD_THREAD_H
#include "../application.h"
typedef struct client_application client_application;
#include "../../core/java/xml_dom.h"
#include "mc/client/sound/sound_file_registration.h"
#include "mc/core/java/print_stream.h"
#include "mc/world/storage/java_output.h"
typedef struct mc_client_source_cz mc_client_download_thread;
typedef struct mc_client_download_classes mc_client_download_classes;

typedef enum mc_client_download_status {
    MC_CLIENT_DOWNLOAD_OK,
    MC_CLIENT_DOWNLOAD_ARGUMENT,
    MC_CLIENT_DOWNLOAD_STATE,
    MC_CLIENT_DOWNLOAD_BUSY,
    MC_CLIENT_DOWNLOAD_LIMIT,
    MC_CLIENT_DOWNLOAD_ALLOCATION,
    MC_CLIENT_DOWNLOAD_NATIVE,
    MC_CLIENT_DOWNLOAD_CANCELLED_NATIVE,
    MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION,
    MC_CLIENT_DOWNLOAD_UNIMPLEMENTED
} mc_client_download_status;

typedef struct mc_client_download_site {
    const char *owner, *method, *descriptor;
    int pc;
} mc_client_download_site;

typedef struct mc_client_download_error {
    mc_client_download_status status;
    int native_error;
    const char *operation, *exception_class;
    mc_client_download_site site, callers[160];
    size_t caller_count;
    bool source_returned, print_stack_trace_pending, uncaught;
    mc_java_file_error file;
    mc_java_url_error url;
    mc_java_output_error output;
    mc_java_xml_error xml;
    mc_sound_pool_file_error sound;
    mc_java_print_error print;
} mc_client_download_error;

typedef struct mc_client_download_options {
    size_t max_depth, max_visits;
    mc_java_http_options http;
    mc_java_xml_options xml;
} mc_client_download_options;

typedef struct mc_client_download_view {
    size_t holds;
    const client_application *b;
    const mc_java_file *a;
    const mc_world_info_name *thread_name;
    bool c, daemon, thread_constructed, started, completed, run_returned, retiring,
        app_borrow_counted;
    int last_constructor_pc, last_run_pc;
    size_t local_visits, registrations, download_writes;
    const mc_client_download_error *failure;
} mc_client_download_view;

/* Error objects contain bounded complete native/source causes and should be
 * caller-owned heap records; native scan depth is limited to128 frames. */
mc_client_download_options mc_client_download_default_options(void);
/* Namespace creation may publish an owned failed candidate after native
 * allocation; retire it even when service binding fails. The caller keeps
 * its Files/System occurrences live during binding. Inspect views/errors
 * borrow owner storage until the next exclusive mutation/retirement.
 * Final thread release sets native stop, returns BUSY while run is active,
 * then joins and retries all fallible cleanup before dropping its App borrow.
 * Repeated BUSY preserves the occurrence; successful release consumes it. */
/* Native cold namespace retains real Files and the configured SAME System
 * output service. It is independent of nullable source App b; no bootstrap,
 * File/Thread/URL/network source action occurs here. */
mc_client_download_status mc_client_download_classes_create(mc_java_files *,
                                                            mc_java_system_output *,
                                                            mc_client_download_classes **,
                                                            mc_client_download_error *);
mc_client_download_status mc_client_download_classes_release(mc_client_download_classes **,
                                                             mc_client_download_error *);
mc_client_download_status mc_client_download_thread_allocate(mc_client_download_classes *,
                                                             const mc_client_download_options *,
                                                             mc_client_download_thread **,
                                                             mc_client_download_error *);
mc_client_download_status mc_client_download_thread_construct(mc_client_download_thread *,
                                                              mc_java_file *, client_application *,
                                                              mc_client_download_error *);
mc_client_download_status mc_client_download_thread_start(mc_client_download_thread *,
                                                          mc_client_download_error *);
/* Explicit source a() synchronous local scan; no cancellation read. */
mc_client_download_status mc_client_download_thread_scan(mc_client_download_thread *,
                                                         mc_client_download_error *);
/* Source b(): only c=true. Retiring/native cancellation is separate. */
/* Explicit actual private cz entry method for finite real local HTTP controls;
 * this does not replace the literal endpoint used by source run(). */
mc_client_download_status mc_client_download_thread_process_entry(mc_client_download_thread *,
                                                                  mc_java_url *,
                                                                  mc_world_info_name *, int64_t,
                                                                  unsigned int,
                                                                  mc_client_download_error *);
mc_client_download_status mc_client_download_thread_cancel(mc_client_download_thread *,
                                                           mc_client_download_error *);
mc_client_download_status mc_client_download_thread_retain(mc_client_download_thread *,
                                                           mc_client_download_error *);
/* Final release requests native stop then returns BUSY while worker runs; it
 * never writes c/J. Join, prefix retirement and App release are retryable.
 * Nonfinal release consumes its occurrence even with another live hold. */
mc_client_download_status mc_client_download_thread_release(mc_client_download_thread **,
                                                            mc_client_download_error *);
/* Strict enclosing-owner retirement leaves external occurrences and this
 * slot intact. A unique occurrence requests native stop even while the worker
 * owns its monitor, then uses the same retryable join/cleanup path as release. */
mc_client_download_status mc_client_download_thread_retire(mc_client_download_thread **,
                                                           mc_client_download_error *);
mc_client_download_status mc_client_download_thread_inspect(mc_client_download_thread *,
                                                            mc_client_download_view *,
                                                            mc_client_download_error *);
#endif
