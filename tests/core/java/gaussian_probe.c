#include "mc/core/java/java.h"
#include "mc/core/math/strict.h"

#include <emmintrin.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#define PROBE_RECORD_LIMIT 2u
#define PROBE_OPERATION_LIMIT 100000u
#define PROBE_LINE_LIMIT 160u

typedef enum probe_line_status {
    PROBE_LINE_OK,
    PROBE_LINE_END,
    PROBE_LINE_INVALID,
    PROBE_LINE_IO_ERROR
} probe_line_status;

typedef struct gaussian_probe_state {
    mc_java_random owners[2];
    bool initialized[2];
} gaussian_probe_state;

static probe_line_status read_line(char line[PROBE_LINE_LIMIT + 1u], bool *skip_lf)
{
    size_t length = 0;
    for (;;) {
        const int character = fgetc(stdin);
        if (character == EOF) {
            if (ferror(stdin)) {
                return PROBE_LINE_IO_ERROR;
            }
            if (length == 0) {
                return PROBE_LINE_END;
            }
            line[length] = '\0';
            return PROBE_LINE_OK;
        }
        if (*skip_lf) {
            *skip_lf = false;
            if (character == '\n') {
                continue;
            }
        }
        if (character == '\r') {
            *skip_lf = true;
            line[length] = '\0';
            return PROBE_LINE_OK;
        }
        if (character == '\n') {
            line[length] = '\0';
            return PROBE_LINE_OK;
        }
        if (character < 0x20 || character > 0x7e || length == PROBE_LINE_LIMIT) {
            return PROBE_LINE_INVALID;
        }
        line[length++] = (char)character;
    }
}

/* Keep empty fields, as Java split(" ", -1) does. No command has over four. */
static bool split_words(char *line, char *words[4], size_t *out_count)
{
    size_t count = 1;
    words[0] = line;
    for (char *cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == ' ') {
            if (count == 4u) {
                return false;
            }
            *cursor = '\0';
            words[count++] = cursor + 1;
        }
    }
    *out_count = count;
    return true;
}

static bool parse_i64(const char *text, int64_t *out)
{
    const char *cursor = text;
    if (*cursor == '+' || *cursor == '-') {
        ++cursor;
    }
    if (*cursor == '\0') {
        return false;
    }
    for (; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
    }
    errno = 0;
    char *end = NULL;
    const intmax_t value = strtoimax(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value < INT64_MIN || value > INT64_MAX) {
        return false;
    }
    *out = (int64_t)value;
    return true;
}

static bool parse_id(const char *text, size_t *out)
{
    int64_t value = 0;
    if (!parse_i64(text, &value) || value < 0 || value >= (int64_t)PROBE_RECORD_LIMIT) {
        return false;
    }
    *out = (size_t)value;
    return true;
}

static uint64_t double_bits(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool positive_finite_hex(const char *text, double *out)
{
    if (strlen(text) != 16) {
        return false;
    }
    uint64_t bits = 0;
    for (size_t index = 0; index < 16; ++index) {
        const unsigned char character = (unsigned char)text[index];
        unsigned digit;
        if (character >= '0' && character <= '9') {
            digit = (unsigned)(character - '0');
        } else if (character >= 'a' && character <= 'f') {
            digit = (unsigned)(character - 'a') + 10u;
        } else if (character >= 'A' && character <= 'F') {
            digit = (unsigned)(character - 'A') + 10u;
        } else {
            return false;
        }
        bits = (bits << 4) | digit;
    }
    if (bits == 0 || bits >= UINT64_C(0x7ff0000000000000)) {
        return false;
    }
    memcpy(out, &bits, sizeof(bits));
    return true;
}

static bool execute_command(gaussian_probe_state *state, char *words[4], size_t count,
                            char result[32])
{
    memcpy(result, "-", 2);
    if (strcmp(words[0], "log") == 0 || strcmp(words[0], "sqrt") == 0) {
        double input;
        if (count != 2 || !positive_finite_hex(words[1], &input)) {
            return false;
        }
        const double value =
            strcmp(words[0], "log") == 0 ? mc_math_strict_log(input) : mc_math_strict_sqrt(input);
        return snprintf(result, 32, "%016" PRIx64, double_bits(value)) == 16;
    }
    size_t id;
    if (count < 2 || !parse_id(words[1], &id)) {
        return false;
    }
    if (strcmp(words[0], "new") == 0) {
        int64_t seed;
        if (count != 3 || !parse_i64(words[2], &seed)) {
            return false;
        }
        mc_java_random_seed(&state->owners[id], seed);
        state->initialized[id] = true;
        return true;
    }
    if (strcmp(words[0], "snapshot") == 0) {
        return count == 2;
    }
    if (!state->initialized[id]) {
        return false;
    }
    mc_java_random *owner = &state->owners[id];
    if (strcmp(words[0], "seed") == 0) {
        int64_t seed;
        if (count != 3 || !parse_i64(words[2], &seed)) {
            return false;
        }
        mc_java_random_reseed(owner, seed);
        return true;
    }
    if (strcmp(words[0], "bounded") == 0) {
        int64_t wide_bound;
        if (count != 3 || !parse_i64(words[2], &wide_bound) || wide_bound < INT32_MIN ||
            wide_bound > INT32_MAX) {
            return false;
        }
        int32_t value;
        if (!mc_java_random_next_bounded(owner, (int32_t)wide_bound, &value)) {
            memcpy(result, "bound-error", 12);
            return true;
        }
        return snprintf(result, 32, "%08" PRIx32, (uint32_t)value) == 8;
    }
    if (count != 2) {
        return false;
    }
    if (strcmp(words[0], "int") == 0) {
        return snprintf(result, 32, "%08" PRIx32, (uint32_t)mc_java_random_next_i32(owner)) == 8;
    }
    if (strcmp(words[0], "long") == 0) {
        return snprintf(result, 32, "%016" PRIx64, (uint64_t)mc_java_random_next_i64(owner)) == 16;
    }
    if (strcmp(words[0], "float") == 0) {
        const float value = mc_java_random_next_float(owner);
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));
        return snprintf(result, 32, "%08" PRIx32, bits) == 8;
    }
    double value;
    if (strcmp(words[0], "double") == 0) {
        value = mc_java_random_next_double(owner);
    } else if (strcmp(words[0], "gaussian") == 0) {
        value = mc_java_random_next_gaussian(owner);
    } else {
        return false;
    }
    return snprintf(result, 32, "%016" PRIx64, double_bits(value)) == 16;
}

static bool print_snapshot(const gaussian_probe_state *state, const char *command,
                           const char *result)
{
    if (printf("%s %s", command, result) < 0) {
        return false;
    }
    for (size_t id = 0; id < 2; ++id) {
        if (!state->initialized[id]) {
            if (fputs(" absent", stdout) == EOF) {
                return false;
            }
        } else {
            const mc_java_random *owner = &state->owners[id];
            if (printf(" %016" PRIx64 ":%016" PRIx64 ":%u", owner->state,
                       double_bits(owner->next_next_gaussian),
                       owner->have_next_next_gaussian ? 1u : 0u) < 0) {
                return false;
            }
        }
    }
    return putchar('\n') != EOF && fflush(stdout) == 0;
}

int main(int argc, char **argv)
{
    (void)argv;
    gaussian_probe_state state = {0};
    if (argc != 1) {
        fputs("Gaussian probe: no arguments accepted\n", stderr);
        return 1;
    }
    /* Admit nearest-even SSE2 arithmetic, gradual underflow and masked traps.
     * Sticky status flags are outside the result/state observation protocol. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("Gaussian probe: unsupported floating control state\n", stderr);
        return 1;
    }
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        fputs("Gaussian probe: binary input mode failed\n", stderr);
        return 1;
    }
#endif
    size_t operations = 0;
    bool skip_lf = false;
    char line[PROBE_LINE_LIMIT + 1u];
    for (;;) {
        const probe_line_status status = read_line(line, &skip_lf);
        if (status == PROBE_LINE_END) {
            break;
        }
        if (status != PROBE_LINE_OK || operations == PROBE_OPERATION_LIMIT) {
            fputs("Gaussian probe: invalid input or input limit\n", stderr);
            return 1;
        }
        ++operations;
        char *words[4] = {0};
        size_t count = 0;
        char result[32];
        if (!split_words(line, words, &count) || !execute_command(&state, words, count, result)) {
            fprintf(stderr, "Gaussian probe: invalid command at operation %zu\n", operations);
            return 1;
        }
        if (!print_snapshot(&state, words[0], result)) {
            fputs("Gaussian probe: output failure\n", stderr);
            return 1;
        }
    }
    if (fflush(stdout) != 0) {
        fputs("Gaussian probe: output flush failed\n", stderr);
        return 1;
    }
    return 0;
}
