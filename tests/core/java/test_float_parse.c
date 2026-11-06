#include "mc/core/java/float_parse.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool parsed(const mc_java_float_parse_text *text, mc_java_float_parse_status expected,
                   uint32_t bits) {
    const uint32_t canary = UINT32_C(0x3eaaaaab);
    float out;
    memcpy(&out, &canary, sizeof(out));
    const mc_java_float_parse_status status = mc_java_float_parse(text, &out);
    const uint32_t expected_bits = expected == MC_JAVA_FLOAT_PARSE_OK ? bits : canary;
    if (status != expected || float_bits(out) != expected_bits) {
        fprintf(stderr, "status %d/%d; bits %08x/%08x\n", (int)status, (int)expected,
                (unsigned int)float_bits(out), (unsigned int)expected_bits);
        return false;
    }
    return true;
}

static bool ascii(const char *input, mc_java_float_parse_status status, uint32_t bits) {
    uint16_t units[2048];
    const size_t length = strlen(input);
    if (length > COUNT(units))
        return false;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)input[i];
    const mc_java_float_parse_text text = {units, length};
    return parsed(&text, status, bits);
}

static int literals(void) {
    int result = 1;

    /* Literal endpoints/ties derive from binary32 integer significands, not a
     * CRT/JVM conversion or a second copy of the parser. */
    static const struct {
        const char *text;
        uint32_t bits;
    } cases[] = {
        {"0", 0},
        {"-0", 0x80000000},
        {"+0.0f", 0},
        {"-0e9999999999999999999999", 0x80000000},
        {"1", 0x3f800000},
        {"-1D", 0xbf800000},
        {".5F", 0x3f000000},
        {"1.e+0d", 0x3f800000},
        {"001.25", 0x3fa00000},
        {"\t\r\n +1.5 \x1f", 0x3fc00000},
        {"NaN", 0x7fc00000},
        {"-NaN", 0x7fc00000},
        {"+NaN", 0x7fc00000},
        {"Infinity", 0x7f800000},
        {"-Infinity", 0xff800000},
        {"1e99999999999999999999999", 0x7f800000},
        {"-1e-9999999999999999999999", 0x80000000},
        {"0X.8P1d", 0x3f800000},
        {"0x1.fp+3F", 0x41780000},
        {"-0x0p999999999999999999999", 0x80000000},
        {"0x1p-149", 1},
        {"0x1p-150", 0},
        {"-0x1p-150", 0x80000000},
        {"0x1.000001p-150", 1},
        {"0x1.fffffcp-127", 0x007fffff},
        {"0x1.fffffdp-127", 0x007fffff},
        {"0x1.fffffep-127", 0x00800000},
        {"1.000000059604644775390625", 0x3f800000},
        {"1.00000005960464477539062499999999999999999999", 0x3f800000},
        {"1.00000005960464477539062500000000000000000001", 0x3f800001},
        {"1.000000178813934326171875", 0x3f800002},
        {"0x1.000001p0", 0x3f800000},
        {"0x1.000003p0", 0x3f800002},
        {"0x1.fffffep127", 0x7f7fffff},
        {"0x1.fffffefffffffp127", 0x7f7fffff},
        {"0x1.ffffffp127", 0x7f800000},
        {"0x1.ffffff0000001p127", 0x7f800000},
        {"340282346638528859811704183484516925440", 0x7f7fffff},
        {"340282356779733661637539395458142568447", 0x7f7fffff},
        {"340282356779733661637539395458142568448", 0x7f800000},
        {"340282356779733661637539395458142568449", 0x7f800000},
        {"7."
         "00649232162408535461864791644958065640130970938257885878534141944895541342930300743319094"
         "181060791015625e-46",
         0},
        {"7."
         "00649232162408535461864791644958065640130970938257885878534141944895541342930300743319094"
         "1810607910156251e-46",
         1},
    };

    for (size_t i = 0; i < COUNT(cases); ++i) {
        const bool valid = ascii(cases[i].text, MC_JAVA_FLOAT_PARSE_OK, cases[i].bits);
        if (!valid)
            fprintf(stderr, "literal case %zu: %s\n", i, cases[i].text);
        CHECK(valid);
    }
    static const char *const invalid[] = {"",
                                          " ",
                                          ".",
                                          "+",
                                          "-",
                                          "true",
                                          "false",
                                          "nan",
                                          "inf",
                                          "NaNf",
                                          "InfinityD",
                                          "++1",
                                          "1 0",
                                          "1_0",
                                          "1,5",
                                          "1e",
                                          "1e+",
                                          "1e-",
                                          ".e1",
                                          "1ff",
                                          "1.2.3",
                                          "0x1",
                                          "0x.p0",
                                          "0x1p",
                                          "0x1p+",
                                          "0x1e1",
                                          "0x1p0ff",
                                          "0x1.2p1.5",
                                          "1e999999999999999999999x",
                                          "0x1p-999999999999999999999_"};
    for (size_t i = 0; i < COUNT(invalid); ++i)
        CHECK(ascii(invalid[i], MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT, 0));
    result = 0;
cleanup:
    return result;
}

static int tails(void) {
    int result = 1;
    char text[1500];
    const char *midpoint = "1.000000059604644775390625";
    const size_t prefix = strlen(midpoint);
    memcpy(text, midpoint, prefix);
    memset(text + prefix, '0', 1024u - prefix);
    text[1024] = 0;
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x3f800000)));
    text[1023] = '1';
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x3f800001)));
    memcpy(text, "1.000000059604644775390624", prefix);
    memset(text + prefix, '9', 1024u - prefix);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x3f800000)));
    memcpy(text, "0x1.000001", 10);
    memset(text + 10, '0', 1014);
    memcpy(text + 1024, "p0", 3);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x3f800000)));
    text[1023] = '1';
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x3f800001)));
    text[0] = '1';
    memset(text + 1, '0', 1000);
    memcpy(text + 1001, "e-1000", 7);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x3f800000)));
    text[0] = '.';
    memset(text + 1, '0', 1000);
    memcpy(text + 1001, "1e1001", 7);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x3f800000)));
    memcpy(text, "0x1", 3);
    memset(text + 3, '0', 1000);
    memcpy(text + 1003, "p-4000", 7);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x3f800000)));
    const char *half_minimum = "7."
                               "0064923216240853546186479164495806564013097093825788587853414194489"
                               "5541342930300743319094181060791015625";
    const size_t half_length = strlen(half_minimum);
    memcpy(text, half_minimum, half_length);
    memset(text + half_length, '0', 1024u - half_length);
    memcpy(text + 1024, "e-46", 5);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, 0));
    text[1023] = '1';
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, 1));
    memcpy(text, "0x1.", 4);
    memset(text + 4, '0', 1020);
    memcpy(text + 1024, "p-150", 6);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, 0));
    text[1023] = '1';
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, 1));
    text[2] = 'f';
    text[1023] = '0';
    memcpy(text + 1024, "p-153", 6);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, 1));
    memcpy(text, "1.", 2);
    memset(text + 2, '0', 1022);
    memcpy(text + 1024, "e-47", 5);
    CHECK(ascii(text, MC_JAVA_FLOAT_PARSE_OK, 0));
    result = 0;
cleanup:
    return result;
}

static int counted_and_budget(void) {
    int result = 1;
    uint16_t *large = NULL;
    const uint16_t edge_nul[] = {0, 32, '-', '0', 0, 9};
    mc_java_float_parse_text text = {edge_nul, COUNT(edge_nul)};
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_OK, UINT32_C(0x80000000)));
    const uint16_t interior[] = {'1', 0, '0'}, unicode_digit[] = {0xff11}, surrogate[] = {0xd800},
                   bom[] = {0xfeff, '1'};
    text = (mc_java_float_parse_text){interior, COUNT(interior)};
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT, 0));
    text = (mc_java_float_parse_text){unicode_digit, COUNT(unicode_digit)};
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT, 0));
    text = (mc_java_float_parse_text){surrogate, COUNT(surrogate)};
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT, 0));
    text = (mc_java_float_parse_text){bom, COUNT(bom)};
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT, 0));
    CHECK(parsed(NULL, MC_JAVA_FLOAT_PARSE_NULL_TEXT, 0));
    text = (mc_java_float_parse_text){NULL, 0};
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT, 0));
    text.length = 1;
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_INVALID_ARGUMENT, 0));
    text.length = (size_t)MC_JAVA_FLOAT_PARSE_MAX_UNITS + 1u;
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_LIMIT, 0));
    CHECK(mc_java_float_parse(NULL, NULL) == MC_JAVA_FLOAT_PARSE_INVALID_ARGUMENT);
    large = malloc((size_t)MC_JAVA_FLOAT_PARSE_MAX_UNITS * sizeof(*large));
    CHECK(large != NULL);
    for (size_t i = 0; i < MC_JAVA_FLOAT_PARSE_MAX_UNITS; ++i)
        large[i] = '0';
    text = (mc_java_float_parse_text){large, MC_JAVA_FLOAT_PARSE_MAX_UNITS};
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_OK, 0));
    large[text.length - 1u] = '_';
    CHECK(parsed(&text, MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT, 0));
    result = 0;
cleanup:
    free(large);
    return result;
}

int main(void) {
    if (literals() || tails() || counted_and_budget())
        return 1;
    printf("PASS: %u counted UTF16 Float.parseFloat native checks\n", checks);
    return 0;
}
