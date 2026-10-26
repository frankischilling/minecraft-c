#include "tick_internal.h"

void mc_entity_tick_error_init(mc_entity_tick_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->watcher_index = -1;
    mc_entity_environment_error_init(&e->environment);
    mc_entity_effect_error_init(&e->effect);
    mc_world_movement_error_init(&e->world);
    e->registry = e->environment.registry;
    e->solid.registry = e->registry;
    e->material.registry = e->registry;
}

bool mc_tick_fail(mc_entity_tick_error *e, mc_entity_status status, const char *operation,
                  unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->bytecode = pc;
    return false;
}

bool mc_tick_enter(mc_entity *entity, mc_entity_tick_error *e) {
    if (entity == NULL)
        return mc_tick_fail(e, MC_ENTITY_INVALID_ARGUMENT, "Entity tick receiver", 0);
    if (entity->phase != MC_ENTITY_READY)
        return mc_tick_fail(e, MC_ENTITY_INVALID_PHASE, "Entity tick READY owner", 0);
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR,
                            "Entity tick canonical LocalPlayer virtual domain", 0);
    const mc_entity_status status = mc_entity_retain(entity);
    return status == MC_ENTITY_OK || mc_tick_fail(e, status, "Entity tick lifetime hold", 0);
}

bool mc_tick_fp(mc_entity_tick_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_tick_fail(e, MC_ENTITY_UNSUPPORTED_FP, "Entity tick FP admission", pc);
}

bool mc_tick_effect(mc_entity_tick_error *e, mc_entity_status status, const char *operation,
                    unsigned int pc) {
    return status == MC_ENTITY_OK || mc_tick_fail(e, status, operation, pc);
}

bool mc_tick_world(mc_entity_tick_error *e, mc_world_storage_status status, const char *operation,
                   unsigned int pc) {
    return status == MC_WORLD_STORAGE_OK ||
           mc_tick_fail(e,
                        status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_WORLD_FAILED,
                        operation, pc);
}

bool mc_tick_registry(mc_entity_tick_error *e, mc_registry_status status, const char *operation,
                      unsigned int pc) {
    return status == MC_REGISTRY_OK ||
           mc_tick_fail(e,
                        status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
                        : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                                                                  : MC_ENTITY_STATE_ERROR,
                        operation, pc);
}

bool mc_tick_code(mc_entity *entity, mc_registry_classes *classes, mc_entity_tick_error *e,
                  unsigned int pc) {
    return (classes != NULL && classes->entities == entity->classes) ||
           mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Entity tick actual code Registry namespace", pc);
}

mc_entity_status mc_tick_finish(mc_entity_tick_error *frame, mc_entity_tick_error *out) {
    const mc_entity_status status = frame->status;
    if (status == MC_ENTITY_OK)
        mc_entity_tick_error_init(frame);
    if (out != NULL)
        *out = *frame;
    return status;
}
