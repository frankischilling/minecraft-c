#ifndef MC_CLIENT_SOUND_WAV_CODEC_H
#define MC_CLIENT_SOUND_WAV_CODEC_H

#include "mc/client/sound/sound_file.h"

typedef struct mc_wav_codec mc_wav_codec;

#define MC_WAV_CODEC_MAX_RESULT_BYTES 1073741824u
#define MC_WAV_CODEC_MAX_FRAME_BYTES 1048576u

typedef enum mc_wav_codec_status {
    MC_WAV_CODEC_OK = 0,
    MC_WAV_CODEC_INVALID_ARGUMENT,
    MC_WAV_CODEC_BUSY,
    MC_WAV_CODEC_LIMIT,
    MC_WAV_CODEC_ALLOCATION_FAILED,
    MC_WAV_CODEC_STREAM_FAILED,
    MC_WAV_CODEC_UNIMPLEMENTED_CONTAINER,
    MC_WAV_CODEC_SOURCE_EXCEPTION,
    MC_WAV_CODEC_FAILED_STATE
} mc_wav_codec_status;

typedef enum mc_wav_codec_encoding {
    MC_WAV_CODEC_PCM_SIGNED,
    MC_WAV_CODEC_PCM_UNSIGNED,
    MC_WAV_CODEC_PCM_FLOAT,
    MC_WAV_CODEC_ALAW,
    MC_WAV_CODEC_ULAW
} mc_wav_codec_encoding;

typedef enum mc_wav_codec_source_exception {
    MC_WAV_CODEC_EXCEPTION_NONE,
    MC_WAV_CODEC_EXCEPTION_NULL_URL,
    MC_WAV_CODEC_EXCEPTION_UNSUPPORTED_AUDIO,
    MC_WAV_CODEC_EXCEPTION_IO,
    MC_WAV_CODEC_EXCEPTION_NEGATIVE_ARRAY,
    MC_WAV_CODEC_EXCEPTION_NULL_CONVERSION_DATA,
    MC_WAV_CODEC_EXCEPTION_NULL_STREAM
} mc_wav_codec_source_exception;

typedef struct mc_wav_codec_limits {
    size_t input_bytes;  /* 1..MC_SOUND_STREAM_MAX_BYTES per actual file */
    size_t result_bytes; /* 1..MC_WAV_CODEC_MAX_RESULT_BYTES per result */
    uint32_t work_steps; /* 1..1048576 per public call */
} mc_wav_codec_limits;

/* WAVE provider format, not a promise of PCM16/OpenAL compatibility. Classic
 * PCM uses ceil(bits/8)*channels instead of the file's block-align field.
 * Float/extensible providers retain actual block alignment. Sample/frame rates
 * share their binary32 bits. Extensible metadata retains numeric properties;
 * no Java Map/String identity is represented. All admitted WAVE bytes are LE. */
typedef struct mc_wav_codec_format {
    bool present, big_endian, extensible;
    mc_wav_codec_encoding encoding;
    int32_t channels, sample_size_bits, frame_size;
    uint32_t sample_rate_bits, frame_rate_bits;
    int64_t frame_length;
    uint16_t valid_bits_per_sample;
    uint32_t channel_mask;
} mc_wav_codec_format;

typedef struct mc_wav_codec_buffer {
    bool present;
    uint8_t *data; /* owned; a present empty array has a nonnull allocation */
    size_t bytes;
    mc_wav_codec_format format;
} mc_wav_codec_buffer;

typedef struct mc_wav_codec_cleanup {
    bool source_close_called, native_retire_called;
    mc_sound_error source_close, native_retire;
} mc_wav_codec_cleanup;

typedef struct mc_wav_codec_error {
    mc_wav_codec_status status;
    const char *operation; /* static borrowed native text; no fabricated stack */
    int32_t source_pc;     /* -1 for native admission without a source site */
    mc_wav_codec_source_exception exception;
    bool source_caught;
    mc_sound_error stream;
    mc_wav_codec_cleanup prior_cleanup, cleanup;
} mc_wav_codec_error;

typedef struct mc_wav_codec_view {
    bool initialized, end_of_stream, stream_present, stream_closed, failed;
    mc_wav_codec_status terminal_status;
    mc_wav_codec_format format;
    int64_t frame_position;
    size_t source_bytes, data_bytes_read, buffered_bytes, pushback_bytes;
} mc_wav_codec_view;

/* Concrete CodecWav policy with the observed JDK17 WAVE providers: classic
 * PCM/ALAW/ULAW, IEEE float and extensible PCM/float. Bytes retain their source
 * encoding. CodecWav's generic AudioSystem call can also admit other containers;
 * this WAVE owner reports UNIMPLEMENTED_CONTAINER for their recognized magic.
 * No JVM, audio device, replacement I/O or decoder callback is used. */
mc_wav_codec_status mc_wav_codec_create(mc_wav_codec_limits limits, mc_wav_codec **out,
                                        mc_wav_codec_error *error);
/* Reinitialization first clears initialized and cleans old resources. NULL
 * identity follows the source's null URL failure and publishes false. Source
 * unsupported/IO initialization failures return native OK with initialized
 * false and a caught source diagnostic. Native bounds/allocation failures stay
 * distinct. Failed setup streams are always retired by native ownership. */
mc_wav_codec_status mc_wav_codec_initialize(mc_wav_codec *codec,
                                            const mc_sound_file_identity *identity,
                                            bool *initialized, mc_wav_codec_error *error);
/* streaming_size is the actual source configuration scalar. Zero returns no
 * result without changing EOS; a negative size reports the source array error.
 * Frame rounding can cause source EOS before the file ends. Read IO failure
 * sets EOS and discards that call's partial result. */
mc_wav_codec_status mc_wav_codec_read(mc_wav_codec *codec, int32_t streaming_size,
                                      mc_wav_codec_buffer *out, mc_wav_codec_error *error);
/* Positive Java-wrapped channels*(int)frameLength*bits/8 preallocates that full
 * length, reads the current suffix and keeps zero padding. It ignores EOS and
 * both configuration scalars, then closes the stream without changing EOS.
 * Only nonpositive wrapped size selects file_chunk_size/max_file_size loops.
 * Source nontermination is bounded by native work; null conversion and negative
 * arrays remain distinct source exceptions. The stream field/format survives
 * read_all's close and is cleared only by cleanup/reinitialization. */
mc_wav_codec_status mc_wav_codec_read_all(mc_wav_codec *codec, int32_t file_chunk_size,
                                          int32_t max_file_size, mc_wav_codec_buffer *out,
                                          mc_wav_codec_error *error);
bool mc_wav_codec_read_view(const mc_wav_codec *codec, mc_wav_codec_view *out);
/* Cleanup clears the stream and format but preserves initialized/EOS flags.
 * Results remain owned by the caller across all codec operations/retirement.
 * Zero-initialize outputs and release present results before reuse. */
mc_wav_codec_status mc_wav_codec_cleanup_resources(mc_wav_codec *codec, mc_wav_codec_error *error);
mc_wav_codec_status mc_wav_codec_free(mc_wav_codec **codec, mc_wav_codec_error *error);
void mc_wav_codec_buffer_free(mc_wav_codec_buffer *buffer);

/* Calls require exclusive access and disjoint valid outputs. Admission failures
 * preserve output. Native terminal failures permit state, cleanup and reinit;
 * source exception outcomes retain the receiver for a later source call. */

#endif
