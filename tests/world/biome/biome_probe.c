#include "mc/world/biome/biome.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_VALUES 4096u
#define PROBE_REQUESTS 8192u

static char request[260];
static mc_biome_memory memory;
static mc_biome_manager manager;
static mc_biome_classifier classifier;
static mc_biome_double_buffer temperatures;
static mc_biome_buffer biomes;

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

static int parse_bits(const char *text, size_t digits, uint64_t *out)
{
    if (strlen(text) != digits)
        return 0;
    uint64_t bits = 0;
    for (size_t i = 0; i < digits; ++i) {
        unsigned int nibble;
        if (text[i] >= '0' && text[i] <= '9')
            nibble = (unsigned int)(text[i] - '0');
        else if (text[i] >= 'a' && text[i] <= 'f')
            nibble = (unsigned int)(text[i] - 'a') + 10u;
        else
            return 0;
        bits = (bits << 4u) | nibble;
    }
    *out = bits;
    return 1;
}

static void slots(const void *pointers[6])
{
    pointers[0] = manager.a.data;
    pointers[1] = manager.b.data;
    pointers[2] = manager.c.data;
    pointers[3] = manager.d.data;
    pointers[4] = temperatures.data;
    pointers[5] = biomes.data;
}

static void print_doubles(const mc_biome_double_buffer *buffer)
{
    if (buffer->data == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"length\":%zu,\"values\":[", buffer->length);
    for (size_t i = 0; i < buffer->length; ++i) {
        if (i != 0)
            putchar(',');
        printf("\"%016" PRIx64 "\"", bits_of(buffer->data[i]));
    }
    fputs("]}", stdout);
}

static void print_biomes(const mc_biome_buffer *buffer)
{
    if (buffer->data == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"length\":%zu,\"values\":\"", buffer->length);
    for (size_t i = 0; i < buffer->length; ++i) {
        char field = mc_biome_field(buffer->data[i]);
        putchar(field != '\0' ? field : '-');
    }
    fputs("\"}", stdout);
}

static void print_response(const uintptr_t before[6], const void *returned, int fixture,
                           int catalog, int table, mc_biome_id biome, int number_present,
                           double number)
{
    const void *after[6];
    slots(after);
    fputs("OK\t{\"catalog\":", stdout);
    if (catalog) {
        putchar('[');
        for (int i = MC_BIOME_RAINFOREST; i <= MC_BIOME_SKY; ++i) {
            if (i != MC_BIOME_RAINFOREST)
                putchar(',');
            printf("\"%s\"", mc_biome_name((mc_biome_id)i));
        }
        putchar(']');
    } else {
        fputs("null", stdout);
    }
    fputs(",\"table\":", stdout);
    if (table) {
        putchar('"');
        for (size_t i = 0; i < MC_BIOME_TABLE_ENTRIES; ++i)
            putchar(mc_biome_field(classifier.table[i]));
        putchar('"');
    } else {
        fputs("null", stdout);
    }
    fputs(",\"biome\":", stdout);
    if (biome != MC_BIOME_NONE)
        printf("\"%c\"", mc_biome_field(biome));
    else
        fputs("null", stdout);
    fputs(",\"number\":", stdout);
    if (number_present)
        printf("\"%016" PRIx64 "\"", bits_of(number));
    else
        fputs("null", stdout);
    fputs(",\"a\":", stdout);
    print_doubles(&manager.a);
    fputs(",\"b\":", stdout);
    print_doubles(&manager.b);
    fputs(",\"c\":", stdout);
    print_doubles(&manager.c);
    fputs(",\"d\":", stdout);
    print_biomes(&manager.d);
    fputs(",\"t\":", stdout);
    print_doubles(&temperatures);
    fputs(",\"o\":", stdout);
    print_biomes(&biomes);
    fputs(",\"retained\":\"", stdout);
    for (size_t i = 0; i < 6; ++i)
        putchar(!fixture && before[i] != 0 && before[i] == (uintptr_t)after[i] ? '1' : '0');
    fputs("\",\"returned\":\"", stdout);
    for (size_t i = 0; i < 6; ++i)
        putchar(returned != NULL && returned == after[i] ? '1' : '0');
    fputs("\",\"aliases\":\"", stdout);
    for (size_t i = 0; i < 6; ++i) {
        for (size_t j = i + 1u; j < 6; ++j)
            putchar(after[i] != NULL && after[i] == after[j] ? '1' : '0');
    }
    puts("\"}");
}

static int replace_buffers(const int64_t lengths[6])
{
    mc_biome_double_buffer *double_buffers[] = {&manager.a, &manager.b, &manager.c, &temperatures};
    const size_t double_slots[] = {0, 1, 2, 4};
    mc_biome_buffer *biome_buffers[] = {&manager.d, &biomes};
    const size_t biome_slots[] = {3, 5};
    for (size_t i = 0; i < 4; ++i) {
        mc_biome_double_buffer_free(double_buffers[i]);
        int64_t length = lengths[double_slots[i]];
        if (length >= 0) {
            if (mc_biome_double_buffer_resize(&memory, double_buffers[i], (size_t)length) !=
                MC_BIOME_OK)
                return 0;
            for (size_t value = 0; value < (size_t)length; ++value)
                double_buffers[i]->data[value] = (double)((int)(value % 11u) - 5) * 0.125;
        }
    }
    for (size_t i = 0; i < 2; ++i) {
        mc_biome_buffer_free(biome_buffers[i]);
        int64_t length = lengths[biome_slots[i]];
        if (length >= 0) {
            if (mc_biome_buffer_resize(&memory, biome_buffers[i], (size_t)length) != MC_BIOME_OK)
                return 0;
            for (size_t value = 0; value < (size_t)length; ++value)
                biome_buffers[i]->data[value] = (mc_biome_id)(1u + value % 13u);
        }
    }
    return 1;
}

static int handle_request(char *line)
{
    char *fields[7];
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
    const void *current[6];
    uintptr_t before[6];
    slots(current);
    for (size_t i = 0; i < 6; ++i)
        before[i] = (uintptr_t)current[i];
    const void *returned = NULL;
    mc_biome_id biome = MC_BIOME_NONE;
    double number = 0;
    int number_present = 0;
    int fixture = 0;
    int catalog = strcmp(fields[0], "catalog") == 0;
    int table = strcmp(fields[0], "table") == 0;
    mc_biome_status status = MC_BIOME_INVALID_ARGUMENT;
    if (catalog || table) {
        if (count != 1)
            return 0;
        status = MC_BIOME_OK;
    } else if (strcmp(fields[0], "float") == 0 || strcmp(fields[0], "double") == 0) {
        int single = strcmp(fields[0], "float") == 0;
        uint64_t tbits;
        uint64_t rbits;
        if (count != 3 || !parse_bits(fields[1], single ? 8u : 16u, &tbits) ||
            !parse_bits(fields[2], single ? 8u : 16u, &rbits))
            return 0;
        if (single) {
            uint32_t tword = (uint32_t)tbits;
            uint32_t rword = (uint32_t)rbits;
            float t;
            float r;
            memcpy(&t, &tword, sizeof(t));
            memcpy(&r, &rword, sizeof(r));
            status = mc_biome_classify_float(t, r, &biome);
        } else {
            double t;
            double r;
            memcpy(&t, &tbits, sizeof(t));
            memcpy(&r, &rbits, sizeof(r));
            status = mc_biome_lookup(&classifier, t, r, &biome);
        }
    } else if (strcmp(fields[0], "init") == 0) {
        int64_t seed;
        if (count != 2 || !parse_signed(fields[1], INT64_MIN, INT64_MAX, &seed))
            return 0;
        mc_biome_double_buffer_free(&temperatures);
        mc_biome_buffer_free(&biomes);
        mc_biome_manager_free(&manager);
        status = mc_biome_manager_init(&manager, seed, &memory);
        fixture = 1;
    } else if (strcmp(fields[0], "buffers") == 0) {
        int64_t lengths[6];
        if (count != 7 || !manager.initialized)
            return 0;
        for (size_t i = 0; i < 6; ++i) {
            if (!parse_signed(fields[i + 1u], -1, PROBE_VALUES, &lengths[i]))
                return 0;
        }
        if (!replace_buffers(lengths))
            return 0;
        status = MC_BIOME_OK;
        fixture = 1;
    } else {
        int temperature = strcmp(fields[0], "temperature") == 0;
        int direct = strcmp(fields[0], "biomes") == 0;
        int cached = strcmp(fields[0], "cached") == 0;
        int point = strcmp(fields[0], "point") == 0;
        int chunk = strcmp(fields[0], "chunk") == 0;
        int raw = strcmp(fields[0], "raw") == 0;
        size_t expected = temperature             ? 5u
                          : direct || cached      ? 4u
                          : point || chunk || raw ? 3u
                                                  : 0u;
        int64_t x;
        int64_t z;
        int64_t width = 1;
        int64_t height = 1;
        if (count != expected || !manager.initialized ||
            !parse_signed(fields[1], INT32_MIN, INT32_MAX, &x) ||
            !parse_signed(fields[2], INT32_MIN, INT32_MAX, &z))
            return 0;
        if (temperature || direct || cached) {
            if (!parse_signed(fields[3], 0, PROBE_VALUES, &width))
                return 0;
            height = width;
            if (temperature && !parse_signed(fields[4], 0, PROBE_VALUES, &height))
                return 0;
            if (width != 0 && height > (int64_t)PROBE_VALUES / width)
                return 0;
        }
        if (temperature) {
            status = mc_biome_temperatures(&manager, (int32_t)x, (int32_t)z, (size_t)width,
                                           (size_t)height, &temperatures);
            returned = temperatures.data;
        } else if (direct) {
            status =
                mc_biome_sample_square(&manager, (int32_t)x, (int32_t)z, (size_t)width, &biomes);
            returned = biomes.data;
        } else if (cached) {
            status = mc_biome_cached_square(&manager, (int32_t)x, (int32_t)z, (size_t)width);
            returned = manager.d.data;
        } else if (point) {
            status = mc_biome_point(&manager, (int32_t)x, (int32_t)z, &biome);
        } else if (chunk) {
            status = mc_biome_chunk(&manager, (int32_t)x, (int32_t)z, &biome);
        } else if (raw) {
            status = mc_biome_client_raw_temperature(&manager, (int32_t)x, (int32_t)z, &number);
            number_present = 1;
        }
    }
    if (status != MC_BIOME_OK)
        return 0;
    print_response(before, returned, fixture, catalog, table, biome, number_present, number);
    return 1;
}

int main(void)
{
    if (mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) != MC_BIOME_OK ||
        mc_biome_classifier_init(&classifier) != MC_BIOME_OK)
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
        if (length > 256) {
            failed = 1;
            break;
        }
        if (!handle_request(request))
            puts("ERR");
    }
    mc_biome_double_buffer_free(&temperatures);
    mc_biome_buffer_free(&biomes);
    mc_biome_manager_free(&manager);
    if (fflush(stdout) == EOF || ferror(stdin) || ferror(stdout)) {
        fputs("probe transport failure\n", stderr);
        return 2;
    }
    return memory.live_allocation_bytes != 0 ? 2 : failed ? 1 : 0;
}
