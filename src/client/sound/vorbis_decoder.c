#include "mc/client/sound/vorbis_decoder.h"

#include <vorbis/vorbisfile.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(long) <= sizeof(int64_t), "Vorbis rate fits the public signed64 view");

struct mc_vorbis_decoder {
    mc_sound_stream *stream;
    OggVorbis_File file;
    mc_vorbis_decoder_state state;
    mc_sound_error callback_error;
    mc_sound_status callback_status;
    bool initialized;
    uint8_t pcm[MC_VORBIS_DECODER_CHUNK_BYTES];
};

static mc_sound_error no_stream_error(void) {
    return (mc_sound_error){.status = MC_SOUND_OK, .source_pc = -1};
}

static mc_vorbis_decoder_status result(mc_vorbis_error *error, mc_vorbis_decoder_status status,
                                       const char *operation, int code, mc_sound_error stream) {
    if (error != NULL)
        *error = (mc_vorbis_error){
            .status = status,
            .operation = operation,
            .vorbis_code = code,
            .stream = stream,
            .cleanup = {.source_close = no_stream_error(), .native_retire = no_stream_error()}};
    return status;
}

static size_t source_read(void *destination, size_t size, size_t count, void *context) {
    mc_vorbis_decoder *decoder = context;
    if (size == 0 || count == 0)
        return 0;
    if (decoder->callback_status != MC_SOUND_OK) {
        errno =
            decoder->callback_error.system_error != 0 ? decoder->callback_error.system_error : EIO;
        return 0;
    }
    if (count > SIZE_MAX / size) {
        decoder->callback_status = MC_SOUND_LIMIT;
        decoder->callback_error = (mc_sound_error){
            .status = MC_SOUND_LIMIT, .operation = "Vorbis callback read size", .source_pc = -1};
        errno = EOVERFLOW;
        return 0;
    }
    size_t actual = 0;
    mc_sound_error cause;
    const mc_sound_status status =
        mc_sound_stream_read(decoder->stream, destination, size * count, &actual, &cause);
    if (status != MC_SOUND_OK) {
        decoder->callback_status = status;
        decoder->callback_error = cause;
        errno = cause.system_error != 0 ? cause.system_error : EIO;
        return 0;
    }
    decoder->state.source_bytes += actual;
    return actual / size;
}

static mc_vorbis_decoder_status cleanup(mc_vorbis_decoder *decoder, mc_vorbis_retirement *out) {
    mc_vorbis_retirement report = {.source_close = no_stream_error(),
                                   .native_retire = no_stream_error()};
    mc_vorbis_decoder_status status = MC_VORBIS_DECODER_OK;
    if (decoder->initialized) {
        report.decoder_clear_called = true;
        report.vorbis_clear_code = ov_clear(&decoder->file);
        decoder->initialized = false;
        if (report.vorbis_clear_code != 0)
            status = MC_VORBIS_DECODER_DECODE_FAILED;
    }
    if (decoder->stream != NULL) {
        report.source_close_called = true;
        if (mc_sound_stream_close(decoder->stream, &report.source_close) != MC_SOUND_OK)
            status = MC_VORBIS_DECODER_STREAM_FAILED;
        report.native_retire_called = true;
        if (mc_sound_stream_free(decoder->stream, &report.native_retire) != MC_SOUND_OK)
            status = MC_VORBIS_DECODER_STREAM_FAILED;
        decoder->stream = NULL;
    }
    *out = report;
    return status;
}

static bool metadata(mc_vorbis_decoder *decoder, int link) {
    const vorbis_info *info = ov_info(&decoder->file, -1);
    if (info == NULL || info->channels <= 0 || info->channels > 255 || info->rate <= 0 ||
        link < 0 || link < decoder->state.logical_link)
        return false;
    decoder->state.logical_link = (int32_t)link;
    decoder->state.channels = (int32_t)info->channels;
    decoder->state.sample_rate = (int64_t)info->rate;
    return true;
}

mc_vorbis_decoder_status mc_vorbis_decoder_open_identity(const mc_sound_file_identity *identity,
                                                         bool mus, size_t input_budget,
                                                         size_t pcm_budget, mc_vorbis_decoder **out,
                                                         mc_vorbis_error *error) {
    if (identity == NULL || out == NULL || input_budget == 0 ||
        input_budget > MC_SOUND_STREAM_MAX_BYTES || pcm_budget == 0 ||
        pcm_budget > MC_VORBIS_DECODER_MAX_PCM_BYTES)
        return result(error, MC_VORBIS_DECODER_INVALID_ARGUMENT, "Vorbis decoder open admission", 0,
                      no_stream_error());
    mc_vorbis_decoder *decoder = calloc(1, sizeof(*decoder));
    if (decoder == NULL)
        return result(error, MC_VORBIS_DECODER_ALLOCATION_FAILED, "Vorbis decoder owner allocation",
                      0, no_stream_error());
    decoder->state.pcm_budget = pcm_budget;
    decoder->callback_error = no_stream_error();
    mc_sound_error cause;
    mc_vorbis_decoder_status status = MC_VORBIS_DECODER_OK;
    int code = 0;
    if (mc_sound_stream_open_identity(identity, mus, input_budget, &decoder->stream, &cause) !=
        MC_SOUND_OK) {
        status = MC_VORBIS_DECODER_STREAM_FAILED;
    } else {
        const ov_callbacks callbacks = {source_read, NULL, NULL, NULL};
        code = ov_open_callbacks(decoder, &decoder->file, NULL, 0, callbacks);
        decoder->initialized = code == 0;
        cause = decoder->callback_error;
        if (decoder->callback_status != MC_SOUND_OK)
            status = MC_VORBIS_DECODER_STREAM_FAILED;
        else if (code != 0)
            status = MC_VORBIS_DECODER_DECODE_FAILED;
        else if (!metadata(decoder, 0))
            status = MC_VORBIS_DECODER_FORMAT_FAILED;
    }
    if (status != MC_VORBIS_DECODER_OK) {
        mc_vorbis_retirement retired;
        (void)cleanup(decoder, &retired);
        free(decoder);
        result(error, status, "Vorbis decoder open", code, cause);
        if (error != NULL)
            error->cleanup = retired;
        return status;
    }
    *out = decoder;
    return result(error, MC_VORBIS_DECODER_OK, NULL, 0, no_stream_error());
}

mc_vorbis_decoder_status mc_vorbis_decoder_open(const mc_sound_file *file, bool mus,
                                                size_t input_budget, size_t pcm_budget,
                                                mc_vorbis_decoder **out, mc_vorbis_error *error) {
    mc_sound_file_identity identity;
    if (!mc_sound_file_read_identity(file, &identity))
        return result(error, MC_VORBIS_DECODER_INVALID_ARGUMENT, "Vorbis decoder file identity", 0,
                      no_stream_error());
    return mc_vorbis_decoder_open_identity(&identity, mus, input_budget, pcm_budget, out, error);
}

static mc_vorbis_chunk view(const mc_vorbis_decoder *decoder, size_t bytes) {
    return (mc_vorbis_chunk){.pcm_le = bytes == 0 ? NULL : decoder->pcm,
                             .bytes = bytes,
                             .total_bytes = decoder->state.total_bytes,
                             .source_bytes = decoder->state.source_bytes,
                             .logical_link = decoder->state.logical_link,
                             .channels = decoder->state.channels,
                             .sample_rate = decoder->state.sample_rate,
                             .eof = decoder->state.eof};
}

mc_vorbis_decoder_status mc_vorbis_decoder_read(mc_vorbis_decoder *decoder, mc_vorbis_chunk *out,
                                                mc_vorbis_error *error) {
    if (decoder == NULL || out == NULL)
        return result(error, MC_VORBIS_DECODER_INVALID_ARGUMENT, "Vorbis decoder read admission", 0,
                      no_stream_error());
    if (decoder->state.failed)
        return result(error, MC_VORBIS_DECODER_FAILED_STATE, "Vorbis decoder terminal state",
                      decoder->state.vorbis_code, decoder->callback_error);
    if (decoder->state.eof) {
        *out = view(decoder, 0);
        return result(error, MC_VORBIS_DECODER_OK, NULL, 0, no_stream_error());
    }
    mc_vorbis_decoder_status status = MC_VORBIS_DECODER_OK;
    const size_t remaining = decoder->state.pcm_budget - decoder->state.total_bytes;
    long bytes = 0;
    int link = -1;
    if (remaining < MC_VORBIS_DECODER_MAX_FRAME_BYTES) {
        status = MC_VORBIS_DECODER_LIMIT;
    } else {
        const size_t capacity = remaining < sizeof(decoder->pcm) ? remaining : sizeof(decoder->pcm);
        bytes = ov_read(&decoder->file, (char *)decoder->pcm, (int)capacity, 0, 2, 1, &link);
        if (bytes < 0) {
            decoder->state.vorbis_code = (int)bytes;
            status = MC_VORBIS_DECODER_DECODE_FAILED;
        } else if (bytes > 0) {
            if (!metadata(decoder, link) || (size_t)bytes > capacity ||
                (size_t)bytes % ((size_t)decoder->state.channels * 2u) != 0) {
                status = MC_VORBIS_DECODER_FORMAT_FAILED;
                bytes = 0;
            } else {
                decoder->state.total_bytes += (size_t)bytes;
            }
        } else {
            decoder->state.eof = true;
        }
        if (decoder->callback_status != MC_SOUND_OK) {
            status = MC_VORBIS_DECODER_STREAM_FAILED;
            decoder->state.eof = false;
        }
    }
    if (status != MC_VORBIS_DECODER_OK) {
        decoder->state.failed = true;
        decoder->state.terminal_status = status;
    }
    *out = view(decoder, bytes > 0 ? (size_t)bytes : 0);
    return result(error, status, status == MC_VORBIS_DECODER_OK ? NULL : "Vorbis decoder read",
                  decoder->state.vorbis_code, decoder->callback_error);
}

bool mc_vorbis_decoder_read_state(const mc_vorbis_decoder *decoder, mc_vorbis_decoder_state *out) {
    if (decoder == NULL || out == NULL)
        return false;
    *out = decoder->state;
    return true;
}

mc_vorbis_decoder_status mc_vorbis_decoder_retire(mc_vorbis_decoder **decoder,
                                                  mc_vorbis_retirement *out,
                                                  mc_vorbis_error *error) {
    if (decoder == NULL)
        return result(error, MC_VORBIS_DECODER_INVALID_ARGUMENT,
                      "Vorbis decoder retirement admission", 0, no_stream_error());
    mc_vorbis_retirement retired = {.source_close = no_stream_error(),
                                    .native_retire = no_stream_error()};
    mc_vorbis_decoder_status status = MC_VORBIS_DECODER_OK;
    if (*decoder != NULL) {
        status = cleanup(*decoder, &retired);
        free(*decoder);
        *decoder = NULL;
    }
    if (out != NULL)
        *out = retired;
    const mc_sound_error cause =
        retired.source_close.status != MC_SOUND_OK ? retired.source_close : retired.native_retire;
    result(error, status, status == MC_VORBIS_DECODER_OK ? NULL : "Vorbis decoder retirement",
           retired.vorbis_clear_code, cause);
    if (error != NULL)
        error->cleanup = retired;
    return status;
}
