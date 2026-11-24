#ifndef MC_CLIENT_RENDER_INTERPOLATION_INTERNAL_H
#define MC_CLIENT_RENDER_INTERPOLATION_INTERNAL_H

#include "../../entity/player/tick_math_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum mc_client_interpolation_status {
    MC_CLIENT_INTERPOLATION_OK = 0,
    MC_CLIENT_INTERPOLATION_UNSUPPORTED_FP,
    MC_CLIENT_INTERPOLATION_UNSUPPORTED_NUMERIC
} mc_client_interpolation_status;

typedef struct mc_client_interpolation_error {
    mc_client_interpolation_status status;
    int32_t pc;
} mc_client_interpolation_error;

static inline bool mc_client_interpolation_fail(mc_client_interpolation_error *error,
                                                mc_client_interpolation_status status, int32_t pc) {
    error->status = status;
    error->pc = pc;
    return false;
}

static inline bool mc_client_interpolation_fp(mc_client_interpolation_error *error, int32_t pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    return mc_client_interpolation_fail(error, MC_CLIENT_INTERPOLATION_UNSUPPORTED_FP, pc);
}

/* Measured LL cases13/14 preserve the current operand's payload through SUB,
 * MUL and ADD; cases17..19 preserve/quiet the partial payload and widen it.
 * OpenJDK17u tag jdk-17.0.17+10, templateTable_x86.cpp fop2/dop2, keeps the
 * right source operand as SSE destination for ADD/MUL, left for SUB. Explicit
 * selection prevents a compiler commuting the intrinsic's NaN operands.
 * https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/hotspot/cpu/x86/templateTable_x86.cpp
 * No Java/JDK body is copied here. Floating exception flags are not a value
 * parity claim; controls are checked and no MXCSR bit is repaired or reset. */
static inline bool mc_client_interpolation_float_nan_admit(float left, float right, int32_t pc,
                                                           mc_client_interpolation_error *error) {
    uint32_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    if ((a & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000) &&
        (b & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000) && ((a & b) & UINT32_C(0x00400000)) == 0)
        return mc_client_interpolation_fail(error, MC_CLIENT_INTERPOLATION_UNSUPPORTED_NUMERIC, pc);
    return true;
}

static inline bool mc_client_interpolation_double_nan_admit(double left, double right, int32_t pc,
                                                            mc_client_interpolation_error *error) {
    uint64_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    if ((a & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (b & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        ((a & b) & UINT64_C(0x0008000000000000)) == 0)
        return mc_client_interpolation_fail(error, MC_CLIENT_INTERPOLATION_UNSUPPORTED_NUMERIC, pc);
    return true;
}

static inline bool mc_client_interpolation_nan_float(float left, float right, bool prefer_right,
                                                     float *out) {
    uint32_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    const bool an = (a & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    const bool bn = (b & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    if (!an && !bn)
        return false;
    uint32_t result = (prefer_right ? (bn ? b : a) : (an ? a : b)) | UINT32_C(0x00400000);
    memcpy(out, &result, sizeof(result));
    return true;
}

static inline bool mc_client_interpolation_nan_double(double left, double right, bool prefer_right,
                                                      double *out) {
    uint64_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    const bool an = (a & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000);
    const bool bn = (b & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000);
    if (!an && !bn)
        return false;
    uint64_t result = (prefer_right ? (bn ? b : a) : (an ? a : b)) | UINT64_C(0x0008000000000000);
    memcpy(out, &result, sizeof(result));
    return true;
}

static inline float mc_client_interpolation_fsub(float a, float b) {
    float result;
    return mc_client_interpolation_nan_float(a, b, false, &result) ? result : at_fsub(a, b);
}

static inline float mc_client_interpolation_fmul(float a, float b) {
    float result;
    return mc_client_interpolation_nan_float(a, b, true, &result) ? result : at_fmul(a, b);
}

static inline float mc_client_interpolation_fadd(float a, float b) {
    float result;
    return mc_client_interpolation_nan_float(a, b, true, &result) ? result : at_fadd(a, b);
}

static inline double mc_client_interpolation_dsub(double a, double b) {
    double result;
    return mc_client_interpolation_nan_double(a, b, false, &result) ? result : at_sub(a, b);
}

static inline double mc_client_interpolation_dmul(double a, double b) {
    double result;
    return mc_client_interpolation_nan_double(a, b, true, &result) ? result : at_mul(a, b);
}

static inline double mc_client_interpolation_dadd(double a, double b) {
    double result;
    return mc_client_interpolation_nan_double(a, b, true, &result) ? result : at_add(a, b);
}

static inline double mc_client_interpolation_widen(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    if ((bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000)) {
        const uint64_t widened = ((uint64_t)(bits & UINT32_C(0x80000000)) << 32) |
                                 UINT64_C(0x7ff8000000000000) |
                                 ((uint64_t)(bits & UINT32_C(0x007fffff)) << 29);
        double result;
        memcpy(&result, &widened, sizeof(result));
        return result;
    }
    return at_widen(value);
}

/* Private synchronous helpers. out/error are nonnull and disjoint. Failure
 * preserves out and reports the caller's exact failing operation PC; success
 * leaves error at OK/-1. Each operation rounds at the actual JVM width,
 * including endpoint partials, subnormal values and intermediate overflow.
 * No algebraic shortcut, FMA, new arithmetic domain or control-state reset. */
static inline bool mc_client_interpolate_float(float previous, float current, float partial,
                                               int32_t sub_pc, int32_t mul_pc, int32_t add_pc,
                                               float *out, mc_client_interpolation_error *error) {
    *error = (mc_client_interpolation_error){MC_CLIENT_INTERPOLATION_OK, -1};
    if (!mc_client_interpolation_fp(error, sub_pc) ||
        !mc_client_interpolation_float_nan_admit(current, previous, sub_pc, error))
        return false;
    const float difference = mc_client_interpolation_fsub(current, previous);
    if (!mc_client_interpolation_fp(error, mul_pc) ||
        !mc_client_interpolation_float_nan_admit(difference, partial, mul_pc, error))
        return false;
    const float scaled = mc_client_interpolation_fmul(difference, partial);
    if (!mc_client_interpolation_fp(error, add_pc) ||
        !mc_client_interpolation_float_nan_admit(previous, scaled, add_pc, error))
        return false;
    const float value = mc_client_interpolation_fadd(previous, scaled);
    *out = value;
    return true;
}

static inline bool mc_client_interpolate_double(double previous, double current, float partial,
                                                int32_t sub_pc, int32_t widen_pc, int32_t mul_pc,
                                                int32_t add_pc, double *out,
                                                mc_client_interpolation_error *error) {
    *error = (mc_client_interpolation_error){MC_CLIENT_INTERPOLATION_OK, -1};
    if (!mc_client_interpolation_fp(error, sub_pc) ||
        !mc_client_interpolation_double_nan_admit(current, previous, sub_pc, error))
        return false;
    const double difference = mc_client_interpolation_dsub(current, previous);
    if (!mc_client_interpolation_fp(error, widen_pc))
        return false;
    const double widened = mc_client_interpolation_widen(partial);
    if (!mc_client_interpolation_fp(error, mul_pc) ||
        !mc_client_interpolation_double_nan_admit(difference, widened, mul_pc, error))
        return false;
    const double scaled = mc_client_interpolation_dmul(difference, widened);
    if (!mc_client_interpolation_fp(error, add_pc) ||
        !mc_client_interpolation_double_nan_admit(previous, scaled, add_pc, error))
        return false;
    const double value = mc_client_interpolation_dadd(previous, scaled);
    *out = value;
    return true;
}

#endif
