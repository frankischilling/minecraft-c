#include "mc/core/java/float_parse.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Input only: header then P<TAB>id<TAB>UTF16BE-hex, or N<TAB>id<TAB>empty.
 * No expected results, JAR, JVM, float formatter or CRT number parser. */
enum { ROW_LIMIT = 10000, ID_LIMIT = 80 };

#define LINE_LIMIT ((size_t)MC_JAVA_FLOAT_PARSE_MAX_UNITS * 4u + ID_LIMIT + 4u)
#define FILE_LIMIT (128u * 1024u * 1024u)

static int nibble(unsigned char byte) {
    if (byte >= '0' && byte <= '9')
        return byte - '0';
    if (byte >= 'a' && byte <= 'f')
        return byte - 'a' + 10;
    return -1;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const char *status_name(mc_java_float_parse_status status) {
    switch (status) {
        case MC_JAVA_FLOAT_PARSE_OK:
            return "ok";
        case MC_JAVA_FLOAT_PARSE_INVALID_ARGUMENT:
            return "native-argument";
        case MC_JAVA_FLOAT_PARSE_LIMIT:
            return "native-limit";
        case MC_JAVA_FLOAT_PARSE_NULL_TEXT:
            return "null-text";
        case MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT:
            return "number-format";
    }
    return NULL;
}

static bool record(const char *line, size_t length, size_t ordinal) {
    if (length < 3u || (line[0] != 'P' && line[0] != 'N') || line[1] != '\t')
        return false;
    size_t separator = 2;
    while (separator < length && line[separator] != '\t') {
        const unsigned char c = (unsigned char)line[separator];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '/' || c == '-' || c == '_'))
            return false;
        ++separator;
    }
    const size_t id_length = separator - 2u;
    if (separator == length || id_length == 0 || id_length > ID_LIMIT)
        return false;
    const size_t encoded = length - separator - 1u;
    if (encoded % 4u != 0 || encoded / 4u > MC_JAVA_FLOAT_PARSE_MAX_UNITS ||
        (line[0] == 'N' && encoded != 0))
        return false;
    uint16_t *units = encoded == 0 ? NULL : malloc((encoded / 4u) * sizeof(*units));
    if (encoded != 0 && units == NULL)
        return false;
    bool result = false;
    for (size_t i = 0; i < encoded / 4u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 4u; ++j) {
            const int digit = nibble((unsigned char)line[separator + 1u + i * 4u + j]);
            if (digit < 0)
                goto cleanup;
            value = value * 16u + (unsigned int)digit;
        }
        units[i] = (uint16_t)value;
    }
    const mc_java_float_parse_text text = {units, encoded / 4u};
    const uint32_t canary = UINT32_C(0x3eaaaaab);
    float value;
    memcpy(&value, &canary, sizeof(value));
    const mc_java_float_parse_status status =
        mc_java_float_parse(line[0] == 'N' ? NULL : &text, &value);
    const char *name = status_name(status);
    if (name == NULL || (status != MC_JAVA_FLOAT_PARSE_OK && float_bits(value) != canary))
        goto cleanup;
    printf("{\"type\":\"value\",\"ordinal\":%zu,\"id\":\"%.*s\",\"units\":%zu,\"status\":\"%s\","
           "\"bits\":\"%08" PRIx32 "\"}\n",
           ordinal, (int)id_length, line + 2, text.length, name, float_bits(value));
    result = true;
cleanup:
    free(units);
    return result;
}

int main(int argc, char **argv) {
    FILE *input = NULL;
    char *line = NULL;
    size_t capacity = 0, length = 0, bytes = 0, rows = 0;
    int result = 1;
    if (argc != 2)
        goto cleanup;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) < 0)
        goto cleanup;
#endif
#ifdef _MSC_VER
    if (fopen_s(&input, argv[1], "rb") != 0)
        goto cleanup;
#else
    input = fopen(argv[1], "rb");
#endif
    if (input == NULL)
        goto cleanup;
    static const char header[] = "minecraft-c-float-parse-inputs-v1\n";
    for (size_t i = 0; i < sizeof(header) - 1u; ++i)
        if (fgetc(input) != (unsigned char)header[i])
            goto cleanup;
    bytes = sizeof(header) - 1u;
    printf("{\"type\":\"header\",\"schema\":1,\"max_text_units\":%u}\n",
           MC_JAVA_FLOAT_PARSE_MAX_UNITS);
    for (;;) {
        const int next = fgetc(input);
        if (next == EOF)
            break;
        if (++bytes > FILE_LIMIT)
            goto cleanup;
        if (next == '\n') {
            if (rows == ROW_LIMIT || !record(line, length, rows))
                goto cleanup;
            ++rows;
            length = 0;
            continue;
        }
        if (length == LINE_LIMIT)
            goto cleanup;
        if (length == capacity) {
            size_t grown = capacity == 0 ? 256u : capacity * 2u;
            if (grown > LINE_LIMIT)
                grown = LINE_LIMIT;
            char *replacement = realloc(line, grown);
            if (replacement == NULL)
                goto cleanup;
            line = replacement;
            capacity = grown;
        }
        line[length++] = (char)(unsigned char)next;
    }
    if (length != 0 || ferror(input))
        goto cleanup;
    if (fclose(input) != 0) {
        input = NULL;
        goto cleanup;
    }
    input = NULL;
    printf("{\"type\":\"terminal\",\"rows\":%zu,\"status\":0}\n", rows);
    if (fflush(stdout) == 0)
        result = 0;
cleanup:
    if (input != NULL)
        (void)fclose(input);
    free(line);
    return result;
}
