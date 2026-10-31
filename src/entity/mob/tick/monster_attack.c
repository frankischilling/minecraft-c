#include "internal.h"

static void at(mc_mob_tick_scope *s, unsigned int pc) {
    mt_at(s, "gz", "a", "(Lsn;F)V", pc);
}

static bool returned(mc_mob_tick_scope *s) {
    at(s, 65);
    s->error->state.attack.returned = true;
    return true;
}

/* The enclosing real Mob runner owns actor/target occurrences. This prefix
 * invokes no child or callback and acquires no substitute World/Math service.
 * Nullable target/bounds fields are read only at their actual GZ operations. */
bool mt_monster_attack(mc_mob_tick_scope *s, mc_entity *target, float distance) {
    s->error->state.attack = (mc_mob_tick_attack_arguments){
        .stored = true, .target_identity = (uintptr_t)target, .distance = distance};
    at(s, 1);
    if (mt_fields(s)->ae > 0)
        return returned(s);
    at(s, 9);
    if (!mt_fp(s))
        return false;
    if (!(distance < 2.0f)) /* Actual fcmpg9/ifge10: NaN takes return65. */
        return returned(s);

    at(s, 14);
    if (target == NULL)
        return mt_null(s, "Monster attack null target bounds field receiver");
    mc_entity_bounds *target_bounds = target->bounds;
    at(s, 17);
    if (target_bounds == NULL)
        return mt_null(s, "Monster attack null target maxY field receiver");
    const double target_max_y = target_bounds->max_y;
    mc_entity_bounds *self_bounds = s->entity->bounds; /* Fresh field21. */
    at(s, 24);
    if (self_bounds == NULL)
        return mt_null(s, "Monster attack null actor minY field receiver");
    const double self_min_y = self_bounds->min_y;
    at(s, 27);
    if (!(target_max_y > self_min_y)) /* Actual dcmpl27/ifle28. */
        return returned(s);

    target_bounds = target->bounds; /* Fresh field32, not the first capture. */
    at(s, 35);
    if (target_bounds == NULL)
        return mt_null(s, "Monster attack null target minY field receiver");
    const double target_min_y = target_bounds->min_y;
    self_bounds = s->entity->bounds; /* Fresh field39. */
    at(s, 42);
    if (self_bounds == NULL)
        return mt_null(s, "Monster attack null actor maxY field receiver");
    const double self_max_y = self_bounds->max_y;
    at(s, 45);
    if (!(target_min_y < self_max_y)) /* Actual dcmpg45/ifge46. */
        return returned(s);

    mt_fields(s)->ae = 20; /* Actual putfield52, before hurt operands. */
    const int32_t amount = s->entity->mob->monster_damage; /* Fresh GZ.c58. */
    at(s, 61);
    s->error->child = MC_MOB_TICK_CHILD_EFFECT;
    mc_entity_effect_error *child = &s->error->detail.effect;
    mc_entity_effect_error_init(child);
    child->status = MC_ENTITY_STATE_ERROR;
    child->cause = MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL;
    child->operation = "Monster actual target hurt invocation dependency";
    child->source_class = "sn";
    child->damage_arguments_stored = true;
    child->damage_source_identity = (uintptr_t)s->entity;
    child->damage_amount = amount;
    s->error->child_site = (mc_mob_site){"sn", "a", "(Lsn;I)Z", 0};
    /* Invocation operands are complete. No hurt body, boolean result, pop64
     * or return65 is credited while the concrete damage producer is absent. */
    return mt_fail(s, MC_ENTITY_STATE_ERROR, child->operation);
}
