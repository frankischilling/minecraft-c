#include "entity_tick_internal.h"

bool wt_tick(wt_scope *s, mc_entity *p, unsigned int pc) {
    s->error.bytecode = pc;
    s->error.virtual_method = "w_()V";
    if (p == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World null virtual Entity tick receiver");
    if (p->classes->source != MC_ENTITY_CLIENT)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World actual server Entity active tick dependency");
    mc_entity_status status;
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        s->error.virtual_class = "gs";
        s->error.child = MC_WORLD_ENTITY_TICK_CHILD_PLAYER;
        status = mc_player_tick(p, s->context == NULL ? NULL : &s->context->player,
                                s->context == NULL ? NULL : s->context->player_limits,
                                &s->error.child_error.player);
    } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ITEM)) {
        s->error.virtual_class = "hl";
        s->error.child = MC_WORLD_ENTITY_TICK_CHILD_ITEM;
        status = mc_item_entity_tick_in_context(
            p, s->context == NULL ? NULL : s->context->player.travel.code_registry,
            s->context == NULL ? NULL : s->context->item_limits, &s->error.child_error.item);
    } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_FALLING_SAND)) {
        s->error.virtual_class = "ju";
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World actual FallingSand.w_ active tick dependency");
    } else if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_MINECART)) {
        s->error.virtual_class = "yl";
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World actual Minecart.w_ active tick dependency");
    } else
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World unsupported concrete Entity active tick");
    if (status != MC_ENTITY_OK)
        return wt_fail(s, wt_entity_status(status), "World actual Entity active tick child");
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_NONE;
    return true;
}

static bool pose_kind(const mc_entity *p) {
    return mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ITEM) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_FALLING_SAND) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_MINECART) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
}

static bool position_passenger(wt_scope *s, mc_entity *mount) {
    s->error.mount_class = "sn";
    s->error.mount_method = "l_()V";
    s->error.mount_bytecode = 1;
    if (!pose_kind(mount))
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Riding actual mount l_ dispatch dependency");
    /* SN.l_ captures its invocation receiver at1 before m() and I(). The
     * supported descendants below have no callback in either arithmetic body. */
    mc_entity *target = NULL, *offset_target = NULL;
    bool ok = false;
    if (!wt_relationship(s, mount->base.aG, mount, 1, &target))
        return false;
    const double x = mount->base.x, y_base = mount->base.y;
    s->error.mount_method = "m()D";
    const bool cart = mc_entity_is_kind(mount, MC_ENTITY_CLASS_KIND_MINECART);
    s->error.mount_class = cart ? "yl" : "sn";
    s->error.mount_bytecode = 4; /* First pending m()D floating instruction is f2d4. */
    if (!wt_fp(s, s->error.bytecode))
        goto done;
    const double height = wt_widen(mount->base.height);
    const double mounted = cart ? wt_sub(wt_mul(height, 0.0), 0x1.333334p-2) : wt_mul(height, .75);
    const double first_y = wt_add(y_base, mounted);
    s->error.mount_method = "I()D";
    if (!wt_relationship(s, mount->base.aG, mount, 21, &offset_target))
        goto done;
    if (offset_target == NULL) {
        s->error.mount_bytecode = 21;
        wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Riding null passenger I receiver");
        goto done;
    }
    if (!pose_kind(offset_target)) {
        wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "Riding actual passenger I dispatch dependency");
        goto done;
    }
    const bool player = mc_entity_is_kind(offset_target, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
    s->error.mount_class = player ? "gs" : "sn";
    s->error.mount_bytecode = player ? 6u : 4u;
    const double offset = player ? wt_widen(wt_fsub(offset_target->base.offset, .5f))
                                 : wt_widen(offset_target->base.offset);
    const double y = wt_add(first_y, offset), z = mount->base.z;
    s->error.mount_class = "sn";
    s->error.mount_method = "e(DDD)V";
    /* SN.e stores coordinates before its first arithmetic or nullable bounds
     * call. Every represented concrete passenger inherits that same body. */
    s->error.mount_bytecode = 29;
    if (target == NULL || !pose_kind(target)) {
        wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "Riding actual passenger e dispatch dependency");
        goto done;
    }
    target->base.x = x;
    target->base.y = y;
    target->base.z = z;
    s->error.mount_bytecode = 21;
    if (!wt_fp(s, s->error.bytecode))
        goto done;
    if (target->bounds == NULL) {
        s->error.mount_bytecode = 86;
        wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Riding position null bounds receiver");
        goto done;
    }
    mc_entity_source_position(target, x, y, z);
    ok = true;
done:
    mc_entity_release(offset_target);
    mc_entity_release(target);
    return ok;
}

static bool angular_loop(wt_scope *s, double *value, double boundary, bool upper, unsigned int pc) {
    /* dcmpl/iflt and dcmpg/ifge skip normalization on NaN. Infinity
     * still enters its corresponding loop and can reach the native ceiling. */
    while (upper ? *value >= boundary : *value < boundary) {
        s->error.riding_bytecode = pc;
        if (!wt_work(s, s->error.bytecode))
            return false;
        *value = upper ? wt_sub(*value, 360.0) : wt_add(*value, 360.0);
    }
    return true;
}

bool wt_riding(wt_scope *s, mc_entity *p, unsigned int pc) {
    s->error.bytecode = pc;
    s->error.virtual_method = "s_()V";
    if (p == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World null riding tick receiver");
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ITEM) ||
        p->classes->source != MC_ENTITY_CLIENT) {
        s->error.riding_class =
            mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ? "gs" : "sn";
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World actual non-Item riding/statistic tick dependency");
    }
    s->error.riding_class = "sn";
    s->error.riding_bytecode = 4;
    mc_entity *mount;
    if (!wt_relationship(s, p->base.aH, p, 4, &mount))
        return false;
    if (mount == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Riding null mount dead receiver");
    const bool dead = mount->base.be;
    mc_entity_release(mount);
    if (dead) {
        p->base.aH = NULL;
        return true;
    }
    p->base.velocity_x = 0.0;
    p->base.velocity_y = 0.0;
    p->base.velocity_z = 0.0;
    s->error.riding_bytecode = 32;
    if (!wt_tick(s, p, pc))
        return false;
    s->error.virtual_class = "sn";
    s->error.virtual_method = "s_()V";
    if (p->base.aH == NULL)
        return true;
    s->error.riding_bytecode = 47;
    if (!wt_relationship(s, p->base.aH, p, 47, &mount))
        return false;
    if (mount == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Riding null mount positioning receiver");
    const bool positioned = position_passenger(s, mount);
    mc_entity_release(mount);
    if (!positioned)
        return false;
    s->error.mount_class = s->error.mount_method = NULL;
    s->error.mount_bytecode = 0;
    s->error.riding_bytecode = 69;
    if (!wt_fp(s, pc))
        return false;
    p->base.private_e =
        wt_add(p->base.private_e, wt_widen(wt_fsub(p->base.aH->base.yaw, p->base.aH->base.aU)));
    p->base.private_d =
        wt_add(p->base.private_d, wt_widen(wt_fsub(p->base.aH->base.aT, p->base.aH->base.aV)));
    if (!angular_loop(s, &p->base.private_e, 180.0, true, 111) ||
        !angular_loop(s, &p->base.private_e, -180.0, false, 137) ||
        !angular_loop(s, &p->base.private_d, 180.0, true, 163) ||
        !angular_loop(s, &p->base.private_d, -180.0, false, 189))
        return false;
    double yaw = wt_mul(p->base.private_e, .5), pitch = wt_mul(p->base.private_d, .5);
    if (yaw > 10.0)
        yaw = 10.0;
    if (yaw < -10.0)
        yaw = -10.0;
    if (pitch > 10.0)
        pitch = 10.0;
    if (pitch < -10.0)
        pitch = -10.0;
    p->base.private_e = wt_sub(p->base.private_e, yaw);
    p->base.private_d = wt_sub(p->base.private_d, pitch);
    p->base.yaw = _mm_cvtss_f32(
        _mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(wt_add(wt_widen(p->base.yaw), yaw))));
    p->base.aT = _mm_cvtss_f32(
        _mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(wt_add(wt_widen(p->base.aT), pitch))));
    s->error.riding_bytecode = 322;
    return true;
}
