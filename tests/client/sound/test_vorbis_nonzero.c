/* Authored nonzero encoder/decoder contracts; no original PCM oracle. */
#include "mc/client/sound/vorbis_decoder.h"
#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>

#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static unsigned int checks;

enum { MAX_ENCODED = 1048576, MAX_STEPS = 65536, INPUT_BATCH = 1024, MAX_ROOT = 32768 };

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && sizeof(float) == 4,
               "Input samples require binary32 floats");

typedef struct bytes {
    unsigned char *data;
    size_t length, capacity;
} bytes;

typedef struct recipe {
    const char *name;
    int channels;
    long rate;
    uint32_t frames, pattern;
    int serial;
    const char *title;
} recipe;

static const recipe inputs[] = {
    {"mono32k.ogg", 1, 32000L, 4096u, 0u, 0x13572468, "authored nonzero mono 32k"},
    {"stereo48k.ogg", 2, 48000L, 4800u, 1u, 0x24681357, "authored nonzero stereo 48k"},
    {"mono44k-long.ogg", 1, 44100L, 80000u, 2u, 0x31415926, "authored nonzero long mono 44k"}};

static bool require(bool condition, const char *operation) {
    ++checks;
    if (!condition)
        (void)fprintf(stderr, "Nonzero Vorbis check failed: %s\n", operation);
    return condition;
}

/* Every conversion and scale below is exact binary32 arithmetic. */
static float sample(uint32_t frame, uint32_t channel, uint32_t pattern) {
    const uint32_t phase = (frame + channel * 53u + pattern * 29u) % 257u;
    int ramp = (int)phase - 128;
    if (((frame / 211u + channel + pattern) & 1u) != 0u)
        ramp = -ramp;
    float value = (float)ramp * 0x1p-9f;
    const uint32_t amplitude = (frame / 127u + channel + pattern) % 3u;
    if (amplitude == 1u)
        value *= 0x1p-1f;
    else if (amplitude == 2u)
        value *= 0x1p-2f;
    const uint32_t impulse = (frame + channel * 17u + pattern * 31u) % 997u;
    if (impulse == 0u)
        value = channel == 0u ? 0x1.cp-1f : -0x1.8p-1f;
    else if (impulse == 1u)
        value = channel == 0u ? -0x1.8p-1f : 0x1.cp-1f;
    return value;
}

static bool append(bytes *out, const unsigned char *data, size_t length) {
    if (!require(length <= MAX_ENCODED && out->length <= MAX_ENCODED - length,
                 "fixture byte budget"))
        return false;
    if (length == 0)
        return true;
    if (!require(data != NULL, "nonempty encoded segment"))
        return false;
    const size_t needed = out->length + length;
    if (needed > out->capacity) {
        size_t capacity = out->capacity == 0 ? 4096u : out->capacity;
        while (capacity < needed)
            capacity = capacity > MAX_ENCODED / 2u ? MAX_ENCODED : capacity * 2u;
        unsigned char *replacement = realloc(out->data, capacity);
        if (!require(replacement != NULL, "encoded buffer allocation"))
            return false;
        out->data = replacement;
        out->capacity = capacity;
    }
    memcpy(out->data + out->length, data, length);
    out->length = needed;
    return true;
}

static bool step(uint32_t *steps) {
    if (!require(*steps < MAX_STEPS, "encoder progress budget"))
        return false;
    ++*steps;
    return true;
}

static bool emit_pages(ogg_stream_state *stream, bytes *out, uint32_t *steps, bool *eos,
                       bool force) {
    for (;;) {
        ogg_page output;
        if (!step(steps))
            return false;
        const int ready =
            force ? ogg_stream_flush(stream, &output) : ogg_stream_pageout(stream, &output);
        if (ready == 0)
            return true;
        if (!require(ready == 1 && output.header_len >= 0 && output.body_len >= 0,
                     "Ogg page output"))
            return false;
        if (!append(out, output.header, (size_t)output.header_len) ||
            !append(out, output.body, (size_t)output.body_len))
            return false;
        *eos = *eos || ogg_page_eos(&output) != 0;
    }
}

static bool drain(vorbis_dsp_state *analysis, vorbis_block *block, ogg_stream_state *stream,
                  bytes *out, uint32_t *steps, bool *eos) {
    for (;;) {
        if (!step(steps))
            return false;
        const int ready = vorbis_analysis_blockout(analysis, block);
        if (ready == 0)
            return true;
        if (!require(ready == 1, "Vorbis block extraction") ||
            !require(vorbis_analysis(block, NULL) == 0, "Vorbis analysis") ||
            !require(vorbis_bitrate_addblock(block) == 0, "Vorbis bitrate block"))
            return false;
        for (;;) {
            ogg_packet packet;
            if (!step(steps))
                return false;
            const int packet_ready = vorbis_bitrate_flushpacket(analysis, &packet);
            if (packet_ready == 0)
                break;
            if (!require(packet_ready == 1, "Vorbis packet extraction") ||
                !require(ogg_stream_packetin(stream, &packet) == 0, "Ogg audio packet") ||
                !emit_pages(stream, out, steps, eos, false))
                return false;
        }
    }
}

static bool encode(const recipe *input, bytes *out) {
    vorbis_info info;
    vorbis_info_init(&info);
    vorbis_comment comment;
    vorbis_comment_init(&comment);
    vorbis_dsp_state analysis = {0};
    vorbis_block block = {0};
    ogg_stream_state stream = {0};
    bool analysis_ready = false, block_ready = false, stream_ready = false;
    bool ok = false, eos = false;
    uint32_t steps = 0;

    if (!require(vorbis_encode_init_vbr(&info, input->channels, input->rate, 0x1p-2f) == 0,
                 "Vorbis encoder configuration"))
        goto done;
    vorbis_comment_add_tag(&comment, "TITLE", input->title);
    const char *title = vorbis_comment_query(&comment, "TITLE", 0);
    if (!require(title != NULL && strcmp(title, input->title) == 0, "authored comment"))
        goto done;
    if (!require(vorbis_analysis_init(&analysis, &info) == 0, "Vorbis analysis owner"))
        goto done;
    analysis_ready = true;
    if (!require(vorbis_block_init(&analysis, &block) == 0, "Vorbis block owner"))
        goto done;
    block_ready = true;
    if (!require(ogg_stream_init(&stream, input->serial) == 0, "Ogg stream owner"))
        goto done;
    stream_ready = true;

    ogg_packet identification, text, setup;
    if (!require(vorbis_analysis_headerout(&analysis, &comment, &identification, &text, &setup) ==
                     0,
                 "Vorbis headers") ||
        !require(ogg_stream_packetin(&stream, &identification) == 0, "identification packet") ||
        !require(ogg_stream_packetin(&stream, &text) == 0, "comment packet") ||
        !require(ogg_stream_packetin(&stream, &setup) == 0, "setup packet") ||
        !emit_pages(&stream, out, &steps, &eos, true))
        goto done;

    uint32_t frame = 0;
    while (frame < input->frames) {
        const uint32_t count =
            input->frames - frame > INPUT_BATCH ? INPUT_BATCH : input->frames - frame;
        float **pcm = vorbis_analysis_buffer(&analysis, (int)count);
        if (!require(pcm != NULL, "Vorbis input buffer"))
            goto done;
        for (int channel = 0; channel < input->channels; ++channel) {
            if (!require(pcm[channel] != NULL, "Vorbis channel input buffer"))
                goto done;
            for (uint32_t i = 0; i < count; ++i)
                pcm[channel][i] = sample(frame + i, (uint32_t)channel, input->pattern);
        }
        if (!require(vorbis_analysis_wrote(&analysis, (int)count) == 0, "Vorbis input commit") ||
            !drain(&analysis, &block, &stream, out, &steps, &eos))
            goto done;
        frame += count;
    }
    if (!require(vorbis_analysis_wrote(&analysis, 0) == 0, "Vorbis input EOF") ||
        !drain(&analysis, &block, &stream, out, &steps, &eos) ||
        !require(eos && out->length != 0, "complete Ogg EOS output"))
        goto done;
    ok = true;

done:
    if (stream_ready && !require(ogg_stream_clear(&stream) == 0, "Ogg owner retirement"))
        ok = false;
    if (block_ready && !require(vorbis_block_clear(&block) == 0, "Vorbis block retirement"))
        ok = false;
    if (analysis_ready)
        vorbis_dsp_clear(&analysis);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    return ok;
}

static char *path(const char *root, const char *leaf) {
    const size_t root_length = strlen(root), leaf_length = strlen(leaf);
    if (!require(root_length <= MAX_ROOT && leaf_length <= 64u, "bounded fixture path"))
        return NULL;
    const size_t size = root_length + leaf_length + 2u;
    char *name = malloc(size);
    if (!require(name != NULL, "fixture path allocation"))
        return NULL;
    const int count = snprintf(name, size, "%s/%s", root, leaf);
    if (!require(count >= 0 && (size_t)count + 1u == size, "complete fixture path")) {
        free(name);
        return NULL;
    }
    return name;
}

static FILE *open_output(const char *root, const char *leaf) {
    char *name = path(root, leaf);
    if (name == NULL)
        return NULL;
    FILE *output = NULL;
#ifdef _WIN32
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    wchar_t *wide = count > 0 ? malloc((size_t)count * sizeof(*wide)) : NULL;
    if (require(wide != NULL, "Unicode output path allocation") &&
        require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, count) == count,
                "Unicode output path"))
        (void)_wfopen_s(&output, wide, L"wbx");
    free(wide);
#else
    output = fopen(name, "wbx");
#endif
    free(name);
    (void)require(output != NULL, "exclusive fixture output");
    return output;
}

static bool write_bytes(FILE *output, const bytes *input) {
    if (input->length == 0)
        return true;
    return require(fwrite(input->data, 1u, input->length, output) == input->length &&
                       ferror(output) == 0,
                   "fixture output write");
}

static bool write_file(const char *root, const char *leaf, const bytes *input) {
    FILE *output = open_output(root, leaf);
    if (output == NULL)
        return false;
    bool ok = write_bytes(output, input);
    if (!require(fclose(output) == 0, "fixture output close"))
        ok = false;
    return ok;
}

static int create_fixtures(const char *root) {
    bytes encoded[3] = {{0}};
    FILE *outputs[4] = {NULL};
    bool ok = false;
    for (size_t i = 0; i < 3u; ++i)
        if (!encode(&inputs[i], &encoded[i]))
            goto done;
    static const char *const names[] = {"mono32k.ogg", "stereo48k.ogg", "chain.ogg",
                                        "mono44k-long.ogg"};
    for (size_t i = 0; i < 4u; ++i) {
        outputs[i] = open_output(root, names[i]);
        if (outputs[i] == NULL)
            goto done;
    }
    if (!write_bytes(outputs[0], &encoded[0]) || !write_bytes(outputs[1], &encoded[1]) ||
        !write_bytes(outputs[2], &encoded[0]) || !write_bytes(outputs[2], &encoded[1]) ||
        !write_bytes(outputs[3], &encoded[2]))
        goto done;
    ok = true;
done:
    for (size_t i = 0; i < 4u; ++i) {
        FILE *output = outputs[i];
        outputs[i] = NULL;
        if (output != NULL && !require(fclose(output) == 0, "encoded fixture close"))
            ok = false;
    }
    for (size_t i = 0; i < 3u; ++i)
        free(encoded[i].data);
    if (ok && (printf("PASS: 4 authored nonzero Vorbis fixtures\n") < 0 || fflush(stdout) != 0 ||
               ferror(stdout)))
        ok = false;
    return ok ? 0 : 1;
}

static bool nonzero(const bytes *value) {
    for (size_t i = 0; i < value->length; ++i)
        if (value->data[i] != 0)
            return true;
    return false;
}

static bool decode(const char *root, const char *leaf, int channels, int64_t rate,
                   size_t expected_bytes, bool chained, bytes *out) {
    char *name = path(root, leaf);
    mc_sound_file *file = NULL;
    mc_vorbis_decoder *decoder = NULL;
    bool ok = false, opened = false, finished = false;
    size_t link_bytes[2] = {0, 0}, previous_source = 0;
    int32_t last_link = 0;
    if (name == NULL)
        goto done;
    if (!require(mc_sound_file_resolve(name, &file, NULL) == MC_SOUND_OK && file != NULL,
                 "resolve authored Ogg file"))
        goto done;
    if (!require(mc_vorbis_decoder_open(file, false, 8u * MAX_ENCODED, MAX_ENCODED, &decoder,
                                        NULL) == MC_VORBIS_DECODER_OK &&
                     decoder != NULL,
                 "open actual sequential decoder"))
        goto done;
    opened = true;
    mc_sound_file_free(file);
    file = NULL;
    mc_vorbis_decoder_state initial = {0};
    if (!require(mc_vorbis_decoder_read_state(decoder, &initial) && initial.logical_link == 0 &&
                     initial.channels == channels && initial.sample_rate == rate &&
                     initial.total_bytes == 0 && initial.source_bytes > 0 && !initial.eof &&
                     !initial.failed && initial.pcm_budget == MAX_ENCODED,
                 "actual initial decoder format/state"))
        goto done;
    previous_source = initial.source_bytes;
    for (size_t calls = 0; calls < 8192u; ++calls) {
        mc_vorbis_chunk chunk = {0};
        const mc_vorbis_decoder_status status = mc_vorbis_decoder_read(decoder, &chunk, NULL);
        if (!require(chunk.bytes <= MC_VORBIS_DECODER_CHUNK_BYTES &&
                         (chunk.bytes == 0 || chunk.pcm_le != NULL),
                     "bounded published PCM prefix"))
            goto done;
        if (chunk.bytes != 0 && !append(out, chunk.pcm_le, chunk.bytes))
            goto done;
        if (!require(status == MC_VORBIS_DECODER_OK, "actual decoder read status") ||
            !require(chunk.logical_link >= last_link && chunk.logical_link <= (chained ? 1 : 0) &&
                         chunk.total_bytes == out->length &&
                         chunk.source_bytes >= previous_source &&
                         chunk.source_bytes <= 8u * MAX_ENCODED,
                     "current link and cumulative byte counts"))
            goto done;
        const int expected_channels = chunk.logical_link == 1 ? 2 : channels;
        const int64_t expected_rate = chunk.logical_link == 1 ? 48000 : rate;
        if (!require(chunk.channels == expected_channels && chunk.sample_rate == expected_rate &&
                         chunk.bytes % ((size_t)expected_channels * 2u) == 0,
                     "actual per-link format and complete PCM frames"))
            goto done;
        link_bytes[(size_t)chunk.logical_link] += chunk.bytes;
        previous_source = chunk.source_bytes;
        last_link = chunk.logical_link;
        if (chunk.eof) {
            if (!require(chunk.bytes == 0 && chunk.pcm_le == NULL, "clear actual EOF view"))
                goto done;
            const size_t source_bytes = chunk.source_bytes;
            if (!require(mc_vorbis_decoder_read(decoder, &chunk, NULL) == MC_VORBIS_DECODER_OK &&
                             chunk.eof && chunk.bytes == 0 && chunk.pcm_le == NULL &&
                             chunk.total_bytes == out->length && chunk.source_bytes == source_bytes,
                         "stable repeated EOF"))
                goto done;
            finished = true;
            break;
        }
        if (!require(chunk.bytes > 0, "positive progress before EOF"))
            goto done;
    }
    if (!require(finished && out->length == expected_bytes && nonzero(out),
                 "complete authored nonzero PCM length") ||
        !require(chained ? link_bytes[0] == 8192u && link_bytes[1] == 19200u
                         : link_bytes[0] == expected_bytes && link_bytes[1] == 0,
                 "complete PCM counts for each logical link"))
        goto done;
    mc_vorbis_decoder_state final_state = {0};
    if (!require(mc_vorbis_decoder_read_state(decoder, &final_state) && final_state.eof &&
                     !final_state.failed && final_state.terminal_status == MC_VORBIS_DECODER_OK &&
                     final_state.vorbis_code == 0 && final_state.total_bytes == out->length &&
                     final_state.source_bytes == previous_source &&
                     final_state.logical_link == last_link &&
                     final_state.channels == (chained ? 2 : channels) &&
                     final_state.sample_rate == (chained ? 48000 : rate),
                 "actual final decoder EOF/state"))
        goto done;
    ok = true;
done:
    free(name);
    if (file != NULL)
        mc_sound_file_free(file);
    mc_vorbis_retirement retired = {0};
    if (!require(mc_vorbis_decoder_retire(&decoder, &retired, NULL) == MC_VORBIS_DECODER_OK &&
                     decoder == NULL,
                 "decoder owner retirement"))
        ok = false;
    if (opened && !require(retired.decoder_clear_called && retired.source_close_called &&
                               retired.native_retire_called && retired.vorbis_clear_code == 0 &&
                               retired.source_close.status == MC_SOUND_OK &&
                               retired.native_retire.status == MC_SOUND_OK,
                           "actual decoder/source/native cleanup"))
        ok = false;
    return ok;
}

static bool stereo_channels(const bytes *stereo) {
    bool left_nonzero = false, right_nonzero = false, different = false;
    for (size_t i = 0; i < stereo->length; i += 4u) {
        left_nonzero = left_nonzero || stereo->data[i] != 0 || stereo->data[i + 1u] != 0;
        right_nonzero = right_nonzero || stereo->data[i + 2u] != 0 || stereo->data[i + 3u] != 0;
        different = different || memcmp(stereo->data + i, stereo->data + i + 2u, 2u) != 0;
    }
    return require(left_nonzero && right_nonzero && different, "distinct nonzero stereo planes");
}

static int check_fixtures(const char *root) {
    static const struct {
        const char *input, *output;
        int channels;
        int64_t rate;
        size_t bytes;
        bool chained;
    } cases[] = {{"mono32k.ogg", "mono32k.pcm", 1, 32000, 8192u, false},
                 {"stereo48k.ogg", "stereo48k.pcm", 2, 48000, 19200u, false},
                 {"chain.ogg", "chain.pcm", 1, 32000, 27392u, true},
                 {"mono44k-long.ogg", "mono44k-long.pcm", 1, 44100, 160000u, false}};

    bytes pcm[4] = {{0}};
    bool ok = true;
    for (size_t i = 0; i < 4u; ++i) {
        const bool decoded = decode(root, cases[i].input, cases[i].channels, cases[i].rate,
                                    cases[i].bytes, cases[i].chained, &pcm[i]);
        const bool written = write_file(root, cases[i].output, &pcm[i]);
        if (!decoded || !written) {
            ok = false;
            break;
        }
    }
    if (ok)
        ok = require(pcm[2].length == pcm[0].length + pcm[1].length &&
                         memcmp(pcm[2].data, pcm[0].data, pcm[0].length) == 0 &&
                         memcmp(pcm[2].data + pcm[0].length, pcm[1].data, pcm[1].length) == 0,
                     "chained PCM equals separately decoded parts") &&
             stereo_channels(&pcm[1]);
    for (size_t i = 0; i < 4u; ++i)
        free(pcm[i].data);
    if (ok && (printf("PASS: %u nonzero Vorbis codec checks\n", checks) < 0 ||
               fflush(stdout) != 0 || ferror(stdout)))
        ok = false;
    return ok ? 0 : 1;
}

static int run(const char *mode, const char *root) {
    if (!require(strlen(root) <= MAX_ROOT, "bounded fixture root"))
        return 1;
    if (strcmp(mode, "--create-fixtures") == 0)
        return create_fixtures(root);
    if (!require(strcmp(mode, "--check") == 0, "fixture mode"))
        return 1;
    return check_fixtures(root);
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    if (!require(argc == 3, "mode and root arguments"))
        return 1;
    const int mode_size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[1], -1, NULL, 0, NULL, NULL);
    const int root_size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[2], -1, NULL, 0, NULL, NULL);
    if (!require(mode_size > 0 && mode_size <= 64 && root_size > 0 && root_size <= MAX_ROOT + 1,
                 "bounded Unicode arguments"))
        return 1;
    char *mode = malloc((size_t)mode_size), *root = malloc((size_t)root_size);
    bool ok = require(mode != NULL && root != NULL, "Unicode argument allocation");
    if (ok)
        ok = require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[1], -1, mode,
                                         mode_size, NULL, NULL) == mode_size &&
                         WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[2], -1, root,
                                             root_size, NULL, NULL) == root_size,
                     "Unicode argument conversion");
    const int status = ok ? run(mode, root) : 1;
    free(root);
    free(mode);
    return status;
}
#else
int main(int argc, char **argv) {
    if (!require(argc == 3, "mode and root arguments"))
        return 1;
    return run(argv[1], argv[2]);
}
#endif
