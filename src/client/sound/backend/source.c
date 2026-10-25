#include "internal.h"

#include <math.h>
#include <string.h>

static bool text_equal(mc_sound_text left, mc_sound_text right) {
    return left.is_null == right.is_null && left.length == right.length &&
           (left.length == 0 ||
            memcmp(left.units, right.units, left.length * sizeof(uint16_t)) == 0);
}

static void source_retain(backend_source *source) {
    if (source != NULL)
        ++source->references;
}

static void source_release(mc_sound_backend *b, backend_source *source) {
    if (source == NULL || --source->references != 0)
        return;
    backend_source **at = &b->sources;
    while (*at != NULL && *at != source)
        at = &(*at)->next;
    if (*at == source)
        *at = source->next;
    for (size_t i = 0; i < source->retained_pcm_count; ++i) {
        b->state.retained_pcm_bytes -= source->retained_pcm[i].bytes;
        backend_pcm_free(&source->retained_pcm[i]);
    }
    backend_codec_free(b, &source->codec);
    backend_resource_free(b, &source->resource);
    backend_free(b, source->name);
    backend_free(b, source);
    --b->allocated_sources;
}

backend_source *backend_source_find(mc_sound_backend *b, const char *name) {
    if (name == NULL || b->source_buckets == NULL)
        return NULL;
    const uint32_t hash = backend_name_hash(name);
    for (backend_source *source = b->source_buckets[hash & (b->source_capacity - 1u)];
         source != NULL; source = source->hash_next)
        if (source->hash == hash && strcmp(source->name, name) == 0)
            return source;
    return NULL;
}

/* JDK17 HashMap's spread hash, bucket append and low/high resize partition.
 * SoundManager's fixed ASCII source-name family never reaches tree buckets. */
static mc_sound_backend_status source_resize(mc_sound_backend *b) {
    const size_t old = b->source_capacity;
    backend_source **table = backend_alloc(b, old * 2u * sizeof(*table));
    if (table == NULL)
        return b->state.last_error.status;
    for (size_t i = 0; i < old; ++i) {
        backend_source *low = NULL, *low_tail = NULL, *high = NULL, *high_tail = NULL;
        for (backend_source *node = b->source_buckets[i]; node != NULL;) {
            backend_source *next = node->hash_next;
            node->hash_next = NULL;
            if ((node->hash & old) == 0) {
                if (low_tail == NULL)
                    low = node;
                else
                    low_tail->hash_next = node;
                low_tail = node;
            } else {
                if (high_tail == NULL)
                    high = node;
                else
                    high_tail->hash_next = node;
                high_tail = node;
            }
            node = next;
        }
        table[i] = low;
        table[i + old] = high;
    }
    backend_free(b, b->source_buckets);
    b->source_buckets = table;
    b->source_capacity *= 2u;
    b->source_threshold *= 2u;
    return MC_SOUND_BACKEND_OK;
}

static mc_sound_backend_status source_publish(mc_sound_backend *b, backend_source *source) {
    const size_t bucket = source->hash & (b->source_capacity - 1u);
    backend_source **at = &b->source_buckets[bucket];
    while (*at != NULL && strcmp((*at)->name, source->name) != 0)
        at = &(*at)->hash_next;
    backend_source *old = *at;
    source_retain(source);
    source->current = true;
    if (old != NULL) {
        source->hash_next = old->hash_next;
        *at = source;
        old->current = false;
        old->hash_next = NULL;
        source_release(b, old);
        return MC_SOUND_BACKEND_OK;
    }
    *at = source;
    ++b->source_size;
    return b->source_size > b->source_threshold ? source_resize(b) : MC_SOUND_BACKEND_OK;
}

static backend_cache *cache_find(mc_sound_backend *b, mc_sound_text key) {
    const uint32_t hash = backend_text_hash(key);
    for (backend_cache *cache = b->cache_buckets[hash & (b->cache_capacity - 1u)]; cache != NULL;
         cache = cache->next)
        if (cache->hash == hash && text_equal(cache->resource.logical, key))
            return cache;
    return NULL;
}

static mc_sound_backend_status cache_resize(mc_sound_backend *b) {
    const size_t old = b->cache_capacity;
    backend_cache **table = backend_alloc(b, old * 2u * sizeof(*table));
    if (table == NULL)
        return b->state.last_error.status;
    for (size_t i = 0; i < old; ++i) {
        backend_cache *low = NULL, *low_tail = NULL, *high = NULL, *high_tail = NULL;
        for (backend_cache *node = b->cache_buckets[i]; node != NULL;) {
            backend_cache *next = node->next;
            node->next = NULL;
            if ((node->hash & old) == 0) {
                if (low_tail == NULL)
                    low = node;
                else
                    low_tail->next = node;
                low_tail = node;
            } else {
                if (high_tail == NULL)
                    high = node;
                else
                    high_tail->next = node;
                high_tail = node;
            }
            node = next;
        }
        table[i] = low;
        table[i + old] = high;
    }
    backend_free(b, b->cache_buckets);
    b->cache_buckets = table;
    b->cache_capacity *= 2u;
    b->cache_threshold *= 2u;
    return MC_SOUND_BACKEND_OK;
}

static ALenum audio_format(int32_t channels, int32_t bits) {
    if (channels == 1 && bits == 8)
        return AL_FORMAT_MONO8;
    if (channels == 1 && bits == 16)
        return AL_FORMAT_MONO16;
    if (channels == 2 && bits == 8)
        return AL_FORMAT_STEREO8;
    if (channels == 2 && bits == 16)
        return AL_FORMAT_STEREO16;
    return 0;
}

static mc_sound_backend_status cache_device_load(mc_sound_backend *b, backend_cache *cache) {
    if (cache->device_present)
        return MC_SOUND_BACKEND_OK;
    const backend_pcm *pcm = &cache->decoded;
    const ALenum format = audio_format(pcm->channels, pcm->bits);
    if (format == 0) {
        ++b->state.source_rejections;
        return MC_SOUND_BACKEND_OK;
    }
    if (!backend_generate_buffer(b, &cache->buffer)) {
        if (b->state.last_error.status == MC_SOUND_BACKEND_LIMIT ||
            b->state.last_error.status == MC_SOUND_BACKEND_ALLOCATION_FAILED)
            return b->state.last_error.status;
        ++b->state.source_rejections;
        return MC_SOUND_BACKEND_OK;
    }
    if (pcm->data_is_null)
        return backend_fail(b, MC_SOUND_BACKEND_SOURCE_EXCEPTION,
                            "Original sound buffer has null audioData", 0);
    alBufferData(cache->buffer, format, pcm->data, (ALsizei)pcm->bytes, pcm->rate);
    (void)backend_al_error(b, "OpenAL normal PCM upload");
    /* The original error branch also tests a nonnull IntBuffer, so an upload
     * error alone still reaches publication of this actual generated name. */
    cache->device_present = true;
    return MC_SOUND_BACKEND_OK;
}

static mc_sound_backend_status cache_load(mc_sound_backend *b, const backend_resource *resource,
                                          backend_cache **out) {
    backend_cache *cache = cache_find(b, resource->logical);
    if (cache != NULL) {
        *out = cache;
        /* The source returns from its decoded-map hit before consulting the
         * AL map, including a previously rejected format/allocation prefix. */
        return MC_SOUND_BACKEND_OK;
    }
    if (b->cache_size >= b->options.max_cached_buffers)
        return backend_fail(b, MC_SOUND_BACKEND_LIMIT, "Sound backend decoded-cache count", 0);
    backend_codec codec;
    mc_sound_backend_status status = backend_codec_create(b, resource, &codec);
    if (status != MC_SOUND_BACKEND_OK)
        return status;
    if (codec.kind == BACKEND_CODEC_NONE) {
        ++b->state.source_rejections;
        *out = NULL;
        return MC_SOUND_BACKEND_OK;
    }
    status = backend_codec_initialize(b, &codec, resource);
    backend_pcm pcm = {0};
    if (status == MC_SOUND_BACKEND_OK)
        status = backend_codec_read(b, &codec, true, &pcm);
    const mc_sound_backend_error cause = b->state.last_error;
    const mc_sound_backend_status retired = backend_codec_free(b, &codec);
    if (status == MC_SOUND_BACKEND_OK)
        status = retired;
    else
        b->state.last_error = cause;
    if (status != MC_SOUND_BACKEND_OK) {
        backend_pcm_free(&pcm);
        return status;
    }
    if (!pcm.present) {
        ++b->state.source_rejections;
        *out = NULL;
        return MC_SOUND_BACKEND_OK;
    }
    if (pcm.bytes > b->options.allocation_bytes - b->state.allocation_bytes) {
        backend_pcm_free(&pcm);
        return backend_fail(b, MC_SOUND_BACKEND_LIMIT, "Sound backend cached PCM bytes", 0);
    }
    cache = backend_alloc(b, sizeof(*cache));
    if (cache == NULL) {
        backend_pcm_free(&pcm);
        return b->state.last_error.status;
    }
    if (!backend_resource_clone(b, resource, &cache->resource)) {
        backend_free(b, cache);
        backend_pcm_free(&pcm);
        return b->state.last_error.status;
    }
    if (pcm.bytes > b->options.allocation_bytes - b->state.allocation_bytes) {
        backend_resource_free(b, &cache->resource);
        backend_free(b, cache);
        backend_pcm_free(&pcm);
        return backend_fail(b, MC_SOUND_BACKEND_LIMIT, "Sound backend cached PCM ownership", 0);
    }
    cache->hash = backend_text_hash(resource->logical);
    cache->decoded = pcm;
    b->state.allocation_bytes += pcm.bytes;
    backend_cache **at = &b->cache_buckets[cache->hash & (b->cache_capacity - 1u)];
    while (*at != NULL)
        at = &(*at)->next;
    *at = cache;
    ++b->cache_size;
    if (b->cache_size > b->cache_threshold && cache_resize(b) != MC_SOUND_BACKEND_OK)
        return b->state.last_error.status;
    *out = cache; /* decoded entry is published before format/device admission */
    return cache_device_load(b, cache);
}

bool backend_source_playing(mc_sound_backend *b, backend_source *source) {
    return source->channel != NULL && source->channel->attached == source && !source->paused &&
           !source->stopped && backend_channel_playing(b, source->channel);
}

static bool attached(const backend_source *source) {
    return source->channel != NULL && source->channel->attached == source;
}

static void check_pitch(mc_sound_backend *b, backend_source *source) {
    if (attached(source) && b->pitch_supported) {
        alSourcef(source->channel->handle, AL_PITCH, source->pitch);
        backend_al_error(b, "OpenAL source pitch");
    }
}

void backend_source_position_changed(mc_sound_backend *b, backend_source *source) {
    const float dx = source->x - b->listener_position[0];
    const float dy = source->y - b->listener_position[1];
    const float dz = source->z - b->listener_position[2];
    const double x = dx, y = dy, z = dz;
    source->distance = (float)sqrt(x * x + y * y + z * z);
    if (source->attenuation == 2) {
        source->gain = source->distance <= 0.0f ? 1.0f
                       : source->distance >= source->dist_or_roll
                           ? 0.0f
                           : 1.0f - source->distance / source->dist_or_roll;
        if (source->gain > 1.0f)
            source->gain = 1.0f;
        if (source->gain < 0.0f)
            source->gain = 0.0f;
    } else
        source->gain = 1.0f;
    if (attached(source)) {
        alSourcef(source->channel->handle, AL_GAIN, source->gain * source->volume * 1.0f * 1.0f);
        backend_al_error(b, "OpenAL source gain");
    }
    check_pitch(b, source);
}

void backend_source_set_pitch(mc_sound_backend *b, backend_source *source, float value) {
    if (value < 0.5f)
        value = 0.5f;
    else if (value > 2.0f)
        value = 2.0f;
    source->pitch = value;
    check_pitch(b, source);
}

void backend_source_stop(mc_sound_backend *b, backend_source *source) {
    source->stopped = true;
    source->paused = false;
    if (source->channel != NULL)
        backend_channel_stop(b, source->channel);
}

static backend_channel *next_channel(mc_sound_backend *b, backend_source *source) {
    backend_channel *channels = source->streaming ? b->streaming : b->normal;
    const size_t count = source->streaming ? b->streaming_count : b->normal_count;
    size_t *cursor = source->streaming ? &b->next_streaming : &b->next_normal;
    for (size_t i = 0; i < count; ++i)
        if (channels[i].remembered_name != NULL &&
            strcmp(channels[i].remembered_name, source->name) == 0)
            return &channels[i];
    for (unsigned pass = 0; pass < 2u; ++pass) {
        size_t slot = *cursor;
        for (size_t i = 0; i < count; ++i) {
            backend_source *old = backend_source_find(b, channels[slot].remembered_name);
            if (old == NULL || !backend_source_playing(b, old) || (pass == 1u && !old->priority)) {
                char *name = backend_string(b, source->name);
                if (name == NULL)
                    return NULL;
                backend_free(b, channels[slot].remembered_name);
                channels[slot].remembered_name = name;
                *cursor = slot + 1u == count ? 0 : slot + 1u;
                return &channels[slot];
            }
            slot = slot + 1u == count ? 0 : slot + 1u;
        }
    }
    return NULL;
}

static void attach_source(mc_sound_backend *b, backend_channel *channel, backend_source *source) {
    backend_source *old = channel->attached;
    source_retain(source);
    channel->attached = source;
    source_release(b, old);
}

void backend_source_watch(mc_sound_backend *b, backend_source *source) {
    mtx_lock(&b->stream_mutex);
    if (source->watched) {
        mtx_unlock(&b->stream_mutex);
        return;
    }
    backend_source **at = &b->watch_head;
    b->watch_tail = NULL;
    while (*at != NULL) {
        backend_source *old = *at;
        if (old->channel == source->channel) {
            backend_source_stop(b, old);
            *at = old->watch_next;
            old->watch_next = NULL;
            old->watched = false;
            --b->watch_count;
            source_release(b, old);
        } else {
            b->watch_tail = old;
            at = &old->watch_next;
        }
    }
    source_retain(source);
    source->watched = true;
    if (b->watch_tail == NULL)
        b->watch_head = source;
    else
        b->watch_tail->watch_next = source;
    b->watch_tail = source;
    ++b->watch_count;
    ++b->stream_wake_generation;
    cnd_signal(&b->stream_condition);
    mtx_unlock(&b->stream_mutex);
}

mc_sound_backend_status backend_source_play(mc_sound_backend *b, backend_source *source) {
    ++b->state.play_attempts;
    if (!source->active || backend_source_playing(b, source))
        return MC_SOUND_BACKEND_OK;
    backend_channel *channel = next_channel(b, source);
    if (channel == NULL)
        return b->state.last_error.status == MC_SOUND_BACKEND_LIMIT ||
                       b->state.last_error.status == MC_SOUND_BACKEND_ALLOCATION_FAILED
                   ? b->state.last_error.status
                   : MC_SOUND_BACKEND_OK;
    if (source->channel != NULL && source->channel->attached != source)
        source->channel = NULL;
    attach_source(b, channel, source);
    const bool changed = source->channel != channel;
    const bool paused = source->paused;
    if (changed) {
        source->channel = channel;
        backend_channel_close(b, channel);
    }
    source->stopped = false;
    source->paused = false;
    if (changed) {
        backend_source_position_changed(b, source);
        alSource3f(channel->handle, AL_POSITION, source->x, source->y, source->z);
        backend_al_error(b, "OpenAL source setPosition");
        check_pitch(b, source);
        if (b->pitch_supported) {
            alSourcef(channel->handle, AL_PITCH, source->pitch);
            backend_al_error(b, "OpenAL source initial pitch");
        }
        alSource3f(channel->handle, AL_POSITION, source->x, source->y, source->z);
        backend_al_error(b, "OpenAL source initial position");
        alSource3f(channel->handle, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        backend_al_error(b, "OpenAL source initial velocity");
        alSourcef(channel->handle, AL_ROLLOFF_FACTOR,
                  source->attenuation == 1 ? source->dist_or_roll : 0.0f);
        backend_al_error(b, "OpenAL source rolloff");
        alSourcei(channel->handle, AL_LOOPING,
                  source->looping && !source->streaming ? AL_TRUE : AL_FALSE);
        backend_al_error(b, "OpenAL source loop mode");
        if (!source->streaming) {
            if (source->cache == NULL || !source->cache->device_present) {
                ++b->state.source_rejections;
                return MC_SOUND_BACKEND_OK;
            }
            alSourcei(channel->handle, AL_BUFFER, (ALint)source->cache->buffer);
            backend_al_error(b, "OpenAL normal source buffer attachment");
        }
    }
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    if (!backend_source_playing(b, source)) {
        if (source->streaming && !paused) {
            if (source->codec.kind == BACKEND_CODEC_NONE) {
                ++b->state.source_rejections;
                goto watch;
            }
            if (!source->codec.format_present) {
                status = backend_codec_initialize(b, &source->codec, &source->resource);
                if (status != MC_SOUND_BACKEND_OK)
                    goto watch;
            }
            const ALenum format = audio_format(source->codec.channels, source->codec.bits);
            if (!source->codec.format_present || format == 0) {
                ++b->state.source_rejections;
                goto watch;
            }
            channel->format = format;
            channel->rate = source->codec.rate;
            source->preloading = true;
        }
        alSourcePlay(channel->handle);
        backend_al_error(b, "OpenAL source play");
        if (source->pitch != 1.0f)
            check_pitch(b, source);
    }
watch:
    if (source->streaming)
        backend_source_watch(b, source);
    return status;
}

mc_sound_backend_status backend_new_source(mc_sound_backend *b, const backend_request *request) {
    if (!request->has_resource || request->source_name == NULL)
        return backend_fail(b, MC_SOUND_BACKEND_INVALID_ARGUMENT, "Sound backend newSource fields",
                            0);
    const bool streaming =
        request->command.kind == MC_SOUND_MANAGER_NEW_STREAMING_SOURCE || request->quick;
    backend_cache *cache = NULL;
    if (!streaming) {
        const mc_sound_backend_status loaded = cache_load(b, &request->resource, &cache);
        if (loaded != MC_SOUND_BACKEND_OK)
            return loaded;
        if (cache == NULL || !cache->device_present) {
            ++b->state.source_rejections;
            return MC_SOUND_BACKEND_OK;
        }
    }
    if (b->allocated_sources >= b->options.max_sources)
        return backend_fail(b, MC_SOUND_BACKEND_LIMIT, "Sound backend source count", 0);
    backend_source *source = backend_alloc(b, sizeof(*source));
    if (source == NULL)
        return b->state.last_error.status;
    source->references = 1;
    source->next = b->sources;
    b->sources = source;
    ++b->allocated_sources;
    source->name = backend_string(b, request->source_name);
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    if (source->name == NULL || !backend_resource_clone(b, &request->resource, &source->resource)) {
        status = b->state.last_error.status;
        goto done;
    }
    source->hash = backend_name_hash(source->name);
    source->cache = cache;
    source->streaming = streaming;
    source->priority = request->quick || request->command.priority;
    source->looping = request->command.looping;
    source->active = true;
    source->stopped = true;
    source->x = request->command.x;
    source->y = request->command.y;
    source->z = request->command.z;
    source->attenuation = request->command.attenuation;
    source->dist_or_roll = request->command.distance;
    source->volume = source->pitch = source->gain = 1.0f;
    if (streaming) {
        status = backend_codec_create(b, &source->resource, &source->codec);
        if (status != MC_SOUND_BACKEND_OK)
            goto done;
    }
    backend_source_position_changed(b, source);
    status = source_publish(b, source);
    if (status == MC_SOUND_BACKEND_OK && request->quick) {
        ++b->state.quick_play_attempts;
        status = backend_source_play(b, source);
    }
done:
    source_release(b, source);
    return status;
}

static void retain_failed_pcm(mc_sound_backend *b, backend_source *source, backend_pcm *pcm) {
    if (!pcm->present && pcm->data == NULL)
        return;
    /* At most one preload batch or one refill result is retained. A terminal
     * native transfer failure prevents a later read from replacing this PCM. */
    source->retained_pcm[source->retained_pcm_count++] = *pcm;
    b->state.retained_pcm_bytes += pcm->bytes;
    *pcm = (backend_pcm){0};
}

static mc_sound_backend_status transfer_failure(mc_sound_backend *b, const char *operation) {
    const mc_sound_backend_error observed = b->state.last_error;
    if (observed.status != MC_SOUND_BACKEND_OK)
        return observed.status;
    const mc_sound_backend_status status =
        backend_fail(b, MC_SOUND_BACKEND_DEVICE_FAILED,
                     observed.operation == NULL ? operation : observed.operation, 0);
    b->state.last_error.al_code = observed.al_code;
    return status;
}

static mc_sound_backend_status preload(mc_sound_backend *b, backend_source *source) {
    if (source->codec.kind == BACKEND_CODEC_NONE)
        return MC_SOUND_BACKEND_OK;
    mc_sound_backend_status status = backend_codec_initialize(b, &source->codec, &source->resource);
    if (status != MC_SOUND_BACKEND_OK)
        return status;
    backend_pcm buffers[64] = {0};
    size_t count = 0;
    for (int32_t i = 0; i < b->options.streaming_buffers; ++i) {
        ++b->state.stream_reads;
        status = backend_codec_read(b, &source->codec, false, &buffers[count]);
        if (status != MC_SOUND_BACKEND_OK || !buffers[count].present || buffers[count].data_is_null)
            break;
        ++count;
    }
    if (status == MC_SOUND_BACKEND_OK) {
        backend_source_position_changed(b, source);
        if (!backend_channel_preload(b, source->channel, buffers, count)) {
            ++b->state.preload_failures;
            status = transfer_failure(b, "OpenAL preload transfer rejected");
        }
        source->preloading = false;
    }
    for (size_t i = 0; i < 64u; ++i) {
        if (status != MC_SOUND_BACKEND_OK)
            retain_failed_pcm(b, source, &buffers[i]);
        backend_pcm_free(&buffers[i]);
    }
    return status;
}

mc_sound_backend_status backend_stream_pass(mc_sound_backend *b) {
    mtx_lock(&b->stream_mutex);
    backend_source **at = &b->watch_head;
    b->watch_tail = NULL;
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    while (*at != NULL) {
        backend_source *source = *at;
        bool remove = source->stopped || !source->active;
        bool progressed = true;
        if (!remove && !source->paused) {
            if (source->preloading) {
                status = preload(b, source);
                if (status != MC_SOUND_BACKEND_OK)
                    break;
            } else if (source->channel == NULL || source->codec.kind == BACKEND_CODEC_NONE)
                progressed = false;
            else {
                const int processed = backend_channel_processed(b, source->channel);
                for (int i = 0; i < processed; ++i) {
                    backend_pcm pcm = {0};
                    ++b->state.stream_reads;
                    status = backend_codec_read(b, &source->codec, false, &pcm);
                    if (status == MC_SOUND_BACKEND_OK && pcm.present && !pcm.data_is_null &&
                        !backend_channel_queue(b, source->channel, &pcm)) {
                        ++b->state.stream_queue_failures;
                        status = transfer_failure(b, "OpenAL stream transfer rejected");
                    }
                    if (status != MC_SOUND_BACKEND_OK)
                        retain_failed_pcm(b, source, &pcm);
                    backend_pcm_free(&pcm);
                    if (status != MC_SOUND_BACKEND_OK)
                        break;
                    if (source->codec.eof) {
                        progressed = false;
                        break;
                    }
                }
                if (status != MC_SOUND_BACKEND_OK)
                    break;
            }
            /* SoundManager never adds sound sequences or fades. Their reached
             * default check returns false before a wall-clock sample. */
            if (!progressed && !backend_source_playing(b, source)) {
                if (source->looping)
                    source->preloading = true;
                else
                    remove = true;
            }
        }
        if (remove) {
            *at = source->watch_next;
            source->watch_next = NULL;
            source->watched = false;
            --b->watch_count;
            source_release(b, source);
        } else {
            b->watch_tail = source;
            at = &source->watch_next;
        }
    }
    mtx_unlock(&b->stream_mutex);
    return status;
}

mc_sound_backend_status backend_sources_cleanup(mc_sound_backend *b) {
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    backend_channel *lists[2] = {b->streaming, b->normal};
    const size_t counts[2] = {b->streaming_count, b->normal_count};
    if (b->context != NULL) {
        for (size_t list = 0; list < 2u; ++list)
            for (size_t i = 0; i < counts[list]; ++i) {
                backend_channel *channel = &lists[list][i];
                if (channel->handle == 0)
                    continue;
                backend_channel_close(b, channel);
                alSourceStop(channel->handle);
                (void)alGetError();
                alDeleteSources(1, &channel->handle);
                const ALenum code = backend_al_error(b, "OpenAL native source retirement");
                if (code != AL_NO_ERROR) {
                    /* Keep this handle, attachment and the remaining channel
                     * arrays available for a later native retirement attempt. */
                    const mc_sound_backend_status failed = backend_fail(
                        b, MC_SOUND_BACKEND_DEVICE_FAILED, "OpenAL native source retirement", 0);
                    b->state.last_error.al_code = code;
                    return failed;
                }
                channel->handle = 0;
                backend_source *source = channel->attached;
                channel->attached = NULL;
                source_release(b, source);
                backend_free(b, channel->remembered_name);
                backend_free(b, channel->staging);
                channel->remembered_name = NULL;
                channel->staging = NULL;
            }
    }
    backend_free(b, b->normal);
    backend_free(b, b->streaming);
    b->normal = b->streaming = NULL;
    b->normal_count = b->streaming_count = 0;
    if (b->source_buckets != NULL)
        for (size_t i = 0; i < b->source_capacity; ++i)
            while (b->source_buckets[i] != NULL) {
                backend_source *source = b->source_buckets[i];
                b->source_buckets[i] = source->hash_next;
                source->hash_next = NULL;
                const mc_sound_backend_status retired = backend_codec_free(b, &source->codec);
                if (retired != MC_SOUND_BACKEND_OK)
                    status = retired;
                source->current = false;
                source_release(b, source);
            }
    while (b->watch_head != NULL) {
        backend_source *source = b->watch_head;
        b->watch_head = source->watch_next;
        source->watch_next = NULL;
        source->watched = false;
        source_release(b, source);
    }
    b->watch_tail = NULL;
    b->watch_count = b->source_size = 0;
    if (b->cache_buckets != NULL)
        for (size_t i = 0; i < b->cache_capacity; ++i)
            while (b->cache_buckets[i] != NULL) {
                backend_cache *cache = b->cache_buckets[i];
                b->cache_buckets[i] = cache->next;
                if (b->context != NULL && cache->device_present) {
                    (void)backend_delete_buffer(b, cache->buffer, "OpenAL normal buffer cleanup");
                }
                b->state.allocation_bytes -= cache->decoded.bytes;
                backend_pcm_free(&cache->decoded);
                backend_resource_free(b, &cache->resource);
                backend_free(b, cache);
            }
    b->cache_size = 0;
    backend_free(b, b->source_buckets);
    backend_free(b, b->cache_buckets);
    b->source_buckets = NULL;
    b->cache_buckets = NULL;
    return status;
}
