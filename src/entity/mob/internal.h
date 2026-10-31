#ifndef MC_ENTITY_MOB_INTERNAL_H
#define MC_ENTITY_MOB_INTERNAL_H
#include "../entity_internal.h"
#include "mc/entity/mob/construction.h"

typedef struct mc_mob_entity_state {
    mc_living_entity_fields living;
    mc_path *creature_path;
    mc_entity *creature_target;
    /* Native owning occurrences for the source LS/II reference slots. */
    mc_entity *look_target_hold, *creature_target_hold;
    mc_path *creature_path_hold;
    /* Intrusive native destruction queue; used only after the final hold ends. */
    mc_entity *retirement_next;
    struct mc_mob_ray_pending *pending_ray;
    struct mc_world_path_call *pending_path;
    bool tick_active;
    bool creature_attacking;
    int32_t monster_damage, creeper_a, creeper_b, slime_jump_delay;
    float slime_a, slime_b;
    mc_mob_chicken_fields chicken;
    mc_mob_wolf_fields wolf;
    mc_mob_squid_fields squid;
    unsigned int living_pc, creature_pc, monster_pc, constructor_pc, watcher_hook_pc;
    unsigned int animal_pc, water_creature_pc;
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
    mc_entity_class_phase sheep_phase;
    unsigned int sheep_pc;
    mc_mob_sheep_colors *sheep_a, *sheep_candidate;
    mc_mob_sheep_color_row *sheep_row_candidate;
    size_t sheep_rows_published, sheep_array_bytes;
    mc_mob_error sheep_failure;
} mc_mob_class_namespace;

bool mc_entity_is_mob_kind(mc_entity_class_kind);
bool mc_entity_has_mob(const mc_entity *);
mc_entity_status mc_mob_watcher_initialize(mc_entity *, mc_entity_error *);
void mc_mob_classes_dispose(mc_entity_classes *);
#endif
