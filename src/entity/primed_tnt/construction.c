#include "internal.h"

#include "../classes/memory_internal.h"
#include "../construction_internal.h"
#include "../position_internal.h"

#include <float.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Primed TNT construction requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Primed TNT construction requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && sizeof(float) == 4 &&
                   sizeof(double) == 8 && FLT_EVAL_METHOD == 0,
               "Primed TNT requires binary32/binary64 evaluation");

static bool fp(unsigned int pc, mc_entity_error *error) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    (void)mc_entity_error_set(error, MC_ENTITY_UNSUPPORTED_FP,
                              "Primed TNT reached strict floating environment", (uint16_t)pc);
    return false;
}

static void world_at(mc_entity *entity, unsigned int pc) {
    mc_primed_tnt_entity_state *state = mc_primed_tnt_state(entity);
    state->world_constructor_pc = pc;
    if (!state->full_constructor)
        state->constructor_pc = pc;
}

mc_entity_status mc_primed_tnt_entity_allocate(mc_entity_classes *classes, mc_entity **out,
                                               mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_PRIMED_TNT,
                                        "Primed TNT allocation", out, error);
}

static mc_entity_status world_prefix(mc_entity *entity, mc_world_runtime *world,
                                     mc_entity_error *error) {
    mc_primed_tnt_entity_state *state = mc_primed_tnt_state(entity);
    world_at(entity, 2);
    mc_entity_status status = mc_entity_construct_base(entity, world, error);
    if (status != MC_ENTITY_OK)
        return status;
    state->fuse = 0;
    world_at(entity, 7);
    entity->base.aF = true;
    world_at(entity, 12);
    mc_entity_source_size(entity, 0.98f, 0.98f);
    world_at(entity, 20);
    world_at(entity, 29);
    if (!fp(29, error))
        return error->status;
    entity->base.offset = entity->base.height / 2.0f;
    world_at(entity, 33);
    return MC_ENTITY_OK;
}

static mc_entity_status construct(mc_entity *entity, mc_world_runtime *world, double x, double y,
                                  double z, const mc_math_trig *trig, bool full,
                                  mc_entity_error *error) {
    mc_entity_error local;
    if (error == NULL)
        error = &local;
    mc_entity_status status = mc_entity_constructor_begin(
        entity, MC_ENTITY_CLASS_KIND_PRIMED_TNT, world, "Primed TNT constructor",
        "Primed TNT constructor World profile", error);
    if (status != MC_ENTITY_OK)
        return status;
    mc_primed_tnt_entity_state *state = mc_primed_tnt_state(entity);
    state->full_constructor = full;
    state->constructor_pc = 2;
    status = world_prefix(entity, world, error);
    if (status != MC_ENTITY_OK || !full)
        return mc_entity_constructor_finish(entity, status, error);

    state->constructor_pc = 11;
    mc_entity_source_position_error position = {0};
    status = mc_entity_source_position_checked(entity, x, y, z, &position);
    if (status != MC_ENTITY_OK) {
        (void)mc_entity_error_set(error, status, position.operation, 11);
        return mc_entity_constructor_finish(entity, status, error);
    }
    state->constructor_pc = 14;
    double draw;
    const mc_java_random_profile_status random_status =
        mc_java_math_random(entity->classes->profile, &draw);
    if (random_status != MC_JAVA_RANDOM_PROFILE_OK) {
        (void)mc_entity_error_set(error, MC_ENTITY_PROFILE_FAILED,
                                  "Primed TNT constructor actual Math.random", 14);
        error->profile = random_status;
        return mc_entity_constructor_finish(entity, error->status, error);
    }
    state->constructor_pc = 20;
    if (!fp(20, error))
        return mc_entity_constructor_finish(entity, error->status, error);
    const double radians = draw * 3.1415927410125732;
    const double doubled = radians * 2.0;
    const float angle = (float)doubled;
    const float scaled_x = angle * 3.1415927f;
    const float argument_x = scaled_x / 180.0f;
    state->constructor_pc = 37;
    if (trig == NULL) {
        (void)mc_entity_error_set(error, MC_ENTITY_STATE_ERROR,
                                  "Primed TNT constructor actual IN sine table", 37);
        return mc_entity_constructor_finish(entity, error->status, error);
    }
    const float sine = mc_math_sin(trig, argument_x);
    const float negative_sine = -sine;
    const float motion_x = negative_sine * 0.02f;
    entity->base.velocity_x = (double)motion_x;
    state->constructor_pc = 45;
    entity->base.velocity_y = 0.20000000298023224;
    state->constructor_pc = 52;
    const float scaled_z = angle * 3.1415927f;
    const float argument_z = scaled_z / 180.0f;
    state->constructor_pc = 64;
    const float cosine = mc_math_cos(trig, argument_z);
    const float negative_cosine = -cosine;
    const float motion_z = negative_cosine * 0.02f;
    entity->base.velocity_z = (double)motion_z;
    state->fuse = 80;
    entity->base.aJ = x;
    entity->base.aK = y;
    entity->base.aL = z;
    state->constructor_pc = 98;
    return mc_entity_constructor_finish(entity, MC_ENTITY_OK, error);
}

mc_entity_status mc_primed_tnt_entity_construct_world(mc_entity *raw, mc_world_runtime *world,
                                                      mc_entity_error *error) {
    return construct(raw, world, 0.0, 0.0, 0.0, NULL, false, error);
}

mc_entity_status mc_primed_tnt_entity_construct(mc_entity *raw, mc_world_runtime *world, double x,
                                                double y, double z, const mc_math_trig *trig,
                                                mc_entity_error *error) {
    return construct(raw, world, x, y, z, trig, true, error);
}

mc_entity_status mc_primed_tnt_entity_get_snapshot(const mc_entity *entity,
                                                   mc_primed_tnt_entity_snapshot *out) {
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PRIMED_TNT) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    const mc_primed_tnt_entity_state *state = &((const mc_primed_tnt_entity_object *)entity)->state;
    *out = (mc_primed_tnt_entity_snapshot){state->fuse, state->constructor_pc,
                                           state->world_constructor_pc, state->full_constructor};
    return MC_ENTITY_OK;
}
