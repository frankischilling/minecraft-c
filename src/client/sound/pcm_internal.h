#ifndef MC_CLIENT_SOUND_PCM_INTERNAL_H
#define MC_CLIENT_SOUND_PCM_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* Binary32 multiply by 32767, Java float-to-int conversion, signed16 clamp,
 * then little-endian bytes. out must point to two writable bytes. */
void mc_sound_pcm_write_s16le(float sample, uint8_t out[2]);

/* Borrowed libvorbis planes. The decoder installs these only after validating
 * the returned link's metadata. No synthesis/read call may occur until frames
 * reaches zero; retirement may discard them. Each plane contains offset+frames
 * samples and channel_count is in 1..255. The helper never modifies a plane. */
typedef struct mc_sound_pcm_pending {
    float **planes;
    size_t offset, frames, channel_count;
} mc_sound_pcm_pending;

/* Write only complete interleaved frames, preserving unconsumed planes/offset.
 * Empty pending data performs no work. For nonempty data, out has capacity
 * writable bytes. The decoder bounds offset+frames to its requested int count. */
size_t mc_sound_pcm_write_pending(mc_sound_pcm_pending *pending, uint8_t *out, size_t capacity);

#endif
