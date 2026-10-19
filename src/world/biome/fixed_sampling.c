#include "sampling_internal.h"

#include "../../block/registry/registry_internal.h"

#include <limits.h>
#include <string.h>

static mc_registry_error native_error(mc_registry_status status, const char *message)
{
    return (mc_registry_error){.status = status,
                               .message = message,
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_registry_status finish(mc_biome_sampling_error *error, const mc_registry_error *cause,
                                 mc_biome_status climate_status, int32_t pc,
                                 const mc_biome_reference_array *array, size_t completed)
{
    if (error != NULL)
        *error = (mc_biome_sampling_error){.registry = *cause,
                                           .climate_status = climate_status,
                                           .sampler_pc = pc,
                                           .array = array,
                                           .cells_completed = completed,
                                           .next_index = 0};
    return cause->status;
}

static mc_registry_status climate_result(mc_biome_status status)
{
    switch (status) {
        case MC_BIOME_OK:
            return MC_REGISTRY_OK;
        case MC_BIOME_LIMIT:
            return MC_REGISTRY_LIMIT;
        case MC_BIOME_ALLOCATION_FAILED:
            return MC_REGISTRY_ALLOCATION_FAILED;
        case MC_BIOME_INVALID_ARGUMENT:
        default:
            return MC_REGISTRY_INVALID_ARGUMENT;
    }
}

static mc_biome_status fresh_buffer(mc_biome_memory *memory, mc_biome_double_buffer *field,
                                    size_t length)
{
    mc_biome_double_buffer fresh = {0};
    const mc_biome_status status = mc_biome_double_buffer_resize(memory, &fresh, length);
    if (status != MC_BIOME_OK)
        return status;
    /* The old request remains charged through successful new allocation. */
    mc_biome_double_buffer_free(field);
    *field = fresh;
    return MC_BIOME_OK;
}

static mc_registry_error fill_double(mc_biome_double_buffer *array, size_t length, double value)
{
    /* Arrays.fill checks the actual array even for an empty requested range. */
    if (array->data == NULL)
        return native_error(MC_REGISTRY_NULL_REFERENCE, "Fixed Biome double fill array");
    if (length > array->length)
        return native_error(MC_REGISTRY_INDEX_OUT_OF_RANGE, "Fixed Biome double fill range");
    for (size_t i = 0; i < length; ++i)
        memcpy(&array->data[i], &value, sizeof(value));
    return native_error(MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_biome_fixed_sample(mc_registry_classes *classes,
                                                  mc_biome_fixed_storage *storage, int32_t x,
                                                  int32_t z, size_t width, size_t height,
                                                  mc_biome_reference_array *input,
                                                  mc_biome_reference_array **out,
                                                  mc_biome_sampling_error *error)
{
    (void)x;
    (void)z;
    mc_registry_error cause = native_error(MC_REGISTRY_OK, NULL);
    if (classes == NULL || storage == NULL || out == NULL) {
        cause = native_error(MC_REGISTRY_INVALID_ARGUMENT, "Fixed Biome sampler owner/output");
        return finish(error, &cause, MC_BIOME_OK, -1, NULL, 0);
    }
    if (classes->entered != 0) {
        cause = native_error(MC_REGISTRY_BUSY, "Fixed Biome sampler registry entered");
        return finish(error, &cause, MC_BIOME_OK, -1, NULL, 0);
    }
    if (storage->memory.live_allocation_bytes > storage->memory.max_allocation_bytes) {
        cause = native_error(MC_REGISTRY_INVALID_ARGUMENT, "Fixed Biome sampler memory owner");
        return finish(error, &cause, MC_BIOME_INVALID_ARGUMENT, -1, NULL, 0);
    }
    if (width > INT32_MAX || height > INT32_MAX ||
        (height != 0 && width > (size_t)INT32_MAX / height)) {
        cause = native_error(MC_REGISTRY_LIMIT, "Fixed Biome sampler rectangular extent");
        return finish(error, &cause, MC_BIOME_LIMIT, -1, NULL, 0);
    }
    const size_t count = width * height;
    if (count > SIZE_MAX / sizeof(double)) {
        cause = native_error(MC_REGISTRY_LIMIT, "Fixed Biome sampler double span");
        return finish(error, &cause, MC_BIOME_LIMIT, -1, NULL, 0);
    }

    ++classes->entered;
    int32_t pc = 19;
    size_t completed = 0;
    mc_biome_status climate_status = MC_BIOME_OK;
    mc_biome_reference_array *array = input;
    if (mc_registry_biome_array_prepare(classes, input, count, &array) != MC_REGISTRY_OK) {
        cause = classes->last_error;
        goto leave;
    }
    if (storage->a.data == NULL || storage->a.length < count) {
        pc = 49;
        climate_status = fresh_buffer(&storage->memory, &storage->a, count);
        if (climate_status != MC_BIOME_OK)
            goto climate_failed;
        pc = 60;
        climate_status = fresh_buffer(&storage->memory, &storage->b, count);
        if (climate_status != MC_BIOME_OK)
            goto climate_failed;
    }
    pc = 76;
    if (mc_registry_biome_array_fill(classes, array, count, storage->e) != MC_REGISTRY_OK) {
        cause = classes->last_error;
        goto leave;
    }
    completed = count;
    pc = 93;
    cause = fill_double(&storage->b, count, storage->g);
    if (cause.status != MC_REGISTRY_OK)
        goto leave;
    pc = 110;
    cause = fill_double(&storage->a, count, storage->f);
    if (cause.status != MC_REGISTRY_OK)
        goto leave;
    pc = 114;
    *out = array;
    goto leave;

climate_failed:
    cause = native_error(climate_result(climate_status), mc_biome_status_string(climate_status));
leave:
    --classes->entered;
    classes->last_error = cause;
    return finish(error, &cause, climate_status, pc, array, completed);
}
