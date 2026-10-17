#include "mc/world/noise/noise.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_LAYERS 128u
#define PROBE_VALUES 65536u

static char request[2048];
static mc_noise_perlin layers[PROBE_LAYERS];
static double values[PROBE_VALUES + 3u];

static uint64_t bits_of(double value)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int parse_seed(const char *text, int64_t *out)
{
    char *end = NULL;
    errno = 0;
    intmax_t value = strtoimax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < INT64_MIN || value > INT64_MAX)
        return 0;
    *out = (int64_t)value;
    return 1;
}

static int parse_size(const char *text, size_t maximum, size_t *out)
{
    if (*text == '\0')
        return 0;
    size_t value = 0;
    for (const char *digit = text; *digit != '\0'; ++digit) {
        if (*digit < '0' || *digit > '9')
            return 0;
        size_t nibble = (size_t)(*digit - '0');
        if (nibble > maximum || value > (maximum - nibble) / 10u)
            return 0;
        value = value * 10u + nibble;
    }
    *out = value;
    return 1;
}

static int parse_double(const char *text, double *out)
{
    if (strlen(text) != 16)
        return 0;
    uint64_t value = 0;
    for (size_t i = 0; i < 16; ++i) {
        unsigned int nibble;
        char digit = text[i];
        if (digit >= '0' && digit <= '9')
            nibble = (unsigned int)(digit - '0');
        else if (digit >= 'a' && digit <= 'f')
            nibble = (unsigned int)(digit - 'a') + 10u;
        else if (digit >= 'A' && digit <= 'F')
            nibble = (unsigned int)(digit - 'A') + 10u;
        else
            return 0;
        value = (value << 4u) | nibble;
    }
    memcpy(out, &value, sizeof(value));
    return 1;
}

static void print_values(const double *output, size_t length)
{
    if (length == 0)
        putchar('-');
    for (size_t i = 0; i < length; ++i) {
        if (i != 0)
            putchar(',');
        printf("%016" PRIx64, bits_of(output[i]));
    }
}

static int handle_request(char *line)
{
    char *fields[18];
    size_t field_count = 0;
    fields[field_count++] = line;
    for (char *cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == '\t') {
            if (field_count == sizeof(fields) / sizeof(fields[0]))
                return 0;
            *cursor = '\0';
            fields[field_count++] = cursor + 1;
        }
    }
    if (field_count < 4)
        return 0;
    int state = strcmp(fields[0], "state") == 0;
    int point = strcmp(fields[0], "point") == 0;
    int sample2 = strcmp(fields[0], "sample2") == 0;
    int add = strcmp(fields[0], "add") == 0;
    int fill = strcmp(fields[0], "fill") == 0;
    size_t expected_fields = state ? 4u : point ? 7u : sample2 ? 6u : add ? 14u : fill ? 13u : 0u;
    if (field_count != expected_fields)
        return 0;
    int64_t seed;
    size_t skip;
    size_t count;
    if (!parse_seed(fields[1], &seed) || !parse_size(fields[2], PROBE_LAYERS, &skip) ||
        !parse_size(fields[3], PROBE_LAYERS, &count) || ((point || add) && count != 1))
        return 0;
    double x = 0;
    double y = 0;
    double z = 0;
    double divisor = 1;
    mc_noise_grid grid = {0};
    size_t product = 0;
    if (point || sample2) {
        if (!parse_double(fields[4], &x) || !parse_double(fields[5], &y) ||
            (point && !parse_double(fields[6], &z)))
            return 0;
    }
    if (add || fill) {
        if (!parse_double(fields[4], &grid.origin_x) || !parse_double(fields[5], &grid.origin_y) ||
            !parse_double(fields[6], &grid.origin_z) ||
            !parse_size(fields[7], INT32_MAX, &grid.size_x) ||
            !parse_size(fields[8], INT32_MAX, &grid.size_y) ||
            !parse_size(fields[9], INT32_MAX, &grid.size_z) ||
            !parse_double(fields[10], &grid.scale_x) || !parse_double(fields[11], &grid.scale_y) ||
            !parse_double(fields[12], &grid.scale_z) ||
            (add && !parse_double(fields[13], &divisor)))
            return 0;
        if (grid.size_x != 0 && grid.size_y != 0 && grid.size_z != 0) {
            if (grid.size_x > PROBE_VALUES / grid.size_y)
                return 0;
            product = grid.size_x * grid.size_y;
            if (product > PROBE_VALUES / grid.size_z)
                return 0;
            product *= grid.size_z;
        }
    }
    mc_java_random random;
    mc_java_random_seed(&random, seed);
    mc_noise_perlin discarded;
    for (size_t i = 0; i < skip; ++i) {
        if (!mc_noise_perlin_init(&discarded, &random))
            return 0;
    }
    mc_noise_octaves octaves;
    if (point || add) {
        if (!mc_noise_perlin_init(&layers[0], &random))
            return 0;
    } else if (!mc_noise_octaves_init(&octaves, layers, count, &random)) {
        return 0;
    }
    size_t value_count = 0;
    if (point) {
        values[0] = mc_noise_perlin_sample(&layers[0], x, y, z);
        value_count = 1;
    } else if (sample2) {
        values[0] = mc_noise_octaves_sample2(&octaves, x, y);
        value_count = 1;
    } else if (add || fill) {
        value_count = product + 3u;
        for (size_t i = 0; i < value_count; ++i)
            values[i] = (double)((int)(i % 11u) - 5) * 0.125;
        if (add) {
            if (!mc_noise_perlin_add(&layers[0], &grid, divisor, values, value_count))
                return 0;
        } else if (!mc_noise_octaves_fill(&octaves, &grid, values, value_count)) {
            return 0;
        }
    }
    fputs("OK\t", stdout);
    if (state) {
        if (count == 0)
            putchar('-');
        for (size_t i = 0; i < count; ++i) {
            if (i != 0)
                putchar(',');
            printf("%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64, bits_of(layers[i].offset_x),
                   bits_of(layers[i].offset_y), bits_of(layers[i].offset_z));
        }
    } else {
        print_values(values, value_count);
    }
    putchar('\t');
    for (size_t i = 0; i < 4; ++i) {
        if (i != 0)
            putchar(',');
        printf("%016" PRIx64, (uint64_t)mc_java_random_next_i64(&random));
    }
    putchar('\t');
    if (state && count != 0) {
        for (size_t layer = 0; layer < count; ++layer) {
            for (size_t i = 0; i < 512; ++i) {
                if (layer != 0 || i != 0)
                    putchar(',');
                printf("%u", (unsigned int)layers[layer].permutation[i]);
            }
        }
    } else {
        putchar('-');
    }
    putchar('\n');
    return 1;
}

int main(void)
{
    while (fgets(request, (int)sizeof(request), stdin) != NULL) {
        size_t length = strlen(request);
        if (length != 0 && request[length - 1u] == '\n')
            request[--length] = '\0';
        else if (!feof(stdin)) {
            int byte;
            do {
                byte = getchar();
            } while (byte != '\n' && byte != EOF);
            puts("ERR");
            continue;
        }
        if (length != 0 && request[length - 1u] == '\r')
            request[length - 1u] = '\0';
        if (!handle_request(request))
            puts("ERR");
    }
    if (fflush(stdout) == EOF || ferror(stdin) || ferror(stdout)) {
        fputs("probe transport failure\n", stderr);
        return 2;
    }
    return 0;
}
