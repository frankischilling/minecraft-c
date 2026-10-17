#include "caves_internal.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_CASES 48u
#define PROBE_BLOCKS 32771u

typedef struct geometry_request {
    char text[257];
    char *fields[13];
    size_t count;
    int64_t seed;
    double x, y, z;
    float width, yaw, pitch;
    int32_t step, end;
    int pattern[5];
} geometry_request;
static geometry_request requests[PROBE_CASES];
static size_t request_count;
static mc_math_trig table, original_table;
static uint8_t blocks[PROBE_BLOCKS], before_blocks[PROBE_BLOCKS];

static int integer(const char *text, int64_t low, int64_t high, int64_t *out)
{
    const char *digits = *text == '-' ? text + 1 : text;
    if (*digits == '\0' || (*digits == '0' && digits[1] != '\0'))
        return 0;
    for (const char *p = digits; *p != '\0'; ++p)
        if (*p < '0' || *p > '9')
            return 0;
    char *end = NULL;
    errno = 0;
    intmax_t value = strtoimax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < low || value > high)
        return 0;
    *out = (int64_t)value;
    return 1;
}
static int raw_bits(const char *text, size_t digits, uint64_t *out)
{
    if (strlen(text) != digits)
        return 0;
    uint64_t value = 0;
    for (size_t i = 0; i < digits; ++i) {
        char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return 0;
        unsigned int digit = c >= 'a' ? (unsigned int)(c - 'a') + 10u : (unsigned int)(c - '0');
        value = (value << 4) | digit;
    }
    *out = value;
    return 1;
}
static int binary64(const char *text, double *out)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(*out), "binary64 required");
    if (!raw_bits(text, 16, &bits) ||
        (bits & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000))
        return 0;
    memcpy(out, &bits, sizeof(bits));
    return 1;
}
static int binary32(const char *text, float *out)
{
    uint64_t wide;
    _Static_assert(sizeof(uint32_t) == sizeof(*out), "binary32 required");
    if (!raw_bits(text, 8, &wide))
        return 0;
    uint32_t bits = (uint32_t)wide;
    if ((bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000))
        return 0;
    memcpy(out, &bits, sizeof(bits));
    return 1;
}
static int pattern(const char *text, int result[5])
{
    char copy[257];
    if (strlen(text) >= sizeof(copy))
        return 0;
    memcpy(copy, text, strlen(text) + 1);
    char *parts[5] = {copy};
    size_t count = 1;
    for (char *p = copy; *p != '\0'; ++p)
        if (*p == ':') {
            if (count == 5)
                return 0;
            *p = '\0';
            parts[count++] = p + 1;
        }
    if (count == 1 && strcmp(parts[0], "stone") == 0) {
        result[0] = 0;
        return 1;
    }
    int64_t values[4] = {0};
    for (size_t i = 1; i < count; ++i)
        if (!integer(parts[i], 0, 255, &values[i - 1]))
            return 0;
    if (count == 2 && strcmp(parts[0], "solid") == 0) {
        static const int allowed[] = {0, 1, 2, 3, 7, 12, 13, 24, 79, 128, 255};
        int found = 0;
        for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i)
            if (values[0] == allowed[i])
                found = 1;
        if (!found)
            return 0;
        result[0] = 1;
        result[1] = (int)values[0];
        return 1;
    }
    if (count == 5 && strcmp(parts[0], "water") == 0 && (values[0] == 8 || values[0] == 9) &&
        values[1] <= 15 && values[2] <= 127 && values[3] <= 15) {
        result[0] = 2;
        for (size_t i = 0; i < 4; ++i)
            result[i + 1] = (int)values[i];
        return 1;
    }
    if (count == 3 && strcmp(parts[0], "water-plane") == 0 && (values[0] == 8 || values[0] == 9) &&
        values[1] <= 127) {
        result[0] = 3;
        result[1] = (int)values[0];
        result[2] = (int)values[1];
        return 1;
    }
    if (count == 2 && strcmp(parts[0], "strata") == 0 && values[0] <= 127) {
        result[0] = 4;
        result[1] = (int)values[0];
        return 1;
    }
    return 0;
}
static int parse_request(geometry_request *request)
{
    request->fields[0] = request->text;
    request->count = 1;
    for (char *p = request->text; *p != '\0'; ++p)
        if (*p == '\t') {
            if (request->count == 13)
                return 0;
            *p = '\0';
            request->fields[request->count++] = p + 1;
        }
    int room = strcmp(request->fields[0], "room") == 0;
    if (!((room && request->count == 8) ||
          (strcmp(request->fields[0], "path") == 0 && request->count == 13)))
        return 0;
    int64_t x, z;
    if (!integer(request->fields[1], INT64_MIN, INT64_MAX, &request->seed) ||
        !integer(request->fields[2], 0, 0, &x) || !integer(request->fields[3], 0, 0, &z) ||
        !binary64(request->fields[4], &request->x) || !binary64(request->fields[5], &request->y) ||
        !binary64(request->fields[6], &request->z) || request->x < 0.0 || request->x > 15.0 ||
        request->y < 0.0 || request->y > 127.0 || request->z < 0.0 || request->z > 15.0 ||
        !pattern(request->fields[7], request->pattern))
        return 0;
    if (!room) {
        int64_t first, last;
        if (!binary32(request->fields[8], &request->width) ||
            !binary32(request->fields[9], &request->yaw) ||
            !binary32(request->fields[10], &request->pitch) || request->width < 0.0f ||
            request->width > 3.0f || request->yaw < 0.0f || request->yaw > 0x1.921fb6p+2f ||
            request->pitch < -0.125f || request->pitch > 0.125f ||
            !integer(request->fields[11], 0, 111, &first) ||
            !integer(request->fields[12], 85, 112, &last) || first >= last)
            return 0;
        request->step = (int32_t)first;
        request->end = (int32_t)last;
    }
    return 1;
}
static int read_requests(void)
{
    char line[260];
    size_t total = 0;
    while (fgets(line, (int)sizeof(line), stdin) != NULL) {
        size_t length = strlen(line);
        total += length;
        if (length == 0 || line[length - 1] != '\n' || request_count == PROBE_CASES ||
            total > 16384)
            return 0;
        line[--length] = '\0';
        if (length != 0 && line[length - 1] == '\r')
            line[--length] = '\0';
        if (length > 256)
            return 0;
        geometry_request *request = &requests[request_count];
        memcpy(request->text, line, length + 1);
        if (!parse_request(request))
            return 0;
        ++request_count;
    }
    return !ferror(stdin) && request_count != 0;
}
static void prepare_blocks(const geometry_request *request)
{
    memset(blocks, 1, 32768);
    const int *p = request->pattern;
    if (p[0] == 1)
        memset(blocks, p[1], 32768);
    else if (p[0] == 2)
        blocks[((size_t)p[2] * 16 + (size_t)p[4]) * 128 + (size_t)p[3]] = (uint8_t)p[1];
    else if (p[0] == 3) {
        for (size_t x = 0; x < 16; ++x)
            for (size_t z = 0; z < 16; ++z)
                blocks[(x * 16 + z) * 128 + (size_t)p[2]] = (uint8_t)p[1];
    } else if (p[0] == 4) {
        for (size_t x = 0; x < 16; ++x)
            for (size_t z = 0; z < 16; ++z)
                for (size_t y = 0; y < 128; ++y)
                    blocks[(x * 16 + z) * 128 + y] = (uint8_t)(y > (size_t)p[1]    ? 0u
                                                               : y == (size_t)p[1] ? 2u
                                                                                   : 3u);
    }
    blocks[32768] = 0x8a;
    blocks[32769] = 0x8b;
    blocks[32770] = 0x8c;
}
static void continuation(mc_java_random random, uint64_t out[4])
{
    for (size_t i = 0; i < 4; ++i)
        out[i] = (uint64_t)mc_java_random_next_i64(&random);
}
static int infer_consumption(const geometry_request *request, const uint64_t observed[4],
                             int *selected)
{
    int room = strcmp(request->fields[0], "room") == 0, matches = 0;
    for (int count = 1; count <= (room ? 1 : 3); count += 2) {
        mc_java_random trial;
        mc_java_random_seed(&trial, request->seed);
        if (room)
            (void)mc_java_random_next_float(&trial);
        for (int i = 0; i < count; ++i)
            (void)mc_java_random_next_i64(&trial);
        uint64_t candidate[4];
        continuation(trial, candidate);
        if (memcmp(candidate, observed, sizeof(candidate)) == 0) {
            ++matches;
            *selected = count;
        }
    }
    return matches == 1;
}
static void print_continuation(const uint64_t values[4])
{
    putchar('[');
    for (size_t i = 0; i < 4; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%016" PRIx64 "\"", values[i]);
    }
    putchar(']');
}
static void print_blocks(const uint8_t *values)
{
    fputs("{\"length\":32771,\"values\":\"", stdout);
    for (size_t i = 0; i < PROBE_BLOCKS; ++i)
        printf("%02x", (unsigned int)values[i]);
    fputs("\"}", stdout);
}
static int observe(const geometry_request *request, size_t index)
{
    mc_java_random random;
    mc_java_random_seed(&random, request->seed);
    uint64_t before[4], after[4];
    continuation(random, before);
    prepare_blocks(request);
    memcpy(before_blocks, blocks, sizeof(blocks));
    mc_terrain_cave_context context = {&table, &random, blocks, 0, 0};
    int room = strcmp(request->fields[0], "room") == 0;
    if (room)
        mc_terrain_cave_room(&context, request->x, request->y, request->z);
    else
        mc_terrain_cave_path(&context, request->x, request->y, request->z, request->width,
                             request->yaw, request->pitch, request->step, request->end, 1.0);
    continuation(random, after);
    int selected = 0;
    if (context.table != &table || context.random != &random || context.blocks != blocks ||
        context.chunk_x != 0 || context.chunk_z != 0 ||
        memcmp(&table, &original_table, sizeof(table)) != 0 ||
        memcmp(before_blocks + 32768, blocks + 32768, 3) != 0 ||
        !infer_consumption(request, after, &selected))
        return 0;
    size_t changes = 0;
    for (size_t i = 0; i < 32768; ++i)
        if (blocks[i] != before_blocks[i])
            ++changes;
    printf("OK\t{\"schema\":2,\"case\":%zu,\"request\":[", index);
    for (size_t i = 0; i < request->count; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%s\"", request->fields[i]);
    }
    fputs("],\"input_blocks\":", stdout);
    print_blocks(before_blocks);
    fputs(",\"before_continuation\":", stdout);
    print_continuation(before);
    fputs(",\"output_blocks\":", stdout);
    print_blocks(blocks);
    fputs(",\"after_continuation\":", stdout);
    print_continuation(after);
    printf(",\"shared_random_consumption\":{\"basis\":\"continuation_match\",\"next_float_calls\":%"
           "d,\"next_long_calls\":%d,\"fork_inferred\":%s,\"candidates_matched\":1}",
           room ? 1 : 0, selected, !room && selected == 3 ? "true" : "false");
    printf(",\"changed_prefix_bytes\":%zu,\"table_unchanged\":%s,\"suffix_unchanged\":true,"
           "\"retained_random\":true,\"range\":%u}\n",
           changes, memcmp(&table, &original_table, sizeof(table)) == 0 ? "true" : "false",
           (unsigned int)MC_TERRAIN_NORMAL_CAVE_RANGE);
    fprintf(stderr,
            "CONTROL_C_GEOMETRY table_exact range_exact random_identity suffix_exact "
            "shared_rng_continuation_match\nGEOMETRY_DONE case=%zu\n",
            index);
    return !ferror(stdout) && !ferror(stderr);
}
int main(void)
{
    if (!read_requests() || !mc_math_trig_init(&table) || MC_TERRAIN_NORMAL_CAVE_RANGE != 8u) {
        fputs("invalid bounded protected cave request\n", stderr);
        return 1;
    }
    memcpy(&original_table, &table, sizeof(table));
    fputs("TABLE\t", stdout);
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i) {
        uint32_t bits;
        _Static_assert(sizeof(bits) == sizeof(table.values[i]), "binary32 required");
        memcpy(&bits, &table.values[i], sizeof(bits));
        printf("%08" PRIx32, bits);
    }
    putchar('\n');
    for (size_t i = 0; i < request_count; ++i)
        if (!observe(&requests[i], i + 1)) {
            fprintf(stderr, "protected cave probe failed case=%zu\n", i + 1);
            return 1;
        }
    return !ferror(stdout) && !ferror(stderr) ? 0 : 1;
}
