#include "internal.h"
#include <stdlib.h>
#include <string.h>

typedef enum mob_entry {
    MOB_ENTRY_TICK,
    MOB_ENTRY_CREEPER_ATTACK,
    MOB_ENTRY_CREEPER_UNSEEN,
    MOB_ENTRY_MONSTER_ATTACK,
    MOB_ENTRY_WOLF_ACTION_TAIL,
    MOB_ENTRY_WOLF_TAME_TRUE,
    MOB_ENTRY_WOLF_TAME_FALSE,
    MOB_ENTRY_WOLF_ATTACK,
    MOB_ENTRY_SKELETON_ATTACK
} mob_entry;

/* Closed native entries share the exact same admission and lifetime guard.
 * Selecting a protected component executes no enclosing Source tick body. */
static mc_entity_status
run_entered(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
            const mc_mob_tick_limits *limits, mob_entry entry, mc_entity *target, float distance,
            mc_mob_tick_error *e, mc_mob_tick_child_workspace *child_workspace,
            mc_entity **owned_arrow, mt_skeleton_fire_observation *capture) {
    /* In-place World output must never expose a preceding Entity's state or
     * hurt receipt on any new attempt, even a rejected admission. */
    mc_mob_tick_error_init(e);
    if (capture != NULL)
        memset(capture, 0, sizeof(*capture));
    mc_player_action_context context = {0};
    context.travel.code_registry = registry;
    context.travel.trig = mc_math_classes_table(math);
    context.travel.math = math;
    context.travel.movement_limits = limits == NULL ? NULL : &limits->movement;
    context.travel.material_limits = limits == NULL ? NULL : &limits->environment.material;
    mc_mob_tick_scope s = {.entity = p,
                           .registry = registry,
                           .math = math,
                           .error = e,
                           .child_workspace = child_workspace,
                           .environment_limits = limits == NULL ? NULL : &limits->environment,
                           .action_context = &context,
                           .action_limits = limits == NULL ? NULL : &limits->action,
                           .path_limits = limits == NULL ? NULL : &limits->path,
                           .skeleton_arrow_output = owned_arrow,
                           .skeleton_capture = capture,
                           .max_work =
                               limits == NULL ? MC_MOB_TICK_DEFAULT_WORK : limits->max_work};
    bool held = false, registry_held = false, math_held = false, active = false;
    bool target_held = false;
    if (owned_arrow != NULL && *owned_arrow != NULL)
        mt_fail(&s, MC_ENTITY_INVALID_ARGUMENT, "Skeleton component empty Arrow occurrence output");
    else if (p == NULL || !mc_entity_has_mob(p) || !mc_entity_is_kind(p, p->class_literal->kind) ||
             p->classes->source != MC_ENTITY_CLIENT)
        mt_fail(&s, MC_ENTITY_STATE_ERROR, "Mob tick actual canonical client owner");
    else if ((entry == MOB_ENTRY_CREEPER_ATTACK || entry == MOB_ENTRY_CREEPER_UNSEEN) &&
             !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_CREEPER))
        mt_fail(&s, MC_ENTITY_STATE_ERROR, "Creeper component actual canonical receiver");
    else if (entry == MOB_ENTRY_MONSTER_ATTACK &&
             !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ZOMBIE))
        mt_fail(&s, MC_ENTITY_STATE_ERROR, "Monster component actual canonical Zombie receiver");
    else if (entry == MOB_ENTRY_SKELETON_ATTACK &&
             !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SKELETON))
        mt_fail(&s, MC_ENTITY_STATE_ERROR, "Skeleton component actual canonical receiver");
    else if ((entry == MOB_ENTRY_WOLF_ACTION_TAIL || entry == MOB_ENTRY_WOLF_TAME_TRUE ||
              entry == MOB_ENTRY_WOLF_TAME_FALSE || entry == MOB_ENTRY_WOLF_ATTACK) &&
             !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF))
        mt_fail(&s, MC_ENTITY_STATE_ERROR, "Wolf component actual canonical receiver");
    else if (p->phase != MC_ENTITY_READY)
        mt_fail(&s, MC_ENTITY_INVALID_PHASE, "Mob tick READY owner");
    else if (p->mob->tick_active)
        mt_fail(&s, MC_ENTITY_BUSY, "Mob tick already entered");
    else if (p->mob->pending_ray != NULL || p->mob->pending_path != NULL ||
             p->mob->pending_death != NULL || p->mob->pending_wolf_damage != NULL ||
             p->mob->pending_slime_removal != NULL) {
        e->state.cleanup_pending = true;
        mt_fail(&s, MC_ENTITY_BUSY, "Mob pending native child cleanup");
    } else {
        e->state.kind = p->class_literal->kind;
        const mc_entity_status status = mc_entity_retain(p);
        held = status == MC_ENTITY_OK;
        if (!held)
            mt_fail(&s, status, "Mob tick actual Entity lifetime hold");
    }
    if (held) {
        p->mob->tick_active = true;
        active = true;
        /* This is a native occurrence independent of the actor and any source
         * field. Creeper's genuinely unread nullable operand stays unchanged. */
        if ((entry == MOB_ENTRY_MONSTER_ATTACK || entry == MOB_ENTRY_WOLF_ATTACK ||
             entry == MOB_ENTRY_SKELETON_ATTACK) &&
            target != NULL) {
            if (target->classes != p->classes || target->holds == 0 ||
                target->class_literal == NULL ||
                !mc_entity_is_kind(target, target->class_literal->kind))
                mt_fail(&s, MC_ENTITY_STATE_ERROR,
                        entry == MOB_ENTRY_SKELETON_ATTACK
                            ? "Skeleton component actual target owner"
                        : entry == MOB_ENTRY_WOLF_ATTACK ? "Wolf component actual target owner"
                                                         : "Monster component actual target owner");
            else {
                const mc_entity_status status = mc_entity_retain(target);
                target_held = status == MC_ENTITY_OK;
                if (!target_held)
                    mt_fail(&s, status,
                            entry == MOB_ENTRY_SKELETON_ATTACK
                                ? "Skeleton component independent target lifetime hold"
                            : entry == MOB_ENTRY_WOLF_ATTACK
                                ? "Wolf component independent target lifetime hold"
                                : "Monster component independent target lifetime hold");
            }
        }
        if (e->status == MC_ENTITY_OK && registry != NULL) {
            const mc_registry_status status = mc_registry_classes_retain(
                registry, MC_REGISTRY_CLIENT, p->classes->profile, &e->registry);
            registry_held = status == MC_REGISTRY_OK;
            if (!registry_held)
                mt_fail(&s, status == MC_REGISTRY_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR,
                        "Mob tick Registry lifetime hold");
            else if (registry->entities != p->classes)
                mt_fail(&s, MC_ENTITY_STATE_ERROR, "Mob tick actual Registry Entity namespace");
        }
        if (e->status == MC_ENTITY_OK && math != NULL) {
            const mc_math_class_status status = mc_math_classes_retain(math, &e->math);
            math_held = status == MC_MATH_CLASS_OK;
            if (!math_held)
                mt_fail(&s, status == MC_MATH_CLASS_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR,
                        "Mob tick Math lifetime hold");
        }
        if (e->status == MC_ENTITY_OK) {
            const bool returned =
                entry == MOB_ENTRY_TICK              ? mt_concrete_w(&s)
                : entry == MOB_ENTRY_CREEPER_ATTACK  ? mt_creeper_attack(&s, target, distance)
                : entry == MOB_ENTRY_CREEPER_UNSEEN  ? mt_creeper_unseen(&s, target, distance)
                : entry == MOB_ENTRY_MONSTER_ATTACK  ? mt_monster_attack(&s, target, distance)
                : entry == MOB_ENTRY_SKELETON_ATTACK ? mt_skeleton_attack(&s, target, distance)
                : entry == MOB_ENTRY_WOLF_ATTACK     ? mt_wolf_attack(&s, target, distance)
                : entry == MOB_ENTRY_WOLF_ACTION_TAIL
                    ? mt_wolf_action_tail(&s)
                    : mt_wolf_tame_entered(&s, entry == MOB_ENTRY_WOLF_TAME_TRUE);
            if (returned)
                e->state.returned = true;
        }
    }
    if (math_held) {
        mc_math_classes *occurrence = math;
        mc_math_class_error cleanup = {0};
        if (mc_math_classes_release(&occurrence, &cleanup) != MC_MATH_CLASS_OK) {
            e->math = cleanup;
            mt_fail(&s, MC_ENTITY_STATE_ERROR, "Mob tick Math native occurrence retirement");
        }
    }
    if (registry_held && !mc_registry_runtime_borrow_release(registry))
        mt_fail(&s, MC_ENTITY_STATE_ERROR, "Mob tick Registry native occurrence retirement");
    if (active)
        p->mob->tick_active = false;
    if (target_held)
        mc_entity_release(target);
    if (held && !s.hold_transferred)
        mc_entity_release(p);
    return e->status;
}

/* Public/protected callers retain the previous independent heap/output
 * contract, including NULL output and allocation-refusal diagnostics. */
static mc_entity_status run_captured(mc_entity *p, mc_registry_classes *registry,
                                     mc_math_classes *math, const mc_mob_tick_limits *limits,
                                     mob_entry entry, mc_entity *target, float distance,
                                     mc_mob_tick_error *out, mc_entity **owned_arrow,
                                     mt_skeleton_fire_observation *capture) {
    if (capture != NULL)
        memset(capture, 0, sizeof(*capture));
    mc_mob_tick_error *e = malloc(sizeof(*e));
    if (e == NULL) {
        mc_mob_tick_error_init(out);
        if (out != NULL) {
            out->status = MC_ENTITY_ALLOCATION_FAILED;
            out->operation = "Mob tick diagnostic workspace";
            out->native_policy = true;
        }
        return MC_ENTITY_ALLOCATION_FAILED;
    }
    const mc_entity_status status = run_entered(p, registry, math, limits, entry, target, distance,
                                                e, NULL, owned_arrow, capture);
    if (out != NULL)
        *out = *e;
    free(e);
    return status;
}

static mc_entity_status run(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                            const mc_mob_tick_limits *limits, mob_entry entry, mc_entity *target,
                            float distance, mc_mob_tick_error *out) {
    return run_captured(p, registry, math, limits, entry, target, distance, out, NULL, NULL);
}

mc_entity_status mt_tick_in_place(mc_entity *p, mc_registry_classes *registry,
                                  mc_math_classes *math, const mc_mob_tick_limits *limits,
                                  mc_mob_tick_error *working,
                                  mc_mob_tick_child_workspace *scratch) {
    if (working == NULL)
        return run(p, registry, math, limits, MOB_ENTRY_TICK, NULL, 0.0f, NULL);
    return run_entered(p, registry, math, limits, MOB_ENTRY_TICK, NULL, 0.0f, working, scratch,
                       NULL, NULL);
}

mc_entity_status mc_mob_tick(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                             const mc_mob_tick_limits *limits, mc_mob_tick_error *out) {
    return run(p, registry, math, limits, MOB_ENTRY_TICK, NULL, 0.0f, out);
}

mc_entity_status mt_creeper_component(mc_entity *p, mc_registry_classes *registry,
                                      mc_math_classes *math, const mc_mob_tick_limits *limits,
                                      mc_entity *target, float distance, bool unseen,
                                      mc_mob_tick_error *out) {
    return run(p, registry, math, limits,
               unseen ? MOB_ENTRY_CREEPER_UNSEEN : MOB_ENTRY_CREEPER_ATTACK, target, distance, out);
}

mc_entity_status mt_monster_component(mc_entity *p, mc_registry_classes *registry,
                                      mc_math_classes *math, const mc_mob_tick_limits *limits,
                                      mc_entity *target, float distance, mc_mob_tick_error *out) {
    return run(p, registry, math, limits, MOB_ENTRY_MONSTER_ATTACK, target, distance, out);
}

mc_entity_status mt_skeleton_component(mc_entity *p, mc_registry_classes *registry,
                                       mc_math_classes *math, const mc_mob_tick_limits *limits,
                                       mc_entity *target, float distance, mc_entity **owned_arrow,
                                       mt_skeleton_fire_observation *capture,
                                       mc_mob_tick_error *out) {
    return run_captured(p, registry, math, limits, MOB_ENTRY_SKELETON_ATTACK, target, distance, out,
                        owned_arrow, capture);
}

mc_entity_status mt_wolf_attack_component(mc_entity *p, mc_registry_classes *registry,
                                          mc_math_classes *math, const mc_mob_tick_limits *limits,
                                          mc_entity *target, float distance,
                                          mc_mob_tick_error *out) {
    return run(p, registry, math, limits, MOB_ENTRY_WOLF_ATTACK, target, distance, out);
}

/* The selected Source range begins after bg.o has returned. Its returned bit
 * never credits the unexecuted superclass or entry0/full GI.o/whole Mob tick. */
mc_entity_status mt_wolf_action_component(mc_entity *p, mc_registry_classes *registry,
                                          mc_math_classes *math, const mc_mob_tick_limits *limits,
                                          mc_mob_tick_error *out) {
    return run(p, registry, math, limits, MOB_ENTRY_WOLF_ACTION_TAIL, NULL, 0.0f, out);
}

mc_entity_status mt_wolf_tame_component(mc_entity *p, mc_registry_classes *registry,
                                        mc_math_classes *math, const mc_mob_tick_limits *limits,
                                        bool value, mc_mob_tick_error *out) {
    return run(p, registry, math, limits,
               value ? MOB_ENTRY_WOLF_TAME_TRUE : MOB_ENTRY_WOLF_TAME_FALSE, NULL, 0.0f, out);
}
