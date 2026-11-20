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

static mc_entity_status path_result(mc_path_error *error, mc_entity_status status,
                                    const char *operation) {
    if (error != NULL) {
        error->status = status == MC_ENTITY_OK                 ? MC_PATH_OK
                        : status == MC_ENTITY_LIMIT            ? MC_PATH_LIMIT
                        : status == MC_ENTITY_INVALID_ARGUMENT ? MC_PATH_ARGUMENT
                                                               : MC_PATH_STATE;
        error->operation = operation;
        error->native_policy = status != MC_ENTITY_OK;
        error->entity = (mc_entity_error){.status = status, .operation = operation};
    }
    return status;
}

static mc_entity_status path_slot(mc_entity *owner, mc_path_error *error) {
    if (!mc_entity_has_mob(owner) || !mc_entity_is_kind(owner, owner->class_literal->kind) ||
        mc_entity_is_kind(owner, MC_ENTITY_CLASS_KIND_SLIME))
        return path_result(error, MC_ENTITY_INVALID_ARGUMENT, "Creature actual Path field owner");
    if (owner->mob->creature_path != owner->mob->creature_path_hold)
        return path_result(error, MC_ENTITY_STATE_ERROR, "Creature Path occurrence provenance");
    if (owner->mob->creature_path_hold != NULL) {
        mc_path_state state;
        if (!mc_path_inspect(owner->mob->creature_path_hold, &state) ||
            state.classes != owner->classes)
            return path_result(error, MC_ENTITY_STATE_ERROR, "Creature Path class namespace");
    }
    return MC_ENTITY_OK;
}

mc_entity_status mc_mob_path_get(mc_entity *owner, mc_path **out, mc_path_error *error) {
    mc_path_error_init(error);
    if (out == NULL)
        return path_result(error, MC_ENTITY_INVALID_ARGUMENT, "Creature Path output");
    const mc_entity_status status = path_slot(owner, error);
    if (status != MC_ENTITY_OK)
        return status;
    *out = owner->mob->creature_path;
    return path_result(error, MC_ENTITY_OK, NULL);
}

mc_entity_status mc_mob_path_store(mc_entity *owner, mc_path *value, mc_path_error *error) {
    mc_path_error_init(error);
    if (error != NULL)
        error->site = (mc_path_site){"ii", "a", "(Ldh;)V", 2};
    const mc_entity_status status = path_slot(owner, error);
    if (status != MC_ENTITY_OK)
        return status;
    if (value == owner->mob->creature_path) {
        if (error != NULL)
            error->site = (mc_path_site){"ii", "a", "(Ldh;)V", 5};
        return path_result(error, MC_ENTITY_OK, NULL);
    }
    if (value != NULL) {
        mc_path_state state;
        if (!mc_path_inspect(value, &state) || state.classes != owner->classes)
            return path_result(error, MC_ENTITY_INVALID_ARGUMENT, "Creature new Path namespace");
        const mc_path_status held = mc_path_retain(value, error);
        if (held != MC_PATH_OK) {
            if (error != NULL)
                error->site = (mc_path_site){"ii", "a", "(Ldh;)V", 2};
            return held == MC_PATH_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR;
        }
    }
    mc_path *old = owner->mob->creature_path_hold;
    owner->mob->creature_path = value;
    owner->mob->creature_path_hold = value;
    mc_path_release(old);
    if (error != NULL)
        error->site = (mc_path_site){"ii", "a", "(Ldh;)V", 5};
    return path_result(error, MC_ENTITY_OK, NULL);
}

mc_entity_status mc_mob_path_present_entered(mc_entity *owner, bool *out, mc_path_error *error) {
    mc_path *path;
    const mc_entity_status status = mc_mob_path_get(owner, &path, error);
    if (error != NULL)
        error->site = (mc_path_site){"ii", "F", "()Z", 1};
    if (status != MC_ENTITY_OK)
        return status;
    if (out == NULL)
        return path_result(error, MC_ENTITY_INVALID_ARGUMENT, "Creature Path presence output");
    *out = path != NULL;
    if (error != NULL)
        error->site = (mc_path_site){"ii", "F", "()Z", 12};
    return path_result(error, MC_ENTITY_OK, NULL);
}

void mc_mob_path_dispose(mc_entity *owner) {
    mc_path *path = owner->mob->creature_path_hold;
    owner->mob->creature_path_hold = NULL;
    owner->mob->creature_path = NULL;
    mc_path_release(path);
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
