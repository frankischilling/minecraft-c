#include "mc/world/biome/biome.h"

#include "biome_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef enum biome_buffer_kind { BIOME_DOUBLE_BUFFER, BIOME_ID_BUFFER } biome_buffer_kind;

/* Every payload is an array of double or mc_biome_id. The union supplies those
 * alignments, including the pointers used by the allocation bookkeeping. */
typedef union biome_allocation {
    struct {
        mc_biome_memory *owner;
        size_t bytes;
        size_t length;
        biome_buffer_kind kind;
    } metadata;
    double double_alignment;
    mc_biome_id biome_alignment;
    void *pointer_alignment;
} biome_allocation;

typedef struct biome_replacement {
    void *data;
    size_t length;
} biome_replacement;

const char *mc_biome_status_string(mc_biome_status status)
{
    switch (status) {
        case MC_BIOME_OK:
            return "ok";
        case MC_BIOME_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_BIOME_LIMIT:
            return "resource limit";
        case MC_BIOME_ALLOCATION_FAILED:
            return "allocation failed";
    }
    return "unknown biome status";
}

const char *mc_biome_name(mc_biome_id biome)
{
    static const char *const names[] = {
        "Rainforest", "Swampland", "Seasonal Forest", "Forest", "Savanna", "Shrubland", "Taiga",
        "Desert",     "Plains",    "Ice Desert",      "Tundra", "Hell",    "Sky"};
    if (biome < MC_BIOME_RAINFOREST || biome > MC_BIOME_SKY) {
        return NULL;
    }
    return names[(size_t)biome - 1];
}

char mc_biome_field(mc_biome_id biome)
{
    return biome < MC_BIOME_RAINFOREST || biome > MC_BIOME_SKY ? '\0'
                                                               : (char)('a' + (int)biome - 1);
}

static mc_biome_id biome_classify(float temperature, float rainfall)
{
    float moisture = rainfall * temperature;
    if (temperature < 0.1f) {
        return MC_BIOME_TUNDRA;
    }
    if (moisture < 0.2f) {
        if (temperature < 0.5f) {
            return MC_BIOME_TUNDRA;
        }
        return temperature < 0.95f ? MC_BIOME_SAVANNA : MC_BIOME_DESERT;
    }
    if (moisture > 0.5f && temperature < 0.7f) {
        return MC_BIOME_SWAMPLAND;
    }
    if (temperature < 0.5f) {
        return MC_BIOME_TAIGA;
    }
    if (temperature < 0.97f) {
        return moisture < 0.35f ? MC_BIOME_SHRUBLAND : MC_BIOME_FOREST;
    }
    if (moisture < 0.45f) {
        return MC_BIOME_PLAINS;
    }
    return moisture < 0.9f ? MC_BIOME_SEASONAL_FOREST : MC_BIOME_RAINFOREST;
}

mc_biome_status mc_biome_classify_float(float temperature, float rainfall, mc_biome_id *out)
{
    if (out == NULL ||
        !(temperature >= 0.0f && temperature <= 1.0f && rainfall >= 0.0f && rainfall <= 1.0f)) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    *out = biome_classify(temperature, rainfall);
    return MC_BIOME_OK;
}

mc_biome_status mc_biome_classifier_init(mc_biome_classifier *classifier)
{
    if (classifier == NULL) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    for (size_t temperature = 0; temperature < 64; ++temperature) {
        for (size_t rainfall = 0; rainfall < 64; ++rainfall) {
            float t = (float)temperature / 63.0f;
            float r = (float)rainfall / 63.0f;
            classifier->table[temperature + rainfall * 64u] = biome_classify(t, r);
        }
    }
    return MC_BIOME_OK;
}

static mc_biome_id biome_lookup(const mc_biome_classifier *classifier, double temperature,
                                double rainfall)
{
    size_t t = (size_t)mc_java_double_to_i32(temperature * 63.0);
    size_t r = (size_t)mc_java_double_to_i32(rainfall * 63.0);
    return classifier->table[t + r * 64u];
}

mc_biome_status mc_biome_lookup(const mc_biome_classifier *classifier, double temperature,
                                double rainfall, mc_biome_id *out)
{
    if (classifier == NULL || out == NULL ||
        !(temperature >= 0.0 && temperature <= 1.0 && rainfall >= 0.0 && rainfall <= 1.0)) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    *out = biome_lookup(classifier, temperature, rainfall);
    return MC_BIOME_OK;
}

static bool biome_memory_valid(const mc_biome_memory *memory)
{
    return memory != NULL && memory->live_allocation_bytes <= memory->max_allocation_bytes;
}

mc_biome_status mc_biome_memory_init(mc_biome_memory *memory, size_t max_allocation_bytes)
{
    if (memory == NULL || memory->live_allocation_bytes != 0) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    memory->max_allocation_bytes = max_allocation_bytes;
    return MC_BIOME_OK;
}

static bool biome_buffer_valid(const mc_biome_memory *memory, const void *data, size_t length,
                               biome_buffer_kind kind)
{
    if (data == NULL) {
        return length == 0;
    }
    const biome_allocation *allocation = (const biome_allocation *)data - 1;
    return allocation->metadata.owner == memory && allocation->metadata.length == length &&
           allocation->metadata.kind == kind;
}

static mc_biome_status biome_allocate(mc_biome_memory *memory, size_t length,
                                      biome_buffer_kind kind, void **out)
{
    size_t element_bytes = kind == BIOME_DOUBLE_BUFFER ? sizeof(double) : sizeof(mc_biome_id);
    if (length > INT32_MAX || length > SIZE_MAX / element_bytes ||
        length * element_bytes > SIZE_MAX - sizeof(biome_allocation)) {
        return MC_BIOME_LIMIT;
    }
    size_t bytes = sizeof(biome_allocation) + length * element_bytes;
    if (bytes > memory->max_allocation_bytes - memory->live_allocation_bytes) {
        return MC_BIOME_LIMIT;
    }
    biome_allocation *allocation = calloc(1, bytes);
    if (allocation == NULL) {
        return MC_BIOME_ALLOCATION_FAILED;
    }
    allocation->metadata.owner = memory;
    allocation->metadata.bytes = bytes;
    allocation->metadata.length = length;
    allocation->metadata.kind = kind;
    memory->live_allocation_bytes += bytes;
    *out = allocation + 1;
    return MC_BIOME_OK;
}

static void biome_release(void *data)
{
    if (data != NULL) {
        biome_allocation *allocation = (biome_allocation *)data - 1;
        allocation->metadata.owner->live_allocation_bytes -= allocation->metadata.bytes;
        free(allocation);
    }
}

void mc_biome_double_buffer_free(mc_biome_double_buffer *buffer)
{
    if (buffer != NULL) {
        biome_release(buffer->data);
        buffer->data = NULL;
        buffer->length = 0;
    }
}

void mc_biome_buffer_free(mc_biome_buffer *buffer)
{
    if (buffer != NULL) {
        biome_release(buffer->data);
        buffer->data = NULL;
        buffer->length = 0;
    }
}

mc_biome_status mc_biome_double_buffer_resize(mc_biome_memory *memory,
                                              mc_biome_double_buffer *buffer, size_t length)
{
    if (!biome_memory_valid(memory) || buffer == NULL ||
        !biome_buffer_valid(memory, buffer->data, buffer->length, BIOME_DOUBLE_BUFFER)) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    if (buffer->data != NULL && buffer->length == length) {
        return MC_BIOME_OK;
    }
    void *replacement;
    mc_biome_status status = biome_allocate(memory, length, BIOME_DOUBLE_BUFFER, &replacement);
    if (status != MC_BIOME_OK) {
        return status;
    }
    biome_release(buffer->data);
    buffer->data = replacement;
    buffer->length = length;
    return MC_BIOME_OK;
}

mc_biome_status mc_biome_buffer_resize(mc_biome_memory *memory, mc_biome_buffer *buffer,
                                       size_t length)
{
    if (!biome_memory_valid(memory) || buffer == NULL ||
        !biome_buffer_valid(memory, buffer->data, buffer->length, BIOME_ID_BUFFER)) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    if (buffer->data != NULL && buffer->length == length) {
        return MC_BIOME_OK;
    }
    void *replacement;
    mc_biome_status status = biome_allocate(memory, length, BIOME_ID_BUFFER, &replacement);
    if (status != MC_BIOME_OK) {
        return status;
    }
    biome_release(buffer->data);
    buffer->data = replacement;
    buffer->length = length;
    return MC_BIOME_OK;
}

static bool biome_manager_valid(const mc_biome_manager *manager)
{
    return manager != NULL && manager->initialized == 1 && biome_memory_valid(manager->memory) &&
           biome_buffer_valid(manager->memory, manager->a.data, manager->a.length,
                              BIOME_DOUBLE_BUFFER) &&
           biome_buffer_valid(manager->memory, manager->b.data, manager->b.length,
                              BIOME_DOUBLE_BUFFER) &&
           biome_buffer_valid(manager->memory, manager->c.data, manager->c.length,
                              BIOME_DOUBLE_BUFFER) &&
           biome_buffer_valid(manager->memory, manager->d.data, manager->d.length, BIOME_ID_BUFFER);
}

bool mc_biome_manager_sampling_valid(const mc_biome_manager *manager)
{
    return biome_manager_valid(manager);
}

mc_biome_status mc_biome_manager_construct_channel(mc_biome_manager *manager, int64_t seed,
                                                   mc_biome_construction_channel channel)
{
    if (manager == NULL) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    mc_noise_simplex *layers;
    size_t count;
    int64_t multiplier;
    switch (channel) {
        case MC_BIOME_CONSTRUCT_TEMPERATURE:
            layers = manager->temperature_layers;
            count = 4;
            multiplier = INT64_C(9871);
            break;
        case MC_BIOME_CONSTRUCT_RAINFALL:
            layers = manager->rainfall_layers;
            count = 4;
            multiplier = INT64_C(39811);
            break;
        case MC_BIOME_CONSTRUCT_AUXILIARY:
            layers = manager->auxiliary_layers;
            count = 2;
            multiplier = INT64_C(543321);
            break;
        default:
            return MC_BIOME_INVALID_ARGUMENT;
    }
    mc_java_random random;
    mc_noise_simplex_octaves noise;
    mc_java_random_seed(&random, mc_java_i64_mul(seed, multiplier));
    return mc_noise_simplex_octaves_init(&noise, layers, count, &random)
               ? MC_BIOME_OK
               : MC_BIOME_INVALID_ARGUMENT;
}

mc_biome_status mc_biome_manager_finish_construction(mc_biome_manager *manager,
                                                     mc_biome_memory *memory)
{
    if (manager == NULL || !biome_memory_valid(memory)) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    mc_biome_status status = mc_biome_classifier_init(&manager->classifier);
    if (status != MC_BIOME_OK) {
        return status;
    }
    manager->memory = memory;
    manager->initialized = 1;
    return MC_BIOME_OK;
}

mc_biome_status mc_biome_manager_init(mc_biome_manager *manager, int64_t seed,
                                      mc_biome_memory *memory)
{
    if (manager == NULL || !biome_memory_valid(memory) || manager->initialized != 0 ||
        manager->memory != NULL || manager->a.data != NULL || manager->b.data != NULL ||
        manager->c.data != NULL || manager->d.data != NULL || manager->a.length != 0 ||
        manager->b.length != 0 || manager->c.length != 0 || manager->d.length != 0) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    mc_biome_manager candidate = {0};
    mc_biome_status status =
        mc_biome_manager_construct_channel(&candidate, seed, MC_BIOME_CONSTRUCT_TEMPERATURE);
    if (status == MC_BIOME_OK) {
        status = mc_biome_manager_construct_channel(&candidate, seed, MC_BIOME_CONSTRUCT_RAINFALL);
    }
    if (status == MC_BIOME_OK) {
        status = mc_biome_manager_construct_channel(&candidate, seed, MC_BIOME_CONSTRUCT_AUXILIARY);
    }
    if (status == MC_BIOME_OK) {
        status = mc_biome_manager_finish_construction(&candidate, memory);
    }
    if (status != MC_BIOME_OK) {
        return status;
    }
    *manager = candidate;
    return MC_BIOME_OK;
}

void mc_biome_manager_free(mc_biome_manager *manager)
{
    if (manager != NULL) {
        mc_biome_double_buffer_free(&manager->a);
        mc_biome_double_buffer_free(&manager->b);
        mc_biome_double_buffer_free(&manager->c);
        mc_biome_buffer_free(&manager->d);
        memset(manager, 0, sizeof(*manager));
    }
}

static mc_biome_status biome_dimensions(size_t width, size_t height, size_t *elements)
{
    if (width > INT32_MAX || height > INT32_MAX ||
        (height != 0 && width > (size_t)INT32_MAX / height)) {
        return MC_BIOME_LIMIT;
    }
    size_t count = width * height;
    if (count > SIZE_MAX / sizeof(double) || count > SIZE_MAX / sizeof(mc_biome_id)) {
        return MC_BIOME_LIMIT;
    }
    *elements = count;
    return MC_BIOME_OK;
}

static mc_biome_status biome_prepare(mc_biome_memory *memory, const void *data,
                                     size_t current_length, size_t needed, biome_buffer_kind kind,
                                     biome_replacement *replacement)
{
    replacement->length = needed;
    return data == NULL || current_length < needed
               ? biome_allocate(memory, needed, kind, &replacement->data)
               : MC_BIOME_OK;
}

static void biome_rollback(biome_replacement *replacements, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        biome_release(replacements[i].data);
    }
}

static void biome_commit_double(mc_biome_double_buffer *buffer, biome_replacement *replacement)
{
    if (replacement->data != NULL) {
        biome_release(buffer->data);
        buffer->data = replacement->data;
        buffer->length = replacement->length;
        replacement->data = NULL;
    }
}

static void biome_commit_ids(mc_biome_buffer *buffer, biome_replacement *replacement)
{
    if (replacement->data != NULL) {
        biome_release(buffer->data);
        buffer->data = replacement->data;
        buffer->length = replacement->length;
        replacement->data = NULL;
    }
}

static void biome_channel(mc_noise_simplex *layers, size_t layer_count, int32_t x, int32_t z,
                          size_t width, size_t height, double scale, double frequency,
                          mc_biome_double_buffer *buffer)
{
    mc_noise_simplex_octaves channel = {layers, layer_count};
    mc_noise_simplex_grid grid = {(double)x, (double)z, width, height, scale, scale};
    /* All dimensions, spans, layer counts and pointers have been validated
     * before committing replacements. This call has no remaining failure. */
    (void)mc_noise_simplex_octaves_fill(&channel, &grid, frequency,
                                        MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER, buffer->data,
                                        buffer->length);
}

mc_biome_status mc_biome_sample_channel_direct(mc_biome_manager *manager, int32_t x, int32_t z,
                                               size_t size, mc_biome_construction_channel channel)
{
    if (!biome_manager_valid(manager)) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    mc_biome_double_buffer *buffer;
    mc_noise_simplex *layers;
    size_t layer_count;
    double scale, frequency;
    switch (channel) {
        case MC_BIOME_CONSTRUCT_TEMPERATURE:
            buffer = &manager->a;
            layers = manager->temperature_layers;
            layer_count = 4;
            scale = (double)0.025f;
            frequency = 0.25;
            break;
        case MC_BIOME_CONSTRUCT_RAINFALL:
            buffer = &manager->b;
            layers = manager->rainfall_layers;
            layer_count = 4;
            scale = (double)0.05f;
            frequency = 0.3333333333333333;
            break;
        case MC_BIOME_CONSTRUCT_AUXILIARY:
            buffer = &manager->c;
            layers = manager->auxiliary_layers;
            layer_count = 2;
            scale = 0.25;
            frequency = 0.5882352941176471;
            break;
        default:
            return MC_BIOME_INVALID_ARGUMENT;
    }
    size_t elements;
    mc_biome_status status = biome_dimensions(size, size, &elements);
    if (status != MC_BIOME_OK) {
        return status;
    }
    biome_replacement replacement = {0};
    status = biome_prepare(manager->memory, buffer->data, buffer->length, elements,
                           BIOME_DOUBLE_BUFFER, &replacement);
    if (status != MC_BIOME_OK) {
        return status;
    }
    mc_biome_double_buffer target = *buffer;
    if (replacement.data != NULL) {
        target.data = replacement.data;
        target.length = replacement.length;
    }
    mc_noise_simplex_octaves noise = {layers, layer_count};
    mc_noise_simplex_grid grid = {(double)x, (double)z, size, size, scale, scale};
    /* Raw fill validates before clearing, then has no later failure. Reused
     * storage therefore remains unchanged on refusal, while an unpublished
     * replacement can be retired without touching the actual field. */
    if (!mc_noise_simplex_octaves_fill(&noise, &grid, frequency,
                                       MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER, target.data,
                                       target.length)) {
        biome_release(replacement.data);
        return MC_BIOME_INVALID_ARGUMENT;
    }
    biome_commit_double(buffer, &replacement);
    return MC_BIOME_OK;
}

static double biome_temperature_mix(double raw_temperature, double auxiliary)
{
    double weight = 0.01;
    double complement = 1.0 - weight;
    return ((raw_temperature * 0.15) + 0.7) * complement + auxiliary * weight;
}

void mc_biome_sample_process_cell(mc_biome_manager *manager, size_t index)
{
    double auxiliary = manager->c.data[index] * 1.1 + 0.5;
    double mixed = biome_temperature_mix(manager->a.data[index], auxiliary);
    double weight = 0.002;
    double complement = 1.0 - weight;
    double rainfall = ((manager->b.data[index] * 0.15) + 0.5) * complement + auxiliary * weight;
    double temperature = 1.0 - ((1.0 - mixed) * (1.0 - mixed));
    if (temperature < 0.0) {
        temperature = 0.0;
    }
    if (rainfall < 0.0) {
        rainfall = 0.0;
    }
    if (temperature > 1.0) {
        temperature = 1.0;
    }
    if (rainfall > 1.0) {
        rainfall = 1.0;
    }
    manager->a.data[index] = temperature;
    manager->b.data[index] = rainfall;
}

mc_biome_status mc_biome_temperatures(mc_biome_manager *manager, int32_t x, int32_t z, size_t width,
                                      size_t height, mc_biome_double_buffer *output)
{
    if (!biome_manager_valid(manager) || output == NULL || output == &manager->a ||
        output == &manager->b || output == &manager->c ||
        !biome_buffer_valid(manager->memory, output->data, output->length, BIOME_DOUBLE_BUFFER) ||
        (output->data != NULL &&
         (output->data == manager->a.data || output->data == manager->b.data ||
          output->data == manager->c.data))) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    size_t elements;
    mc_biome_status status = biome_dimensions(width, height, &elements);
    if (status != MC_BIOME_OK) {
        return status;
    }
    biome_replacement replacements[2] = {0};
    status = biome_prepare(manager->memory, output->data, output->length, elements,
                           BIOME_DOUBLE_BUFFER, &replacements[0]);
    if (status == MC_BIOME_OK) {
        status = biome_prepare(manager->memory, manager->c.data, manager->c.length, elements,
                               BIOME_DOUBLE_BUFFER, &replacements[1]);
    }
    if (status != MC_BIOME_OK) {
        biome_rollback(replacements, 2);
        return status;
    }
    biome_commit_double(output, &replacements[0]);
    biome_commit_double(&manager->c, &replacements[1]);
    biome_channel(manager->temperature_layers, 4, x, z, width, height, (double)0.025f, 0.25,
                  output);
    biome_channel(manager->auxiliary_layers, 2, x, z, width, height, 0.25, 0.5882352941176471,
                  &manager->c);
    for (size_t i = 0; i < elements; ++i) {
        double auxiliary = manager->c.data[i] * 1.1 + 0.5;
        double mixed = biome_temperature_mix(output->data[i], auxiliary);
        double temperature = 1.0 - ((1.0 - mixed) * (1.0 - mixed));
        if (temperature < 0.0) {
            temperature = 0.0;
        }
        if (temperature > 1.0) {
            temperature = 1.0;
        }
        output->data[i] = temperature;
    }
    return MC_BIOME_OK;
}

static mc_biome_status biome_square(mc_biome_manager *manager, int32_t x, int32_t z, size_t size,
                                    mc_biome_buffer *output)
{
    size_t elements;
    mc_biome_status status = biome_dimensions(size, size, &elements);
    if (status != MC_BIOME_OK) {
        return status;
    }
    biome_replacement replacements[4] = {0};
    status = biome_prepare(manager->memory, output->data, output->length, elements, BIOME_ID_BUFFER,
                           &replacements[0]);
    mc_biome_double_buffer *climate[3] = {&manager->a, &manager->b, &manager->c};
    for (size_t i = 0; i < 3 && status == MC_BIOME_OK; ++i) {
        status = biome_prepare(manager->memory, climate[i]->data, climate[i]->length, elements,
                               BIOME_DOUBLE_BUFFER, &replacements[i + 1]);
    }
    if (status != MC_BIOME_OK) {
        biome_rollback(replacements, 4);
        return status;
    }
    biome_commit_ids(output, &replacements[0]);
    for (size_t i = 0; i < 3; ++i) {
        biome_commit_double(climate[i], &replacements[i + 1]);
    }
    biome_channel(manager->temperature_layers, 4, x, z, size, size, (double)0.025f, 0.25,
                  &manager->a);
    biome_channel(manager->rainfall_layers, 4, x, z, size, size, (double)0.05f, 0.3333333333333333,
                  &manager->b);
    biome_channel(manager->auxiliary_layers, 2, x, z, size, size, 0.25, 0.5882352941176471,
                  &manager->c);
    for (size_t i = 0; i < elements; ++i) {
        mc_biome_sample_process_cell(manager, i);
        double temperature = manager->a.data[i];
        double rainfall = manager->b.data[i];
        output->data[i] = biome_lookup(&manager->classifier, temperature, rainfall);
    }
    return MC_BIOME_OK;
}

mc_biome_status mc_biome_sample_square(mc_biome_manager *manager, int32_t x, int32_t z, size_t size,
                                       mc_biome_buffer *output)
{
    if (!biome_manager_valid(manager) || output == NULL || output == &manager->d ||
        !biome_buffer_valid(manager->memory, output->data, output->length, BIOME_ID_BUFFER) ||
        (output->data != NULL && output->data == manager->d.data)) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    return biome_square(manager, x, z, size, output);
}

mc_biome_status mc_biome_cached_square(mc_biome_manager *manager, int32_t x, int32_t z, size_t size)
{
    if (!biome_manager_valid(manager)) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    return biome_square(manager, x, z, size, &manager->d);
}

mc_biome_status mc_biome_point(mc_biome_manager *manager, int32_t x, int32_t z, mc_biome_id *out)
{
    if (out == NULL) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    mc_biome_status status = mc_biome_cached_square(manager, x, z, 1);
    if (status == MC_BIOME_OK) {
        *out = manager->d.data[0];
    }
    return status;
}

mc_biome_status mc_biome_chunk(mc_biome_manager *manager, int32_t chunk_x, int32_t chunk_z,
                               mc_biome_id *out)
{
    return mc_biome_point(manager, mc_java_i32_shl(chunk_x, 4), mc_java_i32_shl(chunk_z, 4), out);
}

mc_biome_status mc_biome_client_raw_temperature(mc_biome_manager *manager, int32_t x, int32_t z,
                                                double *out)
{
    if (!biome_manager_valid(manager) || out == NULL) {
        return MC_BIOME_INVALID_ARGUMENT;
    }
    biome_replacement replacement = {0};
    mc_biome_status status = biome_prepare(manager->memory, manager->a.data, manager->a.length, 1,
                                           BIOME_DOUBLE_BUFFER, &replacement);
    if (status != MC_BIOME_OK) {
        return status;
    }
    biome_commit_double(&manager->a, &replacement);
    biome_channel(manager->temperature_layers, 4, x, z, 1, 1, (double)0.025f, 0.5, &manager->a);
    *out = manager->a.data[0];
    return MC_BIOME_OK;
}
