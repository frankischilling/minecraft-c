#ifndef MC_CLIENT_SOUND_BACKEND_H
#define MC_CLIENT_SOUND_BACKEND_H

#include "mc/client/sound/sound_manager.h"
#include "mc/client/sound/vorbis_codec.h"
#include "mc/client/sound/wav_codec.h"

typedef struct mc_sound_backend mc_sound_backend;

typedef enum mc_sound_backend_status {
    MC_SOUND_BACKEND_OK = 0,
    MC_SOUND_BACKEND_INVALID_ARGUMENT,
    MC_SOUND_BACKEND_BUSY,
    MC_SOUND_BACKEND_LIMIT,
    MC_SOUND_BACKEND_ALLOCATION_FAILED,
    MC_SOUND_BACKEND_THREAD_FAILED,
    MC_SOUND_BACKEND_DEVICE_FAILED,
    MC_SOUND_BACKEND_CODEC_FAILED,
    MC_SOUND_BACKEND_CLOSED,
    MC_SOUND_BACKEND_UNSUPPORTED,
    MC_SOUND_BACKEND_SOURCE_EXCEPTION
} mc_sound_backend_status;

typedef struct mc_sound_backend_error {
    mc_sound_backend_status status;
    const char *operation; /* static borrowed native diagnostic */
    int al_code, alc_code, thread_code, codec_code;
    mc_vorbis_codec_error vorbis;
    mc_wav_codec_error wav;
} mc_sound_backend_error;

typedef struct mc_sound_backend_options {
    const char *device_name; /* copied; NULL selects the actual default device */
    bool loopback, manual_service;
    uint32_t output_rate;
    int32_t normal_channels, streaming_channels, streaming_buffers;
    int32_t streaming_buffer_size, max_file_size, file_chunk_size;
    size_t allocation_bytes, max_sources, max_commands, max_cached_buffers;
    size_t input_bytes, pcm_bytes, max_device_buffers;
} mc_sound_backend_options;

typedef struct mc_sound_backend_state {
    bool constructed, device_open, context_current, loopback, manual_service;
    bool command_worker_started, stream_worker_started;
    bool command_worker_alive, stream_worker_alive, worker_failed;
    bool source_cleanup_reached, native_workers_joined;
    size_t allocation_bytes, sources, current_sources, watched_sources;
    size_t decoded_buffers, device_buffers, normal_channels, streaming_channels;
    size_t device_buffers_created, device_buffers_deleted;
    size_t queued_commands, deferred_plays, submitted, applied;
    size_t play_attempts, quick_play_attempts, stops, stream_reads, pcm_bytes_queued;
    size_t preload_failures, stream_queue_failures, retained_pcm_bytes;
    size_t al_errors, source_rejections;
    int32_t alc_major, alc_minor;
    mc_java_random constructor_random;
    mc_sound_backend_error last_error;
    mc_sound_backend_error worker_error;
    char vendor[128], renderer[128], version[128];
} mc_sound_backend_state;

typedef struct mc_sound_backend_source_state {
    bool present, streaming, priority, looping, stopped, paused, active, preloading;
    bool attached, playing;
    float x, y, z, volume, pitch, gain, distance;
    int32_t attenuation;
    int32_t channel_index;
    uint32_t channel_handle;
} mc_sound_backend_source_state;

/* Source configuration defaults plus explicit native ownership ceilings.
 * The normal route opens a real default device and starts separate command and
 * streaming workers. Loopback/manual modes are explicit diagnostic profiles,
 * not replacements for ordinary device playback or original thread timing. */
mc_sound_backend_options mc_sound_backend_default_options(void);
/* Allocate callback context only. The supplied profile stays borrowed through
 * retirement; actual SoundSystem construction consumes its additional default
 * Random before queues/workers/device initialization. No device opens here. */
mc_sound_backend_status mc_sound_backend_create(mc_java_random_profile *profile,
                                                const mc_sound_backend_options *options,
                                                mc_sound_backend **out,
                                                mc_sound_backend_error *error);
bool mc_sound_backend_manager_interface(mc_sound_backend *backend, mc_sound_manager_backend *out);
bool mc_sound_backend_read_state(mc_sound_backend *backend, mc_sound_backend_state *out);
/* Actual channel query under the backend lock; this is not a queue drain or a
 * passive claim about future commands. Unknown names produce present=false. */
mc_sound_backend_status mc_sound_backend_read_source(mc_sound_backend *backend,
                                                     const char *source_name,
                                                     mc_sound_backend_source_state *out,
                                                     mc_sound_backend_error *error);
/* Manual profiles alone can explicitly apply one ordinary/deferred command
 * drain or one streaming pass. Device calls and codecs remain real. */
mc_sound_backend_status mc_sound_backend_service_commands(mc_sound_backend *backend,
                                                          mc_sound_backend_error *error);
mc_sound_backend_status mc_sound_backend_service_streams(mc_sound_backend *backend,
                                                         mc_sound_backend_error *error);
/* Render actual ALC_SOFT_loopback stereo signed16 output. Only manual loopback
 * profiles admit this call; output contains 2*frames writable samples. */
mc_sound_backend_status mc_sound_backend_render(mc_sound_backend *backend, int16_t *output,
                                                size_t frames, mc_sound_backend_error *error);
/* Wait for current ordinary/deferred queue work, without treating admission as
 * playback. Stream completion and audible output are separate observations. */
mc_sound_backend_status mc_sound_backend_wait_idle(mc_sound_backend *backend,
                                                   uint32_t timeout_millis,
                                                   mc_sound_backend_error *error);
/* The SoundManager shared owner must first call its backend retire callback.
 * This context retirement rejects a still-published instance, preserving the
 * callbacks' borrowed context lifetime. No implicit source cleanup is added. */
mc_sound_backend_status mc_sound_backend_retire(mc_sound_backend **backend,
                                                mc_sound_backend_error *error);

#endif
