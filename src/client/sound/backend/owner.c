#include "internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* A leaf regression build replaces only this host allocation call. The normal
 * backend has no allocator option or runtime fault-injection state. */
#ifdef MC_SOUND_BACKEND_CALLOC
void *MC_SOUND_BACKEND_CALLOC(size_t count, size_t bytes);
#else
#define MC_SOUND_BACKEND_CALLOC calloc
#endif

typedef union allocation_header {
    long double long_double_alignment;
    void *pointer_alignment;
    uint64_t integer_alignment;
    size_t bytes;
} allocation_header;

mc_sound_backend_status backend_fail(mc_sound_backend *b, mc_sound_backend_status status,
                                     const char *operation, int code) {
    if (b->state_ready)
        mtx_lock(&b->state_mutex);
    b->state.last_error = (mc_sound_backend_error){.status = status, .operation = operation};
    if (status == MC_SOUND_BACKEND_THREAD_FAILED)
        b->state.last_error.thread_code = code;
    else if (status == MC_SOUND_BACKEND_DEVICE_FAILED)
        b->state.last_error.alc_code = code;
    else
        b->state.last_error.codec_code = code;
    if (b->state_ready)
        mtx_unlock(&b->state_mutex);
    return status;
}

mc_sound_status backend_sound_status(mc_sound_backend_status status) {
    switch (status) {
        case MC_SOUND_BACKEND_OK:
            return MC_SOUND_OK;
        case MC_SOUND_BACKEND_INVALID_ARGUMENT:
            return MC_SOUND_INVALID_ARGUMENT;
        case MC_SOUND_BACKEND_BUSY:
            return MC_SOUND_BUSY;
        case MC_SOUND_BACKEND_LIMIT:
            return MC_SOUND_LIMIT;
        case MC_SOUND_BACKEND_ALLOCATION_FAILED:
            return MC_SOUND_ALLOCATION_FAILED;
        case MC_SOUND_BACKEND_CLOSED:
        case MC_SOUND_BACKEND_SOURCE_EXCEPTION:
            return MC_SOUND_SOURCE_EXCEPTION;
        case MC_SOUND_BACKEND_UNSUPPORTED:
            return MC_SOUND_UNSUPPORTED_PATH;
        default:
            return MC_SOUND_IO_FAILED;
    }
}

static mc_sound_status callback_result(mc_sound_backend *b, mc_sound_backend_status status,
                                       mc_sound_error *error) {
    if (b->state_ready)
        mtx_lock(&b->state_mutex);
    if (error != NULL)
        *error = (mc_sound_error){
            .status = backend_sound_status(status),
            .operation = status == MC_SOUND_BACKEND_OK ? NULL : b->state.last_error.operation,
            .source_pc = -1};
    if (b->state_ready)
        mtx_unlock(&b->state_mutex);
    return backend_sound_status(status);
}

void *backend_alloc(mc_sound_backend *b, size_t bytes) {
    if (bytes > SIZE_MAX - sizeof(allocation_header) ||
        bytes + sizeof(allocation_header) >
            b->options.allocation_bytes - b->state.allocation_bytes) {
        backend_fail(b, MC_SOUND_BACKEND_LIMIT, "Sound backend allocation budget", 0);
        return NULL;
    }
    allocation_header *header = MC_SOUND_BACKEND_CALLOC(1, sizeof(*header) + bytes);
    if (header == NULL) {
        backend_fail(b, MC_SOUND_BACKEND_ALLOCATION_FAILED, "Sound backend allocation", 0);
        return NULL;
    }
    header->bytes = bytes + sizeof(*header);
    b->state.allocation_bytes += header->bytes;
    return header + 1;
}

void backend_free(mc_sound_backend *b, void *pointer) {
    if (pointer == NULL)
        return;
    allocation_header *header = (allocation_header *)pointer - 1;
    b->state.allocation_bytes -= header->bytes;
    free(header);
}

char *backend_string(mc_sound_backend *b, const char *text) {
    if (text == NULL)
        return NULL;
    size_t length = 0;
    while (length <= 65536u && text[length] != '\0')
        ++length;
    if (length > 65536u) {
        backend_fail(b, MC_SOUND_BACKEND_LIMIT, "Sound backend string bound", 0);
        return NULL;
    }
    char *copy = backend_alloc(b, length + 1u);
    if (copy != NULL)
        memcpy(copy, text, length + 1u);
    return copy;
}

bool backend_resource_clone(mc_sound_backend *b, const backend_resource *input,
                            backend_resource *out) {
    *out = (backend_resource){0};
    if (input->logical.length > MC_SOUND_POOL_MAX_TEXT_UNITS ||
        input->identity.url_path.length > MC_SOUND_FILE_MAX_PATH_BYTES) {
        backend_fail(b, MC_SOUND_BACKEND_LIMIT, "Sound backend resource text", 0);
        return false;
    }
    out->logical_units = backend_alloc(b, (input->logical.length + 1u) * sizeof(uint16_t));
    if (out->logical_units == NULL)
        goto failure;
    if (input->logical.length != 0)
        memcpy(out->logical_units, input->logical.units, input->logical.length * sizeof(uint16_t));
    out->logical =
        (mc_sound_text){out->logical_units, input->logical.length, input->logical.is_null};
    out->url_units = backend_alloc(b, (input->identity.url_path.length + 1u) * sizeof(uint16_t));
    if (out->url_units == NULL)
        goto failure;
    if (input->identity.url_path.length != 0)
        memcpy(out->url_units, input->identity.url_path.units,
               input->identity.url_path.length * sizeof(uint16_t));
    out->path = backend_string(b, input->identity.native_utf8_path);
    if (out->path == NULL)
        goto failure;
    out->identity = (mc_sound_file_identity){
        out->path,
        {out->url_units, input->identity.url_path.length, input->identity.url_path.is_null}};
    return true;
failure:
    backend_resource_free(b, out);
    return false;
}

bool backend_resource_copy(mc_sound_backend *b, const mc_sound_pool_entry *entry,
                           backend_resource *out) {
    mc_sound_pool_entry_view view;
    if (mc_sound_pool_entry_read(entry, &view, NULL) != MC_SOUND_OK) {
        backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT, "Sound backend entry identity", 0);
        return false;
    }
    const backend_resource input = {.logical = view.logical_name, .identity = view.file};
    return backend_resource_clone(b, &input, out);
}

void backend_resource_free(mc_sound_backend *b, backend_resource *resource) {
    backend_free(b, resource->logical_units);
    backend_free(b, resource->url_units);
    backend_free(b, resource->path);
    *resource = (backend_resource){0};
}

uint32_t backend_text_hash(mc_sound_text text) {
    uint32_t hash = 0;
    for (size_t i = 0; i < text.length; ++i)
        hash = hash * 31u + text.units[i];
    return hash ^ (hash >> 16u);
}

uint32_t backend_name_hash(const char *name) {
    uint32_t hash = 0;
    for (size_t i = 0; name[i] != '\0'; ++i)
        hash = hash * 31u + (uint8_t)name[i];
    return hash ^ (hash >> 16u);
}

mc_sound_backend_options mc_sound_backend_default_options(void) {
    return (mc_sound_backend_options){.output_rate = 44100u,
                                      .normal_channels = 28,
                                      .streaming_channels = 4,
                                      .streaming_buffers = 3,
                                      .streaming_buffer_size = 131072,
                                      .max_file_size = 268435456,
                                      .file_chunk_size = 1048576,
                                      .allocation_bytes = 536870912u,
                                      .max_sources = 1024u,
                                      .max_commands = 4096u,
                                      .max_cached_buffers = 4096u,
                                      .input_bytes = 268435456u,
                                      .pcm_bytes = 268435456u,
                                      .max_device_buffers = 32768u};
}

static bool options_valid(const mc_sound_backend_options *o) {
    return o->output_rate >= 8000u && o->output_rate <= 192000u && o->normal_channels >= 0 &&
           o->normal_channels <= 256 && o->streaming_channels >= 0 && o->streaming_channels <= 64 &&
           o->streaming_buffers >= 0 && o->streaming_buffers <= 64 &&
           o->streaming_buffer_size > 0 && o->streaming_buffer_size <= 1048576 &&
           o->file_chunk_size > 0 && o->file_chunk_size <= 16777216 &&
           o->allocation_bytes >= sizeof(mc_sound_backend) && o->allocation_bytes <= 1073741824u &&
           o->max_sources > 0 && o->max_sources <= 65536u && o->max_commands > 0 &&
           o->max_commands <= 65536u && o->max_cached_buffers > 0 &&
           o->max_cached_buffers <= 65536u && o->input_bytes > 0 &&
           o->input_bytes <= MC_SOUND_STREAM_MAX_BYTES && o->pcm_bytes > 0 &&
           o->pcm_bytes <= MC_VORBIS_CODEC_MAX_PCM_BYTES && o->max_device_buffers > 0 &&
           o->max_device_buffers <= 1048576u;
}

mc_sound_backend_status mc_sound_backend_create(mc_java_random_profile *profile,
                                                const mc_sound_backend_options *options,
                                                mc_sound_backend **out,
                                                mc_sound_backend_error *error) {
    const mc_sound_backend_options selected =
        options == NULL ? mc_sound_backend_default_options() : *options;
    if (profile == NULL || out == NULL || !options_valid(&selected)) {
        if (error != NULL)
            *error = (mc_sound_backend_error){.status = MC_SOUND_BACKEND_INVALID_ARGUMENT,
                                              .operation = "Sound backend factory admission"};
        return MC_SOUND_BACKEND_INVALID_ARGUMENT;
    }
    mc_sound_backend *b = calloc(1, sizeof(*b));
    if (b == NULL) {
        if (error != NULL)
            *error = (mc_sound_backend_error){.status = MC_SOUND_BACKEND_ALLOCATION_FAILED,
                                              .operation = "Sound backend factory allocation"};
        return MC_SOUND_BACKEND_ALLOCATION_FAILED;
    }
    b->profile = profile;
    b->options = selected;
    b->state.allocation_bytes = sizeof(*b);
    b->state.loopback = selected.loopback;
    b->state.manual_service = selected.manual_service;
    atomic_init(&b->dying, false);
    if (selected.device_name != NULL) {
        b->device_name = backend_string(b, selected.device_name);
        if (b->device_name == NULL)
            goto failure;
    }
    int code = mtx_init(&b->queue_mutex, mtx_plain);
    if (code != thrd_success)
        goto thread_failure;
    b->queue_ready = true;
    code = mtx_init(&b->state_mutex, mtx_recursive);
    if (code != thrd_success)
        goto thread_failure;
    b->state_ready = true;
    code = mtx_init(&b->stream_mutex, mtx_plain);
    if (code != thrd_success)
        goto thread_failure;
    b->stream_ready = true;
    code = cnd_init(&b->command_condition);
    if (code != thrd_success)
        goto thread_failure;
    b->command_condition_ready = true;
    code = cnd_init(&b->stream_condition);
    if (code != thrd_success)
        goto thread_failure;
    b->stream_condition_ready = true;
    *out = b;
    if (error != NULL)
        *error = (mc_sound_backend_error){0};
    return MC_SOUND_BACKEND_OK;
thread_failure:
    backend_fail(b, MC_SOUND_BACKEND_THREAD_FAILED, "Sound backend synchronization owner", code);
failure:
    if (error != NULL)
        *error = b->state.last_error;
    if (b->stream_condition_ready)
        cnd_destroy(&b->stream_condition);
    if (b->command_condition_ready)
        cnd_destroy(&b->command_condition);
    if (b->stream_ready)
        mtx_destroy(&b->stream_mutex);
    if (b->state_ready)
        mtx_destroy(&b->state_mutex);
    if (b->queue_ready)
        mtx_destroy(&b->queue_mutex);
    backend_free(b, b->device_name);
    const mc_sound_backend_status status = b->state.last_error.status;
    free(b);
    return status;
}

static mc_sound_status configure_callback(void *context,
                                          mc_sound_manager_configuration configuration,
                                          mc_sound_error *error) {
    mc_sound_backend *b = context;
    if (configuration < MC_SOUND_MANAGER_LIBRARY_OPENAL ||
        configuration > MC_SOUND_MANAGER_CODEC_WAV)
        return callback_result(
            b, backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT, "Sound backend configuration", 0),
            error);
    mtx_lock(&b->state_mutex);
    b->configured[(size_t)configuration] = true;
    mtx_unlock(&b->state_mutex);
    return callback_result(b, MC_SOUND_BACKEND_OK, error);
}

static mc_sound_status construct_callback(void *context, void **out, mc_sound_error *error) {
    mc_sound_backend *b = context;
    if (out == NULL)
        return callback_result(b,
                               backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT,
                                            "Sound backend construction output", 0),
                               error);
    mtx_lock(&b->state_mutex);
    if (b->instance || b->constructing || b->state.worker_failed) {
        mtx_unlock(&b->state_mutex);
        return callback_result(
            b, backend_fail(b, MC_SOUND_BACKEND_BUSY, "Sound backend construction state", 0),
            error);
    }
    b->constructing = true;
    b->initialization_done = false;
    b->initialized = false;
    b->command_faulted = false;
    b->next_normal = b->next_streaming = 0;
    b->state.source_cleanup_reached = false;
    b->state.native_workers_joined = false;
    const mc_java_random_profile_status random =
        mc_java_random_construct_default(b->profile, &b->state.constructor_random);
    if (random != MC_JAVA_RANDOM_PROFILE_OK) {
        backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "SoundSystem default Random construction",
                     (int)random);
        b->constructing = false;
        mtx_unlock(&b->state_mutex);
        return callback_result(b, MC_SOUND_BACKEND_DEVICE_FAILED, error);
    }
    b->source_capacity = b->cache_capacity = 16u;
    b->source_threshold = b->cache_threshold = 12u;
    b->source_buckets = backend_alloc(b, b->source_capacity * sizeof(*b->source_buckets));
    b->cache_buckets = backend_alloc(b, b->cache_capacity * sizeof(*b->cache_buckets));
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    if (b->source_buckets == NULL || b->cache_buckets == NULL)
        status = b->state.last_error.status;
    atomic_store(&b->dying, false);
    if (status == MC_SOUND_BACKEND_OK && b->options.manual_service) {
        status = backend_device_initialize(b);
        b->initialization_done = true;
        b->initialized = status == MC_SOUND_BACKEND_OK;
    }
    mtx_unlock(&b->state_mutex);
    if (status == MC_SOUND_BACKEND_OK && !b->options.manual_service)
        status = backend_workers_start(b);
    mtx_lock(&b->state_mutex);
    b->constructing = false;
    if (status == MC_SOUND_BACKEND_OK) {
        b->instance = true;
        b->state.constructed = true;
        *out = b;
    }
    mtx_unlock(&b->state_mutex);
    if (status != MC_SOUND_BACKEND_OK) {
        const mc_sound_backend_error cause = b->state.last_error;
        const mc_sound_backend_status joined = backend_workers_stop(b);
        if (joined != MC_SOUND_BACKEND_OK)
            return callback_result(b, joined, error);
        mtx_lock(&b->state_mutex);
        if (backend_sources_cleanup(b) != MC_SOUND_BACKEND_DEVICE_FAILED)
            backend_device_cleanup(b);
        b->state.last_error = cause;
        mtx_unlock(&b->state_mutex);
    }
    return callback_result(b, status, error);
}

void backend_request_free(mc_sound_backend *b, backend_request *request) {
    if (request == NULL)
        return;
    backend_resource_free(b, &request->resource);
    backend_free(b, request->source_name);
    backend_free(b, request);
}

static backend_request *request_copy(mc_sound_backend *b, const mc_sound_manager_command *command,
                                     bool quick) {
    backend_request *request = backend_alloc(b, sizeof(*request));
    if (request == NULL)
        return NULL;
    request->command = *command;
    request->quick = quick;
    if (command->source_name != NULL) {
        for (size_t i = 0; command->source_name[i] != '\0'; ++i) {
            if (i >= 1024u || (uint8_t)command->source_name[i] >= 128u) {
                backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT, "SoundManager ASCII source name",
                             0);
                goto failure;
            }
        }
        request->source_name = backend_string(b, command->source_name);
        if (request->source_name == NULL)
            goto failure;
        request->command.source_name = request->source_name;
    }
    if (command->entry != NULL) {
        if (!backend_resource_copy(b, command->entry, &request->resource))
            goto failure;
        request->has_resource = true;
    }
    request->command.entry = NULL;
    return request;
failure:
    backend_request_free(b, request);
    return NULL;
}

static bool enqueue(mc_sound_backend *b, const mc_sound_manager_command *command, bool quick) {
    if (b->queue_count + b->deferred_count >= b->options.max_commands) {
        backend_fail(b, MC_SOUND_BACKEND_LIMIT, "Sound backend command budget", 0);
        return false;
    }
    backend_request *request = request_copy(b, command, quick);
    if (request == NULL)
        return false;
    if (b->queue_tail == NULL)
        b->queue_head = request;
    else
        b->queue_tail->next = request;
    b->queue_tail = request;
    ++b->queue_count;
    ++b->state.submitted;
    return true;
}

static mc_sound_status command_callback(void *context, void *handle,
                                        const mc_sound_manager_command *command,
                                        mc_sound_error *error) {
    mc_sound_backend *b = context;
    if (handle != b || command == NULL)
        return callback_result(
            b, backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT, "Sound backend command owner", 0),
            error);
    if (command->kind == MC_SOUND_MANAGER_CLEANUP)
        return callback_result(b, backend_source_cleanup(b), error);
    mtx_lock(&b->queue_mutex);
    mtx_lock(&b->state_mutex);
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    if (!b->instance || !b->state.constructed || b->state.source_cleanup_reached)
        status = backend_fail(b, MC_SOUND_BACKEND_CLOSED, "SoundSystem command after cleanup", 0);
    else if (!enqueue(b, command, command->kind == MC_SOUND_MANAGER_BACKGROUND_MUSIC))
        status = b->state.last_error.status;
    else if (command->kind == MC_SOUND_MANAGER_BACKGROUND_MUSIC) {
        const mc_sound_manager_command play = {.kind = MC_SOUND_MANAGER_PLAY,
                                               .source_name = command->source_name};
        if (!enqueue(b, &play, false))
            status = b->state.last_error.status;
    }
    mtx_unlock(&b->state_mutex);
    /* Source wrappers interrupt only after their complete enqueue prefix. */
    if (status == MC_SOUND_BACKEND_OK)
        cnd_signal(&b->command_condition);
    mtx_unlock(&b->queue_mutex);
    return callback_result(b, status, error);
}

static mc_sound_status playing_callback(void *context, void *handle, const char *name, bool *out,
                                        mc_sound_error *error) {
    mc_sound_backend *b = context;
    if (handle != b || out == NULL)
        return callback_result(
            b, backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT, "Sound backend playing owner", 0),
            error);
    mtx_lock(&b->queue_mutex);
    mtx_lock(&b->state_mutex);
    backend_source *source =
        b->instance && b->state.constructed && name != NULL ? backend_source_find(b, name) : NULL;
    *out = source != NULL && backend_source_playing(b, source);
    mtx_unlock(&b->state_mutex);
    mtx_unlock(&b->queue_mutex);
    return callback_result(b, MC_SOUND_BACKEND_OK, error);
}

static mc_sound_status retire_callback(void *context, void **handle, mc_sound_error *error) {
    mc_sound_backend *b = context;
    if (handle == NULL || (*handle != NULL && *handle != b))
        return callback_result(
            b,
            backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT, "Sound backend retirement owner", 0),
            error);
    if (*handle == NULL)
        return callback_result(b, MC_SOUND_BACKEND_OK, error);
    const mc_sound_backend_status status = backend_native_cleanup(b);
    if (status == MC_SOUND_BACKEND_OK) {
        mtx_lock(&b->state_mutex);
        b->instance = false;
        *handle = NULL;
        mtx_unlock(&b->state_mutex);
    }
    return callback_result(b, status, error);
}

bool mc_sound_backend_manager_interface(mc_sound_backend *b, mc_sound_manager_backend *out) {
    if (b == NULL || out == NULL)
        return false;
    *out = (mc_sound_manager_backend){b,
                                      configure_callback,
                                      construct_callback,
                                      command_callback,
                                      playing_callback,
                                      retire_callback};
    return true;
}

bool mc_sound_backend_read_state(mc_sound_backend *b, mc_sound_backend_state *out) {
    if (b == NULL || out == NULL)
        return false;
    mtx_lock(&b->queue_mutex);
    mtx_lock(&b->state_mutex);
    *out = b->state;
    out->queued_commands = b->queue_count;
    out->deferred_plays = b->deferred_count;
    out->current_sources = b->source_size;
    out->sources = b->allocated_sources;
    out->watched_sources = b->watch_count;
    out->decoded_buffers = b->cache_size;
    out->normal_channels = b->normal_count;
    out->streaming_channels = b->streaming_count;
    out->command_worker_alive = b->command_alive;
    out->stream_worker_alive = b->stream_alive;
    mtx_unlock(&b->state_mutex);
    mtx_unlock(&b->queue_mutex);
    return true;
}

mc_sound_backend_status mc_sound_backend_retire(mc_sound_backend **owner,
                                                mc_sound_backend_error *error) {
    if (owner == NULL)
        return MC_SOUND_BACKEND_INVALID_ARGUMENT;
    mc_sound_backend *b = *owner;
    if (b == NULL) {
        if (error != NULL)
            *error = (mc_sound_backend_error){0};
        return MC_SOUND_BACKEND_OK;
    }
    if (b->instance || b->constructing) {
        const mc_sound_backend_status status = backend_fail(
            b, MC_SOUND_BACKEND_BUSY, "Published SoundSystem still owns backend context", 0);
        if (error != NULL)
            *error = b->state.last_error;
        return status;
    }
    const mc_sound_backend_status joined = backend_workers_stop(b);
    if (joined != MC_SOUND_BACKEND_OK) {
        if (error != NULL)
            *error = b->state.last_error;
        return joined;
    }
    mtx_lock(&b->state_mutex);
    mc_sound_backend_status status = backend_sources_cleanup(b);
    if (status != MC_SOUND_BACKEND_DEVICE_FAILED) {
        const mc_sound_backend_status device = backend_device_cleanup(b);
        if (status == MC_SOUND_BACKEND_OK)
            status = device;
    }
    if (error != NULL)
        *error = status == MC_SOUND_BACKEND_OK ? (mc_sound_backend_error){0} : b->state.last_error;
    mtx_unlock(&b->state_mutex);
    if (status != MC_SOUND_BACKEND_OK)
        return status;
    cnd_destroy(&b->stream_condition);
    cnd_destroy(&b->command_condition);
    mtx_destroy(&b->stream_mutex);
    mtx_destroy(&b->state_mutex);
    mtx_destroy(&b->queue_mutex);
    backend_free(b, b->device_name);
    free(b);
    *owner = NULL;
    return MC_SOUND_BACKEND_OK;
}
