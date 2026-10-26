#include "active_internal.h"

static void at(mc_player_tick_error *e, unsigned int pc) {
    e->source_class = "ls";
    e->source_method = "w_()V";
    e->living_bytecode = pc;
}

static bool delta_loop(float *value, float boundary, bool negative, unsigned int pc,
                       mc_player_tick_error *e) {
    while (negative ? *value < boundary : *value >= boundary) {
        at(e, pc);
        if (!mc_active_work(e))
            return false;
        *value = negative ? at_fadd(*value, 360.0f) : at_fsub(*value, 360.0f);
    }
    return true;
}

static bool previous_loop(float current, float *previous, bool negative, unsigned int pc,
                          mc_player_tick_error *e) {
    while (negative ? at_fsub(current, *previous) < -180.0f
                    : at_fsub(current, *previous) >= 180.0f) {
        at(e, pc);
        if (!mc_active_work(e))
            return false;
        *previous = negative ? at_fsub(*previous, 360.0f) : at_fadd(*previous, 360.0f);
    }
    return true;
}

bool mc_active_living(mc_active_scope *s, mc_player_tick_error *e) {
    mc_entity *p = s->player;
    mc_living_entity_fields *v = &p->local_player->fields.living;
    at(e, 1);
    e->child = MC_PLAYER_TICK_CHILD_ENVIRONMENT;
    const mc_entity_status base = mc_entity_tick_update_in_context(
        p, s->registry, s->limits == NULL ? NULL : &s->limits->environment,
        &e->child_error.environment);
    if (base != MC_ENTITY_OK)
        return mc_active_fail(e, base, "Living tick actual sn.w_ -> virtual ls.U child");
    at(e, 5);
    if (!mc_active_local(s, e))
        return false;
    at(e, 16);
    if (!mc_active_fp(e))
        return false;
    const double x = at_sub(p->base.x, p->base.aJ), z = at_sub(p->base.z, p->base.aL);
    const float speed = at_root(at_add(at_mul(x, x), at_mul(z, z)));
    float target = v->H, phase = 0.0f, weight = 0.0f;
    v->J = v->K;
    /* Actual fcmpg/ifgt enters this branch for NaN as well. */
    if (!(speed <= .05f)) {
        weight = 1.0f;
        phase = at_fmul(speed, 3.0f);
        target = at_fsub(
            at_fdiv(at_fmul(at_narrow(mc_player_tick_atan2(z, x)), 180.0f), 3.1415927f), 90.0f);
    }
    if (v->X > 0.0f)
        target = p->base.yaw;
    if (!p->base.aX)
        weight = 0.0f;
    v->K = at_fadd(v->K, at_fmul(at_fsub(weight, v->K), .3f));
    float difference = at_fsub(target, v->H);
    if (!delta_loop(&difference, -180.0f, true, 163, e) ||
        !delta_loop(&difference, 180.0f, false, 181, e))
        return false;
    v->H = at_fadd(v->H, at_fmul(difference, .3f));
    difference = at_fsub(p->base.yaw, v->H);
    if (!delta_loop(&difference, -180.0f, true, 224, e) ||
        !delta_loop(&difference, 180.0f, false, 242, e))
        return false;
    const bool backwards = difference < -90.0f || difference >= 90.0f;
    if (difference < -75.0f)
        difference = -75.0f;
    if (difference >= 75.0f)
        difference = 75.0f;
    v->H = at_fsub(p->base.yaw, difference);
    if (at_fmul(difference, difference) > 2500.0f)
        v->H = at_fadd(v->H, at_fmul(difference, .2f));
    if (backwards)
        phase = at_fmul(phase, -1.0f);
    if (!previous_loop(p->base.yaw, &p->base.aU, true, 365, e) ||
        !previous_loop(p->base.yaw, &p->base.aU, false, 394, e) ||
        !previous_loop(v->H, &v->I, true, 423, e) || !previous_loop(v->H, &v->I, false, 452, e) ||
        !previous_loop(p->base.aT, &p->base.aV, true, 481, e) ||
        !previous_loop(p->base.aT, &p->base.aV, false, 510, e))
        return false;
    at(e, 524);
    v->L = at_fadd(v->L, phase);
    return true;
}
