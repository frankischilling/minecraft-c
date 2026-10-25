#ifndef MC_CLIENT_SOUND_VORBIS_DECODER_H
#define MC_CLIENT_SOUND_VORBIS_DECODER_H

#include "mc/client/sound/sound_file.h"

typedef struct mc_vorbis_decoder mc_vorbis_decoder;

#define MC_VORBIS_DECODER_MAX_PCM_BYTES 1073741824u
#define MC_VORBIS_DECODER_CHUNK_BYTES 65536u
#define MC_VORBIS_DECODER_MAX_FRAME_BYTES 510u

typedef enum mc_vorbis_decoder_status {
    MC_VORBIS_DECODER_OK = 0,
    MC_VORBIS_DECODER_INVALID_ARGUMENT,
    MC_VORBIS_DECODER_LIMIT,
    MC_VORBIS_DECODER_ALLOCATION_FAILED,
    MC_VORBIS_DECODER_STREAM_FAILED,
    MC_VORBIS_DECODER_DECODE_FAILED,
    MC_VORBIS_DECODER_FORMAT_FAILED,
    MC_VORBIS_DECODER_FAILED_STATE
} mc_vorbis_decoder_status;

typedef struct mc_vorbis_retirement {
    bool decoder_clear_called, source_close_called, native_retire_called;
    int vorbis_clear_code;
    mc_sound_error source_close, native_retire;
} mc_vorbis_retirement;

typedef struct mc_vorbis_error {
    mc_vorbis_decoder_status status;
    const char *operation; /* static borrowed text; no invented Java exception */
    int vorbis_code;       /* actual library return, or 0 for a native failure */
    mc_sound_error stream;
    mc_vorbis_retirement cleanup; /* open-failure or explicit retirement work */
} mc_vorbis_error;

typedef struct mc_vorbis_chunk {
    const uint8_t *pcm_le; /* borrowed until the next read or retirement */
    size_t bytes, total_bytes, source_bytes;
    int32_t logical_link, channels;
    int64_t sample_rate;
    bool eof;
} mc_vorbis_chunk;

typedef struct mc_vorbis_decoder_state {
    size_t total_bytes, source_bytes, pcm_budget;
    int32_t logical_link, channels;
    int64_t sample_rate;
    bool eof, failed;
    mc_vorbis_decoder_status terminal_status;
    int vorbis_code;
} mc_vorbis_decoder_state;

/* Open one real sequential stream with seek/tell/close callbacks NULL. The
 * identity route preserves a pool entry's retained URL.path, including MUS
 * basename escapes; it does not re-resolve the current filesystem identity.
 * The file convenience route reads its identity then calls that same route.
 * The identity/file owner may be freed after return. Input budget is 1..256 MiB;
 * PCM prefix budget is 1..1 GiB. Invalid/native open failure preserves out.
 * Library initialization failure is reported with its actual negative code;
 * any opened stream is source-closed and retired before that failure returns.
 * The library allocates internally; no internal OOM injection is promised. */
mc_vorbis_decoder_status mc_vorbis_decoder_open_identity(const mc_sound_file_identity *identity,
                                                         bool mus, size_t input_budget,
                                                         size_t pcm_budget, mc_vorbis_decoder **out,
                                                         mc_vorbis_error *error);
mc_vorbis_decoder_status mc_vorbis_decoder_open(const mc_sound_file *file, bool mus,
                                                size_t input_budget, size_t pcm_budget,
                                                mc_vorbis_decoder **out, mc_vorbis_error *error);

/* At most one ov_read call; no hole retry or progress loop. Positive chunks
 * are interleaved signed 16-bit little-endian bytes with their actual current link,
 * channel count and rate. Every changed link rechecks metadata before delivery.
 * EOF publishes a clear view (NULL/0), with eof=true and retained format/totals.
 * source_bytes counts bytes returned by the real stream to callbacks, not
 * internal stdio buffering or its input-limit probe.
 *
 * PCM budget is a native prefix ceiling. If fewer than 510 bytes remain, this
 * call fails LIMIT without probing for EOF or entering the library: 510 is one
 * maximal 255-channel signed 16-bit frame. It need not fill every remaining byte.
 * Any decode/native failure is terminal; passive state and retirement remain
 * available. A reached read publishes its view even on failure; a positive
 * view is still a valid prefix if an I/O error was latched in that call.
 * Admission/FAILED_STATE preserves out. All calls require exclusive access. */
mc_vorbis_decoder_status mc_vorbis_decoder_read(mc_vorbis_decoder *decoder, mc_vorbis_chunk *out,
                                                mc_vorbis_error *error);
bool mc_vorbis_decoder_read_state(const mc_vorbis_decoder *decoder, mc_vorbis_decoder_state *out);

/* Clear a successfully initialized decoder once, then source-close once and
 * always retire the actual native stream. MUS source close remains its no-op;
 * native retirement still closes the handle. Failures retain all cleanup
 * outcomes and consume/null the owner. NULL *decoder succeeds with no work.
 * Outputs must not alias the owner; a NULL owner-pointer is invalid. */
mc_vorbis_decoder_status mc_vorbis_decoder_retire(mc_vorbis_decoder **decoder,
                                                  mc_vorbis_retirement *out,
                                                  mc_vorbis_error *error);

#endif
