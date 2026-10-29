#include "mc/core/java/integer_parse.h"

#include <stdbool.h>

/* The selected Character.digit(char,10) profile has 37 consecutive ten-unit
 * ranges. These code-point facts are independently checked against all 65536
 * unit inputs; no generated JDK lookup implementation is used. */
static int decimal_digit(uint16_t unit) {
    static const uint16_t zeroes[] = {
        0x0030, 0x0660, 0x06f0, 0x07c0, 0x0966, 0x09e6, 0x0a66, 0x0ae6, 0x0b66, 0x0be6,
        0x0c66, 0x0ce6, 0x0d66, 0x0de6, 0x0e50, 0x0ed0, 0x0f20, 0x1040, 0x1090, 0x17e0,
        0x1810, 0x1946, 0x19d0, 0x1a80, 0x1a90, 0x1b50, 0x1bb0, 0x1c40, 0x1c50, 0xa620,
        0xa8d0, 0xa900, 0xa9d0, 0xa9f0, 0xaa50, 0xabf0, 0xff10};
    size_t low = 0, high = sizeof(zeroes) / sizeof(zeroes[0]);
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (zeroes[middle] <= unit)
            low = middle + 1;
        else
            high = middle;
    }
    if (low == 0)
        return -1;
    const unsigned int digit = (unsigned int)unit - zeroes[low - 1];
    return digit < 10u ? (int)digit : -1;
}

mc_java_integer_parse_status mc_java_integer_parse(const mc_java_integer_parse_text *text,
                                                   int32_t *out) {
    if (out == NULL)
        return MC_JAVA_INTEGER_PARSE_INVALID_ARGUMENT;
    if (text == NULL)
        return MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT;
    if (text->length > MC_JAVA_INTEGER_PARSE_MAX_UNITS)
        return MC_JAVA_INTEGER_PARSE_LIMIT;
    if (text->length != 0 && text->units == NULL)
        return MC_JAVA_INTEGER_PARSE_INVALID_ARGUMENT;
    if (text->length == 0)
        return MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT;
    size_t index = 0;
    const bool negative = text->units[0] == '-';
    if (negative || text->units[0] == '+')
        ++index;
    if (index == text->length)
        return MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT;
    const uint32_t limit = negative ? UINT32_C(2147483648) : UINT32_C(2147483647);
    uint32_t magnitude = 0;
    for (; index < text->length; ++index) {
        const int digit = decimal_digit(text->units[index]);
        if (digit < 0 || magnitude > (limit - (uint32_t)digit) / 10u)
            return MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT;
        magnitude = magnitude * 10u + (uint32_t)digit;
    }
    *out = negative ? (magnitude == UINT32_C(2147483648) ? INT32_MIN : -(int32_t)magnitude)
                    : (int32_t)magnitude;
    return MC_JAVA_INTEGER_PARSE_OK;
}

mc_java_integer_parse_status mc_java_long_parse(const mc_java_integer_parse_text *text,
                                                int64_t *out) {
    if (out == NULL)
        return MC_JAVA_INTEGER_PARSE_INVALID_ARGUMENT;
    if (text == NULL)
        return MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT;
    if (text->length > MC_JAVA_INTEGER_PARSE_MAX_UNITS)
        return MC_JAVA_INTEGER_PARSE_LIMIT;
    if (text->length != 0 && text->units == NULL)
        return MC_JAVA_INTEGER_PARSE_INVALID_ARGUMENT;
    if (text->length == 0)
        return MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT;
    size_t index = 0;
    const bool negative = text->units[0] == '-';
    if (negative || text->units[0] == '+')
        ++index;
    if (index == text->length)
        return MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT;
    const uint64_t limit = negative ? UINT64_C(9223372036854775808) : UINT64_C(9223372036854775807);
    uint64_t magnitude = 0;
    for (; index < text->length; ++index) {
        const int digit = decimal_digit(text->units[index]);
        if (digit < 0 || magnitude > (limit - (uint64_t)digit) / UINT64_C(10))
            return MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT;
        magnitude = magnitude * UINT64_C(10) + (uint64_t)digit;
    }
    *out = negative ? (magnitude == UINT64_C(9223372036854775808) ? INT64_MIN : -(int64_t)magnitude)
                    : (int64_t)magnitude;
    return MC_JAVA_INTEGER_PARSE_OK;
}
