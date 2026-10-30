#ifndef MC_ENTITY_MOB_INTERNAL_H
#define MC_ENTITY_MOB_INTERNAL_H
#include "../entity_internal.h"
#include "mc/entity/mob/construction.h"

typedef struct mc_mob_entity_state {
    mc_living_entity_fields living;
    const void *creature_path;
    mc_entity *creature_target;
    bool creature_attacking;
    int32_t monster_damage, creeper_a, creeper_b, slime_jump_delay;
    float slime_a, slime_b;
    unsigned int living_pc, creature_pc, monster_pc, constructor_pc, watcher_hook_pc;
    mc_mob_error failure;
} mc_mob_entity_state;

typedef struct mc_mob_entity_object {
    mc_entity entity;
    mc_mob_entity_state state;
} mc_mob_entity_object;

typedef struct mc_mob_class_namespace {
    mc_entity_class_phase skeleton_phase;
    unsigned int skeleton_pc;
    mc_item_stack *skeleton_a, *skeleton_candidate;
    size_t skeleton_stack_bytes;
    mc_mob_error failure;
} mc_mob_class_namespace;

bool mc_entity_is_mob_kind(mc_entity_class_kind);
bool mc_entity_has_mob(const mc_entity *);
mc_entity_status mc_mob_watcher_initialize(mc_entity *, mc_entity_error *);
void mc_mob_classes_dispose(mc_entity_classes *);
#endif
