#include "position_internal.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64)
#error "Entity position requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity position requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Entity position requires binary32/binary64 evaluation");

static double widen(float value) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(value)));
}

static double sub(double left, double right) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(left), _mm_set_sd(right)));
}

static double add(double left, double right) {
    uint64_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    if ((a & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (b & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        b |= UINT64_C(0x0008000000000000);
        memcpy(&right, &b, sizeof(right));
        return right;
    }
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(left), _mm_set_sd(right)));
}

mc_entity_bounds mc_entity_source_position_values(const mc_entity *p, double x, double y, double z,
                                                  float half_width, float height) {
    mc_entity_bounds value;
    value.min_x = sub(x, widen(half_width));
    value.min_y = add(sub(y, widen(p->base.offset)), widen(p->base.y_size));
    value.min_z = sub(z, widen(half_width));
    value.max_x = add(x, widen(half_width));
    value.max_y = add(add(sub(y, widen(p->base.offset)), widen(p->base.y_size)), widen(height));
    value.max_z = add(z, widen(half_width));
    return value;
}

mc_entity_status mc_entity_source_position_checked(mc_entity *entity, double x, double y, double z,
                                                   mc_entity_source_position_error *out) {
    mc_entity_source_position_error error = {0};
    if (entity == NULL) {
        error.status = MC_ENTITY_INVALID_ARGUMENT;
        error.operation = "SN.e native receiver";
        error.native_policy = true;
        goto done;
    }
    entity->base.x = x;
    entity->base.y = y;
    entity->base.z = z;
    error.bytecode = 21;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        error.status = MC_ENTITY_UNSUPPORTED_FP;
        error.operation = "SN.e reached position floating environment";
        error.native_policy = true;
        goto done;
    }
    const float half_width =
        _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(entity->base.width), _mm_set_ss(2.0f)));
    const float height = entity->base.height;
    mc_entity_bounds *receiver = entity->bounds; /* Actual aW read31, before arguments. */
    const mc_entity_bounds value =
        mc_entity_source_position_values(entity, x, y, z, half_width, height);
    error.bytecode = 86;
    if (receiver == NULL) {
        error.status = MC_ENTITY_STATE_ERROR;
        error.operation = "SN.e null permanent bounds receiver";
        error.source_null = true;
        goto done;
    }
    /* Actual eq.c assigns its same receiver's six fields in this order. */
    receiver->min_x = value.min_x;
    receiver->min_y = value.min_y;
    receiver->min_z = value.min_z;
    receiver->max_x = value.max_x;
    receiver->max_y = value.max_y;
    receiver->max_z = value.max_z;
    error.bytecode = 90;
done:
    if (out != NULL)
        *out = error;
    return error.status;
}

void mc_entity_source_size(mc_entity *entity, float width, float height) {
    entity->base.width = width;
    entity->base.height = height;
}
