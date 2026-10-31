#include "mc/entity/player/mouse_filter.h"

#include "tick_math_internal.h"

/* The pinned interpreter's fadd chooses and quiets the right operand when
 * both inputs are NaNs. Plain ADDSS keeps the other payload on qualified
 * native builds. Preserve that observed operand order before each source
 * addition rather than canonicalizing all NaNs or changing comparisons. */
static float filter_add(float left, float right) {
    uint32_t left_bits, right_bits;
    memcpy(&left_bits, &left, sizeof(left_bits));
    memcpy(&right_bits, &right, sizeof(right_bits));
    if ((left_bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000) &&
        (right_bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000)) {
        right_bits |= UINT32_C(0x00400000);
        memcpy(&right, &right_bits, sizeof(right));
        return right;
    }
    return at_fadd(left, right);
}

mc_entity_status mc_player_mouse_filter_apply(mc_player_mouse_filter_fields *fields, float input,
                                              float factor, float *out) {
    if (fields == NULL || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return MC_ENTITY_UNSUPPORTED_FP;
    fields->a = filter_add(fields->a, input);
    float filtered = at_fmul(at_fsub(fields->a, fields->b), factor);
    fields->c = filter_add(fields->c, at_fmul(at_fsub(filtered, fields->c), 0.5f));
    if ((filtered > 0.0f && filtered > fields->c) || (filtered < 0.0f && filtered < fields->c))
        filtered = fields->c;
    fields->b = filter_add(fields->b, filtered);
    *out = filtered;
    return MC_ENTITY_OK;
}
