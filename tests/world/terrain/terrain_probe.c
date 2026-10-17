#include "mc/world/biome/biome.h"
#include "mc/world/terrain/terrain.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_REQUESTS 1024u
#define PROBE_VOLUME_VALUES 512u
#define PROBE_COLUMN_VALUES 64u
#define PROBE_BLOCK_VALUES 32832u

static char request[260];
static mc_terrain_normal_state state;
static mc_biome_memory memory;
static mc_biome_manager manager;
static mc_biome_buffer climate_biomes;
static double volume_storage[4][PROBE_VOLUME_VALUES];
static double column_storage[2][PROBE_COLUMN_VALUES];
static uint8_t block_storage[PROBE_BLOCK_VALUES];
static mc_terrain_double_span spans[6];
static uint8_t *blocks;
static size_t blocks_length;
static int initialized;
static int climate_present;
static int density_present;
static int32_t density_x;
static int32_t density_z;

static uint64_t bits_of(double value)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int parse_signed(const char *text, int64_t minimum, int64_t maximum, int64_t *out)
{
    char *end = NULL;
    errno = 0;
    intmax_t value = strtoimax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < minimum || value > maximum)
        return 0;
    *out = (int64_t)value;
    return 1;
}

static void print_values(const double *values, size_t length)
{
    putchar('[');
    for (size_t i = 0; i < length; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%016" PRIx64 "\"", bits_of(values[i]));
    }
    putchar(']');
}

static void print_span(const mc_terrain_double_span *span)
{
    if (span->data == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"length\":%zu,\"values\":", span->length);
    print_values(span->data, span->length);
    putchar('}');
}

static void print_state(void)
{
    static const char names[] = "klmnoabc";
    static const size_t counts[] = {16, 16, 8, 4, 4, 10, 16, 8};
    const mc_noise_perlin *channels[] = {state.low_layers,
                                         state.high_layers,
                                         state.mix_layers,
                                         state.surface_primary_layers,
                                         state.surface_secondary_layers,
                                         state.column_scale_layers,
                                         state.column_bias_layers,
                                         state.later_layers};
    putchar('{');
    for (size_t channel = 0; channel < 8; ++channel) {
        if (channel != 0)
            putchar(',');
        printf("\"%c\":[", names[channel]);
        for (size_t layer = 0; layer < counts[channel]; ++layer) {
            if (layer != 0)
                putchar(',');
            const mc_noise_perlin *noise = &channels[channel][layer];
            double offsets[] = {noise->offset_x, noise->offset_y, noise->offset_z};
            fputs("{\"offsets\":", stdout);
            print_values(offsets, 3);
            fputs(",\"permutation\":[", stdout);
            for (size_t i = 0; i < 512; ++i) {
                if (i != 0)
                    putchar(',');
                printf("%u", (unsigned int)noise->permutation[i]);
            }
            fputs("]}", stdout);
        }
        putchar(']');
    }
    putchar('}');
}

static void print_response(int full_state, int fixture, const uintptr_t before[7])
{
    static const char *const names[] = {"d", "e", "f", "g", "h", "density"};
    fputs("OK\t{\"state\":", stdout);
    if (full_state)
        print_state();
    else
        fputs("null", stdout);
    fputs(",\"continuation\":[", stdout);
    mc_java_random random = state.random_after_init;
    for (size_t i = 0; i < 4; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%016" PRIx64 "\"", (uint64_t)mc_java_random_next_i64(&random));
    }
    fputs("],\"climate\":", stdout);
    if (climate_present) {
        fputs("{\"temperature\":", stdout);
        print_values(manager.a.data, 256);
        fputs(",\"rainfall\":", stdout);
        print_values(manager.b.data, 256);
        fputs(",\"auxiliary\":", stdout);
        print_values(manager.c.data, 256);
        fputs(",\"biomes\":\"", stdout);
        for (size_t i = 0; i < 256; ++i)
            putchar(mc_biome_field(climate_biomes.data[i]));
        fputs("\"}", stdout);
    } else {
        fputs("null", stdout);
    }
    for (size_t i = 0; i < 6; ++i) {
        printf(",\"%s\":", names[i]);
        print_span(&spans[i]);
    }
    fputs(",\"blocks\":", stdout);
    if (blocks != NULL) {
        printf("{\"length\":%zu,\"values\":\"", blocks_length);
        for (size_t i = 0; i < blocks_length; ++i)
            printf("%02x", (unsigned int)blocks[i]);
        fputs("\"}", stdout);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"retained\":\"", stdout);
    for (size_t i = 0; i < 7; ++i) {
        uintptr_t after = (uintptr_t)(i < 6 ? (void *)spans[i].data : (void *)blocks);
        putchar(!fixture && before[i] != 0 && before[i] == after ? '1' : '0');
    }
    puts("\"}");
}

static int handle_request(char *line)
{
    char *fields[8];
    size_t count = 1;
    fields[0] = line;
    for (char *cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == '\t') {
            if (count == sizeof(fields) / sizeof(fields[0]))
                return 0;
            *cursor = '\0';
            fields[count++] = cursor + 1;
        }
    }
    uintptr_t before[7];
    for (size_t i = 0; i < 7; ++i)
        before[i] = (uintptr_t)(i < 6 ? (void *)spans[i].data : (void *)blocks);
    int full_state = 0;
    int fixture = 0;
    if (strcmp(fields[0], "init") == 0) {
        int64_t seed;
        if (count != 2 || !parse_signed(fields[1], INT64_MIN, INT64_MAX, &seed))
            return 0;
        mc_biome_buffer_free(&climate_biomes);
        mc_biome_manager_free(&manager);
        if (!mc_terrain_normal_init(&state, seed) ||
            mc_biome_manager_init(&manager, seed, &memory) != MC_BIOME_OK)
            return 0;
        memset(spans, 0, sizeof(spans));
        blocks = NULL;
        blocks_length = 0;
        initialized = 1;
        climate_present = 0;
        density_present = 0;
        full_state = 1;
        fixture = 1;
    } else if (!initialized) {
        return 0;
    } else if (strcmp(fields[0], "state") == 0) {
        if (count != 1)
            return 0;
        full_state = 1;
    } else if (strcmp(fields[0], "buffers") == 0) {
        int64_t lengths[7];
        if (count != 8)
            return 0;
        for (size_t i = 0; i < 7; ++i) {
            int64_t minimum = i == 6 ? 32768 : i == 3 || i == 4 ? 25 : 425;
            int64_t maximum = i == 6             ? PROBE_BLOCK_VALUES
                              : i == 3 || i == 4 ? PROBE_COLUMN_VALUES
                                                 : PROBE_VOLUME_VALUES;
            if (!parse_signed(fields[i + 1u], minimum, maximum, &lengths[i]))
                return 0;
        }
        for (size_t i = 0; i < 6; ++i) {
            spans[i].data = i < 3   ? volume_storage[i]
                            : i < 5 ? column_storage[i - 3u]
                                    : volume_storage[3];
            spans[i].length = (size_t)lengths[i];
            for (size_t j = 0; j < spans[i].length; ++j)
                spans[i].data[j] = (double)((int)(j % 11u) - 5) * 0.125;
        }
        blocks = block_storage;
        blocks_length = (size_t)lengths[6];
        for (size_t i = 0; i < blocks_length; ++i)
            blocks[i] = (uint8_t)(i % 251u);
        density_present = 0;
        fixture = 1;
    } else {
        int density = strcmp(fields[0], "density") == 0;
        int base = strcmp(fields[0], "base") == 0;
        int generate = strcmp(fields[0], "generate") == 0;
        int64_t x;
        int64_t z;
        if (count != 3 || (!density && !base && !generate) || blocks == NULL ||
            !parse_signed(fields[1], INT32_MIN, INT32_MAX, &x) ||
            !parse_signed(fields[2], INT32_MIN, INT32_MAX, &z))
            return 0;
        if (base && (!density_present || density_x != x || density_z != z))
            return 0;
        if (!base) {
            if (mc_biome_sample_square(&manager, mc_java_i32_mul((int32_t)x, 16),
                                       mc_java_i32_mul((int32_t)z, 16), 16,
                                       &climate_biomes) != MC_BIOME_OK)
                return 0;
            climate_present = 1;
        }
        mc_terrain_normal_workspace workspace = {spans[0], spans[1], spans[2], spans[3], spans[4]};
        mc_terrain_normal_climate climate = {manager.a.data, manager.a.length, manager.b.data,
                                             manager.b.length};
        int success = base ? mc_terrain_normal_base(spans[5].data, spans[5].length, manager.a.data,
                                                    manager.a.length, blocks, blocks_length)
                      : density
                          ? mc_terrain_normal_density(&state, &workspace, (int32_t)x, (int32_t)z,
                                                      &climate, spans[5])
                          : mc_terrain_normal_generate(&state, &workspace, (int32_t)x, (int32_t)z,
                                                       &climate, spans[5], blocks, blocks_length);
        if (!success)
            return 0;
        density_present = 1;
        density_x = (int32_t)x;
        density_z = (int32_t)z;
    }
    print_response(full_state, fixture, before);
    return 1;
}

int main(void)
{
    if (mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) != MC_BIOME_OK)
        return 2;
    size_t requests = 0;
    int failed = 0;
    while (fgets(request, (int)sizeof(request), stdin) != NULL) {
        if (++requests > PROBE_REQUESTS) {
            failed = 1;
            break;
        }
        size_t length = strlen(request);
        if (length != 0 && request[length - 1u] == '\n')
            request[--length] = '\0';
        else if (!feof(stdin)) {
            failed = 1;
            break;
        }
        if (length != 0 && request[length - 1u] == '\r')
            request[--length] = '\0';
        if (length > 256 || !handle_request(request)) {
            failed = 1;
            break;
        }
    }
    mc_biome_buffer_free(&climate_biomes);
    mc_biome_manager_free(&manager);
    if (fflush(stdout) == EOF || ferror(stdin) || ferror(stdout) ||
        memory.live_allocation_bytes != 0) {
        fputs("probe transport or lifetime failure\n", stderr);
        return 2;
    }
    return failed ? 1 : 0;
}
