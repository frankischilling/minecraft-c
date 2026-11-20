#ifndef MC_ENTITY_PLAYER_ATTACK_H
#define MC_ENTITY_PLAYER_ATTACK_H

#include "mc/entity/movement/effects.h"
#include "mc/entity/player/player.h"
#include "mc/inventory/player_current.h"
#include "mc/item/combat/combat.h"
#include "mc/world/runtime/collision/class_entities.h"

typedef struct mc_player_attack_call mc_player_attack_call;

typedef struct mc_player_attack_error {
    mc_entity_status status;
    const char *operation;
    mc_player_site site, child;
    bool native_policy, source_exception;
    const char *exception_class;
    mc_inventory_current_error inventory;
    mc_inventory_error slots;
    mc_registry_error registry;
    mc_player_statistic_error statistic;
    mc_item_combat_error combat;
    mc_item_stack_status stack;
    mc_entity_error entity;
    mc_entity_bounds_error bounds;
    mc_world_nearby_error nearby;
    mc_entity_effect_error effect;
} mc_player_attack_error;

typedef struct mc_player_attack_state {
    mc_entity *player, *target; /* Borrowed while the call remains live. */
    bool active, returned, damage_returned, damage_result, falling_bonus;
    bool fresh_stack_read, target_alive, alert_entered, alert_returned, statistic_returned;
    bool item_hit_returned, item_hit_result, item_used_returned;
    bool exhausted_stack, destroy_returned, clear_returned;
    int32_t cleared_slot;
    uintptr_t clear_inventory_identity;
    int32_t damage;
    unsigned int source_pc;
    size_t wolves_examined, wolves_alerted;
    uintptr_t first_stack_identity, fresh_stack_identity;
    const mc_player_attack_error *error;
} mc_player_attack_state;

/* Actual client GS.d(SN), reached through OB.b(GS,SN). Operands are captured in
 * source order: Inventory damage, falling bonus, target hurt, fresh held Stack,
 * alive/ally alert, then damage-dealt statistic. Animal hurt/death and Item
 * publication use the actual victim and existing World owners. Held Items use
 * actual GM/DQ/QD/SA bodies, separate Stack lookups and reached statistics;
 * exhausted Stacks enter the real selected Inventory clear continuation.
 * Native owners remain live/exclusive through checked retirement. out must be
 * fresh and disjoint; a returned failed call owns its exact prefix even when
 * error is NULL. No retry replays source work. Success also requires retirement.
 * Keep the Player's borrowed App and all source fields live through the call. */
mc_entity_status mc_local_player_attack_begin(mc_registry_classes *, mc_math_classes *,
                                              mc_entity *player, mc_entity *target,
                                              mc_player_attack_call **out,
                                              mc_player_attack_error *error);
bool mc_local_player_attack_inspect(const mc_player_attack_call *, mc_player_attack_state *);
/* Retire only native occurrences and this call's newly produced pending death. An active
 * call or refused death cleanup preserves the pointer for retry. Published
 * Items, health, score, statistics and Wolf targets are never rolled back. */
mc_entity_status mc_local_player_attack_retire(mc_player_attack_call **,
                                               mc_player_attack_error *error);

#endif
