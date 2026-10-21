#ifndef MC_TEST_SUPPORT_JAVA_RANDOM_H
#define MC_TEST_SUPPORT_JAVA_RANDOM_H

#include "mc/core/java/java.h"

#include <stdint.h>
#include <string.h>

/* Both inputs must be nonnull, initialized Random values. Compare retained
 * cached-double bits even while the cache is invalid; ignore struct padding. */
static inline bool mc_test_java_random_equal(const mc_java_random *left,
                                             const mc_java_random *right) {
    uint64_t left_bits;
    uint64_t right_bits;
    _Static_assert(sizeof(left_bits) == sizeof(left->next_next_gaussian),
                   "binary64 storage required");
    memcpy(&left_bits, &left->next_next_gaussian, sizeof(left_bits));
    memcpy(&right_bits, &right->next_next_gaussian, sizeof(right_bits));
    return left->state == right->state &&
           left->have_next_next_gaussian == right->have_next_next_gaussian &&
           left_bits == right_bits;
}

#endif
