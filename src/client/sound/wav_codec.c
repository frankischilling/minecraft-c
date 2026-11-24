#include "mc/client/sound/wav_codec.h"

#include "mc/core/java/java.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(__x86_64__)
#error "This WAVE codec profile requires little-endian x86-64"
#endif

#define INPUT_BUFFER_BYTES 8192u
#define MAX_WORK_STEPS 1048576u

struct mc_wav_codec {
    mc_wav_codec_limits limits;
    mc_wav_codec_view view;
    mc_sound_stream *source;
    uint8_t input[INPUT_BUFFER_BYTES];
    size_t input_at, input_count, offset;
    uint8_t *pushback;
    size_t pushback_count;
    uint64_t container_remaining;
    bool container_bounded, active, mark_valid;
    uint32_t work;
};

static mc_sound_error no_stream_error(void) {
    return (mc_sound_error){.status = MC_SOUND_OK, .source_pc = -1};
}

static mc_wav_codec_error no_error(void) {
    mc_wav_codec_error error = {.status = MC_WAV_CODEC_OK, .source_pc = -1};
    error.stream = no_stream_error();
    error.prior_cleanup.source_close = error.prior_cleanup.native_retire = no_stream_error();
    error.cleanup.source_close = error.cleanup.native_retire = no_stream_error();
    return error;
}

static mc_wav_codec_status admission(mc_wav_codec_error *out, mc_wav_codec_status status) {
    if (out != NULL) {
        *out = no_error();
        out->status = status;
        out->operation = "WAVE codec admission";
    }
    return status;
}

static bool fail(mc_wav_codec_error *error, mc_wav_codec_status status, const char *operation) {
    if (error->status == MC_WAV_CODEC_OK) {
        error->status = status;
        error->operation = operation;
    }
    return false;
}

static void source_event(mc_wav_codec_error *error, mc_wav_codec_source_exception exception,
                         bool caught, int32_t pc) {
    error->exception = exception;
    error->source_caught = caught;
    error->source_pc = pc;
}

static bool tick(mc_wav_codec *codec, mc_wav_codec_error *error) {
    if (codec->work >= codec->limits.work_steps)
        return fail(error, MC_WAV_CODEC_LIMIT, "WAVE codec work budget");
    ++codec->work;
    return true;
}

static mc_wav_codec_status finish(mc_wav_codec *codec, const mc_wav_codec_error *error,
                                  mc_wav_codec_error *out) {
    if (error->status != MC_WAV_CODEC_OK && error->status != MC_WAV_CODEC_SOURCE_EXCEPTION) {
        if (!codec->view.failed)
            codec->view.terminal_status = error->status;
        codec->view.failed = true;
    }
    codec->active = false;
    if (out != NULL)
        *out = *error;
    return error->status;
}

static mc_wav_codec_status map_stream(mc_sound_status status) {
    return status == MC_SOUND_LIMIT               ? MC_WAV_CODEC_LIMIT
           : status == MC_SOUND_ALLOCATION_FAILED ? MC_WAV_CODEC_ALLOCATION_FAILED
                                                  : MC_WAV_CODEC_STREAM_FAILED;
}

static bool buffered_read(mc_wav_codec *codec, uint8_t *out, size_t capacity, size_t *actual,
                          mc_wav_codec_error *error) {
    *actual = 0;
    if (!tick(codec, error))
        return false;
    if (codec->view.stream_closed) {
        error->stream = (mc_sound_error){.status = MC_SOUND_IO_FAILED,
                                         .operation = "WAVE buffered stream closed",
                                         .system_error = EBADF,
                                         .source_pc = -1};
        return false;
    }
    while (*actual < capacity) {
        if (!tick(codec, error))
            return false;
        if (codec->input_at == codec->input_count) {
            if (codec->input_at >= 200u)
                codec->mark_valid = false;
            codec->input_at = codec->input_count = 0;
            if (!codec->mark_valid && capacity - *actual >= sizeof(codec->input)) {
                size_t count = 0;
                const mc_sound_status direct_status = mc_sound_stream_read(
                    codec->source, out + *actual, capacity - *actual, &count, &error->stream);
                if (direct_status != MC_SOUND_OK) {
                    if (direct_status != MC_SOUND_IO_FAILED)
                        (void)fail(error, map_stream(direct_status), "WAVE direct source input");
                    return false;
                }
                codec->view.source_bytes += count;
                codec->offset += count;
                *actual += count;
                if (count == 0)
                    break;
                continue;
            }
            const mc_sound_status status =
                mc_sound_stream_read(codec->source, codec->input, sizeof(codec->input),
                                     &codec->input_count, &error->stream);
            if (status != MC_SOUND_OK) {
                if (status != MC_SOUND_IO_FAILED)
                    (void)fail(error, map_stream(status), "WAVE source input");
                return false;
            }
            codec->view.source_bytes += codec->input_count;
            if (codec->input_count == 0)
                break;
        }
        size_t count = codec->input_count - codec->input_at;
        if (count > capacity - *actual)
            count = capacity - *actual;
        memcpy(out + *actual, codec->input + codec->input_at, count);
        codec->input_at += count;
        codec->offset += count;
        *actual += count;
    }
    return true;
}

static bool header_read(mc_wav_codec *codec, uint8_t *out, size_t size, mc_wav_codec_error *error) {
    size_t actual = 0;
    if (!buffered_read(codec, out, size, &actual, error))
        return false;
    return actual == size;
}

static bool header_skip(mc_wav_codec *codec, uint64_t size, mc_wav_codec_error *error) {
    uint8_t scratch[1024];
    while (size != 0) {
        if (codec->input_at != codec->input_count) {
            size_t count = codec->input_count - codec->input_at;
            if (count > size)
                count = (size_t)size;
            if (!tick(codec, error))
                return false;
            codec->input_at += count;
            codec->offset += count;
            size -= count;
        } else if (!codec->mark_valid) {
            const size_t count = size < sizeof(scratch) ? (size_t)size : sizeof(scratch);
            if (!tick(codec, error))
                return false;
            size_t actual = 0;
            const mc_sound_status status =
                mc_sound_stream_read(codec->source, scratch, count, &actual, &error->stream);
            codec->view.source_bytes += actual;
            codec->offset += actual;
            if (status != MC_SOUND_OK) {
                if (status != MC_SOUND_IO_FAILED)
                    (void)fail(error, map_stream(status), "WAVE skipped source input");
                return false;
            }
            if (actual != count)
                return false;
            size -= count;
        } else {
            /* BufferedInputStream.skip fills once while a mark is retained.
             * Once invalidated, later skips do not prefetch past the skip. */
            if (!header_read(codec, scratch, 1, error))
                return false;
            --size;
        }
    }
    return true;
}

static uint16_t u16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8u));
}

static uint32_t u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8u | (uint32_t)bytes[2] << 16u |
           (uint32_t)bytes[3] << 24u;
}

static uint32_t rate_bits(uint32_t rate, bool classic) {
    const int64_t integer = classic ? (int64_t)mc_java_i32_narrow((int64_t)rate) : (int64_t)rate;
    volatile float rounded = (float)integer;
    const float stored = rounded;
    uint32_t bits;
    _Static_assert(sizeof(bits) == sizeof(stored), "WAVE format requires binary32 rates");
    memcpy(&bits, &stored, sizeof(bits));
    return bits;
}

static bool format_chunk(mc_wav_codec *codec, uint32_t size, mc_wav_codec_format *format,
                         bool *classic, bool prior_data, mc_wav_codec_error *error) {
    uint8_t fixed[16];
    /* The first JDK WAVE providers reject a foreign tag then reset their mark.
     * Reading that tag after the original 8192-byte buffer invalidates the mark. */
    if (!prior_data && codec->offset > INPUT_BUFFER_BYTES - 2u) {
        source_event(error, MC_WAV_CODEC_EXCEPTION_IO, true, 64);
        return false;
    }
    if (size < sizeof(fixed) || !header_read(codec, fixed, sizeof(fixed), error))
        return false;
    const uint16_t tag = u16(fixed);
    *classic = tag == 1u || tag == 6u || tag == 7u;
    format->channels =
        *classic ? (int32_t)mc_java_i16_narrow(u16(fixed + 2)) : (int32_t)u16(fixed + 2);
    format->sample_size_bits =
        *classic ? (int32_t)mc_java_i16_narrow(u16(fixed + 14)) : (int32_t)u16(fixed + 14);
    format->sample_rate_bits = format->frame_rate_bits = rate_bits(u32(fixed + 4), *classic);
    if (*classic) {
        if (format->channels <= 0 || format->sample_size_bits <= 0)
            return false;
        format->frame_size = ((format->sample_size_bits + 7) / 8) * format->channels;
        format->encoding = tag == 6u                       ? MC_WAV_CODEC_ALAW
                           : tag == 7u                     ? MC_WAV_CODEC_ULAW
                           : format->sample_size_bits == 8 ? MC_WAV_CODEC_PCM_UNSIGNED
                                                           : MC_WAV_CODEC_PCM_SIGNED;
    } else if (tag == 3u || tag == 65534u) {
        format->frame_size = (int32_t)u16(fixed + 12);
        if (format->frame_size == 0)
            return false;
        format->encoding = MC_WAV_CODEC_PCM_FLOAT;
        if (tag == 65534u) {
            uint8_t extension[24];
            if (size < 40u || !header_read(codec, extension, sizeof(extension), error))
                return false;
            static const uint8_t guid_tail[12] = {0, 0, 16, 0, 128, 0, 0, 170, 0, 56, 155, 113};
            const uint32_t subtype = u32(extension + 8);
            if (u16(extension) != 22u || u16(extension + 2) > (uint32_t)format->sample_size_bits ||
                memcmp(extension + 12, guid_tail, sizeof(guid_tail)) != 0 ||
                (subtype != 1u && subtype != 3u))
                return false;
            format->extensible = true;
            format->valid_bits_per_sample = u16(extension + 2);
            format->channel_mask = u32(extension + 4);
            if (subtype == 1u)
                format->encoding = format->sample_size_bits == 8 ? MC_WAV_CODEC_PCM_UNSIGNED
                                                                 : MC_WAV_CODEC_PCM_SIGNED;
            if (!header_skip(codec, size - 40u, error))
                return false;
        } else if (!header_skip(codec, size - 16u, error)) {
            return false;
        }
        if ((size & 1u) != 0 && !header_skip(codec, 1, error))
            return false;
    } else {
        return false;
    }
    if (format->frame_size <= 0 || (uint32_t)format->frame_size > MC_WAV_CODEC_MAX_FRAME_BYTES)
        return fail(error, MC_WAV_CODEC_LIMIT, "WAVE frame-size bound");
    /* Classic WaveFileReader skips its fmt extension but not an odd fmt pad. */
    return !*classic || header_skip(codec, size - 16u, error);
}

static bool parse_wave(mc_wav_codec *codec, mc_wav_codec_error *error) {
    uint8_t riff[12];
    if (!header_read(codec, riff, sizeof(riff), error))
        return false;
    if (memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        if (memcmp(riff, "FORM", 4) == 0 || memcmp(riff, ".snd", 4) == 0 ||
            memcmp(riff, "MThd", 4) == 0)
            return fail(error, MC_WAV_CODEC_UNIMPLEMENTED_CONTAINER,
                        "CodecWav non-WAVE AudioSystem container");
        return false;
    }
    const uint64_t riff_end = (uint64_t)u32(riff + 4) + 8u;
    bool have_format = false, classic = false, prior_data = false;
    mc_wav_codec_format format = {0};
    for (;;) {
        uint8_t chunk[8];
        if (!header_read(codec, chunk, sizeof(chunk), error))
            return false;
        const uint32_t size = u32(chunk + 4);
        if (!have_format && memcmp(chunk, "fmt ", 4) == 0) {
            if (!format_chunk(codec, size, &format, &classic, prior_data, error))
                return false;
            if (!classic && prior_data)
                return false;
            have_format = true;
        } else if (have_format && memcmp(chunk, "data", 4) == 0) {
            if (!classic) {
                if ((uint64_t)codec->offset > riff_end)
                    return false;
                if (codec->offset > INPUT_BUFFER_BYTES) {
                    source_event(error, MC_WAV_CODEC_EXCEPTION_IO, true, 64);
                    return false;
                }
                codec->container_bounded = true;
                codec->container_remaining = riff_end - (uint64_t)codec->offset;
                if (codec->container_remaining > size)
                    codec->container_remaining = size;
            }
            format.frame_length = (int64_t)((uint64_t)size / (uint32_t)format.frame_size);
            format.present = true;
            codec->view.format = format;
            return true;
        } else {
            if (memcmp(chunk, "data", 4) == 0)
                prior_data = true;
            if (!header_skip(codec, (uint64_t)size + (size & 1u), error))
                return false;
        }
    }
}

static void close_source(mc_wav_codec *codec, mc_wav_codec_cleanup *report) {
    if (codec->source == NULL)
        return;
    report->source_close_called = true;
    (void)mc_sound_stream_close(codec->source, &report->source_close);
    codec->view.stream_closed = true;
    codec->input_at = codec->input_count = 0;
}

static void cleanup(mc_wav_codec *codec, mc_wav_codec_cleanup *report) {
    *report = (mc_wav_codec_cleanup){.source_close = no_stream_error(),
                                     .native_retire = no_stream_error()};
    close_source(codec, report);
    if (codec->source != NULL) {
        report->native_retire_called = true;
        (void)mc_sound_stream_free(codec->source, &report->native_retire);
        codec->source = NULL;
    }
    free(codec->pushback);
    codec->pushback = NULL;
    codec->pushback_count = 0;
    codec->view.stream_present = false;
    codec->view.format = (mc_wav_codec_format){0};
}

mc_wav_codec_status mc_wav_codec_create(mc_wav_codec_limits limits, mc_wav_codec **out,
                                        mc_wav_codec_error *error) {
    if (out == NULL || limits.input_bytes == 0 || limits.input_bytes > MC_SOUND_STREAM_MAX_BYTES ||
        limits.result_bytes == 0 || limits.result_bytes > MC_WAV_CODEC_MAX_RESULT_BYTES ||
        limits.work_steps == 0 || limits.work_steps > MAX_WORK_STEPS)
        return admission(error, MC_WAV_CODEC_INVALID_ARGUMENT);
    mc_wav_codec *codec = calloc(1, sizeof(*codec));
    if (codec == NULL)
        return admission(error, MC_WAV_CODEC_ALLOCATION_FAILED);
    codec->limits = limits;
    *out = codec;
    if (error != NULL)
        *error = no_error();
    return MC_WAV_CODEC_OK;
}

mc_wav_codec_status mc_wav_codec_initialize(mc_wav_codec *codec,
                                            const mc_sound_file_identity *identity,
                                            bool *initialized, mc_wav_codec_error *out) {
    if (codec == NULL || initialized == NULL)
        return admission(out, MC_WAV_CODEC_INVALID_ARGUMENT);
    if (codec->active)
        return admission(out, MC_WAV_CODEC_BUSY);
    codec->active = true;
    codec->work = 0;
    mc_wav_codec_error error = no_error();
    codec->view.initialized = false;
    cleanup(codec, &error.prior_cleanup);
    const bool old_eos = codec->view.end_of_stream;
    codec->view = (mc_wav_codec_view){.end_of_stream = old_eos};
    codec->offset = 0;
    codec->mark_valid = true;
    codec->container_bounded = false;
    codec->container_remaining = 0;
    *initialized = false;
    if (identity == NULL) {
        source_event(&error, MC_WAV_CODEC_EXCEPTION_NULL_URL, false, 16);
        return finish(codec, &error, out);
    }
    const mc_sound_status status = mc_sound_stream_open_identity(
        identity, false, codec->limits.input_bytes, &codec->source, &error.stream);
    if (status != MC_SOUND_OK) {
        if (status == MC_SOUND_IO_FAILED)
            source_event(&error, MC_WAV_CODEC_EXCEPTION_IO, true, 64);
        else
            (void)fail(&error, map_stream(status), "WAVE source open");
        return finish(codec, &error, out);
    }
    if (!parse_wave(codec, &error)) {
        if (error.status == MC_WAV_CODEC_OK && error.exception == MC_WAV_CODEC_EXCEPTION_NONE)
            source_event(&error,
                         error.stream.status == MC_SOUND_IO_FAILED
                             ? MC_WAV_CODEC_EXCEPTION_IO
                             : MC_WAV_CODEC_EXCEPTION_UNSUPPORTED_AUDIO,
                         true, error.stream.status == MC_SOUND_IO_FAILED ? 64 : 50);
        cleanup(codec, &error.cleanup);
        return finish(codec, &error, out);
    }
    codec->view.stream_present = true;
    codec->view.stream_closed = false;
    codec->view.initialized = true;
    codec->view.end_of_stream = false;
    *initialized = true;
    return finish(codec, &error, out);
}

/* One AudioInputStream.read call: whole-frame request rounding precedes EOF,
 * then retained incomplete-frame bytes precede this actual underlying read. */
static bool audio_read(mc_wav_codec *codec, uint8_t *destination, size_t capacity, int32_t *count,
                       mc_wav_codec_error *error) {
    if (!tick(codec, error))
        return false;
    const size_t frame = (size_t)codec->view.format.frame_size;
    size_t requested = capacity - capacity % frame;
    if (capacity != requested && requested == 0) {
        *count = 0;
        return true;
    }
    if (codec->view.frame_position >= codec->view.format.frame_length) {
        *count = -1;
        return true;
    }
    const uint64_t frames_left =
        (uint64_t)(codec->view.format.frame_length - codec->view.frame_position);
    if (requested / frame > frames_left)
        requested = (size_t)frames_left * frame;
    size_t prefix = 0;
    if (codec->pushback_count != 0 && requested >= codec->pushback_count) {
        prefix = codec->pushback_count;
        memcpy(destination, codec->pushback, prefix);
        codec->pushback_count = 0;
    }
    size_t capacity_underlying = requested - prefix;
    if (codec->container_bounded && capacity_underlying > codec->container_remaining)
        capacity_underlying = (size_t)codec->container_remaining;
    size_t actual = 0;
    if (codec->container_bounded && codec->container_remaining == 0) {
        *count = -1;
        return true;
    }
    if (!buffered_read(codec, destination + prefix, capacity_underlying, &actual, error))
        return false;
    if (codec->container_bounded)
        codec->container_remaining -= actual;
    codec->view.data_bytes_read += actual;
    if (actual == 0 && capacity_underlying != 0) {
        *count = -1;
        return true; /* Source discards an already copied pushback prefix. */
    }
    size_t total = prefix + actual;
    if (total != 0) {
        const size_t remainder = total % frame;
        if (remainder != 0) {
            if (codec->pushback == NULL) {
                codec->pushback = malloc(frame);
                if (codec->pushback == NULL)
                    return fail(error, MC_WAV_CODEC_ALLOCATION_FAILED, "WAVE frame pushback");
            }
            memcpy(codec->pushback, destination + total - remainder, remainder);
            codec->pushback_count = remainder;
            total -= remainder;
        }
        codec->view.frame_position += (int64_t)(total / frame);
    }
    *count = (int32_t)total;
    return true;
}

static bool result_admission(mc_wav_codec *codec, mc_wav_codec_buffer *out,
                             mc_wav_codec_error *error) {
    if (codec == NULL || out == NULL || out->present || out->data != NULL || out->bytes != 0) {
        (void)admission(error, MC_WAV_CODEC_INVALID_ARGUMENT);
        return false;
    }
    if (codec->active) {
        (void)admission(error, MC_WAV_CODEC_BUSY);
        return false;
    }
    if (codec->view.failed) {
        (void)admission(error, MC_WAV_CODEC_FAILED_STATE);
        return false;
    }
    return true;
}

static bool allocate(mc_wav_codec *codec, size_t size, uint8_t **out, mc_wav_codec_error *error) {
    if (size > codec->limits.result_bytes)
        return fail(error, MC_WAV_CODEC_LIMIT, "WAVE result budget");
    if (!tick(codec, error))
        return false;
    uint8_t *memory = calloc(size == 0 ? 1u : size, 1u);
    if (memory == NULL)
        return fail(error, MC_WAV_CODEC_ALLOCATION_FAILED, "WAVE result allocation");
    *out = memory;
    return true;
}

static void publish(mc_wav_codec *codec, uint8_t *data, size_t bytes, mc_wav_codec_buffer *out) {
    if (data != NULL) {
        /* ByteBuffer's 16-bit path leaves an unpaired final byte zero. */
        if (codec->view.format.sample_size_bits == 16 && (bytes & 1u) != 0)
            data[bytes - 1u] = 0;
        *out = (mc_wav_codec_buffer){
            .present = true, .data = data, .bytes = bytes, .format = codec->view.format};
    } else {
        *out = (mc_wav_codec_buffer){0};
    }
}

static bool negative_array(mc_wav_codec_error *error, int32_t pc) {
    source_event(error, MC_WAV_CODEC_EXCEPTION_NEGATIVE_ARRAY, false, pc);
    return fail(error, MC_WAV_CODEC_SOURCE_EXCEPTION, "WAVE source negative array");
}

mc_wav_codec_status mc_wav_codec_read(mc_wav_codec *codec, int32_t streaming_size,
                                      mc_wav_codec_buffer *out, mc_wav_codec_error *result) {
    mc_wav_codec_error error = no_error();
    if (!result_admission(codec, out, &error)) {
        if (result != NULL)
            *result = error;
        return error.status;
    }
    codec->active = true;
    codec->work = 0;
    uint8_t *data = NULL;
    size_t used = 0;
    if (!codec->view.stream_present)
        goto done;
    if (streaming_size < 0) {
        (void)negative_array(&error, 36);
        goto done;
    }
    if (!allocate(codec, (size_t)streaming_size, &data, &error))
        goto done;
    while (!codec->view.end_of_stream && used < (size_t)streaming_size) {
        int32_t count = 0;
        if (!audio_read(codec, data + used, (size_t)streaming_size - used, &count, &error)) {
            if (error.status == MC_WAV_CODEC_OK) {
                codec->view.end_of_stream = true;
                source_event(&error, MC_WAV_CODEC_EXCEPTION_IO, true, 96);
            }
            free(data);
            data = NULL;
            goto done;
        }
        if (count <= 0) {
            codec->view.end_of_stream = true;
            break;
        }
        used += (size_t)count;
    }
    if (used == 0) {
        free(data);
        data = NULL;
    }
done:
    publish(codec, data, used, out);
    return finish(codec, &error, result);
}

static bool append(mc_wav_codec *codec, uint8_t **data, size_t *size, const uint8_t *suffix,
                   size_t bytes, mc_wav_codec_error *error) {
    if (bytes > codec->limits.result_bytes || *size > codec->limits.result_bytes - bytes)
        return fail(error, MC_WAV_CODEC_LIMIT, "WAVE append budget");
    uint8_t *combined = NULL;
    if (!allocate(codec, *size + bytes, &combined, error))
        return false;
    if (*size != 0)
        memcpy(combined, *data, *size);
    if (bytes != 0)
        memcpy(combined + *size, suffix, bytes);
    free(*data);
    *data = combined;
    *size += bytes;
    return true;
}

mc_wav_codec_status mc_wav_codec_read_all(mc_wav_codec *codec, int32_t file_chunk_size,
                                          int32_t max_file_size, mc_wav_codec_buffer *out,
                                          mc_wav_codec_error *result) {
    mc_wav_codec_error error = no_error();
    if (!result_admission(codec, out, &error)) {
        if (result != NULL)
            *result = error;
        return error.status;
    }
    codec->active = true;
    codec->work = 0;
    uint8_t *data = NULL, *chunk = NULL;
    size_t bytes = 0;
    if (!codec->view.stream_present) {
        source_event(&error, MC_WAV_CODEC_EXCEPTION_NULL_STREAM, false, 8);
        goto done;
    }
    const mc_wav_codec_format format = codec->view.format;
    const int32_t expected =
        mc_java_i32_mul(mc_java_i32_mul(format.channels, mc_java_i32_narrow(format.frame_length)),
                        format.sample_size_bits) /
        8;
    if (expected > 0) {
        bytes = (size_t)expected;
        if (!allocate(codec, bytes, &data, &error))
            goto done;
        size_t used = 0;
        for (;;) {
            int32_t count = 0;
            if (!audio_read(codec, data + used, bytes - used, &count, &error)) {
                if (error.status == MC_WAV_CODEC_OK)
                    source_event(&error, MC_WAV_CODEC_EXCEPTION_IO, true, 138);
                free(data);
                data = NULL;
                bytes = 0;
                goto done;
            }
            if (count == -1 || used >= bytes)
                break;
            used += (size_t)count;
        }
    } else {
        if (file_chunk_size < 0) {
            (void)negative_array(&error, 169);
            goto done;
        }
        if (!allocate(codec, (size_t)file_chunk_size, &chunk, &error))
            goto done;
        int32_t total = 0;
        while (!codec->view.end_of_stream && total < max_file_size) {
            size_t used = 0;
            if (!tick(codec, &error))
                goto done;
            while (used < (size_t)file_chunk_size) {
                int32_t count = 0;
                if (!audio_read(codec, chunk + used, (size_t)file_chunk_size - used, &count,
                                &error)) {
                    if (error.status == MC_WAV_CODEC_OK)
                        source_event(&error, MC_WAV_CODEC_EXCEPTION_IO, true, 253);
                    free(data);
                    data = NULL;
                    bytes = 0;
                    goto done;
                }
                if (count <= 0) {
                    codec->view.end_of_stream = true;
                    break;
                }
                used += (size_t)count;
            }
            total = mc_java_i32_add(total, (int32_t)used);
            if (!append(codec, &data, &bytes, chunk, used, &error))
                goto done;
        }
        if (data == NULL) {
            source_event(&error, MC_WAV_CODEC_EXCEPTION_NULL_CONVERSION_DATA, false, 300);
            (void)fail(&error, MC_WAV_CODEC_SOURCE_EXCEPTION, "WAVE source null conversion data");
            goto done;
        }
    }
    close_source(codec, &error.cleanup);
done:
    free(chunk);
    publish(codec, data, bytes, out);
    return finish(codec, &error, result);
}

bool mc_wav_codec_read_view(const mc_wav_codec *codec, mc_wav_codec_view *out) {
    if (codec == NULL || out == NULL)
        return false;
    *out = codec->view;
    out->buffered_bytes = codec->input_count - codec->input_at;
    out->pushback_bytes = codec->pushback_count;
    return true;
}

mc_wav_codec_status mc_wav_codec_cleanup_resources(mc_wav_codec *codec, mc_wav_codec_error *out) {
    if (codec == NULL)
        return admission(out, MC_WAV_CODEC_INVALID_ARGUMENT);
    if (codec->active)
        return admission(out, MC_WAV_CODEC_BUSY);
    codec->active = true;
    mc_wav_codec_error error = no_error();
    cleanup(codec, &error.cleanup);
    return finish(codec, &error, out);
}

mc_wav_codec_status mc_wav_codec_free(mc_wav_codec **owner, mc_wav_codec_error *out) {
    if (owner == NULL)
        return admission(out, MC_WAV_CODEC_INVALID_ARGUMENT);
    if (*owner == NULL) {
        if (out != NULL)
            *out = no_error();
        return MC_WAV_CODEC_OK;
    }
    if ((*owner)->active)
        return admission(out, MC_WAV_CODEC_BUSY);
    mc_wav_codec_error error = no_error();
    cleanup(*owner, &error.cleanup);
    free(*owner);
    *owner = NULL;
    if (out != NULL)
        *out = error;
    return MC_WAV_CODEC_OK;
}

void mc_wav_codec_buffer_free(mc_wav_codec_buffer *buffer) {
    if (buffer != NULL) {
        free(buffer->data);
        *buffer = (mc_wav_codec_buffer){0};
    }
}
