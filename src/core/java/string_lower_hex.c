#include "mc/core/java/string_lower_hex.h"

#include <limits.h>
#include <stdbool.h>

typedef struct mc_lower_hex_ccc_range {
    uint32_t first, last;
    uint8_t partition; /* 1: CCC230; 2: other nonzero CCC. Unlisted: CCC0. */
} mc_lower_hex_ccc_range;

#include "string_lower_hex_ccc_partition.inc"

static uint8_t combining_partition(uint32_t point) {
    size_t low = 0, high = sizeof(mc_lower_hex_ccc_ranges) / sizeof(mc_lower_hex_ccc_ranges[0]);
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (point < mc_lower_hex_ccc_ranges[middle].first)
            high = middle;
        else if (point > mc_lower_hex_ccc_ranges[middle].last)
            low = middle + 1;
        else
            return mc_lower_hex_ccc_ranges[middle].partition;
    }
    return 0;
}

mc_java_string_lower_hex_status
mc_java_string_lower_hex_prepare(const uint16_t *units, size_t length,
                                 mc_java_string_lower_hex_view *out) {
    if (out == NULL || length > INT32_MAX || (length != 0 && units == NULL))
        return MC_JAVA_STRING_LOWER_HEX_INVALID_ARGUMENT;
    bool more_above_candidate = false, after_i = false;
    for (size_t index = 0; index < length; ++index) {
        uint32_t point = units[index];
        /* ASCII never needs a property lookup or changes a hex position.
         * I/J are retained only for a following non-ASCII combining context. */
        if (point < UINT32_C(0x80)) {
            more_above_candidate = point == 'I' || point == 'J';
            after_i = point == 'I';
            continue;
        }
        if (point == UINT32_C(0x0130) || point == UINT32_C(0x00cc) || point == UINT32_C(0x00cd) ||
            point == UINT32_C(0x0128))
            return MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED;
        if (point == UINT32_C(0x012e)) {
            more_above_candidate = true;
            after_i = false;
            continue;
        }
        if (point >= UINT32_C(0xd800) && point <= UINT32_C(0xdbff) && index + 1 < length &&
            units[index + 1] >= UINT16_C(0xdc00) && units[index + 1] <= UINT16_C(0xdfff)) {
            point = UINT32_C(0x10000) + ((point - UINT32_C(0xd800)) << 10) + units[++index] -
                    UINT32_C(0xdc00);
        }
        /* Once no candidate precedes this point, a non-ASCII point cannot
         * start either relevant context. Unpaired surrogates remain units. */
        if (more_above_candidate || after_i) {
            const uint8_t partition = combining_partition(point);
            if ((partition == 1 && more_above_candidate) || (point == UINT32_C(0x0307) && after_i))
                return MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED;
            if (partition == 0 || partition == 1) {
                more_above_candidate = false;
                after_i = false;
            }
        }
    }
    *out = (mc_java_string_lower_hex_view){units, length};
    return MC_JAVA_STRING_LOWER_HEX_OK;
}

uint8_t mc_java_string_lower_hex_project(const mc_java_string_lower_hex_view *view, size_t index) {
    const uint16_t unit = view->units[index];
    if (unit >= '0' && unit <= '9')
        return (uint8_t)(unit - '0');
    if (unit >= 'A' && unit <= 'F')
        return (uint8_t)(unit - 'A' + 10);
    if (unit >= 'a' && unit <= 'f')
        return (uint8_t)(unit - 'a' + 10);
    return 15;
}
