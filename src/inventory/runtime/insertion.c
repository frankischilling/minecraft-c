#include "mc/inventory/runtime/insertion.h"
#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../item/stack/source_internal.h"
#include "../internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"
#include <string.h>

typedef struct insert_scope {
    mc_inventory_player *inventory;
    mc_registry_classes *classes;
    mc_item_stack *incoming;
    bool active, registry_held, stack_held;
} insert_scope;

void mc_inventory_insert_error_init(mc_inventory_insert_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->slot = -1;
    e->registry.initializer_pc = e->registry.constructor_pc = -1;
    e->registry.block_id = e->registry.item_argument = e->registry.active_initializer_pc = -1;
}

static bool fail(mc_inventory_insert_error *e, mc_entity_status status, const char *op) {
    e->status = status;
    e->operation = op;
    return false;
}

static void at(mc_inventory_insert_error *e, const char *method, unsigned int pc) {
    e->source_method = method;
    e->bytecode = pc;
    if (strcmp(method, "a(Liz;)Z") == 0)
        e->add_bytecode = pc;
    else if (strcmp(method, "e(Liz;)I") == 0)
        e->merge_bytecode = pc;
    else
        e->find_bytecode = pc;
}

static bool work(mc_inventory_insert_error *e) {
    if (e->loop_work == e->max_loop_work)
        return fail(e, MC_ENTITY_LIMIT, "Inventory insertion pending loop-work ceiling");
    ++e->loop_work;
    return true;
}

static bool stack_result(mc_inventory_insert_error *e, mc_item_stack_status status) {
    e->stack = status;
    return status == MC_ITEM_STACK_OK ||
           fail(e,
                status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                : status == MC_ITEM_STACK_LIMIT           ? MC_ENTITY_LIMIT
                                                          : MC_ENTITY_STATE_ERROR,
                "Inventory insertion actual Stack operation");
}

static bool fields(mc_item_stack *stack, mc_item_stack_fields *out, mc_inventory_insert_error *e) {
    if (stack == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion null Stack receiver");
    return stack_result(e, mc_item_stack_fields_get(stack, out));
}

static bool registry_result(mc_inventory_insert_error *e, mc_registry_status status) {
    return status == MC_REGISTRY_OK ||
           fail(e,
                status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
                : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                          : MC_ENTITY_STATE_ERROR,
                "Inventory insertion actual Item class operation");
}

/* Capture gm.c before the source id read, with no later current-c reload. */
static bool item(insert_scope *s, mc_item_stack *stack, const mc_item_fields **out,
                 mc_inventory_insert_error *e, const char *method) {
    e->stack_method = method;
    e->stack_bytecode = 0;
    if (s->classes == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion missing code Registry");
    const mc_registry_item_array *array;
    if (!registry_result(e, mc_registry_item_c_get(s->classes, &array, &e->registry)))
        return false;
    e->stack_bytecode = 4;
    mc_item_stack_fields f;
    if (!fields(stack, &f, e))
        return false;
    e->stack_bytecode = 7;
    const mc_item *selected;
    if (!registry_result(e,
                         mc_registry_item_c_at(s->classes, array, f.id, &selected, &e->registry)))
        return false;
    if (selected == NULL) {
        *out = NULL;
        return true;
    }
    *out = mc_item_fields_get(selected);
    return *out != NULL || fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion Item owner");
}

static bool maximum(insert_scope *s, mc_item_stack *stack, int32_t *out,
                    mc_inventory_insert_error *e) {
    e->stack_method = "c()I";
    if (stack == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion null Stack c receiver");
    const mc_item_fields *f;
    if (!item(s, stack, &f, e, "a()Lgm;"))
        return false;
    e->stack_method = "c()I";
    e->stack_bytecode = 4;
    if (f == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion null Item d receiver");
    *out = f->bg; /* All40 actual descendants inherit literal gm.d field body. */
    return true;
}

static bool damageable(insert_scope *s, mc_item_stack *stack, bool *out,
                       mc_inventory_insert_error *e) {
    e->stack_method = "e()Z";
    if (stack == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion null Stack e receiver");
    const mc_item_fields *f;
    if (!item(s, stack, &f, e, "e()Z"))
        return false;
    e->stack_bytecode = 8;
    if (f == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion null Item f receiver");
    *out = f->a > 0; /* Actual gm.f()I, not the metadata/subtype flag. */
    return true;
}

static bool damaged(insert_scope *s, mc_item_stack *stack, bool *out,
                    mc_inventory_insert_error *e) {
    bool possible;
    if (!damageable(s, stack, &possible, e))
        return false;
    e->stack_method = "g()Z";
    e->stack_bytecode = 8;
    if (!possible) {
        *out = false;
        return true;
    }
    mc_item_stack_fields f;
    if (!fields(stack, &f, e))
        return false;
    *out = f.damage > 0;
    return true;
}

static bool stackable(insert_scope *s, mc_item_stack *stack, bool *out,
                      mc_inventory_insert_error *e) {
    int32_t max;
    if (!maximum(s, stack, &max, e))
        return false;
    *out = false;
    if (max <= 1)
        return true;
    bool can_damage, has_damage = false;
    if (!damageable(s, stack, &can_damage, e))
        return false;
    if (can_damage && !damaged(s, stack, &has_damage, e))
        return false;
    *out = !can_damage || !has_damage;
    return true;
}

static bool cell(insert_scope *s, int32_t slot, mc_item_stack **out, mc_inventory_insert_error *e) {
    e->slot = slot;
    if (s->inventory->main_slots == NULL || slot < 0 || slot >= 36)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion actual main array access");
    *out = s->inventory->main_slots[(size_t)(uint32_t)slot];
    return true;
}

static bool find_empty(insert_scope *s, int32_t *out, mc_inventory_insert_error *e) {
    for (int32_t slot = 0;; slot = mc_java_i32_add(slot, 1)) {
        at(e, "j()I", 7);
        if (s->inventory->main_slots == NULL)
            return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion null main array length");
        if (slot >= 36)
            break;
        at(e, "j()I", 16);
        if (!work(e))
            return false;
        mc_item_stack *value;
        if (!cell(s, slot, &value, e))
            return false;
        if (value == NULL) {
            *out = slot;
            return true;
        }
    }
    *out = -1;
    return true;
}

static bool receiver_property(insert_scope *s, int32_t slot, bool stackable_value, int32_t *max,
                              bool *boolean, mc_inventory_insert_error *e) {
    mc_item_stack *value;
    if (!cell(s, slot, &value, e))
        return false;
    if (value == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion null property Stack");
    if (!stack_result(e, mc_item_stack_retain(value)))
        return false;
    bool okay;
    if (max != NULL)
        okay = maximum(s, value, max, e);
    else if (stackable_value)
        okay = stackable(s, value, boolean, e);
    else {
        e->stack_method = "f()Z";
        const mc_item_fields *f;
        okay = item(s, value, &f, e, "f()Z");
        if (okay)
            e->stack_bytecode = 8;
        if (okay && f == NULL)
            okay = fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion null Item e receiver");
        if (okay)
            *boolean = f->bj;
    }
    mc_item_stack_release(value);
    return okay;
}

static bool find_merge(insert_scope *s, mc_item_stack *incoming, int32_t *out,
                       mc_inventory_insert_error *e) {
    for (int32_t slot = 0;; slot = mc_java_i32_add(slot, 1)) {
        at(e, "d(Liz;)I", 7);
        if (s->inventory->main_slots == NULL)
            return fail(e, MC_ENTITY_STATE_ERROR, "Inventory merge null array length");
        if (slot >= 36)
            break;
        at(e, "d(Liz;)I", 16);
        if (!work(e))
            return false;
        mc_item_stack *value;
        if (!cell(s, slot, &value, e))
            return false;
        if (value == NULL)
            continue;
        mc_item_stack_fields a, b;
        at(e, "d(Liz;)I", 26);
        if (!cell(s, slot, &value, e) || !fields(value, &a, e))
            return false;
        at(e, "d(Liz;)I", 30);
        if (!fields(incoming, &b, e))
            return false;
        if (a.id != b.id)
            continue;
        bool yes;
        at(e, "d(Liz;)I", 42);
        if (!receiver_property(s, slot, true, NULL, &yes, e))
            return false;
        if (!yes)
            continue;
        at(e, "d(Liz;)I", 54);
        if (!cell(s, slot, &value, e) || !fields(value, &a, e))
            return false;
        int32_t max;
        at(e, "d(Liz;)I", 63);
        if (!receiver_property(s, slot, false, &max, NULL, e))
            return false;
        if (a.count >= max)
            continue;
        at(e, "d(Liz;)I", 75);
        if (!cell(s, slot, &value, e) || !fields(value, &a, e))
            return false;
        if (a.count >= 64) /* Actual ix.d()I has the literal return64. */
            continue;
        at(e, "d(Liz;)I", 91);
        if (!receiver_property(s, slot, false, NULL, &yes, e))
            return false;
        if (yes) {
            at(e, "d(Liz;)I", 103);
            if (!cell(s, slot, &value, e) || !fields(value, &a, e))
                return false;
            at(e, "d(Liz;)I", 107);
            if (!fields(incoming, &b, e))
                return false;
            if (a.damage != b.damage)
                continue;
        }
        *out = slot;
        return true;
    }
    *out = -1;
    return true;
}

static bool publish(insert_scope *s, mc_item_stack **captured_array, int32_t slot,
                    mc_item_stack *value, mc_inventory_insert_error *e) {
    if (captured_array == NULL || captured_array != s->inventory->main_slots)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory insertion captured actual array owner");
    const mc_inventory_status status =
        mc_inventory_player_set(s->inventory, slot, value, &e->inventory);
    return status == MC_INVENTORY_OK ||
           fail(e,
                status == MC_INVENTORY_LIMIT               ? MC_ENTITY_LIMIT
                : status == MC_INVENTORY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                                                           : MC_ENTITY_STATE_ERROR,
                "Inventory insertion actual array occurrence publication");
}

static bool merge(insert_scope *s, mc_item_stack *incoming, int32_t *out,
                  mc_inventory_insert_error *e) {
    mc_item_stack_fields source;
    at(e, "e(Liz;)I", 1);
    if (!fields(incoming, &source, e))
        return false;
    const int32_t id = source.id;
    at(e, "e(Liz;)I", 6);
    if (!fields(incoming, &source, e))
        return false;
    int32_t remaining = source.count, slot;
    e->merge_bytecode = 12;
    if (!find_merge(s, incoming, &slot, e))
        return false;
    if (slot < 0) {
        e->merge_bytecode = 23;
        if (!find_empty(s, &slot, e))
            return false;
    }
    if (slot < 0) {
        *out = remaining;
        return true;
    }
    mc_item_stack *value;
    at(e, "e(Liz;)I", 41);
    if (!cell(s, slot, &value, e))
        return false;
    if (value == NULL) {
        /* Actual array45 is captured before new51 and damage argument58. */
        mc_item_stack **captured_array = s->inventory->main_slots;
        at(e, "e(Liz;)I", 51);
        mc_item_stack *created = NULL;
        if (!stack_result(e, mc_item_stack_allocate(&created)))
            return false;
        at(e, "e(Liz;)I", 58);
        bool okay = fields(incoming, &source, e);
        if (okay) {
            at(e, "e(Liz;)I", 61);
            okay = stack_result(e, mc_item_stack_construct(created, id, 0, source.damage));
        }
        if (okay) {
            at(e, "e(Liz;)I", 64);
            okay = publish(s, captured_array, slot, created, e);
        }
        mc_item_stack_release(created);
        if (!okay)
            return false;
    }
    int32_t transfer = remaining, max;
    at(e, "e(Liz;)I", 77);
    if (!receiver_property(s, slot, false, &max, NULL, e))
        return false;
    at(e, "e(Liz;)I", 87);
    if (!cell(s, slot, &value, e) || !fields(value, &source, e))
        return false;
    if (transfer > mc_java_i32_sub(max, source.count)) {
        at(e, "e(Liz;)I", 101);
        if (!receiver_property(s, slot, false, &max, NULL, e))
            return false;
        at(e, "e(Liz;)I", 111);
        if (!cell(s, slot, &value, e) || !fields(value, &source, e))
            return false;
        transfer = mc_java_i32_sub(max, source.count);
    }
    at(e, "e(Liz;)I", 130);
    if (!cell(s, slot, &value, e) || !fields(value, &source, e))
        return false;
    if (transfer > mc_java_i32_sub(64, source.count)) {
        at(e, "e(Liz;)I", 148);
        if (!cell(s, slot, &value, e) || !fields(value, &source, e))
            return false;
        transfer = mc_java_i32_sub(64, source.count);
    }
    if (transfer == 0) {
        *out = remaining;
        return true;
    }
    remaining = mc_java_i32_sub(remaining, transfer);
    at(e, "e(Liz;)I", 176);
    if (!cell(s, slot, &value, e) || !fields(value, &source, e))
        return false;
    at(e, "e(Liz;)I", 180);
    if (!stack_result(
            e, mc_item_stack_source_count_set(value, mc_java_i32_add(source.count, transfer))))
        return false;
    at(e, "e(Liz;)I", 191);
    if (!cell(s, slot, &value, e) || !stack_result(e, mc_item_stack_source_animation_set(value, 5)))
        return false;
    *out = remaining;
    return true;
}

static bool add(insert_scope *s, bool *out, mc_inventory_insert_error *e) {
    at(e, "a(Liz;)Z", 1);
    bool is_damaged;
    if (s->incoming == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Inventory add null Stack g receiver");
    if (!damaged(s, s->incoming, &is_damaged, e))
        return false;
    if (!is_damaged) {
        int32_t old, remaining;
        do {
            at(e, "a(Liz;)Z", 8);
            if (!work(e) || !stack_result(e, mc_item_stack_source_count_get(s->incoming, &old)))
                return false;
            e->add_bytecode = 15;
            if (!merge(s, s->incoming, &remaining, e))
                return false;
            at(e, "a(Liz;)Z", 18);
            if (!stack_result(e, mc_item_stack_source_count_set(s->incoming, remaining)))
                return false;
            at(e, "a(Liz;)Z", 23);
            if (!stack_result(e, mc_item_stack_source_count_get(s->incoming, &remaining)))
                return false;
        } while (remaining > 0 && remaining < old);
        at(e, "a(Liz;)Z", 37);
        if (!stack_result(e, mc_item_stack_source_count_get(s->incoming, &remaining)))
            return false;
        *out = remaining < old;
        return true;
    }
    int32_t slot;
    e->add_bytecode = 51;
    if (!find_empty(s, &slot, e))
        return false;
    if (slot < 0) {
        *out = false;
        return true;
    }
    at(e, "a(Liz;)Z", 65);
    /* Actual array60/index63 precede iz.b65 and its new/constructor work. */
    mc_item_stack **captured_array = s->inventory->main_slots;
    mc_item_stack *copy = NULL;
    mc_item_stack_source_site site;
    if (!stack_result(
            e, mc_item_stack_source_clone(s->incoming, mc_item_stack_allocate, &copy, &site))) {
        e->stack_method = "k()Liz;";
        e->stack_bytecode = site.bytecode;
        return false;
    }
    at(e, "a(Liz;)Z", 68);
    const bool published = publish(s, captured_array, slot, copy, e);
    mc_item_stack_release(copy);
    if (!published)
        return false;
    at(e, "a(Liz;)Z", 76);
    mc_item_stack *value;
    if (!cell(s, slot, &value, e) || !stack_result(e, mc_item_stack_source_animation_set(value, 5)))
        return false;
    at(e, "a(Liz;)Z", 81);
    if (!stack_result(e, mc_item_stack_source_count_set(s->incoming, 0)))
        return false;
    *out = true;
    return true;
}

mc_entity_status mc_inventory_player_insert(mc_inventory_player *inventory, mc_item_stack *incoming,
                                            mc_registry_classes *classes,
                                            const mc_inventory_insert_limits *limits, bool *out,
                                            mc_inventory_insert_error *error) {
    mc_inventory_insert_error frame;
    mc_inventory_insert_error_init(&frame);
    frame.max_loop_work =
        limits == NULL ? MC_INVENTORY_INSERT_DEFAULT_LOOP_WORK : limits->max_loop_work;
    insert_scope scope = {.inventory = inventory, .classes = classes, .incoming = incoming};
    bool value = false, completed = false;
    if (inventory == NULL || out == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, "Inventory insertion native arguments");
    else if (inventory->side != MC_REGISTRY_CLIENT)
        fail(&frame, MC_ENTITY_STATE_ERROR, "Inventory insertion actual client class");
    else if (inventory->active)
        fail(&frame, MC_ENTITY_BUSY, "Inventory insertion active owner");
    else {
        inventory->active = true;
        scope.active = true;
        if (classes != NULL) {
            scope.registry_held =
                registry_result(&frame, mc_registry_classes_retain(classes, MC_REGISTRY_CLIENT,
                                                                   classes->options.random_profile,
                                                                   &frame.registry));
        }
        if (frame.status == MC_ENTITY_OK && incoming != NULL)
            scope.stack_held = stack_result(&frame, mc_item_stack_retain(incoming));
        if (frame.status == MC_ENTITY_OK)
            completed = add(&scope, &value, &frame);
    }
    if (scope.stack_held)
        mc_item_stack_release(incoming);
    if (scope.registry_held && !mc_registry_runtime_borrow_release(classes))
        fail(&frame, MC_ENTITY_STATE_ERROR, "Inventory insertion Registry borrow retirement");
    if (scope.active)
        inventory->active = false;
    if (frame.status == MC_ENTITY_OK && completed) {
        mc_inventory_insert_error_init(&frame);
        *out = value;
    }
    if (error != NULL)
        *error = frame;
    return frame.status;
}
