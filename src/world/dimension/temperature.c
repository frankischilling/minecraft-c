#include "mc/world/dimension/temperature.h"

#include "../biome/biome_internal.h"
#include "mc/world/noise/simplex.h"

#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__x86_64__) && !defined(_M_X64) && !defined(_M_AMD64)
#error "GAME temperature requires the captured x86-64 floating profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "GAME temperature requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && sizeof(float) == 4 && sizeof(double) == 8 &&
                   FLT_EVAL_METHOD == 0,
               "GAME temperature requires source-width binary32/binary64");

struct mc_temperature_owner {
    mc_biome_memory buffers;
    mc_temperature_limits limits;
    size_t holds, arrays, direct_bytes;
};

struct mc_temperature_array {
    mc_temperature_owner *owner;
    mc_biome_double_buffer buffer;
    size_t holds;
};

static mc_biome_status finish(mc_temperature_error *out, const mc_temperature_error *frame) {
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

static mc_biome_status fail(mc_temperature_error *frame, mc_biome_status status,
                            const char *operation) {
    frame->status = status;
    frame->operation = operation;
    frame->native_boundary = true;
    return status;
}

static mc_biome_status dimension_failure(mc_temperature_error *frame,
                                         mc_world_dimension_status status, const char *operation) {
    frame->manager_status = status;
    return fail(frame,
                status == MC_WORLD_DIMENSION_LIMIT               ? MC_BIOME_LIMIT
                : status == MC_WORLD_DIMENSION_ALLOCATION_FAILED ? MC_BIOME_ALLOCATION_FAILED
                                                                 : MC_BIOME_INVALID_ARGUMENT,
                operation);
}

void mc_temperature_limits_default(mc_temperature_limits *out) {
    if (out != NULL)
        *out = (mc_temperature_limits){1024u * 1024u, MC_BIOME_DEFAULT_ALLOCATION_BYTES};
}

mc_biome_status mc_temperature_owner_create(const mc_temperature_limits *limits,
                                            mc_temperature_owner **out,
                                            mc_temperature_error *error) {
    mc_temperature_error frame = {0};
    if (out == NULL) {
        (void)fail(&frame, MC_BIOME_INVALID_ARGUMENT, "temperature owner output");
        return finish(error, &frame);
    }
    mc_temperature_limits admitted;
    mc_temperature_limits_default(&admitted);
    if (limits != NULL)
        admitted = *limits;
    if (admitted.max_direct_allocation_bytes < sizeof(mc_temperature_owner)) {
        (void)fail(&frame, MC_BIOME_LIMIT, "temperature owner direct request");
        return finish(error, &frame);
    }
    mc_temperature_owner *owner = calloc(1u, sizeof(*owner));
    if (owner == NULL) {
        (void)fail(&frame, MC_BIOME_ALLOCATION_FAILED, "temperature owner allocation");
        return finish(error, &frame);
    }
    owner->limits = admitted;
    owner->holds = 1u;
    owner->direct_bytes = sizeof(*owner);
    mc_biome_status status =
        mc_biome_memory_init(&owner->buffers, admitted.max_buffer_allocation_bytes);
    if (status != MC_BIOME_OK) {
        free(owner);
        (void)fail(&frame, status, "temperature buffer ledger initialization");
        return finish(error, &frame);
    }
    *out = owner;
    return finish(error, &frame);
}

mc_biome_status mc_temperature_owner_retain(mc_temperature_owner *owner) {
    if (owner == NULL)
        return MC_BIOME_INVALID_ARGUMENT;
    if (owner->holds == SIZE_MAX)
        return MC_BIOME_LIMIT;
    ++owner->holds;
    return MC_BIOME_OK;
}

void mc_temperature_owner_release(mc_temperature_owner *owner) {
    if (owner != NULL && --owner->holds == 0u)
        free(owner);
}

mc_biome_status mc_temperature_owner_get(const mc_temperature_owner *owner,
                                         mc_temperature_owner_state *out) {
    if (owner == NULL || out == NULL)
        return MC_BIOME_INVALID_ARGUMENT;
    *out = (mc_temperature_owner_state){owner->holds, owner->arrays, owner->direct_bytes,
                                        owner->buffers.live_allocation_bytes, owner->limits};
    return MC_BIOME_OK;
}

mc_biome_status mc_temperature_array_retain(mc_temperature_array *array) {
    if (array == NULL)
        return MC_BIOME_INVALID_ARGUMENT;
    if (array->holds == SIZE_MAX)
        return MC_BIOME_LIMIT;
    ++array->holds;
    return MC_BIOME_OK;
}

void mc_temperature_array_release(mc_temperature_array *array) {
    if (array == NULL || --array->holds != 0u)
        return;
    mc_temperature_owner *owner = array->owner;
    mc_biome_double_buffer_free(&array->buffer);
    --owner->arrays;
    owner->direct_bytes -= sizeof(*array);
    free(array);
    mc_temperature_owner_release(owner);
}

mc_biome_status mc_temperature_array_get(const mc_temperature_array *array,
                                         mc_temperature_array_view *out) {
    if (array == NULL || out == NULL)
        return MC_BIOME_INVALID_ARGUMENT;
    *out = (mc_temperature_array_view){array->owner, array->buffer.data, array->buffer.length,
                                       array->holds};
    return MC_BIOME_OK;
}

/* Each genuine handle owns exactly one buffer-descriptor reference and one
 * ledger hold. Additional array aliases only retain the SAME native handle.
 * No manager allocation layout, magic header or detached double[] is copied. */
static mc_biome_status array_create(mc_temperature_owner *owner, size_t length,
                                    mc_temperature_array **out, mc_temperature_error *frame) {
    if (owner == NULL)
        return fail(frame, MC_BIOME_INVALID_ARGUMENT, "temperature reached output allocator");
    if (owner->direct_bytes > owner->limits.max_direct_allocation_bytes ||
        sizeof(mc_temperature_array) >
            owner->limits.max_direct_allocation_bytes - owner->direct_bytes ||
        owner->arrays == SIZE_MAX || owner->holds == SIZE_MAX)
        return fail(frame, MC_BIOME_LIMIT, "temperature array handle direct request");
    mc_temperature_array *array = calloc(1u, sizeof(*array));
    if (array == NULL)
        return fail(frame, MC_BIOME_ALLOCATION_FAILED, "temperature array handle allocation");
    owner->direct_bytes += sizeof(*array);
    mc_biome_status status = mc_biome_double_buffer_resize(&owner->buffers, &array->buffer, length);
    if (status != MC_BIOME_OK) {
        owner->direct_bytes -= sizeof(*array);
        free(array);
        return fail(frame, status, "temperature array buffer request");
    }
    /* All count admission preceded allocation. No failure remains here. */
    ++owner->holds;
    ++owner->arrays;
    array->owner = owner;
    array->holds = 1u;
    *out = array;
    return MC_BIOME_OK;
}

static bool dimensions(size_t width, size_t height, size_t *count) {
    if (width > INT32_MAX || height > INT32_MAX ||
        (height != 0u && width > (size_t)INT32_MAX / height))
        return false;
    const size_t product = width * height;
    if (product > SIZE_MAX / sizeof(double))
        return false;
    *count = product;
    return true;
}

static bool strict_fp(void) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u;
}

static double add(double a, double b) {
    volatile double value = a + b;
    return value;
}

static double subtract(double a, double b) {
    volatile double value = a - b;
    return value;
}

static double multiply(double a, double b) {
    volatile double value = a * b;
    return value;
}

static bool raw_fill(mc_noise_simplex *layers, size_t layer_count, int32_t x, int32_t z,
                     size_t width, size_t height, double scale, double frequency, double *data,
                     size_t length) {
    const mc_noise_simplex_octaves noise = {layers, layer_count};
    const mc_noise_simplex_grid grid = {(double)x, (double)z, width, height, scale, scale};
    return mc_noise_simplex_octaves_fill(&noise, &grid, frequency,
                                         MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER, data, length);
}

/* Called ONLY after the completed raw temperature fill44. There is no early
 * manager.c allocation/span inspection in the enclosing entry. Capturing its
 * real allocation preserves old+new peak charges until publication77 returns.
 * This call has no callbacks; fields remain exclusive throughout. */
static mc_biome_status auxiliary(mc_biome_manager *manager, int32_t x, int32_t z, size_t width,
                                 size_t height, size_t count, mc_temperature_error *frame) {
    mc_biome_double_reference old = {0};
    mc_biome_double_buffer fresh = {0};
    frame->bytecode = 74u;
    mc_biome_status status = mc_biome_double_capture(&manager->c, &old);
    if (status != MC_BIOME_OK)
        return fail(frame, status, "temperature auxiliary array native hold");
    double *data = (double *)(void *)old.data;
    size_t length = old.length;
    if (data == NULL || length < count) {
        frame->raw_bytecode = 35u;
        status = mc_biome_double_buffer_resize(manager->memory, &fresh, count);
        if (status != MC_BIOME_OK) {
            (void)fail(frame, status, "temperature auxiliary buffer request");
            goto leave;
        }
        data = fresh.data;
        length = fresh.length;
    }
    frame->raw_bytecode = 0u;
    if (!raw_fill(manager->auxiliary_layers, 2u, x, z, width, height, 0.25, 0.5882352941176471,
                  data, length)) {
        status = fail(frame, MC_BIOME_INVALID_ARGUMENT, "temperature auxiliary raw fill");
        goto leave;
    }
    frame->bytecode = 77u;
    if (fresh.data != NULL) {
        mc_biome_double_buffer_free(&manager->c);
        manager->c = fresh; /* MOVE to the actual field, never a duplicate owner. */
        fresh = (mc_biome_double_buffer){0};
    }
    frame->auxiliary_published = true;
leave:
    mc_biome_double_buffer_free(&fresh);
    mc_biome_double_release(&old);
    return status;
}

mc_biome_status mc_temperature_sample(mc_world_dimension_biome *receiver,
                                      mc_temperature_owner *allocation_owner,
                                      mc_temperature_array *input, int32_t x, int32_t z,
                                      size_t width, size_t height, mc_temperature_array **out,
                                      mc_temperature_error *error) {
    mc_temperature_error frame = {0};
    mc_temperature_array *input_hold = NULL;
    mc_temperature_array *fresh = NULL;
    bool manager_held = false;
    size_t count = 0u;
    if (out == NULL) {
        (void)fail(&frame, MC_BIOME_INVALID_ARGUMENT, "temperature result output");
        goto leave;
    }
    /* Native finite-domain/count admission precedes source effects. It does
     * not inspect any a/b/c/d payload or require a future output allocator. */
    if (!dimensions(width, height, &count)) {
        (void)fail(&frame, MC_BIOME_LIMIT, "temperature native rectangular extent");
        goto leave;
    }
    mc_world_dimension_status manager_status = mc_world_dimension_biome_retain(receiver);
    if (manager_status != MC_WORLD_DIMENSION_OK) {
        (void)dimension_failure(&frame, manager_status, "temperature receiver native hold");
        goto leave;
    }
    manager_held = true;
    if (input != NULL) {
        mc_biome_status status = mc_temperature_array_retain(input);
        if (status != MC_BIOME_OK) {
            (void)fail(&frame, status, "temperature input native hold");
            goto leave;
        }
        input_hold = input;
    }
    mc_world_dimension_biome_fields fields;
    manager_status = mc_world_dimension_biome_fields_get(receiver, &fields);
    if (manager_status != MC_WORLD_DIMENSION_OK) {
        (void)dimension_failure(&frame, manager_status, "temperature receiver native fields");
        goto leave;
    }
    frame.body = fields.fixed ? MC_TEMPERATURE_BODY_FIXED : MC_TEMPERATURE_BODY_NORMAL;
    mc_temperature_array *target = input_hold;
    if (target == NULL || target->buffer.length < count) {
        frame.bytecode = 19u;
        if (array_create(allocation_owner, count, &fresh, &frame) != MC_BIOME_OK)
            goto leave;
        target = fresh;
    } else {
        frame.reused_input = true;
    }

    if (fields.fixed) {
        /* Fresh actual f read at30, AFTER output allocation19. Never substitute
         * the earlier passive dispatch snapshot or normal temperature noise. */
        frame.bytecode = 30u;
        manager_status = mc_world_dimension_biome_fields_get(receiver, &fields);
        if (manager_status != MC_WORLD_DIMENSION_OK) {
            (void)dimension_failure(&frame, manager_status, "fixed temperature f receiver");
            goto leave;
        }
        double value;
        memcpy(&value, &fields.fixed_f, sizeof(value));
        frame.bytecode = 33u;
        for (size_t i = 0u; i < count; ++i)
            memcpy(&target->buffer.data[i], &value, sizeof(value));
        frame.cells_completed = count;
    } else {
        mc_biome_manager *manager = fields.normal;
        frame.bytecode = 44u;
        /* The opaque Dimension owner supplies actual constructed embedded
         * noise. Do NOT use sampling_valid(), which preflights future c/a/b/d. */
        if (manager == NULL || manager->initialized != 1) {
            (void)fail(&frame, MC_BIOME_INVALID_ARGUMENT,
                       "normal temperature initialized native noise owner");
            goto leave;
        }
        if (!strict_fp()) {
            (void)fail(&frame, MC_BIOME_INVALID_ARGUMENT,
                       "normal temperature strict floating environment");
            goto leave;
        }
        if (!raw_fill(manager->temperature_layers, 4u, x, z, width, height, (double)0.025f, 0.25,
                      target->buffer.data, target->buffer.length)) {
            (void)fail(&frame, MC_BIOME_INVALID_ARGUMENT, "temperature raw fill");
            goto leave;
        }
        frame.temperature_filled = true;
        if (auxiliary(manager, x, z, width, height, count, &frame) != MC_BIOME_OK)
            goto leave;

        /* Exact source association/rounding. This pure call has no callbacks;
         * its actual manager.c field cannot change between these cell reads.
         * The caller's later World query MUST independently reload provider.w. */
        for (size_t i = 0u; i < count; ++i) {
            frame.bytecode = 109u;
            const double auxiliary_value = add(multiply(manager->c.data[i], 1.1), 0.5);
            const double weight = 0.01;
            const double complement = subtract(1.0, weight);
            frame.bytecode = 134u;
            const double raw_value = target->buffer.data[i];
            const double temperature_part =
                multiply(add(multiply(raw_value, 0.15), 0.7), complement);
            const double mixed = add(temperature_part, multiply(auxiliary_value, weight));
            const double first_difference = subtract(1.0, mixed);
            const double second_difference = subtract(1.0, mixed);
            double processed = subtract(1.0, multiply(first_difference, second_difference));
            if (processed < 0.0)
                processed = 0.0;
            if (processed > 1.0)
                processed = 1.0;
            frame.bytecode = 192u;
            target->buffer.data[i] = processed;
            ++frame.cells_completed;
        }
    }

    /* Return one already-owned reference: no hold/allocator operation can fail
     * after the fills. The source provider field has NOT been changed here. */
    *out = target;
    if (fresh != NULL)
        fresh = NULL;
    else
        input_hold = NULL;
    frame = (mc_temperature_error){0};
leave:
    mc_temperature_array_release(fresh);
    mc_temperature_array_release(input_hold);
    if (manager_held)
        mc_world_dimension_biome_release(receiver);
    return finish(error, &frame);
}
