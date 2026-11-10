#include "../../../src/core/java/string_case_internal.h"
#include "mc/core/java/string_case.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "String case:%d: %s\n", __LINE__, #c);                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static FILE *open_file(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *out = NULL;
    return fopen_s(&out, path, mode) == 0 ? out : NULL;
#else
    return fopen(path, mode);
#endif
}

static bool put_u32(FILE *out, uint32_t value) {
    const uint8_t bytes[] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16), (uint8_t)(value >> 8),
                             (uint8_t)value};
    return fwrite(bytes, 1, sizeof(bytes), out) == sizeof(bytes);
}

static bool read_u32(FILE *in, uint32_t *out) {
    uint8_t bytes[4];
    if (fread(bytes, 1, sizeof(bytes), in) != sizeof(bytes))
        return false;
    *out = (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
    return true;
}

static bool read_units(FILE *in, uint16_t *units, uint32_t *length) {
    if (!read_u32(in, length) || *length > 64)
        return false;
    for (uint32_t i = 0; i < *length; ++i) {
        const int high = fgetc(in), low = fgetc(in);
        if (high == EOF || low == EOF)
            return false;
        units[i] = (uint16_t)((unsigned int)high << 8 | (unsigned int)low);
    }
    return true;
}

static int fold_output(const char *path) {
    FILE *out = open_file(path, "wb");
    if (out == NULL)
        return 1;
    bool okay = true;
    for (uint32_t value = 0; value < UINT32_C(0x110000) && okay; ++value)
        okay = put_u32(out, mc_java_string_case_fold(value));
    if (fclose(out) != 0)
        okay = false;
    if (!okay)
        return 1;
    puts("String case folds: 1114112 values");
    return 0;
}

static int pairs(const char *path) {
    FILE *in = open_file(path, "rb");
    if (in == NULL)
        return 1;
    uint32_t magic = 0, count = 0;
    bool okay = read_u32(in, &magic) && magic == UINT32_C(0x53434931) && read_u32(in, &count) &&
                count != 0 && count <= UINT32_C(2000000);
    uint32_t matched = 0;
    for (uint32_t i = 0; i < count && okay; ++i) {
        uint16_t left[64], right[64];
        uint32_t left_length = 0, right_length = 0;
        okay = read_units(in, left, &left_length) && read_units(in, right, &right_length);
        if (!okay)
            break;
        const int expected = fgetc(in);
        if (expected != 0 && expected != 1) {
            okay = false;
            break;
        }
        const bool actual =
            mc_java_string_equals_ignore_case(left, left_length, right, right_length);
        if (actual != (expected != 0)) {
            fprintf(stderr, "String case reference mismatch at pair %" PRIu32 "\n", i);
            okay = false;
            break;
        }
        ++matched;
    }
    if (okay && fgetc(in) != EOF)
        okay = false;
    if (fclose(in) != 0)
        okay = false;
    if (!okay)
        return 1;
    printf("String case pairs: %" PRIu32 " checks\n", matched);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--fold-output") == 0)
        return fold_output(argv[2]);
    if (argc == 3 && strcmp(argv[1], "--pairs") == 0)
        return pairs(argv[2]);
    if (argc != 1)
        return 2;
    const uint16_t upper[] = {'W', 'o', 'L', 'F'};
    const uint16_t lower[] = {'w', 'O', 'l', 'f'};
    const uint16_t greek_upper[] = {0x03a3};
    const uint16_t greek_final[] = {0x03c2};
    const uint16_t deseret_upper[] = {0xd801, 0xdc00};
    const uint16_t deseret_lower[] = {0xd801, 0xdc28};
    const uint16_t pair_boundary[] = {0xd801, 0xdc28, 0xd801};
    const uint16_t same_boundary[] = {0xd801, 0xdc00, 0xd801};
    const uint16_t sharp_s[] = {0x00df};
    const uint16_t two_s[] = {'S', 'S'};
    const uint16_t dotted_i[] = {0x0130}, dotless_i[] = {0x0131}, normal_i[] = {'i'};
    const uint16_t kelvin[] = {0x212a}, normal_k[] = {'K'};
    const uint16_t long_s[] = {0x017f}, normal_s[] = {'S'};
    const uint16_t capital_sharp_s[] = {0x1e9e};
    const uint16_t adlam_upper[] = {0xd83a, 0xdd00}, adlam_lower[] = {0xd83a, 0xdd22};
    const uint16_t high_alone[] = {0xd801}, low_alone[] = {0xdc00};
    const uint16_t malformed_upper[] = {0xdc00, 'A', 0xd801};
    const uint16_t malformed_lower[] = {0xdc00, 'a', 0xd801};
    CHECK(mc_java_string_equals_ignore_case(NULL, 0, NULL, 0));
    CHECK(!mc_java_string_equals_ignore_case(NULL, 1, upper, 1));
    CHECK(!mc_java_string_equals_ignore_case(upper, 1, NULL, 1));
    CHECK(mc_java_string_equals_ignore_case(upper, COUNT(upper), lower, COUNT(lower)));
    CHECK(mc_java_string_equals_ignore_case(greek_upper, 1, greek_final, 1));
    CHECK(mc_java_string_equals_ignore_case(deseret_upper, 2, deseret_lower, 2));
    CHECK(mc_java_string_equals_ignore_case(pair_boundary, 3, same_boundary, 3));
    CHECK(!mc_java_string_equals_ignore_case(sharp_s, 1, two_s, 2));
    CHECK(!mc_java_string_equals_ignore_case(upper, 3, lower, 4));
    CHECK(mc_java_string_equals_ignore_case(dotted_i, 1, dotless_i, 1));
    CHECK(mc_java_string_equals_ignore_case(dotted_i, 1, normal_i, 1));
    CHECK(mc_java_string_equals_ignore_case(dotless_i, 1, normal_i, 1));
    CHECK(mc_java_string_equals_ignore_case(kelvin, 1, normal_k, 1));
    CHECK(mc_java_string_equals_ignore_case(long_s, 1, normal_s, 1));
    CHECK(mc_java_string_equals_ignore_case(sharp_s, 1, capital_sharp_s, 1));
    CHECK(mc_java_string_equals_ignore_case(adlam_upper, 2, adlam_lower, 2));
    CHECK(!mc_java_string_equals_ignore_case(high_alone, 1, low_alone, 1));
    CHECK(mc_java_string_equals_ignore_case(high_alone, 1, high_alone, 1));
    CHECK(mc_java_string_equals_ignore_case(malformed_upper, 3, malformed_lower, 3));
    CHECK(mc_java_string_case_fold(UINT32_MAX) == UINT32_MAX);
    uint64_t fingerprint = UINT64_C(0xcbf29ce484222325);
    for (uint32_t value = 0; value < UINT32_C(0x110000); ++value) {
        const uint32_t folded = mc_java_string_case_fold(value);
        for (unsigned int shift = 24;; shift -= 8) {
            fingerprint = (fingerprint ^ (uint8_t)(folded >> shift)) * UINT64_C(0x100000001b3);
            if (shift == 0)
                break;
        }
    }
    CHECK(fingerprint == UINT64_C(0x5924be167d7ccace));
    printf("String case: %u checks\n", checks);
    return 0;
}
