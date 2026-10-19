#include "mc/world/runtime/environment.h"

#include "mc/core/java/java.h"
#include "mc/world/dimension/dimension.h"
#include "runtime_internal.h"

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "World environment arithmetic requires x86-64/SSE2"
#endif
#include <emmintrin.h>

static mc_world_storage_status result(mc_world_storage_error *error, mc_world_storage_status status,
                                      const char *operation)
{
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status admit(const mc_world_runtime *runtime, mc_world_storage_error *error)
{
    if (runtime == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World environment arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World environment phase");
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World environment FP admission");
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_rain_strength(const mc_world_runtime *runtime,
                                                       float partial_tick, float *out,
                                                       mc_world_storage_error *error)
{
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World rain output");
    mc_world_storage_status status = admit(runtime, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    float previous = runtime->fields.i;
    float current = runtime->fields.j;
    volatile float difference = current - runtime->fields.i;
    difference = difference * partial_tick;
    difference = previous + difference;
    *out = difference;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_thunder_strength(const mc_world_runtime *runtime,
                                                          float partial_tick, float *out,
                                                          mc_world_storage_error *error)
{
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World thunder output");
    mc_world_storage_status status = admit(runtime, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    float previous = runtime->fields.k;
    float current = runtime->fields.l;
    volatile float difference = current - runtime->fields.k;
    difference = difference * partial_tick;
    difference = previous + difference;
    float rain;
    status = mc_world_runtime_rain_strength(runtime, partial_tick, &rain, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    difference = difference * rain;
    *out = difference;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_celestial_angle(const mc_world_runtime *runtime,
                                                         float partial_tick, float *out,
                                                         mc_world_storage_error *error)
{
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World angle output");
    mc_world_storage_status status = admit(runtime, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_world_dimension *dimension = runtime->fields.t;
    const mc_world_info_live *info = mc_world_info_live_get(runtime->fields.x);
    if (info == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World angle Info time");
    int64_t time = *info->time;
    mc_world_dimension_status angle =
        mc_world_dimension_celestial_angle(dimension, time, partial_tick, out, NULL);
    if (angle != MC_WORLD_DIMENSION_OK)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      mc_world_dimension_status_string(angle));
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_sky_subtraction(const mc_world_runtime *runtime,
                                                         float partial_tick, int32_t *out,
                                                         mc_world_storage_error *error)
{
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World sky output");
    float angle;
    mc_world_storage_status status =
        mc_world_runtime_celestial_angle(runtime, partial_tick, &angle, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    const mc_math_trig *trig = mc_world_runtime_class_trig(runtime);
    volatile float value = angle * 0x1.921fb6p+1f;
    value = value * 2.0f;
    value = mc_math_cos(trig, value);
    value = value * 2.0f;
    value = value + 0.5f;
    value = 1.0f - value;
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    value = 1.0f - value;
    volatile double left = (double)value;
    float rain;
    status = mc_world_runtime_rain_strength(runtime, partial_tick, &rain, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    volatile float strength = rain * 5.0f;
    volatile double factor = (double)strength;
    factor = factor / 16.0;
    factor = 1.0 - factor;
    left = left * factor;
    value = (float)left;
    left = (double)value;
    float thunder;
    status = mc_world_runtime_thunder_strength(runtime, partial_tick, &thunder, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    strength = thunder * 5.0f;
    factor = (double)strength;
    factor = factor / 16.0;
    factor = 1.0 - factor;
    left = left * factor;
    value = (float)left;
    value = 1.0f - value;
    value = value * 11.0f;
    *out = mc_java_float_to_i32(value);
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_initialize_environment(mc_world_runtime *runtime,
                                                                mc_world_storage_error *error)
{
    int32_t light;
    mc_world_storage_status status = mc_world_runtime_sky_subtraction(runtime, 1.0f, &light, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (runtime->fields.f != light)
        runtime->fields.f = light;
    bool raining;
    if (mc_world_info_raining_get(runtime->fields.x, &raining) != MC_WORLD_INFO_OK)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World initial rain Info");
    if (raining) {
        runtime->fields.j = 1.0f;
        bool thundering;
        if (mc_world_info_thundering_get(runtime->fields.x, &thundering) != MC_WORLD_INFO_OK)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World initial thunder Info");
        if (thundering)
            runtime->fields.l = 1.0f;
    }
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}
