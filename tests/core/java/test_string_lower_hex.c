#include "mc/core/java/string_lower_hex.h"

#include <inttypes.h>
#include <stdio.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "String lower hex:%d: %s\n", __LINE__, #condition);                    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int accepted(const uint16_t *units, size_t length, size_t index, uint8_t expected) {
    mc_java_string_lower_hex_view view = {0};
    CHECK(mc_java_string_lower_hex_prepare(units, length, &view) == MC_JAVA_STRING_LOWER_HEX_OK);
    CHECK(view.units == units && view.length == length);
    CHECK(mc_java_string_lower_hex_project(&view, index) == expected);
    return 0;
}

static int refused(const uint16_t *units, size_t length) {
    const uint16_t retained[] = {'x'};
    mc_java_string_lower_hex_view view = {retained, COUNT(retained)};
    CHECK(mc_java_string_lower_hex_prepare(units, length, &view) ==
          MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED);
    CHECK(view.units == retained && view.length == COUNT(retained));
    return 0;
}

static int controls(void) {
    const uint16_t reached[] = {0x00a7, 'b', 0x00e9, 0x03a9};
    const uint16_t prefix[] = {0x00e9, 0x00a7, 'C', 0x03a9};
    const uint16_t sigma[] = {'A', 0x03a3, 0x00a7, 'E'};
    const uint16_t kelvin[] = {0x00a7, 0x212a};
    const uint16_t supplementary[] = {0xd801, 0xdc00, 0x00a7, 'D'};
    const uint16_t high[] = {0xd83d, 0x00a7, 'A'};
    const uint16_t low[] = {0xdc00, 0x00a7, '9'};
    const uint16_t color_pair[] = {0x00a7, 0xd83d, 0xde00};
    const uint16_t nul[] = {0, 0x00a7, '0', 0};
    const uint16_t base_boundary[] = {'I', 0x00e9, 0x00a7, 'F'};
    const uint16_t format_boundary[] = {'J', 0x00a7, 'a', 0x0301};
    const uint16_t other_combining[] = {'I', 0x0323, 0x00a7, 'c'};
    const uint16_t dot_without_i[] = {0x0307, 0x00a7, 'c'};
    const uint16_t dotted_i[] = {0x0130, 0x00a7, 'a'};
    const uint16_t grave_i[] = {0x00cc, 0x00a7, 'a'};
    const uint16_t acute_i[] = {0x00cd, 0x00a7, 'a'};
    const uint16_t tilde_i[] = {0x0128, 0x00a7, 'a'};
    const uint16_t more_above[] = {'I', 0x0301, 0x00a7, 'a'};
    const uint16_t more_after_other[] = {'J', 0x0323, 0x0301, 0x00a7, 'a'};
    const uint16_t ogonek[] = {0x012e, 0x0301, 0x00a7, 'a'};
    const uint16_t after_i[] = {'I', 0x0323, 0x0307, 0x00a7, 'a'};
    const uint16_t suffix[] = {0x00a7, 'b', 'I', 0x0301};
    const uint16_t supplementary_above[] = {'J', 0xd800, 0xdf76, 0x00a7, 'a'};
    const uint16_t supplementary_other[] = {'I', 0xd800, 0xddfd, 0x0307, 0x00a7, 'a'};
    CHECK(accepted(reached, COUNT(reached), 1, 11) == 0);
    CHECK(accepted(prefix, COUNT(prefix), 2, 12) == 0);
    CHECK(accepted(sigma, COUNT(sigma), 3, 14) == 0);
    CHECK(accepted(kelvin, COUNT(kelvin), 1, 15) == 0);
    CHECK(accepted(supplementary, COUNT(supplementary), 3, 13) == 0);
    CHECK(accepted(high, COUNT(high), 2, 10) == 0);
    CHECK(accepted(low, COUNT(low), 2, 9) == 0);
    CHECK(accepted(color_pair, COUNT(color_pair), 1, 15) == 0);
    CHECK(accepted(nul, COUNT(nul), 2, 0) == 0);
    CHECK(accepted(base_boundary, COUNT(base_boundary), 3, 15) == 0);
    CHECK(accepted(format_boundary, COUNT(format_boundary), 2, 10) == 0);
    CHECK(accepted(other_combining, COUNT(other_combining), 3, 12) == 0);
    CHECK(accepted(dot_without_i, COUNT(dot_without_i), 2, 12) == 0);
    CHECK(refused(dotted_i, COUNT(dotted_i)) == 0);
    CHECK(refused(grave_i, COUNT(grave_i)) == 0);
    CHECK(refused(acute_i, COUNT(acute_i)) == 0);
    CHECK(refused(tilde_i, COUNT(tilde_i)) == 0);
    CHECK(refused(more_above, COUNT(more_above)) == 0);
    CHECK(refused(more_after_other, COUNT(more_after_other)) == 0);
    CHECK(refused(ogonek, COUNT(ogonek)) == 0);
    CHECK(refused(after_i, COUNT(after_i)) == 0);
    CHECK(refused(suffix, COUNT(suffix)) == 0);
    CHECK(refused(supplementary_above, COUNT(supplementary_above)) == 0);
    CHECK(refused(supplementary_other, COUNT(supplementary_other)) == 0);

    mc_java_string_lower_hex_view view = {reached, COUNT(reached)};
    CHECK(mc_java_string_lower_hex_prepare(NULL, 1, &view) ==
          MC_JAVA_STRING_LOWER_HEX_INVALID_ARGUMENT);
    CHECK(view.units == reached && view.length == COUNT(reached));
    CHECK(mc_java_string_lower_hex_prepare(reached, (size_t)INT32_MAX + 1, &view) ==
          MC_JAVA_STRING_LOWER_HEX_INVALID_ARGUMENT);
    CHECK(view.units == reached && view.length == COUNT(reached));
    CHECK(mc_java_string_lower_hex_prepare(reached, COUNT(reached), NULL) ==
          MC_JAVA_STRING_LOWER_HEX_INVALID_ARGUMENT);
    CHECK(mc_java_string_lower_hex_prepare(NULL, 0, &view) == MC_JAVA_STRING_LOWER_HEX_OK);
    CHECK(view.units == NULL && view.length == 0);
    for (uint16_t unit = 0; unit < 128; ++unit) {
        CHECK(mc_java_string_lower_hex_prepare(&unit, 1, &view) == MC_JAVA_STRING_LOWER_HEX_OK);
        const uint8_t actual = mc_java_string_lower_hex_project(&view, 0);
        const uint8_t expected = unit >= '0' && unit <= '9'   ? (uint8_t)(unit - '0')
                                 : unit >= 'a' && unit <= 'f' ? (uint8_t)(unit - 'a' + 10)
                                 : unit >= 'A' && unit <= 'F' ? (uint8_t)(unit - 'A' + 10)
                                                              : 15;
        CHECK(actual == expected);
    }
    return 0;
}

static int full_codepoint_contexts(void) {
    uint64_t fingerprint = UINT64_C(0xcbf29ce484222325);
    uint32_t first_refused = 0, second_refused = 0;
    for (uint32_t point = 0; point < UINT32_C(0x110000); ++point) {
        uint16_t text[4] = {'I'};
        size_t length = 1;
        if (point <= UINT32_C(0xffff))
            text[length++] = (uint16_t)point;
        else {
            const uint32_t delta = point - UINT32_C(0x10000);
            text[length++] = (uint16_t)(UINT32_C(0xd800) + (delta >> 10));
            text[length++] = (uint16_t)(UINT32_C(0xdc00) + (delta & UINT32_C(0x3ff)));
        }
        mc_java_string_lower_hex_view view = {0};
        const mc_java_string_lower_hex_status first =
            mc_java_string_lower_hex_prepare(text, length, &view);
        text[length++] = UINT16_C(0x0307);
        const mc_java_string_lower_hex_status second =
            mc_java_string_lower_hex_prepare(text, length, &view);
        CHECK(first == MC_JAVA_STRING_LOWER_HEX_OK ||
              first == MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED);
        CHECK(second == MC_JAVA_STRING_LOWER_HEX_OK ||
              second == MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED);
        first_refused += first == MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED;
        second_refused += second == MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED;
        fingerprint = (fingerprint ^ (first == MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED ? 2u : 0u)) *
                      UINT64_C(0x100000001b3);
        fingerprint = (fingerprint ^ (second == MC_JAVA_STRING_LOWER_HEX_UNSUPPORTED ? 2u : 0u)) *
                      UINT64_C(0x100000001b3);
    }
    /* Golden comes from complete actual CCC observations + independent direct
     * MORE_ABOVE/AFTER_I source-context predicates, not the generated C table. */
    CHECK(first_refused == 488 && second_refused == 879);
    CHECK(fingerprint == UINT64_C(0x2524dfda23d2be1f));
    return 0;
}

int main(void) {
    if (controls() != 0 || full_codepoint_contexts() != 0)
        return 1;
    printf("PASS: %u invariant lowercase hex checks; 1114112 codepoint contexts\n", checks);
    return 0;
}
