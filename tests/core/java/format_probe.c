#include "mc/core/java/format.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define CASES 1024u
#define ARGUMENTS 32u
#define TEXT_UNITS 8192u
#define LINE_BYTES 65536u
#define RUNTIME_BYTES (1024u * 1024u)

typedef struct declaration {
    mc_java_format_text pattern, arguments[ARGUMENTS];
    bool null_pattern, null_array, null_argument[ARGUMENTS];
    size_t count;
} declaration;

typedef union allocation allocation;

union allocation {
    struct {
        allocation *next;
    } link;

    long double floating;
    void *pointer;
    uint64_t integer;
};

typedef struct arena {
    allocation *head;
    size_t bytes;
} arena;

/* Header storage is separate from the uninitialized max-aligned payload.
 * Every successful allocation remains live until this one case retires. */
static void *allocate(void *context, size_t bytes) {
    arena *a = context;
    size_t payload = bytes == 0 ? 1 : bytes;
    if (payload > RUNTIME_BYTES - sizeof(allocation) ||
        a->bytes > RUNTIME_BYTES - sizeof(allocation) - payload)
        return NULL;
    allocation *block = malloc(sizeof(*block) + payload);
    if (block == NULL)
        return NULL;
    block->link.next = a->head;
    a->head = block;
    a->bytes += sizeof(*block) + payload;
    return block + 1;
}

static void retire(arena *a) {
    while (a->head != NULL) {
        allocation *next = a->head->link.next;
        free(a->head);
        a->head = next;
    }
    a->bytes = 0;
}

/* Exactly ASCII/TAB records terminated by LF. Missing final LF, CRLF,
 * embedded controls, overlong lines and I/O errors are rejected. */
static int read_line(char *line) {
    size_t length = 0;
    for (;;) {
        int c = fgetc(stdin);
        if (c == EOF)
            return ferror(stdin) || length != 0 ? -1 : 0;
        if (c == '\n') {
            line[length] = 0;
            return 1;
        }
        if (length == LINE_BYTES || (c != '\t' && (c < 32 || c > 126)))
            return -1;
        line[length++] = (char)c;
    }
}

static size_t fields(char *line, char **out) {
    size_t count = 1;
    out[0] = line;
    for (char *p = line; *p != 0; ++p) {
        if (*p == '\t') {
            if (count == 5)
                return 0;
            *p = 0;
            out[count++] = p + 1;
        }
    }
    for (size_t i = 0; i < count; ++i)
        if (*out[i] == 0)
            return 0;
    return count;
}

static bool decimal(const char *text, size_t maximum, size_t *out) {
    if (*text == 0 || (text[0] == '0' && text[1] != 0))
        return false;
    size_t value = 0;
    for (const char *p = text; *p != 0; ++p) {
        if (*p < '0' || *p > '9')
            return false;
        size_t digit = (unsigned char)*p - (unsigned char)'0';
        if (digit > maximum || value > (maximum - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    *out = value;
    return true;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static bool text(const char *encoded, bool allow_null, mc_java_format_text *out, bool *is_null) {
    *is_null = false;
    if (strcmp(encoded, "NULL") == 0) {
        *is_null = allow_null;
        return allow_null;
    }
    if (strcmp(encoded, "-") == 0)
        return true;
    size_t length = strlen(encoded);
    if (length == 0 || length % 4 != 0 || length / 4 > TEXT_UNITS)
        return false;
    uint16_t *units = malloc(length / 4 * sizeof(*units));
    if (units == NULL)
        return false;
    out->units = units;
    out->length = length / 4;
    for (size_t i = 0; i < out->length; ++i) {
        unsigned int unit = 0;
        for (size_t j = 0; j < 4; ++j) {
            int digit = hex_digit(encoded[i * 4 + j]);
            if (digit < 0)
                return false;
            unit = (unit << 4) | (unsigned int)digit;
        }
        units[i] = (uint16_t)unit;
    }
    return true;
}

static bool parse(char *line, declaration *cases, size_t *count, mc_java_format_text *separator) {
    char *f[5];
    bool unused;
    if (read_line(line) != 1 || strcmp(line, "FORMAT_INPUT_V1") != 0 || read_line(line) != 1 ||
        fields(line, f) != 3 || strcmp(f[0], "ENV") != 0 || strcmp(f[1], "LINE_SEPARATOR") != 0 ||
        !text(f[2], false, separator, &unused))
        return false;
    for (;;) {
        if (read_line(line) != 1)
            return false;
        if (strcmp(line, "END") == 0)
            return read_line(line) == 0;
        size_t index;
        if (*count == CASES || fields(line, f) != 4 || strcmp(f[0], "CASE") != 0 ||
            !decimal(f[1], CASES - 1, &index) || index != *count)
            return false;
        declaration *c = &cases[*count];
        if (!text(f[2], true, &c->pattern, &c->null_pattern))
            return false;
        if (strcmp(f[3], "NULL") == 0)
            c->null_array = true;
        else if (!decimal(f[3], ARGUMENTS, &c->count))
            return false;
        for (size_t i = 0; i < c->count; ++i)
            if (read_line(line) != 1 || fields(line, f) != 2 || strcmp(f[0], "ARG") != 0 ||
                !text(f[1], true, &c->arguments[i], &c->null_argument[i]))
                return false;
        if (read_line(line) != 1 || strcmp(line, "RUN") != 0)
            return false;
        ++*count;
    }
}

static bool print_text(const mc_java_format_text *value) {
    static const char digits[] = "0123456789abcdef";
    if (value->length == 0)
        return fputc('-', stdout) != EOF;
    if (value->units == NULL || value->length > TEXT_UNITS)
        return false;
    for (size_t i = 0; i < value->length; ++i)
        for (int shift = 12; shift >= 0; shift -= 4)
            if (fputc(digits[(value->units[i] >> shift) & 15], stdout) == EOF)
                return false;
    return true;
}

static bool run(const declaration *c, size_t index, const mc_java_format_text *separator) {
    arena a = {0};
    const mc_java_format_text *arguments[ARGUMENTS];
    for (size_t i = 0; i < c->count; ++i)
        arguments[i] = c->null_argument[i] ? NULL : &c->arguments[i];
    mc_java_format_options options = {
        {&a, allocate}, MC_JAVA_FORMAT_LOCALE_ENGLISH, *separator, TEXT_UNITS};
    mc_java_format_text result = {0};
    mc_java_format_error error = {0};
    mc_java_format_status status =
        mc_java_format_strings(&options, c->null_pattern ? NULL : &c->pattern,
                               c->null_array ? NULL : arguments, c->count, &result, &error);
    bool good;
    if (status == MC_JAVA_FORMAT_OK)
        good = printf("RESULT\t%zu\tOK\t", index) >= 0 && print_text(&result) &&
               fputc('\n', stdout) != EOF;
    else
        good =
            printf("RESULT\t%zu\tERROR\t%u\t%" PRId32 "\t%" PRIu32 "\t%u\t%u\t%zu\t%zu\n", index,
                   (unsigned int)status, error.value, error.flags, (unsigned int)error.conversion,
                   error.date_time ? 1u : 0u, error.offset, error.length) >= 0;
    retire(&a);
    return good;
}

int main(void) {
    int result = 1;
    bool accepted = false;
    size_t count = 0;
    char *line = NULL;
    declaration *cases = NULL;
    mc_java_format_text separator = {0};
#ifdef _WIN32
    if (_setmode(_fileno(stderr), _O_BINARY) == -1 || _setmode(_fileno(stdin), _O_BINARY) == -1 ||
        _setmode(_fileno(stdout), _O_BINARY) == -1)
        goto cleanup;
#endif
    line = malloc(LINE_BYTES + 1u);
    cases = calloc(CASES, sizeof(*cases));
    if (line == NULL || cases == NULL || !parse(line, cases, &count, &separator))
        goto cleanup;
    accepted = true;
    for (size_t i = 0; i < count; ++i)
        if (!run(&cases[i], i, &separator))
            goto cleanup;
    if (printf("END\t%zu\n", count) < 0 || fflush(stdout) != 0 || ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    if (cases != NULL)
        for (size_t i = 0; i < CASES; ++i) {
            free((void *)cases[i].pattern.units);
            for (size_t j = 0; j < ARGUMENTS; ++j)
                free((void *)cases[i].arguments[j].units);
        }
    free((void *)separator.units);
    free(cases);
    free(line);
    if (!accepted)
        fputs("invalid format input\n", stderr);
    return result;
}
