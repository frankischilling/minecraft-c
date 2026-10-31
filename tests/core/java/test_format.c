#include "mc/core/java/format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define BLOCKS 2048u
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef union alignment {
    long double floating;
    void *pointer;
    uint64_t integer;
} alignment;

typedef struct arena {
    void *blocks[BLOCKS];
    size_t count, requests, bytes, fail_at;
    bool refused, invalid, exhausted;
} arena;

/* Retained, distinct, genuinely uncleared malloc blocks. No shadow owner or
 * production failure hook is used. Views retire before this arena does. */
static void *allocate(void *context, size_t bytes) {
    arena *a = context;
    ++a->requests;
    if (a->fail_at != 0 && a->requests == a->fail_at) {
        a->refused = true;
        return NULL;
    }
    if (a->count == BLOCKS || bytes > SIZE_MAX - a->bytes) {
        a->invalid = true;
        return NULL;
    }
    void *p = malloc(bytes == 0 ? 1 : bytes);
    if (p == NULL) {
        a->exhausted = true;
        return NULL;
    }
    if ((uintptr_t)p % _Alignof(alignment) != 0) {
        free(p);
        a->invalid = true;
        return NULL;
    }
    a->blocks[a->count++] = p;
    a->bytes += bytes;
    return p;
}

static void retire(arena *a) {
    for (size_t i = 0; i < a->count; ++i)
        free(a->blocks[i]);
    *a = (arena){0};
}

static mc_java_format_options options(arena *a) {
    static const uint16_t lf[] = {10};
    return (mc_java_format_options){
        {a, allocate}, MC_JAVA_FORMAT_LOCALE_ENGLISH, {lf, COUNT(lf)}, 4096};
}

static mc_java_format_text ascii(const char *source, uint16_t *storage) {
    size_t n = strlen(source);
    for (size_t i = 0; i < n; ++i)
        storage[i] = (unsigned char)source[i];
    return (mc_java_format_text){storage, n};
}

static bool equals(const mc_java_format_text *text, const uint16_t *units, size_t length) {
    if (text->length != length || (length != 0 && text->units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (text->units[i] != units[i])
            return false;
    return true;
}

static bool is_ascii(const mc_java_format_text *text, const char *expected) {
    size_t n = strlen(expected);
    if (text->length != n || (n != 0 && text->units == NULL))
        return false;
    for (size_t i = 0; i < n; ++i)
        if (text->units[i] != (unsigned char)expected[i])
            return false;
    return true;
}

static int ascii_case(arena *a, const char *pattern, const char *first, const char *second,
                      size_t count, bool null_array, mc_java_format_status expected,
                      const char *output) {
    int result = 1;
    uint16_t p[256], x[128], y[128];
    CHECK(strlen(pattern) < COUNT(p) && (first == NULL || strlen(first) < COUNT(x)) &&
          (second == NULL || strlen(second) < COUNT(y)) && count <= 2);
    mc_java_format_text text = ascii(pattern, p), one = {0}, two = {0};
    if (first != NULL)
        one = ascii(first, x);
    if (second != NULL)
        two = ascii(second, y);
    const mc_java_format_text *arguments[] = {first == NULL ? NULL : &one,
                                              second == NULL ? NULL : &two};
    static const uint16_t sentinel[] = {0x1234};
    mc_java_format_text out = {sentinel, 97};
    mc_java_format_error error = {0};
    mc_java_format_options opt = options(a);
    mc_java_format_status status = mc_java_format_strings(
        &opt, &text, null_array ? NULL : arguments, null_array ? 0 : count, &out, &error);
    CHECK(status == expected);
    if (status == MC_JAVA_FORMAT_OK)
        CHECK(output != NULL && is_ascii(&out, output));
    else
        CHECK(error.status == expected && out.units == sentinel && out.length == 97);
    CHECK(!a->invalid && !a->exhausted);
    result = 0;
cleanup:
    return result;
}

static int test_strings(void) {
    int result = 1;
    arena a = {0};
#define OK(p, x, y, n, s) CHECK(ascii_case(&a, p, x, y, n, false, MC_JAVA_FORMAT_OK, s) == 0)
    OK("literal", NULL, NULL, 0, "literal");
    OK("", NULL, NULL, 0, "");
    OK("[%s]|[%s]", "", NULL, 2, "[]|[null]");
    OK("%s/%<S/%s/%<s", "one", "two", 2, "one/ONE/two/two");
    OK("%2$s/%s/%<s/%1$s", "one", "two", 2, "two/one/one/one");
    OK("%2$s/%1$<s", "one", "two", 2, "two/two");
    OK("%2$s/%s/%s", "one", "two", 2, "two/one/two");
    OK("[%7.3s][%-7.3s][%1$.0s]", "abcdef", "xyzuvw", 2, "[    abc][xyz    ][]");
    OK("%b|%B", "", NULL, 2, "true|FALSE");
    OK("%.2b|%6.3B", "false", NULL, 2, "tr|   FAL");
    OK("%h|%H", "Aa", "BB", 2, "840|840");
    OK("%h|%H", "", NULL, 2, "0|NULL");
    OK("[%5%][%-3%]|%n", NULL, NULL, 0, "[    %][%  ]|\n");
    OK("%1$%/%1$n", NULL, NULL, 0, "%/\n");
    CHECK(ascii_case(&a, "%s|%s|%2$s|%<s", NULL, NULL, 0, true, MC_JAVA_FORMAT_OK,
                     "null|null|null|null") == 0);
    CHECK(ascii_case(&a, "%2147483647$s", NULL, NULL, 0, true, MC_JAVA_FORMAT_OK, "null") == 0);
#undef OK
    result = 0;
cleanup:
    retire(&a);
    return result;
}

typedef struct failure_case {
    const char *pattern;
    mc_java_format_status status;
    size_t arguments;
} failure_case;

static int test_validation(void) {
    int result = 1;
    arena a = {0};
    static const failure_case cases[] = {
        {"%", MC_JAVA_FORMAT_UNKNOWN_CONVERSION, 0},
        {"%q", MC_JAVA_FORMAT_UNKNOWN_CONVERSION, 0},
        {"%.s", MC_JAVA_FORMAT_UNKNOWN_CONVERSION, 0},
        {"%N", MC_JAVA_FORMAT_UNKNOWN_CONVERSION, 0},
        {"%tq", MC_JAVA_FORMAT_UNKNOWN_CONVERSION, 1},
        {"%--s", MC_JAVA_FORMAT_DUPLICATE_FLAGS, 0},
        {"%00d", MC_JAVA_FORMAT_DUPLICATE_FLAGS, 1},
        {"%1$<<s", MC_JAVA_FORMAT_DUPLICATE_FLAGS, 1},
        {"%+ d", MC_JAVA_FORMAT_ILLEGAL_FLAGS, 1},
        {"%-05d", MC_JAVA_FORMAT_ILLEGAL_FLAGS, 1},
        {"%#%", MC_JAVA_FORMAT_ILLEGAL_FLAGS, 0},
        {"%<%", MC_JAVA_FORMAT_ILLEGAL_FLAGS, 0},
        {"%+s", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%0s", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%#b", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%#h", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%#d", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%,x", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%,e", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%#g", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%(a", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%-s", MC_JAVA_FORMAT_MISSING_WIDTH, 0},
        {"%0d", MC_JAVA_FORMAT_MISSING_WIDTH, 1},
        {"%-0d", MC_JAVA_FORMAT_MISSING_WIDTH, 1},
        {"%1n", MC_JAVA_FORMAT_ILLEGAL_WIDTH, 0},
        {"%2147483648s", MC_JAVA_FORMAT_ILLEGAL_WIDTH, 0},
        {"%2147483648n", MC_JAVA_FORMAT_ILLEGAL_WIDTH, 0},
        {"%.2147483648s", MC_JAVA_FORMAT_ILLEGAL_PRECISION, 0},
        {"%.1c", MC_JAVA_FORMAT_ILLEGAL_PRECISION, 1},
        {"%.1d", MC_JAVA_FORMAT_ILLEGAL_PRECISION, 1},
        {"%.1n", MC_JAVA_FORMAT_ILLEGAL_PRECISION, 0},
        {"%.1tq", MC_JAVA_FORMAT_ILLEGAL_PRECISION, 1},
        {"%0$s", MC_JAVA_FORMAT_ILLEGAL_ARGUMENT_INDEX, 1},
        {"%2147483648$s", MC_JAVA_FORMAT_ILLEGAL_ARGUMENT_INDEX, 1},
        {"%0$--s", MC_JAVA_FORMAT_ILLEGAL_ARGUMENT_INDEX, 1},
        {"%s", MC_JAVA_FORMAT_MISSING_ARGUMENT, 0},
        {"%2$s", MC_JAVA_FORMAT_MISSING_ARGUMENT, 1},
        {"%<s", MC_JAVA_FORMAT_MISSING_ARGUMENT, 1},
        {"%#s", MC_JAVA_FORMAT_MISSING_ARGUMENT, 0},
        {"%#s", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1},
        {"%c", MC_JAVA_FORMAT_ILLEGAL_CONVERSION, 1},
        {"%d", MC_JAVA_FORMAT_ILLEGAL_CONVERSION, 1},
        {"%f", MC_JAVA_FORMAT_ILLEGAL_CONVERSION, 1},
        {"%tY", MC_JAVA_FORMAT_ILLEGAL_CONVERSION, 1},
        /* Parse validates the ENTIRE pattern before first argument access or
         * formatting: these later errors beat earlier missing/type/# errors. */
        {"%s %q", MC_JAVA_FORMAT_UNKNOWN_CONVERSION, 0},
        {"%d %q", MC_JAVA_FORMAT_UNKNOWN_CONVERSION, 1},
        {"%#s %--s", MC_JAVA_FORMAT_DUPLICATE_FLAGS, 1},
        {"%s %-s", MC_JAVA_FORMAT_MISSING_WIDTH, 0},
        {"%d %0$s", MC_JAVA_FORMAT_ILLEGAL_ARGUMENT_INDEX, 1},
        {"%#s %.1d", MC_JAVA_FORMAT_ILLEGAL_PRECISION, 0},
        /* Valid huge width must fail argument/type checking before padding. */
        {"%2147483647d", MC_JAVA_FORMAT_ILLEGAL_CONVERSION, 1},
        {"%2147483647s", MC_JAVA_FORMAT_MISSING_ARGUMENT, 0}};
    for (size_t i = 0; i < COUNT(cases); ++i)
        CHECK(ascii_case(&a, cases[i].pattern, "word", NULL, cases[i].arguments, false,
                         cases[i].status, NULL) == 0);
    result = 0;
cleanup:
    retire(&a);
    return result;
}

static int test_null_conversions(void) {
    int result = 1;
    arena a = {0};
    /* String arguments are never numeric/date/Character values; NULL is
     * admitted before those type checks, then precision/case/width apply. */
    static const char *const patterns[] = {"%c", "%C", "%d", "%o", "%x",  "%X",  "%e",  "%E", "%f",
                                           "%g", "%G", "%a", "%A", "%tY", "%TY", "%tH", "%Tc"};
    for (size_t i = 0; i < COUNT(patterns); ++i) {
        const char *expected =
            strchr("CXEGA", patterns[i][1]) != NULL || patterns[i][1] == 'T' ? "NULL" : "null";
        CHECK(ascii_case(&a, patterns[i], NULL, NULL, 1, false, MC_JAVA_FORMAT_OK, expected) == 0);
    }
    CHECK(ascii_case(&a, "%08d", NULL, NULL, 1, false, MC_JAVA_FORMAT_OK, "    null") == 0);
    CHECK(ascii_case(&a, "%.2E", NULL, NULL, 1, false, MC_JAVA_FORMAT_OK, "NU") == 0);
    CHECK(ascii_case(&a, "%6.2A", NULL, NULL, 1, false, MC_JAVA_FORMAT_OK, "    NU") == 0);
    result = 0;
cleanup:
    retire(&a);
    return result;
}

static int test_error_operands(void) {
    int result = 1;
    arena a = {0};

    typedef struct operand_case {
        const char *pattern;
        mc_java_format_status status;
        size_t offset, length;
        int32_t value;
        uint32_t flags;
        uint16_t conversion;
        bool date_time;
    } operand_case;

    /* Source Flags bit values and original whole-specifier spans. These are
     * native diagnostic operands, never purported Java Throwable identities. */
    const operand_case cases[] = {
        {"a%0$s!", MC_JAVA_FORMAT_ILLEGAL_ARGUMENT_INDEX, 1, 4, 0, 0, 0, false},
        {"x%--s!", MC_JAVA_FORMAT_DUPLICATE_FLAGS, 1, 4, 0, 1, 0, false},
        {"%+ d", MC_JAVA_FORMAT_ILLEGAL_FLAGS, 0, 4, 0, 24, 'd', false},
        {"[%#s]", MC_JAVA_FORMAT_FLAGS_CONVERSION_MISMATCH, 1, 3, 0, 4, 's', false},
        {"%.1TY", MC_JAVA_FORMAT_ILLEGAL_PRECISION, 0, 5, 1, 2, 'Y', true}};
    uint16_t p[64], x[16];
    const mc_java_format_text arg = ascii("word", x);
    const mc_java_format_text *args[] = {&arg};
    static const uint16_t marker[] = {0x1234};
    mc_java_format_options opt = options(&a);
    for (size_t i = 0; i < COUNT(cases); ++i) {
        mc_java_format_text pattern = ascii(cases[i].pattern, p), out = {marker, 99};
        mc_java_format_error error = {0};
        CHECK(mc_java_format_strings(&opt, &pattern, args, 1, &out, &error) == cases[i].status);
        CHECK(error.status == cases[i].status && error.offset == cases[i].offset &&
              error.length == cases[i].length);
        CHECK(error.value == cases[i].value && error.flags == cases[i].flags &&
              error.conversion == cases[i].conversion && error.date_time == cases[i].date_time);
        CHECK(out.units == marker && out.length == 99);
    }
    result = 0;
cleanup:
    retire(&a);
    return result;
}

static int unicode_case(arena *a, const char *pattern, const uint16_t *input, size_t input_length,
                        const uint16_t *expected, size_t expected_length) {
    int result = 1;
    uint16_t p[64];
    CHECK(strlen(pattern) < COUNT(p));
    mc_java_format_text fmt = ascii(pattern, p), arg = {input, input_length}, out = {0};
    const mc_java_format_text *args[] = {&arg};
    mc_java_format_options opt = options(a);
    CHECK(mc_java_format_strings(&opt, &fmt, args, COUNT(args), &out, NULL) == MC_JAVA_FORMAT_OK);
    CHECK(equals(&out, expected, expected_length) && !a->invalid && !a->exhausted);
    result = 0;
cleanup:
    return result;
}

static int test_unicode(void) {
    int result = 1;
    arena a = {0};
    const uint16_t sharp[] = {0xdf, 'x'}, ss[] = {'S', 'S'}, pad[] = {' ', 'S', 'S'},
                   right[] = {'S', 'S', ' '};
    const uint16_t ligature[] = {0xfb03}, ffi[] = {'F', 'F', 'I'};
    const uint16_t dotted[] = {'i', 0x131, 0x130, 0x307}, upper[] = {'I', 'I', 0x130, 0x307};
    const uint16_t deseret[] = {0xd801, 0xdc28}, deseret_upper[] = {0xd801, 0xdc00};
    const uint16_t odd[] = {0xd800, 'a', 0, 0xdc00}, odd_upper[] = {0xd800, 'A', 0, 0xdc00};
    const uint16_t hash[] = {0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
    const uint16_t hash_lower[] = {'8', 'f', 'b', '2', '7', '0', '3', 'f'};
    const uint16_t hash_upper[] = {'8', 'F', 'B', '2', '7', '0', '3', 'F'};
    CHECK(unicode_case(&a, "%.1S", sharp, COUNT(sharp), ss, COUNT(ss)) == 0);
    CHECK(unicode_case(&a, "%3.1S", sharp, COUNT(sharp), pad, COUNT(pad)) == 0);
    CHECK(unicode_case(&a, "%-3.1S", sharp, COUNT(sharp), right, COUNT(right)) == 0);
    CHECK(unicode_case(&a, "%2S", ligature, COUNT(ligature), ffi, COUNT(ffi)) == 0);
    CHECK(unicode_case(&a, "%S", dotted, COUNT(dotted), upper, COUNT(upper)) == 0);
    CHECK(unicode_case(&a, "%S", deseret, COUNT(deseret), deseret_upper, COUNT(deseret_upper)) ==
          0);
    CHECK(unicode_case(&a, "%.1S", deseret, COUNT(deseret), deseret, 1) == 0);
    CHECK(unicode_case(&a, "%S", odd, COUNT(odd), odd_upper, COUNT(odd_upper)) == 0);
    /* 31*h+UTF16 unit, wrapped at 32bits: five FFFF units ->8fb2703f. */
    CHECK(unicode_case(&a, "%h", hash, COUNT(hash), hash_lower, COUNT(hash_lower)) == 0);
    CHECK(unicode_case(&a, "%H", hash, COUNT(hash), hash_upper, COUNT(hash_upper)) == 0);
    result = 0;
cleanup:
    retire(&a);
    return result;
}

static int test_native_contracts(void) {
    int result = 1;
    arena a = {0};
    uint16_t p[80], x[16];
    mc_java_format_text pattern = ascii("%s", p), arg = ascii("i", x);
    const mc_java_format_text *args[] = {&arg};
    static const uint16_t marker[] = {0x1234};
    mc_java_format_text out = {marker, 99};
    mc_java_format_error error;
    mc_java_format_options opt = options(&a), bad = opt;
    CHECK(mc_java_format_strings(NULL, &pattern, args, 1, &out, &error) ==
          MC_JAVA_FORMAT_INVALID_ARGUMENT);
    CHECK(out.units == marker && out.length == 99);
    bad.allocator.allocate = NULL;
    CHECK(mc_java_format_strings(&bad, &pattern, args, 1, &out, NULL) ==
          MC_JAVA_FORMAT_INVALID_ARGUMENT);
    CHECK(mc_java_format_strings(&opt, &pattern, args, 1, NULL, NULL) ==
          MC_JAVA_FORMAT_INVALID_ARGUMENT);
    CHECK(mc_java_format_strings(&opt, &pattern, NULL, 1, &out, NULL) ==
          MC_JAVA_FORMAT_INVALID_ARGUMENT);
    mc_java_format_text invalid = {NULL, 1};
    CHECK(mc_java_format_strings(&opt, &invalid, args, 1, &out, NULL) ==
          MC_JAVA_FORMAT_INVALID_ARGUMENT);
    const mc_java_format_text *invalid_args[] = {&invalid};
    CHECK(mc_java_format_strings(&opt, &pattern, invalid_args, 1, &out, NULL) ==
          MC_JAVA_FORMAT_INVALID_ARGUMENT);
    CHECK(out.units == marker && out.length == 99);
    CHECK(mc_java_format_strings(&opt, NULL, args, 1, &out, &error) == MC_JAVA_FORMAT_NULL_FORMAT);
    CHECK(error.status == MC_JAVA_FORMAT_NULL_FORMAT && out.units == marker && out.length == 99);
    opt.locale = MC_JAVA_FORMAT_LOCALE_UNSUPPORTED;
    CHECK(mc_java_format_strings(&opt, &pattern, args, 1, &out, NULL) == MC_JAVA_FORMAT_OK &&
          is_ascii(&out, "i"));
    pattern = ascii("%S", p);
    out = (mc_java_format_text){marker, 99};
    CHECK(mc_java_format_strings(&opt, &pattern, args, 1, &out, &error) ==
          MC_JAVA_FORMAT_UNSUPPORTED_LOCALE);
    CHECK(error.status == MC_JAVA_FORMAT_UNSUPPORTED_LOCALE && out.units == marker &&
          out.length == 99);
    static const uint16_t crlf[] = {13, 10};
    opt = options(&a);
    opt.line_separator = (mc_java_format_text){crlf, COUNT(crlf)};
    pattern = ascii("a%nb", p);
    CHECK(mc_java_format_strings(&opt, &pattern, args, 0, &out, NULL) == MC_JAVA_FORMAT_OK &&
          is_ascii(&out, "a\r\nb"));
    opt.line_separator = (mc_java_format_text){NULL, 0};
    CHECK(mc_java_format_strings(&opt, &pattern, args, 0, &out, NULL) == MC_JAVA_FORMAT_OK &&
          is_ascii(&out, "ab"));
    opt = options(&a);
    opt.max_output_units = 2;
    pattern = ascii("%s", p);
    arg = ascii("abc", x);
    out = (mc_java_format_text){marker, 99};
    CHECK(mc_java_format_strings(&opt, &pattern, args, 1, &out, &error) == MC_JAVA_FORMAT_LIMIT);
    CHECK(error.status == MC_JAVA_FORMAT_LIMIT && out.units == marker && out.length == 99);
    const uint16_t two_sharp[] = {0xdf, 0xdf};
    arg = (mc_java_format_text){two_sharp, COUNT(two_sharp)};
    pattern = ascii("%S", p);
    opt.max_output_units = 3;
    CHECK(mc_java_format_strings(&opt, &pattern, args, 1, &out, NULL) == MC_JAVA_FORMAT_LIMIT &&
          out.units == marker && out.length == 99);
    opt.max_output_units = 4;
    CHECK(mc_java_format_strings(&opt, &pattern, args, 1, &out, NULL) == MC_JAVA_FORMAT_OK &&
          is_ascii(&out, "SSSS"));
    CHECK(!a.invalid && !a.exhausted);
    result = 0;
cleanup:
    retire(&a);
    return result;
}

static mc_java_format_status complex(arena *a, mc_java_format_text *out) {
    static const uint16_t p[] = {'[', '%', '1', '2', 'S', '|', '%', '1', '$', 'h',
                                 '|', '%', '.', '2', 's', '|', '%', 'n', ']'};
    static const uint16_t first[] = {0xdf, 'a', 'b', 'c'}, second[] = {'w', 'o', 'r', 'd'};
    const mc_java_format_text pattern = {p, COUNT(p)}, one = {first, COUNT(first)},
                              two = {second, COUNT(second)};
    const mc_java_format_text *args[] = {&one, &two};
    mc_java_format_options opt = options(a);
    return mc_java_format_strings(&opt, &pattern, args, COUNT(args), out, NULL);
}

static int test_allocator(void) {
    int result = 1;
    arena baseline = {0}, trial = {0};
    mc_java_format_text reference = {0};
    CHECK(complex(&baseline, &reference) == MC_JAVA_FORMAT_OK && baseline.requests != 0);
    size_t requests = baseline.requests;
    for (size_t fail = 1; fail <= requests; ++fail) {
        mc_java_format_options opt = options(&trial);
        static const uint16_t kept_units[] = {'k', 'e', 'p', 't'}, marker[] = {0x1234};
        const mc_java_format_text kept_pattern = {kept_units, COUNT(kept_units)};
        mc_java_format_text kept = {0};
        CHECK(mc_java_format_strings(&opt, &kept_pattern, NULL, 0, &kept, NULL) ==
              MC_JAVA_FORMAT_OK);
        CHECK(is_ascii(&kept, "kept"));
        trial.fail_at = trial.requests + fail;
        mc_java_format_text out = {marker, 99};
        CHECK(complex(&trial, &out) == MC_JAVA_FORMAT_ALLOCATION_FAILED);
        CHECK(trial.refused && out.units == marker && out.length == 99 && is_ascii(&kept, "kept"));
        CHECK(!trial.invalid && !trial.exhausted);
        trial.fail_at = 0;
        CHECK(complex(&trial, &out) == MC_JAVA_FORMAT_OK &&
              equals(&out, reference.units, reference.length));
        CHECK(is_ascii(&kept, "kept"));
        retire(&trial);
    }
    CHECK(!baseline.invalid && !baseline.exhausted);
    result = 0;
cleanup:
    retire(&trial);
    retire(&baseline);
    return result;
}

int main(void) {
    if (test_strings() != 0 || test_validation() != 0 || test_null_conversions() != 0 ||
        test_error_operands() != 0 || test_unicode() != 0 || test_native_contracts() != 0 ||
        test_allocator() != 0)
        return 1;
    return printf("String-format native contracts: %u checks passed\n", checks) < 0 ? 1 : 0;
}
