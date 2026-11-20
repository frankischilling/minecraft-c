#ifndef MC_CLIENT_SOUND_BACKEND_INTERNAL_H
#define MC_CLIENT_SOUND_BACKEND_INTERNAL_H

#include "mc/client/sound/backend.h"
#include "mc/client/sound/vorbis_codec.h"
#include "mc/client/sound/wav_codec.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <stdatomic.h>
#include <threads.h>

typedef struct backend_resource {
    mc_sound_text logical;
    mc_sound_file_identity identity;
    uint16_t *logical_units, *url_units;
    char *path;
} backend_resource;

typedef enum backend_codec_kind {
    BACKEND_CODEC_NONE,
    BACKEND_CODEC_OGG,
    BACKEND_CODEC_MUS,
    BACKEND_CODEC_WAV
} backend_codec_kind;

typedef struct backend_codec {
    backend_codec_kind kind;
    mc_vorbis_codec *vorbis;
    mc_wav_codec *wav;
    bool format_present, eof;
    int32_t channels, bits, rate;
} backend_codec;

typedef struct backend_pcm {
    bool present, data_is_null;
    uint8_t *data;
    size_t bytes;
    int32_t channels, bits, rate;
} backend_pcm;

typedef struct backend_cache {
    struct backend_cache *next;
    backend_resource resource;
    uint32_t hash;
    backend_pcm decoded;
    ALuint buffer;
    bool device_present;
} backend_cache;

typedef struct backend_source backend_source;

typedef struct backend_channel {
    ALuint handle;
    int32_t type, index, format, rate;
    char *remembered_name;
    backend_source *attached;
    uint8_t *staging;
} backend_channel;

struct backend_source {
    backend_source *next, *hash_next, *watch_next;
    size_t references;
    uint32_t hash;
    char *name;
    backend_resource resource;
    backend_cache *cache;
    backend_codec codec;
    backend_pcm retained_pcm[64];
    size_t retained_pcm_count;
    backend_channel *channel;
    bool current, watched, streaming, priority, looping, active, stopped, paused, preloading;
    float x, y, z, volume, pitch, gain, distance, dist_or_roll;
    int32_t attenuation;
};

typedef struct backend_request {
    struct backend_request *next;
    mc_sound_manager_command command;
    char *source_name;
    backend_resource resource;
    bool has_resource, quick;
} backend_request;

struct mc_sound_backend {
    mc_java_random_profile *profile;
    mc_sound_backend_options options;
    char *device_name;
    mc_sound_backend_state state;
    bool configured[4], instance, constructing, initialized, command_started, stream_started;
    bool initialize_requested, initialization_done;
    atomic_bool dying;
    bool queue_ready, state_ready, stream_ready, command_condition_ready, stream_condition_ready;
    mtx_t queue_mutex, state_mutex, stream_mutex;
    cnd_t command_condition, stream_condition;
    thrd_t command_thread, stream_thread;
    bool command_alive, stream_alive, command_faulted;
    ALCdevice *device;
    ALCcontext *context;
    LPALCRENDERSAMPLESSOFT render_samples;
    ALuint *device_buffer_names;
    size_t device_buffer_count, device_buffer_capacity;
    backend_channel *normal, *streaming;
    size_t normal_count, streaming_count, next_normal, next_streaming;
    backend_source *sources, **source_buckets, *watch_head, *watch_tail;
    size_t source_capacity, source_size, source_threshold, watch_count;
    backend_cache **cache_buckets;
    size_t cache_capacity, cache_size, cache_threshold;
    backend_request *queue_head, *queue_tail, *deferred_head, *deferred_tail;
    size_t queue_count, deferred_count, allocated_sources, allocated_cache;
    size_t stream_wake_generation;
    float listener_position[3], listener_data_position[3], listener_orientation[6];
    bool pitch_supported;
    int64_t maintenance_time;
};

mc_sound_backend_status backend_fail(mc_sound_backend *b, mc_sound_backend_status status,
                                     const char *operation, int code);
void *backend_alloc(mc_sound_backend *b, size_t bytes);
void backend_free(mc_sound_backend *b, void *pointer);
char *backend_string(mc_sound_backend *b, const char *text);
bool backend_resource_copy(mc_sound_backend *b, const mc_sound_pool_entry *entry,
                           backend_resource *out);
bool backend_resource_clone(mc_sound_backend *b, const backend_resource *input,
                            backend_resource *out);
void backend_resource_free(mc_sound_backend *b, backend_resource *resource);
uint32_t backend_text_hash(mc_sound_text text);
uint32_t backend_name_hash(const char *name);
mc_sound_backend_status backend_device_initialize(mc_sound_backend *b);
mc_sound_backend_status backend_device_cleanup(mc_sound_backend *b);
ALenum backend_al_error(mc_sound_backend *b, const char *operation);
bool backend_channel_playing(mc_sound_backend *b, const backend_channel *channel);
void backend_channel_close(mc_sound_backend *b, backend_channel *channel);
void backend_channel_stop(mc_sound_backend *b, backend_channel *channel);
bool backend_channel_preload(mc_sound_backend *b, backend_channel *channel, backend_pcm *pcm,
                             size_t count);
bool backend_channel_queue(mc_sound_backend *b, backend_channel *channel, const backend_pcm *pcm);
bool backend_generate_buffer(mc_sound_backend *b, ALuint *buffer);
bool backend_delete_buffer(mc_sound_backend *b, ALuint buffer, const char *operation);
int backend_channel_processed(mc_sound_backend *b, const backend_channel *channel);
mc_sound_backend_status backend_codec_create(mc_sound_backend *b, const backend_resource *resource,
                                             backend_codec *codec);
mc_sound_backend_status backend_codec_initialize(mc_sound_backend *b, backend_codec *codec,
                                                 const backend_resource *resource);
mc_sound_backend_status backend_codec_read(mc_sound_backend *b, backend_codec *codec, bool all,
                                           backend_pcm *out);
void backend_pcm_free(backend_pcm *pcm);
mc_sound_backend_status backend_codec_free(mc_sound_backend *b, backend_codec *codec);
backend_source *backend_source_find(mc_sound_backend *b, const char *name);
mc_sound_backend_status backend_new_source(mc_sound_backend *b, const backend_request *request);
mc_sound_backend_status backend_source_play(mc_sound_backend *b, backend_source *source);
bool backend_source_playing(mc_sound_backend *b, backend_source *source);
void backend_source_stop(mc_sound_backend *b, backend_source *source);
void backend_source_position_changed(mc_sound_backend *b, backend_source *source);
void backend_source_set_pitch(mc_sound_backend *b, backend_source *source, float value);
void backend_source_watch(mc_sound_backend *b, backend_source *source);
mc_sound_backend_status backend_stream_pass(mc_sound_backend *b);
mc_sound_backend_status backend_sources_cleanup(mc_sound_backend *b);
mc_sound_backend_status backend_drain(mc_sound_backend *b);
void backend_request_free(mc_sound_backend *b, backend_request *request);
mc_sound_backend_status backend_workers_start(mc_sound_backend *b);
mc_sound_backend_status backend_workers_stop(mc_sound_backend *b);
int backend_command_worker(void *context);
int backend_stream_worker(void *context);
int64_t backend_now(void);
struct timespec backend_deadline(uint32_t millis);
mc_sound_status backend_sound_status(mc_sound_backend_status status);
mc_sound_backend_status backend_source_cleanup(mc_sound_backend *b);
mc_sound_backend_status backend_native_cleanup(mc_sound_backend *b);

#endif
