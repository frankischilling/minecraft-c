#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "../application_access.h"
#include "download_thread_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif
static atomic_uint_least32_t default_thread_names;

mc_client_download_status mc_download_fail(mc_client_download_error *e, mc_client_download_status s,
                                           const char *owner, const char *method,
                                           const char *descriptor, int pc, const char *op) {
    if (e != NULL) {
        e->status = s;
        e->operation = op;
        e->site = (mc_client_download_site){owner, method, descriptor, pc};
    }
    return s;
}

void mc_download_caller(mc_client_download_error *e, const char *owner, const char *method,
                        const char *descriptor, int pc) {
    if (e != NULL && e->caller_count < 160u)
        e->callers[e->caller_count++] = (mc_client_download_site){owner, method, descriptor, pc};
}

mc_client_download_status mc_download_name(const uint16_t *units, size_t length,
                                           mc_world_info_name **out, mc_client_download_error *e) {
    if (length == 0) {
        *out = mc_world_info_name_empty();
        return MC_CLIENT_DOWNLOAD_OK;
    }
    mc_world_info_status s = mc_world_info_name_copy((mc_nbt_string){units, length}, SIZE_MAX, out);
    return s == MC_WORLD_INFO_OK ? MC_CLIENT_DOWNLOAD_OK
                                 : mc_download_fail(e, MC_CLIENT_DOWNLOAD_ALLOCATION, NULL, NULL,
                                                    NULL, -1, "download String native storage");
}

mc_client_download_status mc_download_concat(mc_nbt_string a, mc_nbt_string b, const char *suffix,
                                             mc_world_info_name **out,
                                             mc_client_download_error *e) {
    size_t tail = strlen(suffix);
    if (a.length > 65536u || b.length > 65536u - a.length || tail > 65536u - a.length - b.length)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_LIMIT, NULL, NULL, NULL, -1,
                                "resource String bound");
    size_t count = a.length + b.length + tail;
    uint16_t *units = malloc((count == 0 ? 1u : count) * sizeof(*units));
    if (units == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ALLOCATION, NULL, NULL, NULL, -1,
                                "resource StringBuilder storage");
    if (a.length != 0)
        memcpy(units, a.units, a.length * sizeof(*units));
    if (b.length != 0)
        memcpy(units + a.length, b.units, b.length * sizeof(*units));
    for (size_t i = 0; i < tail; ++i)
        units[a.length + b.length + i] = (unsigned char)suffix[i];
    mc_client_download_status s = mc_download_name(units, count, out, e);
    free(units);
    return s;
}

mc_client_download_status mc_download_file_error(mc_client_download_error *e, mc_java_file_error f,
                                                 int pc, const char *method) {
    mc_client_download_status s = f.status == MC_JAVA_FILE_BUSY    ? MC_CLIENT_DOWNLOAD_BUSY
                                  : f.status == MC_JAVA_FILE_LIMIT ? MC_CLIENT_DOWNLOAD_LIMIT
                                  : f.status == MC_JAVA_FILE_ALLOCATION_FAILED
                                      ? MC_CLIENT_DOWNLOAD_ALLOCATION
                                      : MC_CLIENT_DOWNLOAD_UNIMPLEMENTED;
    mc_download_fail(e, s, pc < 0 ? NULL : "cz", method,
                     method != NULL && strcmp(method, "<init>") == 0
                         ? "(Ljava/io/File;Lnet/minecraft/client/Minecraft;)V"
                         : NULL,
                     pc, f.operation);
    e->file = f;
    return s;
}

mc_client_download_options mc_client_download_default_options(void) {
    return (mc_client_download_options){128u, 1048576u, mc_java_http_default_options(),
                                        mc_java_xml_default_options()};
}

mc_client_download_status mc_client_download_classes_create(mc_java_files *files,
                                                            mc_java_system_output *system,
                                                            mc_client_download_classes **out,
                                                            mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (files == NULL || out == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ARGUMENT, NULL, NULL, NULL, -1,
                                "download namespace arguments");
    mc_client_download_classes *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ALLOCATION, NULL, NULL, NULL, -1,
                                "download namespace");
    if (mtx_init(&c->mutex, mtx_recursive) != thrd_success) {
        free(c);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, NULL, NULL, NULL, -1,
                                "download namespace monitor");
    }
    c->holds = 1;
    *out = c;
    mc_java_file_error fe = {0};
    if (mc_java_files_retain(files, &fe) != MC_JAVA_FILE_OK)
        return mc_download_file_error(e, fe, -1, NULL);
    c->files = files;
    if (system != NULL) {
        if (mc_java_system_output_retain(system, &e->print) != MC_JAVA_PRINT_OK)
            return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, NULL, NULL, NULL, -1,
                                    "download shared System namespace hold");
        c->system = system;
    }
    c->ready = true;
    return MC_CLIENT_DOWNLOAD_OK;
}

mc_client_download_status mc_client_download_classes_release(mc_client_download_classes **slot,
                                                             mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (slot == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ARGUMENT, NULL, NULL, NULL, -1,
                                "download namespace occurrence");
    mc_client_download_classes *c = *slot;
    if (c == NULL)
        return MC_CLIENT_DOWNLOAD_OK;
    if (mtx_trylock(&c->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "download namespace active");
    if (c->holds > 1) {
        --c->holds;
        mtx_unlock(&c->mutex);
        *slot = NULL;
        return MC_CLIENT_DOWNLOAD_OK;
    }
    c->retiring = true;
    if (mc_java_system_output_release(&c->system, &e->print) != MC_JAVA_PRINT_OK) {
        mtx_unlock(&c->mutex);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "download System namespace cleanup");
    }
    mc_java_file_error fe = {0};
    if (mc_java_files_release(&c->files, &fe) != MC_JAVA_FILE_OK) {
        mtx_unlock(&c->mutex);
        return mc_download_file_error(e, fe, -1, NULL);
    }
    for (size_t i = 0; i < 4u; ++i)
        mc_world_info_name_release(c->literals[i]);
    mtx_unlock(&c->mutex);
    mtx_destroy(&c->mutex);
    free(c);
    *slot = NULL;
    return MC_CLIENT_DOWNLOAD_OK;
}

mc_client_download_status mc_download_literal(mc_client_download_classes *c, unsigned int index,
                                              mc_world_info_name **out,
                                              mc_client_download_error *e) {
    static const char *const values[] = {"Resource download thread", "resources/",
                                         "http://s3.amazonaws.com/MinecraftResources/",
                                         "Failed to add "};
    if (index >= 4u)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ARGUMENT, NULL, NULL, NULL, -1,
                                "download literal index");
    if (mtx_lock(&c->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, NULL, NULL, NULL, -1,
                                "download literal lock");
    mc_client_download_status status = MC_CLIENT_DOWNLOAD_OK;
    if (c->literals[index] == NULL) {
        const char *text = values[index];
        uint16_t units[64];
        size_t count = strlen(text);
        for (size_t i = 0; i < count; ++i)
            units[i] = (unsigned char)text[i];
        status = mc_download_name(units, count, &c->literals[index], e);
    }
    if (status == MC_CLIENT_DOWNLOAD_OK) {
        if (mc_world_info_name_retain(c->literals[index]) != MC_WORLD_INFO_OK)
            status = mc_download_fail(e, MC_CLIENT_DOWNLOAD_LIMIT, NULL, NULL, NULL, -1,
                                      "download literal hold");
        else
            *out = c->literals[index];
    }
    if (mtx_unlock(&c->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, NULL, NULL, NULL, -1,
                                "download literal unlock");
    return status;
}

mc_client_download_status
mc_client_download_thread_allocate(mc_client_download_classes *classes,
                                   const mc_client_download_options *options,
                                   mc_client_download_thread **out, mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (classes == NULL || out == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ARGUMENT, NULL, NULL, NULL, -1,
                                "cz raw arguments");
    mc_client_download_options o =
        options == NULL ? mc_client_download_default_options() : *options;
    if (o.max_depth == 0 || o.max_depth > 128u || o.max_visits == 0)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ARGUMENT, NULL, NULL, NULL, -1,
                                "cz native work limits");
    mc_client_download_thread *w = calloc(1, sizeof(*w));
    if (w == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ALLOCATION, NULL, NULL, NULL, -1,
                                "cz RAW allocation");
    if (mtx_init(&w->mutex, mtx_recursive) != thrd_success) {
        free(w);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, NULL, NULL, NULL, -1,
                                "cz native monitor");
    }
    atomic_init(&w->holds, 1);
    atomic_init(&w->c, false);
    atomic_init(&w->stop, false);
    atomic_init(&w->retiring, false);
    atomic_init(&w->worker_exited, false);
    w->options = o;
    w->constructor_pc = w->run_pc = -1;
    if (mtx_lock(&classes->mutex) != thrd_success) {
        mtx_destroy(&w->mutex);
        free(w);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, NULL, NULL, NULL, -1,
                                "cz namespace admission");
    }
    if (!classes->ready || classes->retiring || classes->holds == SIZE_MAX) {
        mtx_unlock(&classes->mutex);
        mtx_destroy(&w->mutex);
        free(w);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz namespace retiring");
    }
    ++classes->holds;
    w->classes = classes;
    mtx_unlock(&classes->mutex);
    *out = w;
    return MC_CLIENT_DOWNLOAD_OK;
}

mc_client_download_status mc_client_download_thread_construct(mc_client_download_thread *w,
                                                              mc_java_file *parent,
                                                              client_application *app,
                                                              mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (w == NULL || e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    if (mtx_trylock(&w->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz constructor active");
    if (w->constructed || w->failed || w->thread_constructed || atomic_load(&w->retiring)) {
        mtx_unlock(&w->mutex);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_STATE, NULL, NULL, NULL, -1,
                                "cz constructor one-shot");
    }
    /* Thread() constructs before source c6/b11; it does not start an OS thread.
     * Selected ordinary creator: no SecurityManager/custom ThreadGroup/profile.
     * Native default-name sequence is shared by this actual Thread() entry. */
    uint32_t next = (uint32_t)atomic_fetch_add(&default_thread_names, 1u);
    char digits[32];
    int count =
        snprintf(digits, sizeof(digits), "Thread-%ld",
                 next <= INT32_MAX ? (long)next : (long)((int64_t)next - INT64_C(4294967296)));
    uint16_t units[32];
    if (count < 0 || (size_t)count >= sizeof(digits)) {
        mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "cz", "<init>",
                         "(Ljava/io/File;Lnet/minecraft/client/Minecraft;)V", 1,
                         "Thread default name formatting");
        goto done;
    }
    for (size_t i = 0; i < (size_t)count; ++i)
        units[i] = (unsigned char)digits[i];
    if (mc_download_name(units, (size_t)count, &w->thread_name, e) != MC_CLIENT_DOWNLOAD_OK)
        goto done;
    w->thread_constructed = true;
    w->constructor_pc = 1;
    atomic_store(&w->c, false);
    w->constructor_pc = 6;
    if (!mc_client_application_resource_thread_borrow(app, &w->counted)) {
        mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                         "cz actual App native borrow at b11");
        goto done;
    }
    w->b = app;
    w->constructor_pc = 11;
    mc_world_info_name *name = NULL;
    if (mc_download_literal(w->classes, 0, &name, e) != MC_CLIENT_DOWNLOAD_OK) {
        mc_download_caller(e, "cz", "<init>", "(Ljava/io/File;Lnet/minecraft/client/Minecraft;)V",
                           15);
        goto done;
    }
    mc_world_info_name_release(w->thread_name);
    w->thread_name = name;
    w->constructor_pc = 17;
    w->daemon = true;
    w->constructor_pc = 22;
    mc_java_file_error fe = {0};
    if (mc_java_file_allocate(w->classes->files, &w->pending_file, &fe) != MC_JAVA_FILE_OK) {
        mc_download_file_error(e, fe, 26, "<init>");
        goto done;
    }
    mc_world_info_name *child = NULL;
    if (mc_download_literal(w->classes, 1, &child, e) != MC_CLIENT_DOWNLOAD_OK) {
        mc_download_caller(e, "cz", "<init>", "(Ljava/io/File;Lnet/minecraft/client/Minecraft;)V",
                           31);
        goto done;
    }
    mc_java_file_status fs = mc_java_file_construct_child(w->pending_file, parent, child, &fe);
    mc_world_info_name_release(child);
    if (fs != MC_JAVA_FILE_OK) {
        mc_download_file_error(e, fe, 33, "<init>");
        goto done;
    }
    w->a = w->pending_file;
    w->pending_file = NULL;
    w->constructor_pc = 36;
    bool exists = false;
    if (mc_java_file_exists(w->a, &exists, &fe) != MC_JAVA_FILE_OK) {
        mc_download_file_error(e, fe, 43, "<init>");
        goto done;
    }
    if (!exists) {
        bool made = false;
        if (mc_java_file_mkdirs(w->a, &made, &fe) != MC_JAVA_FILE_OK) {
            mc_download_file_error(e, fe, 53, "<init>");
            goto done;
        }
        if (!made) {
            mc_download_fail(e, MC_CLIENT_DOWNLOAD_UNIMPLEMENTED, "cz", "<init>",
                             "(Ljava/io/File;Lnet/minecraft/client/Minecraft;)V", 59,
                             "actual RuntimeException allocation/Throwable owner");
            goto done;
        }
    }
    w->constructor_pc = 89;
    w->constructed = true;
    e->source_returned = true;
done:
    if (e->status != MC_CLIENT_DOWNLOAD_OK) {
        w->failed = true;
        w->failure = *e;
    }
    mtx_unlock(&w->mutex);
    return e->status;
}

static int native_thread_name(void) {
#ifdef _WIN32
    typedef HRESULT(WINAPI * set_description)(HANDLE, PCWSTR);
    HMODULE kernel = GetModuleHandleW(L"Kernel32.dll");
    FARPROC raw = kernel == NULL ? NULL : GetProcAddress(kernel, "SetThreadDescription");
    if (raw == NULL)
        return (int)GetLastError();
    set_description function = NULL;
    _Static_assert(sizeof(function) == sizeof(raw), "Windows thread-name function width");
    memcpy(&function, &raw, sizeof(function));
    HRESULT result = function(GetCurrentThread(), L"Resource download thread");
    return FAILED(result) ? (int)result : 0;
#else
    return pthread_setname_np(pthread_self(), "Resource downlo");
#endif
}

static int worker(void *context) {
    mc_client_download_thread *w = context;
    if (mtx_lock(&w->mutex) != thrd_success) {
        atomic_store(&w->worker_exited, true);
        return 1;
    }
    mc_client_download_error *e = &w->failure;
    *e = (mc_client_download_error){0};
    int named = native_thread_name();
    if (named != 0) {
        mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "java/lang/Thread", "start", "()V", 64,
                         "native thread name publication");
        e->native_error = named;
    } else
        (void)mc_download_run(w, e);
    w->run_returned = e->source_returned;
    w->completed = true;
    const int native_result = e->status == MC_CLIENT_DOWNLOAD_OK ? 0 : 1;
    mtx_unlock(&w->mutex);
    atomic_store(&w->worker_exited, true);
    return native_result;
}

mc_client_download_status mc_client_download_thread_start(mc_client_download_thread *w,
                                                          mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (w == NULL || e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    if (mtx_trylock(&w->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz Thread.start active");
    if (!w->constructed || w->started || atomic_load(&w->retiring)) {
        mtx_unlock(&w->mutex);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_STATE, "java/lang/Thread", "start", "()V", 0,
                                "Thread.start one-shot native profile");
    }
    int result = thrd_create(&w->thread, worker, w);
    if (result != thrd_success) {
        mtx_unlock(&w->mutex);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "java/lang/Thread", "start", "()V",
                                64, "actual native thread creation");
    }
    w->started = true;
    mtx_unlock(&w->mutex);
    e->source_returned = true;
    return MC_CLIENT_DOWNLOAD_OK;
}

mc_client_download_status mc_client_download_thread_cancel(mc_client_download_thread *w,
                                                           mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (w == NULL || e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    if (atomic_load(&w->retiring) || !w->constructed)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_STATE, NULL, NULL, NULL, -1,
                                "cz retiring/nonconstructed source admission");
    atomic_store_explicit(&w->c, true, memory_order_seq_cst);
    e->source_returned = true;
    return MC_CLIENT_DOWNLOAD_OK;
}

mc_client_download_status mc_client_download_thread_scan(mc_client_download_thread *w,
                                                         mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (w == NULL || e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    if (mtx_trylock(&w->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz source scan active");
    if (!w->constructed || w->started || atomic_load(&w->retiring)) {
        mtx_unlock(&w->mutex);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_STATE, NULL, NULL, NULL, -1,
                                "cz source scan admission");
    }
    mc_client_download_status s = mc_download_scan(w, e);
    if (s != MC_CLIENT_DOWNLOAD_OK)
        mc_download_caller(e, "cz", "a", "()V", 7);
    if (s == MC_CLIENT_DOWNLOAD_OK)
        e->source_returned = true;
    else
        w->failure = *e;
    mtx_unlock(&w->mutex);
    return s;
}

mc_client_download_status mc_client_download_thread_retain(mc_client_download_thread *w,
                                                           mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (w == NULL || e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    if (mtx_trylock(&w->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz retain active");
    size_t count = atomic_load(&w->holds);
    mc_client_download_status s = MC_CLIENT_DOWNLOAD_OK;
    if (atomic_load(&w->retiring) || count == SIZE_MAX)
        s = mc_download_fail(e, MC_CLIENT_DOWNLOAD_STATE, NULL, NULL, NULL, -1,
                             "cz retiring/overflow retain");
    else
        atomic_fetch_add(&w->holds, 1u);
    mtx_unlock(&w->mutex);
    return s;
}

mc_client_download_status mc_download_call_retire(mc_client_download_thread *w,
                                                  mc_client_download_error *e) {
    download_call *c = &w->call;
    if (mc_sound_pool_file_call_release(&c->sound, &e->sound) != MC_SOUND_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz retained sound prefix");
    if (mc_java_print_stream_release(&c->print, &e->print) != MC_JAVA_PRINT_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz retained PrintStream occurrence");
    if (mc_java_data_output_release(&c->dos, &e->output) != MC_JAVA_OUTPUT_OK ||
        mc_java_file_output_release(&c->fos, &e->output) != MC_JAVA_OUTPUT_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz retained binary output cleanup");
    if (mc_java_url_data_input_release(&c->dis, &e->url) != MC_JAVA_URL_OK ||
        mc_java_http_release(&c->input, &e->url) != MC_JAVA_URL_OK ||
        mc_java_url_release(&c->url, &e->url) != MC_JAVA_URL_OK ||
        mc_java_uri_release(&c->uri, &e->url) != MC_JAVA_URL_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz retained URL/input cleanup");
    mc_java_file_error fe = {0};
    if (mc_java_file_release(&c->parent, &fe) != MC_JAVA_FILE_OK ||
        mc_java_file_release(&c->file, &fe) != MC_JAVA_FILE_OK)
        return mc_download_file_error(e, fe, -1, NULL);
    mc_world_info_name_release(c->key);
    mc_world_info_name_release(c->escaped);
    mc_world_info_name_release(c->entry_prefix);
    mc_world_info_name_release(c->prefix);
    mc_world_info_name_release(c->suffix);
    mc_world_info_name_release(c->message);
    free(c->buffer);
    *c = (download_call){0};
    return MC_CLIENT_DOWNLOAD_OK;
}

mc_client_download_status mc_client_download_thread_release(mc_client_download_thread **slot,
                                                            mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (slot == NULL || e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    mc_client_download_thread *w = *slot;
    if (w == NULL)
        return MC_CLIENT_DOWNLOAD_OK;
    size_t count = atomic_load(&w->holds);
    while (count > 1) {
        if (atomic_compare_exchange_weak(&w->holds, &count, count - 1u)) {
            *slot = NULL;
            return MC_CLIENT_DOWNLOAD_OK;
        }
    }
    atomic_store(&w->retiring, true);
    atomic_store(&w->stop, true);
    if (mtx_trylock(&w->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz running native retirement");
    if (w->started && !w->completed && !atomic_load(&w->worker_exited)) {
        mtx_unlock(&w->mutex);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz worker termination pending");
    }
    if (w->started && !w->joined) {
        int result = 0;
        if (thrd_join(w->thread, &result) != thrd_success) {
            mtx_unlock(&w->mutex);
            return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, NULL, NULL, NULL, -1,
                                    "cz native join retry");
        }
        w->joined = true;
    }
    if (mc_download_call_retire(w, e) != MC_CLIENT_DOWNLOAD_OK)
        goto blocked;
    while (w->depth > 0) {
        download_frame *f = &w->frames[w->depth - 1u];
        mc_java_file_error fe = {0};
        if (mc_java_file_array_release(&f->array, &fe) != MC_JAVA_FILE_OK) {
            mc_download_file_error(e, fe, -1, NULL);
            goto blocked;
        }
        mc_world_info_name_release(f->prefix);
        mc_world_info_name_release(f->name);
        *f = (download_frame){0};
        --w->depth;
    }
    mc_java_xml_list_release(&w->selection);
    mc_java_xml_list_release(&w->contents);
    if (mc_java_xml_document_release(&w->document, &e->xml) != MC_JAVA_XML_OK) {
        mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1, "cz DOM cleanup");
        goto blocked;
    }
    if (mc_java_xml_builder_release(&w->builder, &e->xml) != MC_JAVA_XML_OK ||
        mc_java_xml_factory_release(&w->factory, &e->xml) != MC_JAVA_XML_OK) {
        mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                         "cz builder/factory cleanup");
        goto blocked;
    }
    if (mc_java_http_release(&w->listing, &e->url) != MC_JAVA_URL_OK ||
        mc_java_url_release(&w->listing_url, &e->url) != MC_JAVA_URL_OK) {
        mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1, "cz listing cleanup");
        goto blocked;
    }
    mc_java_file_error fe = {0};
    if (mc_java_file_release(&w->pending_file, &fe) != MC_JAVA_FILE_OK ||
        mc_java_file_release(&w->a, &fe) != MC_JAVA_FILE_OK) {
        mc_download_file_error(e, fe, -1, NULL);
        goto blocked;
    }
    if (mc_client_download_classes_release(&w->classes, e) != MC_CLIENT_DOWNLOAD_OK)
        goto blocked;
    mc_client_application_resource_thread_release(w->b, w->counted);
    w->b = NULL;
    w->counted = false;
    mc_world_info_name_release(w->thread_name);
    free(w->frames);
    mtx_unlock(&w->mutex);
    mtx_destroy(&w->mutex);
    free(w);
    *slot = NULL;
    return MC_CLIENT_DOWNLOAD_OK;
blocked:
    mtx_unlock(&w->mutex);
    return e->status;
}

mc_client_download_status mc_client_download_thread_retire(mc_client_download_thread **slot,
                                                           mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (slot == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ARGUMENT, NULL, NULL, NULL, -1,
                                "cz strict retirement occurrence");
    mc_client_download_thread *w = *slot;
    if (w == NULL)
        return MC_CLIENT_DOWNLOAD_OK;
    if (mtx_trylock(&w->mutex) != thrd_success) {
        /* A running worker owns this monitor. Request native stop without
         * consuming the enclosing occurrence or writing source c/J. */
        if (atomic_load(&w->holds) == 1u)
            atomic_store(&w->stop, true);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz strict retirement waits for active monitor");
    }
    if (atomic_load(&w->holds) != 1u) {
        mtx_unlock(&w->mutex);
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz external occurrences prevent enclosing retirement");
    }
    /* Retain tests this flag under the same monitor. Freeze new occurrences
     * before releasing the monitor and entering the canonical cleanup body. */
    atomic_store(&w->retiring, true);
    if (mtx_unlock(&w->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, NULL, NULL, NULL, -1,
                                "cz strict retirement monitor unlock");
    return mc_client_download_thread_release(slot, e);
}

mc_client_download_status mc_client_download_thread_inspect(mc_client_download_thread *w,
                                                            mc_client_download_view *out,
                                                            mc_client_download_error *e) {
    if (e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    *e = (mc_client_download_error){0};
    if (w == NULL || out == NULL || e == NULL)
        return MC_CLIENT_DOWNLOAD_ARGUMENT;
    if (mtx_trylock(&w->mutex) != thrd_success)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_BUSY, NULL, NULL, NULL, -1,
                                "cz live source inspection");
    *out = (mc_client_download_view){atomic_load(&w->holds),
                                     w->b,
                                     w->a,
                                     w->thread_name,
                                     atomic_load(&w->c),
                                     w->daemon,
                                     w->thread_constructed,
                                     w->started,
                                     w->completed,
                                     w->run_returned,
                                     atomic_load(&w->retiring),
                                     w->counted,
                                     w->constructor_pc,
                                     w->run_pc,
                                     w->local_visits,
                                     w->registrations,
                                     w->writes,
                                     &w->failure};
    mtx_unlock(&w->mutex);
    return MC_CLIENT_DOWNLOAD_OK;
}
