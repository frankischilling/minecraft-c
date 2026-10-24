#include "mc/core/java/float_format.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Source-derived special/layout cases and earlier retained profile examples.
 * No expected string is supplied to the formatter's input interface. */
int main(void) {
    static const struct {
        uint32_t bits;
        const char *text;
    } cases[] = {{UINT32_C(0x00000000), "0.0"},
                 {UINT32_C(0x80000000), "-0.0"},
                 {UINT32_C(0x7f800000), "Infinity"},
                 {UINT32_C(0xff800000), "-Infinity"},
                 {UINT32_C(0x7fc00000), "NaN"},
                 {UINT32_C(0xffc00000), "NaN"},
                 {UINT32_C(0x7f800001), "NaN"},
                 {UINT32_C(0x00000001), "1.4E-45"},
                 {UINT32_C(0x00000002), "2.8E-45"},
                 {UINT32_C(0x00000003), "4.2E-45"},
                 {UINT32_C(0x00000047), "1.0E-43"},
                 {UINT32_C(0x007fffff), "1.1754942E-38"},
                 {UINT32_C(0x00800000), "1.17549435E-38"},
                 {UINT32_C(0x00800001), "1.1754945E-38"},
                 {UINT32_C(0x3a83126e), "9.999999E-4"},
                 {UINT32_C(0x3a83126f), "0.001"},
                 {UINT32_C(0x3f7fffff), "0.99999994"},
                 {UINT32_C(0x3f800000), "1.0"},
                 {UINT32_C(0x3f800001), "1.0000001"},
                 {UINT32_C(0x4b189680), "1.0E7"},
                 {UINT32_C(0x4b800000), "1.6777216E7"},
                 {UINT32_C(0x4c000000), "3.3554432E7"},
                 {UINT32_C(0x4c800000), "6.7108864E7"},
                 {UINT32_C(0x4e932c06), "1.23456794E9"},
                 {UINT32_C(0x5182629a), "7.0000001E10"},
                 {UINT32_C(0x5e800000), "4.611686E18"},
                 {UINT32_C(0x5f000000), "9.223372E18"},
                 {UINT32_C(0x0f800000), "1.26217745E-29"},
                 {UINT32_C(0xea6c3793), "-7.1392233E25"},
                 {UINT32_C(0x7f7fffff), "3.4028235E38"},
                 {UINT32_C(0xff7fffff), "-3.4028235E38"}};

    const uint32_t zero_bits = 0;
    float zero;
    memcpy(&zero, &zero_bits, sizeof(zero));
    if (mc_java_float_to_string(zero, NULL) != MC_JAVA_FLOAT_FORMAT_INVALID_ARGUMENT)
        return 1;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mc_java_float_string result;
        memset(&result, 0xa5, sizeof(result));
        float value;
        memcpy(&value, &cases[i].bits, sizeof(value));
        if (mc_java_float_to_string(value, &result) != MC_JAVA_FLOAT_FORMAT_OK ||
            result.length >= sizeof(result.text) || result.length != strlen(cases[i].text) ||
            result.text[result.length] != '\0' ||
            memcmp(result.text, cases[i].text, result.length) != 0) {
            fprintf(stderr, "Float.toString literal %zu failed\n", i);
            return 1;
        }
    }
    puts("31 Float.toString literal cases and null-output control passed");
    return 0;
}
