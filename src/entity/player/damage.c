#include "damage_internal.h"

#include "../living/fields_internal.h"
#include "mc/core/java/java.h"
#include "player_internal.h"

static void at(mc_entity_effect_error *e, uint16_t pc) {
    e->living_damage.player_health.pc = pc;
    e->living_damage.child_site.class_name = "gs";
    e->living_damage.child_site.method = "b";
    e->living_damage.child_site.descriptor = "(I)V";
    e->living_damage.child_site.pc = pc;
}

static bool fail(mc_entity_effect_error *e, mc_entity_status status, mc_entity_effect_cause cause,
                 const char *operation) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    return false;
}

static bool null_inventory(mc_entity_effect_error *e) {
    e->living_damage.native_policy = false;
    e->living_damage.source_exception = true;
    e->living_damage.exception_class = "java/lang/NullPointerException";
    return fail(e, MC_ENTITY_STATE_ERROR, MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE,
                "Player armor null Inventory invocation receiver");
}

static bool inventory_result(mc_entity_effect_error *e, mc_inventory_armor_status status) {
    if (status == MC_INVENTORY_ARMOR_OK)
        return true;
    e->living_damage.native_policy = e->armor.native_policy;
    e->living_damage.source_exception = e->armor.source_exception;
    e->living_damage.source_error = e->armor.source_error;
    e->living_damage.exception_class = e->armor.exception_class;
    mc_entity_status result = MC_ENTITY_STATE_ERROR;
    mc_entity_effect_cause cause = MC_ENTITY_EFFECT_CAUSE_INVENTORY;
    switch (status) {
        case MC_INVENTORY_ARMOR_BUSY:
            result = MC_ENTITY_BUSY;
            break;
        case MC_INVENTORY_ARMOR_LIMIT:
            result = MC_ENTITY_LIMIT;
            break;
        case MC_INVENTORY_ARMOR_ALLOCATION_FAILED:
            result = MC_ENTITY_ALLOCATION_FAILED;
            break;
        case MC_INVENTORY_ARMOR_NULL_REFERENCE:
            cause = MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE;
            break;
        case MC_INVENTORY_ARMOR_REGISTRY_FAILED:
            cause = MC_ENTITY_EFFECT_CAUSE_REGISTRY;
            e->registry = e->armor.registry;
            break;
        case MC_INVENTORY_ARMOR_COMBAT_FAILED:
            result = e->armor.combat.status;
            if (e->armor.combat.statistic_entered && !e->armor.combat.statistic_returned)
                cause = MC_ENTITY_EFFECT_CAUSE_STATISTIC;
            break;
        case MC_INVENTORY_ARMOR_INVALID_ARGUMENT:
        case MC_INVENTORY_ARMOR_OWNER:
            cause = MC_ENTITY_EFFECT_CAUSE_OWNER;
            break;
        default:
            break;
    }
    return fail(e, result, cause, e->armor.operation);
}

bool mc_player_health_damage_entered(mc_entity *p, mc_registry_classes *registry, int32_t amount,
                                     mc_entity_effect_error *e) {
    mc_entity_player_health_damage_state *receipt = &e->living_damage.player_health;
    receipt->entered = true;
    receipt->amount = amount;
    at(e, 6);
    mc_inventory_player *inventory = p->local_player->fields.player.c;
    if (inventory == NULL)
        return null_inventory(e);
    int32_t armor;
    if (!inventory_result(e,
                          mc_inventory_player_armor_value(inventory, registry, &armor, &e->armor)))
        return false;
    receipt->value_returned = true;
    receipt->armor = armor;
    const int32_t factor = mc_java_i32_sub(25, armor);
    receipt->factor = factor;
    const int32_t remainder = p->local_player->fields.player.private_bO;
    receipt->remainder_before = remainder;
    const int32_t combined = mc_java_i32_add(mc_java_i32_mul(amount, factor), remainder);
    receipt->combined = combined;
    at(e, 25);
    inventory = p->local_player->fields.player.c;
    if (inventory == NULL)
        return null_inventory(e);
    if (!inventory_result(e,
                          mc_inventory_player_armor_wear(inventory, registry, amount, &e->armor)))
        return false;
    receipt->wear_returned = true;
    /* Both operations have a fixed positive divisor; C and Java have the
     * same truncation/remainder here, including negative wrapped operands. */
    const int32_t reduced = combined / 25;
    receipt->reduced = reduced;
    at(e, 38);
    p->local_player->fields.player.private_bO = combined % 25;
    receipt->remainder_after = p->local_player->fields.player.private_bO;
    receipt->remainder_stored = true;
    at(e, 43);
    /* The real invokespecial LS.b reloads Y after both Inventory children. */
    mc_living_entity_fields *v = mc_entity_living_fields(p);
    const int32_t before = v->Y;
    const int32_t after = mc_java_i32_sub(before, reduced);
    v->Y = after;
    e->living_damage.health_before = before;
    e->living_damage.health_after = after;
    e->living_damage.health_stored = true;
    e->living_damage.health_pc = 10;
    at(e, 46);
    receipt->returned = true;
    return true;
}
