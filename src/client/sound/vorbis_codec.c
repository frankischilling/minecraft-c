#include "mc/client/sound/vorbis_codec.h"

#include "pcm_internal.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(__x86_64__)
#error "This codec profile currently requires little-endian x86-64"
#endif

_Static_assert(sizeof(ogg_int64_t) == sizeof(int64_t), "Ogg sequence profile is signed64");

#define MAX_SOURCE_BUFFER 1048576u
#define MAX_RESULT_BYTES MC_VORBIS_CODEC_MAX_PCM_BYTES
#define MAX_WORK_STEPS 1048576u

struct mc_vorbis_codec {
    mc_vorbis_codec_limits limits;
    mc_vorbis_codec_view view;
    mc_sound_stream *source;
    ogg_sync_state sync;
    ogg_stream_state packets;
    ogg_page page;
    ogg_packet packet;
    vorbis_info info;
    vorbis_comment comment;
    vorbis_dsp_state dsp;
    vorbis_block block;
    float *saved_pcm;
    size_t saved_frames, saved_offset;
    bool sync_ready, packets_ready, info_ready, comment_ready, dsp_ready, block_ready;
    bool active;
    uint32_t work;
};

typedef struct sample_array {
    uint8_t *data;
    size_t bytes;
    bool is_null;
} sample_array;

static mc_sound_error no_stream_error(void) {
    return (mc_sound_error){.status = MC_SOUND_OK, .source_pc = -1};
}

static mc_vorbis_codec_error no_error(void) {
    mc_vorbis_codec_error error = {.status = MC_VORBIS_CODEC_OK};
    error.stream = error.caught_stream = no_stream_error();
    error.cleanup.source_close = error.cleanup.native_retire = no_stream_error();
    error.prior_cleanup.source_close = error.prior_cleanup.native_retire = no_stream_error();
    return error;
}

static bool fail(mc_vorbis_codec_error *error, mc_vorbis_codec_status status, const char *operation,
                 int code, mc_sound_error stream) {
    if (error->status == MC_VORBIS_CODEC_OK) {
        error->status = status;
        error->operation = operation;
        error->library_code = code;
        error->stream = stream;
    }
    return false;
}

static mc_vorbis_codec_status admission(mc_vorbis_codec_error *out, mc_vorbis_codec_status status) {
    if (out != NULL) {
        *out = no_error();
        out->status = status;
        out->operation = "Vorbis codec admission";
    }
    return status;
}

static bool tick(mc_vorbis_codec *codec, mc_vorbis_codec_error *error) {
    if (codec->work >= codec->limits.work_steps)
        return fail(error, MC_VORBIS_CODEC_LIMIT, "Vorbis codec work budget", 0, no_stream_error());
    ++codec->work;
    return true;
}

static mc_vorbis_codec_status stream_status(mc_sound_status status) {
    if (status == MC_SOUND_LIMIT)
        return MC_VORBIS_CODEC_LIMIT;
    if (status == MC_SOUND_ALLOCATION_FAILED)
        return MC_VORBIS_CODEC_ALLOCATION_FAILED;
    return MC_VORBIS_CODEC_STREAM_FAILED;
}

static mc_vorbis_codec_status finish(mc_vorbis_codec *codec, mc_vorbis_codec_error *error,
                                     mc_vorbis_codec_error *out) {
    if (error->status != MC_VORBIS_CODEC_OK) {
        if (!codec->view.failed)
            codec->view.terminal_status = error->status;
        codec->view.failed = true;
    }
    codec->active = false;
    if (out != NULL)
        *out = *error;
    return error->status;
}

static void clear_resources(mc_vorbis_codec *codec, mc_vorbis_codec_cleanup *report,
                            mc_vorbis_codec_error *error) {
    *report = (mc_vorbis_codec_cleanup){.source_close = no_stream_error(),
                                        .native_retire = no_stream_error()};
    if (codec->packets_ready) {
        report->stream_clear = true;
        report->stream_clear_code = ogg_stream_clear(&codec->packets);
        codec->packets_ready = false;
        if (report->stream_clear_code != 0 && error != NULL)
            (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "Ogg stream clear",
                       report->stream_clear_code, no_stream_error());
    }
    if (codec->block_ready) {
        report->block_clear = true;
        report->block_clear_code = vorbis_block_clear(&codec->block);
        codec->block_ready = false;
        if (report->block_clear_code != 0 && error != NULL)
            (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "Vorbis block clear",
                       report->block_clear_code, no_stream_error());
    }
    if (codec->dsp_ready) {
        report->dsp_clear = true;
        vorbis_dsp_clear(&codec->dsp);
        codec->dsp_ready = false;
    }
    free(codec->saved_pcm);
    codec->saved_pcm = NULL;
    codec->saved_frames = codec->saved_offset = 0;
    codec->view.saved_pcm_frames = 0;
    if (codec->info_ready) {
        report->info_clear = true;
        vorbis_info_clear(&codec->info);
        codec->info_ready = false;
    }
    if (codec->comment_ready) {
        report->comment_clear = true;
        vorbis_comment_clear(&codec->comment);
        codec->comment_ready = false;
    }
    if (codec->sync_ready) {
        report->sync_clear = true;
        report->sync_clear_code = ogg_sync_clear(&codec->sync);
        codec->sync_ready = false;
        if (report->sync_clear_code != 0 && error != NULL)
            (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "Ogg sync clear",
                       report->sync_clear_code, no_stream_error());
    }
    if (codec->source != NULL) {
        report->source_close_called = true;
        const mc_sound_status closed = mc_sound_stream_close(codec->source, &report->source_close);
        if (closed != MC_SOUND_OK && error != NULL)
            (void)fail(error, MC_VORBIS_CODEC_STREAM_FAILED, "codec source close", 0,
                       report->source_close);
        report->native_retire_called = true;
        const mc_sound_status retired = mc_sound_stream_free(codec->source, &report->native_retire);
        codec->source = NULL;
        if (retired != MC_SOUND_OK && error != NULL)
            (void)fail(error, MC_VORBIS_CODEC_STREAM_FAILED, "codec native stream retirement", 0,
                       report->native_retire);
    }
}

static char *reserve_input(mc_vorbis_codec *codec, mc_vorbis_codec_error *error) {
    if (!tick(codec, error))
        return NULL;
    if (!codec->sync_ready || codec->view.buffer_size <= 0 || codec->sync.fill < 0) {
        (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec sync buffer state", 0,
                   no_stream_error());
        return NULL;
    }
    char *destination = ogg_sync_buffer(&codec->sync, codec->view.buffer_size);
    if (destination == NULL) {
        (void)fail(error, MC_VORBIS_CODEC_ALLOCATION_FAILED, "codec sync buffer", 0,
                   no_stream_error());
        return NULL;
    }
    codec->view.index = codec->sync.fill;
    return destination;
}

/* Header reads clamp InputStream EOF to zero; readBytes preserves -1. */
static bool refill(mc_vorbis_codec *codec, bool header, int32_t *source_count,
                   mc_vorbis_codec_error *error) {
    char *destination = reserve_input(codec, error);
    if (destination == NULL)
        return false;
    size_t bytes = 0;
    mc_sound_error cause = no_stream_error();
    const mc_sound_status status = mc_sound_stream_read(
        codec->source, (uint8_t *)destination, (size_t)codec->view.buffer_size, &bytes, &cause);
    if (status != MC_SOUND_OK) {
        if (!header && status == MC_SOUND_IO_FAILED) {
            if (!error->source_refill_caught) {
                error->source_refill_caught = true;
                error->caught_stream = cause;
            }
            return true; /* source catch returns this page's already decoded array */
        }
        return fail(error, stream_status(status), "codec source refill", 0, cause);
    }
    codec->view.source_bytes += bytes;
    *source_count = bytes == 0 ? (header ? 0 : -1) : (int32_t)bytes;
    if (!header)
        codec->view.count = *source_count;
    if (*source_count == -1 && codec->sync.fill == 0) {
        /* The original makes fill=-1. Keeping zero avoids invalid native state;
         * the next source pageout still returns zero and then publishes EOS. */
        ++codec->view.avoided_negative_sync_states;
        return true;
    }
    codec->view.last_wrote_code = ogg_sync_wrote(&codec->sync, *source_count);
    if (codec->view.last_wrote_code != 0)
        return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec sync wrote",
                    codec->view.last_wrote_code, no_stream_error());
    return true;
}

static bool page_out(mc_vorbis_codec *codec, int *code, mc_vorbis_codec_error *error) {
    if (!tick(codec, error))
        return false;
    *code = codec->view.last_page_code = ogg_sync_pageout(&codec->sync, &codec->page);
    if (*code == 1)
        ++codec->view.pages;
    return true;
}

static bool packet_out(mc_vorbis_codec *codec, int *code, mc_vorbis_codec_error *error) {
    if (!tick(codec, error))
        return false;
    *code = codec->view.last_packet_code = ogg_stream_packetout(&codec->packets, &codec->packet);
    if (*code == 1)
        ++codec->view.packets;
    return true;
}

static bool headers(mc_vorbis_codec *codec, mc_vorbis_codec_error *error) {
    int32_t count = 0;
    int code = 0;
    if (!refill(codec, true, &count, error) || !page_out(codec, &code, error))
        return false;
    if (code != 1) {
        if (count < codec->view.buffer_size)
            return true; /* source short-header branch; later native format guard */
        return fail(error, MC_VORBIS_CODEC_HEADER_FAILED, "codec first header page", code,
                    no_stream_error());
    }
    code = ogg_stream_init(&codec->packets, ogg_page_serialno(&codec->page));
    if (code != 0)
        return fail(error, MC_VORBIS_CODEC_ALLOCATION_FAILED, "codec packet owner", code,
                    no_stream_error());
    codec->packets_ready = true;
    codec->view.last_pagein_code = ogg_stream_pagein(&codec->packets, &codec->page);
    if (ogg_stream_check(&codec->packets) != 0)
        return fail(error, MC_VORBIS_CODEC_ALLOCATION_FAILED, "codec first packet storage",
                    codec->view.last_pagein_code, no_stream_error());
    if (codec->view.last_pagein_code < 0)
        return fail(error, MC_VORBIS_CODEC_HEADER_FAILED, "codec first header pagein",
                    codec->view.last_pagein_code, no_stream_error());
    if (!packet_out(codec, &code, error))
        return false;
    if (code != 1)
        return fail(error, MC_VORBIS_CODEC_HEADER_FAILED, "codec first header packet", code,
                    no_stream_error());
    codec->view.last_header_code =
        vorbis_synthesis_headerin(&codec->info, &codec->comment, &codec->packet);
    if (codec->view.last_header_code < 0)
        return fail(error, MC_VORBIS_CODEC_HEADER_FAILED, "codec identification header",
                    codec->view.last_header_code, no_stream_error());
    int remaining = 2;
    while (remaining != 0) {
        while (remaining != 0) {
            if (!page_out(codec, &code, error))
                return false;
            if (code == 0)
                break;
            if (code != 1)
                continue;
            codec->view.last_pagein_code = ogg_stream_pagein(&codec->packets, &codec->page);
            if (ogg_stream_check(&codec->packets) != 0)
                return fail(error, MC_VORBIS_CODEC_ALLOCATION_FAILED, "codec header packet storage",
                            codec->view.last_pagein_code, no_stream_error());
            while (remaining != 0) {
                if (!packet_out(codec, &code, error))
                    return false;
                if (code == 0)
                    break;
                if (code == -1)
                    return fail(error, MC_VORBIS_CODEC_HEADER_FAILED, "codec secondary packet",
                                code, no_stream_error());
                codec->view.last_header_code =
                    vorbis_synthesis_headerin(&codec->info, &codec->comment, &codec->packet);
                --remaining; /* source ignores secondary headerin's return */
            }
        }
        /* This refill occurs even when the last secondary header just arrived. */
        if (!refill(codec, true, &count, error))
            return false;
        if (count == 0 && remaining != 0)
            return fail(error, MC_VORBIS_CODEC_HEADER_FAILED, "codec incomplete headers", 0,
                        no_stream_error());
    }
    return reserve_input(codec, error) != NULL;
}

mc_vorbis_codec_status mc_vorbis_codec_create(mc_vorbis_codec_limits limits, mc_vorbis_codec **out,
                                              mc_vorbis_codec_error *error) {
    if (out == NULL || limits.input_bytes == 0 || limits.input_bytes > MC_SOUND_STREAM_MAX_BYTES ||
        limits.pcm_bytes == 0 || limits.pcm_bytes > MAX_RESULT_BYTES || limits.work_steps == 0 ||
        limits.work_steps > MAX_WORK_STEPS)
        return admission(error, MC_VORBIS_CODEC_INVALID_ARGUMENT);
    mc_vorbis_codec *codec = calloc(1, sizeof(*codec));
    if (codec == NULL)
        return admission(error, MC_VORBIS_CODEC_ALLOCATION_FAILED);
    codec->limits = limits;
    *out = codec;
    if (error != NULL)
        *error = no_error();
    return MC_VORBIS_CODEC_OK;
}

mc_vorbis_codec_status mc_vorbis_codec_initialize(mc_vorbis_codec *codec,
                                                  const mc_sound_file_identity *identity, bool mus,
                                                  int32_t streaming_size, bool *initialized,
                                                  mc_vorbis_codec_error *out) {
    if (codec == NULL || identity == NULL || initialized == NULL)
        return admission(out, MC_VORBIS_CODEC_INVALID_ARGUMENT);
    if (codec->active)
        return admission(out, MC_VORBIS_CODEC_BUSY);
    codec->active = true;
    codec->work = 0;
    mc_vorbis_codec_error error = no_error();
    codec->view.initialized = false;
    clear_resources(codec, &error.prior_cleanup, NULL);
    const mc_vorbis_codec_format retained_format = codec->view.format;
    const bool retained_eos = codec->view.end_of_stream;
    const int32_t retained_converted = codec->view.converted_buffer_size;
    codec->view = (mc_vorbis_codec_view){.format = retained_format,
                                         .end_of_stream = retained_eos,
                                         .converted_buffer_size = retained_converted,
                                         .buffer_size = streaming_size / 2};
    *initialized = false;
    if (codec->view.buffer_size <= 0 || (uint32_t)codec->view.buffer_size > MAX_SOURCE_BUFFER) {
        (void)fail(&error, MC_VORBIS_CODEC_LIMIT, "codec source buffer guard", 0,
                   no_stream_error());
        goto done;
    }
    vorbis_info_init(&codec->info);
    codec->info_ready = true;
    vorbis_comment_init(&codec->comment);
    codec->comment_ready = true;
    if (codec->info.codec_setup == NULL) {
        (void)fail(&error, MC_VORBIS_CODEC_ALLOCATION_FAILED, "codec info allocation", 0,
                   no_stream_error());
        goto done;
    }
    mc_sound_error cause = no_stream_error();
    const mc_sound_status opened = mc_sound_stream_open_identity(
        identity, mus, codec->limits.input_bytes, &codec->source, &cause);
    if (opened != MC_SOUND_OK) {
        (void)fail(&error, stream_status(opened), "codec source open", 0, cause);
        clear_resources(codec, &error.cleanup, &error);
        goto done;
    }
    codec->view.end_of_stream = false;
    const int sync_code = ogg_sync_init(&codec->sync);
    if (sync_code != 0) {
        (void)fail(&error, MC_VORBIS_CODEC_SOURCE_STATE, "codec sync initialize", sync_code,
                   no_stream_error());
        goto done;
    }
    codec->sync_ready = true;
    if (reserve_input(codec, &error) == NULL || !headers(codec, &error))
        goto done;
    codec->view.converted_buffer_size = codec->view.buffer_size * 2;
    if (codec->info.channels <= 0 || codec->info.channels > 255 || codec->info.rate <= 0) {
        (void)fail(&error, MC_VORBIS_CODEC_SOURCE_STATE, "codec missing/unsafe header format", 0,
                   no_stream_error());
        goto done;
    }
#if LONG_MAX > INT32_MAX
    if (codec->info.rate > INT32_MAX) {
        (void)fail(&error, MC_VORBIS_CODEC_SOURCE_STATE, "codec signed source rate guard", 0,
                   no_stream_error());
        goto done;
    }
#endif
    const int synthesis = vorbis_synthesis_init(&codec->dsp, &codec->info);
    if (synthesis != 0) {
        (void)fail(&error, MC_VORBIS_CODEC_HEADER_FAILED, "codec synthesis initialize", synthesis,
                   no_stream_error());
        goto done;
    }
    codec->dsp_ready = true;
    const int block = vorbis_block_init(&codec->dsp, &codec->block);
    if (block != 0) {
        (void)fail(&error, MC_VORBIS_CODEC_SOURCE_STATE, "codec block initialize", block,
                   no_stream_error());
        goto done;
    }
    codec->block_ready = true;
    codec->view.format = (mc_vorbis_codec_format){
        .present = true, .channels = codec->info.channels, .sample_rate = codec->info.rate};
    volatile float rate = (float)(int32_t)codec->info.rate;
    float stored_rate = rate;
    _Static_assert(sizeof(stored_rate) == sizeof(uint32_t),
                   "Codec format requires binary32 storage");
    memcpy(&codec->view.format.sample_rate_bits, &stored_rate, sizeof(stored_rate));
    codec->view.granule_position = codec->dsp.granulepos;
    codec->view.initialized = true;
    *initialized = true;
done:
    return finish(codec, &error, out);
}

static bool allocate_array(mc_vorbis_codec *codec, size_t bytes, size_t ceiling, sample_array *out,
                           mc_vorbis_codec_error *error) {
    if (bytes > ceiling)
        return fail(error, MC_VORBIS_CODEC_LIMIT, "codec array budget", 0, no_stream_error());
    if (!tick(codec, error))
        return false;
    uint8_t *data = calloc(bytes == 0 ? 1u : bytes, 1u);
    if (data == NULL)
        return fail(error, MC_VORBIS_CODEC_ALLOCATION_FAILED, "codec array allocation", 0,
                    no_stream_error());
    *out = (sample_array){.data = data, .bytes = bytes, .is_null = false};
    return true;
}

static bool append_arrays(mc_vorbis_codec *codec, sample_array *left, const sample_array *right,
                          mc_vorbis_codec_error *error) {
    if (!tick(codec, error))
        return false;
    if (left->is_null && right->is_null)
        return true;
    if (right->bytes > codec->limits.pcm_bytes ||
        left->bytes > codec->limits.pcm_bytes - right->bytes)
        return fail(error, MC_VORBIS_CODEC_LIMIT, "codec append budget", 0, no_stream_error());
    sample_array combined = {.is_null = true};
    if (!allocate_array(codec, left->bytes + right->bytes, codec->limits.pcm_bytes, &combined,
                        error))
        return false;
    if (left->bytes != 0)
        memcpy(combined.data, left->data, left->bytes);
    if (right->bytes != 0)
        memcpy(combined.data + left->bytes, right->data, right->bytes);
    free(left->data);
    *left = combined;
    return true;
}

/* libvorbis requires the preceding PCM to be consumed before blockin. JOrbis
 * appends new blocks while old PCM remains unread. Save those exact samples
 * before consuming the library view, then convert this prefix first. */
static bool save_unread_pcm(mc_vorbis_codec *codec, mc_vorbis_codec_error *error) {
    if (!tick(codec, error))
        return false;
    float **planes = NULL;
    const int available = vorbis_synthesis_pcmout(&codec->dsp, &planes);
    if (available < 0)
        return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec pending PCM state", available,
                    no_stream_error());
    if (available == 0)
        return true;
    const size_t channels = (size_t)codec->view.format.channels;
    const size_t held = codec->saved_frames - codec->saved_offset;
    const size_t ceiling = MC_VORBIS_CODEC_MAX_SAVED_PCM_BYTES / (channels * sizeof(float));
    if ((size_t)available > ceiling || held > ceiling - (size_t)available)
        return fail(error, MC_VORBIS_CODEC_LIMIT, "codec saved PCM budget", 0, no_stream_error());
    if (planes == NULL)
        return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec pending PCM planes", 0,
                    no_stream_error());
    for (size_t channel = 0; channel < channels; ++channel)
        if (planes[channel] == NULL)
            return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec pending PCM plane", 0,
                        no_stream_error());
    const size_t total = held + (size_t)available;
    float *saved = malloc(total * channels * sizeof(*saved));
    if (saved == NULL)
        return fail(error, MC_VORBIS_CODEC_ALLOCATION_FAILED, "codec saved PCM allocation", 0,
                    no_stream_error());
    if (held != 0)
        memcpy(saved, codec->saved_pcm + codec->saved_offset * channels,
               held * channels * sizeof(*saved));
    for (size_t frame = 0; frame < (size_t)available; ++frame)
        for (size_t channel = 0; channel < channels; ++channel)
            memcpy(saved + (held + frame) * channels + channel, planes[channel] + frame,
                   sizeof(*saved));
    free(codec->saved_pcm);
    codec->saved_pcm = saved;
    codec->saved_frames = total;
    codec->saved_offset = 0;
    codec->view.saved_pcm_frames = total;
    codec->view.saved_pcm_frames_copied += (size_t)available;
    if (total > codec->view.saved_pcm_peak_frames)
        codec->view.saved_pcm_peak_frames = total;
    codec->view.last_pcm_read_code = vorbis_synthesis_read(&codec->dsp, available);
    if (codec->view.last_pcm_read_code != 0)
        return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec pending PCM consumption",
                    codec->view.last_pcm_read_code, no_stream_error());
    return true;
}

static void consume_saved_pcm(mc_vorbis_codec *codec, size_t frames) {
    codec->saved_offset += frames;
    codec->view.saved_pcm_frames = codec->saved_frames - codec->saved_offset;
    if (codec->saved_offset == codec->saved_frames) {
        free(codec->saved_pcm);
        codec->saved_pcm = NULL;
        codec->saved_frames = codec->saved_offset = 0;
    }
}

static bool synthesis_block(mc_vorbis_codec *codec, mc_vorbis_codec_error *error) {
    const ogg_int64_t actual_granule = codec->block.granulepos;
    if (codec->dsp.sequence == INT64_MAX || actual_granule < -1 || actual_granule > INT32_MAX)
        return fail(error, MC_VORBIS_CODEC_LIMIT, "codec sequence/granule guard", 0,
                    no_stream_error());
    const bool discontinuity =
        codec->dsp.sequence == -1 || codec->dsp.sequence + 1 != codec->block.sequence;
    const bool first_known = actual_granule != -1 && (codec->dsp.granulepos == -1 || discontinuity);
    if (codec->dsp.W < 0 || codec->dsp.W > 1 || codec->block.W < 0 || codec->block.W > 1)
        return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec block mode guard", 0,
                    no_stream_error());
    const long previous_size = vorbis_info_blocksize(&codec->info, (int)codec->dsp.W);
    const long current_size = vorbis_info_blocksize(&codec->info, (int)codec->block.W);
    if (previous_size <= 0 || current_size <= 0 || previous_size > 32768 || current_size > 32768 ||
        (!discontinuity && codec->dsp.granulepos != -1 &&
         codec->dsp.granulepos > INT32_MAX - previous_size / 4 - current_size / 4))
        return fail(error, MC_VORBIS_CODEC_LIMIT, "codec granule arithmetic guard", 0,
                    no_stream_error());
    if (!save_unread_pcm(codec, error))
        return false;
    size_t saved_end_trim = 0;
    const ogg_int64_t advance = previous_size / 4 + current_size / 4;
    if (!first_known && !discontinuity && codec->dsp.granulepos != -1 && actual_granule != -1 &&
        codec->block.eofflag != 0 && codec->dsp.granulepos + advance > actual_granule) {
        const ogg_int64_t extra = codec->dsp.granulepos + advance - actual_granule;
        /* C blockin can trim only its newly produced frames. JOrbis's unread
         * tail includes our saved prefix, so an excess end trim reaches it. */
        if (extra > advance) {
            const ogg_int64_t held_trim = extra - advance;
            if ((uint64_t)held_trim > codec->saved_frames - codec->saved_offset)
                return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec end granule range", 0,
                            no_stream_error());
            saved_end_trim = (size_t)held_trim;
        }
    }
    /* JOrbis assigns its first known granule without trimming. Suppress only
     * libvorbis's extra first-known trimming branch, then publish the actual
     * packet value. All overlap/add and synthesis stay in the real library. */
    if (first_known)
        codec->block.granulepos = -1;
    codec->view.last_blockin_code = vorbis_synthesis_blockin(&codec->dsp, &codec->block);
    codec->block.granulepos = actual_granule;
    if (codec->view.last_blockin_code != 0)
        return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec synthesis block admission",
                    codec->view.last_blockin_code, no_stream_error());
    if (first_known) {
        codec->dsp.granulepos = actual_granule;
        ++codec->view.first_granule_transitions;
    }
    if (saved_end_trim != 0) {
        codec->saved_frames -= saved_end_trim;
        codec->view.saved_pcm_end_trimmed_frames += saved_end_trim;
        consume_saved_pcm(codec, 0);
    }
    codec->view.granule_position = codec->dsp.granulepos;
    return true;
}

static bool decode_packet(mc_vorbis_codec *codec, sample_array *out, mc_vorbis_codec_error *error) {
    const size_t capacity = (size_t)codec->view.converted_buffer_size;
    const size_t channels = (size_t)codec->view.format.channels;
    const size_t frame_bytes = channels * 2u;
    sample_array scratch = {.is_null = true};
    bool ok = false;
    if (!allocate_array(codec, capacity, MAX_SOURCE_BUFFER * 2u, &scratch, error) ||
        !tick(codec, error))
        goto done;
    codec->view.last_synthesis_code = vorbis_synthesis(&codec->block, &codec->packet);
    if (codec->view.last_synthesis_code == 0 && !synthesis_block(codec, error))
        goto done;
    const size_t batch_limit = capacity / frame_bytes;
    size_t offset = 0;
    while (offset < capacity) {
        if (!tick(codec, error))
            goto done;
        float **planes = NULL;
        const int live = vorbis_synthesis_pcmout(&codec->dsp, &planes);
        if (live < 0) {
            (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec PCM state", live,
                       no_stream_error());
            goto done;
        }
        const size_t saved = codec->saved_frames - codec->saved_offset;
        const size_t available = saved + (size_t)live;
        if (available == 0)
            break;
        const size_t frames = available < batch_limit ? available : batch_limit;
        if (frames == 0 || frames > (capacity - offset) / frame_bytes) {
            (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec packet conversion guard", 0,
                       no_stream_error());
            goto done;
        }
        const size_t from_saved = frames < saved ? frames : saved;
        const size_t from_live = frames - from_saved;
        if (from_live != 0) {
            if (planes == NULL) {
                (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec PCM planes", 0,
                           no_stream_error());
                goto done;
            }
            for (size_t channel = 0; channel < channels; ++channel)
                if (planes[channel] == NULL) {
                    (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec PCM plane", 0,
                               no_stream_error());
                    goto done;
                }
        }
        if (from_saved != 0 && codec->saved_pcm == NULL) {
            (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec PCM plane", 0,
                       no_stream_error());
            goto done;
        }
        for (size_t frame = 0; frame < frames; ++frame)
            for (size_t channel = 0; channel < channels; ++channel) {
                const float sample =
                    frame < from_saved
                        ? codec->saved_pcm[(codec->saved_offset + frame) * channels + channel]
                        : planes[channel][frame - from_saved];
                mc_sound_pcm_write_s16le(sample,
                                         scratch.data + offset + (frame * channels + channel) * 2u);
            }
        offset += frames * frame_bytes;
        codec->view.converted_bytes += frames * frame_bytes;
        consume_saved_pcm(codec, from_saved);
        if (from_live != 0) {
            codec->view.last_pcm_read_code = vorbis_synthesis_read(&codec->dsp, (int)from_live);
            if (codec->view.last_pcm_read_code != 0) {
                (void)fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec PCM consumption",
                           codec->view.last_pcm_read_code, no_stream_error());
                goto done;
            }
        }
    }
    if (offset == capacity) {
        /* Actual trimArray returns null, not the full original array. */
        codec->view.discarded_full_buffer_bytes += offset;
        *out = (sample_array){.is_null = true};
    } else {
        if (!allocate_array(codec, offset, codec->limits.pcm_bytes, out, error))
            goto done;
        if (offset != 0)
            memcpy(out->data, scratch.data, offset);
    }
    ok = true;
done:
    free(scratch.data);
    return ok;
}

static bool read_bytes(mc_vorbis_codec *codec, sample_array *out, mc_vorbis_codec_error *error) {
    if (!tick(codec, error))
        return false;
    if (!codec->view.initialized || codec->view.end_of_stream)
        return true;
    if (!codec->sync_ready || !codec->packets_ready || !codec->dsp_ready || !codec->block_ready ||
        codec->source == NULL)
        return fail(error, MC_VORBIS_CODEC_SOURCE_STATE, "codec read after resource cleanup", 0,
                    no_stream_error());
    int code = 0;
    if (!page_out(codec, &code, error))
        return false;
    if (code == -1 || code == 0) {
        codec->view.end_of_stream = true;
        return true;
    }
    if (code == 1) {
        codec->view.last_pagein_code = ogg_stream_pagein(&codec->packets, &codec->page);
        if (ogg_stream_check(&codec->packets) != 0)
            return fail(error, MC_VORBIS_CODEC_ALLOCATION_FAILED, "codec audio packet storage",
                        codec->view.last_pagein_code, no_stream_error());
        if (ogg_page_granulepos(&codec->page) == 0) {
            codec->view.end_of_stream = true;
            return true;
        }
        for (;;) {
            if (!packet_out(codec, &code, error))
                return false;
            if (code == 0 || code == -1)
                break;
            if (code == 1) {
                sample_array packet = {.is_null = true};
                const bool decoded = decode_packet(codec, &packet, error);
                const bool appended = decoded && append_arrays(codec, out, &packet, error);
                free(packet.data);
                if (!decoded || !appended)
                    return false;
            }
        }
        if (ogg_page_eos(&codec->page) != 0)
            codec->view.end_of_stream = true;
    }
    if (!codec->view.end_of_stream) {
        int32_t count = 0;
        if (!refill(codec, false, &count, error))
            return false;
        if (!error->source_refill_caught && count == 0)
            codec->view.end_of_stream = true;
    }
    return true;
}

static bool empty_result(const mc_vorbis_codec_buffer *out) {
    return out != NULL && !out->present && out->data == NULL && out->bytes == 0;
}

static mc_vorbis_codec_status read_admission(mc_vorbis_codec *codec, mc_vorbis_codec_buffer *out,
                                             mc_vorbis_codec_error *error) {
    if (codec == NULL || !empty_result(out))
        return admission(error, MC_VORBIS_CODEC_INVALID_ARGUMENT);
    if (codec->active)
        return admission(error, MC_VORBIS_CODEC_BUSY);
    if (codec->view.failed && codec->view.initialized && !codec->view.end_of_stream)
        return admission(error, MC_VORBIS_CODEC_FAILED_STATE);
    return MC_VORBIS_CODEC_OK;
}

static void publish(mc_vorbis_codec *codec, sample_array *array, bool all,
                    mc_vorbis_codec_buffer *out) {
    const bool present = all || !array->is_null;
    *out = (mc_vorbis_codec_buffer){.present = present,
                                    .data_is_null = array->is_null,
                                    .data = array->data,
                                    .bytes = array->bytes,
                                    .format =
                                        present ? codec->view.format : (mc_vorbis_codec_format){0}};
    array->data = NULL;
}

mc_vorbis_codec_status mc_vorbis_codec_read(mc_vorbis_codec *codec, mc_vorbis_codec_buffer *out,
                                            mc_vorbis_codec_error *error) {
    const mc_vorbis_codec_status admitted = read_admission(codec, out, error);
    if (admitted != MC_VORBIS_CODEC_OK)
        return admitted;
    codec->active = true;
    codec->work = 0;
    mc_vorbis_codec_error cause = no_error();
    sample_array array = {.is_null = true};
    (void)read_bytes(codec, &array, &cause);
    publish(codec, &array, false, out);
    return finish(codec, &cause, error);
}

mc_vorbis_codec_status mc_vorbis_codec_read_all(mc_vorbis_codec *codec, int32_t max_file_size,
                                                mc_vorbis_codec_buffer *out,
                                                mc_vorbis_codec_error *error) {
    const mc_vorbis_codec_status admitted = read_admission(codec, out, error);
    if (admitted != MC_VORBIS_CODEC_OK)
        return admitted;
    codec->active = true;
    codec->work = 0;
    mc_vorbis_codec_error cause = no_error();
    sample_array array = {.is_null = true};
    if (!read_bytes(codec, &array, &cause))
        goto done;
    while (!codec->view.end_of_stream) {
        sample_array next = {.is_null = true};
        const bool decoded = read_bytes(codec, &next, &cause);
        const bool appended = decoded && append_arrays(codec, &array, &next, &cause);
        free(next.data);
        if (!decoded || !appended)
            break;
        /* Source PCs16/19 precede the max-file-size test at29/32. */
        if (!array.is_null && (max_file_size <= 0 || array.bytes >= (size_t)max_file_size))
            break;
    }
done:
    publish(codec, &array, true, out);
    return finish(codec, &cause, error);
}

bool mc_vorbis_codec_read_view(const mc_vorbis_codec *codec, mc_vorbis_codec_view *out) {
    if (codec == NULL || out == NULL)
        return false;
    *out = codec->view;
    out->resources_present = codec->source != NULL || codec->sync_ready || codec->packets_ready ||
                             codec->info_ready || codec->comment_ready || codec->dsp_ready ||
                             codec->block_ready;
    return true;
}

mc_vorbis_codec_status mc_vorbis_codec_cleanup_resources(mc_vorbis_codec *codec,
                                                         mc_vorbis_codec_error *out) {
    if (codec == NULL)
        return admission(out, MC_VORBIS_CODEC_INVALID_ARGUMENT);
    if (codec->active)
        return admission(out, MC_VORBIS_CODEC_BUSY);
    codec->active = true;
    mc_vorbis_codec_error error = no_error();
    clear_resources(codec, &error.cleanup, &error);
    return finish(codec, &error, out);
}

mc_vorbis_codec_status mc_vorbis_codec_free(mc_vorbis_codec **codec, mc_vorbis_codec_error *out) {
    if (codec == NULL)
        return admission(out, MC_VORBIS_CODEC_INVALID_ARGUMENT);
    if (*codec == NULL) {
        if (out != NULL)
            *out = no_error();
        return MC_VORBIS_CODEC_OK;
    }
    if ((*codec)->active)
        return admission(out, MC_VORBIS_CODEC_BUSY);
    mc_vorbis_codec_error error = no_error();
    clear_resources(*codec, &error.cleanup, &error);
    free(*codec);
    *codec = NULL;
    if (out != NULL)
        *out = error;
    return error.status;
}

void mc_vorbis_codec_buffer_free(mc_vorbis_codec_buffer *buffer) {
    if (buffer != NULL) {
        free(buffer->data);
        *buffer = (mc_vorbis_codec_buffer){0};
    }
}
