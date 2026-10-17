#include "mc/world/biome/biome.h"
#include "mc/world/terrain/surface.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_REQUESTS 1024u
#define PROBE_INITIALIZATIONS 32u
#define PROBE_SCRATCH_VALUES 512u
#define PROBE_BLOCK_VALUES 32832u

static char request[260];
static mc_terrain_normal_state state;
static mc_terrain_normal_state original_state;
static mc_java_random constructor_random;
static mc_java_random live_random;
static mc_biome_memory memory;
static mc_biome_manager manager;
static mc_biome_buffer biomes;
static double scratch[3][PROBE_SCRATCH_VALUES];
static mc_terrain_normal_surface_workspace surface;
static mc_terrain_surface_material materials[256];
static double density[425];
static double density_volumes[3][425];
static double density_columns[2][25];
static uint8_t block_storage[PROBE_BLOCK_VALUES];
static uint8_t *blocks;
static size_t blocks_length;
static int initialized;
static size_t initializations;
static int base_valid;
static int noise_valid;
static int climate_observed;
static int density_observed;
static int seed_observed;
static int32_t base_x;
static int32_t base_z;
static int32_t noise_x;
static int32_t noise_z;
static int64_t chunk_seed;

static uint64_t bits_of(double value)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int parse_signed(const char *text, int64_t minimum, int64_t maximum, int64_t *out)
{
    const char *digits = *text == '-' || *text == '+' ? text + 1 : text;
    if (*digits == '\0')
        return 0;
    for (const char *digit = digits; *digit != '\0'; ++digit) {
        if (*digit < '0' || *digit > '9')
            return 0;
    }
    char *end = NULL;
    errno = 0;
    intmax_t value = strtoimax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < minimum || value > maximum)
        return 0;
    *out = (int64_t)value;
    return 1;
}

static void addresses(uintptr_t result[4])
{
    result[0] = (uintptr_t)surface.sand.data;
    result[1] = (uintptr_t)surface.gravel.data;
    result[2] = (uintptr_t)surface.thickness.data;
    result[3] = (uintptr_t)blocks;
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

static void print_continuation(mc_java_random random)
{
    putchar('[');
    for (size_t i = 0; i < 4; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%016" PRIx64 "\"", (uint64_t)mc_java_random_next_i64(&random));
    }
    putchar(']');
}

static void print_span(mc_terrain_double_span span)
{
    printf("{\"length\":%zu,\"values\":", span.length);
    print_values(span.data, span.length);
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

static void print_response(int full_state, int fixture, const uintptr_t before[4])
{
    fputs("OK\t{\"state\":", stdout);
    if (full_state)
        print_state();
    else
        fputs("null", stdout);
    fputs(",\"constructor_continuation\":", stdout);
    print_continuation(constructor_random);
    fputs(",\"continuation\":", stdout);
    print_continuation(live_random);
    fputs(",\"climate\":", stdout);
    if (climate_observed) {
        fputs("{\"temperature\":", stdout);
        print_values(manager.a.data, 256);
        fputs(",\"rainfall\":", stdout);
        print_values(manager.b.data, 256);
        fputs(",\"auxiliary\":", stdout);
        print_values(manager.c.data, 256);
        fputs(",\"biomes\":\"", stdout);
        for (size_t i = 0; i < 256; ++i)
            putchar(mc_biome_field(biomes.data[i]));
        fputs("\"}", stdout);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"materials\":", stdout);
    if (climate_observed) {
        putchar('"');
        for (size_t i = 0; i < 256; ++i)
            printf("%02x%02x", (unsigned int)materials[i].top, (unsigned int)materials[i].filler);
        putchar('"');
    } else {
        fputs("null", stdout);
    }
    fputs(",\"chunk_seed\":", stdout);
    if (seed_observed)
        printf("\"%016" PRIx64 "\"", (uint64_t)chunk_seed);
    else
        fputs("null", stdout);
    fputs(",\"sand\":", stdout);
    print_span(surface.sand);
    fputs(",\"gravel\":", stdout);
    print_span(surface.gravel);
    fputs(",\"thickness\":", stdout);
    print_span(surface.thickness);
    fputs(",\"density\":", stdout);
    if (density_observed)
        print_span((mc_terrain_double_span){density, 425});
    else
        fputs("null", stdout);
    fputs(",\"blocks\":", stdout);
    if (blocks != NULL) {
        printf("{\"length\":%zu,\"values\":\"", blocks_length);
        for (size_t i = 0; i < blocks_length; ++i)
            printf("%02x", (unsigned int)blocks[i]);
        fputs("\"}", stdout);
    } else {
        fputs("null", stdout);
    }
    uintptr_t after[4];
    addresses(after);
    fputs(",\"retained\":\"", stdout);
    for (size_t i = 0; i < 4; ++i)
        putchar(!fixture && before[i] != 0 && before[i] == after[i] ? '1' : '0');
    puts("\"}");
}

static int generate_base(int32_t x, int32_t z)
{
    if (mc_biome_sample_square(&manager, mc_java_i32_mul(x, 16), mc_java_i32_mul(z, 16), 16,
                               &biomes) != MC_BIOME_OK)
        return 0;
    for (size_t i = 0; i < 256; ++i) {
        mc_biome_id id = biomes.data[i];
        if (id < MC_BIOME_RAINFOREST || id > MC_BIOME_SKY)
            return 0;
        /* Fixture adapter for observed initialized biome p/q defaults. The
         * runtime surface API accepts actual mutable byte pairs explicitly. */
        materials[i] = id == MC_BIOME_DESERT || id == MC_BIOME_ICE_DESERT
                           ? (mc_terrain_surface_material){12, 12}
                           : (mc_terrain_surface_material){2, 3};
    }
    mc_terrain_normal_workspace work = {{density_volumes[0], 425},
                                        {density_volumes[1], 425},
                                        {density_volumes[2], 425},
                                        {density_columns[0], 25},
                                        {density_columns[1], 25}};
    mc_terrain_normal_climate climate = {manager.a.data, manager.a.length, manager.b.data,
                                         manager.b.length};
    if (!mc_terrain_normal_generate(&state, &work, x, z, &climate,
                                    (mc_terrain_double_span){density, 425}, blocks, blocks_length))
        return 0;
    base_x = x;
    base_z = z;
    base_valid = 1;
    noise_valid = 0;
    climate_observed = 1;
    density_observed = 1;
    return 1;
}

static int handle_request(char *line)
{
    char *fields[5];
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
    uintptr_t before[4];
    addresses(before);
    int full_state = 0;
    int fixture = 0;
    if (strcmp(fields[0], "init") == 0) {
        int64_t seed;
        if (count != 2 || initializations >= PROBE_INITIALIZATIONS ||
            !parse_signed(fields[1], INT64_MIN, INT64_MAX, &seed))
            return 0;
        mc_biome_buffer_free(&biomes);
        mc_biome_manager_free(&manager);
        if (!mc_terrain_normal_init(&state, seed) ||
            mc_biome_manager_init(&manager, seed, &memory) != MC_BIOME_OK)
            return 0;
        memcpy(&original_state, &state, sizeof(state));
        constructor_random = state.random_after_init;
        live_random = constructor_random;
        memset(scratch, 0, sizeof(scratch));
        surface = (mc_terrain_normal_surface_workspace){
            {scratch[0], 256}, {scratch[1], 256}, {scratch[2], 256}};
        blocks = NULL;
        blocks_length = 0;
        initialized = 1;
        ++initializations;
        base_valid = 0;
        noise_valid = 0;
        climate_observed = 0;
        density_observed = 0;
        seed_observed = 0;
        full_state = 1;
        fixture = 1;
    } else if (!initialized) {
        return 0;
    } else if (strcmp(fields[0], "state") == 0) {
        if (count != 1)
            return 0;
        full_state = 1;
    } else if (strcmp(fields[0], "buffers") == 0) {
        int64_t lengths[4];
        if (count != 5)
            return 0;
        for (size_t i = 0; i < 4; ++i) {
            if (!parse_signed(fields[i + 1u], i == 3 ? 32768 : 256,
                              i == 3 ? PROBE_BLOCK_VALUES : PROBE_SCRATCH_VALUES, &lengths[i]))
                return 0;
        }
        mc_terrain_double_span *spans[] = {&surface.sand, &surface.gravel, &surface.thickness};
        for (size_t i = 0; i < 3; ++i) {
            spans[i]->length = (size_t)lengths[i];
            for (size_t value = 0; value < spans[i]->length; ++value)
                spans[i]->data[value] = (double)((int)(value % 11u) - 5) * 0.125;
        }
        blocks = block_storage;
        blocks_length = (size_t)lengths[3];
        for (size_t i = 0; i < blocks_length; ++i)
            blocks[i] = (uint8_t)(i % 251u);
        base_valid = 0;
        noise_valid = 0;
        fixture = 1;
    } else {
        int base = strcmp(fields[0], "base") == 0;
        int seed = strcmp(fields[0], "seed") == 0;
        int composed = strcmp(fields[0], "surface") == 0;
        int apply = strcmp(fields[0], "apply") == 0;
        int64_t x;
        int64_t z;
        if (count != 3 || (!base && !seed && !composed && !apply) ||
            !parse_signed(fields[1], INT32_MIN, INT32_MAX, &x) ||
            !parse_signed(fields[2], INT32_MIN, INT32_MAX, &z))
            return 0;
        if (seed) {
            chunk_seed = mc_terrain_normal_chunk_seed((int32_t)x, (int32_t)z);
            mc_java_random_seed(&live_random, chunk_seed);
            seed_observed = 1;
        } else if (base) {
            if (blocks == NULL || !generate_base((int32_t)x, (int32_t)z))
                return 0;
        } else {
            if (!base_valid || base_x != x || base_z != z ||
                (apply && (!noise_valid || noise_x != x || noise_z != z)))
                return 0;
            int success =
                composed
                    ? mc_terrain_normal_surface(&state, &surface, (int32_t)x, (int32_t)z, materials,
                                                256, &live_random, blocks, blocks_length)
                    : mc_terrain_normal_surface_apply(&surface, materials, 256, &live_random,
                                                      blocks, blocks_length);
            if (!success)
                return 0;
            noise_valid = 1;
            noise_x = (int32_t)x;
            noise_z = (int32_t)z;
        }
    }
    if (memcmp(&state, &original_state, sizeof(state)) != 0)
        return 0;
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
    mc_biome_buffer_free(&biomes);
    mc_biome_manager_free(&manager);
    if (fflush(stdout) == EOF || ferror(stdin) || ferror(stdout) ||
        memory.live_allocation_bytes != 0) {
        fputs("surface probe transport or lifetime failure\n", stderr);
        return 2;
    }
    if (failed)
        fputs("surface probe request or execution failure\n", stderr);
    return failed ? 1 : 0;
}
