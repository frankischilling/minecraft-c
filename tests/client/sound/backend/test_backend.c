#include "mc/client/sound/backend.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

static unsigned checks;
static bool cleanup_failed;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_sound_backend *backend;
    mc_sound_manager_backend calls;
    mc_sound_manager_shared *shared;
    mc_sound_manager *manager;
    mc_game_settings *settings;
    mc_sound_pool *pool;
    void *handle;
} fixture;

static mc_sound_text ascii(const char *name, uint16_t out[128]) {
    const size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        out[i] = (uint8_t)name[i];
    return (mc_sound_text){out, length, false};
}

static void fixture_free(fixture *f) {
    if (f->settings != NULL)
        mc_game_settings_attach_sound_manager(f->settings, NULL, NULL);
    mc_sound_manager_retire(&f->manager, NULL);
    mc_sound_manager_shared_retire(&f->shared, NULL);
    if (f->handle != NULL)
        if (f->calls.retire(f->calls.context, &f->handle, NULL) != MC_SOUND_OK)
            cleanup_failed = true;
    mc_game_settings_retire(&f->settings, NULL);
    mc_sound_pool_free(f->pool, NULL);
    if (f->backend != NULL) {
        mc_sound_backend_state state;
        if (!mc_sound_backend_read_state(f->backend, &state) || state.device_open ||
            state.command_worker_alive || state.stream_worker_alive || state.device_buffers != 0 ||
            state.device_buffers_created != state.device_buffers_deleted) {
            fprintf(stderr, "Backend native cleanup left device/buffer/worker ownership\n");
            cleanup_failed = true;
        }
    }
    if (mc_sound_backend_retire(&f->backend, NULL) != MC_SOUND_BACKEND_OK)
        cleanup_failed = true;
    else
        mc_java_random_profile_free(f->profile);
}

static bool fixture_create_options(fixture *f, mc_sound_backend_options options) {
    if (mc_java_random_profile_create(4096, &f->profile) != MC_JAVA_RANDOM_PROFILE_OK)
        return false;
    if (mc_sound_backend_create(f->profile, &options, &f->backend, NULL) != MC_SOUND_BACKEND_OK ||
        !mc_sound_backend_manager_interface(f->backend, &f->calls))
        return false;
    for (int i = 0; i < 4; ++i)
        if (f->calls.configure(f->calls.context, (mc_sound_manager_configuration)i, NULL) !=
            MC_SOUND_OK)
            return false;
    if (f->calls.construct(f->calls.context, &f->handle, NULL) != MC_SOUND_OK)
        return false;
    return mc_sound_pool_create(f->profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &f->pool, NULL) ==
           MC_SOUND_OK;
}

static bool fixture_create(fixture *f, bool manual, bool loopback) {
    mc_sound_backend_options options = mc_sound_backend_default_options();
    options.loopback = loopback;
    options.manual_service = manual;
    options.normal_channels = 2;
    options.streaming_channels = 1;
    return fixture_create_options(f, options);
}

static const mc_sound_pool_entry *entry(fixture *f, const char *root, const char *logical,
                                        const char *leaf) {
    char path[32768];
    const int length = snprintf(path, sizeof(path), "%s/%s", root, leaf);
    if (length < 0 || (size_t)length >= sizeof(path))
        return NULL;
    uint16_t units[128];
    const mc_sound_pool_entry *result = NULL;
    if (mc_sound_pool_add_file(f->pool, ascii(logical, units), path, &result, NULL, NULL) !=
        MC_SOUND_OK)
        return NULL;
    return result;
}

static bool command(fixture *f, mc_sound_manager_command request) {
    return f->calls.command(f->calls.context, f->handle, &request, NULL) == MC_SOUND_OK;
}

static bool service(fixture *f) {
    return mc_sound_backend_service_commands(f->backend, NULL) == MC_SOUND_BACKEND_OK;
}

static bool read_source(fixture *f, const char *name, mc_sound_backend_source_state *out) {
    return mc_sound_backend_read_source(f->backend, name, out, NULL) == MC_SOUND_BACKEND_OK;
}

static bool manual_normal(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_state state;
    mc_sound_backend_source_state source;
    int16_t *pcm = calloc(16384u, sizeof(*pcm));
    CHECK(pcm != NULL && fixture_create(&f, true, true));
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.constructed &&
          state.device_open && state.loopback && state.manual_service &&
          state.normal_channels == 2 && state.streaming_channels == 1 &&
          !state.command_worker_started && !state.stream_worker_started &&
          state.vendor[0] != '\0' && state.renderer[0] != '\0' &&
          !state.constructor_random.have_next_next_gaussian);
    const mc_sound_pool_entry *tone = entry(&f, root, "tone1.ogg", "mono32k.ogg");
    CHECK(tone != NULL);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "normal",
                                                 .entry = tone}));
    CHECK(command(
        &f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "normal"}));
    CHECK(command(
        &f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_STOP, .source_name = "normal"}));
    bool playing = true;
    CHECK(f.calls.playing(f.calls.context, f.handle, "normal", &playing, NULL) == MC_SOUND_OK &&
          !playing);
    /* Pool retirement precedes command application: commands own real copies. */
    CHECK(mc_sound_pool_free(f.pool, NULL) == MC_SOUND_OK);
    f.pool = NULL;
    CHECK(service(&f));
    CHECK(read_source(&f, "normal", &source) && source.present && source.playing &&
          !source.stopped && source.attached);
    CHECK(mc_sound_backend_render(f.backend, pcm, 2048u, NULL) == MC_SOUND_BACKEND_OK);
    int peak = 0;
    for (size_t i = 0; i < 4096u; ++i) {
        int sample = pcm[i];
        if (sample < 0)
            sample = -sample;
        if (sample > peak)
            peak = sample;
    }
    CHECK(peak > 1000); /* actual rendered authored audio, not a playing flag */
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_SET_PITCH,
                                                 .source_name = "normal",
                                                 .value = 9.0f}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_SET_VOLUME,
                                                 .source_name = "normal",
                                                 .value = 2.0f}));
    CHECK(service(&f) && read_source(&f, "normal", &source) && source.pitch == 2.0f &&
          source.volume == 1.0f);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.current_sources == 1 &&
          state.decoded_buffers == 1 && state.queued_commands == 0 && state.deferred_plays == 0 &&
          state.play_attempts == 1 && !state.worker_failed);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_CLEANUP}));
    CHECK(mc_sound_backend_read_state(f.backend, &state) && !state.device_open &&
          !state.constructed && state.source_cleanup_reached && state.native_workers_joined &&
          state.sources == 0);
    CHECK(f.calls.playing(f.calls.context, f.handle, "normal", &playing, NULL) == MC_SOUND_OK &&
          !playing);
    CHECK(f.calls.command(
              f.calls.context, f.handle,
              &(mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "normal"},
              NULL) == MC_SOUND_SOURCE_EXCEPTION);
    ok = true;
cleanup:
    fixture_free(&f);
    free(pcm);
    return ok;
}

static bool selection_and_listener(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_source_state a, b, c;
    mc_sound_backend_state state;
    CHECK(fixture_create(&f, true, true));
    const mc_sound_pool_entry *tone = entry(&f, root, "shared.ogg", "mono32k.ogg");
    const mc_sound_pool_entry *other = entry(&f, root, "shared.ogg", "zero-mono32k.ogg");
    CHECK(tone != NULL && other != NULL);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "a",
                                                 .entry = tone,
                                                 .priority = true,
                                                 .x = 16.0f,
                                                 .attenuation = 2,
                                                 .distance = 32.0f,
                                                 .looping = true}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "b",
                                                 .entry = other,
                                                 .priority = true,
                                                 .looping = true}));
    CHECK(command(&f, (mc_sound_manager_command){
                          .kind = MC_SOUND_MANAGER_NEW_SOURCE, .source_name = "c", .entry = tone}));
    CHECK(
        command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "a"}));
    CHECK(
        command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "b"}));
    CHECK(
        command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "c"}));
    CHECK(service(&f));
    CHECK(read_source(&f, "a", &a) && read_source(&f, "b", &b) && read_source(&f, "c", &c));
    CHECK(a.playing && b.playing && !c.playing && a.channel_index != b.channel_index &&
          a.gain == 0.5f);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.decoded_buffers == 1);
    CHECK(command(
        &f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_LISTENER_POSITION, .x = 16.0f}));
    CHECK(service(&f) && read_source(&f, "a", &a) && a.gain == 0.5f);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_LISTENER_ORIENTATION,
                                                 .orientation = {0, 0, -1, 0, 1, 0}}));
    CHECK(service(&f) && read_source(&f, "a", &a) && a.gain == 1.0f);
    CHECK(
        command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_STOP, .source_name = "a"}));
    CHECK(
        command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "c"}));
    CHECK(service(&f) && read_source(&f, "c", &c) && c.playing);
    CHECK(read_source(&f, "a", &a) && !a.attached && !a.playing);
    /* A stale retained Source.channel still stops the newly attached source. */
    CHECK(
        command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_STOP, .source_name = "a"}));
    CHECK(service(&f) && read_source(&f, "c", &c) && !c.playing && !c.stopped);
    ok = true;
cleanup:
    fixture_free(&f);
    return ok;
}

static bool streaming(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_source_state source;
    mc_sound_backend_state state;
    int16_t *pcm = calloc(16384u, sizeof(*pcm));
    CHECK(pcm != NULL && fixture_create(&f, true, true));
    const mc_sound_pool_entry *tone = entry(&f, root, "music.ogg", "mono32k.ogg");
    CHECK(tone != NULL);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_BACKGROUND_MUSIC,
                                                 .source_name = "BgMusic",
                                                 .entry = tone}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_SET_VOLUME,
                                                 .source_name = "BgMusic",
                                                 .value = 0.5f}));
    CHECK(command(
        &f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "BgMusic"}));
    CHECK(service(&f));
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.quick_play_attempts == 1 &&
          state.play_attempts == 3 && state.watched_sources == 1);
    CHECK(read_source(&f, "BgMusic", &source) && source.present && source.streaming &&
          source.preloading && !source.playing);
    CHECK(mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(read_source(&f, "BgMusic", &source) && source.playing && !source.preloading);
    CHECK(mc_sound_backend_render(f.backend, pcm, 8192u, NULL) == MC_SOUND_BACKEND_OK);
    int peak = 0;
    for (size_t i = 0; i < 16384u; ++i) {
        int value = pcm[i];
        if (value < 0)
            value = -value;
        if (value > peak)
            peak = value;
    }
    CHECK(peak > 500);
    CHECK(mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(read_source(&f, "BgMusic", &source) && !source.playing && !source.stopped &&
          source.attached);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.watched_sources == 0 &&
          state.stream_reads >= 3 && state.pcm_bytes_queued == 8192u);
    ok = true;
cleanup:
    fixture_free(&f);
    free(pcm);
    return ok;
}

static bool worker_profile(const char *root, bool default_device) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_state state;
    mc_sound_backend_source_state source;
    CHECK(fixture_create(&f, false, !default_device));
    const mc_sound_pool_entry *tone = entry(&f, root, "worker.ogg", "mono32k.ogg");
    CHECK(tone != NULL);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "worker",
                                                 .entry = tone,
                                                 .looping = true}));
    CHECK(command(
        &f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "worker"}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_STREAMING_SOURCE,
                                                 .source_name = "worker-stream",
                                                 .entry = tone}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY,
                                                 .source_name = "worker-stream"}));
    CHECK(mc_sound_backend_wait_idle(f.backend, 5000u, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(read_source(&f, "worker", &source) && source.present && source.playing);
    bool stream_playing = false;
    for (unsigned i = 0; i < 2000u; ++i) {
        CHECK(read_source(&f, "worker-stream", &source));
        if (source.playing && !source.preloading) {
            stream_playing = true;
            break;
        }
        const struct timespec delay = {0, 1000000L};
        thrd_sleep(&delay, NULL);
    }
    CHECK(stream_playing);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.command_worker_started &&
          state.stream_worker_started && state.command_worker_alive && state.stream_worker_alive &&
          !state.worker_failed && state.stream_reads >= 2 && state.pcm_bytes_queued == 8192u);
    if (default_device) {
        const struct timespec delay = {0, 150000000L};
        thrd_sleep(&delay, NULL);
    }
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_CLEANUP}));
    CHECK(mc_sound_backend_read_state(f.backend, &state) && !state.command_worker_alive &&
          !state.stream_worker_alive && state.native_workers_joined && !state.device_open);
    ok = true;
cleanup:
    fixture_free(&f);
    return ok;
}

static bool wav_stream_refill(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_state state;
    mc_sound_backend_source_state source;
    int16_t pcm[1024];
    int peak = 0;
    mc_sound_backend_options options = mc_sound_backend_default_options();
    options.loopback = options.manual_service = true;
    options.normal_channels = 2;
    options.streaming_channels = 1;
    options.streaming_buffer_size = 4096;
    CHECK(fixture_create_options(&f, options));
    const mc_sound_pool_entry *tone =
        entry(&f, root, "wave.wav", "../../backend/fixtures/square-mono44k.wav");
    CHECK(tone != NULL);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_STREAMING_SOURCE,
                                                 .source_name = "wave",
                                                 .entry = tone}));
    CHECK(command(
        &f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "wave"}));
    CHECK(service(&f));
    CHECK(mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.stream_reads == 3 &&
          state.pcm_bytes_queued == 12288u && state.watched_sources == 1);
    for (unsigned pass = 0; pass < 40u; ++pass) {
        CHECK(mc_sound_backend_render(f.backend, pcm, 512u, NULL) == MC_SOUND_BACKEND_OK);
        for (size_t i = 0; i < 1024u; ++i) {
            int sample = pcm[i];
            if (sample < 0)
                sample = -sample;
            if (sample > peak)
                peak = sample;
        }
        CHECK(mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    }
    CHECK(peak > 1000);
    CHECK(read_source(&f, "wave", &source) && !source.playing && !source.stopped &&
          source.attached);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.stream_reads >= 9 &&
          state.pcm_bytes_queued == 32768u && state.watched_sources == 0 && state.al_errors == 0 &&
          !state.worker_failed && state.retained_pcm_bytes == 0);
    ok = true;
cleanup:
    fixture_free(&f);
    return ok;
}

static bool failed_stream_transfer(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_state state;
    mc_sound_backend_options options = mc_sound_backend_default_options();
    options.loopback = options.manual_service = true;
    options.normal_channels = 2;
    options.streaming_channels = 1;
    options.max_device_buffers = 1;
    CHECK(fixture_create_options(&f, options));
    const mc_sound_pool_entry *tone = entry(&f, root, "limited.ogg", "mono32k.ogg");
    CHECK(tone != NULL);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "normal",
                                                 .entry = tone}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_BACKGROUND_MUSIC,
                                                 .source_name = "BgMusic",
                                                 .entry = tone}));
    CHECK(service(&f));
    mc_sound_backend_error error;
    CHECK(mc_sound_backend_service_streams(f.backend, &error) == MC_SOUND_BACKEND_LIMIT &&
          error.status == MC_SOUND_BACKEND_LIMIT);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.worker_failed &&
          state.worker_error.status == MC_SOUND_BACKEND_LIMIT && state.preload_failures == 1 &&
          state.pcm_bytes_queued == 0 && state.retained_pcm_bytes == 8192u);
    /* A native transfer ceiling retains already-decoded PCM until cleanup;
     * it never reports a successful refill or consumes further codec data. */
    const size_t reads = state.stream_reads;
    CHECK(mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_LIMIT);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.stream_reads == reads);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_CLEANUP}));
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.retained_pcm_bytes == 0 &&
          !state.device_open && state.native_workers_joined && state.sources == 0);
    ok = true;
cleanup:
    fixture_free(&f);
    return ok;
}

static bool displaced_sources(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_state state;
    mc_sound_backend_source_state source;
    int16_t pcm[1024];
    CHECK(fixture_create(&f, true, true));
    const mc_sound_pool_entry *tone = entry(&f, root, "displaced.ogg", "mono32k.ogg");
    CHECK(tone != NULL);
    const mc_sound_manager_command normal = {
        .kind = MC_SOUND_MANAGER_NEW_SOURCE, .source_name = "same", .entry = tone};
    const mc_sound_manager_command play = {.kind = MC_SOUND_MANAGER_PLAY, .source_name = "same"};
    CHECK(command(&f, normal) && command(&f, play) && service(&f));
    CHECK(read_source(&f, "same", &source) && source.playing);
    const uint32_t normal_handle = source.channel_handle;
    CHECK(command(&f, normal) && service(&f));
    CHECK(read_source(&f, "same", &source) && source.stopped && !source.attached &&
          !source.playing);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.sources == 2 &&
          state.current_sources == 1);
    CHECK(mc_sound_backend_render(f.backend, pcm, 512u, NULL) == MC_SOUND_BACKEND_OK);
    bool nonzero = false;
    for (size_t i = 0; i < 1024u; ++i)
        nonzero = nonzero || pcm[i] != 0;
    CHECK(nonzero); /* displaced old source remains attached and audible */
    CHECK(command(&f, play) && service(&f));
    CHECK(read_source(&f, "same", &source) && source.playing &&
          source.channel_handle == normal_handle);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.sources == 1 &&
          state.current_sources == 1);

    const mc_sound_manager_command stream = {.kind = MC_SOUND_MANAGER_NEW_STREAMING_SOURCE,
                                             .source_name = "replace-stream",
                                             .entry = tone};
    const mc_sound_manager_command stream_play = {.kind = MC_SOUND_MANAGER_PLAY,
                                                  .source_name = "replace-stream"};
    CHECK(command(&f, stream) && command(&f, stream_play) && service(&f));
    CHECK(mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(command(&f, stream) && service(&f));
    CHECK(read_source(&f, "replace-stream", &source) && !source.attached && source.stopped);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.sources == 3 &&
          state.current_sources == 2 && state.watched_sources == 1);
    CHECK(command(&f, stream_play) && service(&f));
    CHECK(read_source(&f, "replace-stream", &source) && source.attached && source.preloading);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.sources == 2 &&
          state.current_sources == 2 && state.watched_sources == 1);
    CHECK(mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(read_source(&f, "replace-stream", &source) && source.playing);
    /* Native retirement alone must not claim the source cleanup call ran. */
    CHECK(f.calls.retire(f.calls.context, &f.handle, NULL) == MC_SOUND_OK && f.handle == NULL);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && !state.source_cleanup_reached &&
          !state.constructed && state.native_workers_joined && state.sources == 0 &&
          state.device_buffers == 0 &&
          state.device_buffers_created == state.device_buffers_deleted);
    ok = true;
cleanup:
    fixture_free(&f);
    return ok;
}

static void put_u32(uint8_t *out, uint32_t value) {
    for (unsigned i = 0; i < 4u; ++i)
        out[i] = (uint8_t)(value >> (8u * i));
}

static bool working_directory(char out[32768]) {
#ifdef _WIN32
    wchar_t wide[32768];
    const DWORD count = GetCurrentDirectoryW(32768u, wide);
    return count != 0 && count < 32768u &&
           WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, out, 32768, NULL, NULL) > 0;
#else
    return getcwd(out, 32768u) != NULL;
#endif
}

static bool partial_cache_publication(const char *root) {
    bool ok = false, created = false;
    fixture f = {0};
    FILE *file = NULL;
    char path[96] = {0};
    char directory[32768];
    mc_sound_backend_state state;
    struct timespec now;
    CHECK(timespec_get(&now, TIME_UTC) == TIME_UTC);
    CHECK(working_directory(directory));
    CHECK(snprintf(path, sizeof(path), "mc-backend-cache-%lld-%ld.wav", (long long)now.tv_sec,
                   now.tv_nsec) > 0);
    uint8_t wave[56] = {0};
    memcpy(wave, "RIFF", 4u);
    put_u32(wave + 4u, 48u);
    memcpy(wave + 8u, "WAVEfmt ", 8u);
    put_u32(wave + 16u, 16u);
    wave[20] = 1u; /* classic PCM, three channels, signed16 */
    wave[22] = 3u;
    put_u32(wave + 24u, 44100u);
    put_u32(wave + 28u, 264600u);
    wave[32] = 6u;
    wave[34] = 16u;
    memcpy(wave + 36u, "data", 4u);
    put_u32(wave + 40u, 12u);
#ifdef _WIN32
    CHECK(fopen_s(&file, path, "wbx") == 0);
#else
    file = fopen(path, "wbx");
#endif
    CHECK(file != NULL);
    created = true;
    CHECK(fwrite(wave, 1u, sizeof(wave), file) == sizeof(wave));
    CHECK(fclose(file) == 0);
    file = NULL;
    CHECK(fixture_create(&f, true, true));
    const mc_sound_pool_entry *unsupported = entry(&f, directory, "partial.wav", path);
    const mc_sound_pool_entry *valid =
        entry(&f, root, "partial.wav", "../../backend/fixtures/square-mono44k.wav");
    CHECK(unsupported != NULL && valid != NULL);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "partial",
                                                 .entry = unsupported}));
    CHECK(service(&f));
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.decoded_buffers == 1 &&
          state.device_buffers == 0 && state.sources == 0 && !state.worker_failed);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "partial",
                                                 .entry = valid}));
    CHECK(service(&f));
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.decoded_buffers == 1 &&
          state.device_buffers_created == 0 && state.sources == 0 && state.source_rejections == 3 &&
          !state.worker_failed);
    ok = true;
cleanup:
    if (file != NULL)
        fclose(file);
    fixture_free(&f);
    if (created && remove(path) != 0)
        ok = false;
    return ok;
}

static mc_game_settings_status manager_diagnostic(void *context,
                                                  const mc_game_settings_diagnostic *event,
                                                  mc_game_settings_error *error) {
    (void)error;
    unsigned *count = context;
    if (event->kind != MC_GAME_SETTINGS_SAVE_FAILED ||
        event->exception != MC_GAME_SETTINGS_SOURCE_NULL_FILE)
        return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
    ++*count;
    return MC_GAME_SETTINGS_OK;
}

static bool manager_integration(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_math_trig *trig = malloc(sizeof(*trig));
    mc_sound_manager_report report;
    mc_sound_manager_state manager;
    mc_sound_backend_state state;
    mc_sound_backend_source_state source;
    unsigned diagnostics = 0;
    uint16_t units[128];
    CHECK(trig != NULL && mc_math_trig_init(trig));
    CHECK(mc_java_random_profile_create(4096, &f.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_sound_backend_options options = mc_sound_backend_default_options();
    options.loopback = options.manual_service = true;
    options.normal_channels = 2;
    options.streaming_channels = 1;
    CHECK(mc_sound_backend_create(f.profile, &options, &f.backend, NULL) == MC_SOUND_BACKEND_OK &&
          mc_sound_backend_manager_interface(f.backend, &f.calls));
    CHECK(mc_sound_manager_shared_create(&f.calls, &f.shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_create(f.shared, f.profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES,
                                  &f.manager, NULL) == MC_SOUND_OK);
    CHECK(mc_game_settings_create_defaults(NULL, &f.settings, NULL) == MC_GAME_SETTINGS_OK);
    const mc_game_settings_diagnostics delivery = {&diagnostics, manager_diagnostic};
    CHECK(mc_sound_manager_bind(f.manager, f.settings, &delivery, &report, NULL) == MC_SOUND_OK &&
          diagnostics == 2 && report.setup.backend_published && report.setup.volumes_restored);
    CHECK(mc_game_settings_attach_sound_manager(f.settings, f.manager, NULL) ==
          MC_GAME_SETTINGS_OK);
    char path[32768];
    CHECK(snprintf(path, sizeof(path), "%s/mono32k.ogg", root) > 0);
    const mc_sound_pool_entry *registered = NULL;
    for (int i = 0; i < MC_SOUND_MANAGER_POOL_COUNT; ++i)
        CHECK(mc_sound_manager_add_file(f.manager, (mc_sound_manager_pool_kind)i,
                                        ascii("actual.ogg", units), path, &registered, NULL,
                                        NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_ui(f.manager, ascii("actual", units), 1.0f, 9.0f, NULL, NULL) ==
          MC_SOUND_OK);
    CHECK(mc_sound_manager_effect(f.manager, ascii("actual", units), 8.0f, 0, 0, 1.5f, 0.25f, NULL,
                                  NULL) == MC_SOUND_OK);
    CHECK(service(&f));
    CHECK(read_source(&f, "sound_1", &source) && source.playing && source.attenuation == 0 &&
          source.volume == 0.25f && source.pitch == 2.0f);
    CHECK(read_source(&f, "sound_2", &source) && source.playing && source.priority &&
          source.volume == 1.0f && source.pitch == 0.5f);
    CHECK(mc_sound_manager_stream(f.manager, ascii("actual", units), 0, 0, 0, 1.0f, 9.0f, NULL,
                                  NULL) == MC_SOUND_OK);
    CHECK(read_source(&f, "streaming", &source) && !source.present);
    CHECK(service(&f) && mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(read_source(&f, "streaming", &source) && source.playing && source.priority &&
          source.pitch == 1.0f && source.volume == 0.5f);
    const mc_sound_manager_listener listener = {.previous_x = 8.0, .x = 8.0};
    CHECK(mc_sound_manager_listener_update(f.manager, &listener, 1.0f, trig, NULL, NULL) ==
          MC_SOUND_OK);
    CHECK(service(&f) && read_source(&f, "sound_2", &source) && source.gain == 1.0f);
    CHECK(mc_sound_manager_stream(f.manager, (mc_sound_text){.is_null = true}, 0, 0, 0, 1, 1, NULL,
                                  NULL) == MC_SOUND_OK);
    CHECK(service(&f) && mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(mc_sound_manager_read_state(f.manager, &manager));
    for (int32_t i = 0; i <= manager.music_delay; ++i)
        CHECK(mc_sound_manager_music_tick(f.manager, NULL, NULL) == MC_SOUND_OK);
    CHECK(service(&f) && mc_sound_backend_service_streams(f.backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(read_source(&f, "BgMusic", &source) && source.playing && source.streaming);
    CHECK(mc_sound_manager_cleanup(f.manager, NULL, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.source_cleanup_reached &&
          state.native_workers_joined && !state.device_open && state.device_buffers_created > 0 &&
          state.device_buffers_created == state.device_buffers_deleted);
    CHECK(mc_sound_manager_ui(f.manager, ascii("actual", units), 1.0f, 1.0f, &report, NULL) ==
              MC_SOUND_SOURCE_EXCEPTION &&
          report.exception == MC_SOUND_MANAGER_EXCEPTION_BACKEND_THROWABLE);
    ok = true;
cleanup:
    fixture_free(&f);
    free(trig);
    return ok;
}

static bool worker_failure_prefix(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_state state;
    mc_sound_backend_source_state source;
    mc_sound_backend_options options = mc_sound_backend_default_options();
    options.loopback = true;
    options.normal_channels = 2;
    options.streaming_channels = 1;
    options.max_cached_buffers = 1;
    CHECK(fixture_create_options(&f, options));
    const mc_sound_pool_entry *first = entry(&f, root, "first.ogg", "mono32k.ogg");
    const mc_sound_pool_entry *second = entry(&f, root, "second.ogg", "mono32k.ogg");
    CHECK(first != NULL && second != NULL);
    /* Enqueue one wrapper while the real command worker is awake. Requests
     * remain separate public calls, so only the failure's own prefix and the
     * first fault classification are asserted across allowed interleavings. */
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "first",
                                                 .entry = first}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "second",
                                                 .entry = second}));
    CHECK(mc_sound_backend_wait_idle(f.backend, 5000u, NULL) == MC_SOUND_BACKEND_LIMIT);
    bool stopped = false;
    for (unsigned i = 0; i < 2000u; ++i) {
        CHECK(mc_sound_backend_read_state(f.backend, &state));
        if (!state.command_worker_alive && !state.stream_worker_alive) {
            stopped = true;
            break;
        }
        const struct timespec delay = {0, 1000000L};
        thrd_sleep(&delay, NULL);
    }
    CHECK(stopped && state.worker_failed && state.worker_error.status == MC_SOUND_BACKEND_LIMIT &&
          !state.source_cleanup_reached && state.applied == 2 && state.current_sources == 1);
    int16_t output[2];
    mc_sound_backend_error error;
    CHECK(mc_sound_backend_render(f.backend, output, 1u, &error) ==
          MC_SOUND_BACKEND_INVALID_ARGUMENT);
    CHECK(mc_sound_backend_wait_idle(f.backend, 1u, &error) == MC_SOUND_BACKEND_LIMIT &&
          error.status == MC_SOUND_BACKEND_LIMIT);
    CHECK(read_source(&f, "first", &source) && source.present && source.stopped && !source.playing);
    CHECK(read_source(&f, "second", &source) && !source.present);
    CHECK(f.calls.retire(f.calls.context, &f.handle, NULL) == MC_SOUND_OK && f.handle == NULL);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.native_workers_joined &&
          !state.source_cleanup_reached &&
          state.device_buffers_created == state.device_buffers_deleted);
    ok = true;
cleanup:
    fixture_free(&f);
    return ok;
}

static bool drain_failure_prefix(const char *root) {
    bool ok = false;
    fixture f = {0};
    mc_sound_backend_state state;
    mc_sound_backend_source_state source;
    mc_sound_backend_options options = mc_sound_backend_default_options();
    options.loopback = options.manual_service = true;
    options.normal_channels = 2;
    options.streaming_channels = 1;
    options.max_cached_buffers = 1;
    CHECK(fixture_create_options(&f, options));
    const mc_sound_pool_entry *first = entry(&f, root, "first.ogg", "mono32k.ogg");
    const mc_sound_pool_entry *second = entry(&f, root, "second.ogg", "mono32k.ogg");
    CHECK(first != NULL && second != NULL);
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "first",
                                                 .entry = first}));
    CHECK(command(
        &f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "first"}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                 .source_name = "second",
                                                 .entry = second}));
    CHECK(command(&f, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_SET_VOLUME,
                                                 .source_name = "first",
                                                 .value = 0.25f}));
    CHECK(mc_sound_backend_service_commands(f.backend, NULL) == MC_SOUND_BACKEND_LIMIT);
    CHECK(mc_sound_backend_read_state(f.backend, &state) && state.worker_failed &&
          state.applied == 2 && state.queued_commands == 1 && state.deferred_plays == 1);
    CHECK(read_source(&f, "first", &source) && source.stopped && !source.playing &&
          source.volume == 1.0f);
    ok = true;
cleanup:
    fixture_free(&f);
    return ok;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3)
        return 1;
    if (!manual_normal(argv[1]) || !selection_and_listener(argv[1]) || !streaming(argv[1]) ||
        !wav_stream_refill(argv[1]) || !failed_stream_transfer(argv[1]) ||
        !displaced_sources(argv[1]) || !partial_cache_publication(argv[1]) ||
        !manager_integration(argv[1]) || !drain_failure_prefix(argv[1]) ||
        !worker_failure_prefix(argv[1]) ||
        !worker_profile(argv[1], argc == 3 && strcmp(argv[2], "--default-device") == 0))
        return 1;
    if (cleanup_failed)
        return 1;
    printf("Real OpenAL backend checks passed: %u\n", checks);
    return 0;
}
