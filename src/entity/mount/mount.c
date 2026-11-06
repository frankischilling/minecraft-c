#include "mc/entity/mount/mount.h"

#include "../entity_internal.h"
#include "../mob/internal.h"

#include <emmintrin.h>
#include <string.h>

static mc_entity_status store(mc_entity **source, mc_entity **hold, mc_entity *value) {
    if (value != NULL) {
        const mc_entity_status status = mc_entity_retain(value);
        if (status != MC_ENTITY_OK)
            return status;
    }
    mc_entity *old = *hold;
    *source = value;
    *hold = value;
    mc_entity_release(old);
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_mount_vehicle_store(mc_entity *p, mc_entity *value) {
    return store(&p->base.aH, &p->mount_hold, value);
}

mc_entity_status mc_entity_mount_passenger_store(mc_entity *p, mc_entity *value) {
    return store(&p->base.aG, &p->passenger_hold, value);
}

void mc_entity_mount_references_dispose(mc_entity *p) {
    (void)mc_entity_mount_vehicle_store(p, NULL);
    (void)mc_entity_mount_passenger_store(p, NULL);
}

static bool fail(mc_entity_mount_error *e, mc_entity_status status, const char *operation,
                 unsigned int pc, bool source_null) {
    e->status = status;
    e->operation = operation;
    e->bytecode = pc;
    e->source_null = source_null;
    e->native_policy = !source_null;
    return false;
}

static bool fp(mc_entity_mount_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_STATE_ERROR, "Mount strict floating environment", pc, false);
}

static double add(double a, double b) {
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static bool known(mc_entity *p) {
    return p == NULL || mc_entity_has_mob(p) || mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ITEM) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_FALLING_SAND) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_MINECART) ||
           mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
}

static bool link(mc_entity *owner, bool passenger, mc_entity *const *scope, size_t count,
                 mc_entity **out, mc_entity_mount_error *e, unsigned int pc) {
    mc_entity *value = passenger ? owner->base.aG : owner->base.aH;
    mc_entity *held = passenger ? owner->passenger_hold : owner->mount_hold;
    bool proved = value == NULL || value == owner || value == held;
    for (size_t i = 0; !proved && i < count; ++i)
        proved = scope[i] == value;
    if (!proved || (held != NULL && held != value))
        return fail(e, MC_ENTITY_STATE_ERROR, "Mount unproved relationship occurrence", pc, false);
    if (value != NULL && value->classes != owner->classes)
        return fail(e, MC_ENTITY_STATE_ERROR, "Mount relationship namespace", pc, false);
    if (value != NULL && mc_entity_retain(value) != MC_ENTITY_OK)
        return fail(e, MC_ENTITY_LIMIT, "Mount relationship local hold", pc, false);
    *out = value;
    return true;
}

static bool position(mc_entity *p, mc_entity *mount, mc_entity_mount_error *e, unsigned int pc) {
    const double x = mount->base.x;
    if (mount->bounds == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Mount dismount null bounds", pc == 63 ? 36u : 110u,
                    true);
    if (!fp(e, pc == 63 ? 47u : 118u))
        return false;
    const double y = add(mount->bounds->min_y, (double)mount->base.height), z = mount->base.z;
    const float yaw = p->base.yaw, pitch = p->base.aT;
    e->bytecode = pc;
    /* The admitted concrete receivers all inherit the actual SN.c body. */
    p->base.x = p->base.aJ = p->base.bl = x;
    p->base.y = add(y, (double)p->base.offset);
    p->base.aK = p->base.bm = p->base.y;
    p->base.z = p->base.aL = p->base.bn = z;
    p->base.yaw = yaw;
    p->base.aT = pitch;
    e->position_bytecode = 77;
    if (p->bounds == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Mount position null rider bounds", pc, true);
    mc_entity_source_position(p, p->base.x, p->base.y, p->base.z);
    return true;
}

static bool changed(mc_entity_mount_error *e, mc_entity_status status, unsigned int pc) {
    return status == MC_ENTITY_OK ||
           fail(e, status, "Mount source reference occurrence store", pc, false);
}

mc_entity_status mc_entity_mount(mc_entity *p, mc_entity *target, mc_entity *const *scope,
                                 size_t count, mc_entity_mount_error *out) {
    mc_entity_mount_error local = {0};
    mc_entity_mount_error *e = out == NULL ? &local : out;
    *e = (mc_entity_mount_error){0};
    if (count != 0 && scope == NULL) {
        fail(e, MC_ENTITY_INVALID_ARGUMENT, "Mount explicit live scope", 0, false);
        return e->status;
    }
    if (p == NULL) {
        fail(e, MC_ENTITY_STATE_ERROR, "Mount null invocation receiver", 0, true);
        return e->status;
    }
    if (!known(p) || p->classes->source != MC_ENTITY_CLIENT || !known(target) ||
        (target != NULL && target->classes != p->classes)) {
        fail(e, MC_ENTITY_STATE_ERROR, "Mount actual inherited client SN declaration", 0, false);
        return e->status;
    }
    if (mc_entity_retain(p) != MC_ENTITY_OK) {
        fail(e, MC_ENTITY_LIMIT, "Mount rider local hold", 0, false);
        return e->status;
    }
    if (target != NULL && mc_entity_retain(target) != MC_ENTITY_OK) {
        mc_entity_release(p);
        fail(e, MC_ENTITY_LIMIT, "Mount target local hold", 0, false);
        return e->status;
    }
    mc_entity *old = NULL, *passenger = NULL;
    p->base.private_d = 0.0; /* SN.i:2,7 precede every source branch. */
    p->base.private_e = 0.0;
    if (!link(p, false, scope, count, &old, e, target == NULL ? 23u : 81u))
        goto done;
    if (target == NULL) {
        if (old != NULL && (!position(p, old, e, 63) ||
                            !changed(e, mc_entity_mount_passenger_store(old, NULL), 71)))
            goto done;
        if (!changed(e, mc_entity_mount_vehicle_store(p, NULL), 76))
            goto done;
        e->bytecode = 79;
    } else if (old == target) {
        if (!changed(e, mc_entity_mount_passenger_store(old, NULL), 93) ||
            !changed(e, mc_entity_mount_vehicle_store(p, NULL), 98) || !position(p, target, e, 131))
            goto done;
        e->bytecode = 134;
    } else {
        if (old != NULL && !changed(e, mc_entity_mount_passenger_store(old, NULL), 147))
            goto done;
        if (!link(target, true, scope, count, &passenger, e, 151))
            goto done;
        if (passenger != NULL && !changed(e, mc_entity_mount_vehicle_store(passenger, NULL), 162))
            goto done;
        if (!changed(e, mc_entity_mount_vehicle_store(p, target), 167) ||
            !changed(e, mc_entity_mount_passenger_store(target, p), 172))
            goto done;
        e->bytecode = 175;
    }
done:
    mc_entity_release(passenger);
    mc_entity_release(old);
    mc_entity_release(target);
    mc_entity_release(p);
    return e->status;
}
