#include "mc/client/sound/vorbis_decoder.h"

#include "pcm_internal.h"

#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <share.h>
#include <windows.h>
#endif

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Vorbis decoder check failed at line %d: %s\n", __LINE__, #condition); \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static float float_bits(uint32_t bits) {
    _Static_assert(sizeof(float) == sizeof(bits), "PCM vectors require binary32 storage");
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void pcm_vectors(void) {
    static const struct {
        uint32_t bits;
        uint8_t low, high;
    } vectors[] = {{UINT32_C(0x00000000), 0x00, 0x00}, {UINT32_C(0x80000000), 0x00, 0x00},
                   {UINT32_C(0x3e800000), 0xff, 0x1f}, {UINT32_C(0xbe800000), 0x01, 0xe0},
                   {UINT32_C(0x3f000000), 0xff, 0x3f}, {UINT32_C(0xbf000000), 0x01, 0xc0},
                   {UINT32_C(0x3f000100), 0x00, 0x40}, {UINT32_C(0xbf000100), 0x00, 0xc0},
                   {UINT32_C(0x3f7fffff), 0xfe, 0x7f}, {UINT32_C(0xbf7fffff), 0x02, 0x80},
                   {UINT32_C(0x3f800000), 0xff, 0x7f}, {UINT32_C(0xbf800000), 0x01, 0x80},
                   {UINT32_C(0x3f800001), 0xff, 0x7f}, {UINT32_C(0xbf800001), 0x01, 0x80},
                   {UINT32_C(0x40000000), 0xff, 0x7f}, {UINT32_C(0xc0000000), 0x00, 0x80},
                   {UINT32_C(0x7f800000), 0xff, 0x7f}, {UINT32_C(0xff800000), 0x00, 0x80},
                   {UINT32_C(0x7fc12345), 0x00, 0x00}, {UINT32_C(0xffc12345), 0x00, 0x00},
                   {UINT32_C(0x00000001), 0x00, 0x00}, {UINT32_C(0x80000001), 0x00, 0x00},
                   {UINT32_C(0x7f7fffff), 0xff, 0x7f}, {UINT32_C(0xff7fffff), 0x00, 0x80}};

    /* Literal policy results: half amplitude truncates to 16383. For 3f000100,
     * the binary32 product rounds to 16384 before the Java conversion. */
    for (size_t i = 0; i < sizeof(vectors) / sizeof(*vectors); ++i) {
        uint8_t sample_bytes[] = {0xa5, 0xa5, 0xa5, 0xa5};
        mc_sound_pcm_write_s16le(float_bits(vectors[i].bits), sample_bytes + 1u);
        CHECK(sample_bytes[1] == vectors[i].low && sample_bytes[2] == vectors[i].high);
        CHECK(sample_bytes[0] == 0xa5 && sample_bytes[3] == 0xa5);
    }

    float left[] = {0.5f, -1.0f, 0.0f};
    float right[] = {-0.5f, 1.0f, 0.0f};
    left[2] = float_bits(UINT32_C(0x7fc12345));
    right[2] = float_bits(UINT32_C(0x7f800000));
    float *planes[] = {left, right};
    uint8_t source_left[sizeof(left)], source_right[sizeof(right)];
    memcpy(source_left, left, sizeof(left));
    memcpy(source_right, right, sizeof(right));
    mc_sound_pcm_pending pending = {.planes = planes, .frames = 3, .channel_count = 2};
    static const uint8_t expected[] = {0xff, 0x3f, 0x01, 0xc0, 0x01, 0x80,
                                       0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f};
    uint8_t actual[16], untouched[16];
    memset(actual, 0xa5, sizeof(actual));
    memset(untouched, 0xa5, sizeof(untouched));
    CHECK(mc_sound_pcm_write_pending(&pending, actual + 1u, 3u) == 0);
    CHECK(memcmp(actual, untouched, sizeof(actual)) == 0);
    CHECK(pending.planes == planes && pending.frames == 3u && pending.offset == 0 &&
          pending.channel_count == 2u);
    CHECK(mc_sound_pcm_write_pending(&pending, actual + 1u, 5u) == 4u);
    CHECK(memcmp(actual + 1u, expected, 4u) == 0 && actual[0] == 0xa5 && actual[5] == 0xa5);
    CHECK(pending.planes == planes && pending.frames == 2u && pending.offset == 1u &&
          pending.channel_count == 2u);
    memset(actual, 0xa5, sizeof(actual));
    CHECK(mc_sound_pcm_write_pending(&pending, actual + 1u, 8u) == 8u);
    CHECK(memcmp(actual + 1u, expected + 4u, 8u) == 0 && actual[0] == 0xa5 && actual[9] == 0xa5);
    CHECK(pending.planes == NULL && pending.frames == 0 && pending.offset == 0 &&
          pending.channel_count == 0);
    CHECK(mc_sound_pcm_write_pending(&pending, NULL, 0) == 0);
    CHECK(memcmp(left, source_left, sizeof(left)) == 0 &&
          memcmp(right, source_right, sizeof(right)) == 0);
}

typedef struct bytes {
    uint8_t *data;
    size_t length, capacity;
} bytes;

static void append(bytes *out, const void *data, size_t length) {
    CHECK(length <= 1048576u && out->length <= 1048576u - length);
    const size_t needed = out->length + length;
    if (needed > out->capacity) {
        size_t capacity = out->capacity == 0 ? 4096u : out->capacity;
        while (capacity < needed)
            capacity *= 2u;
        uint8_t *replacement = realloc(out->data, capacity);
        CHECK(replacement != NULL);
        out->data = replacement;
        out->capacity = capacity;
    }
    if (length != 0)
        memcpy(out->data + out->length, data, length);
    out->length = needed;
}

static void page(bytes *out, const ogg_page *value) {
    CHECK(value->header_len >= 0 && value->body_len >= 0);
    append(out, value->header, (size_t)value->header_len);
    append(out, value->body, (size_t)value->body_len);
}

/* Public encoder calls generate this test's own zero-valued PCM. Fixed serials
 * and comments are authored inputs, not original encoded assets or PCM oracles. */
static bytes zero_stream(int channels, long rate, int frames, int serial) {
    vorbis_info info;
    vorbis_info_init(&info);
    CHECK(vorbis_encode_init_vbr(&info, channels, rate, 0.1f) == 0);
    vorbis_comment comment;
    vorbis_comment_init(&comment);
    vorbis_comment_add_tag(&comment, "TITLE", "authored decoder silence");
    vorbis_dsp_state analysis;
    CHECK(vorbis_analysis_init(&analysis, &info) == 0);
    vorbis_block block;
    CHECK(vorbis_block_init(&analysis, &block) == 0);
    ogg_stream_state stream;
    CHECK(ogg_stream_init(&stream, serial) == 0);
    ogg_packet identification, text, setup;
    CHECK(vorbis_analysis_headerout(&analysis, &comment, &identification, &text, &setup) == 0);
    CHECK(ogg_stream_packetin(&stream, &identification) == 0);
    CHECK(ogg_stream_packetin(&stream, &text) == 0);
    CHECK(ogg_stream_packetin(&stream, &setup) == 0);
    bytes encoded = {0};
    ogg_page output;
    while (ogg_stream_flush(&stream, &output) != 0)
        page(&encoded, &output);
    float **pcm = vorbis_analysis_buffer(&analysis, frames);
    CHECK(pcm != NULL);
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < frames; ++i)
            pcm[channel][i] = 0.0f;
    CHECK(vorbis_analysis_wrote(&analysis, frames) == 0);
    CHECK(vorbis_analysis_wrote(&analysis, 0) == 0);
    bool eos = false;
    int available;
    while ((available = vorbis_analysis_blockout(&analysis, &block)) == 1) {
        CHECK(vorbis_analysis(&block, NULL) == 0);
        CHECK(vorbis_bitrate_addblock(&block) == 0);
        ogg_packet packet;
        int packet_ready;
        while ((packet_ready = vorbis_bitrate_flushpacket(&analysis, &packet)) == 1) {
            CHECK(ogg_stream_packetin(&stream, &packet) == 0);
            /* Separate audio pages also permit an authored missing-page case. */
            while (ogg_stream_flush(&stream, &output) != 0) {
                page(&encoded, &output);
                eos = eos || ogg_page_eos(&output) != 0;
            }
        }
        CHECK(packet_ready == 0);
    }
    CHECK(available == 0 && eos && encoded.length > 100u);
    CHECK(ogg_stream_clear(&stream) == 0);
    CHECK(vorbis_block_clear(&block) == 0);
    vorbis_dsp_clear(&analysis);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    return encoded;
}

static char *path(const char *root, const char *leaf) {
    const size_t size = strlen(root) + strlen(leaf) + 2u;
    char *value = malloc(size);
    CHECK(value != NULL);
    const int count = snprintf(value, size, "%s/%s", root, leaf);
    CHECK(count >= 0 && (size_t)count + 1u == size);
    return value;
}

#ifdef _WIN32
static wchar_t *wide_path(const char *name) {
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    CHECK(length > 0);
    wchar_t *wide = malloc((size_t)length * sizeof(*wide));
    CHECK(wide != NULL);
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, length) == length);
    return wide;
}
#endif

static void write_file(const char *root, const char *leaf, const bytes *raw) {
    char *name = path(root, leaf);
#ifdef _WIN32
    wchar_t *wide = wide_path(name);
    FILE *output = _wfsopen(wide, L"wbx", _SH_DENYNO);
    free(wide);
#else
    FILE *output = fopen(name, "wbx");
#endif
    free(name);
    CHECK(output != NULL);
    CHECK(fwrite(raw->data, 1, raw->length, output) == raw->length);
    CHECK(fclose(output) == 0);
}

static bytes remove_page(const bytes *input) {
    bytes out = {0};
    size_t offset = 0, index = 0;
    bool omitted = false;
    while (offset < input->length) {
        CHECK(input->length - offset >= 27u);
        const uint8_t *header = input->data + offset;
        CHECK(memcmp(header, "OggS", 4u) == 0);
        const size_t header_bytes = 27u + header[26];
        CHECK(header_bytes <= input->length - offset);
        size_t body = 0;
        for (size_t i = 27u; i < header_bytes; ++i)
            body += header[i];
        CHECK(body <= input->length - offset - header_bytes);
        if (index == 3u)
            omitted = true;
        else
            append(&out, header, header_bytes + body);
        offset += header_bytes + body;
        ++index;
    }
    CHECK(omitted && index > 4u);
    return out;
}

static int create_fixtures(const char *root) {
    bytes mono = zero_stream(1, 32000, 3200, 112233);
    bytes stereo = zero_stream(2, 48000, 4800, 445566);
    write_file(root, "mono.ogg", &mono);
    write_file(root, "stereo.ogg", &stereo);
    bytes chain = {0};
    append(&chain, mono.data, mono.length);
    append(&chain, stereo.data, stereo.length);
    write_file(root, "chain.ogg", &chain);
    bytes encrypted = {0};
    append(&encrypted, mono.data, mono.length);
    const char basename[] = "stream%20sample.mus";
    uint32_t state = 0;
    for (size_t i = 0; i + 1u < sizeof(basename); ++i)
        state = state * UINT32_C(31) + (uint8_t)basename[i];
    for (size_t i = 0; i < encrypted.length; ++i) {
        const uint8_t decoded = encrypted.data[i];
        encrypted.data[i] ^= (uint8_t)(state >> 8);
        const int32_t signed_byte = decoded < 128u ? (int32_t)decoded : (int32_t)decoded - 256;
        state = state * UINT32_C(498729871) + UINT32_C(85731) * (uint32_t)signed_byte;
    }
    write_file(root, "stream sample.mus", &encrypted);
    static uint8_t bad[] = {'n', 'o', 't', '-', 'v', 'o', 'r', 'b', 'i', 's'};
    const bytes malformed = {bad, sizeof(bad), sizeof(bad)};
    write_file(root, "malformed.ogg", &malformed);
    const bytes truncated = {mono.data, 20u, 20u};
    write_file(root, "truncated.ogg", &truncated);
    bytes hole = remove_page(&mono);
    write_file(root, "hole.ogg", &hole);
    free(hole.data);
    free(encrypted.data);
    free(chain.data);
    free(stereo.data);
    free(mono.data);
    if (printf("PASS: 7 authored Vorbis fixtures\n") < 0 || fflush(stdout) != 0 || ferror(stdout))
        return 1;
    return 0;
}

static mc_sound_file *resolve(const char *root, const char *leaf) {
    char *name = path(root, leaf);
    mc_sound_file *file = NULL;
    CHECK(mc_sound_file_resolve(name, &file, NULL) == MC_SOUND_OK && file != NULL);
    free(name);
    return file;
}

static mc_vorbis_decoder *open_decoder(const char *root, const char *leaf, bool mus,
                                       size_t budget) {
    mc_sound_file *file = resolve(root, leaf);
    mc_sound_file_identity identity;
    CHECK(mc_sound_file_read_identity(file, &identity));
    if (mus) {
        static const uint16_t suffix[] = {'s', 't', 'r', 'e', 'a', 'm', '%', '2', '0', 's',
                                          'a', 'm', 'p', 'l', 'e', '.', 'm', 'u', 's'};
        CHECK(identity.url_path.length >= sizeof(suffix) / sizeof(*suffix));
        CHECK(memcmp(identity.url_path.units + identity.url_path.length -
                         sizeof(suffix) / sizeof(*suffix),
                     suffix, sizeof(suffix)) == 0);
    }
    mc_vorbis_decoder *decoder = NULL;
    mc_vorbis_error error;
    CHECK(mc_vorbis_decoder_open_identity(&identity, mus, MC_SOUND_STREAM_MAX_BYTES, budget,
                                          &decoder, &error) == MC_VORBIS_DECODER_OK);
    CHECK(decoder != NULL && error.status == MC_VORBIS_DECODER_OK);
    mc_sound_file_free(file); /* Decoder retains neither borrowed view. */
    return decoder;
}

static void retire(mc_vorbis_decoder **decoder) {
    mc_vorbis_retirement report;
    mc_vorbis_error error;
    CHECK(mc_vorbis_decoder_retire(decoder, &report, &error) == MC_VORBIS_DECODER_OK);
    CHECK(*decoder == NULL && report.decoder_clear_called && report.source_close_called &&
          report.native_retire_called && report.vorbis_clear_code == 0);
    CHECK(report.source_close.status == MC_SOUND_OK && report.native_retire.status == MC_SOUND_OK);
    CHECK(error.status == MC_VORBIS_DECODER_OK && error.cleanup.decoder_clear_called);
    CHECK(mc_vorbis_decoder_retire(decoder, &report, NULL) == MC_VORBIS_DECODER_OK);
    CHECK(!report.decoder_clear_called && !report.source_close_called &&
          !report.native_retire_called);
}

static bool zeroes(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i)
        if (data[i] != 0)
            return false;
    return true;
}

static bytes decode(const char *root, const char *leaf, bool mus, bool chained) {
    mc_vorbis_decoder *decoder = open_decoder(root, leaf, mus, 1048576u);
    mc_vorbis_decoder_state state;
    CHECK(mc_vorbis_decoder_read_state(decoder, &state));
    CHECK(state.channels == (strcmp(leaf, "stereo.ogg") == 0 ? 2 : 1));
    CHECK(state.sample_rate == (state.channels == 1 ? 32000 : 48000));
    bytes decoded = {0};
    size_t mono_bytes = 0, stereo_bytes = 0;
    int32_t last_link = 0;
    bool finished = false;
    for (size_t calls = 0; calls < 512u; ++calls) {
        mc_vorbis_chunk chunk;
        CHECK(mc_vorbis_decoder_read(decoder, &chunk, NULL) == MC_VORBIS_DECODER_OK);
        CHECK(chunk.logical_link >= last_link && chunk.logical_link <= (chained ? 1 : 0));
        if (chunk.eof) {
            CHECK(chunk.bytes == 0 && chunk.pcm_le == NULL && chunk.total_bytes == decoded.length);
            const size_t source_bytes = chunk.source_bytes;
            CHECK(mc_vorbis_decoder_read(decoder, &chunk, NULL) == MC_VORBIS_DECODER_OK &&
                  chunk.eof);
            CHECK(chunk.pcm_le == NULL && chunk.bytes == 0 && chunk.source_bytes == source_bytes);
            finished = true;
            break;
        }
        CHECK(chunk.bytes > 0 && chunk.pcm_le != NULL &&
              chunk.bytes <= MC_VORBIS_DECODER_CHUNK_BYTES);
        CHECK(zeroes(chunk.pcm_le, chunk.bytes));
        const bool stereo = strcmp(leaf, "stereo.ogg") == 0 || chunk.logical_link == 1;
        CHECK(chunk.channels == (stereo ? 2 : 1) && chunk.sample_rate == (stereo ? 48000 : 32000));
        CHECK(chunk.bytes % (stereo ? 4u : 2u) == 0);
        if (stereo)
            stereo_bytes += chunk.bytes;
        else
            mono_bytes += chunk.bytes;
        append(&decoded, chunk.pcm_le, chunk.bytes);
        CHECK(chunk.total_bytes == decoded.length);
        last_link = chunk.logical_link;
    }
    CHECK(finished);
    CHECK(mono_bytes == (strcmp(leaf, "stereo.ogg") == 0 ? 0u : 6400u));
    CHECK(stereo_bytes == (chained || strcmp(leaf, "stereo.ogg") == 0 ? 19200u : 0u));
    CHECK(mc_vorbis_decoder_read_state(decoder, &state) && state.eof && !state.failed);
    CHECK(state.total_bytes == decoded.length && state.source_bytes > 0);
    retire(&decoder);
    return decoded;
}

static void failures(const char *root) {
    mc_vorbis_error error;
    mc_sound_file *file = resolve(root, "mono.ogg");
    mc_vorbis_decoder *sentinel = open_decoder(root, "mono.ogg", false, 1048576u);
    mc_vorbis_decoder *out = sentinel;
    CHECK(mc_vorbis_decoder_open(file, false, 0, 1000u, &out, &error) ==
          MC_VORBIS_DECODER_INVALID_ARGUMENT);
    CHECK(out == sentinel);
    CHECK(mc_vorbis_decoder_open(file, false, MC_SOUND_STREAM_MAX_BYTES, 0, &out, NULL) ==
              MC_VORBIS_DECODER_INVALID_ARGUMENT &&
          out == sentinel);
    CHECK(mc_vorbis_decoder_open(file, false, MC_SOUND_STREAM_MAX_BYTES,
                                 (size_t)MC_VORBIS_DECODER_MAX_PCM_BYTES + 1u, &out,
                                 NULL) == MC_VORBIS_DECODER_INVALID_ARGUMENT &&
          out == sentinel);
    CHECK(mc_vorbis_decoder_open(NULL, false, 1000u, 1000u, &out, NULL) ==
              MC_VORBIS_DECODER_INVALID_ARGUMENT &&
          out == sentinel);
    CHECK(mc_vorbis_decoder_open(file, false, 1u, 1000u, &out, &error) ==
          MC_VORBIS_DECODER_STREAM_FAILED);
    CHECK(out == sentinel && error.stream.status == MC_SOUND_LIMIT &&
          !error.cleanup.decoder_clear_called);
    mc_sound_file_free(file);
    const char *bad[] = {"malformed.ogg", "truncated.ogg", "stream sample.mus"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        file = resolve(root, bad[i]);
        CHECK(mc_vorbis_decoder_open(file, false, MC_SOUND_STREAM_MAX_BYTES, 1000u, &out, &error) ==
              MC_VORBIS_DECODER_DECODE_FAILED);
        CHECK(out == sentinel && error.vorbis_code < 0 && error.cleanup.source_close_called &&
              error.cleanup.native_retire_called && !error.cleanup.decoder_clear_called);
        mc_sound_file_free(file);
    }
    retire(&sentinel);
    file = resolve(root, "missing.ogg");
    out = NULL;
    CHECK(mc_vorbis_decoder_open(file, false, 1000u, 1000u, &out, &error) ==
          MC_VORBIS_DECODER_STREAM_FAILED);
    CHECK(out == NULL && error.stream.status == MC_SOUND_IO_FAILED &&
          !error.cleanup.decoder_clear_called && !error.cleanup.source_close_called &&
          !error.cleanup.native_retire_called);
    mc_sound_file_free(file);
    mc_vorbis_decoder *decoder = open_decoder(root, "mono.ogg", false, 509u);
    mc_vorbis_chunk chunk = {.bytes = 123u};
    CHECK(mc_vorbis_decoder_read(decoder, &chunk, &error) == MC_VORBIS_DECODER_LIMIT);
    CHECK(chunk.bytes == 0 && chunk.pcm_le == NULL && !chunk.eof && chunk.total_bytes == 0);
    chunk.bytes = 123u;
    CHECK(mc_vorbis_decoder_read(decoder, &chunk, &error) == MC_VORBIS_DECODER_FAILED_STATE &&
          chunk.bytes == 123u);
    mc_vorbis_decoder_state state;
    CHECK(mc_vorbis_decoder_read_state(decoder, &state) && state.failed &&
          state.terminal_status == MC_VORBIS_DECODER_LIMIT && state.total_bytes == 0);
    retire(&decoder);
    decoder = open_decoder(root, "mono.ogg", false, 1100u);
    bool limited = false;
    size_t prefix = 0;
    for (size_t i = 0; i < 32u; ++i) {
        const mc_vorbis_decoder_status status = mc_vorbis_decoder_read(decoder, &chunk, &error);
        CHECK(chunk.total_bytes <= 1100u && zeroes(chunk.pcm_le, chunk.bytes));
        prefix += chunk.bytes;
        if (status != MC_VORBIS_DECODER_OK) {
            CHECK(status == MC_VORBIS_DECODER_LIMIT && !chunk.eof);
            limited = true;
            break;
        }
    }
    CHECK(limited && prefix > 0 && prefix <= 1100u);
    CHECK(mc_vorbis_decoder_read_state(decoder, &state) && state.total_bytes == prefix);
    retire(&decoder);
    decoder = open_decoder(root, "hole.ogg", false, 1048576u);
    bool rejected_hole = false;
    for (size_t i = 0; i < 128u; ++i) {
        const mc_vorbis_decoder_status status = mc_vorbis_decoder_read(decoder, &chunk, &error);
        if (status != MC_VORBIS_DECODER_OK) {
            CHECK(status == MC_VORBIS_DECODER_DECODE_FAILED && error.vorbis_code == OV_HOLE);
            CHECK(mc_vorbis_decoder_read_state(decoder, &state) && state.failed && !state.eof);
            rejected_hole = true;
            break;
        }
        CHECK(!chunk.eof && zeroes(chunk.pcm_le, chunk.bytes));
    }
    CHECK(rejected_hole);
    retire(&decoder);
    chunk.bytes = 123u;
    CHECK(mc_vorbis_decoder_read(NULL, &chunk, NULL) == MC_VORBIS_DECODER_INVALID_ARGUMENT &&
          chunk.bytes == 123u);
    CHECK(!mc_vorbis_decoder_read_state(NULL, &state));
    mc_vorbis_retirement retired = {.vorbis_clear_code = 123};
    CHECK(mc_vorbis_decoder_retire(NULL, &retired, NULL) == MC_VORBIS_DECODER_INVALID_ARGUMENT &&
          retired.vorbis_clear_code == 123);
    decoder = NULL;
    CHECK(mc_vorbis_decoder_retire(&decoder, &retired, NULL) == MC_VORBIS_DECODER_OK &&
          !retired.decoder_clear_called);
    decoder = open_decoder(root, "mono.ogg", false, 1048576u);
    CHECK(mc_vorbis_decoder_read_state(decoder, &state));
    CHECK(mc_vorbis_decoder_read(decoder, NULL, &error) == MC_VORBIS_DECODER_INVALID_ARGUMENT);
    mc_vorbis_decoder_state after;
    CHECK(mc_vorbis_decoder_read_state(decoder, &after) && after.total_bytes == state.total_bytes &&
          after.source_bytes == state.source_bytes && !after.failed);
    retire(&decoder); /* Close without ever requesting PCM. */
    decoder = open_decoder(root, "mono.ogg", false, MC_VORBIS_DECODER_MAX_PCM_BYTES);
    retire(&decoder); /* The admitted ceiling does not allocate that many bytes. */
}

static void chain_budget(const char *root) {
    mc_vorbis_decoder *decoder = open_decoder(root, "chain.ogg", false, 7000u);
    size_t total = 0, mono = 0, stereo = 0;
    int32_t last_link = 0;
    bool limited = false;
    for (size_t calls = 0; calls < 128u; ++calls) {
        mc_vorbis_chunk chunk;
        const mc_vorbis_decoder_status status = mc_vorbis_decoder_read(decoder, &chunk, NULL);
        CHECK(!chunk.eof && chunk.total_bytes <= 7000u);
        if (status != MC_VORBIS_DECODER_OK) {
            CHECK(status == MC_VORBIS_DECODER_LIMIT && chunk.bytes == 0 && chunk.pcm_le == NULL);
            limited = true;
            break;
        }
        CHECK(chunk.bytes > 0 && chunk.pcm_le != NULL && zeroes(chunk.pcm_le, chunk.bytes));
        CHECK(chunk.logical_link >= last_link && chunk.logical_link <= 1);
        if (chunk.logical_link == 0) {
            CHECK(chunk.channels == 1 && chunk.sample_rate == 32000 && chunk.bytes % 2u == 0);
            mono += chunk.bytes;
        } else {
            CHECK(chunk.channels == 2 && chunk.sample_rate == 48000 && chunk.bytes % 4u == 0);
            stereo += chunk.bytes;
        }
        total += chunk.bytes;
        CHECK(chunk.total_bytes == total && total <= 7000u);
        last_link = chunk.logical_link;
        mc_vorbis_decoder_state first, second;
        CHECK(mc_vorbis_decoder_read_state(decoder, &first) &&
              mc_vorbis_decoder_read_state(decoder, &second));
        CHECK(first.total_bytes == second.total_bytes && first.source_bytes == second.source_bytes);
    }
    /* Do not assume packet sizes or exact budget filling: the unchanged 510-byte
     * guard may stop with a smaller valid stereo prefix. */
    CHECK(limited && mono == 6400u && stereo > 0 && total == mono + stereo && total <= 7000u);
    mc_vorbis_decoder_state state;
    CHECK(mc_vorbis_decoder_read_state(decoder, &state) && state.failed && !state.eof &&
          state.terminal_status == MC_VORBIS_DECODER_LIMIT && state.total_bytes == total &&
          state.logical_link == 1 && state.channels == 2 && state.sample_rate == 48000);
    retire(&decoder); /* May discard still-borrowed planes after a capacity-limited slice. */
}

static int check_fixtures(const char *root) {
    pcm_vectors();
    bytes mono = decode(root, "mono.ogg", false, false);
    bytes stereo = decode(root, "stereo.ogg", false, false);
    bytes chain = decode(root, "chain.ogg", false, true);
    bytes mus = decode(root, "stream sample.mus", true, false);
    CHECK(mono.length == mus.length && memcmp(mono.data, mus.data, mono.length) == 0);
    CHECK(chain.length == mono.length + stereo.length &&
          memcmp(chain.data, mono.data, mono.length) == 0 &&
          memcmp(chain.data + mono.length, stereo.data, stereo.length) == 0);
    failures(root);
    chain_budget(root);
    write_file(root, "mono.pcm", &mono);
    write_file(root, "stereo.pcm", &stereo);
    write_file(root, "chain.pcm", &chain);
    write_file(root, "mus.pcm", &mus);
    free(mus.data);
    free(chain.data);
    free(stereo.data);
    free(mono.data);
    if (printf("PASS: %u Vorbis decoder interface checks\n", checks) < 0 || fflush(stdout) != 0 ||
        ferror(stdout))
        return 1;
    return 0;
}

static int run(int argc, const char *mode, const char *root) {
    CHECK(argc == 3 && mode != NULL && root != NULL);
    CHECK(strlen(root) <= MC_SOUND_FILE_MAX_PATH_BYTES);
    if (strcmp(mode, "--create-fixtures") == 0)
        return create_fixtures(root);
    CHECK(strcmp(mode, "--check") == 0);
    return check_fixtures(root);
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    CHECK(argc == 3);
    const int mode_size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[1], -1, NULL, 0, NULL, NULL);
    const int root_size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[2], -1, NULL, 0, NULL, NULL);
    CHECK(mode_size > 0 && mode_size <= 64 && root_size > 0 &&
          (size_t)root_size <= MC_SOUND_FILE_MAX_PATH_BYTES + 1u);
    char *mode = malloc((size_t)mode_size), *root = malloc((size_t)root_size);
    CHECK(mode != NULL && root != NULL);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[1], -1, mode, mode_size, NULL,
                              NULL) == mode_size);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[2], -1, root, root_size, NULL,
                              NULL) == root_size);
    const int status = run(argc, mode, root);
    free(root);
    free(mode);
    return status;
}
#else
int main(int argc, char **argv) {
    CHECK(argc == 3);
    return run(argc, argv[1], argv[2]);
}
#endif
