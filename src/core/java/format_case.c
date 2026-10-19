#include "format_internal.h"

#include <limits.h>

typedef struct uppercase_mapping {
    uint32_t point;
    uint16_t units[3];
    uint8_t length;
} uppercase_mapping;

#include "format_case_data.h"

static const uppercase_mapping *mapping(uint32_t point)
{
    size_t low = 0, high = sizeof(uppercase_mappings) / sizeof(*uppercase_mappings);
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (uppercase_mappings[middle].point < point)
            low = middle + 1;
        else
            high = middle;
    }
    if (low < sizeof(uppercase_mappings) / sizeof(*uppercase_mappings) &&
        uppercase_mappings[low].point == point)
        return &uppercase_mappings[low];
    return NULL;
}

static uint32_t next_point(const mc_java_format_text *text, size_t *index)
{
    const uint16_t first = text->units[(*index)++];
    if (first >= UINT16_C(0xd800) && first <= UINT16_C(0xdbff) && *index < text->length) {
        const uint16_t second = text->units[*index];
        if (second >= UINT16_C(0xdc00) && second <= UINT16_C(0xdfff)) {
            ++*index;
            return UINT32_C(0x10000) + (((uint32_t)first - UINT32_C(0xd800)) << 10u) +
                   (uint32_t)second - UINT32_C(0xdc00);
        }
    }
    return first;
}

mc_java_format_status mc_java_format_upper(const mc_java_format_options *options,
                                           const mc_java_format_text *input,
                                           mc_java_format_text *out)
{
    if (options == NULL || options->allocator.allocate == NULL || input == NULL || out == NULL ||
        (input->units == NULL && input->length != 0))
        return MC_JAVA_FORMAT_INVALID_ARGUMENT;
    if (options->locale != MC_JAVA_FORMAT_LOCALE_ENGLISH)
        return MC_JAVA_FORMAT_UNSUPPORTED_LOCALE;
    const size_t maximum =
        options->max_output_units == 0 ? (size_t)INT32_MAX : options->max_output_units;
    if (input->length > (size_t)INT32_MAX || input->length > maximum)
        return MC_JAVA_FORMAT_LIMIT;
    size_t length = 0, index = 0;
    bool changed = false;
    while (index < input->length) {
        const size_t first = index;
        const uppercase_mapping *entry = mapping(next_point(input, &index));
        const size_t count = entry == NULL ? index - first : entry->length;
        if (count > maximum - length || count > (size_t)INT32_MAX - length)
            return MC_JAVA_FORMAT_LIMIT;
        length += count;
        changed = changed || entry != NULL;
    }
    if (!changed) {
        *out = *input;
        return MC_JAVA_FORMAT_OK;
    }
    if (length > SIZE_MAX / sizeof(uint16_t))
        return MC_JAVA_FORMAT_LIMIT;
    uint16_t *units =
        options->allocator.allocate(options->allocator.context, length * sizeof(*units));
    if (units == NULL)
        return MC_JAVA_FORMAT_ALLOCATION_FAILED;
    index = 0;
    size_t destination = 0;
    while (index < input->length) {
        const size_t first = index;
        const uppercase_mapping *entry = mapping(next_point(input, &index));
        if (entry == NULL) {
            for (size_t i = first; i < index; ++i)
                units[destination++] = input->units[i];
        } else {
            for (size_t i = 0; i < entry->length; ++i)
                units[destination++] = entry->units[i];
        }
    }
    *out = (mc_java_format_text){.units = units, .length = length};
    return MC_JAVA_FORMAT_OK;
}
