#ifndef MC_CLIENT_GUI_FONT_KERNELS_H
#define MC_CLIENT_GUI_FONT_KERNELS_H
#include <emmintrin.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Return the first matching code unit, or length for a miss. */
static inline size_t mc_font_unit_index(const uint16_t *units, size_t length, uint16_t unit) {
    if (length == 0 || units[0] == unit)
        return 0;
    size_t first = 1;
    const __m128i target = _mm_set1_epi16((int16_t)unit);
    while (length - first >= 8) {
        const __m128i values = _mm_loadu_si128((const __m128i *)(const void *)(units + first));
        unsigned int mask = (unsigned int)_mm_movemask_epi8(_mm_cmpeq_epi16(values, target));
        if (mask != 0) {
            while ((mask & 3u) == 0) {
                mask >>= 2;
                ++first;
            }
            return first;
        }
        first += 8;
    }
    while (first < length && units[first] != unit)
        ++first;
    return first;
}

/* Shared pure source arithmetic. Callers own admission, live field
 * reads, resource effects and failure/store order. No fp active use. */
static inline uint32_t mc_font_source_palette(unsigned int index, bool anaglyph) {
    const unsigned int bright = ((index >> 3u) & 1u) * 85u;
    unsigned int red = ((index >> 2u) & 1u) * 170u + bright;
    unsigned int green = ((index >> 1u) & 1u) * 170u + bright;
    unsigned int blue = (index & 1u) * 170u + bright;
    /* sj.<init>: the gold adjustment applies to index6, not shadow index22. */
    if (index == 6u)
        red += 85u;
    if (anaglyph) {
        const unsigned int r = (red * 30u + green * 59u + blue * 11u) / 100u;
        const unsigned int g = (red * 30u + green * 70u) / 100u;
        const unsigned int b = (red * 30u + blue * 70u) / 100u;
        red = r;
        green = g;
        blue = b;
    }
    if (index >= 16u) {
        red /= 4u;
        green /= 4u;
        blue /= 4u;
    }
    return UINT32_C(0xff000000) | (red << 16u) | (green << 8u) | blue;
}

static inline uint32_t mc_font_source_initial_color(uint32_t argb, bool shadow) {
    uint32_t alpha = argb & UINT32_C(0xff000000);
    uint32_t rgb = argb & UINT32_C(0x00ffffff);
    if (shadow)
        rgb = (rgb & UINT32_C(0x00fcfcfc)) >> 2u;
    if (alpha == 0)
        alpha = UINT32_C(0xff000000);
    return alpha | rgb;
}
#endif
