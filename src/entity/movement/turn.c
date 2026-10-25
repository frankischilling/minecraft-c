#include "mc/entity/movement/turn.h"

#include "../entity_internal.h"

#include <float.h>
#include <limits.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Entity turn requires the captured x86-64/SSE2 target"
#endif
#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "Entity turn requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity turn requires strict floating evaluation"
#endif
#include <emmintrin.h>

_Static_assert(CHAR_BIT == 8 && sizeof(float) == sizeof(uint32_t) &&
                   sizeof(double) == sizeof(uint64_t) && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
                   FLT_EVAL_METHOD == 0,
               "Entity turn requires strict binary32/binary64");

static float component(float current, float delta, bool subtract) {
    const __m128d value = _mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(current));
    const __m128d change = _mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(delta));
    /* Exact original double 0.15, bits 3fc3333333333333. */
    const __m128d scaled = _mm_mul_sd(change, _mm_set_sd(0x1.3333333333333p-3));
    const __m128d updated = subtract ? _mm_sub_sd(value, scaled) : _mm_add_sd(value, scaled);
    /* cvtsd2ss defines Java overflow/underflow/NaN narrowing on this profile;
     * no out-of-range binary64-to-float C17 cast is evaluated. */
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), updated));
}

static float previous(float value, float current, float old) {
    const __m128 difference = _mm_sub_ss(_mm_set_ss(current), _mm_set_ss(old));
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(value), difference));
}

mc_entity_status mc_entity_turn(mc_entity *entity, float yaw_delta, float pitch_delta) {
    if (entity == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (entity->phase != MC_ENTITY_READY)
        return MC_ENTITY_INVALID_PHASE;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return MC_ENTITY_UNSUPPORTED_FP;
    const float old_pitch = entity->base.aT;
    const float old_yaw = entity->base.yaw;
    entity->base.yaw = component(entity->base.yaw, yaw_delta, false); /* store25 */
    entity->base.aT = component(entity->base.aT, pitch_delta, true);  /* store42 */
    if (entity->base.aT < -90.0f)
        entity->base.aT = -90.0f; /* store58 */
    if (entity->base.aT > 90.0f)
        entity->base.aT = 90.0f;                                             /* store74 */
    entity->base.aV = previous(entity->base.aV, entity->base.aT, old_pitch); /* store89 */
    entity->base.aU = previous(entity->base.aU, entity->base.yaw, old_yaw);  /* store105 */
    return MC_ENTITY_OK;
}
