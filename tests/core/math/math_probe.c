#include "mc/core/math/math.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PROBE_REQUESTS 768u
#define PROBE_LINE 32u

static mc_math_trig table;
static mc_math_trig original;
static char request[PROBE_LINE + 4u];

static uint32_t float_bits(float value) {
    uint32_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary32 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static double double_from_bits(uint64_t bits) {
    double value;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int parse_bits(const char *text, size_t digits, uint64_t *out) {
    if (strlen(text) != digits) {
        return 0;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < digits; ++i) {
        unsigned int digit;
        if (text[i] >= '0' && text[i] <= '9') {
            digit = (unsigned int)(text[i] - '0');
        } else if (text[i] >= 'a' && text[i] <= 'f') {
            digit = (unsigned int)(text[i] - 'a') + 10u;
        } else {
            return 0;
        }
        value = (value << 4u) | digit;
    }
    *out = value;
    return 1;
}

static int respond(char *line) {
    char *argument = strchr(line, '\t');
    if (argument == NULL) {
        return 0;
    }
    *argument++ = '\0';
    uint64_t bits;
    if (strcmp(line, "sin") == 0 || strcmp(line, "cos") == 0) {
        if (!parse_bits(argument, 8, &bits)) {
            return 0;
        }
        float angle = float_from_bits((uint32_t)bits);
        float result =
            strcmp(line, "sin") == 0 ? mc_math_sin(&table, angle) : mc_math_cos(&table, angle);
        printf("CALL\t%s\t(F)F\t%s\t%08" PRIx32 "\t-\n", line, argument, float_bits(result));
        return 1;
    }
    if (strcmp(line, "floor") == 0) {
        if (!parse_bits(argument, 16, &bits)) {
            return 0;
        }
        int32_t result = mc_math_floor_double(double_from_bits(bits));
        printf("CALL\tfloor\t(D)I\t%s\t%08" PRIx32 "\t%" PRId32 "\n", argument, (uint32_t)result,
               result);
        return 1;
    }
    return 0;
}

/* Return one for a complete line, zero for clean EOF, and -1 for invalid input.
 * Count content bytes independently of C string termination and permit CRLF. */
static int read_request(void) {
    size_t length = 0;
    int byte;
    while ((byte = fgetc(stdin)) != EOF) {
        if (byte == '\n') {
            if (length > 0 && request[length - 1] == '\r') {
                --length;
            }
            if (length > PROBE_LINE) {
                return -1;
            }
            request[length] = '\0';
            return 1;
        }
        if (byte == 0 || length >= PROBE_LINE + 1u) {
            return -1;
        }
        request[length++] = (char)byte;
    }
    if (ferror(stdin) || length > PROBE_LINE) {
        return -1;
    }
    request[length] = '\0';
    return length == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1 || !mc_math_trig_init(&table)) {
        return 1;
    }
    memcpy(&original, &table, sizeof(original));
    printf("TABLE_DESCRIPTOR\ta\t[F\t65536\n"
           "METHOD_DESCRIPTOR\tsin\ta\t(F)F\n"
           "METHOD_DESCRIPTOR\tcos\tb\t(F)F\n"
           "METHOD_DESCRIPTOR\tfloor\tb\t(D)I\n");
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i) {
        printf("TABLE\t%zu\t%08" PRIx32 "\n", i, float_bits(table.values[i]));
    }

    size_t calls = 0;
    int status;
    while ((status = read_request()) > 0) {
        if (calls >= PROBE_REQUESTS || !respond(request)) {
            return 1;
        }
        ++calls;
    }
    if (status < 0) {
        return 1;
    }
    int unchanged = memcmp(&table, &original, sizeof(table)) == 0;
    printf("TABLE_UNCHANGED\t%d\nCALL_COUNT\t%zu\n", unchanged, calls);
    return unchanged && fflush(stdout) == 0 ? 0 : 1;
}
