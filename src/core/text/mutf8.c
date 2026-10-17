#include "mc/core/text/mutf8.h"

/* Caller validates the whole payload before decoding to its output buffer. */
static mc_mutf8_status read_unit(const uint8_t *input, size_t length, size_t *offset,
                                 uint16_t *unit)
{
    const uint8_t first = input[*offset];
    size_t width;
    uint16_t value;

    if (first <= UINT8_C(0x7f)) {
        width = 1;
        value = first;
    } else if (first >= UINT8_C(0xc0) && first <= UINT8_C(0xdf)) {
        width = 2;
        if (length - *offset < width || (input[*offset + 1] & UINT8_C(0xc0)) != UINT8_C(0x80)) {
            return MC_MUTF8_INVALID_ENCODING;
        }
        value = (uint16_t)(((uint16_t)(first & UINT8_C(0x1f)) << 6) |
                           (uint16_t)(input[*offset + 1] & UINT8_C(0x3f)));
    } else if (first >= UINT8_C(0xe0) && first <= UINT8_C(0xef)) {
        width = 3;
        if (length - *offset < width || (input[*offset + 1] & UINT8_C(0xc0)) != UINT8_C(0x80) ||
            (input[*offset + 2] & UINT8_C(0xc0)) != UINT8_C(0x80)) {
            return MC_MUTF8_INVALID_ENCODING;
        }
        value = (uint16_t)(((uint16_t)(first & UINT8_C(0x0f)) << 12) |
                           ((uint16_t)(input[*offset + 1] & UINT8_C(0x3f)) << 6) |
                           (uint16_t)(input[*offset + 2] & UINT8_C(0x3f)));
    } else {
        return MC_MUTF8_INVALID_ENCODING;
    }

    *offset += width;
    *unit = value;
    return MC_MUTF8_OK;
}

mc_mutf8_status mc_mutf8_decoded_length(const uint8_t *input, size_t length, size_t *units)
{
    size_t offset = 0;
    size_t count = 0;

    if (units == NULL || (input == NULL && length != 0)) {
        return MC_MUTF8_INVALID_ARGUMENT;
    }

    while (offset < length) {
        uint16_t unit;
        mc_mutf8_status status = read_unit(input, length, &offset, &unit);
        if (status != MC_MUTF8_OK) {
            return status;
        }
        /* Every unit consumes at least one byte, so count cannot exceed length. */
        ++count;
    }

    *units = count;
    return MC_MUTF8_OK;
}

mc_mutf8_status mc_mutf8_decode(const uint8_t *input, size_t length, uint16_t *output,
                                size_t capacity, size_t *written)
{
    size_t count;
    size_t offset = 0;
    size_t index = 0;
    mc_mutf8_status status;

    if (written == NULL) {
        return MC_MUTF8_INVALID_ARGUMENT;
    }
    status = mc_mutf8_decoded_length(input, length, &count);
    if (status != MC_MUTF8_OK) {
        return status;
    }
    if (count != 0 && output == NULL) {
        return MC_MUTF8_INVALID_ARGUMENT;
    }
    if (count > SIZE_MAX / sizeof(*output)) {
        return MC_MUTF8_OVERFLOW;
    }
    if (count > capacity) {
        return MC_MUTF8_BUFFER_TOO_SMALL;
    }

    while (offset < length) {
        uint16_t unit;
        (void)read_unit(input, length, &offset, &unit);
        output[index++] = unit;
    }

    *written = count;
    return MC_MUTF8_OK;
}

static size_t unit_width(uint16_t unit)
{
    if (unit >= UINT16_C(1) && unit <= UINT16_C(0x7f)) {
        return 1;
    }
    if (unit <= UINT16_C(0x7ff)) {
        return 2;
    }
    return 3;
}

mc_mutf8_status mc_mutf8_encoded_length(const uint16_t *input, size_t units, size_t *bytes)
{
    size_t count = 0;

    if (bytes == NULL || (input == NULL && units != 0)) {
        return MC_MUTF8_INVALID_ARGUMENT;
    }
    if (units > SIZE_MAX / sizeof(*input)) {
        return MC_MUTF8_OVERFLOW;
    }
    for (size_t index = 0; index < units; ++index) {
        size_t width = unit_width(input[index]);
        if (width > SIZE_MAX - count) {
            return MC_MUTF8_OVERFLOW;
        }
        count += width;
    }

    *bytes = count;
    return MC_MUTF8_OK;
}

mc_mutf8_status mc_mutf8_encode(const uint16_t *input, size_t units, uint8_t *output,
                                size_t capacity, size_t *written)
{
    size_t count;
    size_t offset = 0;
    mc_mutf8_status status;

    if (written == NULL) {
        return MC_MUTF8_INVALID_ARGUMENT;
    }
    status = mc_mutf8_encoded_length(input, units, &count);
    if (status != MC_MUTF8_OK) {
        return status;
    }
    if (count != 0 && output == NULL) {
        return MC_MUTF8_INVALID_ARGUMENT;
    }
    if (count > capacity) {
        return MC_MUTF8_BUFFER_TOO_SMALL;
    }

    for (size_t index = 0; index < units; ++index) {
        const uint16_t unit = input[index];
        const size_t width = unit_width(unit);
        if (width == 1) {
            output[offset++] = (uint8_t)unit;
        } else if (width == 2) {
            output[offset++] = (uint8_t)(UINT16_C(0xc0) | (unit >> 6));
            output[offset++] = (uint8_t)(UINT16_C(0x80) | (unit & UINT16_C(0x3f)));
        } else {
            output[offset++] = (uint8_t)(UINT16_C(0xe0) | (unit >> 12));
            output[offset++] = (uint8_t)(UINT16_C(0x80) | ((unit >> 6) & UINT16_C(0x3f)));
            output[offset++] = (uint8_t)(UINT16_C(0x80) | (unit & UINT16_C(0x3f)));
        }
    }

    *written = count;
    return MC_MUTF8_OK;
}
