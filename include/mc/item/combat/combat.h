#ifndef MC_ITEM_COMBAT_COMBAT_H
#define MC_ITEM_COMBAT_COMBAT_H

#include "mc/entity/player/statistics.h"
#include "mc/item/registry/runtime.h"

typedef struct mc_item_combat_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_item_combat_site;

typedef struct mc_item_combat_error {
    mc_entity_status status;
    const char *operation;
    mc_item_combat_site site, child, dependency, watcher;
    bool native_policy, source_exception, source_error;
    const char *exception_class;
    mc_registry_error registry;
    mc_item_stack_status stack;
    mc_player_statistic_error statistic;
    bool durability_entered, damage_stored, break_selected;
    bool statistic_entered, statistic_returned, count_read, count_stored;
    bool count_clamped, damage_reset, saddle_stored;
    int32_t amount, damage_before, damage_after, damageability_maximum, maximum;
    int32_t count_before, count_after;
} mc_item_combat_error;

void mc_item_combat_error_init(mc_item_combat_error *);

/* Actual client virtual GM.a(SN)I and GM.a(IZ,LS,LS)Z, resolved across all38
 * represented receiver kinds. GM returns1/false without reading arguments;
 * DQ/QD read their distinct damage fields and hit through actual IZ durability
 * with amount2/1. SA performs its actual Pig saddle setter/count decrement and
 * returns true even for an already saddled or non-Pig target. No Item-used
 * statistic, Inventory clear/dirty store or source initializer is added here.
 * These belong to the enclosing Stack/Player wrappers when actually reached.
 *
 * Caller retains the actual quiescent client Registry and every referenced
 * Item/Stack/Entity owner exclusively through the call. Source phases are
 * transparent; native admission checks no constructor/READY flag. Arguments
 * unused by the selected original body may be NULL. A foreign class-loader
 * target is an ordinary false instanceof result in SA, not an adopted owner.
 * No allocation, pending work or borrowed error pointer is returned. Errors
 * contain the exact completed mutation prefix; failure never rolls it back.
 * out is required and preserved on failure; error is optional. All outputs,
 * errors and inputs are disjoint from each other and live owner storage. */
mc_entity_status mc_item_combat_damage(mc_registry_classes *, const mc_item *, mc_entity *target,
                                       int32_t *out, mc_item_combat_error *);
mc_entity_status mc_item_combat_hit(mc_registry_classes *, const mc_item *, mc_item_stack *,
                                    mc_entity *target, mc_entity *attacker, bool *out,
                                    mc_item_combat_error *);

/* Actual IZ.a(int,SN)V, including separate e()/j() Item-array captures, wrapped
 * damage store15, strict damage>maximum, Player broken-statistic49 BEFORE
 * count--59/negative clamp71/damage-reset76. Null/non-Player attackers skip
 * that statistic. There is no break sound/particle callback in this source.
 * Registry/Stack/Entity lifetime and phase contracts are as above. A failed
 * child preserves damage, count and already completed statistic mutations;
 * optional error does not suppress source work or require later cleanup. */
mc_entity_status mc_item_stack_combat_damage(mc_registry_classes *, mc_item_stack *, int32_t amount,
                                             mc_entity *attacker, mc_item_combat_error *);

#endif
