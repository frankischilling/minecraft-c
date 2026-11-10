#include "mc/core/java/string_case.h"
#include "string_case_internal.h"

typedef struct mc_string_case_pair {
    uint32_t point, folded;
} mc_string_case_pair;

#include "string_case_table.inc"

uint32_t mc_java_string_case_fold(uint32_t point) {
    if (point < UINT32_C(0x80))
        return point >= 'A' && point <= 'Z' ? point + ('a' - 'A') : point;
    size_t low = 0, high = sizeof(mc_string_case_folds) / sizeof(mc_string_case_folds[0]);
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (point == mc_string_case_folds[middle].point)
            return mc_string_case_folds[middle].folded;
        if (point < mc_string_case_folds[middle].point)
            high = middle;
        else
            low = middle + 1;
    }
    return point;
}

static bool high_surrogate(uint32_t value) {
    return value >= UINT32_C(0xd800) && value <= UINT32_C(0xdbff);
}

static bool low_surrogate(uint32_t value) {
    return value >= UINT32_C(0xdc00) && value <= UINT32_C(0xdfff);
}

static uint32_t pair(uint32_t high, uint32_t low) {
    return ((high - UINT32_C(0xd800)) << 10) + low - UINT32_C(0xdc00) + UINT32_C(0x10000);
}

/* StringUTF16's fallback can combine a low surrogate with the preceding high
 * surrogate, even though the earlier char comparison already succeeded. A
 * high/low pair starting here consumes the extra following unit instead. */
static uint32_t including(const uint16_t *units, size_t length, size_t *index, uint32_t value) {
    if (low_surrogate(value) && *index != 0 && high_surrogate(units[*index - 1]))
        return pair(units[*index - 1], value);
    if (high_surrogate(value) && *index < length - 1 && low_surrogate(units[*index + 1])) {
        ++*index;
        return pair(value, units[*index]);
    }
    return value;
}

bool mc_java_string_equals_ignore_case(const uint16_t *a, size_t a_length, const uint16_t *b,
                                       size_t b_length) {
    if (a_length != b_length || (a_length != 0 && (a == NULL || b == NULL)))
        return false;
    size_t left_index = 0, right_index = 0;
    while (left_index < a_length && right_index < b_length) {
        uint32_t left = a[left_index], right = b[right_index];
        if (left != right && mc_java_string_case_fold(left) != mc_java_string_case_fold(right)) {
            left = including(a, a_length, &left_index, left);
            right = including(b, b_length, &right_index, right);
            if (mc_java_string_case_fold(left) != mc_java_string_case_fold(right))
                return false;
        }
        ++left_index;
        ++right_index;
    }
    return true;
}
