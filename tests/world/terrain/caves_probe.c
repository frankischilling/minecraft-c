#include "../../support/java_random.h"
#include "mc/core/math/math.h"
#include "mc/world/biome/biome.h"
#include "mc/world/terrain/caves.h"
#include "mc/world/terrain/surface.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PROBE_CASES 12u
#define PROBE_BLOCKS 32771u

typedef struct probe_request {
    int64_t seed;
    int32_t x, z;
} probe_request;

typedef struct probe_pass {
    uint8_t input[PROBE_BLOCKS], output[PROBE_BLOCKS];
    mc_java_random continuation;
    size_t changes;
} probe_pass;

static mc_math_trig table, original_table;
static mc_terrain_normal_state state, original_state;
static mc_java_random constructor_random, generator_random, cave_random, saved_generator_random;
static mc_biome_memory memory;
static mc_biome_manager manager, saved_manager;
static mc_biome_buffer biomes, saved_biome_descriptor;
static double density[425], volumes[3][425], columns[2][25], fields[3][256], initial_fields[3][256];
static double saved_density[425], saved_volumes[3][425], saved_columns[2][25], saved_fields[3][256],
    saved_climate[3][256];
static mc_terrain_surface_material materials[256], saved_materials[256];
static mc_biome_id saved_biomes[256];
static uint8_t blocks[PROBE_BLOCKS], prefix[PROBE_BLOCKS];
static probe_pass passes[3];
static probe_request requests[PROBE_CASES];
static size_t request_count;

static uint64_t bits_of(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary32 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int parse_signed(const char *text, int64_t low, int64_t high, int64_t *out) {
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

static int read_requests(void) {
    char line[99];
    size_t input_bytes = 0;
    while (fgets(line, (int)sizeof(line), stdin) != NULL) {
        size_t length = strlen(line);
        input_bytes += length;
        if (length == 0 || line[length - 1] != '\n' || input_bytes > 4096 ||
            request_count == PROBE_CASES)
            return 0;
        line[--length] = '\0';
        if (length != 0 && line[length - 1] == '\r')
            line[--length] = '\0';
        if (length > 96 || strncmp(line, "case\t", 5) != 0)
            return 0;
        char *seed = line + 5, *x = strchr(seed, '\t');
        if (x == NULL)
            return 0;
        *x++ = '\0';
        char *z = strchr(x, '\t');
        if (z == NULL)
            return 0;
        *z++ = '\0';
        if (strchr(z, '\t') != NULL)
            return 0;
        int64_t sv, xv, zv;
        if (!parse_signed(seed, INT64_MIN, INT64_MAX, &sv) ||
            !parse_signed(x, (int64_t)INT32_MIN + 8, (int64_t)INT32_MAX - 9, &xv) ||
            !parse_signed(z, (int64_t)INT32_MIN + 8, (int64_t)INT32_MAX - 9, &zv))
            return 0;
        requests[request_count++] = (probe_request){sv, (int32_t)xv, (int32_t)zv};
    }
    return !ferror(stdin) && request_count != 0;
}

static void print_values(const double *values, size_t length) {
    putchar('[');
    for (size_t i = 0; i < length; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%016" PRIx64 "\"", bits_of(values[i]));
    }
    putchar(']');
}

static void print_span(const double *values, size_t length) {
    printf("{\"length\":%zu,\"values\":", length);
    print_values(values, length);
    putchar('}');
}

static void print_bytes(const uint8_t *values) {
    fputs("{\"length\":32771,\"values\":\"", stdout);
    for (size_t i = 0; i < PROBE_BLOCKS; ++i)
        printf("%02x", (unsigned int)values[i]);
    fputs("\"}", stdout);
}

static void print_continuation(mc_java_random random) {
    putchar('[');
    for (size_t i = 0; i < 4; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%016" PRIx64 "\"", (uint64_t)mc_java_random_next_i64(&random));
    }
    putchar(']');
}

static void print_noise(void) {
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

static void print_snapshot(int constructor, const mc_java_random *observed_cave,
                           const uint8_t *observed_blocks) {
    fputs("{\"generator_continuation\":", stdout);
    print_continuation(constructor ? constructor_random : generator_random);
    fputs(",\"cave_continuation\":", stdout);
    if (observed_cave != NULL)
        print_continuation(*observed_cave);
    else
        fputs("null", stdout);
    fputs(",\"noise82\":", stdout);
    print_noise();
    fputs(",\"q425\":", stdout);
    if (constructor)
        fputs("null", stdout);
    else
        print_span(density, 425);
    fputs(",\"scratch\":{", stdout);
    static const char names[] = "defghrst";
    static const size_t lengths[] = {425, 425, 425, 25, 25, 256, 256, 256};
    const double *arrays[] = {volumes[0],
                              volumes[1],
                              volumes[2],
                              columns[0],
                              columns[1],
                              constructor ? initial_fields[0] : fields[0],
                              constructor ? initial_fields[1] : fields[1],
                              constructor ? initial_fields[2] : fields[2]};
    for (size_t i = 0; i < 8; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%c\":", names[i]);
        if (constructor && i < 5)
            fputs("null", stdout);
        else
            print_span(arrays[i], lengths[i]);
    }
    fputs("},\"climate\":", stdout);
    if (constructor)
        fputs("null,\"materials\":null", stdout);
    else {
        fputs("{\"temperature\":", stdout);
        print_span(manager.a.data, manager.a.length);
        fputs(",\"rainfall\":", stdout);
        print_span(manager.b.data, manager.b.length);
        fputs(",\"auxiliary\":", stdout);
        print_span(manager.c.data, manager.c.length);
        fputs(",\"biomes\":\"", stdout);
        for (size_t i = 0; i < 256; ++i)
            putchar(mc_biome_field(biomes.data[i]));
        fputs("\"},\"materials\":\"", stdout);
        for (size_t i = 0; i < 256; ++i)
            printf("%02x%02x", (unsigned int)materials[i].top, (unsigned int)materials[i].filler);
        putchar('"');
    }
    printf(",\"table_unchanged\":%s,\"blocks\":",
           memcmp(&table, &original_table, sizeof(table)) == 0 ? "true" : "false");
    if (observed_blocks == NULL)
        fputs("null", stdout);
    else
        print_bytes(observed_blocks);
    putchar('}');
}

static int initialize_case(probe_request request) {
    mc_biome_buffer_free(&biomes);
    mc_biome_manager_free(&manager);
    if (mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) != MC_BIOME_OK ||
        mc_biome_manager_init(&manager, request.seed, &memory) != MC_BIOME_OK ||
        !mc_terrain_normal_init(&state, request.seed))
        return 0;
    memcpy(&original_state, &state, sizeof(state));
    constructor_random = state.random_after_init;
    generator_random = constructor_random;
    memset(fields, 0, sizeof(fields));
    memcpy(initial_fields, fields, sizeof(fields));
    return memcmp(&table, &original_table, sizeof(table)) == 0;
}

static int prepare_prefix(probe_request request) {
    mc_java_random_reseed(&generator_random, mc_terrain_normal_chunk_seed(request.x, request.z));
    mc_java_random seeded = generator_random;
    if (mc_biome_sample_square(&manager, mc_java_i32_mul(request.x, 16),
                               mc_java_i32_mul(request.z, 16), 16, &biomes) != MC_BIOME_OK ||
        manager.a.length != 256 || manager.b.length != 256 || manager.c.length != 256)
        return 0;
    for (size_t i = 0; i < 256; ++i) {
        mc_biome_id id = biomes.data[i];
        if (id < MC_BIOME_RAINFOREST || id > MC_BIOME_SKY)
            return 0;
        /* Explicit observed initialized p/q defaults; the runtime accepts mutable material pairs.
         */
        materials[i] = id == MC_BIOME_DESERT || id == MC_BIOME_ICE_DESERT
                           ? (mc_terrain_surface_material){12, 12}
                           : (mc_terrain_surface_material){2, 3};
    }
    for (size_t i = 0; i < PROBE_BLOCKS; ++i)
        blocks[i] = (uint8_t)(i % 251);
    mc_terrain_normal_workspace work = {{volumes[0], 425},
                                        {volumes[1], 425},
                                        {volumes[2], 425},
                                        {columns[0], 25},
                                        {columns[1], 25}};
    mc_terrain_normal_climate climate = {manager.a.data, manager.a.length, manager.b.data,
                                         manager.b.length};
    if (!mc_terrain_normal_generate(&state, &work, request.x, request.z, &climate,
                                    (mc_terrain_double_span){density, 425}, blocks, PROBE_BLOCKS) ||
        !mc_test_java_random_equal(&seeded, &generator_random))
        return 0;
    mc_terrain_normal_surface_workspace surface = {
        {fields[0], 256}, {fields[1], 256}, {fields[2], 256}};
    if (!mc_terrain_normal_surface(&state, &surface, request.x, request.z, materials, 256,
                                   &generator_random, blocks, PROBE_BLOCKS))
        return 0;
    memcpy(prefix, blocks, sizeof(blocks));
    memcpy(saved_density, density, sizeof(density));
    memcpy(saved_volumes, volumes, sizeof(volumes));
    memcpy(saved_columns, columns, sizeof(columns));
    memcpy(saved_fields, fields, sizeof(fields));
    memcpy(saved_climate[0], manager.a.data, sizeof(saved_climate[0]));
    memcpy(saved_climate[1], manager.b.data, sizeof(saved_climate[1]));
    memcpy(saved_climate[2], manager.c.data, sizeof(saved_climate[2]));
    memcpy(saved_materials, materials, sizeof(materials));
    memcpy(saved_biomes, biomes.data, sizeof(saved_biomes));
    memcpy(&saved_manager, &manager, sizeof(manager));
    saved_biome_descriptor = biomes;
    saved_generator_random = generator_random;
    return memcmp(&state, &original_state, sizeof(state)) == 0 && blocks[32768] == 0x8a &&
           blocks[32769] == 0x8b && blocks[32770] == 0x8c;
}

static int unchanged_prefix_state(void) {
    return memcmp(&state, &original_state, sizeof(state)) == 0 &&
           mc_test_java_random_equal(&generator_random, &saved_generator_random) &&
           memcmp(density, saved_density, sizeof(density)) == 0 &&
           memcmp(volumes, saved_volumes, sizeof(volumes)) == 0 &&
           memcmp(columns, saved_columns, sizeof(columns)) == 0 &&
           memcmp(fields, saved_fields, sizeof(fields)) == 0 &&
           memcmp(&manager, &saved_manager, sizeof(manager)) == 0 &&
           biomes.data == saved_biome_descriptor.data &&
           biomes.length == saved_biome_descriptor.length &&
           memcmp(manager.a.data, saved_climate[0], sizeof(saved_climate[0])) == 0 &&
           memcmp(manager.b.data, saved_climate[1], sizeof(saved_climate[1])) == 0 &&
           memcmp(manager.c.data, saved_climate[2], sizeof(saved_climate[2])) == 0 &&
           memcmp(biomes.data, saved_biomes, sizeof(saved_biomes)) == 0 &&
           memcmp(materials, saved_materials, sizeof(materials)) == 0 &&
           memcmp(&table, &original_table, sizeof(table)) == 0;
}

static int measure_pass(probe_request request, size_t number) {
    probe_pass *pass = &passes[number];
    memcpy(pass->input, blocks, sizeof(blocks));
    if (!mc_terrain_normal_caves(&table, request.seed, request.x, request.z, &cave_random, blocks,
                                 PROBE_BLOCKS) ||
        !unchanged_prefix_state() || memcmp(blocks + 32768, pass->input + 32768, 3) != 0)
        return 0;
    memcpy(pass->output, blocks, sizeof(blocks));
    pass->continuation = cave_random;
    pass->changes = 0;
    for (size_t i = 0; i < 32768; ++i)
        pass->changes += pass->input[i] != pass->output[i] ? 1u : 0u;
    fputs("CONTROL_CAVES_STATE j_exact noise82_exact q425_exact scratch_exact climate_exact "
          "materials_exact table_exact suffix_exact\n",
          stderr);
    return 1;
}

static int observe(probe_request request, size_t number) {
    if (!initialize_case(request) || !prepare_prefix(request) || !measure_pass(request, 0))
        return 0;
    memcpy(blocks, prefix, sizeof(blocks));
    if (!measure_pass(request, 1) || !measure_pass(request, 2) ||
        memcmp(passes[0].output, passes[1].output, sizeof(blocks)) != 0 ||
        !mc_test_java_random_equal(&passes[0].continuation, &passes[1].continuation))
        return 0;
    printf("OK\t{\"case\":%zu,\"seed\":%" PRId64 ",\"chunk\":[%" PRId32 ",%" PRId32
           "],\"chunk_seed\":\"%016" PRIx64 "\",\"constructor\":",
           number, request.seed, request.x, request.z,
           (uint64_t)mc_terrain_normal_chunk_seed(request.x, request.z));
    print_snapshot(1, NULL, NULL);
    fputs(",\"prefix\":", stdout);
    print_snapshot(0, NULL, prefix);
    fputs(",\"passes\":[", stdout);
    static const char *const labels[] = {"first", "restored", "changed"};
    for (size_t i = 0; i < 3; ++i) {
        if (i != 0)
            putchar(',');
        printf("{\"label\":\"%s\",\"input_blocks\":", labels[i]);
        print_bytes(passes[i].input);
        fputs(",\"after\":", stdout);
        print_snapshot(0, &passes[i].continuation, passes[i].output);
        printf(",\"changed_prefix_bytes\":%zu,\"suffix_unchanged\":true}", passes[i].changes);
    }
    printf("],\"restored_output_equal_first\":%s,\"reused_input_differs_from_prefix\":%s,\"changed_"
           "output_equal_its_input\":%s}\n",
           memcmp(passes[0].output, passes[1].output, sizeof(blocks)) == 0 ? "true" : "false",
           memcmp(prefix, passes[1].output, sizeof(blocks)) != 0 ? "true" : "false",
           memcmp(passes[1].output, passes[2].output, sizeof(blocks)) == 0 ? "true" : "false");
    fprintf(stderr, "CASE_DONE case=%zu\n", number);
    return !ferror(stdout) && !ferror(stderr);
}

int main(void) {
    if (!read_requests() || !mc_math_trig_init(&table)) {
        fputs("invalid bounded cave requests or table initialization failure\n", stderr);
        return 1;
    }
    memcpy(&original_table, &table, sizeof(table));
    fputs("TABLE\t", stdout);
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i)
        printf("%08" PRIx32, float_bits(table.values[i]));
    putchar('\n');
    int result = 0;
    for (size_t i = 0; i < request_count; ++i)
        if (!observe(requests[i], i + 1)) {
            fprintf(stderr, "cave probe failed case=%zu\n", i + 1);
            result = 1;
            break;
        }
    mc_biome_buffer_free(&biomes);
    mc_biome_manager_free(&manager);
    return result == 0 && !ferror(stdout) && !ferror(stderr) ? 0 : 1;
}
