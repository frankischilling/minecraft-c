#ifndef MC_CLIENT_SOUND_VORBIS_CODEC_H
#define MC_CLIENT_SOUND_VORBIS_CODEC_H

#include "mc/client/sound/sound_file.h"

typedef struct mc_vorbis_codec mc_vorbis_codec;

#define MC_VORBIS_CODEC_MAX_PCM_BYTES 1073741824u
#define MC_VORBIS_CODEC_MAX_SAVED_PCM_BYTES 16777216u

typedef enum mc_vorbis_codec_status {
    MC_VORBIS_CODEC_OK = 0,
    MC_VORBIS_CODEC_INVALID_ARGUMENT,
    MC_VORBIS_CODEC_BUSY,
    MC_VORBIS_CODEC_LIMIT,
    MC_VORBIS_CODEC_ALLOCATION_FAILED,
    MC_VORBIS_CODEC_STREAM_FAILED,
    MC_VORBIS_CODEC_HEADER_FAILED,
    MC_VORBIS_CODEC_SOURCE_STATE,
    MC_VORBIS_CODEC_FAILED_STATE
} mc_vorbis_codec_status;

typedef struct mc_vorbis_codec_limits {
    size_t input_bytes;  /* 1..MC_SOUND_STREAM_MAX_BYTES per opened stream */
    size_t pcm_bytes;    /* 1..1 GiB per reached read/read_all result */
    uint32_t work_steps; /* 1..1048576 primitive/loop steps per public call */
} mc_vorbis_codec_limits;

typedef struct mc_vorbis_codec_format {
    bool present;
    int32_t channels;
    int64_t sample_rate;
    uint32_t sample_rate_bits; /* AudioFormat's binary32 integer-to-float result */
} mc_vorbis_codec_format;

/* Zero-initialize before use; release a present result before reuse. An absent
 * result owns nothing and may be reused. On OK, present=false is
 * read()'s absent SoundBuffer, while present=true/data_is_null=true represents
 * read_all()'s nonnull SoundBuffer with null data. Nonnull empty data has bytes=0
 * and a distinct owned allocation. Format is a value snapshot, not JVM identity.
 * Returned allocations survive further calls, cleanup and codec retirement.
 * A reached native failure may publish an owned partial result; status!=OK never
 * claims that the original call returned that partial result successfully. */
typedef struct mc_vorbis_codec_buffer {
    bool present, data_is_null;
    uint8_t *data;
    size_t bytes;
    mc_vorbis_codec_format format;
} mc_vorbis_codec_buffer;

typedef struct mc_vorbis_codec_cleanup {
    bool stream_clear, block_clear, dsp_clear, info_clear, comment_clear, sync_clear;
    int stream_clear_code, block_clear_code, sync_clear_code;
    bool source_close_called, native_retire_called;
    mc_sound_error source_close, native_retire;
} mc_vorbis_codec_cleanup;

typedef struct mc_vorbis_codec_error {
    mc_vorbis_codec_status status;
    const char *operation; /* static native diagnostic; not a Java Throwable */
    int library_code;
    mc_sound_error stream;
    bool source_refill_caught;
    mc_sound_error caught_stream;
    mc_vorbis_codec_cleanup prior_cleanup;
    mc_vorbis_codec_cleanup cleanup;
} mc_vorbis_codec_error;

typedef struct mc_vorbis_codec_view {
    bool initialized, end_of_stream, resources_present, failed;
    mc_vorbis_codec_status terminal_status;
    mc_vorbis_codec_format format;
    int32_t buffer_size, converted_buffer_size, count, index;
    size_t source_bytes, pages, packets, converted_bytes, discarded_full_buffer_bytes;
    size_t avoided_negative_sync_states;
    size_t first_granule_transitions;
    size_t saved_pcm_frames, saved_pcm_peak_frames, saved_pcm_frames_copied;
    size_t saved_pcm_end_trimmed_frames;
    int64_t granule_position;
    int last_page_code, last_pagein_code, last_packet_code, last_header_code;
    int last_synthesis_code, last_blockin_code, last_pcm_read_code, last_wrote_code;
} mc_vorbis_codec_view;

/* One concrete CodecJOrbis policy owner, using low Ogg/Vorbis synthesis. There
 * is no libvorbisfile delegation or device. Project allocations and native
 * work/prefix limits are checked; internal library OOM is not fault-injected.
 * Unread synthesis is retained as exact interleaved binary32 samples, up to
 * MC_VORBIS_CODEC_MAX_SAVED_PCM_BYTES, before the next block enters libvorbis.
 * This preserves JOrbis's pending PCM when a small conversion buffer fills.
 * Saved state is separate from the returned PCM byte budget. Replacing that
 * allocation can temporarily require twice its storage bound. */
mc_vorbis_codec_status mc_vorbis_codec_create(mc_vorbis_codec_limits limits, mc_vorbis_codec **out,
                                              mc_vorbis_codec_error *error);
/* Reinitialize this receiver. Prior cleanup outcomes are retained even though
 * the source initializer catches close failures and continues. streaming_size
 * is the actual SoundSystemConfig scalar; division by two occurs internally.
 * Native guards reject nonpositive/oversized buffers after reached resets.
 * A false header result leaves the partial stream/resources available for
 * cleanup, rather than rolling back initialization. */
mc_vorbis_codec_status mc_vorbis_codec_initialize(mc_vorbis_codec *codec,
                                                  const mc_sound_file_identity *identity, bool mus,
                                                  int32_t streaming_size, bool *initialized,
                                                  mc_vorbis_codec_error *error);
mc_vorbis_codec_status mc_vorbis_codec_read(mc_vorbis_codec *codec, mc_vorbis_codec_buffer *out,
                                            mc_vorbis_codec_error *error);
/* max_file_size is the actual source scalar. Its test follows the first
 * additional page append, not the initial read. Native limits do not silently
 * replace this source setting. */
mc_vorbis_codec_status mc_vorbis_codec_read_all(mc_vorbis_codec *codec, int32_t max_file_size,
                                                mc_vorbis_codec_buffer *out,
                                                mc_vorbis_codec_error *error);
bool mc_vorbis_codec_read_view(const mc_vorbis_codec *codec, mc_vorbis_codec_view *out);
/* Clears source codec resources and source-closes the stream; additionally
 * retires the native handle, including a MUS facade. Retains initialized/EOS,
 * count/index and format. Does not invalidate previously returned buffers. */
mc_vorbis_codec_status mc_vorbis_codec_cleanup_resources(mc_vorbis_codec *codec,
                                                         mc_vorbis_codec_error *error);
mc_vorbis_codec_status mc_vorbis_codec_free(mc_vorbis_codec **codec, mc_vorbis_codec_error *error);
void mc_vorbis_codec_buffer_free(mc_vorbis_codec_buffer *buffer);

/* Calls require exclusive access and disjoint, valid typed output storage.
 * Invalid argument/BUSY/FAILED_STATE admission
 * preserves caller outputs. Reached initialization/read failures retain their
 * state prefix; reinitialization can reset a failed native receiver. */

#endif
