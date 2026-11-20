#ifndef MC_JAVA_RANDOM_INTERNAL_H
#define MC_JAVA_RANDOM_INTERNAL_H

#include "mc/core/java/java.h"

#include <stddef.h>

#define MC_JAVA_RANDOM_MULTIPLIER UINT64_C(0x5deece66d)
#define MC_JAVA_RANDOM_MASK UINT64_C(0xffffffffffff)

static inline uint32_t mc_java_random_next_bits(mc_java_random *random, unsigned int bits) {
    random->state = (random->state * MC_JAVA_RANDOM_MULTIPLIER + 11u) & MC_JAVA_RANDOM_MASK;
    return (uint32_t)(random->state >> (48u - bits));
}

/* The inline body lets callers with a constant bound specialize the arithmetic. */
static inline bool mc_java_random_next_bounded_inline(mc_java_random *random, int32_t bound,
                                                      int32_t *out) {
    if (bound <= 0 || out == NULL)
        return false;
    uint32_t bits = mc_java_random_next_bits(random, 31);
    const uint32_t range = (uint32_t)bound;
    if ((range & (range - 1u)) == 0) {
        *out = (int32_t)(((uint64_t)range * bits) >> 31);
        return true;
    }
    for (;;) {
        const uint32_t value = bits % range;
        /* Test the sign of the wrapping int expression. */
        if (bits - value + (range - 1u) <= INT32_MAX) {
            *out = (int32_t)value;
            return true;
        }
        bits = mc_java_random_next_bits(random, 31);
    }
}

#endif
