#include "mc/world/biome/sampling.h"

#include "../../block/registry/registry_internal.h"
#include "biome_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

struct mc_biome_reference_array {
    mc_registry_classes *classes;
    size_t length;
    const mc_biome **elements;
    struct mc_biome_reference_array *next;
};

_Static_assert(sizeof(mc_biome_reference_array) % _Alignof(const mc_biome *) == 0,
               "reference array tail alignment");

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

static mc_registry_status finish(mc_biome_sampling_error *error, const mc_registry_error *registry,
                                 mc_biome_status climate_status, int32_t pc,
                                 const mc_biome_reference_array *array, size_t completed,
                                 size_t next_index)
{
    if (error != NULL)
        *error = (mc_biome_sampling_error){.registry = *registry,
                                           .climate_status = climate_status,
                                           .sampler_pc = pc,
                                           .array = array,
                                           .cells_completed = completed,
                                           .next_index = next_index};
    return registry->status;
}

static bool owns_array(const mc_registry_classes *classes, const mc_biome_reference_array *array)
{
    for (const mc_biome_reference_array *owned = classes->biomes.sample_arrays; owned != NULL;
         owned = owned->next)
        if (owned == array)
            return true;
    return false;
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

mc_registry_status mc_biome_sample_references(mc_registry_classes *classes,
                                              mc_biome_manager *manager, int32_t x, int32_t z,
                                              size_t size, mc_biome_reference_array *input,
                                              mc_biome_reference_array **out,
                                              mc_biome_sampling_error *error)
{
    mc_registry_error cause = native_error(MC_REGISTRY_OK, NULL);
    if (classes == NULL || out == NULL) {
        cause = native_error(MC_REGISTRY_INVALID_ARGUMENT, "Biome sampler owner/output");
        return finish(error, &cause, MC_BIOME_OK, -1, NULL, 0, 0);
    }
    if (classes->entered != 0) {
        cause = native_error(MC_REGISTRY_BUSY, "Biome sampler registry entered");
        return finish(error, &cause, MC_BIOME_OK, -1, NULL, 0, 0);
    }
    if (!mc_biome_manager_sampling_valid(manager) ||
        (input != NULL && !owns_array(classes, input))) {
        cause = native_error(MC_REGISTRY_INVALID_ARGUMENT, "Biome sampler manager/input owner");
        return finish(error, &cause, MC_BIOME_INVALID_ARGUMENT, -1, NULL, 0, 0);
    }
    if (size > INT32_MAX || (size != 0 && size > (size_t)INT32_MAX / size)) {
        cause = native_error(MC_REGISTRY_LIMIT, "Biome sampler square extent");
        return finish(error, &cause, MC_BIOME_LIMIT, -1, NULL, 0, 0);
    }
    const size_t count = size * size;
    if (count > (SIZE_MAX - sizeof(mc_biome_reference_array)) / sizeof(const mc_biome *) ||
        count > SIZE_MAX / sizeof(double)) {
        cause = native_error(MC_REGISTRY_LIMIT, "Biome sampler native span");
        return finish(error, &cause, MC_BIOME_LIMIT, -1, NULL, 0, 0);
    }

    ++classes->entered;
    mc_biome_reference_array *array = input;
    int32_t pc = 19;
    size_t completed = 0, next_index = 0;
    mc_biome_status climate_status = MC_BIOME_OK;
    if (array == NULL || array->length < count) {
        mc_biome_reference_array *replacement =
            mc_registry_allocate(classes, sizeof(*replacement) + count * sizeof(const mc_biome *));
        if (replacement == NULL) {
            /* This ordinary allocation has no source initializer/constructor
             * frame. Do not attach a stale successful Block position to it. */
            cause = native_error(classes->last_error.status, classes->last_error.message);
            goto leave;
        }
        replacement->classes = classes;
        replacement->length = count;
        replacement->elements = (const mc_biome **)(replacement + 1);
        replacement->next = classes->biomes.sample_arrays;
        classes->biomes.sample_arrays = replacement;
        array = replacement;
    }

    pc = 49;
    climate_status =
        mc_biome_sample_channel_direct(manager, x, z, size, MC_BIOME_CONSTRUCT_TEMPERATURE);
    if (climate_status != MC_BIOME_OK)
        goto climate_failed;
    pc = 81;
    climate_status =
        mc_biome_sample_channel_direct(manager, x, z, size, MC_BIOME_CONSTRUCT_RAINFALL);
    if (climate_status != MC_BIOME_OK)
        goto climate_failed;
    pc = 113;
    climate_status =
        mc_biome_sample_channel_direct(manager, x, z, size, MC_BIOME_CONSTRUCT_AUXILIARY);
    if (climate_status != MC_BIOME_OK)
        goto climate_failed;

    while (completed < count) {
        mc_biome_sample_process_cell(manager, completed);
        mc_biome_reference_array *target = array;
        const size_t target_index = next_index++;
        const double temperature = manager->a.data[completed];
        const double rainfall = manager->b.data[completed];
        const mc_biome *biome = NULL;
        pc = 314;
        const mc_registry_status status =
            mc_registry_biome_lookup(classes, temperature, rainfall, &biome);
        if (status != MC_REGISTRY_OK) {
            cause = classes->last_error;
            cause.status = status;
            goto leave;
        }
        target->elements[target_index] = biome;
        ++completed;
    }
    pc = 331;
    *out = array;
    goto leave;

climate_failed:
    cause = native_error(climate_result(climate_status), mc_biome_status_string(climate_status));
leave:
    --classes->entered;
    classes->last_error = cause;
    return finish(error, &cause, climate_status, pc, array, completed, next_index);
}

size_t mc_biome_reference_array_length(const mc_biome_reference_array *array)
{
    return array == NULL ? 0 : array->length;
}

const mc_biome *const *mc_biome_reference_array_data(const mc_biome_reference_array *array)
{
    return array == NULL ? NULL : array->elements;
}
