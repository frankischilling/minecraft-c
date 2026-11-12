#include "internal.h"

#include <limits.h>
#include <string.h>

static once_flag module_once = ONCE_FLAG_INIT;
static mtx_t module_mutex;
static bool module_mutex_ready;
static mc_sound_backend *context_owner;

static void module_initialize(void) {
    module_mutex_ready = mtx_init(&module_mutex, mtx_plain) == thrd_success;
}

static void text_copy(char out[128], const ALchar *text) {
    size_t length = 0;
    if (text != NULL)
        while (length < 127u && text[length] != '\0') {
            out[length] = text[length];
            ++length;
        }
    out[length] = '\0';
}

ALenum backend_al_error(mc_sound_backend *b, const char *operation) {
    const ALenum code = alGetError();
    if (code != AL_NO_ERROR) {
        ++b->state.al_errors;
        b->state.last_error.al_code = code;
        b->state.last_error.operation = operation;
    }
    return code;
}

static bool channel_create(mc_sound_backend *b, backend_channel *channel, int32_t type,
                           int32_t index) {
    ALuint handle = 0;
    alGenSources(1, &handle);
    if (backend_al_error(b, "alGenSources") != AL_NO_ERROR)
        return false;
    *channel = (backend_channel){.handle = handle, .type = type, .index = index};
    return true;
}

mc_sound_backend_status backend_device_initialize(mc_sound_backend *b) {
    call_once(&module_once, module_initialize);
    if (!module_mutex_ready)
        return backend_fail(b, MC_SOUND_BACKEND_THREAD_FAILED, "OpenAL module mutex", thrd_error);
    mtx_lock(&module_mutex);
    if (context_owner != NULL && context_owner != b) {
        mtx_unlock(&module_mutex);
        return backend_fail(b, MC_SOUND_BACKEND_BUSY, "OpenAL process context already owned", 0);
    }
    context_owner = b;
    mtx_unlock(&module_mutex);
    if (b->options.loopback) {
        if (!alcIsExtensionPresent(NULL, "ALC_SOFT_loopback"))
            return backend_fail(b, MC_SOUND_BACKEND_UNSUPPORTED, "ALC_SOFT_loopback unavailable",
                                0);
        LPALCLOOPBACKOPENDEVICESOFT open_loopback = NULL;
        LPALCISRENDERFORMATSUPPORTEDSOFT format_supported = NULL;
        void *address = alcGetProcAddress(NULL, "alcLoopbackOpenDeviceSOFT");
        _Static_assert(sizeof(address) == sizeof(open_loopback), "OpenAL function address profile");
        memcpy(&open_loopback, &address, sizeof(address));
        address = alcGetProcAddress(NULL, "alcIsRenderFormatSupportedSOFT");
        memcpy(&format_supported, &address, sizeof(address));
        address = alcGetProcAddress(NULL, "alcRenderSamplesSOFT");
        memcpy(&b->render_samples, &address, sizeof(address));
        if (open_loopback == NULL || format_supported == NULL || b->render_samples == NULL)
            return backend_fail(b, MC_SOUND_BACKEND_UNSUPPORTED, "OpenAL loopback functions", 0);
        b->device = open_loopback(b->device_name);
        if (b->device == NULL)
            return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL loopback device open",
                                0);
        b->state.device_open = true;
        if (!format_supported(b->device, (ALCsizei)b->options.output_rate, ALC_STEREO_SOFT,
                              ALC_SHORT_SOFT))
            return backend_fail(b, MC_SOUND_BACKEND_UNSUPPORTED,
                                "OpenAL loopback stereo-s16 format", 0);
        const ALCint attributes[] = {ALC_FORMAT_CHANNELS_SOFT,
                                     ALC_STEREO_SOFT,
                                     ALC_FORMAT_TYPE_SOFT,
                                     ALC_SHORT_SOFT,
                                     ALC_FREQUENCY,
                                     (ALCint)b->options.output_rate,
                                     0};
        b->context = alcCreateContext(b->device, attributes);
    } else {
        b->device = alcOpenDevice(b->device_name);
        if (b->device == NULL)
            return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL default device open", 0);
        b->state.device_open = true;
        /* Bounded source-intended attributes. The old LWJGL attribute-buffer
         * position defect is not reproduced or asserted to have honored these. */
        const ALCint attributes[] = {
            ALC_FREQUENCY, (ALCint)b->options.output_rate, ALC_REFRESH, 60, ALC_SYNC, ALC_FALSE, 0};
        b->context = alcCreateContext(b->device, attributes);
    }
    const ALCenum context_error = alcGetError(b->device);
    if (b->context == NULL || context_error != ALC_NO_ERROR)
        return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL context creation",
                            context_error);
    if (!alcMakeContextCurrent(b->context))
        return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL ordinary context binding",
                            alcGetError(b->device));
    b->state.context_current = true;
    alcGetIntegerv(b->device, ALC_MAJOR_VERSION, 1, &b->state.alc_major);
    alcGetIntegerv(b->device, ALC_MINOR_VERSION, 1, &b->state.alc_minor);
    text_copy(b->state.vendor, alGetString(AL_VENDOR));
    text_copy(b->state.renderer, alGetString(AL_RENDERER));
    text_copy(b->state.version, alGetString(AL_VERSION));
    b->listener_orientation[2] = -1.0f;
    b->listener_orientation[4] = 1.0f;
    const ALfloat velocity[3] = {0.0f, 0.0f, 0.0f};
    alListenerfv(AL_POSITION, b->listener_position);
    ALenum error = backend_al_error(b, "OpenAL initial listener position");
    alListenerfv(AL_ORIENTATION, b->listener_orientation);
    const ALenum orientation_error = backend_al_error(b, "OpenAL initial listener orientation");
    alListenerfv(AL_VELOCITY, velocity);
    const ALenum velocity_error = backend_al_error(b, "OpenAL initial listener velocity");
    if (error != AL_NO_ERROR || orientation_error != AL_NO_ERROR || velocity_error != AL_NO_ERROR)
        return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL listener initialization",
                            error);
    b->normal = backend_alloc(b, (size_t)b->options.normal_channels * sizeof(*b->normal));
    b->streaming = backend_alloc(b, (size_t)b->options.streaming_channels * sizeof(*b->streaming));
    if (b->normal == NULL || b->streaming == NULL)
        return b->state.last_error.status;
    for (int32_t i = 0; i < b->options.streaming_channels; ++i) {
        if (!channel_create(b, &b->streaming[i], 1, i))
            break;
        ++b->streaming_count;
    }
    for (int32_t i = 0; i < b->options.normal_channels; ++i) {
        if (!channel_create(b, &b->normal[i], 0, i))
            break;
        ++b->normal_count;
    }
    if (b->normal_count < 2u)
        return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED,
                            "OpenAL source pitch-test channel index", 0);
    alSourcef(b->normal[1].handle, AL_PITCH, 1.0f);
    error = backend_al_error(b, "OpenAL pitch capability test");
    b->pitch_supported = error == AL_NO_ERROR;
    if (!b->pitch_supported)
        return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL pitch capability", error);
    return MC_SOUND_BACKEND_OK;
}

mc_sound_backend_status backend_device_cleanup(mc_sound_backend *b) {
    if (b->context != NULL) {
        /* Generated stream names, including names overwritten by the source's
         * generate-before-unqueue path, stay owned until native retirement. */
        while (b->device_buffer_count != 0) {
            const ALuint name = b->device_buffer_names[b->device_buffer_count - 1u];
            if (!backend_delete_buffer(b, name, "OpenAL native buffer retirement")) {
                const ALenum observed = b->state.last_error.al_code;
                const mc_sound_backend_status status = backend_fail(
                    b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL native buffer retirement", 0);
                b->state.last_error.al_code = observed;
                return status;
            }
        }
        if (!alcMakeContextCurrent(NULL))
            return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL native context unbind",
                                alcGetError(b->device));
        b->state.context_current = false;
        alcDestroyContext(b->context);
        b->context = NULL;
    }
    if (b->device != NULL) {
        if (!alcCloseDevice(b->device))
            return backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL native device close",
                                alcGetError(b->device));
        b->device = NULL;
        b->state.device_open = false;
    }
    b->render_samples = NULL;
    backend_free(b, b->device_buffer_names);
    b->device_buffer_names = NULL;
    b->device_buffer_capacity = 0;
    b->state.device_buffers = 0;
    if (module_mutex_ready) {
        mtx_lock(&module_mutex);
        if (context_owner == b)
            context_owner = NULL;
        mtx_unlock(&module_mutex);
    }
    return MC_SOUND_BACKEND_OK;
}

bool backend_channel_playing(mc_sound_backend *b, const backend_channel *channel) {
    ALint state = 0;
    alGetSourcei(channel->handle, AL_SOURCE_STATE, &state);
    return backend_al_error(b, "OpenAL playing query") == AL_NO_ERROR && state == AL_PLAYING;
}

void backend_channel_stop(mc_sound_backend *b, backend_channel *channel) {
    alSourceStop(channel->handle);
    backend_al_error(b, "OpenAL source stop");
    ++b->state.stops;
}

void backend_channel_close(mc_sound_backend *b, backend_channel *channel) {
    alSourceStop(channel->handle);
    (void)alGetError(); /* source close deliberately discards this first error */
    if (channel->type != 1)
        return;
    if (backend_al_error(b, "OpenAL flush entry") != AL_NO_ERROR)
        return;
    ALint queued = 0;
    alGetSourcei(channel->handle, AL_BUFFERS_QUEUED, &queued);
    for (ALint i = 0; i < queued; ++i) {
        ALuint buffer = 0;
        alSourceUnqueueBuffers(channel->handle, 1, &buffer);
        if (backend_al_error(b, "OpenAL channel flush unqueue") != AL_NO_ERROR)
            return;
    }
}

static bool generated(mc_sound_backend *b, ALsizei count, ALuint *buffers) {
    if (count < 0 || (size_t)count > b->options.max_device_buffers - b->state.device_buffers) {
        backend_fail(b, MC_SOUND_BACKEND_LIMIT, "OpenAL device buffer budget", 0);
        return false;
    }
    const size_t wanted = b->device_buffer_count + (size_t)count;
    if (wanted > b->device_buffer_capacity) {
        size_t capacity = b->device_buffer_capacity == 0 ? 16u : b->device_buffer_capacity;
        while (capacity < wanted)
            capacity *= 2u;
        if (capacity > b->options.max_device_buffers)
            capacity = b->options.max_device_buffers;
        ALuint *names = backend_alloc(b, capacity * sizeof(*names));
        if (names == NULL)
            return false;
        if (b->device_buffer_count != 0)
            memcpy(names, b->device_buffer_names, b->device_buffer_count * sizeof(*names));
        backend_free(b, b->device_buffer_names);
        b->device_buffer_names = names;
        b->device_buffer_capacity = capacity;
    }
    alGenBuffers(count, buffers);
    if (backend_al_error(b, "OpenAL buffer generation") != AL_NO_ERROR)
        return false;
    if (count != 0)
        memcpy(b->device_buffer_names + b->device_buffer_count, buffers,
               (size_t)count * sizeof(*buffers));
    b->device_buffer_count += (size_t)count;
    b->state.device_buffers += (size_t)count;
    b->state.device_buffers_created += (size_t)count;
    return true;
}

bool backend_delete_buffer(mc_sound_backend *b, ALuint buffer, const char *operation) {
    alDeleteBuffers(1, &buffer);
    if (backend_al_error(b, operation) != AL_NO_ERROR)
        return false;
    for (size_t i = 0; i < b->device_buffer_count; ++i)
        if (b->device_buffer_names[i] == buffer) {
            --b->device_buffer_count;
            b->device_buffer_names[i] = b->device_buffer_names[b->device_buffer_count];
            --b->state.device_buffers;
            ++b->state.device_buffers_deleted;
            break;
        }
    return true;
}

bool backend_channel_preload(mc_sound_backend *b, backend_channel *channel, backend_pcm *pcm,
                             size_t count) {
    if (channel->type != 1 || count > 64u)
        return false;
    const bool was_playing = backend_channel_playing(b, channel);
    if (was_playing)
        backend_channel_stop(b, channel);
    ALint processed = 0;
    alGetSourcei(channel->handle, AL_BUFFERS_PROCESSED, &processed);
    if (processed > 0) {
        if (processed > 64) {
            backend_fail(b, MC_SOUND_BACKEND_LIMIT, "OpenAL processed buffer bound", processed);
            return false;
        }
        ALuint old[64] = {0};
        /* The source generates before unqueue; the unqueue overwrites these
         * names. Context destruction still releases the native allocations. */
        if (!generated(b, processed, old))
            return false;
        alSourceUnqueueBuffers(channel->handle, processed, old);
        if (backend_al_error(b, "OpenAL preload old-buffer unqueue") != AL_NO_ERROR)
            return false;
    }
    if (was_playing) {
        alSourcePlay(channel->handle);
        backend_al_error(b, "OpenAL preload restore play");
    }
    ALuint buffers[64] = {0};
    if (!generated(b, (ALsizei)count, buffers))
        return false;
    if (channel->staging == NULL) {
        channel->staging = backend_alloc(b, 0x500000u);
        if (channel->staging == NULL)
            return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (pcm[i].bytes > 0x500000u) {
            backend_fail(b, MC_SOUND_BACKEND_LIMIT, "OpenAL streaming staging budget", 0);
            return false;
        }
        if (pcm[i].bytes != 0)
            memcpy(channel->staging, pcm[i].data, pcm[i].bytes);
        alBufferData(buffers[i], channel->format, channel->staging, (ALsizei)pcm[i].bytes,
                     channel->rate);
        if (backend_al_error(b, "OpenAL preload PCM upload") != AL_NO_ERROR)
            return false;
        b->state.pcm_bytes_queued += pcm[i].bytes;
    }
    alSourceQueueBuffers(channel->handle, (ALsizei)count, buffers);
    if (backend_al_error(b, "OpenAL preload PCM queue") != AL_NO_ERROR)
        return false;
    alSourcePlay(channel->handle);
    return backend_al_error(b, "OpenAL preload play") == AL_NO_ERROR;
}

bool backend_channel_queue(mc_sound_backend *b, backend_channel *channel, const backend_pcm *pcm) {
    if (channel->type != 1)
        return false;
    if (pcm->bytes > 0x500000u) {
        backend_fail(b, MC_SOUND_BACKEND_LIMIT, "OpenAL streaming staging budget", 0);
        return false;
    }
    if (channel->staging == NULL) {
        channel->staging = backend_alloc(b, 0x500000u);
        if (channel->staging == NULL)
            return false;
    }
    if (pcm->bytes != 0)
        memcpy(channel->staging, pcm->data, pcm->bytes);
    ALuint buffer = 0;
    alSourceUnqueueBuffers(channel->handle, 1, &buffer);
    if (backend_al_error(b, "OpenAL stream unqueue") != AL_NO_ERROR)
        return false;
    alBufferData(buffer, channel->format, channel->staging, (ALsizei)pcm->bytes, channel->rate);
    if (backend_al_error(b, "OpenAL stream PCM upload") != AL_NO_ERROR)
        return false;
    alSourceQueueBuffers(channel->handle, 1, &buffer);
    if (backend_al_error(b, "OpenAL stream PCM requeue") != AL_NO_ERROR)
        return false;
    b->state.pcm_bytes_queued += pcm->bytes;
    return true;
}

int backend_channel_processed(mc_sound_backend *b, const backend_channel *channel) {
    if (channel->type != 1)
        return 0;
    ALint count = 0;
    alGetSourcei(channel->handle, AL_BUFFERS_PROCESSED, &count);
    return backend_al_error(b, "OpenAL processed query") == AL_NO_ERROR ? count : 0;
}

bool backend_generate_buffer(mc_sound_backend *b, ALuint *buffer) {
    return generated(b, 1, buffer);
}
