#include "mc/core/math/classes.h"
#include <float.h>
#include <stdlib.h>
#include <xmmintrin.h>
#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "MathHelper class owner requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "MathHelper class owner requires strict floating transformations"
#endif
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && sizeof(float) == 4,
               "MathHelper binary32 zero array");

struct mc_math_classes {
    mc_math_class_state state;
    mc_math_trig *table;
    mc_math_class_error first_failure;
    bool active;
};

static mc_math_class_status result(mc_math_class_error *e, mc_math_class_status s, const char *op,
                                   int pc, mc_math_class_status child) {
    if (e != NULL)
        *e = (mc_math_class_error){s, op, pc, child};
    return s;
}

mc_math_class_status mc_math_classes_create(size_t max, mc_math_classes **out,
                                            mc_math_class_error *e) {
    if (out == NULL)
        return result(e, MC_MATH_CLASS_INVALID_ARGUMENT, "Math class output", -1, MC_MATH_CLASS_OK);
    if (max < sizeof(mc_math_classes))
        return result(e, MC_MATH_CLASS_LIMIT, "Math class native allocation bound", -1,
                      MC_MATH_CLASS_OK);
    mc_math_classes *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return result(e, MC_MATH_CLASS_ALLOCATION_FAILED, "Math class native allocation", -1,
                      MC_MATH_CLASS_OK);
    c->state = (mc_math_class_state){.phase = MC_MATH_CLASS_COLD, .holds = 1, .maximum_bytes = max};
    *out = c;
    return result(e, MC_MATH_CLASS_OK, NULL, -1, MC_MATH_CLASS_OK);
}

mc_math_class_status mc_math_classes_retain(mc_math_classes *c, mc_math_class_error *e) {
    if (c == NULL)
        return result(e, MC_MATH_CLASS_INVALID_ARGUMENT, "Math class retain", -1, MC_MATH_CLASS_OK);
    if (c->active || c->state.phase == MC_MATH_CLASS_RETIRING)
        return result(e, MC_MATH_CLASS_BUSY, "Math class active/terminal retain", -1,
                      MC_MATH_CLASS_OK);
    if (c->state.holds == SIZE_MAX)
        return result(e, MC_MATH_CLASS_LIMIT, "Math class holds", -1, MC_MATH_CLASS_OK);
    ++c->state.holds;
    return result(e, MC_MATH_CLASS_OK, NULL, -1, MC_MATH_CLASS_OK);
}

mc_math_class_status mc_math_classes_release(mc_math_classes **slot, mc_math_class_error *e) {
    if (slot == NULL)
        return result(e, MC_MATH_CLASS_INVALID_ARGUMENT, "Math class release slot", -1,
                      MC_MATH_CLASS_OK);
    mc_math_classes *c = *slot;
    if (c == NULL)
        return result(e, MC_MATH_CLASS_OK, NULL, -1, MC_MATH_CLASS_OK);
    if (c->active || c->state.phase == MC_MATH_CLASS_RETIRING)
        return result(e, MC_MATH_CLASS_BUSY, "Math class active retirement", -1, MC_MATH_CLASS_OK);
    *slot = NULL;
    if (--c->state.holds == 0) {
        c->state.phase = MC_MATH_CLASS_RETIRING;
        free(c->table);
        free(c);
    }
    return result(e, MC_MATH_CLASS_OK, NULL, -1, MC_MATH_CLASS_OK);
}

mc_math_class_status mc_math_classes_state(const mc_math_classes *c, mc_math_class_state *out,
                                           mc_math_class_error *e) {
    if (c == NULL || out == NULL)
        return result(e, MC_MATH_CLASS_INVALID_ARGUMENT, "Math class state", -1, MC_MATH_CLASS_OK);
    *out = c->state;
    return result(e, MC_MATH_CLASS_OK, NULL, -1, MC_MATH_CLASS_OK);
}

static mc_math_class_status failed(mc_math_classes *c, mc_math_class_status s, const char *op,
                                   int pc, mc_math_class_error *e) {
    c->state.phase = MC_MATH_CLASS_FAILED;
    c->active = false;
    c->first_failure = (mc_math_class_error){s, op, pc, MC_MATH_CLASS_OK};
    if (e != NULL)
        *e = c->first_failure;
    return s;
}

mc_math_class_status mc_math_classes_ensure(mc_math_classes *c, mc_math_class_error *e) {
    if (c == NULL)
        return result(e, MC_MATH_CLASS_INVALID_ARGUMENT, "Math class active use", -1,
                      MC_MATH_CLASS_OK);
    if (c->active || c->state.phase == MC_MATH_CLASS_CONSTRUCTING ||
        c->state.phase == MC_MATH_CLASS_RETIRING)
        return result(e, MC_MATH_CLASS_BUSY, "Math class initialization admission", -1,
                      MC_MATH_CLASS_OK);
    if (c->state.phase == MC_MATH_CLASS_READY)
        return result(e, MC_MATH_CLASS_OK, NULL, -1, MC_MATH_CLASS_OK);
    if (c->state.phase == MC_MATH_CLASS_FAILED)
        return result(e, MC_MATH_CLASS_INITIALIZATION_FAILED, c->first_failure.operation,
                      c->first_failure.source_pc, c->first_failure.status);
    c->active = true;
    c->state.phase = MC_MATH_CLASS_CONSTRUCTING;
    if (sizeof(mc_math_trig) > c->state.maximum_bytes - sizeof(*c))
        return failed(c, MC_MATH_CLASS_LIMIT, "in zero array allocation bound", 2, e);
    c->table = calloc(1, sizeof(*c->table));
    if (c->table == NULL)
        return failed(c, MC_MATH_CLASS_ALLOCATION_FAILED, "in zero array allocation", 2, e);
    c->state.source_a = c->table;
    c->state.last_store_pc = 4;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return failed(c, MC_MATH_CLASS_UNSUPPORTED_FP, "in first conversion FP admission", 20, e);
    if (!mc_math_trig_init(c->table))
        return failed(c, MC_MATH_CLASS_STATE_ERROR, "in real table fill", 33, e);
    c->state.last_store_pc = 37;
    c->state.phase = MC_MATH_CLASS_READY;
    c->active = false;
    return result(e, MC_MATH_CLASS_OK, NULL, -1, MC_MATH_CLASS_OK);
}

const mc_math_trig *mc_math_classes_table(const mc_math_classes *c) {
    return c != NULL && c->state.phase == MC_MATH_CLASS_READY ? c->table : NULL;
}
