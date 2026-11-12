#include "mc/core/java/float_format.h"

#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Input only: header followed by counted ASCII id<TAB>eight binary32 hex digits.
 * Expected strings, JARs and JVMs are not inputs to this producer. */
enum { ROW_LIMIT = 65536, ID_LIMIT = 80, LINE_LIMIT = ID_LIMIT + 1 + 8 };

#define FILE_LIMIT (8u * 1024u * 1024u)

static int nibble(unsigned char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static bool record(const char *line, size_t length, size_t ordinal) {
    size_t id_length = 0;
    while (id_length < length && line[id_length] != '\t') {
        const unsigned char unit = (unsigned char)line[id_length];
        if (!((unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') ||
              (unit >= '0' && unit <= '9') || unit == '/' || unit == '-' || unit == '_'))
            return false;
        ++id_length;
    }
    if (id_length == 0 || id_length > ID_LIMIT || length != id_length + 1u + 8u)
        return false;
    uint32_t bits = 0;
    for (size_t i = 0; i < 8u; ++i) {
        const int digit = nibble((unsigned char)line[id_length + 1u + i]);
        if (digit < 0)
            return false;
        bits = bits << 4 | (uint32_t)digit;
    }
    float value;
    memcpy(&value, &bits, sizeof(value));
    mc_java_float_string result;
    memset(&result, 0xa5, sizeof(result));
    const mc_java_float_format_status status = mc_java_float_to_string(value, &result);
    if (status != MC_JAVA_FLOAT_FORMAT_OK || result.length >= sizeof(result.text) ||
        result.text[result.length] != '\0')
        return false;
    for (size_t i = 0; i < result.length; ++i) {
        const unsigned char unit = (unsigned char)result.text[i];
        if (!((unit >= '0' && unit <= '9') || unit == '-' || unit == '.' || unit == 'E' ||
              unit == 'N' || unit == 'a' || unit == 'n' || unit == 'I' || unit == 'f' ||
              unit == 'i' || unit == 't' || unit == 'y'))
            return false;
    }
    return printf("{\"type\":\"value\",\"ordinal\":%zu,\"id\":\"%.*s\",\"input_bits\":\"%08" PRIx32
                  "\",\"status\":\"ok\",\"length\":%zu,\"text\":\"%s\"}\n",
                  ordinal, (int)id_length, line, bits, result.length, result.text) >= 0;
}

int main(int argc, char **argv) {
    FILE *input = NULL;
    char line[LINE_LIMIT];
    size_t length = 0, bytes = 0, rows = 0;
    int result = 1;
    const int initial_rounding = fegetround();
    if (argc != 2 || initial_rounding != FE_TONEAREST)
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
    static const char header[] = "minecraft-c-float-format-inputs-v1\n";
    for (size_t i = 0; i < sizeof(header) - 1u; ++i)
        if (fgetc(input) != (unsigned char)header[i])
            goto cleanup;
    bytes = sizeof(header) - 1u;
    if (printf("{\"type\":\"header\",\"schema\":1,\"row_limit\":%d,\"id_limit\":%d,"
               "\"fp_round\":%d,\"nearest_mode\":%d,\"flt_eval_method\":%d,\"double_bytes\":%zu}\n",
               ROW_LIMIT, ID_LIMIT, initial_rounding, FE_TONEAREST, FLT_EVAL_METHOD,
               sizeof(double)) < 0)
        goto cleanup;
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
        } else {
            if (length == sizeof(line))
                goto cleanup;
            line[length++] = (char)(unsigned char)next;
        }
    }
    if (length != 0 || ferror(input))
        goto cleanup;
    if (fclose(input) != 0) {
        input = NULL;
        goto cleanup;
    }
    input = NULL;
    const bool rounding_unchanged = fegetround() == initial_rounding;
    if (!rounding_unchanged)
        goto cleanup;
    if (printf("{\"type\":\"terminal\",\"rows\":%zu,\"status\":0,\"rounding_unchanged\":true}\n",
               rows) < 0)
        goto cleanup;
    if (fflush(stdout) == 0 && !ferror(stdout))
        result = 0;
cleanup:
    if (input != NULL)
        (void)fclose(input);
    return result;
}
