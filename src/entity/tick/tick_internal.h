#ifndef MC_ENTITY_TICK_INTERNAL_H
#define MC_ENTITY_TICK_INTERNAL_H
#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../player/player_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/tick/environment.h"
#include <float.h>
#include <string.h>
#if !defined(__SSE2__) && !defined(_M_X64)
#error "Entity tick requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity tick requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Entity tick requires binary32/binary64 evaluation");
bool mc_tick_enter(mc_entity *, mc_entity_tick_error *);
bool mc_tick_fail(mc_entity_tick_error *, mc_entity_status, const char *, unsigned int);
bool mc_tick_fp(mc_entity_tick_error *, unsigned int);
bool mc_tick_effect(mc_entity_tick_error *, mc_entity_status, const char *, unsigned int);
bool mc_tick_world(mc_entity_tick_error *, mc_world_storage_status, const char *, unsigned int);
bool mc_tick_registry(mc_entity_tick_error *, mc_registry_status, const char *, unsigned int);
bool mc_tick_code(mc_entity *, mc_registry_classes *, mc_entity_tick_error *, unsigned int);
mc_entity_status mc_tick_finish(mc_entity_tick_error *, mc_entity_tick_error *);
bool mc_tick_flag(mc_entity *, int32_t, bool, mc_entity_tick_error *);
bool mc_tick_suffocating(mc_entity *, bool *, mc_entity_tick_error *);
/* Held canonical Item caller only; no public Player admission is widened. */
bool mc_tick_item_base_body(mc_entity *, mc_registry_classes *, const mc_entity_tick_limits *,
                            mc_entity_tick_error *);

static inline float mc_tick_fadd(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float mc_tick_fsub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float mc_tick_fmul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float mc_tick_i2f(int32_t a) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), a));
}

static inline double mc_tick_widen(float a) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(a)));
}

static inline double mc_tick_dadd(double a, double b) {
    uint64_t left, right;
    memcpy(&left, &a, sizeof(left));
    memcpy(&right, &b, sizeof(right));
    if ((left & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (right & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        right |= UINT64_C(0x0008000000000000);
        memcpy(&b, &right, sizeof(b));
        return b;
    }
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double mc_tick_dsub(double a, double b) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double mc_tick_dmul(double a, double b) {
    return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(a), _mm_set_sd(b)));
}
#endif
