#include "mc/inventory/runtime/armor.h"

#include "../../block/registry/registry_internal.h"
#include "../../item/registry/registry_internal.h"
#include "../../item/render/internal.h"
#include "../../item/stack/source_internal.h"
#include "../internal.h"

#include <string.h>

static mc_inventory_site armor_site(const char *method, const char *descriptor, int32_t pc) {
    return (mc_inventory_site){"ix", method, descriptor, pc};
}

void mc_inventory_armor_error_init(mc_inventory_armor_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->native_policy = true;
    e->slot = -1;
    e->site.pc = e->child.pc = e->dependency.pc = -1;
    mc_item_combat_error_init(&e->combat);
    e->registry = e->combat.registry;
}

static bool armor_fail(mc_inventory_armor_error *e, mc_inventory_armor_status status,
                       const char *operation) {
    e->status = status;
    e->operation = operation;
    e->native_policy = !e->source_exception && !e->source_error;
    return false;
}

static bool armor_source(mc_inventory_armor_error *e, mc_inventory_armor_status status,
                         const char *exception, const char *operation) {
    e->source_exception = true;
    e->exception_class = exception;
    return armor_fail(e, status, operation);
}

static bool armor_null(mc_inventory_armor_error *e, const char *operation) {
    return armor_source(e, MC_INVENTORY_ARMOR_NULL_REFERENCE, "java/lang/NullPointerException",
                        operation);
}

static void armor_at(mc_inventory_armor_error *e, int32_t pc) {
    e->site.pc = pc;
    e->child = e->dependency = (mc_inventory_site){.pc = -1};
}

static bool armor_enter(mc_inventory_player *inventory, const void *out, const char *method,
                        const char *descriptor, mc_inventory_armor_error *e) {
    if (inventory == NULL || out == NULL || !inv_side_valid(inventory->side))
        return armor_fail(e, MC_INVENTORY_ARMOR_INVALID_ARGUMENT,
                          "Armor actual native owner/output");
    if (inventory->active || inventory->terminal_sealed)
        return armor_fail(e, MC_INVENTORY_ARMOR_BUSY, "Armor active/terminal Inventory owner");
    if (inventory->side != MC_REGISTRY_CLIENT)
        return armor_fail(e, MC_INVENTORY_ARMOR_UNIMPLEMENTED_DEPENDENCY,
                          "Armor actual client IX source scope");
    inventory->active = true;
    e->source_entered = true;
    e->site = armor_site(method, descriptor, 0);
    return true;
}

static mc_inventory_armor_status armor_finish(mc_inventory_player *inventory, bool entered,
                                              const mc_inventory_armor_error *e,
                                              mc_inventory_armor_error *out) {
    if (entered)
        inventory->active = false;
    if (out != NULL)
        *out = *e;
    return e->status;
}

/* Only the constructor-owned four-cell array is represented. Each call captures
 * the current field anew; no array, cell or owner is substituted across a child. */
static bool armor_array(mc_inventory_player *inventory, int32_t pc, mc_item_stack ***out,
                        mc_inventory_armor_error *e) {
    armor_at(e, pc);
    mc_item_stack **slots = inventory->armor_slots;
    if (slots == NULL)
        return armor_null(e, "Armor actual null IX.b array receiver");
    *out = slots;
    return true;
}

static bool armor_cell(mc_inventory_player *inventory, int32_t pc, int32_t slot,
                       mc_item_stack **out, mc_inventory_armor_error *e) {
    mc_item_stack **slots;
    if (!armor_array(inventory, pc, &slots, e))
        return false;
    *out = slots[slot];
    return true;
}

static bool armor_stack_result(mc_inventory_armor_error *e, mc_item_stack_status status,
                               const char *operation) {
    e->stack = status;
    return status == MC_ITEM_STACK_OK ||
           armor_fail(e,
                      status == MC_ITEM_STACK_LIMIT ? MC_INVENTORY_ARMOR_LIMIT
                      : status == MC_ITEM_STACK_ALLOCATION_FAILED
                          ? MC_INVENTORY_ARMOR_ALLOCATION_FAILED
                          : MC_INVENTORY_ARMOR_STACK_FAILED,
                      operation);
}

static bool armor_registry_failed(mc_registry_classes *classes, mc_inventory_armor_error *e) {
    mc_item_render_throwable throwable;
    mc_item_render_registry_classification(classes, &e->registry, &throwable, &e->exception_class);
    e->source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    e->source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    return armor_fail(e,
                      e->registry.status == MC_REGISTRY_LIMIT ? MC_INVENTORY_ARMOR_LIMIT
                      : e->registry.status == MC_REGISTRY_ALLOCATION_FAILED
                          ? MC_INVENTORY_ARMOR_ALLOCATION_FAILED
                      : e->registry.status == MC_REGISTRY_BUSY ? MC_INVENTORY_ARMOR_BUSY
                                                               : MC_INVENTORY_ARMOR_REGISTRY_FAILED,
                      e->registry.message);
}

static bool armor_item_owner(mc_registry_classes *classes, const mc_item *item,
                             mc_inventory_armor_error *e) {
    if (item != NULL &&
        (item->classes != classes || (unsigned int)item->kind >= (unsigned int)MC_ITEM_KIND_COUNT))
        return armor_fail(e, MC_INVENTORY_ARMOR_OWNER, "Armor actual Item storage namespace");
    return true;
}

/* Actual IZ.a()/j(): capture GM.c0, then read this.c4, then aaload7.
 * A reached receiver hold is native lifetime only, accepting every Stack phase. */
static bool armor_item(mc_registry_classes *classes, mc_item_stack *stack, const char *method,
                       const mc_item **out, int32_t *maximum, mc_inventory_armor_error *e) {
    if (stack == NULL)
        return armor_null(e, "Armor null Stack invocation receiver");
    if (!armor_stack_result(e, mc_item_stack_retain(stack), "Armor Stack invocation occurrence"))
        return false;
    bool ok = false;
    const bool max = maximum != NULL;
    e->child = (mc_inventory_site){"iz", method, max ? "()I" : "()Lgm;", 0};
    if (classes != NULL && classes->options.side != MC_REGISTRY_CLIENT) {
        (void)armor_fail(e, MC_INVENTORY_ARMOR_UNIMPLEMENTED_DEPENDENCY,
                         "Armor reached actual client Item code Registry");
        goto done;
    }
    const mc_registry_item_array *array = NULL;
    if (mc_registry_item_c_get(classes, &array, &e->registry) != MC_REGISTRY_OK) {
        (void)armor_registry_failed(classes, e);
        goto done;
    }
    e->child.pc = 4;
    int32_t id;
    if (!armor_stack_result(e, mc_item_stack_source_id_get(stack, &id),
                            "Armor actual Stack id field"))
        goto done;
    e->child.pc = 7;
    const mc_item *item = NULL;
    if (mc_registry_item_c_at(classes, array, id, &item, &e->registry) != MC_REGISTRY_OK) {
        (void)armor_registry_failed(classes, e);
        goto done;
    }
    if (max) {
        e->child.pc = 8;
        if (item == NULL) {
            (void)armor_null(e, "Armor null maximum Item invocation receiver");
            goto done;
        }
        if (!armor_item_owner(classes, item, e))
            goto done;
        /* All represented Item kinds inherit the actual GM.f field1/return4. */
        e->dependency = (mc_inventory_site){"gm", "f", "()I", 1};
        *maximum = item->fields.a;
        e->dependency.pc = 4;
        e->child.pc = 11;
    } else {
        *out = item;
        e->child.pc = 8;
    }
    ok = true;
done:
    mc_item_stack_release(stack);
    return ok;
}

static bool armor_value(mc_inventory_player *inventory, mc_registry_classes *classes, int32_t *out,
                        mc_inventory_armor_error *e) {
    for (int32_t slot = 0;; slot = mc_java_i32_add(slot, 1)) {
        e->slot = slot;
        mc_item_stack **slots;
        if (!armor_array(inventory, 15, &slots, e))
            return false;
        if (slot >= 4)
            break;
        mc_item_stack *stack;
        if (!armor_cell(inventory, 25, slot, &stack, e))
            return false;
        if (stack != NULL) {
            const mc_item *item;
            if (!armor_cell(inventory, 35, slot, &stack, e))
                return false;
            armor_at(e, 36);
            if (!armor_item(classes, stack, "a", &item, NULL, e))
                return false;
            armor_at(e, 39);
            if (!armor_item_owner(classes, item, e))
                return false;
            if (item != NULL && item->kind == MC_ITEM_ARMOR) {
                int32_t maximum, damage;
                if (!armor_cell(inventory, 51, slot, &stack, e))
                    return false;
                armor_at(e, 52);
                if (!armor_item(classes, stack, "j", NULL, &maximum, e))
                    return false;
                if (!armor_cell(inventory, 63, slot, &stack, e))
                    return false;
                armor_at(e, 64);
                if (stack == NULL)
                    return armor_null(e, "Armor null damage Stack invocation receiver");
                e->child = (mc_inventory_site){"iz", "h", "()I", 1};
                if (!armor_stack_result(e, mc_item_stack_source_damage_get(stack, &damage),
                                        "Armor actual Stack damage field"))
                    return false;
                e->child.pc = 4;
                e->remaining = mc_java_i32_add(e->remaining, mc_java_i32_sub(maximum, damage));
                e->maximum = mc_java_i32_add(e->maximum, maximum);
                if (!armor_cell(inventory, 92, slot, &stack, e))
                    return false;
                armor_at(e, 93);
                if (!armor_item(classes, stack, "a", &item, NULL, e))
                    return false;
                armor_at(e, 96);
                if (!armor_item_owner(classes, item, e))
                    return false;
                if (item != NULL && item->kind != MC_ITEM_ARMOR)
                    return armor_source(e, MC_INVENTORY_ARMOR_CLASS_CAST,
                                        "java/lang/ClassCastException",
                                        "Armor actual WA checkcast");
                armor_at(e, 99);
                if (item == NULL)
                    return armor_null(e, "Armor null WA.bl field receiver");
                e->defense = mc_java_i32_add(e->defense, item->subtype.wa.bl);
            }
        }
        ++e->completed_slots;
    }
    int32_t value = 0;
    if (e->maximum != 0) {
        int32_t quotient = 0;
        /* The preceding Java ifne guarantees a nonzero divisor, including
         * wrapped negative totals. The helper preserves MIN_VALUE/-1. */
        (void)mc_java_i32_div(mc_java_i32_mul(mc_java_i32_sub(e->defense, 1), e->remaining),
                              e->maximum, &quotient);
        value = mc_java_i32_add(quotient, 1);
    }
    armor_at(e, e->maximum == 0 ? 120 : 130);
    e->source_returned = true;
    *out = value;
    return true;
}

static bool armor_wear_child(mc_registry_classes *classes, mc_item_stack *stack, int32_t amount,
                             mc_entity *player, mc_inventory_armor_error *e) {
    if (stack == NULL)
        return armor_null(e, "Armor null durability Stack invocation receiver");
    if (!armor_stack_result(e, mc_item_stack_retain(stack), "Armor durability Stack occurrence"))
        return false;
    e->durability_entered = true;
    ++e->wear_calls;
    const mc_entity_status status =
        mc_item_stack_combat_damage(classes, stack, amount, player, &e->combat);
    mc_item_stack_release(stack);
    e->durability_returned = status == MC_ENTITY_OK;
    if (status == MC_ENTITY_OK)
        return true;
    e->child = (mc_inventory_site){e->combat.site.class_name, e->combat.site.method,
                                   e->combat.site.descriptor, e->combat.site.pc};
    e->dependency =
        (mc_inventory_site){e->combat.dependency.class_name, e->combat.dependency.method,
                            e->combat.dependency.descriptor, e->combat.dependency.pc};
    e->registry = e->combat.registry;
    e->stack = e->combat.stack;
    e->source_exception = e->combat.source_exception;
    e->source_error = e->combat.source_error;
    e->exception_class = e->combat.exception_class;
    return armor_fail(e,
                      status == MC_ENTITY_LIMIT               ? MC_INVENTORY_ARMOR_LIMIT
                      : status == MC_ENTITY_ALLOCATION_FAILED ? MC_INVENTORY_ARMOR_ALLOCATION_FAILED
                      : status == MC_ENTITY_BUSY              ? MC_INVENTORY_ARMOR_BUSY
                                                              : MC_INVENTORY_ARMOR_COMBAT_FAILED,
                      e->combat.operation);
}

static bool armor_wear(mc_inventory_player *inventory, mc_registry_classes *classes, int32_t amount,
                       mc_inventory_armor_error *e) {
    for (int32_t slot = 0;; slot = mc_java_i32_add(slot, 1)) {
        e->slot = slot;
        mc_item_stack **slots;
        if (!armor_array(inventory, 7, &slots, e))
            return false;
        if (slot >= 4)
            break;
        mc_item_stack *stack;
        if (!armor_cell(inventory, 16, slot, &stack, e))
            return false;
        if (stack != NULL) {
            const mc_item *item;
            if (!armor_cell(inventory, 25, slot, &stack, e))
                return false;
            armor_at(e, 26);
            if (!armor_item(classes, stack, "a", &item, NULL, e))
                return false;
            armor_at(e, 29);
            if (!armor_item_owner(classes, item, e))
                return false;
            if (item != NULL && item->kind == MC_ITEM_ARMOR) {
                if (!armor_cell(inventory, 40, slot, &stack, e))
                    return false;
                mc_entity *player = inventory->player; /* Fresh IX.d43 before invocation46. */
                armor_at(e, 46);
                if (!armor_wear_child(classes, stack, amount, player, e))
                    return false;
                if (!armor_cell(inventory, 54, slot, &stack, e))
                    return false;
                armor_at(e, 55);
                if (stack == NULL)
                    return armor_null(e, "Armor null Stack count field receiver");
                int32_t count;
                if (!armor_stack_result(e, mc_item_stack_source_count_get(stack, &count),
                                        "Armor actual post-durability count field"))
                    return false;
                if (count == 0) {
                    if (!armor_cell(inventory, 66, slot, &stack, e))
                        return false;
                    player = inventory->player; /* Fresh argument68, even for a null receiver. */
                    armor_at(e, 71);
                    if (stack == NULL)
                        return armor_null(e, "Armor null empty Stack hook invocation receiver");
                    (void)player; /* Actual IZ.a(GS)V is literal return0. */
                    e->child = (mc_inventory_site){"iz", "a", "(Lgs;)V", 0};
                    e->empty_hook_entered = e->empty_hook_returned = true;
                    ++e->empty_hooks;
                    if (!armor_array(inventory, 80, &slots, e))
                        return false;
                    mc_item_stack *old = slots[slot];
                    slots[slot] = NULL; /* Actual aastore80, before native occurrence release. */
                    ++e->cleared_slots;
                    mc_item_stack_release(old);
                }
            }
        }
        ++e->completed_slots;
    }
    armor_at(e, 87);
    e->source_returned = true;
    return true;
}

mc_inventory_armor_status mc_inventory_player_armor_value(mc_inventory_player *inventory,
                                                          mc_registry_classes *classes,
                                                          int32_t *out,
                                                          mc_inventory_armor_error *error) {
    mc_inventory_armor_error e;
    mc_inventory_armor_error_init(&e);
    const bool entered = armor_enter(inventory, out, "f", "()I", &e);
    if (entered)
        (void)armor_value(inventory, classes, out, &e);
    return armor_finish(inventory, entered, &e, error);
}

mc_inventory_armor_status mc_inventory_player_armor_wear(mc_inventory_player *inventory,
                                                         mc_registry_classes *classes,
                                                         int32_t amount,
                                                         mc_inventory_armor_error *error) {
    mc_inventory_armor_error e;
    mc_inventory_armor_error_init(&e);
    const bool entered = armor_enter(inventory, inventory, "e", "(I)V", &e);
    if (entered)
        (void)armor_wear(inventory, classes, amount, &e);
    return armor_finish(inventory, entered, &e, error);
}
