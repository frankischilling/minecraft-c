#include "internal.h"

#include "mc/core/java/java.h"

#include <stdlib.h>
#include <string.h>

static bool extension(mc_sound_text name, const char *suffix) {
    const size_t length = strlen(suffix);
    if (name.is_null || name.length < length)
        return false;
    for (size_t i = 0; i < length; ++i) {
        uint16_t unit = name.units[name.length - length + i];
        if (unit >= 'A' && unit <= 'Z')
            unit = (uint16_t)(unit + ('a' - 'A'));
        if (unit != (uint8_t)suffix[i])
            return false;
    }
    return true;
}

static mc_sound_backend_status vorbis_result(mc_sound_backend *b, mc_vorbis_codec_status status,
                                             const char *operation,
                                             const mc_vorbis_codec_error *error) {
    if (status == MC_VORBIS_CODEC_OK) {
        if (b->state.last_error.status == MC_SOUND_BACKEND_OK &&
            (error->source_refill_caught ||
             error->prior_cleanup.source_close.status != MC_SOUND_OK ||
             error->prior_cleanup.native_retire.status != MC_SOUND_OK ||
             error->cleanup.source_close.status != MC_SOUND_OK ||
             error->cleanup.native_retire.status != MC_SOUND_OK))
            b->state.last_error.vorbis = *error;
        return MC_SOUND_BACKEND_OK;
    }
    const mc_sound_backend_status result = backend_fail(
        b,
        status == MC_VORBIS_CODEC_LIMIT               ? MC_SOUND_BACKEND_LIMIT
        : status == MC_VORBIS_CODEC_ALLOCATION_FAILED ? MC_SOUND_BACKEND_ALLOCATION_FAILED
                                                      : MC_SOUND_BACKEND_CODEC_FAILED,
        operation, (int)status);
    b->state.last_error.vorbis = *error;
    return result;
}

static mc_sound_backend_status wav_result(mc_sound_backend *b, mc_wav_codec_status status,
                                          const char *operation, const mc_wav_codec_error *error) {
    if (status == MC_WAV_CODEC_OK) {
        if (b->state.last_error.status == MC_SOUND_BACKEND_OK &&
            (error->exception != MC_WAV_CODEC_EXCEPTION_NONE ||
             error->prior_cleanup.source_close.status != MC_SOUND_OK ||
             error->prior_cleanup.native_retire.status != MC_SOUND_OK ||
             error->cleanup.source_close.status != MC_SOUND_OK ||
             error->cleanup.native_retire.status != MC_SOUND_OK))
            b->state.last_error.wav = *error;
        return MC_SOUND_BACKEND_OK;
    }
    const mc_sound_backend_status result =
        backend_fail(b,
                     status == MC_WAV_CODEC_LIMIT               ? MC_SOUND_BACKEND_LIMIT
                     : status == MC_WAV_CODEC_ALLOCATION_FAILED ? MC_SOUND_BACKEND_ALLOCATION_FAILED
                     : status == MC_WAV_CODEC_SOURCE_EXCEPTION  ? MC_SOUND_BACKEND_SOURCE_EXCEPTION
                                                                : MC_SOUND_BACKEND_CODEC_FAILED,
                     operation, (int)status);
    b->state.last_error.wav = *error;
    return result;
}

mc_sound_backend_status backend_codec_create(mc_sound_backend *b, const backend_resource *resource,
                                             backend_codec *codec) {
    *codec = (backend_codec){0};
    if (extension(resource->logical, ".ogg") && b->configured[MC_SOUND_MANAGER_CODEC_OGG])
        codec->kind = BACKEND_CODEC_OGG;
    else if (extension(resource->logical, ".mus") && b->configured[MC_SOUND_MANAGER_CODEC_MUS])
        codec->kind = BACKEND_CODEC_MUS;
    else if (extension(resource->logical, ".wav") && b->configured[MC_SOUND_MANAGER_CODEC_WAV])
        codec->kind = BACKEND_CODEC_WAV;
    else
        return MC_SOUND_BACKEND_OK; /* actual getCodec miss, not a PCM substitute */
    if (codec->kind == BACKEND_CODEC_WAV) {
        const mc_wav_codec_limits limits = {b->options.input_bytes, b->options.pcm_bytes, 1048576u};
        mc_wav_codec_error error;
        const mc_wav_codec_status status = mc_wav_codec_create(limits, &codec->wav, &error);
        return wav_result(b, status, "WAV owner construction", &error);
    }
    const mc_vorbis_codec_limits limits = {b->options.input_bytes, b->options.pcm_bytes, 1048576u};
    mc_vorbis_codec_error error;
    const mc_vorbis_codec_status status = mc_vorbis_codec_create(limits, &codec->vorbis, &error);
    return vorbis_result(b, status, "Vorbis owner construction", &error);
}

static void update_format(backend_codec *codec) {
    if (codec->kind == BACKEND_CODEC_WAV) {
        mc_wav_codec_view view;
        if (mc_wav_codec_read_view(codec->wav, &view)) {
            codec->eof = view.end_of_stream;
            codec->format_present = view.format.present;
            codec->channels = view.format.channels;
            codec->bits = view.format.sample_size_bits;
            float rate;
            memcpy(&rate, &view.format.sample_rate_bits, sizeof(rate));
            codec->rate = mc_java_float_to_i32(rate);
        }
    } else if (codec->vorbis != NULL) {
        mc_vorbis_codec_view view;
        if (mc_vorbis_codec_read_view(codec->vorbis, &view)) {
            codec->eof = view.end_of_stream;
            codec->format_present = view.format.present;
            codec->channels = view.format.channels;
            codec->bits = 16;
            float rate;
            memcpy(&rate, &view.format.sample_rate_bits, sizeof(rate));
            codec->rate = mc_java_float_to_i32(rate);
        }
    }
}

mc_sound_backend_status backend_codec_initialize(mc_sound_backend *b, backend_codec *codec,
                                                 const backend_resource *resource) {
    bool initialized = false;
    mc_sound_backend_status status;
    if (codec->kind == BACKEND_CODEC_NONE)
        return MC_SOUND_BACKEND_OK;
    if (codec->kind == BACKEND_CODEC_WAV) {
        mc_wav_codec_error error;
        const mc_wav_codec_status result =
            mc_wav_codec_initialize(codec->wav, &resource->identity, &initialized, &error);
        status = wav_result(b, result, "WAV initialization", &error);
    } else {
        mc_vorbis_codec_error error;
        const mc_vorbis_codec_status result = mc_vorbis_codec_initialize(
            codec->vorbis, &resource->identity, codec->kind == BACKEND_CODEC_MUS,
            b->options.streaming_buffer_size, &initialized, &error);
        status = vorbis_result(b, result, "Vorbis initialization", &error);
    }
    update_format(codec);
    return status;
}

mc_sound_backend_status backend_codec_read(mc_sound_backend *b, backend_codec *codec, bool all,
                                           backend_pcm *out) {
    *out = (backend_pcm){.data_is_null = true};
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    if (codec->kind == BACKEND_CODEC_NONE)
        return status;
    if (codec->kind == BACKEND_CODEC_WAV) {
        mc_wav_codec_buffer buffer = {0};
        mc_wav_codec_error error;
        const mc_wav_codec_status result =
            all ? mc_wav_codec_read_all(codec->wav, b->options.file_chunk_size,
                                        b->options.max_file_size, &buffer, &error)
                : mc_wav_codec_read(codec->wav, b->options.streaming_buffer_size, &buffer, &error);
        status = wav_result(b, result, all ? "WAV readAll" : "WAV stream read", &error);
        out->present = buffer.present;
        out->data_is_null = buffer.data == NULL;
        out->data = buffer.data;
        out->bytes = buffer.bytes;
        buffer.data = NULL;
        mc_wav_codec_buffer_free(&buffer);
    } else {
        mc_vorbis_codec_buffer buffer = {0};
        mc_vorbis_codec_error error;
        const mc_vorbis_codec_status result =
            all ? mc_vorbis_codec_read_all(codec->vorbis, b->options.max_file_size, &buffer, &error)
                : mc_vorbis_codec_read(codec->vorbis, &buffer, &error);
        status = vorbis_result(b, result, all ? "Vorbis readAll" : "Vorbis stream read", &error);
        out->present = buffer.present;
        out->data_is_null = buffer.data_is_null;
        out->data = buffer.data;
        out->bytes = buffer.bytes;
        buffer.data = NULL;
        mc_vorbis_codec_buffer_free(&buffer);
    }
    update_format(codec);
    out->channels = codec->channels;
    out->bits = codec->bits;
    out->rate = codec->rate;
    return status;
}

void backend_pcm_free(backend_pcm *pcm) {
    free(pcm->data);
    *pcm = (backend_pcm){0};
}

mc_sound_backend_status backend_codec_free(mc_sound_backend *b, backend_codec *codec) {
    mc_sound_backend_status status = MC_SOUND_BACKEND_OK;
    if (codec->wav != NULL) {
        mc_wav_codec_error error;
        const mc_wav_codec_status result = mc_wav_codec_free(&codec->wav, &error);
        status = wav_result(b, result, "WAV native retirement", &error);
    }
    if (codec->vorbis != NULL) {
        mc_vorbis_codec_error error;
        const mc_vorbis_codec_status result = mc_vorbis_codec_free(&codec->vorbis, &error);
        status = vorbis_result(b, result, "Vorbis native retirement", &error);
    }
    *codec = (backend_codec){0};
    return status;
}
