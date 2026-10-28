#include "java_file_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

mc_java_file_status mc_file_result(mc_java_file_error *e, mc_java_file_status s, const char *op,
                                   int code) {
    if (e != NULL)
        *e = (mc_java_file_error){s, op, code};
    return s;
}

mc_java_file_options mc_java_file_default_options(void) {
    return (mc_java_file_options){1048576u, 8388608u, 1048576u};
}

mc_java_file_status mc_file_name_hold(mc_world_info_name *name, mc_java_file_error *e) {
    if (name == NULL)
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    mc_world_info_status s = mc_world_info_name_retain(name);
    return s == MC_WORLD_INFO_OK
               ? mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0)
               : mc_file_result(
                     e, s == MC_WORLD_INFO_LIMIT ? MC_JAVA_FILE_LIMIT : MC_JAVA_FILE_STATE_ERROR,
                     "File String hold", 0);
}

/* Lifetime entrants already own/protect a real counted occurrence. No weak
 * pointer is rescued by this guard. The mutex is ONLY the lazy name cache. */
typedef struct namespace_cache_scope {
    mc_java_files *files;
    struct namespace_cache_scope *previous;
} namespace_cache_scope;
#ifdef _MSC_VER
static __declspec(thread) namespace_cache_scope *cache_scope;
#else
static _Thread_local namespace_cache_scope *cache_scope;
#endif

static bool own_cache(const mc_java_files *files) {
    for (namespace_cache_scope *scope = cache_scope; scope != NULL; scope = scope->previous)
        if (scope->files == files)
            return true;
    return false;
}

static void namespace_leave(mc_java_files *files) {
    atomic_fetch_sub_explicit(&files->native_calls, 1, memory_order_release);
}

static mc_java_file_status namespace_enter(mc_java_files *files, bool allow_quarantine,
                                           mc_java_file_error *e) {
    size_t calls = atomic_load_explicit(&files->native_calls, memory_order_acquire);
    do {
        if (calls == SIZE_MAX)
            return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File namespace entered-call bound", 0);
    } while (!atomic_compare_exchange_weak_explicit(&files->native_calls, &calls, calls + 1,
                                                    memory_order_acq_rel, memory_order_acquire));
    if (own_cache(files) || atomic_load_explicit(&files->retiring, memory_order_acquire)) {
        namespace_leave(files);
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File namespace reentry/terminal", 0);
    }
    if (!allow_quarantine && atomic_load_explicit(&files->quarantined, memory_order_acquire)) {
        namespace_leave(files);
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File namespace cache quarantine",
                              files->native_failure);
    }
    return MC_JAVA_FILE_OK;
}

mc_java_file_status mc_file_namespace_admit(mc_java_files *files, mc_java_file_error *e) {
    const mc_java_file_status status = namespace_enter(files, true, e);
    if (status != MC_JAVA_FILE_OK)
        return status;
    namespace_leave(files);
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_files_create(const mc_java_file_options *options, mc_java_files **out,
                                         mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File namespace output", 0);
    mc_java_files *files = calloc(1, sizeof(*files));
    if (files == NULL)
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File namespace allocation", 0);
    atomic_init(&files->holds, 1);
    atomic_init(&files->native_calls, 0);
    atomic_init(&files->active, false);
    atomic_init(&files->retiring, false);
    atomic_init(&files->quarantined, false);
    files->options = options == NULL ? mc_java_file_default_options() : *options;
    const int initialized = mtx_init(&files->mutex, mtx_recursive);
    if (initialized != thrd_success) {
        free(files);
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File namespace cache mutex",
                              initialized);
    }
    *out = files;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_files_retain(mc_java_files *files, mc_java_file_error *e) {
    if (files == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File namespace retain", 0);
    mc_java_file_status status = namespace_enter(files, false, e);
    if (status != MC_JAVA_FILE_OK)
        return status;
    size_t holds = atomic_load_explicit(&files->holds, memory_order_acquire);
    for (;;) {
        if (holds == 0 || holds == SIZE_MAX) {
            status = mc_file_result(e, holds == 0 ? MC_JAVA_FILE_STATE_ERROR : MC_JAVA_FILE_LIMIT,
                                    "File namespace holds", 0);
            break;
        }
        if (atomic_compare_exchange_weak_explicit(&files->holds, &holds, holds + 1,
                                                  memory_order_acq_rel, memory_order_acquire))
            break;
    }
    namespace_leave(files);
    return status == MC_JAVA_FILE_OK ? mc_file_result(e, status, NULL, 0) : status;
}

mc_java_file_status mc_java_files_release(mc_java_files **slot, mc_java_file_error *e) {
    if (slot == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File namespace slot", 0);
    mc_java_files *files = *slot;
    if (files == NULL)
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    mc_java_file_status status = namespace_enter(files, true, e);
    if (status != MC_JAVA_FILE_OK)
        return status;
    size_t holds = atomic_load_explicit(&files->holds, memory_order_acquire);
    while (holds > 1) {
        if (atomic_compare_exchange_weak_explicit(&files->holds, &holds, holds - 1,
                                                  memory_order_acq_rel, memory_order_acquire)) {
            namespace_leave(files);
            *slot = NULL;
            return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
        }
    }
    if (holds == 0) {
        namespace_leave(files);
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File namespace zero occurrence", 0);
    }
    if (atomic_load_explicit(&files->quarantined, memory_order_acquire)) {
        namespace_leave(files);
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File namespace final cache quarantine",
                              files->native_failure);
    }
    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit(&files->retiring, &expected, true,
                                                 memory_order_acq_rel, memory_order_acquire)) {
        namespace_leave(files);
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File namespace final retry", 0);
    }
    const size_t calls = atomic_load_explicit(&files->native_calls, memory_order_acquire);
    const bool active = atomic_load_explicit(&files->active, memory_order_acquire);
    const size_t current = atomic_load_explicit(&files->holds, memory_order_acquire);
    if (calls != 1 || active || current != 1) {
        atomic_store_explicit(&files->retiring, false, memory_order_release);
        namespace_leave(files);
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File namespace final entered/waiter retry", 0);
    }
    atomic_store_explicit(&files->holds, 0, memory_order_release);
    namespace_leave(files);
    mc_world_info_name_release(files->default_parent);
    mtx_destroy(&files->mutex);
    free(files);
    *slot = NULL;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_file_allocate(mc_java_files *files, mc_java_file **out,
                                          mc_java_file_error *e) {
    if (files == NULL || out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File native allocation arguments",
                              0);
    mc_java_file *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File native allocation", 0);
    mc_java_file_status s = mc_java_files_retain(files, e);
    if (s != MC_JAVA_FILE_OK) {
        free(f);
        return s;
    }
    f->files = files;
    f->state.holds = 1;
    f->state.phase = MC_JAVA_FILE_RAW;
    *out = f;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}
#ifdef _WIN32
static bool letter(uint16_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
#endif
static size_t prefix(mc_nbt_string p) {
#ifdef _WIN32
    if (p.length == 0)
        return 0;
    if (p.units[0] == '\\')
        return p.length > 1 && p.units[1] == '\\' ? 2u : 1u;
    if (p.length > 1 && letter(p.units[0]) && p.units[1] == ':')
        return p.length > 2 && p.units[2] == '\\' ? 3u : 2u;
    return 0;
#else
    return p.length != 0 && p.units[0] == '/' ? 1u : 0u;
#endif
}

static bool normalization_reached(mc_nbt_string p) {
#ifdef _WIN32
    uint16_t previous = 0;
    for (size_t i = 0; i < p.length; ++i) {
        uint16_t c = p.units[i];
        if (c == '/' || (c == '\\' && previous == '\\' && i > 1u) || (c == ':' && i > 1u))
            return true;
        previous = c;
    }
    return previous == '\\';
#else
    for (size_t i = 0; i < p.length; ++i)
        if (p.units[i] == '/' &&
            (i + 1u == p.length || (i + 1u < p.length && p.units[i + 1u] == '/')))
            return true;
    return false;
#endif
}

static mc_java_file_status name_copy(mc_java_files *files, const uint16_t *u, size_t n,
                                     mc_world_info_name **out, mc_java_file_error *e) {
    mc_world_info_status s =
        mc_world_info_name_copy((mc_nbt_string){u, n}, files->options.max_string_bytes, out);
    if (s == MC_WORLD_INFO_OK)
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    return mc_file_result(e,
                          s == MC_WORLD_INFO_LIMIT               ? MC_JAVA_FILE_LIMIT
                          : s == MC_WORLD_INFO_ALLOCATION_FAILED ? MC_JAVA_FILE_ALLOCATION_FAILED
                                                                 : MC_JAVA_FILE_INVALID_ARGUMENT,
                          "File String allocation", 0);
}

static mc_java_file_status default_parent(mc_java_files *files, mc_world_info_name **out,
                                          mc_java_file_error *e) {
    mc_java_file_status status = namespace_enter(files, false, e);
    if (status != MC_JAVA_FILE_OK)
        return status;
    const int locked = mtx_lock(&files->mutex);
    if (locked != thrd_success) {
        namespace_leave(files);
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File default-parent mutex lock",
                              locked);
    }
    if (atomic_load_explicit(&files->quarantined, memory_order_acquire)) {
        (void)mtx_unlock(&files->mutex);
        namespace_leave(files);
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File default-parent cache quarantine",
                              files->native_failure);
    }
    namespace_cache_scope scope = {files, cache_scope};
    cache_scope = &scope;
    atomic_store_explicit(&files->active, true, memory_order_release);
    if (files->default_parent == NULL) {
#ifdef _WIN32
        const uint16_t root = '\\';
#else
        const uint16_t root = '/';
#endif
        status = name_copy(files, &root, 1, &files->default_parent, e);
    }
    if (status == MC_JAVA_FILE_OK)
        status = mc_file_name_hold(files->default_parent, e);
    const bool held = status == MC_JAVA_FILE_OK;
    atomic_store_explicit(&files->active, false, memory_order_release);
    const int unlocked = mtx_unlock(&files->mutex);
    if (unlocked != thrd_success) {
        files->native_failure = unlocked;
        if (held)
            files->failed_default_parent_hold = true;
        atomic_store_explicit(&files->quarantined, true, memory_order_release);
        status = mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File uncertain default-parent unlock",
                                unlocked);
    } else if (held)
        *out = files->default_parent;
    cache_scope = scope.previous;
    namespace_leave(files);
    return status;
}

static mc_java_file_status normalize(mc_java_files *files, mc_world_info_name *input,
                                     mc_world_info_name **out, mc_java_file_error *e) {
    mc_nbt_string text = mc_world_info_name_text(input);
    if (text.length > files->options.max_path_units ||
        text.length > (SIZE_MAX / sizeof(uint16_t)) - 1u)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File UTF16 path bound", 0);
    if (!normalization_reached(text)) {
        mc_java_file_status s = mc_file_name_hold(input, e);
        if (s == MC_JAVA_FILE_OK)
            *out = input;
        return s;
    }
#ifndef _WIN32
    bool all_slashes = text.length != 0;
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != '/')
            all_slashes = false;
    if (all_slashes)
        return default_parent(files, out, e);
#endif
    size_t bytes = (text.length + 1u) * sizeof(uint16_t);
    if (bytes > files->options.max_string_bytes)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File normalization scratch bound", 0);
    uint16_t *buffer = malloc(bytes);
    if (buffer == NULL)
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File normalization scratch", 0);
    size_t length = mc_file_normalize_units(text.units, text.length, buffer);
    mc_java_file_status s = name_copy(files, buffer, length, out, e);
    free(buffer);
    return s;
}

static mc_java_file_status resolve(mc_java_files *files, mc_world_info_name *parent,
                                   mc_world_info_name *child, mc_world_info_name **out,
                                   mc_java_file_error *e) {
    mc_nbt_string p = mc_world_info_name_text(parent), c = mc_world_info_name_text(child);
    if (c.length == 0) {
        mc_java_file_status s = mc_file_name_hold(parent, e);
        if (s == MC_JAVA_FILE_OK)
            *out = parent;
        return s;
    }
    size_t pe = p.length, cs = 0;
    bool add = true;
#ifdef _WIN32
    if (p.length == 0) {
        mc_java_file_status s = mc_file_name_hold(child, e);
        if (s == MC_JAVA_FILE_OK)
            *out = child;
        return s;
    }
    bool relative = p.length == 2 && letter(p.units[0]) && p.units[1] == ':';
    if (c.length > 1 && c.units[0] == '\\') {
        if (c.units[1] == '\\')
            cs = 2;
        else if (!relative)
            cs = 1;
        if (c.length == cs) {
            if (p.units[pe - 1u] == '\\')
                --pe;
            if (pe == p.length) {
                mc_java_file_status s = mc_file_name_hold(parent, e);
                if (s == MC_JAVA_FILE_OK)
                    *out = parent;
                return s;
            }
            if (pe == 0) {
                *out = mc_world_info_name_empty();
                return MC_JAVA_FILE_OK;
            }
            return name_copy(files, p.units, pe, out, e);
        }
    }
    if (p.units[pe - 1u] == '\\')
        --pe;
    add = c.units[cs] != '\\' && !relative;
    const uint16_t slash = '\\';
#else
    if (c.units[0] == '/') {
        if (p.length == 1 && p.units[0] == '/') {
            mc_java_file_status s = mc_file_name_hold(child, e);
            if (s == MC_JAVA_FILE_OK)
                *out = child;
            return s;
        }
        add = false;
    } else if (p.length == 1 && p.units[0] == '/')
        add = false;
    const uint16_t slash = '/';
#endif
    size_t tail = c.length - cs;
    if (pe > SIZE_MAX - tail - (add ? 1u : 0u))
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File resolve length", 0);
    size_t n = pe + tail + (add ? 1u : 0u);
    if (n > files->options.max_path_units || n > SIZE_MAX / sizeof(uint16_t) ||
        n * sizeof(uint16_t) > files->options.max_string_bytes)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File resolve bound", 0);
    uint16_t *u = malloc(n * sizeof(uint16_t));
    if (u == NULL)
        return mc_file_result(e, MC_JAVA_FILE_ALLOCATION_FAILED, "File resolve scratch", 0);
    if (pe)
        memcpy(u, p.units, pe * sizeof(*u));
    if (add)
        u[pe++] = slash;
    memcpy(u + pe, c.units + cs, tail * sizeof(*u));
    mc_java_file_status s = name_copy(files, u, n, out, e);
    free(u);
    return s;
}

static mc_java_file_status construct(mc_java_file *f, mc_java_file *parent,
                                     mc_world_info_name *parent_text, mc_world_info_name *input,
                                     bool child_mode, bool private_child, bool string_parent,
                                     mc_java_file_error *e) {
    if (f == NULL || f->state.phase != MC_JAVA_FILE_RAW || f->state.active)
        return mc_file_result(e,
                              f == NULL ? MC_JAVA_FILE_INVALID_ARGUMENT : MC_JAVA_FILE_STATE_ERROR,
                              "File constructor receiver", 0);
    f->state.active = true;
    f->state.phase = MC_JAVA_FILE_CONSTRUCTING;
    mc_java_file_status status;
    mc_java_file *held_parent = NULL;
    mc_world_info_name *normal = NULL, *p = NULL, *result = NULL;
    if (input == NULL) {
        status =
            mc_file_result(e, MC_JAVA_FILE_SOURCE_NULL_STRING, "File null child/path String", 0);
        goto failed;
    }
    status = mc_file_name_hold(input, e);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    f->pending_input = input;
    if (string_parent && parent_text != NULL) {
        status = mc_file_name_hold(parent_text, e);
        if (status != MC_JAVA_FILE_OK)
            goto failed;
        f->pending_parent = parent_text;
    }
    if (parent != NULL &&
        (parent->files != f->files || parent->state.phase != MC_JAVA_FILE_READY)) {
        status = mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File parent namespace/state", 0);
        goto failed;
    }
    if (parent != NULL && !private_child) {
        status = mc_java_file_retain(parent, e);
        if (status != MC_JAVA_FILE_OK)
            goto failed;
        held_parent = parent;
    }
    bool resolve_parent = child_mode && (string_parent ? parent_text != NULL : parent != NULL);
    if (resolve_parent) {
        mc_world_info_name *source_parent = string_parent ? parent_text : parent->state.path;
        /* Java evaluates the first resolve argument BEFORE normalize(child).
         * In the two-String overload that includes normalize(parent). */
        if (!private_child && mc_world_info_name_text(source_parent).length == 0)
            status = default_parent(f->files, &p, e);
        else if (string_parent)
            status = normalize(f->files, source_parent, &p, e);
        else {
            status = mc_file_name_hold(source_parent, e);
            if (status == MC_JAVA_FILE_OK)
                p = source_parent;
        }
        if (status != MC_JAVA_FILE_OK)
            goto failed;
    }
    status = private_child ? mc_file_name_hold(input, e) : normalize(f->files, input, &normal, e);
    if (private_child && status == MC_JAVA_FILE_OK)
        normal = input;
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    if (resolve_parent) {
        status = resolve(f->files, p, normal, &result, e);
        if (status != MC_JAVA_FILE_OK)
            goto failed;
    } else {
        result = normal;
        normal = NULL;
    }
    f->state.path = result;
    f->state.prefix_length = private_child && parent != NULL
                                 ? parent->state.prefix_length
                                 : prefix(mc_world_info_name_text(result));
    f->state.phase = MC_JAVA_FILE_READY;
    f->state.active = false;
    mc_world_info_name_release(p);
    mc_world_info_name_release(normal);
    mc_world_info_name_release(f->pending_input);
    f->pending_input = NULL;
    mc_world_info_name_release(f->pending_parent);
    f->pending_parent = NULL;
    (void)mc_java_file_release(&held_parent, NULL);
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
failed:
    mc_world_info_name_release(p);
    mc_world_info_name_release(normal);
    f->state.phase = MC_JAVA_FILE_FAILED;
    f->state.active = false;
    (void)mc_java_file_release(&held_parent, NULL);
    return status;
}

mc_java_file_status mc_java_file_construct_string(mc_java_file *f, mc_world_info_name *s,
                                                  mc_java_file_error *e) {
    return construct(f, NULL, NULL, s, false, false, false, e);
}

mc_java_file_status mc_java_file_construct_child(mc_java_file *f, mc_java_file *parent,
                                                 mc_world_info_name *s, mc_java_file_error *e) {
    return construct(f, parent, NULL, s, true, false, false, e);
}

mc_java_file_status mc_java_file_construct_strings(mc_java_file *f, mc_world_info_name *parent,
                                                   mc_world_info_name *s, mc_java_file_error *e) {
    return construct(f, NULL, parent, s, true, false, true, e);
}

mc_java_file_status mc_file_construct_private_child(mc_java_file *f, mc_java_file *parent,
                                                    mc_world_info_name *s, mc_java_file_error *e) {
    return construct(f, parent, NULL, s, true, true, false, e);
}

mc_java_file_status mc_file_private_child(mc_java_files *files, mc_java_file *parent,
                                          mc_world_info_name *name, mc_java_file **out,
                                          mc_java_file_error *e) {
    mc_java_file *f = NULL;
    mc_java_file_status s = mc_java_file_allocate(files, &f, e);
    if (s == MC_JAVA_FILE_OK)
        s = construct(f, parent, NULL, name, true, true, false, e);
    if (s != MC_JAVA_FILE_OK) {
        (void)mc_java_file_release(&f, NULL);
        return s;
    }
    *out = f;
    return MC_JAVA_FILE_OK;
}

mc_java_file_status mc_java_file_retain(mc_java_file *f, mc_java_file_error *e) {
    if (f == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File retain", 0);
    if (f->state.active || f->state.phase == MC_JAVA_FILE_RETIRING)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File active/terminal retain", 0);
    if (f->state.holds == SIZE_MAX)
        return mc_file_result(e, MC_JAVA_FILE_LIMIT, "File holds", 0);
    ++f->state.holds;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_file_clear_failure(mc_java_file *f, mc_java_file_error *e) {
    if (f == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File failure cleanup", 0);
    if (f->state.active)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File active cleanup", 0);
    f->state.active = true;
    mc_java_file_status s = mc_java_file_array_release(&f->failed_array, e);
    f->state.active = false;
    f->state.has_failed_array = f->failed_array != NULL;
    return s;
}

mc_java_file_status mc_java_file_release(mc_java_file **slot, mc_java_file_error *e) {
    if (slot == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File release slot", 0);
    mc_java_file *f = *slot;
    if (f == NULL)
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    if (f->state.active)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File active retirement", 0);
    if (f->state.holds > 1) {
        --f->state.holds;
        *slot = NULL;
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    mc_java_file_status namespace_status = mc_file_namespace_admit(f->files, e);
    if (namespace_status != MC_JAVA_FILE_OK)
        return namespace_status;
    f->state.phase = MC_JAVA_FILE_RETIRING;
    mc_java_file_status s = mc_java_file_clear_failure(f, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    f->state.phase = MC_JAVA_FILE_RETIRING;
    f->state.active = true;
    s = mc_java_files_release(&f->files, e);
    if (s != MC_JAVA_FILE_OK) {
        f->state.active = false;
        return s;
    }
    mc_world_info_name_release(f->state.path);
    mc_world_info_name_release(f->pending_input);
    mc_world_info_name_release(f->pending_parent);
    *slot = NULL;
    free(f);
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_java_file_status mc_java_file_read_state(const mc_java_file *f, mc_java_file_state *out,
                                            mc_java_file_error *e) {
    if (f == NULL || out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File state", 0);
    *out = f->state;
    return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
}

mc_world_info_name *mc_java_file_path(const mc_java_file *f) {
    return f == NULL ? NULL : f->state.path;
}

mc_java_files *mc_java_file_namespace(const mc_java_file *f) {
    return f == NULL ? NULL : f->files;
}

mc_java_file_status mc_file_ready(mc_java_file *f, mc_java_file_error *e) {
    if (f == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File receiver", 0);
    if (f->state.active)
        return mc_file_result(e, MC_JAVA_FILE_BUSY, "File active operation", 0);
    if (f->state.phase != MC_JAVA_FILE_READY)
        return mc_file_result(e, MC_JAVA_FILE_STATE_ERROR, "File nonready operation", 0);
    return MC_JAVA_FILE_OK;
}

mc_java_file_status mc_java_file_get_name(mc_java_file *f, mc_world_info_name **out,
                                          mc_java_file_error *e) {
    if (out == NULL)
        return mc_file_result(e, MC_JAVA_FILE_INVALID_ARGUMENT, "File name output", 0);
    mc_java_file_status s = mc_file_ready(f, e);
    if (s != MC_JAVA_FILE_OK)
        return s;
    mc_nbt_string p = mc_world_info_name_text(f->state.path);
    size_t begin = f->state.prefix_length;
    for (size_t i = 0; i < p.length; ++i) {
#ifdef _WIN32
        if (p.units[i] == '\\')
            begin = i + 1u;
#else
        if (p.units[i] == '/')
            begin = i + 1u;
#endif
    }
    if (begin == 0) {
        s = mc_file_name_hold(f->state.path, e);
        if (s == MC_JAVA_FILE_OK)
            *out = f->state.path;
        return s;
    }
    if (begin == p.length) {
        *out = mc_world_info_name_empty();
        return mc_file_result(e, MC_JAVA_FILE_OK, NULL, 0);
    }
    return name_copy(f->files, p.units + begin, p.length - begin, out, e);
}

bool mc_file_invalid(const mc_java_file *f) {
    mc_nbt_string p = mc_world_info_name_text(f->state.path);
    for (size_t i = 0; i < p.length; ++i)
        if (p.units[i] == 0)
            return true;
    return false;
}
