/* Fixed authored streams exercise CodecJOrbis page/buffer policy. Finite
 * original-derived scalar observations below are not a PCM equality oracle. */
#include "mc/client/sound/vorbis_codec.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

enum { MAX_PATH_BYTES = 32768, MAX_INPUT_BYTES = 1048576, MAX_READS = 128 };

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "Vorbis codec check failed at line %d: %s\n", __LINE__,          \
                          #condition);                                                             \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

typedef struct bytes {
    uint8_t *data;
    size_t length;
} bytes;

typedef struct fixture {
    const char *name;
    size_t encoded_bytes, original_pcm_bytes;
    int32_t channels;
    int64_t rate;
} fixture;

/* These lengths come from the retained original read controls on these exact
 * authored inputs. They assert frame/page policy, not equality of PCM samples. */
static const fixture profiles[] = {{"mono32k.ogg", 5019u, 8192u, 1, 32000},
                                   {"stereo48k.ogg", 6797u, 21248u, 2, 48000},
                                   {"mono44k-long.ogg", 30974u, 146688u, 1, 44100},
                                   {"zero-mono32k.ogg", 3351u, 0u, 1, 32000}};

static char *path(const char *root, const char *name) {
    const size_t a = strlen(root), b = strlen(name);
    CHECK(a != 0 && a < MAX_PATH_BYTES - 2u && b < MAX_PATH_BYTES - a - 2u);
    char *out = malloc(a + b + 2u);
    CHECK(out != NULL);
    memcpy(out, root, a);
    out[a] = '/';
    memcpy(out + a + 1u, name, b + 1u);
    return out;
}

static FILE *open_file(const char *name, bool write) {
#ifdef _WIN32
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    CHECK(count > 0 && count < MAX_PATH_BYTES);
    wchar_t *wide = malloc((size_t)count * sizeof(*wide));
    CHECK(wide != NULL);
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, count) == count);
    FILE *file = _wfopen(wide, write ? L"wb" : L"rb");
    free(wide);
#else
    FILE *file = fopen(name, write ? "wb" : "rb");
#endif
    CHECK(file != NULL);
    return file;
}

static bytes load(const char *name) {
    FILE *file = open_file(name, false);
    CHECK(fseek(file, 0L, SEEK_END) == 0);
    const long length = ftell(file);
    CHECK(length >= 0 && length <= MAX_INPUT_BYTES);
    CHECK(fseek(file, 0L, SEEK_SET) == 0);
    bytes out = {.data = malloc(length == 0 ? 1u : (size_t)length), .length = (size_t)length};
    CHECK(out.data != NULL);
    CHECK(fread(out.data, 1u, out.length, file) == out.length);
    CHECK(fgetc(file) == EOF && !ferror(file));
    CHECK(fclose(file) == 0);
    return out;
}

static void write_file(const char *name, const uint8_t *first, size_t first_bytes,
                       const uint8_t *second, size_t second_bytes) {
    FILE *file = open_file(name, true);
    if (first_bytes != 0)
        CHECK(fwrite(first, 1u, first_bytes, file) == first_bytes);
    if (second_bytes != 0)
        CHECK(fwrite(second, 1u, second_bytes, file) == second_bytes);
    CHECK(fclose(file) == 0);
}

static void join(bytes *out, const uint8_t *data, size_t count) {
    CHECK(count <= MAX_INPUT_BYTES && out->length <= MAX_INPUT_BYTES - count);
    if (count == 0)
        return;
    uint8_t *replacement = realloc(out->data, out->length + count);
    CHECK(replacement != NULL);
    memcpy(replacement + out->length, data, count);
    out->data = replacement;
    out->length += count;
}

static mc_vorbis_codec_limits normal_limits(void) {
    return (mc_vorbis_codec_limits){
        .input_bytes = MAX_INPUT_BYTES, .pcm_bytes = MAX_INPUT_BYTES, .work_steps = 65536u};
}

static mc_vorbis_codec *create(mc_vorbis_codec_limits limits) {
    mc_vorbis_codec *codec = NULL;
    mc_vorbis_codec_error error;
    CHECK(mc_vorbis_codec_create(limits, &codec, &error) == MC_VORBIS_CODEC_OK);
    CHECK(codec != NULL && error.status == MC_VORBIS_CODEC_OK);
    return codec;
}

static mc_vorbis_codec_status initialize(mc_vorbis_codec *codec, const char *name,
                                         int32_t streaming_size, bool *initialized,
                                         mc_vorbis_codec_error *error) {
    mc_sound_file *file = NULL;
    mc_sound_error cause;
    CHECK(mc_sound_file_resolve(name, &file, &cause) == MC_SOUND_OK);
    mc_sound_file_identity identity;
    CHECK(mc_sound_file_read_identity(file, &identity));
    const mc_vorbis_codec_status status =
        mc_vorbis_codec_initialize(codec, &identity, false, streaming_size, initialized, error);
    mc_sound_file_free(file);
    return status;
}

static mc_vorbis_codec_view view(const mc_vorbis_codec *codec) {
    mc_vorbis_codec_view out;
    CHECK(mc_vorbis_codec_read_view(codec, &out));
    return out;
}

static bool same_format(mc_vorbis_codec_format a, mc_vorbis_codec_format b) {
    return a.present == b.present && a.channels == b.channels && a.sample_rate == b.sample_rate &&
           a.sample_rate_bits == b.sample_rate_bits;
}

static void free_codec(mc_vorbis_codec **codec) {
    mc_vorbis_codec_error error;
    CHECK(mc_vorbis_codec_free(codec, &error) == MC_VORBIS_CODEC_OK);
    CHECK(*codec == NULL && error.status == MC_VORBIS_CODEC_OK);
    CHECK(mc_vorbis_codec_free(codec, &error) == MC_VORBIS_CODEC_OK);
}

static uint32_t page_crc(const uint8_t *data, size_t length) {
    uint32_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        const uint8_t value = i >= 22u && i < 26u ? 0 : data[i];
        crc ^= (uint32_t)value << 24u;
        for (unsigned int bit = 0; bit < 8u; ++bit)
            crc =
                (crc & UINT32_C(0x80000000)) != 0 ? (crc << 1u) ^ UINT32_C(0x04c11db7) : crc << 1u;
    }
    return crc;
}

/* Build an authored transport variant without re-encoding or changing any
 * packet. The initial granule=-1 bypasses the source codec's granule-zero stop,
 * exposing later silence packets whose 1024 frames exactly fill conversion. */
static bytes trim_input(const char *fixture_root) {
    char *name = path(fixture_root, "zero-mono32k.ogg");
    bytes input = load(name);
    free(name);
    size_t offset = 0, pages = 0;
    while (offset < input.length) {
        CHECK(input.length - offset >= 27u);
        uint8_t *page = input.data + offset;
        CHECK(memcmp(page, "OggS", 4u) == 0 && page[4] == 0);
        const size_t header = 27u + page[26];
        CHECK(header <= input.length - offset);
        size_t length = header;
        for (size_t i = 27u; i < header; ++i)
            length += page[i];
        CHECK(length <= input.length - offset);
        const uint32_t original_crc = (uint32_t)page[22] | ((uint32_t)page[23] << 8u) |
                                      ((uint32_t)page[24] << 16u) | ((uint32_t)page[25] << 24u);
        CHECK(page_crc(page, length) == original_crc);
        if (pages == 2u) {
            CHECK(length == 29u);
            for (size_t i = 6u; i < 14u; ++i) {
                CHECK(page[i] == 0);
                page[i] = 0xff;
            }
            const uint32_t crc = page_crc(page, length);
            for (size_t i = 0; i < 4u; ++i)
                page[22u + i] = (uint8_t)(crc >> (unsigned int)(i * 8u));
        }
        offset += length;
        ++pages;
    }
    CHECK(offset == input.length && pages == 7u);
    return input;
}

/* This prepared page program ends after the fourth audio packet. Its backdated
 * granule trims safely into prior unread PCM, beyond the new block's frames.
 * Four trailing bytes keep the source EOF decrement from truncating that page. */
static bytes saved_tail_input(const bytes *base) {
    bytes input = {.data = malloc(base->length), .length = base->length};
    CHECK(input.data != NULL);
    memcpy(input.data, base->data, base->length);
    size_t offset = 0;
    for (size_t page_index = 0; page_index <= 5u; ++page_index) {
        CHECK(input.length - offset >= 27u);
        uint8_t *page = input.data + offset;
        const size_t header = 27u + page[26];
        CHECK(header <= input.length - offset);
        size_t length = header;
        for (size_t i = 27u; i < header; ++i)
            length += page[i];
        CHECK(length <= input.length - offset);
        if (page_index == 5u) {
            CHECK(length == 29u && input.length - offset - length >= 4u);
            page[5] |= 4u;
            const uint64_t granule = 1000u;
            for (size_t i = 0; i < 8u; ++i)
                page[6u + i] = (uint8_t)(granule >> (unsigned int)(i * 8u));
            const uint32_t crc = page_crc(page, length);
            for (size_t i = 0; i < 4u; ++i)
                page[22u + i] = (uint8_t)(crc >> (unsigned int)(i * 8u));
            memset(page + length, 0, 4u);
            input.length = offset + length + 4u;
            return input;
        }
        offset += length;
    }
    (void)fprintf(stderr, "Saved-tail fixture has no target page\n");
    exit(1);
}

static bytes profile(const char *fixture_root, const char *output_root, const fixture *input) {
    char *name = path(fixture_root, input->name);
    bytes original_input = load(name);
    CHECK(original_input.length == input->encoded_bytes);
    mc_vorbis_codec *codec = create(normal_limits());
    mc_vorbis_codec_error error;
    bool initialized = false;
    CHECK(initialize(codec, name, 131072, &initialized, &error) == MC_VORBIS_CODEC_OK);
    CHECK(initialized);
    mc_vorbis_codec_view state = view(codec);
    CHECK(state.initialized && !state.end_of_stream && !state.failed && state.resources_present);
    CHECK(state.buffer_size == 65536 && state.converted_buffer_size == 131072 && state.count == 0);
    CHECK(state.format.present && state.format.channels == input->channels &&
          state.format.sample_rate == input->rate);
    CHECK(state.source_bytes == input->encoded_bytes);
    const mc_vorbis_codec_format format = state.format;
    mc_vorbis_codec_buffer held[MAX_READS] = {0};
    bytes copies[MAX_READS] = {0};
    bytes accumulated = {0};
    size_t used = 0;
    static const size_t long_bytes[] = {24320u, 24576u, 23808u, 25088u, 24576u, 24320u};
    static const int32_t long_indexes[] = {23458, 19218, 14999, 10758, 6498, 2254};
    while (!state.end_of_stream) {
        CHECK(used < MAX_READS);
        const size_t pages_before = state.pages;
        CHECK(mc_vorbis_codec_read(codec, &held[used], &error) == MC_VORBIS_CODEC_OK);
        CHECK(error.status == MC_VORBIS_CODEC_OK && !error.source_refill_caught);
        state = view(codec);
        CHECK(state.pages == pages_before || state.pages == pages_before + 1u);
        CHECK(state.initialized && !state.failed && same_format(state.format, format));
        if (held[used].present) {
            CHECK(!held[used].data_is_null && held[used].data != NULL);
            CHECK(same_format(held[used].format, format));
            CHECK(held[used].bytes % ((size_t)input->channels * 2u) == 0);
            join(&accumulated, held[used].data, held[used].bytes);
            join(&copies[used], held[used].data, held[used].bytes);
        } else {
            CHECK(held[used].data == NULL && held[used].bytes == 0 && state.end_of_stream);
        }
        if (input->rate == 44100 && used < sizeof(long_bytes) / sizeof(*long_bytes)) {
            /* Original per-page chronology includes the EOF fill decrement. */
            CHECK(held[used].present && held[used].bytes == long_bytes[used]);
            CHECK(state.count == -1 && state.index == long_indexes[used] && !state.end_of_stream);
        }
        ++used;
    }
    CHECK(accumulated.length == input->original_pcm_bytes);
    if (input->original_pcm_bytes == 0) {
        CHECK(used == 1u && !held[0].present && state.converted_bytes == 0);
        CHECK(state.packets == 3u);
    } else {
        bool nonzero = false;
        for (size_t i = 0; i < accumulated.length; ++i)
            nonzero = nonzero || accumulated.data[i] != 0;
        CHECK(nonzero && state.first_granule_transitions != 0);
    }
    mc_vorbis_codec_buffer after_eos = {0};
    const size_t terminal_pages = state.pages;
    CHECK(mc_vorbis_codec_read(codec, &after_eos, &error) == MC_VORBIS_CODEC_OK);
    CHECK(!after_eos.present && after_eos.data == NULL && view(codec).pages == terminal_pages);
    CHECK(mc_vorbis_codec_read_all(codec, INT32_MAX, &after_eos, &error) == MC_VORBIS_CODEC_OK);
    CHECK(after_eos.present && after_eos.data_is_null && after_eos.data == NULL &&
          after_eos.bytes == 0 && same_format(after_eos.format, format));
    mc_vorbis_codec_buffer_free(&after_eos);
    CHECK(mc_vorbis_codec_cleanup_resources(codec, &error) == MC_VORBIS_CODEC_OK);
    CHECK(error.cleanup.stream_clear && error.cleanup.block_clear && error.cleanup.dsp_clear &&
          error.cleanup.info_clear && error.cleanup.comment_clear && error.cleanup.sync_clear);
    CHECK(error.cleanup.source_close_called && error.cleanup.native_retire_called);
    const mc_vorbis_codec_view cleaned = view(codec);
    CHECK(cleaned.initialized && cleaned.end_of_stream && !cleaned.resources_present &&
          cleaned.count == state.count && cleaned.index == state.index &&
          same_format(cleaned.format, format) && cleaned.saved_pcm_frames == 0);
    CHECK(mc_vorbis_codec_cleanup_resources(codec, &error) == MC_VORBIS_CODEC_OK);
    CHECK(!error.cleanup.source_close_called && !error.cleanup.native_retire_called);
    CHECK(initialize(codec, name, 131072, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    mc_vorbis_codec_buffer whole = {0};
    CHECK(mc_vorbis_codec_read_all(codec, 268435456, &whole, &error) == MC_VORBIS_CODEC_OK);
    CHECK(whole.present && whole.bytes == accumulated.length);
    CHECK(whole.data_is_null == (input->original_pcm_bytes == 0));
    if (whole.bytes != 0)
        CHECK(memcmp(whole.data, accumulated.data, whole.bytes) == 0);
    mc_vorbis_codec_buffer_free(&whole);
    free_codec(&codec);
    for (size_t i = 0; i < used; ++i) {
        CHECK(held[i].bytes == copies[i].length);
        if (held[i].bytes != 0)
            CHECK(memcmp(held[i].data, copies[i].data, held[i].bytes) == 0);
        free(copies[i].data);
        mc_vorbis_codec_buffer_free(&held[i]);
    }
    bytes unchanged = load(name);
    CHECK(unchanged.length == original_input.length &&
          memcmp(unchanged.data, original_input.data, unchanged.length) == 0);
    free(unchanged.data);
    free(original_input.data);
    char *output = path(output_root, input->name);
    const size_t length = strlen(output);
    char *pcm = malloc(length + 5u);
    CHECK(pcm != NULL);
    memcpy(pcm, output, length);
    memcpy(pcm + length, ".pcm", 5u);
    write_file(pcm, accumulated.data, accumulated.length, NULL, 0);
    free(pcm);
    free(output);
    free(name);
    return accumulated;
}

static void chain_and_threshold(const char *fixture_root, const char *output_root,
                                const bytes *mono_pcm) {
    char *mono_name = path(fixture_root, "mono32k.ogg");
    char *stereo_name = path(fixture_root, "stereo48k.ogg");
    bytes mono = load(mono_name), stereo = load(stereo_name);
    char *chain = path(output_root, "chain.ogg");
    write_file(chain, mono.data, mono.length, stereo.data, stereo.length);
    mc_vorbis_codec *codec = create(normal_limits());
    mc_vorbis_codec_error error;
    bool initialized = false;
    CHECK(initialize(codec, chain, 131072, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    mc_vorbis_codec_buffer output = {0};
    CHECK(mc_vorbis_codec_read_all(codec, 268435456, &output, &error) == MC_VORBIS_CODEC_OK);
    CHECK(output.present && !output.data_is_null && output.bytes == mono_pcm->length &&
          output.format.channels == 1 && output.format.sample_rate == 32000);
    CHECK(memcmp(output.data, mono_pcm->data, output.bytes) == 0);
    CHECK(view(codec).end_of_stream && view(codec).pages == 3u);
    mc_vorbis_codec_buffer_free(&output);
    char *long_name = path(fixture_root, "mono44k-long.ogg");
    static const int32_t maxima[] = {1, 0, -1, 25000};
    for (size_t i = 0; i < sizeof(maxima) / sizeof(*maxima); ++i) {
        CHECK(initialize(codec, long_name, 131072, &initialized, &error) == MC_VORBIS_CODEC_OK &&
              initialized);
        CHECK(mc_vorbis_codec_read_all(codec, maxima[i], &output, &error) == MC_VORBIS_CODEC_OK);
        /* The source appends a second page before inspecting its threshold. */
        CHECK(output.present && !output.data_is_null && output.bytes == 24320u + 24576u);
        CHECK(!view(codec).end_of_stream && view(codec).index == 19218);
        mc_vorbis_codec_buffer_free(&output);
    }
    /* Mono uses short packets here; its 2048-byte conversion never fills. */
    CHECK(initialize(codec, mono_name, 2048, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    CHECK(mc_vorbis_codec_read_all(codec, 268435456, &output, &error) == MC_VORBIS_CODEC_OK);
    CHECK(output.bytes == mono_pcm->length && view(codec).discarded_full_buffer_bytes == 0);
    CHECK(memcmp(output.data, mono_pcm->data, output.bytes) == 0);
    mc_vorbis_codec_buffer_free(&output);
    /* Stereo's larger audio page is incomplete when read starts. The source
     * publishes EOS on pageout0 without refilling and returns null data. */
    CHECK(initialize(codec, stereo_name, 2048, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    CHECK(mc_vorbis_codec_read_all(codec, 268435456, &output, &error) == MC_VORBIS_CODEC_OK);
    CHECK(output.present && output.data_is_null && output.bytes == 0);
    CHECK(view(codec).end_of_stream && view(codec).pages == 2u &&
          view(codec).converted_bytes == 0 && view(codec).discarded_full_buffer_bytes == 0);
    mc_vorbis_codec_buffer_free(&output);
    bytes authored_trim = trim_input(fixture_root);
    char *trim_name = path(output_root, "exact-full-trim.ogg");
    write_file(trim_name, authored_trim.data, authored_trim.length, NULL, 0);
    CHECK(initialize(codec, trim_name, 2048, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    CHECK(mc_vorbis_codec_read_all(codec, 268435456, &output, &error) == MC_VORBIS_CODEC_OK);
    const mc_vorbis_codec_view small = view(codec);
    CHECK(small.end_of_stream && small.discarded_full_buffer_bytes != 0 &&
          small.last_blockin_code == 0);
    CHECK(output.bytes + small.discarded_full_buffer_bytes == small.converted_bytes);
    CHECK(output.present && !output.data_is_null);
    for (size_t i = 0; i < output.bytes; ++i)
        CHECK(output.data[i] == 0);
    mc_vorbis_codec_buffer_free(&output);
    static const int32_t tiny_buffers[] = {512, 256};
    for (size_t i = 0; i < sizeof(tiny_buffers) / sizeof(*tiny_buffers); ++i) {
        CHECK(initialize(codec, trim_name, tiny_buffers[i], &initialized, &error) ==
                  MC_VORBIS_CODEC_OK &&
              initialized);
        CHECK(mc_vorbis_codec_read_all(codec, 268435456, &output, &error) == MC_VORBIS_CODEC_OK);
        const mc_vorbis_codec_view tiny = view(codec);
        /* JOrbis appends new synthesis while prior PCM remains unread. It
         * fills and discards each small packet result, retaining the first
         * distinct empty result. Rejecting blockin would lose later packets. */
        CHECK(tiny.last_blockin_code == 0);
        CHECK(tiny.end_of_stream && tiny.discarded_full_buffer_bytes != 0);
        CHECK(tiny.saved_pcm_frames_copied != 0 && tiny.saved_pcm_peak_frames != 0);
        CHECK(output.present && !output.data_is_null && output.bytes == 0);
        mc_vorbis_codec_buffer_free(&output);
    }
    bytes saved_tail = saved_tail_input(&authored_trim);
    char *saved_tail_name = path(output_root, "saved-tail-trim.ogg");
    write_file(saved_tail_name, saved_tail.data, saved_tail.length, NULL, 0);
    CHECK(initialize(codec, saved_tail_name, 512, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    CHECK(mc_vorbis_codec_read_all(codec, 268435456, &output, &error) == MC_VORBIS_CODEC_OK);
    const mc_vorbis_codec_view backdated = view(codec);
    CHECK(backdated.last_blockin_code == 0 && backdated.end_of_stream);
    CHECK(backdated.saved_pcm_end_trimmed_frames == 600u && backdated.saved_pcm_frames == 232u);
    CHECK(output.present && !output.data_is_null && output.bytes == 0);
    mc_vorbis_codec_buffer_free(&output);
    CHECK(mc_vorbis_codec_cleanup_resources(codec, &error) == MC_VORBIS_CODEC_OK);
    CHECK(view(codec).saved_pcm_frames == 0 && !view(codec).resources_present);
    free(saved_tail_name);
    free(saved_tail.data);
    free(trim_name);
    free(authored_trim.data);
    free_codec(&codec);
    free(long_name);
    free(chain);
    free(stereo.data);
    free(mono.data);
    free(stereo_name);
    free(mono_name);
}

static void failures(const char *fixture_root, const char *output_root) {
    mc_vorbis_codec_error error;
    mc_vorbis_codec_limits limits = normal_limits();
    mc_vorbis_codec *sentinel = NULL;
    limits.work_steps = 0;
    CHECK(mc_vorbis_codec_create(limits, &sentinel, &error) == MC_VORBIS_CODEC_INVALID_ARGUMENT &&
          sentinel == NULL);
    mc_vorbis_codec *codec = create(normal_limits());
    mc_vorbis_codec_buffer output = {0};
    CHECK(mc_vorbis_codec_read(codec, &output, &error) == MC_VORBIS_CODEC_OK && !output.present);
    char *mono = path(fixture_root, "mono32k.ogg");
    bool initialized = false;
    CHECK(initialize(codec, mono, 131072, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    const mc_vorbis_codec_format format = view(codec).format;
    CHECK(mc_vorbis_codec_cleanup_resources(codec, &error) == MC_VORBIS_CODEC_OK);
    CHECK(mc_vorbis_codec_read(codec, &output, &error) == MC_VORBIS_CODEC_SOURCE_STATE);
    CHECK(view(codec).failed && output.data == NULL && !output.present);
    CHECK(mc_vorbis_codec_read(codec, &output, &error) == MC_VORBIS_CODEC_FAILED_STATE);
    CHECK(initialize(codec, mono, -3, &initialized, &error) == MC_VORBIS_CODEC_LIMIT &&
          !initialized);
    CHECK(!view(codec).initialized && same_format(view(codec).format, format));
    CHECK(initialize(codec, mono, 131072, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    CHECK(!view(codec).failed && !view(codec).end_of_stream);
    free_codec(&codec);

    limits = normal_limits();
    limits.pcm_bytes = 1000u;
    codec = create(limits);
    CHECK(initialize(codec, mono, 131072, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    CHECK(mc_vorbis_codec_read(codec, &output, &error) == MC_VORBIS_CODEC_LIMIT);
    CHECK(view(codec).failed && view(codec).terminal_status == MC_VORBIS_CODEC_LIMIT);
    CHECK(output.bytes <= limits.pcm_bytes);
    mc_vorbis_codec_buffer_free(&output);
    CHECK(mc_vorbis_codec_read(codec, &output, &error) == MC_VORBIS_CODEC_FAILED_STATE);
    free_codec(&codec);

    limits = normal_limits();
    limits.work_steps = 1u;
    codec = create(limits);
    CHECK(initialize(codec, mono, 131072, &initialized, &error) == MC_VORBIS_CODEC_LIMIT &&
          !initialized);
    CHECK(view(codec).failed && view(codec).resources_present);
    free_codec(&codec);

    limits = normal_limits();
    limits.input_bytes = 1u;
    codec = create(limits);
    CHECK(initialize(codec, mono, 131072, &initialized, &error) == MC_VORBIS_CODEC_LIMIT &&
          !initialized);
    CHECK(error.cleanup.info_clear && error.cleanup.comment_clear &&
          !view(codec).resources_present);
    free_codec(&codec);

    char *bad = path(output_root, "malformed.ogg");
    static const uint8_t malformed[] = {0x4e, 0x6f, 0x74, 0x20, 0x4f, 0x67, 0x67};
    write_file(bad, malformed, sizeof(malformed), NULL, 0);
    codec = create(normal_limits());
    CHECK(initialize(codec, bad, 131072, &initialized, &error) == MC_VORBIS_CODEC_SOURCE_STATE &&
          !initialized);
    CHECK(!view(codec).initialized && view(codec).resources_present && view(codec).failed);
    CHECK(initialize(codec, mono, 131072, &initialized, &error) == MC_VORBIS_CODEC_OK &&
          initialized);
    CHECK(mc_vorbis_codec_read(codec, &output, &error) == MC_VORBIS_CODEC_OK &&
          output.bytes == 8192u);
    mc_vorbis_codec_buffer_free(&output);
    free_codec(&codec);
    free(bad);
    free(mono);
}

static void trace_row(const char *operation, unsigned int sequence, mc_vorbis_codec_status status,
                      mc_vorbis_codec *codec, const mc_vorbis_codec_buffer *buffer,
                      const char *output_root) {
    const mc_vorbis_codec_view state = view(codec);
    CHECK(printf("%s\t%u\t%d\t%d\t%d\t%zu\t%d\t%d\t%d\t%" PRId64 "\t%d\t%d\n", operation, sequence,
                 (int)status, buffer->present ? 1 : 0,
                 !buffer->present || buffer->data_is_null ? 1 : 0, buffer->bytes,
                 state.initialized ? 1 : 0, state.end_of_stream ? 1 : 0, state.format.channels,
                 state.format.sample_rate, state.count, state.index) > 0);
    if (buffer->present && !buffer->data_is_null) {
        char leaf[64];
        const int count = snprintf(leaf, sizeof(leaf), "trace-%u.pcm", sequence);
        CHECK(count > 0 && (size_t)count < sizeof(leaf));
        char *name = path(output_root, leaf);
        write_file(name, buffer->data, buffer->bytes, NULL, 0);
        free(name);
    }
}

static int trace(const char *name, const char *output_root, int32_t streaming_size, bool all,
                 int32_t maximum) {
    mc_vorbis_codec *codec = create(normal_limits());
    mc_vorbis_codec_error error;
    bool initialized = false;
    mc_vorbis_codec_status status = initialize(codec, name, streaming_size, &initialized, &error);
    mc_vorbis_codec_buffer output = {0};
    unsigned int sequence = 0;
    trace_row("INIT", sequence++, status, codec, &output, output_root);
    if (status != MC_VORBIS_CODEC_OK || !initialized) {
        free_codec(&codec);
        return 1;
    }
    if (all) {
        status = mc_vorbis_codec_read_all(codec, maximum, &output, &error);
        trace_row("READ_ALL", sequence++, status, codec, &output, output_root);
        mc_vorbis_codec_buffer_free(&output);
        CHECK(status == MC_VORBIS_CODEC_OK);
        status = mc_vorbis_codec_cleanup_resources(codec, &error);
        trace_row("CLEANUP", sequence, status, codec, &output, output_root);
        CHECK(status == MC_VORBIS_CODEC_OK);
        free_codec(&codec);
        return 0;
    }
    while (!view(codec).end_of_stream) {
        CHECK(sequence <= MAX_READS);
        status = mc_vorbis_codec_read(codec, &output, &error);
        trace_row("READ", sequence++, status, codec, &output, output_root);
        mc_vorbis_codec_buffer_free(&output);
        CHECK(status == MC_VORBIS_CODEC_OK);
    }
    status = mc_vorbis_codec_read(codec, &output, &error);
    trace_row("READ", sequence++, status, codec, &output, output_root);
    mc_vorbis_codec_buffer_free(&output);
    CHECK(status == MC_VORBIS_CODEC_OK);
    status = mc_vorbis_codec_read_all(codec, 268435456, &output, &error);
    trace_row("READ_ALL", sequence++, status, codec, &output, output_root);
    mc_vorbis_codec_buffer_free(&output);
    CHECK(status == MC_VORBIS_CODEC_OK);
    status = mc_vorbis_codec_cleanup_resources(codec, &error);
    trace_row("CLEANUP", sequence, status, codec, &output, output_root);
    CHECK(status == MC_VORBIS_CODEC_OK);
    free_codec(&codec);
    return 0;
}

int main(int argc, char **argv) {
    if ((argc == 4 || argc == 5) && strcmp(argv[3], "trace") == 0) {
        int32_t streaming_size = 131072;
        if (argc == 5) {
            char *end = NULL;
            errno = 0;
            const long value = strtol(argv[4], &end, 10);
            CHECK(errno == 0 && end != argv[4] && *end == '\0' && value > 0 && value <= INT32_MAX);
            streaming_size = (int32_t)value;
        }
        return trace(argv[1], argv[2], streaming_size, false, 268435456);
    }
    if ((argc == 5 || argc == 6) && strcmp(argv[3], "trace-all") == 0) {
        char *end = NULL;
        errno = 0;
        const long maximum = strtol(argv[4], &end, 10);
        CHECK(errno == 0 && end != argv[4] && *end == '\0' && maximum >= INT32_MIN &&
              maximum <= INT32_MAX);
        int32_t streaming_size = 131072;
        if (argc == 6) {
            end = NULL;
            errno = 0;
            const long value = strtol(argv[5], &end, 10);
            CHECK(errno == 0 && end != argv[5] && *end == '\0' && value > 0 && value <= INT32_MAX);
            streaming_size = (int32_t)value;
        }
        return trace(argv[1], argv[2], streaming_size, true, (int32_t)maximum);
    }
    CHECK(argc == 3);
    bytes results[sizeof(profiles) / sizeof(*profiles)] = {0};
    for (size_t i = 0; i < sizeof(profiles) / sizeof(*profiles); ++i)
        results[i] = profile(argv[1], argv[2], &profiles[i]);
    chain_and_threshold(argv[1], argv[2], &results[0]);
    failures(argv[1], argv[2]);
    for (size_t i = 0; i < sizeof(profiles) / sizeof(*profiles); ++i)
        free(results[i].data);
    (void)printf("Vorbis codec checks passed: %u\n", checks);
    return 0;
}
