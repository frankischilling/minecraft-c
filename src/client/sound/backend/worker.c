#include "internal.h"

#include "../../../platform/time/millis.h"
#include "mc/core/java/java.h"

#include <string.h>

int64_t backend_now(void) {
    int64_t now = 0;
    (void)mc_platform_current_time_millis(&now);
    return now;
}

struct timespec backend_deadline(uint32_t millis) {
    struct timespec deadline;
    (void)timespec_get(&deadline, TIME_UTC);
    deadline.tv_sec += (time_t)(millis / 1000u);
    deadline.tv_nsec += (long)(millis % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }
    return deadline;
}

static void worker_failure(mc_sound_backend *b, mc_sound_backend_status status) {
    if (!b->state.worker_failed) {
        b->state.worker_failed = true;
        b->state.worker_error = b->state.last_error;
        b->state.worker_error.status = status;
    }
    atomic_store(&b->dying, true);
}

static void wake_workers(mc_sound_backend *b) {
    /* Acquire each wait predicate's own mutex after dropping the worker's
     * state lock. This closes a signal-before-wait race without reversing the
     * command queue -> state -> stream lock order. */
    mtx_lock(&b->queue_mutex);
    cnd_broadcast(&b->command_condition);
    mtx_unlock(&b->queue_mutex);
    mtx_lock(&b->stream_mutex);
    cnd_broadcast(&b->stream_condition);
    mtx_unlock(&b->stream_mutex);
}

static mc_sound_backend_status apply(mc_sound_backend *b, backend_request *request) {
    const mc_sound_manager_command *command = &request->command;
    backend_source *source;
    switch (command->kind) {
        case MC_SOUND_MANAGER_NEW_SOURCE:
        case MC_SOUND_MANAGER_NEW_STREAMING_SOURCE:
        case MC_SOUND_MANAGER_BACKGROUND_MUSIC:
            return backend_new_source(b, request);
        case MC_SOUND_MANAGER_SET_VOLUME:
            source = backend_source_find(b, command->source_name);
            if (source != NULL) {
                float value = command->value;
                if (value < 0.0f)
                    value = 0.0f;
                else if (value > 1.0f)
                    value = 1.0f;
                source->volume = value;
                backend_source_position_changed(b, source);
            }
            return MC_SOUND_BACKEND_OK;
        case MC_SOUND_MANAGER_SET_PITCH:
            source = backend_source_find(b, command->source_name);
            if (source != NULL) {
                float value = command->value;
                if (value < 0.5f)
                    value = 0.5f;
                else if (value > 2.0f)
                    value = 2.0f;
                backend_source_set_pitch(b, source, value);
                backend_source_position_changed(b, source);
            }
            return MC_SOUND_BACKEND_OK;
        case MC_SOUND_MANAGER_PLAY:
            source = backend_source_find(b, command->source_name);
            return source == NULL ? MC_SOUND_BACKEND_OK : backend_source_play(b, source);
        case MC_SOUND_MANAGER_STOP:
            source = backend_source_find(b, command->source_name);
            if (source != NULL)
                backend_source_stop(b, source);
            return MC_SOUND_BACKEND_OK;
        case MC_SOUND_MANAGER_LISTENER_POSITION:
            b->listener_data_position[0] = command->x;
            b->listener_data_position[1] = command->y;
            b->listener_data_position[2] = command->z;
            for (size_t i = 0; i < b->source_capacity; ++i)
                for (source = b->source_buckets[i]; source != NULL; source = source->hash_next)
                    backend_source_position_changed(b, source);
            memcpy(b->listener_position, b->listener_data_position, sizeof(b->listener_position));
            alListenerfv(AL_POSITION, b->listener_position);
            backend_al_error(b, "OpenAL listener position");
            return MC_SOUND_BACKEND_OK;
        case MC_SOUND_MANAGER_LISTENER_ORIENTATION:
            for (size_t i = 0; i < b->source_capacity; ++i)
                for (source = b->source_buckets[i]; source != NULL; source = source->hash_next)
                    backend_source_position_changed(b, source);
            memcpy(b->listener_orientation, command->orientation, sizeof(b->listener_orientation));
            alListenerfv(AL_ORIENTATION, b->listener_orientation);
            backend_al_error(b, "OpenAL listener orientation");
            return MC_SOUND_BACKEND_OK;
        default:
            return backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT, "Sound backend command kind",
                                0);
    }
}

mc_sound_backend_status backend_drain(mc_sound_backend *b) {
    while (b->queue_head != NULL) {
        backend_request *request = b->queue_head;
        b->queue_head = request->next;
        if (b->queue_head == NULL)
            b->queue_tail = NULL;
        request->next = NULL;
        --b->queue_count;
        if (request->command.kind == MC_SOUND_MANAGER_PLAY) {
            if (b->deferred_tail == NULL)
                b->deferred_head = request;
            else
                b->deferred_tail->next = request;
            b->deferred_tail = request;
            ++b->deferred_count;
            continue;
        }
        b->state.last_error = (mc_sound_backend_error){0};
        const mc_sound_backend_status status = apply(b, request);
        backend_request_free(b, request);
        ++b->state.applied;
        if (status != MC_SOUND_BACKEND_OK)
            return status;
    }
    while (b->deferred_head != NULL) {
        backend_request *request = b->deferred_head;
        b->deferred_head = request->next;
        if (b->deferred_head == NULL)
            b->deferred_tail = NULL;
        request->next = NULL;
        --b->deferred_count;
        b->state.last_error = (mc_sound_backend_error){0};
        const mc_sound_backend_status status = apply(b, request);
        backend_request_free(b, request);
        ++b->state.applied;
        if (status != MC_SOUND_BACKEND_OK)
            return status;
    }
    return MC_SOUND_BACKEND_OK;
}

int backend_command_worker(void *context) {
    mc_sound_backend *b = context;
    mtx_lock(&b->state_mutex);
    b->command_alive = true;
    b->maintenance_time = backend_now();
    mtx_unlock(&b->state_mutex);
    mtx_lock(&b->queue_mutex);
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    while (!atomic_load(&b->dying)) {
        while (!atomic_load(&b->dying) && b->queue_head == NULL && !b->initialize_requested) {
            const struct timespec deadline = backend_deadline(3600000u);
            const int code = cnd_timedwait(&b->command_condition, &b->queue_mutex, &deadline);
            if (code != thrd_success && code != thrd_timedout) {
                mtx_lock(&b->state_mutex);
                status = backend_fail(b, MC_SOUND_BACKEND_THREAD_FAILED,
                                      "Sound command worker wait", code);
                worker_failure(b, status);
                mtx_unlock(&b->state_mutex);
                break;
            }
        }
        if (atomic_load(&b->dying))
            break;
        mtx_lock(&b->state_mutex);
        if (b->initialize_requested) {
            b->initialize_requested = false;
            const int code = thrd_create(&b->stream_thread, backend_stream_worker, b);
            if (code != thrd_success)
                status = backend_fail(b, MC_SOUND_BACKEND_THREAD_FAILED,
                                      "Sound streaming worker start", code);
            else {
                b->stream_started = true;
                b->state.stream_worker_started = true;
                status = backend_device_initialize(b);
            }
            b->initialization_done = true;
            b->initialized = status == MC_SOUND_BACKEND_OK;
        }
        if (status == MC_SOUND_BACKEND_OK && b->initialized) {
            status = backend_drain(b);
            const int64_t now = backend_now();
            if (mc_java_i64_sub(now, b->maintenance_time) > 10000) {
                /* Every Source created by the reached manager route has
                 * temporary=false; its source cleanup scan makes no calls. */
                b->maintenance_time = now;
            }
        }
        if (status != MC_SOUND_BACKEND_OK) {
            b->command_faulted = true;
            worker_failure(b, status);
        }
        mtx_unlock(&b->state_mutex);
        if (status != MC_SOUND_BACKEND_OK)
            break;
    }
    mtx_unlock(&b->queue_mutex);
    wake_workers(b);
    mtx_lock(&b->state_mutex);
    b->command_alive = false;
    mtx_unlock(&b->state_mutex);
    return (int)status;
}

int backend_stream_worker(void *context) {
    mc_sound_backend *b = context;
    mtx_lock(&b->state_mutex);
    b->stream_alive = true;
    mtx_unlock(&b->state_mutex);
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    while (!atomic_load(&b->dying)) {
        mtx_lock(&b->stream_mutex);
        while (!atomic_load(&b->dying) && b->watch_count == 0) {
            const struct timespec deadline = backend_deadline(3600000u);
            const int code = cnd_timedwait(&b->stream_condition, &b->stream_mutex, &deadline);
            if (code != thrd_success && code != thrd_timedout) {
                status = MC_SOUND_BACKEND_THREAD_FAILED;
                break;
            }
        }
        const size_t generation = b->stream_wake_generation;
        mtx_unlock(&b->stream_mutex);
        if (atomic_load(&b->dying))
            break;
        mtx_lock(&b->state_mutex);
        if (status == MC_SOUND_BACKEND_OK && b->initialized) {
            b->state.last_error = (mc_sound_backend_error){0};
            status = backend_stream_pass(b);
        } else if (status != MC_SOUND_BACKEND_OK)
            backend_fail(b, status, "Sound streaming worker wait", thrd_error);
        if (status != MC_SOUND_BACKEND_OK)
            worker_failure(b, status);
        mtx_unlock(&b->state_mutex);
        if (status != MC_SOUND_BACKEND_OK)
            break;
        mtx_lock(&b->stream_mutex);
        if (!atomic_load(&b->dying) && b->watch_count != 0 &&
            generation == b->stream_wake_generation) {
            const struct timespec deadline = backend_deadline(20u);
            const int code = cnd_timedwait(&b->stream_condition, &b->stream_mutex, &deadline);
            if (code != thrd_success && code != thrd_timedout)
                status = MC_SOUND_BACKEND_THREAD_FAILED;
        }
        mtx_unlock(&b->stream_mutex);
    }
    wake_workers(b);
    mtx_lock(&b->state_mutex);
    b->stream_alive = false;
    mtx_unlock(&b->state_mutex);
    return (int)status;
}

mc_sound_backend_status backend_workers_start(mc_sound_backend *b) {
    const int code = thrd_create(&b->command_thread, backend_command_worker, b);
    if (code != thrd_success)
        return backend_fail(b, MC_SOUND_BACKEND_THREAD_FAILED, "Sound command worker start", code);
    mtx_lock(&b->state_mutex);
    b->command_started = true;
    b->state.command_worker_started = true;
    mtx_unlock(&b->state_mutex);
    const struct timespec initial_sleep = {0, 200000000L};
    thrd_sleep(&initial_sleep, NULL);
    mtx_lock(&b->queue_mutex);
    b->initialize_requested = true;
    cnd_signal(&b->command_condition);
    mtx_unlock(&b->queue_mutex);
    for (unsigned i = 0; i < 100u; ++i) {
        mtx_lock(&b->state_mutex);
        const bool ready = b->initialization_done;
        const mc_sound_backend_status status =
            b->state.worker_failed ? b->state.worker_error.status : MC_SOUND_BACKEND_OK;
        mtx_unlock(&b->state_mutex);
        if (ready)
            return status;
        const struct timespec delay = {0, 400000000L};
        thrd_sleep(&delay, NULL);
        cnd_signal(&b->command_condition);
    }
    return backend_fail(b, MC_SOUND_BACKEND_THREAD_FAILED,
                        "Sound backend initialization wait limit", thrd_timedout);
}

mc_sound_backend_status backend_workers_stop(mc_sound_backend *b) {
    atomic_store(&b->dying, true);
    wake_workers(b);
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    if (b->command_started) {
        int result = 0;
        const int code = thrd_join(b->command_thread, &result);
        if (code != thrd_success)
            status =
                backend_fail(b, MC_SOUND_BACKEND_THREAD_FAILED, "Sound command native join", code);
        else
            b->command_started = false;
    }
    if (b->stream_started) {
        int result = 0;
        const int code = thrd_join(b->stream_thread, &result);
        if (code != thrd_success)
            status = backend_fail(b, MC_SOUND_BACKEND_THREAD_FAILED, "Sound streaming native join",
                                  code);
        else
            b->stream_started = false;
    }
    mtx_lock(&b->state_mutex);
    b->state.native_workers_joined = !b->command_started && !b->stream_started;
    mtx_unlock(&b->state_mutex);
    return status;
}

mc_sound_backend_status backend_native_cleanup(mc_sound_backend *b) {
    mtx_lock(&b->state_mutex);
    b->state.constructed = false;
    mtx_unlock(&b->state_mutex);
    const mc_sound_backend_status joined = backend_workers_stop(b);
    if (joined != MC_SOUND_BACKEND_OK)
        return joined;
    mtx_lock(&b->queue_mutex);
    mtx_lock(&b->state_mutex);
    mc_sound_backend_status status = backend_sources_cleanup(b);
    if (status == MC_SOUND_BACKEND_DEVICE_FAILED) {
        mtx_unlock(&b->state_mutex);
        mtx_unlock(&b->queue_mutex);
        return status;
    }
    const mc_sound_backend_status device = backend_device_cleanup(b);
    if (status == MC_SOUND_BACKEND_OK)
        status = device;
    while (b->queue_head != NULL) {
        backend_request *request = b->queue_head;
        b->queue_head = request->next;
        backend_request_free(b, request);
    }
    while (b->deferred_head != NULL) {
        backend_request *request = b->deferred_head;
        b->deferred_head = request->next;
        backend_request_free(b, request);
    }
    b->queue_tail = b->deferred_tail = NULL;
    b->queue_count = b->deferred_count = 0;
    b->state.constructed = false;
    b->initialized = false;
    mtx_unlock(&b->state_mutex);
    mtx_unlock(&b->queue_mutex);
    return status;
}

mc_sound_backend_status backend_source_cleanup(mc_sound_backend *b) {
    mtx_lock(&b->state_mutex);
    b->state.source_cleanup_reached = true;
    mtx_unlock(&b->state_mutex);
    return backend_native_cleanup(b);
}

mc_sound_backend_status mc_sound_backend_service_commands(mc_sound_backend *b,
                                                          mc_sound_backend_error *error) {
    if (b == NULL)
        return MC_SOUND_BACKEND_INVALID_ARGUMENT;
    mtx_lock(&b->queue_mutex);
    mtx_lock(&b->state_mutex);
    mc_sound_backend_status status;
    if (!b->options.manual_service)
        status = backend_fail(b, MC_SOUND_BACKEND_BUSY, "Sound backend manual command mode", 0);
    else if (!b->state.constructed)
        status =
            backend_fail(b, MC_SOUND_BACKEND_CLOSED, "Sound backend manual command receiver", 0);
    else if (b->state.worker_failed) {
        b->state.last_error = b->state.worker_error;
        status = b->state.worker_error.status;
    } else {
        status = backend_drain(b);
        if (status != MC_SOUND_BACKEND_OK)
            worker_failure(b, status);
    }
    if (error != NULL)
        *error = status == MC_SOUND_BACKEND_OK ? (mc_sound_backend_error){0} : b->state.last_error;
    mtx_unlock(&b->state_mutex);
    mtx_unlock(&b->queue_mutex);
    return status;
}

mc_sound_backend_status mc_sound_backend_service_streams(mc_sound_backend *b,
                                                         mc_sound_backend_error *error) {
    if (b == NULL)
        return MC_SOUND_BACKEND_INVALID_ARGUMENT;
    mtx_lock(&b->state_mutex);
    mc_sound_backend_status status;
    if (!b->options.manual_service)
        status = backend_fail(b, MC_SOUND_BACKEND_BUSY, "Sound backend manual stream mode", 0);
    else if (!b->state.constructed)
        status =
            backend_fail(b, MC_SOUND_BACKEND_CLOSED, "Sound backend manual stream receiver", 0);
    else if (b->state.worker_failed) {
        b->state.last_error = b->state.worker_error;
        status = b->state.worker_error.status;
    } else {
        b->state.last_error = (mc_sound_backend_error){0};
        status = backend_stream_pass(b);
        if (status != MC_SOUND_BACKEND_OK)
            worker_failure(b, status);
    }
    if (error != NULL)
        *error = status == MC_SOUND_BACKEND_OK ? (mc_sound_backend_error){0} : b->state.last_error;
    mtx_unlock(&b->state_mutex);
    return status;
}

mc_sound_backend_status mc_sound_backend_read_source(mc_sound_backend *b, const char *name,
                                                     mc_sound_backend_source_state *out,
                                                     mc_sound_backend_error *error) {
    if (b == NULL || name == NULL || out == NULL)
        return MC_SOUND_BACKEND_INVALID_ARGUMENT;
    mtx_lock(&b->queue_mutex);
    mtx_lock(&b->state_mutex);
    *out = (mc_sound_backend_source_state){.channel_index = -1};
    backend_source *source = backend_source_find(b, name);
    if (source != NULL) {
        *out = (mc_sound_backend_source_state){
            .present = true,
            .streaming = source->streaming,
            .priority = source->priority,
            .looping = source->looping,
            .stopped = source->stopped,
            .paused = source->paused,
            .active = source->active,
            .preloading = source->preloading,
            .attached = source->channel != NULL && source->channel->attached == source,
            .playing = backend_source_playing(b, source),
            .x = source->x,
            .y = source->y,
            .z = source->z,
            .volume = source->volume,
            .pitch = source->pitch,
            .gain = source->gain,
            .distance = source->distance,
            .attenuation = source->attenuation,
            .channel_index = source->channel == NULL ? -1 : source->channel->index,
            .channel_handle = source->channel == NULL ? 0 : source->channel->handle};
    }
    if (error != NULL)
        *error = (mc_sound_backend_error){0};
    mtx_unlock(&b->state_mutex);
    mtx_unlock(&b->queue_mutex);
    return MC_SOUND_BACKEND_OK;
}

mc_sound_backend_status mc_sound_backend_render(mc_sound_backend *b, int16_t *output, size_t frames,
                                                mc_sound_backend_error *error) {
    if (b == NULL || output == NULL || frames > 1048576u)
        return MC_SOUND_BACKEND_INVALID_ARGUMENT;
    mtx_lock(&b->state_mutex);
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    if (!b->options.loopback || !b->options.manual_service || !b->state.constructed ||
        b->render_samples == NULL)
        status = backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT,
                              "Sound backend loopback render profile", 0);
    else {
        b->render_samples(b->device, output, (ALCsizei)frames);
        const ALCenum code = alcGetError(b->device);
        if (code != ALC_NO_ERROR)
            status =
                backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL loopback render", code);
    }
    if (error != NULL)
        *error = status == MC_SOUND_BACKEND_OK ? (mc_sound_backend_error){0} : b->state.last_error;
    mtx_unlock(&b->state_mutex);
    return status;
}

mc_sound_backend_status mc_sound_backend_wait_idle(mc_sound_backend *b, uint32_t timeout_millis,
                                                   mc_sound_backend_error *error) {
    if (b == NULL)
        return MC_SOUND_BACKEND_INVALID_ARGUMENT;
    const int64_t start = backend_now();
    for (;;) {
        mtx_lock(&b->queue_mutex);
        mtx_lock(&b->state_mutex);
        mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
        const bool idle = b->queue_count == 0 && b->deferred_count == 0;
        if (b->state.worker_failed) {
            b->state.last_error = b->state.worker_error;
            status = b->state.worker_error.status;
        } else if (!b->state.constructed)
            status = backend_fail(b, MC_SOUND_BACKEND_CLOSED, "Sound backend queue receiver", 0);
        else if (!idle && (b->options.manual_service ||
                           mc_java_i64_sub(backend_now(), start) >= (int64_t)timeout_millis))
            status = backend_fail(b, MC_SOUND_BACKEND_BUSY, "Sound backend queue wait deadline", 0);
        if (error != NULL)
            *error =
                status == MC_SOUND_BACKEND_OK ? (mc_sound_backend_error){0} : b->state.last_error;
        mtx_unlock(&b->state_mutex);
        mtx_unlock(&b->queue_mutex);
        if (idle || status != MC_SOUND_BACKEND_OK)
            return status;
        const struct timespec delay = {0, 1000000L};
        thrd_sleep(&delay, NULL);
    }
}
