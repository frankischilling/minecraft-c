#include "mc/client/sound/wav_codec.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <share.h>
#include <wchar.h>
#include <windows.h>
#endif

static unsigned checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

typedef struct recipe {
    const char *name;
    uint16_t tag, channels, bits, alignment, valid_bits;
    uint32_t rate, mask, data_bytes, declared_bytes, before_junk, after_junk;
    uint32_t riff_override;
    bool odd_format, float_subtype;
} recipe;

static FILE *open_file(const char *name, const char *mode) {
#ifdef _WIN32
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    CHECK(count > 0 && count < 32768);
    wchar_t *wide = malloc((size_t)count * sizeof(*wide));
    CHECK(wide != NULL);
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, count) == count);
    wchar_t wide_mode[8];
    const size_t length = strlen(mode);
    CHECK(length < sizeof(wide_mode) / sizeof(*wide_mode));
    for (size_t i = 0; i <= length; ++i)
        wide_mode[i] = (wchar_t)(unsigned char)mode[i];
    FILE *file = _wfsopen(wide, wide_mode, _SH_DENYNO);
    free(wide);
    return file;
#else
    return fopen(name, mode);
#endif
}

static char *path(const char *root, const char *name) {
    const size_t a = strlen(root), b = strlen(name);
    CHECK(a < 32768 && b < 1024);
    char *out = malloc(a + b + 2u);
    CHECK(out != NULL);
    memcpy(out, root, a);
    out[a] = '/';
    memcpy(out + a + 1u, name, b + 1u);
    return out;
}

static void bytes(FILE *file, const void *data, size_t count) {
    CHECK(count == 0 || fwrite(data, 1, count, file) == count);
}

static void le16(FILE *file, uint16_t value) {
    const uint8_t data[] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    bytes(file, data, sizeof(data));
}

static void le32(FILE *file, uint32_t value) {
    const uint8_t data[] = {(uint8_t)value, (uint8_t)(value >> 8u), (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 24u)};
    bytes(file, data, sizeof(data));
}

static void junk(FILE *file, uint32_t count) {
    if (count == 0)
        return;
    bytes(file, "JUNK", 4);
    le32(file, count);
    for (uint32_t i = 0; i < count + (count & 1u); ++i) {
        const uint8_t zero = 0;
        bytes(file, &zero, 1);
    }
}

static uint8_t sample(size_t i) {
    return (uint8_t)(i * 17u + 3u);
}

static void write_wave(const char *name, const recipe *input) {
    FILE *file = open_file(name, "wb");
    CHECK(file != NULL);
    const uint32_t fmt_bytes = input->tag == 65534u ? 40u : input->odd_format ? 17u : 16u;
    const uint16_t align = input->alignment != 0
                               ? input->alignment
                               : (uint16_t)(((input->bits + 7u) / 8u) * input->channels);
    const uint32_t before =
        input->before_junk == 0 ? 0u : 8u + input->before_junk + (input->before_junk & 1u);
    const uint32_t after =
        input->after_junk == 0 ? 0u : 8u + input->after_junk + (input->after_junk & 1u);
    const uint32_t riff_size =
        4u + before + 8u + fmt_bytes + (fmt_bytes & 1u) + after + 8u + input->data_bytes;
    bytes(file, "RIFF", 4);
    le32(file, input->riff_override != 0 ? input->riff_override : riff_size);
    bytes(file, "WAVE", 4);
    junk(file, input->before_junk);
    bytes(file, "fmt ", 4);
    le32(file, fmt_bytes);
    le16(file, input->tag);
    le16(file, input->channels);
    le32(file, input->rate);
    le32(file, input->rate * align);
    le16(file, align);
    le16(file, input->bits);
    if (input->tag == 65534u) {
        le16(file, 22);
        le16(file, input->valid_bits);
        le32(file, input->mask);
        le32(file, input->float_subtype ? 3u : 1u);
        static const uint8_t guid[] = {0, 0, 16, 0, 128, 0, 0, 170, 0, 56, 155, 113};
        bytes(file, guid, sizeof(guid));
    } else if (input->odd_format) {
        static const uint8_t extension[] = {7, 0};
        bytes(file, extension, sizeof(extension));
    }
    junk(file, input->after_junk);
    bytes(file, "data", 4);
    le32(file, input->declared_bytes);
    for (uint32_t i = 0; i < input->data_bytes; ++i) {
        const uint8_t value = sample(i);
        bytes(file, &value, 1);
    }
    CHECK(fclose(file) == 0);
}

static mc_wav_codec_limits normal_limits(void) {
    return (mc_wav_codec_limits){MC_SOUND_STREAM_MAX_BYTES, MC_WAV_CODEC_MAX_RESULT_BYTES, 1048576};
}

static mc_wav_codec *create(mc_wav_codec_limits limits) {
    mc_wav_codec *codec = NULL;
    CHECK(mc_wav_codec_create(limits, &codec, NULL) == MC_WAV_CODEC_OK && codec != NULL);
    return codec;
}

static mc_wav_codec_view view(mc_wav_codec *codec) {
    mc_wav_codec_view out;
    CHECK(mc_wav_codec_read_view(codec, &out));
    return out;
}

static mc_wav_codec_status initialize(mc_wav_codec *codec, const char *name, bool *initialized,
                                      mc_wav_codec_error *error) {
    mc_sound_file *file = NULL;
    mc_sound_file_identity identity;
    CHECK(mc_sound_file_resolve(name, &file, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_file_read_identity(file, &identity));
    const mc_wav_codec_status status =
        mc_wav_codec_initialize(codec, &identity, initialized, error);
    mc_sound_file_free(file);
    return status;
}

static void free_codec(mc_wav_codec **codec) {
    CHECK(mc_wav_codec_free(codec, NULL) == MC_WAV_CODEC_OK && *codec == NULL);
}

static void expect_samples(const uint8_t *data, size_t count, size_t offset) {
    for (size_t i = 0; i < count; ++i)
        CHECK(data[i] == sample(offset + i));
}

static void expect_zero(const uint8_t *data, size_t count) {
    for (size_t i = 0; i < count; ++i)
        CHECK(data[i] == 0);
}

static const recipe formats[] = {{.name = "pcm8.wav",
                                  .tag = 1,
                                  .channels = 1,
                                  .bits = 8,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "pcm16.wav",
                                  .tag = 1,
                                  .channels = 1,
                                  .bits = 16,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "pcm24.wav",
                                  .tag = 1,
                                  .channels = 1,
                                  .bits = 24,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "pcm32.wav",
                                  .tag = 1,
                                  .channels = 1,
                                  .bits = 32,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "pcm7.wav",
                                  .tag = 1,
                                  .channels = 1,
                                  .bits = 7,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "pcm12.wav",
                                  .tag = 1,
                                  .channels = 1,
                                  .bits = 12,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "stereo16.wav",
                                  .tag = 1,
                                  .channels = 2,
                                  .bits = 16,
                                  .alignment = 1,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "alaw8.wav",
                                  .tag = 6,
                                  .channels = 1,
                                  .bits = 8,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "ulaw8.wav",
                                  .tag = 7,
                                  .channels = 1,
                                  .bits = 8,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "alaw16.wav",
                                  .tag = 6,
                                  .channels = 1,
                                  .bits = 16,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "float32.wav",
                                  .tag = 3,
                                  .channels = 1,
                                  .bits = 32,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "float64.wav",
                                  .tag = 3,
                                  .channels = 1,
                                  .bits = 64,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "extensible24.wav",
                                  .tag = 65534,
                                  .channels = 1,
                                  .bits = 24,
                                  .valid_bits = 20,
                                  .mask = 3,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "extensible-float32.wav",
                                  .tag = 65534,
                                  .channels = 1,
                                  .bits = 32,
                                  .valid_bits = 32,
                                  .mask = 3,
                                  .float_subtype = true,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "extensible-float64.wav",
                                  .tag = 65534,
                                  .channels = 1,
                                  .bits = 64,
                                  .valid_bits = 64,
                                  .float_subtype = true,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "junk-odd.wav",
                                  .tag = 1,
                                  .channels = 1,
                                  .bits = 16,
                                  .before_junk = 3,
                                  .after_junk = 5,
                                  .rate = 32000,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "classic-negative-rate.wav",
                                  .tag = 1,
                                  .channels = 1,
                                  .bits = 16,
                                  .rate = UINT32_MAX,
                                  .data_bytes = 24,
                                  .declared_bytes = 24},
                                 {.name = "float-unsigned-rate.wav",
                                  .tag = 3,
                                  .channels = 1,
                                  .bits = 32,
                                  .rate = UINT32_MAX,
                                  .data_bytes = 24,
                                  .declared_bytes = 24}};

static void format_profiles(const char *root) {
    mc_wav_codec *codec = create(normal_limits());
    for (size_t i = 0; i < sizeof(formats) / sizeof(*formats); ++i) {
        const recipe *input = formats + i;
        char *name = path(root, input->name);
        write_wave(name, input);
        bool initialized = false;
        mc_wav_codec_error error;
        CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
        const mc_wav_codec_view state = view(codec);
        const int32_t frame = ((int32_t)input->bits + 7) / 8 * input->channels;
        CHECK(state.initialized && !state.end_of_stream && state.stream_present &&
              !state.stream_closed);
        CHECK(state.format.present && !state.format.big_endian &&
              state.format.channels == input->channels &&
              state.format.sample_size_bits == input->bits && state.format.frame_size == frame &&
              state.format.frame_length == 24 / frame &&
              state.format.frame_rate_bits == state.format.sample_rate_bits);
        if (input->rate == 32000u)
            CHECK(state.format.sample_rate_bits == UINT32_C(0x46fa0000));
        else
            CHECK(state.format.sample_rate_bits ==
                  (input->tag == 1u ? UINT32_C(0xbf800000) : UINT32_C(0x4f800000)));
        const mc_wav_codec_encoding encoding = input->tag == 6u   ? MC_WAV_CODEC_ALAW
                                               : input->tag == 7u ? MC_WAV_CODEC_ULAW
                                               : input->tag == 3u || input->float_subtype
                                                   ? MC_WAV_CODEC_PCM_FLOAT
                                               : input->bits == 8u ? MC_WAV_CODEC_PCM_UNSIGNED
                                                                   : MC_WAV_CODEC_PCM_SIGNED;
        CHECK(state.format.encoding == encoding &&
              state.format.extensible == (input->tag == 65534u));
        if (input->tag == 65534u)
            CHECK(state.format.valid_bits_per_sample == input->valid_bits &&
                  state.format.channel_mask == input->mask);
        size_t total = 0;
        mc_wav_codec_buffer buffer = {0};
        while (!view(codec).end_of_stream) {
            CHECK(total <= 24);
            CHECK(mc_wav_codec_read(codec, frame * 3, &buffer, &error) == MC_WAV_CODEC_OK);
            if (buffer.present) {
                CHECK(buffer.data != NULL && buffer.bytes <= 24u - total);
                expect_samples(buffer.data, buffer.bytes, total);
                total += buffer.bytes;
            }
            mc_wav_codec_buffer_free(&buffer);
        }
        CHECK(total == 24);
        CHECK(mc_wav_codec_read_all(codec, -7, -1, &buffer, &error) == MC_WAV_CODEC_OK);
        const size_t expected =
            (size_t)(input->channels * (24u / (uint32_t)frame) * input->bits / 8u);
        CHECK(buffer.present && buffer.bytes == expected && view(codec).stream_closed);
        expect_zero(buffer.data, buffer.bytes);
        mc_wav_codec_buffer_free(&buffer);
        CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
        CHECK(mc_wav_codec_read_all(codec, -7, -1, &buffer, &error) == MC_WAV_CODEC_OK);
        CHECK(buffer.present && buffer.bytes == expected && !view(codec).end_of_stream &&
              view(codec).stream_closed);
        expect_samples(buffer.data, buffer.bytes, 0);
        mc_wav_codec_buffer held = buffer;
        buffer = (mc_wav_codec_buffer){0};
        CHECK(mc_wav_codec_cleanup_resources(codec, &error) == MC_WAV_CODEC_OK);
        CHECK(!view(codec).stream_present && !view(codec).format.present &&
              view(codec).initialized);
        expect_samples(held.data, held.bytes, 0);
        mc_wav_codec_buffer_free(&held);
        free(name);
    }
    free_codec(&codec);
}

static void rounding_padding_and_limits(const char *root) {
    mc_wav_codec *codec = create(normal_limits());
    recipe input = formats[1];
    input.name = "truncated.wav";
    input.declared_bytes = 40;
    char *name = path(root, input.name);
    write_wave(name, &input);
    bool initialized = false;
    mc_wav_codec_error error;
    mc_wav_codec_buffer output = {0};
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read(codec, 7, &output, &error) == MC_WAV_CODEC_OK);
    CHECK(output.present && output.bytes == 6 && view(codec).end_of_stream &&
          view(codec).frame_position == 3);
    expect_samples(output.data, output.bytes, 0);
    mc_wav_codec_buffer_free(&output);
    CHECK(mc_wav_codec_read_all(codec, -1, 0, &output, &error) == MC_WAV_CODEC_OK);
    CHECK(output.present && output.bytes == 40 && view(codec).end_of_stream &&
          view(codec).stream_closed);
    expect_samples(output.data, 18, 6);
    expect_zero(output.data + 18, 22);
    mc_wav_codec_buffer_free(&output);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read_all(codec, -1, 0, &output, &error) == MC_WAV_CODEC_OK);
    CHECK(output.bytes == 40 && !view(codec).end_of_stream && view(codec).frame_position == 12);
    expect_samples(output.data, 24, 0);
    expect_zero(output.data + 24, 16);
    mc_wav_codec_buffer_free(&output);
    CHECK(mc_wav_codec_read(codec, 8, &output, &error) == MC_WAV_CODEC_OK && !output.present &&
          view(codec).end_of_stream && error.source_caught &&
          error.exception == MC_WAV_CODEC_EXCEPTION_IO);
    input.data_bytes = 1;
    input.declared_bytes = 4;
    write_wave(name, &input);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read_all(codec, 1, 1, &output, &error) == MC_WAV_CODEC_OK);
    CHECK(output.bytes == 4 && !view(codec).end_of_stream && view(codec).frame_position == 0);
    CHECK(output.data[0] == sample(0));
    expect_zero(output.data + 1, 3);
    mc_wav_codec_buffer_free(&output);
    input.data_bytes = input.declared_bytes = 0;
    write_wave(name, &input);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read(codec, 0, &output, &error) == MC_WAV_CODEC_OK && !output.present &&
          !view(codec).end_of_stream);
    CHECK(mc_wav_codec_read(codec, -1, &output, &error) == MC_WAV_CODEC_SOURCE_EXCEPTION &&
          error.exception == MC_WAV_CODEC_EXCEPTION_NEGATIVE_ARRAY && !error.source_caught &&
          !view(codec).failed);
    CHECK(mc_wav_codec_read_all(codec, 9, 0, &output, &error) == MC_WAV_CODEC_SOURCE_EXCEPTION &&
          error.exception == MC_WAV_CODEC_EXCEPTION_NULL_CONVERSION_DATA &&
          !view(codec).stream_closed);
    CHECK(mc_wav_codec_read_all(codec, -1, 100, &output, &error) == MC_WAV_CODEC_SOURCE_EXCEPTION &&
          error.exception == MC_WAV_CODEC_EXCEPTION_NEGATIVE_ARRAY && !view(codec).failed);
    CHECK(mc_wav_codec_read_all(codec, 9, 100, &output, &error) == MC_WAV_CODEC_OK);
    CHECK(output.present && output.data != NULL && output.bytes == 0 && view(codec).end_of_stream &&
          view(codec).stream_closed);
    mc_wav_codec_buffer_free(&output);
    CHECK(mc_wav_codec_read_all(codec, 9, 100, &output, &error) == MC_WAV_CODEC_SOURCE_EXCEPTION &&
          error.exception == MC_WAV_CODEC_EXCEPTION_NULL_CONVERSION_DATA);
    CHECK(mc_wav_codec_initialize(codec, NULL, &initialized, &error) == MC_WAV_CODEC_OK &&
          !initialized && !view(codec).initialized && view(codec).end_of_stream &&
          error.exception == MC_WAV_CODEC_EXCEPTION_NULL_URL);
    CHECK(mc_wav_codec_read(codec, 1, &output, NULL) == MC_WAV_CODEC_OK && !output.present);
    CHECK(mc_wav_codec_read_all(codec, 1, 1, &output, &error) == MC_WAV_CODEC_OK &&
          !output.present && error.exception == MC_WAV_CODEC_EXCEPTION_NULL_STREAM);
    free_codec(&codec);
    mc_wav_codec_limits limits = normal_limits();
    limits.work_steps = 32;
    codec = create(limits);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read_all(codec, 0, 1, &output, &error) == MC_WAV_CODEC_LIMIT &&
          view(codec).failed);
    mc_wav_codec_buffer_free(&output);
    CHECK(mc_wav_codec_read(codec, 1, &output, &error) == MC_WAV_CODEC_FAILED_STATE);
    free_codec(&codec);
    input.data_bytes = input.declared_bytes = 24;
    write_wave(name, &input);
    limits = normal_limits();
    limits.result_bytes = 8;
    codec = create(limits);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read_all(codec, 1, 1, &output, &error) == MC_WAV_CODEC_LIMIT &&
          !output.present && view(codec).frame_position == 0 && !view(codec).stream_closed);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read(codec, 8, &output, NULL) == MC_WAV_CODEC_OK && output.bytes == 8);
    mc_wav_codec_buffer_free(&output);
    free_codec(&codec);
    limits.input_bytes = 8;
    codec = create(limits);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_LIMIT && !initialized);
    free_codec(&codec);
    free(name);
}

static void provider_boundaries(const char *root) {
    mc_wav_codec *codec = create(normal_limits());
    recipe input = formats[1];
    char *name = path(root, "provider-boundary.wav");
    bool initialized = false;
    mc_wav_codec_error error;
    mc_wav_codec_buffer output = {0};
    input.odd_format = true;
    write_wave(name, &input);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && !initialized &&
          error.exception == MC_WAV_CODEC_EXCEPTION_UNSUPPORTED_AUDIO && error.source_caught &&
          error.cleanup.native_retire_called);
    input.odd_format = false;
    input.before_junk = 8192;
    write_wave(name, &input);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && !initialized &&
          error.exception == MC_WAV_CODEC_EXCEPTION_IO && error.source_caught);
    input.before_junk = 0;
    input.after_junk = 16384;
    write_wave(name, &input);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read_all(codec, 1, 1, &output, NULL) == MC_WAV_CODEC_OK &&
          output.bytes == 24);
    expect_samples(output.data, 24, 0);
    mc_wav_codec_buffer_free(&output);
    input.after_junk = 0;
    input.riff_override = 36;
    write_wave(name, &input);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read_all(codec, 1, 1, &output, NULL) == MC_WAV_CODEC_OK &&
          output.bytes == 24);
    expect_samples(output.data, 24, 0); /* Classic provider ignores RIFF extent. */
    mc_wav_codec_buffer_free(&output);
    input.tag = 3;
    input.bits = 32;
    write_wave(name, &input);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(mc_wav_codec_read_all(codec, 1, 1, &output, NULL) == MC_WAV_CODEC_OK &&
          output.bytes == 24);
    expect_zero(output.data, 24); /* Float provider retains the RIFFReader bound. */
    mc_wav_codec_buffer_free(&output);
    FILE *file = open_file(name, "wb");
    CHECK(file != NULL);
    bytes(file, "FORM\0\0\0\4AIFF", 12);
    CHECK(fclose(file) == 0);
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_UNIMPLEMENTED_CONTAINER &&
          !initialized && error.exception == MC_WAV_CODEC_EXCEPTION_NONE);
    free_codec(&codec);
    free(name);
}

static void buffered_file_and_ownership(const char *root) {
    mc_wav_codec *codec = create(normal_limits());
    recipe input = formats[1];
    input.data_bytes = input.declared_bytes = 20000;
    char *name = path(root, "buffered.wav");
    write_wave(name, &input);
    bool initialized = false;
    mc_wav_codec_error error;
    mc_wav_codec_buffer output = {0};
    CHECK(initialize(codec, name, &initialized, &error) == MC_WAV_CODEC_OK && initialized);
    CHECK(view(codec).source_bytes == 8192 && view(codec).buffered_bytes == 8192u - 44u);
    FILE *file = open_file(name, "r+b");
    CHECK(file != NULL && fseek(file, 44, SEEK_SET) == 0);
    for (size_t i = 0; i < input.data_bytes; ++i) {
        const uint8_t zero = 0;
        bytes(file, &zero, 1);
    }
    CHECK(fclose(file) == 0);
    CHECK(mc_wav_codec_read(codec, 16384, &output, &error) == MC_WAV_CODEC_OK &&
          output.bytes == 16384);
    expect_samples(output.data, 8192u - 44u, 0);
    expect_zero(output.data + 8192u - 44u, output.bytes - (8192u - 44u));
    mc_wav_codec_buffer held = output;
    output = (mc_wav_codec_buffer){0};
    CHECK(mc_wav_codec_cleanup_resources(codec, &error) == MC_WAV_CODEC_OK &&
          error.cleanup.source_close_called && error.cleanup.native_retire_called &&
          view(codec).initialized && !view(codec).format.present);
    free_codec(&codec);
    expect_samples(held.data, 8192u - 44u, 0);
    mc_wav_codec_buffer_free(&held);
    free(name);
}

static const char *encoding_name(mc_wav_codec_encoding encoding) {
    static const char *const names[] = {"PCM_SIGNED", "PCM_UNSIGNED", "PCM_FLOAT", "ALAW", "ULAW"};
    CHECK((unsigned)encoding < sizeof(names) / sizeof(*names));
    return names[encoding];
}

static void trace_row(const char *operation, unsigned sequence, mc_wav_codec *codec,
                      const mc_wav_codec_buffer *buffer, mc_wav_codec_status status,
                      const mc_wav_codec_error *error, const char *root) {
    const mc_wav_codec_view state = view(codec);
    const char *exception = status == MC_WAV_CODEC_SOURCE_EXCEPTION
                                ? error->exception == MC_WAV_CODEC_EXCEPTION_NEGATIVE_ARRAY
                                      ? "NegativeArraySizeException"
                                      : "NullPointerException"
                            : status == MC_WAV_CODEC_OK ? "NONE"
                                                        : "NATIVE";
    printf("%s\t%u\t%s\t%s\t%s\t%" PRId64 "\t%s\t%s\t%d\t%d\t%d\t%" PRIu32 "\t%d\t%" PRId64 "\n",
           operation, sequence, state.initialized ? "true" : "false",
           state.end_of_stream ? "true" : "false", buffer->present ? "true" : "false",
           buffer->present ? (int64_t)buffer->bytes : -1, exception,
           state.format.present ? encoding_name(state.format.encoding) : "NONE",
           state.format.present ? state.format.channels : 0,
           state.format.present ? state.format.sample_size_bits : 0,
           state.format.present ? state.format.frame_size : 0,
           state.format.present ? state.format.sample_rate_bits : 0, 0,
           state.format.present ? state.format.frame_length : -1);
    if (buffer->present) {
        char leaf[64];
        CHECK(snprintf(leaf, sizeof(leaf), "pcm-%u.bin", sequence) > 0);
        char *name = path(root, leaf);
        FILE *file = open_file(name, "wb");
        CHECK(file != NULL);
        bytes(file, buffer->data, buffer->bytes);
        CHECK(fclose(file) == 0);
        free(name);
    }
}

static int trace(int argc, char **argv) {
    CHECK(argc == 8);
    mc_wav_codec *codec = create(normal_limits());
    mc_wav_codec_buffer output = {0};
    mc_wav_codec_error error;
    const int32_t stream_size = (int32_t)strtol(argv[5], NULL, 10);
    const int32_t chunk_size = (int32_t)strtol(argv[6], NULL, 10);
    const int32_t maximum = (int32_t)strtol(argv[7], NULL, 10);
    bool initialized = false;
    const mc_wav_codec_status init_status = initialize(codec, argv[1], &initialized, &error);
    unsigned sequence = 0;
    trace_row(initialized ? "INIT-true" : "INIT-false", sequence++, codec, &output, init_status,
              &error, argv[2]);
    if (initialized) {
        if (strcmp(argv[4], "all") == 0) {
            const mc_wav_codec_status status =
                mc_wav_codec_read_all(codec, chunk_size, maximum, &output, &error);
            trace_row("readAll", sequence++, codec, &output, status, &error, argv[2]);
            mc_wav_codec_buffer_free(&output);
        } else {
            for (size_t i = 0; i < 10; ++i) {
                const mc_wav_codec_status status =
                    mc_wav_codec_read(codec, stream_size, &output, &error);
                trace_row("read", sequence++, codec, &output, status, &error, argv[2]);
                mc_wav_codec_buffer_free(&output);
                if (view(codec).end_of_stream)
                    break;
            }
        }
        mc_wav_codec_status status = mc_wav_codec_read(codec, stream_size, &output, &error);
        trace_row("read", sequence++, codec, &output, status, &error, argv[2]);
        mc_wav_codec_buffer_free(&output);
        status = mc_wav_codec_read_all(codec, chunk_size, maximum, &output, &error);
        trace_row("readAll", sequence++, codec, &output, status, &error, argv[2]);
        mc_wav_codec_buffer_free(&output);
    }
    CHECK(mc_wav_codec_cleanup_resources(codec, &error) == MC_WAV_CODEC_OK);
    trace_row("CLEANUP", sequence, codec, &output, MC_WAV_CODEC_OK, &error, argv[2]);
    free_codec(&codec);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 4 && strcmp(argv[3], "trace") == 0)
        return trace(argc, argv);
    CHECK(argc == 2);
    format_profiles(argv[1]);
    rounding_padding_and_limits(argv[1]);
    provider_boundaries(argv[1]);
    buffered_file_and_ownership(argv[1]);
    printf("WAVE codec checks passed: %u\n", checks);
    return 0;
}
