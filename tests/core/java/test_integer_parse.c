#include "mc/core/java/integer_parse.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define REQUIRE(condition)                                                                         \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool parsed(const uint16_t *units, size_t length, mc_java_integer_parse_status expected,
                   int32_t value) {
    const mc_java_integer_parse_text text = {units, length};
    int32_t out = 193;
    return mc_java_integer_parse(&text, &out) == expected &&
           out == (expected == MC_JAVA_INTEGER_PARSE_OK ? value : 193);
}

static bool ascii(const char *input, mc_java_integer_parse_status expected, int32_t value) {
    uint16_t units[128];
    const size_t length = strlen(input);
    if (length > COUNT(units))
        return false;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)input[i];
    return parsed(units, length, expected, value);
}

int main(void) {
    int result = 1;
    uint16_t *zeroes = NULL;

    static const struct {
        const char *text;
        int32_t value;
    } valid[] = {{"0", 0},
                 {"-0", 0},
                 {"+0", 0},
                 {"17", 17},
                 {"-31", -31},
                 {"0000000018", 18},
                 {"2147483647", INT32_MAX},
                 {"-2147483648", INT32_MIN},
                 {"+2147483647", INT32_MAX},
                 {"-0002147483648", INT32_MIN}};

    for (size_t i = 0; i < COUNT(valid); ++i)
        REQUIRE(ascii(valid[i].text, MC_JAVA_INTEGER_PARSE_OK, valid[i].value));
    static const char *const invalid[] = {"",
                                          "+",
                                          "-",
                                          " 1",
                                          "1 ",
                                          "\t1",
                                          "1\n",
                                          "1.0",
                                          "0x10",
                                          "0b10",
                                          "1_0",
                                          "++1",
                                          "+-1",
                                          "-+1",
                                          "--1",
                                          "2147483648",
                                          "-2147483649",
                                          "+2147483648",
                                          "99999999999999999999"};
    for (size_t i = 0; i < COUNT(invalid); ++i)
        REQUIRE(ascii(invalid[i], MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    const uint16_t mixed[] = {'-', 0x0661, 0xff12, 0x0969, '4'};
    REQUIRE(parsed(mixed, COUNT(mixed), MC_JAVA_INTEGER_PARSE_OK, -1234));
    const uint16_t devanagari[] = {0x0966, 0x096d};
    REQUIRE(parsed(devanagari, COUNT(devanagari), MC_JAVA_INTEGER_PARSE_OK, 7));
    const uint16_t non_ascii_minus[] = {0x2212, '1'};
    REQUIRE(
        parsed(non_ascii_minus, COUNT(non_ascii_minus), MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    const uint16_t surrogate_pair[] = {0xd835, 0xdfce}; /* Supplementary mathematical digit zero. */
    REQUIRE(parsed(surrogate_pair, COUNT(surrogate_pair), MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    const uint16_t superscript[] = {0x00b2};
    REQUIRE(parsed(superscript, COUNT(superscript), MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    const uint16_t nul[] = {'1', 0, '2'};
    REQUIRE(parsed(nul, COUNT(nul), MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    const uint16_t *one = mixed + 3;
    REQUIRE(parsed(one, 1, MC_JAVA_INTEGER_PARSE_OK, 3));
    REQUIRE(parsed(NULL, 0, MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT, 0));
    REQUIRE(parsed(NULL, 1, MC_JAVA_INTEGER_PARSE_INVALID_ARGUMENT, 0));
    REQUIRE(parsed(one, MC_JAVA_INTEGER_PARSE_MAX_UNITS + 1u, MC_JAVA_INTEGER_PARSE_LIMIT, 0));
    int32_t unchanged = 193;
    REQUIRE(mc_java_integer_parse(NULL, &unchanged) == MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT &&
            unchanged == 193);
    REQUIRE(mc_java_integer_parse(NULL, NULL) == MC_JAVA_INTEGER_PARSE_INVALID_ARGUMENT);
    zeroes = calloc(MC_JAVA_INTEGER_PARSE_MAX_UNITS, sizeof(*zeroes));
    REQUIRE(zeroes != NULL);
    for (size_t i = 0; i < MC_JAVA_INTEGER_PARSE_MAX_UNITS; ++i)
        zeroes[i] = '0';
    zeroes[MC_JAVA_INTEGER_PARSE_MAX_UNITS - 1u] = '7';
    REQUIRE(parsed(zeroes, MC_JAVA_INTEGER_PARSE_MAX_UNITS, MC_JAVA_INTEGER_PARSE_OK, 7));
    result = 0;
cleanup:
    free(zeroes);
    if (result == 0)
        printf("PASS: %u counted UTF16 Integer.parseInt checks\n", checks);
    return result;
}
