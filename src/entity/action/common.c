#include "action_internal.h"

void mc_player_action_error_init(mc_player_action_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->nearby_index = SIZE_MAX;
    mc_player_travel_error_init(&e->travel);
    e->registry = e->travel.registry;
    e->inventory = mc_inventory_tick_empty_error();
    e->bounds = e->travel.movement.bounds;
    mc_entity_environment_error_init(&e->environment);
    mc_world_collision_error_init(&e->collision);
    mc_world_nearby_error_init(&e->nearby);
}

bool mc_action_fail(mc_player_action_error *e, mc_entity_status status, const char *op,
                    unsigned int pc) {
    e->status = status;
    e->operation = op;
    if (e->source_method != NULL && strcmp(e->source_method, "o()V") == 0) {
        if (e->source_class != NULL && strcmp(e->source_class, "ls") == 0)
            e->living_bytecode = pc;
        else if (e->source_class != NULL && strcmp(e->source_class, "gs") == 0)
            e->player_bytecode = pc;
    }
    return false;
}

bool mc_action_enter(mc_entity *p, mc_player_action_error *e) {
    if (p == NULL)
        return mc_action_fail(e, MC_ENTITY_INVALID_ARGUMENT, "Action Player receiver", 0);
    if (p->phase != MC_ENTITY_READY)
        return mc_action_fail(e, MC_ENTITY_INVALID_PHASE, "Action READY Player", 0);
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) || p->local_player == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Action canonical dc owner", 0);
    const mc_entity_status held = mc_entity_retain(p);
    return held == MC_ENTITY_OK || mc_action_fail(e, held, "Action Player lifetime hold", 0);
}

bool mc_action_fp(mc_player_action_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_action_fail(e, MC_ENTITY_UNSUPPORTED_FP, "Action FP admission", pc);
}

bool mc_action_work(mc_player_action_error *e, unsigned int pc) {
    if (e->loop_work == e->max_loop_work)
        return mc_action_fail(e, MC_ENTITY_LIMIT, "Action pending loop-work ceiling", pc);
    ++e->loop_work;
    return true;
}

mc_entity_status mc_action_finish(mc_player_action_error *frame, mc_player_action_error *out) {
    const mc_entity_status status = frame->status;
    if (status == MC_ENTITY_OK)
        mc_player_action_error_init(frame);
    if (out != NULL)
        *out = *frame;
    return status;
}
