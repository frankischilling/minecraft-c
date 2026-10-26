#ifndef MC_ENTITY_ACTION_INTERNAL_H
#define MC_ENTITY_ACTION_INTERNAL_H
#include "../../client/input/movement_input_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../player/player_internal.h"
#include "angle_internal.h"
#include "atan_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/action/action.h"
#include <float.h>
#include <math.h>
#include <string.h>
#if !defined(__SSE2__) && !defined(_M_X64)
#error "Player action requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Player action requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Player action requires binary32/binary64 evaluation");
bool mc_action_enter(mc_entity *, mc_player_action_error *);
bool mc_action_fail(mc_player_action_error *, mc_entity_status, const char *, unsigned int);
bool mc_action_fp(mc_player_action_error *, unsigned int);
bool mc_action_work(mc_player_action_error *, unsigned int);
mc_entity_status mc_action_finish(mc_player_action_error *, mc_player_action_error *);
bool mc_action_input(mc_entity *, mc_player_action_error *);
bool mc_action_push(mc_entity *, mc_entity *, mc_player_action_error *);
bool mc_action_pickup(mc_entity *, mc_entity *, mc_player_action_error *);
bool mc_action_collidable(mc_entity *, bool *, mc_player_action_error *);
bool mc_action_candidate(mc_entity *, mc_entity *, mc_player_action_error *, unsigned int);

static inline double ac_add(double a, double b) {
    uint64_t x, y;
    memcpy(&x, &a, 8);
    memcpy(&y, &b, 8);
    if ((x & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (y & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        y |= UINT64_C(0x0008000000000000);
        memcpy(&b, &y, 8);
        return b;
    }
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double ac_sub(double a, double b) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double ac_mul(double a, double b) {
    return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double ac_div(double a, double b) {
    return _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double ac_widen(float a) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(a)));
}

static inline float ac_narrow(double a) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(a)));
}

static inline float ac_fadd(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float ac_fsub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float ac_fmul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float ac_fdiv(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float ac_i2f(int32_t a) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), a));
}

static inline double ac_neg(double a) {
    uint64_t x;
    memcpy(&x, &a, 8);
    x ^= UINT64_C(0x8000000000000000);
    memcpy(&a, &x, 8);
    return a;
}
#endif
