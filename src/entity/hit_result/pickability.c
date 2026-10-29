#include "internal.h"

static mc_entity_hit_status pick_failure(mc_entity *receiver, mc_entity_hit_error *error,
                                         mc_entity_hit_status status, const char *operation,
                                         bool native_policy, bool border) {
    mc_entity_hit_fail(error, status, operation, native_policy,
                       receiver == NULL ? NULL : receiver->classes, border ? "m_" : "h_",
                       border ? "()F" : "()Z", false, 0);
    if (error != NULL) {
        error->site.declaring_class = "sn";
        if (!border && receiver != NULL) {
            if (mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
                error->site.declaring_class = "ls";
            else if (mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_FALLING_SAND))
                error->site.declaring_class = "ju";
            else if (mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_MINECART))
                error->site.declaring_class = "yl";
        }
        if (status == MC_ENTITY_HIT_UNSUPPORTED_SOURCE)
            error->site = (mc_entity_geometry_site){0};
    }
    return status;
}

static mc_entity_hit_status admit(mc_entity *receiver, bool output, mc_entity_hit_error *error,
                                  bool border) {
    mc_entity_hit_error_clear(error);
    if (!output)
        return pick_failure(receiver, error, MC_ENTITY_HIT_INVALID_ARGUMENT,
                            "Entity pick native output", true, border);
    if (receiver == NULL)
        return pick_failure(receiver, error, MC_ENTITY_HIT_SOURCE_NULL_REFERENCE,
                            "Entity pick null receiver", false, border);
    if (receiver->classes->source != MC_ENTITY_CLIENT ||
        (!mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_ITEM) &&
         !mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_FALLING_SAND) &&
         !mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_MINECART) &&
         !mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) &&
         !mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE)))
        return pick_failure(receiver, error, MC_ENTITY_HIT_UNSUPPORTED_SOURCE,
                            "Entity pick actual descendant frontier", true, border);
    const mc_entity_status held = mc_entity_retain(receiver);
    if (held != MC_ENTITY_OK) {
        if (error != NULL)
            error->native.status = held;
        return pick_failure(receiver, error, MC_ENTITY_HIT_NATIVE_FAILED,
                            "Entity pick native occurrence", true, border);
    }
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_pickable(mc_entity *receiver, bool *out,
                                        mc_entity_hit_error *error) {
    const mc_entity_hit_status status = admit(receiver, out != NULL, error, false);
    if (status != MC_ENTITY_HIT_OK)
        return status;
    *out = (mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
            mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_FALLING_SAND) ||
            mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_MINECART)) &&
           !receiver->base.be;
    mc_entity_release(receiver);
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_pick_border_size(mc_entity *receiver, float *out,
                                                mc_entity_hit_error *error) {
    const mc_entity_hit_status status = admit(receiver, out != NULL, error, true);
    if (status != MC_ENTITY_HIT_OK)
        return status;
    *out = 0.1f;
    mc_entity_release(receiver);
    return MC_ENTITY_HIT_OK;
}
