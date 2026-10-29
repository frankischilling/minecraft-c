#include "mc/world/dimension/sky.h"

#include "../../entity/player/tick_math_internal.h"
#include "world_provider_internal.h"

#include <string.h>

typedef struct sunrise_call {
    mc_world_provider *dimension;
    mc_math_classes *math;
    bool dimension_held, math_held;
    mc_world_provider_sky_error error;
} sunrise_call;

static bool failed(sunrise_call *call, mc_world_provider_status status, const char *operation,
                   unsigned int pc, bool native) {
    call->error.status = status;
    call->error.operation = operation;
    call->error.bytecode = pc;
    call->error.native_policy = native;
    call->error.cause =
        (mc_world_provider_error){.status = status, .operation = operation, .bytecode = pc};
    return false;
}

static mc_world_provider_status finish(sunrise_call *call, mc_world_provider_sky_error *error) {
    if (call->math_held)
        (void)mc_math_classes_release(&call->math, NULL);
    if (call->dimension_held)
        mc_world_provider_release(call->dimension);
    if (error != NULL)
        *error = call->error;
    return call->error.status;
}

static bool begin(sunrise_call *call, mc_world_provider *dimension, const void *out,
                  mc_world_provider_kind *kind) {
    memset(call, 0, sizeof(*call));
    call->dimension = dimension;
    call->error.source_class = "xa";
    call->error.math.source_pc = -1;
    if (out == NULL)
        return failed(call, MC_WORLD_PROVIDER_INVALID_ARGUMENT, "Dimension sky output", 0, true);
    if (dimension == NULL)
        return failed(call, MC_WORLD_PROVIDER_INVALID_ARGUMENT,
                      "actual Dimension sky null receiver", 0, false);
    mc_world_provider_state state;
    const mc_world_provider_status status = mc_world_provider_state_get(dimension, &state);
    if (status != MC_WORLD_PROVIDER_OK)
        return failed(call, status, "Dimension sky actual owner", 0, true);
    *kind = state.kind;
    if (state.kind == MC_WORLD_PROVIDER_SKY)
        call->error.source_class = "ay";
    return true;
}

static bool math_ready(sunrise_call *call, mc_math_classes *math) {
    call->math = math;
    mc_math_class_status status = mc_math_classes_retain(math, &call->error.math);
    if (status != MC_MATH_CLASS_OK)
        return failed(call, MC_WORLD_PROVIDER_MATH_FAILED, call->error.math.operation, 9, true);
    call->math_held = true;
    status = mc_math_classes_ensure(math, &call->error.math);
    if (status != MC_MATH_CLASS_OK)
        return failed(call, MC_WORLD_PROVIDER_MATH_FAILED, call->error.math.operation, 9, true);
    return true;
}

static bool store(sunrise_call *call, size_t index, float value, unsigned int pc) {
    float *array = mc_world_provider_sunrise_storage(call->dimension);
    if (array == NULL)
        return failed(call, MC_WORLD_PROVIDER_INVALID_ARGUMENT,
                      "actual Dimension sunrise null h array", pc, false);
    array[index] = value;
    ++call->error.stores;
    return true;
}

mc_world_provider_status mc_world_provider_sunrise_color(mc_world_provider *dimension, float angle,
                                                         float partial, mc_math_classes *math,
                                                         const float **out,
                                                         mc_world_provider_sky_error *error) {
    (void)partial;
    sunrise_call call;
    mc_world_provider_kind kind;
    if (!begin(&call, dimension, out, &kind))
        return finish(&call, error);
    if (kind == MC_WORLD_PROVIDER_SKY) {
        *out = NULL;
        call.error.bytecode = 1;
        call.error.source_returned = true;
        return finish(&call, error);
    }
    mc_world_provider_status status = mc_world_provider_retain(dimension);
    if (status != MC_WORLD_PROVIDER_OK) {
        failed(&call, status, "Dimension sunrise native occurrence", 0, true);
        goto done;
    }
    call.dimension_held = true;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        failed(&call, MC_WORLD_PROVIDER_UNSUPPORTED_FP, "Dimension sunrise strict FP", 6, true);
        goto done;
    }
    const float argument = at_fmul(at_fmul(angle, 3.1415927f), 2.0f);
    if (!math_ready(&call, math))
        goto done;
    const mc_math_trig *table = mc_math_classes_table(math);
    const float direction = at_fsub(mc_math_cos(table, argument), 0.0f);
    const float center = -0.0f, range = 0.4f;
    if (!(direction >= at_fsub(center, range) && direction <= at_fadd(center, range))) {
        *out = NULL;
        call.error.bytecode = 140;
        call.error.source_returned = true;
        goto done;
    }
    const float blend = at_fadd(at_fmul(at_fdiv(at_fsub(direction, center), range), 0.5f), 0.5f);
    float alpha = at_fsub(
        1.0f, at_fmul(at_fsub(1.0f, mc_math_sin(table, at_fmul(blend, 3.1415927f))), 0.99f));
    alpha = at_fmul(alpha, alpha);
    if (!store(&call, 0, at_fadd(at_fmul(blend, 0.3f), 0.7f), 92) ||
        !store(&call, 1, at_fadd(at_fmul(at_fmul(blend, blend), 0.7f), 0.2f), 109) ||
        !store(&call, 2, at_fadd(at_fmul(at_fmul(blend, blend), 0.0f), 0.2f), 125) ||
        !store(&call, 3, alpha, 133))
        goto done;
    *out = mc_world_provider_sunrise_storage(dimension);
    call.error.bytecode = 138;
    call.error.source_returned = true;
done:
    return finish(&call, error);
}

mc_world_provider_status mc_world_provider_horizon_tint(mc_world_provider *dimension, bool *out,
                                                        mc_world_provider_sky_error *error) {
    sunrise_call call;
    mc_world_provider_kind kind;
    if (!begin(&call, dimension, out, &kind))
        return finish(&call, error);
    *out = kind != MC_WORLD_PROVIDER_SKY;
    call.error.bytecode = 1;
    call.error.source_returned = true;
    return finish(&call, error);
}
