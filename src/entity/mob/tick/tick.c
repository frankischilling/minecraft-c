#include "internal.h"
#include <stdlib.h>
#include <string.h>

mc_entity_status mc_mob_tick(mc_entity *p, mc_registry_classes *registry, mc_math_classes *math,
                             const mc_mob_tick_limits *limits, mc_mob_tick_error *out) {
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
    mc_mob_tick_error_init(e);
    mc_player_action_context context = {0};
    context.travel.code_registry = registry;
    context.travel.trig = mc_math_classes_table(math);
    context.travel.movement_limits = limits == NULL ? NULL : &limits->movement;
    context.travel.material_limits = limits == NULL ? NULL : &limits->environment.material;
    mc_mob_tick_scope s = {.entity = p,
                           .registry = registry,
                           .math = math,
                           .error = e,
                           .environment_limits = limits == NULL ? NULL : &limits->environment,
                           .action_context = &context,
                           .action_limits = limits == NULL ? NULL : &limits->action,
                           .path_limits = limits == NULL ? NULL : &limits->path,
                           .max_work =
                               limits == NULL ? MC_MOB_TICK_DEFAULT_WORK : limits->max_work};
    bool held = false, registry_held = false, math_held = false, active = false;
    if (p == NULL || !mc_entity_has_mob(p) || !mc_entity_is_kind(p, p->class_literal->kind) ||
        p->classes->source != MC_ENTITY_CLIENT)
        mt_fail(&s, MC_ENTITY_STATE_ERROR, "Mob tick actual canonical client owner");
    else if (p->phase != MC_ENTITY_READY)
        mt_fail(&s, MC_ENTITY_INVALID_PHASE, "Mob tick READY owner");
    else if (p->mob->tick_active)
        mt_fail(&s, MC_ENTITY_BUSY, "Mob tick already entered");
    else if (p->mob->pending_ray != NULL || p->mob->pending_path != NULL) {
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
        if (registry != NULL) {
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
        if (e->status == MC_ENTITY_OK && mt_concrete_w(&s))
            e->state.returned = true;
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
    if (held && !s.hold_transferred)
        mc_entity_release(p);
    const mc_entity_status status = e->status;
    if (out != NULL)
        *out = *e;
    free(e);
    return status;
}
