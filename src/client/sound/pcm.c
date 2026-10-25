#include "pcm_internal.h"

#include "mc/core/java/java.h"

#include <float.h>
#include <limits.h>

_Static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128,
               "Sound PCM conversion requires binary32 and eight-bit bytes");

void mc_sound_pcm_write_s16le(float sample, uint8_t out[2]) {
    /* The volatile store makes the source fmul's binary32 rounding explicit.
     * The build's existing strict floating-point profile remains required. */
    volatile float scaled = sample * 32767.0f;
    int32_t value = mc_java_float_to_i32(scaled);
    if (value > 32767)
        value = 32767;
    else if (value < -32768)
        value = -32768;
    const uint32_t bits = (uint32_t)value;
    out[0] = (uint8_t)bits;
    out[1] = (uint8_t)(bits >> 8u);
}

size_t mc_sound_pcm_write_pending(mc_sound_pcm_pending *pending, uint8_t *out, size_t capacity) {
    if (pending->frames == 0)
        return 0;
    const size_t frame_bytes = pending->channel_count * 2u;
    size_t frames = capacity / frame_bytes;
    if (frames > pending->frames)
        frames = pending->frames;
    size_t at = 0;
    for (size_t frame = 0; frame < frames; ++frame)
        for (size_t channel = 0; channel < pending->channel_count; ++channel) {
            mc_sound_pcm_write_s16le(pending->planes[channel][pending->offset + frame], out + at);
            at += 2u;
        }
    pending->offset += frames;
    pending->frames -= frames;
    if (pending->frames == 0)
        *pending = (mc_sound_pcm_pending){0};
    return at;
}
