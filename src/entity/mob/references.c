#include "references_internal.h"

#include "internal.h"

static mc_entity_status result(mc_entity_error *error, mc_entity_status status,
                               const char *operation) {
    if (error != NULL)
        *error = (mc_entity_error){.status = status, .operation = operation};
    return status;
}

static mc_entity_status slots(mc_entity *owner, bool creature, mc_entity ***source,
                              mc_entity ***held, mc_entity_error *error) {
    if (!mc_entity_has_mob(owner) || !mc_entity_is_kind(owner, owner->class_literal->kind) ||
        (creature && mc_entity_is_kind(owner, MC_ENTITY_CLASS_KIND_SLIME)))
        return result(error, MC_ENTITY_INVALID_ARGUMENT, "Mob actual reference field owner");
    *source = creature ? &owner->mob->creature_target : &owner->mob->living.private_b;
    *held = creature ? &owner->mob->creature_target_hold : &owner->mob->look_target_hold;
    if (**source != **held)
        return result(error, MC_ENTITY_STATE_ERROR, "Mob source reference occurrence provenance");
    return MC_ENTITY_OK;
}

static mc_entity_status get(mc_entity *owner, bool creature, mc_entity **out,
                            mc_entity_error *error) {
    if (out == NULL)
        return result(error, MC_ENTITY_INVALID_ARGUMENT, "Mob reference output");
    mc_entity **source, **held;
    const mc_entity_status status = slots(owner, creature, &source, &held, error);
    if (status != MC_ENTITY_OK)
        return status;
    *out = *source;
    return result(error, MC_ENTITY_OK, NULL);
}

static mc_entity_status store(mc_entity *owner, bool creature, mc_entity *value,
                              mc_entity_error *error) {
    mc_entity **source, **held;
    mc_entity_status status = slots(owner, creature, &source, &held, error);
    if (status != MC_ENTITY_OK)
        return status;
    if (value == *source)
        return result(error, MC_ENTITY_OK, NULL);
    if (value != NULL) {
        if (value->classes != owner->classes)
            return result(error, MC_ENTITY_INVALID_ARGUMENT, "Mob reference class namespace");
        status = mc_entity_retain(value);
        if (status != MC_ENTITY_OK)
            return result(error, status, "Mob new reference occurrence hold");
    }
    mc_entity *old = *held;
    *source = value;
    *held = value;
    mc_entity_release(old);
    return result(error, MC_ENTITY_OK, NULL);
}

mc_entity_status mc_mob_look_target_get(mc_entity *owner, mc_entity **out, mc_entity_error *error) {
    return get(owner, false, out, error);
}

mc_entity_status mc_mob_look_target_store(mc_entity *owner, mc_entity *value,
                                          mc_entity_error *error) {
    return store(owner, false, value, error);
}

mc_entity_status mc_mob_creature_target_get(mc_entity *owner, mc_entity **out,
                                            mc_entity_error *error) {
    return get(owner, true, out, error);
}

mc_entity_status mc_mob_creature_target_store(mc_entity *owner, mc_entity *value,
                                              mc_entity_error *error) {
    return store(owner, true, value, error);
}

void mc_mob_references_dispose(mc_entity *owner) {
    mc_entity *look = owner->mob->look_target_hold;
    mc_entity *creature = owner->mob->creature_target_hold;
    owner->mob->look_target_hold = NULL;
    owner->mob->creature_target_hold = NULL;
    owner->mob->living.private_b = NULL;
    owner->mob->creature_target = NULL;
    mc_entity_release(look);
    mc_entity_release(creature);
}
