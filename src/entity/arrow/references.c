#include "references_internal.h"

#include "../living/fields_internal.h"
#include "internal.h"

static mc_entity_status result(mc_entity_error *error, mc_entity_status status,
                               const char *operation) {
    if (error != NULL)
        *error = (mc_entity_error){.status = status, .operation = operation};
    return status;
}

static mc_entity_status slot(mc_entity *owner, mc_entity_error *error) {
    if (!mc_entity_has_arrow(owner))
        return result(error, MC_ENTITY_INVALID_ARGUMENT, "Arrow actual shooter field owner");
    if (owner->arrow->fields.c != owner->arrow->shooter_hold)
        return result(error, MC_ENTITY_STATE_ERROR, "Arrow shooter occurrence provenance");
    mc_entity *held = owner->arrow->shooter_hold;
    if (held != NULL && (held->holds == 0 || held->classes != owner->classes ||
                         mc_entity_living_fields(held) == NULL))
        return result(error, MC_ENTITY_STATE_ERROR, "Arrow shooter actual Living namespace");
    return MC_ENTITY_OK;
}

mc_entity_status mc_arrow_shooter_get(mc_entity *owner, mc_entity **out, mc_entity_error *error) {
    if (out == NULL)
        return result(error, MC_ENTITY_INVALID_ARGUMENT, "Arrow shooter output");
    const mc_entity_status status = slot(owner, error);
    if (status != MC_ENTITY_OK)
        return status;
    *out = owner->arrow->fields.c;
    return result(error, MC_ENTITY_OK, NULL);
}

mc_entity_status mc_arrow_shooter_store(mc_entity *owner, mc_entity *value,
                                        mc_entity_error *error) {
    mc_entity_status status = slot(owner, error);
    if (status != MC_ENTITY_OK)
        return status;
    if (value == owner->arrow->fields.c)
        return result(error, MC_ENTITY_OK, NULL);
    if (value != NULL) {
        if (value->holds == 0 || value->classes != owner->classes ||
            mc_entity_living_fields(value) == NULL)
            return result(error, MC_ENTITY_INVALID_ARGUMENT, "Arrow new shooter Living namespace");
        status = mc_entity_retain(value);
        if (status != MC_ENTITY_OK)
            return result(error, status, "Arrow new shooter occurrence hold");
    }
    mc_entity *old = owner->arrow->shooter_hold;
    owner->arrow->fields.c = value;
    owner->arrow->shooter_hold = value;
    mc_entity_release(old);
    return result(error, MC_ENTITY_OK, NULL);
}

void mc_arrow_references_dispose(mc_entity *owner) {
    mc_entity *old = owner->arrow->shooter_hold;
    owner->arrow->fields.c = NULL;
    owner->arrow->shooter_hold = NULL;
    mc_entity_release(old);
}
