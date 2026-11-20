#include "image_download.h"

#include "../images/image_io.h"
#include "image_buffer.h"
#include "internal.h"
#include "mc/core/java/java.h"

#include <stdlib.h>
#include <string.h>
#include <threads.h>

enum { MAX_ENTRIES = 128, MAX_WORKERS = 16, MAX_CALLS = 1024, MAX_URL_UNITS = 16384 };

struct mc_client_image_download_entry {
    mc_player_string *key;
    mc_client_image_download *value;
};

struct mc_client_image_download {
    mc_client_image_download *next;
    mc_client_image_download_native *native;
    mc_player_string *url;
    mc_client_image_buffer *processor;
    mc_client_argb_image *a, *decoded, *processed;
    int32_t b, c;
    bool d, thread_constructed, thread_started, thread_joined, mutex_ready;
    bool source_returned, print_pending;
    unsigned int constructor_pc, worker_pc;
    thrd_t thread;
    mtx_t mutex;
    atomic_bool stop, exited;
    mc_world_info_name *url_projection;
    mc_java_url *source_url;
    mc_java_http_input *connection;
    mc_client_image_download_error failure, cleanup;
    mc_client_image_buffer_error processor_error;
    mc_client_image_buffer_error processor_cleanup;
    mc_client_image_io_error image_error;
    mc_java_url_error input_error;
};

struct mc_client_image_download_native {
    mc_client_image_download *entries;
    size_t count, calls;
    atomic_size_t workers;
    mc_client_image_download_options options;
    bool retiring;
};

mc_client_image_download_options mc_client_image_download_default_options(void) {
    return (mc_client_image_download_options){MAX_ENTRIES, MAX_ENTRIES, MAX_WORKERS, MAX_CALLS,
                                              MAX_URL_UNITS};
}

mc_client_image_download_status
mc_client_render_image_downloads_configure(mc_client_render_engine *engine,
                                           const mc_client_image_download_options *options) {
    if (engine == NULL || options == NULL || options->max_entries > MAX_ENTRIES ||
        options->max_map_entries > MAX_ENTRIES || options->max_active_workers > MAX_WORKERS ||
        options->max_retained_calls > MAX_CALLS || options->max_url_units > MAX_URL_UNITS)
        return MC_CLIENT_IMAGE_DOWNLOAD_ARGUMENT;
    if (engine->image_downloads_retired)
        return MC_CLIENT_IMAGE_DOWNLOAD_STATE;
    if (engine->active_resource_calls != 0 ||
        engine->phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING ||
        (engine->image_downloads != NULL &&
         (engine->image_downloads->count != 0 || engine->image_downloads->calls != 0 ||
          engine->image_downloads->retiring)))
        return MC_CLIENT_IMAGE_DOWNLOAD_BUSY;
    if (engine->image_downloads == NULL) {
        if (engine->fields.h != NULL && engine->fields.h->count != 0)
            return MC_CLIENT_IMAGE_DOWNLOAD_STATE;
        engine->image_downloads = calloc(1, sizeof(*engine->image_downloads));
        if (engine->image_downloads == NULL)
            return MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION;
        atomic_init(&engine->image_downloads->workers, 0);
    }
    engine->image_downloads->options = *options;
    return MC_CLIENT_IMAGE_DOWNLOAD_OK;
}

struct mc_client_image_download_call {
    mc_client_render_engine *engine;
    mc_player_string *url;
    mc_client_image_buffer *processor;
    mc_client_image_download *lookup, *candidate;
    bool engine_held, url_held, processor_held, counted;
    bool active, retiring, source_returned, map_published;
    unsigned int pc;
    mc_client_image_download_error failure, cleanup;
};

static mc_client_image_download_site site(const char *name, const char *method,
                                          const char *descriptor, unsigned int pc) {
    return (mc_client_image_download_site){name, method, descriptor, pc};
}

static mc_client_image_download_status fail(mc_client_image_download_error *e,
                                            mc_client_image_download_status status,
                                            const char *operation,
                                            mc_client_image_download_site where) {
    e->status = status;
    e->operation = operation;
    e->site = where;
    e->native_policy = status != MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL &&
                       status != MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_EXCEPTION;
    e->source_exception = !e->native_policy;
    if (status == MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL)
        e->exception_class = "java/lang/NullPointerException";
    return status;
}

static mc_client_image_download_status ji_fail(mc_client_image_download_call *call,
                                               mc_client_image_download_status status,
                                               const char *operation, unsigned int pc) {
    call->pc = pc;
    return fail(&call->failure, status, operation,
                site("ji", "a", "(Ljava/lang/String;Lnf;)Lek;", pc));
}

static bool key_equal(const mc_player_string *a, const mc_player_string *b) {
    if (a == b)
        return true;
    mc_player_string_view av, bv;
    return a != NULL && b != NULL && mc_player_string_read(a, &av) &&
           mc_player_string_read(b, &bv) && av.length == bv.length &&
           (av.length == 0 || memcmp(av.units, bv.units, av.length * sizeof(*av.units)) == 0);
}

static bool map_shape(const mc_client_render_map *map) {
    return map != NULL && map->count <= map->capacity && map->capacity <= MAX_ENTRIES &&
           (map->capacity == 0 || map->downloads != NULL);
}

static mc_client_image_download_status native_bind(mc_client_image_download_call *call) {
    mc_client_render_engine *engine = call->engine;
    if (engine->image_downloads == NULL) {
        /* h was created by the real ji constructor. The sidecar is only native
         * worker/call ownership; it is not a second source cache. */
        if (engine->fields.h->count != 0)
            return ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_STATE, "unbound image-download Map", 5);
        mc_client_image_download_native *native = calloc(1, sizeof(*native));
        if (native == NULL)
            return ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION,
                           "image-download native ownership", 5);
        atomic_init(&native->workers, 0);
        native->options = mc_client_image_download_default_options();
        engine->image_downloads = native;
    }
    if (engine->image_downloads->retiring)
        return ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_BUSY, "image-download native retirement", 5);
    if (engine->image_downloads->calls == engine->image_downloads->options.max_retained_calls)
        return ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT, "image-download retained calls", 5);
    ++engine->image_downloads->calls;
    call->counted = true;
    return MC_CLIENT_IMAGE_DOWNLOAD_OK;
}

static void worker_fail(mc_client_image_download *entry, mc_client_image_download_status status,
                        const char *operation, unsigned int pc) {
    entry->worker_pc = pc;
    fail(&entry->failure, status, operation, site("tj", "run", "()V", pc));
}

static void worker_url_fail(mc_client_image_download *entry, unsigned int pc) {
    const mc_java_url_error cause = entry->failure.url;
    worker_fail(entry,
                cause.source_exception                   ? MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_EXCEPTION
                : cause.status == MC_JAVA_URL_LIMIT      ? MC_CLIENT_IMAGE_DOWNLOAD_LIMIT
                : cause.status == MC_JAVA_URL_ALLOCATION ? MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION
                                                         : MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
                cause.operation, pc);
    entry->failure.url = cause;
    entry->failure.exception_class = cause.exception_class;
    if (cause.source_exception) {
        /* The real catch has not returned from printStackTrace108. Native
         * cleanup does not invent the following source disconnect112. */
        entry->print_pending = true;
        entry->failure.caller = entry->failure.site;
        worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
                    "actual image-download Exception.printStackTrace", 108);
    }
}

static mc_client_image_io_status image_read(void *context, uint8_t *bytes, size_t capacity,
                                            size_t *count, mc_client_image_io_error *error) {
    mc_client_image_download *entry = context;
    int32_t n = 0;
    const mc_java_url_status status =
        mc_java_http_read(entry->connection, bytes, capacity, &n, &entry->input_error);
    *count = 0;
    if (status != MC_JAVA_URL_OK)
        return mc_client_image_io_fail(
            error,
            entry->input_error.source_exception ? MC_CLIENT_IMAGE_IO_SOURCE_IO
            : status == MC_JAVA_URL_LIMIT       ? MC_CLIENT_IMAGE_IO_LIMIT
            : status == MC_JAVA_URL_ALLOCATION  ? MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED
                                                : MC_CLIENT_IMAGE_IO_PROVIDER_FAILED,
            entry->input_error.operation);
    if (n > 0)
        *count = (size_t)n;
    return MC_CLIENT_IMAGE_IO_OK;
}

static bool worker_disconnect(mc_client_image_download *entry, unsigned int pc) {
    entry->worker_pc = pc;
    if (mc_java_http_disconnect(entry->connection, &entry->failure.url) != MC_JAVA_URL_OK) {
        worker_url_fail(entry, pc);
        return false;
    }
    return true;
}

static void worker_body(mc_client_image_download *entry) {
    mc_player_string_view text = {0};
    entry->worker_pc = 2;
    if (mc_java_url_allocate(&entry->source_url, &entry->failure.url) != MC_JAVA_URL_OK) {
        worker_url_fail(entry, 2);
        return;
    }
    if (entry->url != NULL) {
        if (!mc_player_string_read(entry->url, &text) ||
            text.length > entry->native->options.max_url_units) {
            worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT,
                        "image-download native URL text bound", 10);
            return;
        }
        /* This is a native immutable argument projection for the shared URL
         * parser. tj.a and the h key still retain the SAME supplied String;
         * no UTF8 round trip or replacement source String is performed. */
        if (mc_world_info_name_copy((mc_nbt_string){text.units, text.length}, SIZE_MAX,
                                    &entry->url_projection) != MC_WORLD_INFO_OK) {
            worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION,
                        "image-download URL argument projection", 10);
            return;
        }
    }
    entry->worker_pc = 10;
    if (mc_java_url_construct(entry->source_url, entry->url_projection, &entry->failure.url) !=
        MC_JAVA_URL_OK) {
        worker_url_fail(entry, 10);
        return;
    }
    mc_java_url_view url;
    if (!mc_java_url_inspect(entry->source_url, &url)) {
        worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_STATE, "constructed URL view", 15);
        return;
    }
    if (!url.http) {
        /* The shared constructor admits HTTP/HTTPS only. HttpsURLConnection
         * is an HttpURLConnection too; missing native TLS is not a Java cast
         * failure and must stop before that actual source checkcast18. */
        entry->failure.url =
            (mc_java_url_error){.status = MC_JAVA_URL_PROVIDER,
                                .operation = "HTTPS connection native transport provider"};
        worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
                    "HTTPS connection native transport provider", 15);
        return;
    }
    entry->worker_pc = 33;
    if (mc_java_http_connect(entry->source_url, NULL, &entry->stop, &entry->connection,
                             &entry->failure.url) != MC_JAVA_URL_OK) {
        worker_url_fail(entry, 33);
        return;
    }
    long code = 0;
    entry->worker_pc = 37;
    if (mc_java_http_response_code(entry->connection, &code, &entry->failure.url) !=
        MC_JAVA_URL_OK) {
        worker_url_fail(entry, 37);
        return;
    }
    if (code / 100 == 4) {
        if (worker_disconnect(entry, 48)) {
            entry->worker_pc = 51;
            entry->source_returned = true;
        }
        return;
    }
    const unsigned int input_pc = entry->processor == NULL ? 64u : 85u;
    entry->worker_pc = input_pc;
    if (mc_java_http_get_input(entry->connection, &entry->failure.url) != MC_JAVA_URL_OK) {
        worker_url_fail(entry, input_pc);
        return;
    }
    const mc_client_image_io_input input = {entry, image_read, false};
    const mc_client_image_io_limits limits = {16u * 1024u * 1024u, 16u * 1024u * 1024u,
                                              8u * 1024u * 1024u};
    entry->worker_pc = entry->processor == NULL ? 67u : 88u;
    const mc_client_image_io_status decoded =
        mc_client_image_io_read(&input, &limits, &entry->decoded, &entry->image_error);
    if (decoded != MC_CLIENT_IMAGE_IO_OK) {
        worker_fail(entry,
                    decoded == MC_CLIENT_IMAGE_IO_LIMIT ? MC_CLIENT_IMAGE_DOWNLOAD_LIMIT
                    : decoded == MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED
                        ? MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION
                        : MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
                    entry->image_error.operation, entry->worker_pc);
        entry->failure.image = &entry->image_error;
        entry->failure.url = entry->input_error;
        if (decoded == MC_CLIENT_IMAGE_IO_SOURCE_IO ||
            decoded == MC_CLIENT_IMAGE_IO_SOURCE_ILLEGAL_ARGUMENT) {
            entry->print_pending = true;
            entry->failure.caller = entry->failure.site;
            worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
                        "actual image-download Exception.printStackTrace", 108);
        }
        return;
    }
    mc_client_argb_image *published = entry->decoded;
    if (entry->processor != NULL) {
        entry->worker_pc = 91;
        if (mc_client_image_buffer_process(entry->processor, entry->decoded, &entry->processed,
                                           &entry->processor_error) != MC_CLIENT_IMAGE_BUFFER_OK) {
            worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
                        entry->processor_error.operation, 91);
            entry->failure.processor = &entry->processor_error;
            return;
        }
        published = entry->processed;
    }
    entry->worker_pc = entry->processor == NULL ? 70u : 96u;
    if (mtx_lock(&entry->mutex) != thrd_success) {
        worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_NATIVE_THREAD,
                    "image-download image publication lock", entry->worker_pc);
        return;
    }
    entry->a = published;
    if (entry->processor == NULL)
        entry->decoded = NULL;
    else
        entry->processed = NULL;
    if (mtx_unlock(&entry->mutex) != thrd_success) {
        worker_fail(entry, MC_CLIENT_IMAGE_DOWNLOAD_NATIVE_THREAD,
                    "image-download image publication unlock", entry->worker_pc);
        return;
    }
    if (worker_disconnect(entry, 100)) {
        entry->worker_pc = 125;
        entry->source_returned = true;
    }
}

static int worker(void *context) {
    mc_client_image_download *entry = context;
    worker_body(entry);
    entry->failure.source_returned = entry->source_returned;
    atomic_fetch_sub_explicit(&entry->native->workers, 1, memory_order_acq_rel);
    atomic_store_explicit(&entry->exited, true, memory_order_release);
    return 0;
}

static bool construct_entry(mc_client_image_download_call *call) {
    mc_client_image_download_native *native = call->engine->image_downloads;
    if (native->count == native->options.max_entries) {
        ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT, "image-download retained entries", 23);
        return false;
    }
    mc_client_image_download *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION, "actual NEW ek native owner", 23);
        return false;
    }
    atomic_init(&entry->stop, false);
    atomic_init(&entry->exited, false);
    entry->native = native;
    entry->next = native->entries;
    native->entries = entry;
    ++native->count;
    call->candidate = entry;
    entry->constructor_pc = 6;
    entry->b = 1;
    entry->constructor_pc = 11;
    entry->c = -1;
    entry->constructor_pc = 16;
    entry->d = false;
    if (mtx_init(&entry->mutex, mtx_plain) != thrd_success) {
        ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_NATIVE_THREAD, "image-download native mutex", 29);
        return false;
    }
    entry->mutex_ready = true;
    entry->constructor_pc = 26;
    if (call->url != NULL) {
        if (mc_player_string_retain(call->url) != MC_PLAYER_OK) {
            ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT, "captured tj.a String occurrence", 29);
            return false;
        }
        entry->url = call->url;
    }
    if (call->processor != NULL) {
        if (mc_client_image_buffer_retain(call->processor, &entry->processor_error) !=
            MC_CLIENT_IMAGE_BUFFER_OK) {
            call->failure.processor = &entry->processor_error;
            ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_BUSY, "captured tj.b processor occurrence", 29);
            return false;
        }
        entry->processor = call->processor;
    }
    entry->thread_constructed = true;
    entry->constructor_pc = 29;
    if (atomic_load_explicit(&native->workers, memory_order_acquire) >=
        native->options.max_active_workers) {
        ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT, "image-download native active workers", 29);
        return false;
    }
    atomic_fetch_add_explicit(&native->workers, 1, memory_order_acq_rel);
    const int started = thrd_create(&entry->thread, worker, entry);
    if (started != thrd_success) {
        atomic_fetch_sub_explicit(&native->workers, 1, memory_order_acq_rel);
        ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_NATIVE_THREAD, "actual tj Thread.start", 29);
        return false;
    }
    entry->thread_started = true;
    entry->constructor_pc = 32;
    return true;
}

static void request_body(mc_client_image_download_call *call) {
    mc_client_render_engine *engine = call->engine;
    if (engine == NULL) {
        ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL, "actual ji null receiver", 1);
        return;
    }
    mc_client_render_map *map = (mc_client_render_map *)engine->fields.h;
    if (map == NULL) {
        ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL, "actual ji.h get receiver", 5);
        return;
    }
    if (!map_shape(map)) {
        ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_STATE, "actual ji.h native metadata", 5);
        return;
    }
    if (native_bind(call) != MC_CLIENT_IMAGE_DOWNLOAD_OK)
        return;
    call->pc = 5;
    for (size_t i = 0; i < map->count; ++i)
        if (key_equal(map->downloads[i].key, call->url)) {
            call->lookup = map->downloads[i].value;
            break;
        }
    if (call->lookup != NULL) {
        call->pc = 48;
        call->lookup->b = mc_java_i32_add(call->lookup->b, 1);
    } else {
        call->pc = 23;
        if (!construct_entry(call))
            return;
        call->pc = 32;
        if (map->count >= call->engine->image_downloads->options.max_map_entries) {
            ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT, "actual h.put native entry bound", 32);
            return;
        }
        if (map->count == map->capacity) {
            size_t capacity = map->capacity == 0 ? 4u : map->capacity * 2u;
            if (capacity > call->engine->image_downloads->options.max_map_entries)
                capacity = call->engine->image_downloads->options.max_map_entries;
            if (capacity <= map->count) {
                ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT, "actual h.put native entry bound",
                        32);
                return;
            }
            mc_client_image_download_entry *entries = malloc(capacity * sizeof(*entries));
            if (entries == NULL) {
                ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION, "actual h.put native storage",
                        32);
                return;
            }
            if (map->count != 0)
                memcpy(entries, map->downloads, map->count * sizeof(*entries));
            free(map->downloads);
            map->downloads = entries;
            map->capacity = capacity;
        }
        if (call->url != NULL && mc_player_string_retain(call->url) != MC_PLAYER_OK) {
            ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT, "actual h.put key occurrence", 32);
            return;
        }
        map->downloads[map->count++] = (mc_client_image_download_entry){call->url, call->candidate};
        call->map_published = true;
    }
    call->pc = 52;
    call->source_returned = true;
    call->failure.source_returned = true;
}

mc_client_image_download_status
mc_client_image_download_request_begin(mc_client_render_engine *engine, mc_player_string *url,
                                       mc_client_image_buffer *processor,
                                       mc_client_image_download_call **out) {
    if (out == NULL)
        return MC_CLIENT_IMAGE_DOWNLOAD_ARGUMENT;
    mc_client_image_download_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return MC_CLIENT_IMAGE_DOWNLOAD_ALLOCATION;
    call->engine = engine;
    call->url = url;
    call->processor = processor;
    *out = call;
    if (engine != NULL) {
        if (engine->image_downloads_retired) {
            call->engine = NULL;
            call->url = NULL;
            call->processor = NULL;
            return ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_STATE,
                           "image-download Engine terminal native stop", 0);
        }
        if (engine->phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING ||
            engine->active_resource_calls != 0 || engine->classes->active_source_calls != 0 ||
            mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK) {
            call->engine = NULL;
            call->url = NULL;
            call->processor = NULL;
            return ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_BUSY, "captured ji native occurrence", 0);
        }
        call->engine_held = true;
    }
    if (url != NULL) {
        if (mc_player_string_retain(url) != MC_PLAYER_OK) {
            call->url = NULL;
            call->processor = NULL;
            return ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT, "captured request String", 0);
        }
        call->url_held = true;
    }
    if (processor != NULL) {
        if (mc_client_image_buffer_retain(processor, NULL) != MC_CLIENT_IMAGE_BUFFER_OK) {
            call->processor = NULL;
            return ji_fail(call, MC_CLIENT_IMAGE_DOWNLOAD_BUSY, "captured request processor", 0);
        }
        call->processor_held = true;
    }
    call->active = true;
    if (engine != NULL) {
        ++engine->active_resource_calls;
        ++engine->classes->active_source_calls;
    }
    request_body(call);
    if (engine != NULL) {
        --engine->classes->active_source_calls;
        --engine->active_resource_calls;
    }
    call->active = false;
    return call->failure.status;
}

bool mc_client_image_download_call_inspect(const mc_client_image_download_call *call,
                                           mc_client_image_download_call_state *out) {
    if (call == NULL || out == NULL)
        return false;
    *out = (mc_client_image_download_call_state){
        call->active,   call->retiring, call->source_returned, call->map_published, call->pc,
        call->engine,   call->url,      call->processor,       call->lookup,        call->candidate,
        &call->failure, &call->cleanup};
    return true;
}

mc_client_image_download_status
mc_client_image_download_call_retire(mc_client_image_download_call **slot) {
    if (slot == NULL)
        return MC_CLIENT_IMAGE_DOWNLOAD_ARGUMENT;
    mc_client_image_download_call *call = *slot;
    if (call == NULL)
        return MC_CLIENT_IMAGE_DOWNLOAD_OK;
    if (call->active)
        return MC_CLIENT_IMAGE_DOWNLOAD_BUSY;
    call->retiring = true;
    call->cleanup = (mc_client_image_download_error){0};
    if (call->processor_held) {
        mc_client_image_buffer *processor = call->processor;
        if (mc_client_image_buffer_release(&processor, NULL) != MC_CLIENT_IMAGE_BUFFER_OK)
            return fail(&call->cleanup, MC_CLIENT_IMAGE_DOWNLOAD_BUSY,
                        "captured image processor occurrence release", site(NULL, NULL, NULL, 0));
        call->processor_held = false;
        call->processor = NULL;
    }
    if (call->url_held) {
        mc_player_string_release(call->url);
        call->url_held = false;
        call->url = NULL;
    }
    if (call->counted) {
        --call->engine->image_downloads->calls;
        call->counted = false;
        call->lookup = NULL;
        call->candidate = NULL;
        call->failure.processor = NULL;
        call->cleanup.processor = NULL;
    }
    if (call->engine_held) {
        if (mc_client_render_engine_release(call->engine) != MC_CLIENT_RENDER_ENGINE_OK)
            return fail(&call->cleanup, MC_CLIENT_IMAGE_DOWNLOAD_BUSY,
                        "captured image Engine occurrence release", site(NULL, NULL, NULL, 0));
        call->engine_held = false;
    }
    free(call);
    *slot = NULL;
    return MC_CLIENT_IMAGE_DOWNLOAD_OK;
}

bool mc_client_image_download_inspect(const mc_client_image_download *entry,
                                      mc_client_image_download_state *out) {
    if (entry == NULL || out == NULL)
        return false;
    const bool exited = atomic_load_explicit(&entry->exited, memory_order_acquire);
    /* Worker diagnostics become immutable at exit. Rendering can observe its
     * published image earlier through the dedicated locked texture query. */
    mc_client_image_download_state state = {.b = entry->b,
                                            .c = entry->c,
                                            .d = entry->d,
                                            .thread_constructed = entry->thread_constructed,
                                            .thread_started = entry->thread_started,
                                            .thread_exited = exited,
                                            .thread_joined = entry->thread_joined,
                                            .constructor_pc = entry->constructor_pc,
                                            .url = entry->url,
                                            .processor = entry->processor};
    state.native_stop_requested = atomic_load_explicit(&entry->stop, memory_order_acquire);
    if (entry->mutex_ready) {
        mtx_t *mutex = (mtx_t *)&entry->mutex;
        if (mtx_lock(mutex) != thrd_success)
            return false;
        state.a = entry->a;
        if (mtx_unlock(mutex) != thrd_success)
            return false;
    }
    if (exited || !entry->thread_started) {
        state.worker_source_returned = entry->source_returned;
        state.print_stack_trace_pending = entry->print_pending;
        state.worker_pc = entry->worker_pc;
        state.worker_error = &entry->failure;
        state.cleanup_error = &entry->cleanup;
        state.connection = entry->connection;
    }
    *out = state;
    return true;
}

mc_client_image_download_status
mc_client_render_download_texture(mc_client_render_engine *engine, mc_client_display *display,
                                  const mc_player_string *url, const mc_player_string *fallback,
                                  mc_client_texture *out, mc_client_download_texture_error *e) {
    if (e == NULL || out == NULL)
        return MC_CLIENT_IMAGE_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_texture_error){0};
    const char *descriptor = "(Ljava/lang/String;Ljava/lang/String;)I";
    if (engine == NULL)
        return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL,
                    "actual downloaded texture null Engine", site("ji", "a", descriptor, 1));
    if (engine->image_downloads_retired || engine->active_resource_calls != 0 ||
        engine->classes->active_source_calls != 0 ||
        (engine->image_downloads != NULL && engine->image_downloads->retiring))
        return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_BUSY,
                    "downloaded texture native operation admission",
                    site("ji", "a", descriptor, 5));
    mc_client_render_map *map = (mc_client_render_map *)engine->fields.h;
    if (map == NULL)
        return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL,
                    "actual downloaded texture h Map receiver", site("ji", "a", descriptor, 5));
    if (!map_shape(map))
        return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_STATE,
                    "downloaded texture actual Map metadata", site("ji", "a", descriptor, 5));
    mc_client_image_download *entry = NULL;
    for (size_t i = 0; i < map->count; ++i)
        if (key_equal(map->downloads[i].key, url)) {
            entry = map->downloads[i].value;
            break;
        }
    mc_client_argb_image *image = NULL;
    if (entry != NULL) {
        if (!entry->mutex_ready || mtx_lock(&entry->mutex) != thrd_success)
            return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_BUSY,
                        "downloaded texture image publication lock",
                        site("ji", "a", descriptor, 19));
        image = entry->a;
        if (image != NULL && !entry->d &&
            mc_client_argb_image_retain(image) != MC_CLIENT_RENDER_ENGINE_OK) {
            mtx_unlock(&entry->mutex);
            return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_LIMIT,
                        "downloaded texture image occurrence", site("ji", "a", descriptor, 19));
        }
        const bool upload = image != NULL && !entry->d;
        mtx_unlock(&entry->mutex);
        if (upload) {
            if (entry->c >= 0) {
                mc_client_argb_image_release(image);
                e->source_pc = 63;
                return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
                            "actual downloaded texture existing-id image refresh",
                            site("ji", "a", descriptor, 63));
            }
            mc_client_texture generated;
            const mc_client_render_texture_status status = mc_client_render_engine_register_image(
                engine, display, NULL, image, &generated, &e->texture);
            mc_client_argb_image_release(image);
            if (status != MC_CLIENT_RENDER_TEXTURE_OK) {
                e->source_pc = 45;
                return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_STATE,
                            "actual downloaded texture image registration child",
                            site("ji", "a", descriptor, 45));
            }
            entry->c = (int32_t)generated; /* Actual ek.c48. */
            entry->d = true;               /* Actual ek.d68. */
        }
    }
    if (entry == NULL || entry->c < 0) {
        if (fallback == NULL) {
            *out = (mc_client_texture)-1;
            e->source_pc = 87;
        } else {
            mc_player_string_view text;
            if (!mc_player_string_read(fallback, &text))
                return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_STATE,
                            "downloaded texture actual fallback String",
                            site("ji", "a", descriptor, 90));
            e->fallback = true;
            const mc_texture_pack_text key = {text.units, text.length, false};
            if (mc_client_render_engine_texture(engine, display, NULL, key, out, &e->lookup) !=
                MC_CLIENT_LOOKUP_OK) {
                e->source_pc = 90;
                return fail(&e->download, MC_CLIENT_IMAGE_DOWNLOAD_STATE,
                            "actual downloaded texture fallback lookup child",
                            site("ji", "a", descriptor, 90));
            }
            e->source_pc = 93;
        }
    } else {
        *out = (mc_client_texture)entry->c;
        e->source_pc = 98;
    }
    e->returned = true;
    e->download.source_returned = true;
    e->download.site = site("ji", "a", descriptor, e->source_pc);
    return MC_CLIENT_IMAGE_DOWNLOAD_OK;
}

mc_client_image_download_status
mc_client_render_image_download_release(mc_client_render_engine *engine, mc_player_string *url,
                                        mc_client_image_download_error *error) {
    mc_client_image_download_error local = {0};
    mc_client_image_download_error *e = error == NULL ? &local : error;
    *e = (mc_client_image_download_error){0};
    if (engine == NULL)
        return fail(e, MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL, "actual ji.c null receiver",
                    site("ji", "c", "(Ljava/lang/String;)V", 1));
    if (engine->image_downloads_retired)
        return fail(e, MC_CLIENT_IMAGE_DOWNLOAD_STATE, "image-download Engine terminal native stop",
                    site(NULL, NULL, NULL, 0));
    if (engine->active_resource_calls != 0 || engine->classes->active_source_calls != 0 ||
        (engine->image_downloads != NULL && engine->image_downloads->retiring))
        return fail(e, MC_CLIENT_IMAGE_DOWNLOAD_BUSY, "image-release native active operation",
                    site(NULL, NULL, NULL, 0));
    mc_client_render_map *map = (mc_client_render_map *)engine->fields.h;
    if (map == NULL)
        return fail(e, MC_CLIENT_IMAGE_DOWNLOAD_SOURCE_NULL, "actual ji.h release receiver",
                    site("ji", "c", "(Ljava/lang/String;)V", 5));
    if (!map_shape(map))
        return fail(e, MC_CLIENT_IMAGE_DOWNLOAD_STATE, "image-release native Map metadata",
                    site("ji", "c", "(Ljava/lang/String;)V", 5));
    if (map->count != 0 && engine->image_downloads == NULL)
        return fail(e, MC_CLIENT_IMAGE_DOWNLOAD_STATE, "unbound image-release Map values",
                    site("ji", "c", "(Ljava/lang/String;)V", 5));
    for (size_t i = 0; i < map->count; ++i) {
        if (!key_equal(map->downloads[i].key, url))
            continue;
        mc_client_image_download *entry = map->downloads[i].value;
        if (entry == NULL)
            break;
        entry->b = mc_java_i32_sub(entry->b, 1);
        if (entry->b != 0)
            break;
        if (entry->c >= 0)
            return fail(e, MC_CLIENT_IMAGE_DOWNLOAD_UNIMPLEMENTED,
                        "actual ji.a(int) downloaded texture deletion",
                        site("ji", "c", "(Ljava/lang/String;)V", 47));
        mc_player_string_release(map->downloads[i].key);
        if (i + 1u < map->count)
            memmove(map->downloads + i, map->downloads + i + 1u,
                    (map->count - i - 1u) * sizeof(*map->downloads));
        --map->count;
        break;
    }
    e->site = site("ji", "c", "(Ljava/lang/String;)V", 61);
    e->source_returned = true;
    return MC_CLIENT_IMAGE_DOWNLOAD_OK;
}

static mc_client_image_download_status retirement_fail(mc_client_image_download_error *error,
                                                       mc_client_image_download_status status,
                                                       const char *operation) {
    if (error != NULL)
        fail(error, status, operation, site(NULL, NULL, NULL, 0));
    return status;
}

mc_client_image_download_status
mc_client_render_image_downloads_retire(mc_client_render_engine *engine,
                                        mc_client_image_download_error *error) {
    if (error != NULL)
        *error = (mc_client_image_download_error){0};
    if (engine == NULL)
        return retirement_fail(error, MC_CLIENT_IMAGE_DOWNLOAD_ARGUMENT,
                               "image-download retirement Engine");
    mc_client_image_download_native *native = engine->image_downloads;
    if ((native != NULL && native->calls != 0) || engine->active_resource_calls != 0 ||
        engine->holds != 1)
        return retirement_fail(error, MC_CLIENT_IMAGE_DOWNLOAD_BUSY,
                               "image-download retained invocation");
    engine->image_downloads_retired = true;
    if (native == NULL)
        return MC_CLIENT_IMAGE_DOWNLOAD_OK;
    native->retiring = true;
    for (mc_client_image_download *entry = native->entries; entry != NULL; entry = entry->next)
        atomic_store_explicit(&entry->stop, true, memory_order_release);
    for (mc_client_image_download *entry = native->entries; entry != NULL; entry = entry->next) {
        if (entry->thread_started && !entry->thread_joined) {
            if (!atomic_load_explicit(&entry->exited, memory_order_acquire))
                return retirement_fail(error, MC_CLIENT_IMAGE_DOWNLOAD_BUSY,
                                       "image-download worker stopping");
            if (thrd_join(entry->thread, NULL) != thrd_success)
                return retirement_fail(error, MC_CLIENT_IMAGE_DOWNLOAD_CLEANUP_FAILED,
                                       "image-download worker join");
            entry->thread_joined = true;
        }
        if (mc_java_http_release(&entry->connection, &entry->cleanup.url) != MC_JAVA_URL_OK ||
            mc_java_url_release(&entry->source_url, &entry->cleanup.url) != MC_JAVA_URL_OK) {
            if (error != NULL)
                *error = entry->cleanup;
            return retirement_fail(error, MC_CLIENT_IMAGE_DOWNLOAD_CLEANUP_FAILED,
                                   entry->cleanup.url.operation);
        }
        if (entry->processor != NULL &&
            mc_client_image_buffer_release(&entry->processor, &entry->processor_cleanup) !=
                MC_CLIENT_IMAGE_BUFFER_OK) {
            if (error != NULL)
                error->processor = &entry->processor_cleanup;
            return retirement_fail(error, MC_CLIENT_IMAGE_DOWNLOAD_BUSY,
                                   entry->processor_cleanup.operation);
        }
    }
    mc_client_render_map *map = (mc_client_render_map *)engine->fields.h;
    if (map != NULL) {
        for (size_t i = 0; i < map->count; ++i)
            mc_player_string_release(map->downloads[i].key);
        free(map->downloads);
        map->downloads = NULL;
        map->count = map->capacity = 0;
    }
    while (native->entries != NULL) {
        mc_client_image_download *entry = native->entries;
        native->entries = entry->next;
        mc_client_argb_image_release(entry->a);
        mc_client_argb_image_release(entry->decoded);
        mc_client_argb_image_release(entry->processed);
        mc_world_info_name_release(entry->url_projection);
        mc_player_string_release(entry->url);
        if (entry->mutex_ready)
            mtx_destroy(&entry->mutex);
        free(entry);
    }
    free(native);
    engine->image_downloads = NULL;
    return MC_CLIENT_IMAGE_DOWNLOAD_OK;
}
