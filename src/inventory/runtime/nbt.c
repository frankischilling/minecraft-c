#include "mc/inventory/runtime/nbt.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"

#include <stdlib.h>
#include <string.h>

static const uint16_t inventory_name[] = {'I', 'n', 'v', 'e', 'n', 't', 'o', 'r', 'y'};
static const uint16_t slot_name[] = {'S', 'l', 'o', 't'};
static const uint16_t id_name[] = {'i', 'd'};
static const uint16_t count_name[] = {'C', 'o', 'u', 'n', 't'};
static const uint16_t damage_name[] = {'D', 'a', 'm', 'a', 'g', 'e'};

void mc_inventory_nbt_error_init(mc_inventory_nbt_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->index = SIZE_MAX;
    e->slot = -1;
    e->registry.initializer_pc = e->registry.constructor_pc = -1;
    e->registry.block_id = e->registry.item_argument = e->registry.active_initializer_pc = -1;
}

static bool fail(mc_inventory_nbt_error *e, mc_inventory_nbt_status status, const char *op) {
    e->status = status;
    e->operation = op;
    return false;
}

static bool stack_result(mc_inventory_nbt_error *e, mc_item_stack_status status) {
    e->stack.status = status;
    return status == MC_ITEM_STACK_OK ||
           fail(e,
                status == MC_ITEM_STACK_LIMIT               ? MC_INVENTORY_NBT_LIMIT
                : status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_INVENTORY_NBT_ALLOCATION_FAILED
                                                            : MC_INVENTORY_NBT_STACK_FAILED,
                "Inventory NBT actual Stack operation");
}

static bool registry_result(mc_inventory_nbt_error *e, mc_registry_status status) {
    return status == MC_REGISTRY_OK ||
           fail(e,
                status == MC_REGISTRY_LIMIT               ? MC_INVENTORY_NBT_LIMIT
                : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_INVENTORY_NBT_ALLOCATION_FAILED
                                                          : MC_INVENTORY_NBT_REGISTRY_FAILED,
                "Inventory NBT actual Item class operation");
}

static bool replace_array(mc_inventory_player *inventory, bool armor, mc_inventory_nbt_error *e) {
    const size_t size = armor ? 4u : 36u;
    e->bytecode = armor ? 11u : 3u;
    mc_inventory_error allocation = {0};
    mc_item_stack **next = inv_alloc(&inventory->local_budget, size * sizeof(*next), &allocation);
    if (next == NULL)
        return fail(e,
                    allocation.status == MC_INVENTORY_LIMIT ? MC_INVENTORY_NBT_LIMIT
                                                            : MC_INVENTORY_NBT_ALLOCATION_FAILED,
                    "Inventory NBT actual replacement array allocation");
    mc_item_stack ***cell = armor ? &inventory->armor_slots : &inventory->main_slots;
    mc_item_stack **old = *cell;
    e->bytecode = armor ? 14u : 6u;
    *cell = next;
    if (old != NULL)
        for (size_t i = 0; i < size; ++i)
            mc_item_stack_release(old[i]);
    inv_free(&inventory->local_budget, old);
    return true;
}

static bool read_body(mc_inventory_player *inventory, mc_registry_classes *classes,
                      const mc_nbt_tag *list, const mc_nbt_limits *limits,
                      mc_inventory_nbt_error *e) {
    if (!replace_array(inventory, false, e) || !replace_array(inventory, true, e))
        return false;
    const size_t ceiling =
        limits == NULL ? mc_nbt_default_limits().max_elements : limits->max_elements;
    for (size_t i = 0;; ++i) {
        e->bytecode = 21;
        if (list == NULL)
            return fail(e, MC_INVENTORY_NBT_NULL_REFERENCE, "Inventory NBT List.size receiver");
        if (list->type != MC_NBT_LIST)
            return fail(e, MC_INVENTORY_NBT_WRONG_TAG, "Inventory NBT List view type");
        if (i >= list->value.list.length)
            return true;
        e->index = i;
        e->slot = -1;
        e->bytecode = 29;
        if (i >= ceiling)
            return fail(e, MC_INVENTORY_NBT_LIMIT, "Inventory NBT pending List.get work ceiling");
        if (list->value.list.items == NULL)
            return fail(e, MC_INVENTORY_NBT_INVALID_ARGUMENT, "Inventory NBT readable List span");
        const mc_nbt_tag *entry = list->value.list.items[i];
        e->bytecode = 32;
        if (entry != NULL && entry->type != MC_NBT_COMPOUND)
            return fail(e, MC_INVENTORY_NBT_WRONG_TAG, "Inventory NBT entry Compound cast");
        e->bytecode = 39;
        e->field = "Slot";
        if (entry == NULL)
            return fail(e, MC_INVENTORY_NBT_NULL_REFERENCE, "Inventory NBT Slot Compound receiver");
        const mc_nbt_tag *tag = mc_nbt_compound_get(entry, slot_name, 4);
        if (tag != NULL && tag->type != MC_NBT_BYTE)
            return fail(e, MC_INVENTORY_NBT_WRONG_TAG, "Inventory NBT Slot Byte cast");
        const int32_t slot = (tag == NULL ? 0 : (int32_t)tag->value.byte_value) & 255;
        e->slot = slot;
        e->bytecode = 48;
        mc_item_stack *stack = NULL;
        if (!stack_result(e, mc_item_stack_allocate(&stack)))
            return false;
        e->bytecode = 53;
        mc_item_stack_status constructed = mc_item_stack_construct_nbt(stack, entry, &e->stack);
        if (constructed != MC_ITEM_STACK_OK) {
            if (e->stack.field != NULL) {
                e->stack_bytecode = strcmp(e->stack.field, "id") == 0      ? 4u
                                    : strcmp(e->stack.field, "Count") == 0 ? 14u
                                                                           : 24u;
                e->field = e->stack.field;
            }
            stack_result(e, constructed);
            mc_item_stack_release(stack);
            return false;
        }
        e->bytecode = 60;
        e->stack_bytecode = 0;
        if (classes == NULL) {
            mc_item_stack_release(stack);
            return fail(e, MC_INVENTORY_NBT_REGISTRY_FAILED, "Inventory NBT missing code Registry");
        }
        const mc_registry_item_array *array;
        if (!registry_result(e, mc_registry_item_c_get(classes, &array, &e->registry))) {
            mc_item_stack_release(stack);
            return false;
        }
        mc_item_stack_fields values;
        e->stack_bytecode = 4;
        if (!stack_result(e, mc_item_stack_fields_get(stack, &values))) {
            mc_item_stack_release(stack);
            return false;
        }
        const mc_item *item;
        e->stack_bytecode = 7;
        if (!registry_result(
                e, mc_registry_item_c_at(classes, array, values.id, &item, &e->registry))) {
            mc_item_stack_release(stack);
            return false;
        }
        if (item != NULL) {
            mc_item_stack **cell = NULL;
            if (slot < 36) {
                e->bytecode = 89;
                cell = &inventory->main_slots[slot];
            }
            if (slot >= 100 && slot < 104) {
                e->bytecode = 121;
                cell = &inventory->armor_slots[slot - 100];
            }
            if (cell != NULL) {
                mc_item_stack *old = *cell;
                *cell = stack; /* Transfer this real constructor hold. */
                stack = NULL;
                mc_item_stack_release(old);
            }
        }
        mc_item_stack_release(stack);
    }
}

mc_inventory_nbt_status mc_inventory_player_read_nbt(mc_inventory_player *inventory,
                                                     mc_registry_classes *classes,
                                                     const mc_nbt_tag *list,
                                                     const mc_nbt_limits *limits,
                                                     mc_inventory_nbt_error *out) {
    mc_inventory_nbt_error e;
    mc_inventory_nbt_error_init(&e);
    bool active = false, registry_held = false;
    if (inventory == NULL)
        fail(&e, MC_INVENTORY_NBT_NULL_REFERENCE, "Inventory NBT actual receiver");
    else if (inventory->side != MC_REGISTRY_CLIENT)
        fail(&e, MC_INVENTORY_NBT_INVALID_ARGUMENT, "Inventory NBT actual client class");
    else if (inventory->active)
        fail(&e, MC_INVENTORY_NBT_BUSY, "Inventory NBT active native owner");
    else {
        inventory->active = true;
        active = true;
        if (classes != NULL)
            registry_held = registry_result(
                &e, mc_registry_classes_retain(classes, inventory->side,
                                               classes->options.random_profile, &e.registry));
        if (e.status == MC_INVENTORY_NBT_OK)
            (void)read_body(inventory, classes, list, limits, &e);
    }
    if (registry_held && !mc_registry_runtime_borrow_release(classes))
        fail(&e, MC_INVENTORY_NBT_REGISTRY_FAILED, "Inventory NBT Registry borrow retirement");
    if (active)
        inventory->active = false;
    const mc_inventory_nbt_status status = e.status;
    if (status == MC_INVENTORY_NBT_OK)
        mc_inventory_nbt_error_init(&e);
    if (out != NULL)
        *out = e;
    return status;
}

typedef struct inventory_nbt_build {
    mc_nbt_tag root, inventory, entries[40], fields[40][4];
    const mc_nbt_tag *root_children[1], *children[40], *entry_children[40][4];
    mc_item_stack *held[40];
} inventory_nbt_build;

mc_inventory_nbt_status mc_inventory_player_build_nbt(mc_inventory_player *inventory,
                                                      const mc_nbt_limits *requested,
                                                      mc_nbt_tree **out,
                                                      mc_inventory_nbt_error *error) {
    mc_inventory_nbt_error e;
    mc_inventory_nbt_error_init(&e);
    const mc_nbt_limits limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    inventory_nbt_build *b = NULL;
    mc_nbt_tree *result = NULL;
    bool active = false;
    if (inventory == NULL || out == NULL)
        fail(&e, MC_INVENTORY_NBT_INVALID_ARGUMENT, "Inventory NBT writer native arguments");
    else if (inventory->side != MC_REGISTRY_CLIENT)
        fail(&e, MC_INVENTORY_NBT_INVALID_ARGUMENT, "Inventory NBT writer actual client class");
    else if (inventory->active)
        fail(&e, MC_INVENTORY_NBT_BUSY, "Inventory NBT writer active native owner");
    else if (limits.max_allocation_bytes < sizeof(*b))
        fail(&e, MC_INVENTORY_NBT_LIMIT, "Inventory NBT writer native scratch budget");
    else if ((b = calloc(1, sizeof(*b))) == NULL)
        fail(&e, MC_INVENTORY_NBT_ALLOCATION_FAILED, "Inventory NBT writer native scratch");
    else {
        inventory->active = true;
        active = true;
        size_t count = 0;
        for (size_t section = 0; section < 2 && e.status == MC_INVENTORY_NBT_OK; ++section) {
            const size_t length = section == 0 ? 36u : 4u;
            for (size_t i = 0; i < length; ++i) {
                e.index = i;
                e.slot = (int32_t)i + (section == 0 ? 0 : 100);
                e.bytecode = section == 0 ? 7u : 65u;
                if ((section == 0 ? inventory->main_slots : inventory->armor_slots) == NULL) {
                    fail(&e, MC_INVENTORY_NBT_NULL_REFERENCE,
                         "Inventory NBT writer arraylength receiver");
                    break;
                }
                e.bytecode = section == 0 ? 16u : 74u;
                if ((section == 0 ? inventory->main_slots : inventory->armor_slots)[i] == NULL)
                    continue;
                b->fields[count][0] = (mc_nbt_tag){.type = MC_NBT_BYTE,
                                                   .name = {slot_name, 4},
                                                   .value.byte_value = mc_java_i8_narrow(e.slot)};
                e.bytecode = section == 0 ? 43u : 104u;
                mc_item_stack *stack =
                    (section == 0 ? inventory->main_slots : inventory->armor_slots)[i];
                if (!stack_result(&e, mc_item_stack_retain(stack)))
                    break;
                b->held[count] = stack;
                mc_item_stack_fields values;
                if (!stack_result(&e, mc_item_stack_fields_get(stack, &values)))
                    break;
                e.stack_bytecode = 8;
                b->fields[count][1] =
                    (mc_nbt_tag){.type = MC_NBT_SHORT,
                                 .name = {id_name, 2},
                                 .value.short_value = mc_java_i16_narrow(values.id)};
                e.stack_bytecode = 19;
                b->fields[count][2] =
                    (mc_nbt_tag){.type = MC_NBT_BYTE,
                                 .name = {count_name, 5},
                                 .value.byte_value = mc_java_i8_narrow(values.count)};
                e.stack_bytecode = 30;
                b->fields[count][3] =
                    (mc_nbt_tag){.type = MC_NBT_SHORT,
                                 .name = {damage_name, 6},
                                 .value.short_value = mc_java_i16_narrow(values.damage)};
                for (size_t j = 0; j < 4; ++j)
                    b->entry_children[count][j] = &b->fields[count][j];
                b->entries[count] = (mc_nbt_tag){.type = MC_NBT_COMPOUND,
                                                 .value.compound = {b->entry_children[count], 4}};
                b->children[count] = &b->entries[count];
                e.bytecode = section == 0 ? 49u : 110u;
                ++count;
            }
        }
        if (e.status == MC_INVENTORY_NBT_OK) {
            b->inventory = (mc_nbt_tag){
                .type = MC_NBT_LIST,
                .name = {inventory_name, 9},
                .value.list = {.element_type = count == 0 ? MC_NBT_END : MC_NBT_COMPOUND,
                               .length = count,
                               .items = b->children}};
            b->root_children[0] = &b->inventory;
            b->root =
                (mc_nbt_tag){.type = MC_NBT_COMPOUND, .value.compound = {b->root_children, 1}};
            mc_nbt_limits remaining = limits;
            remaining.max_allocation_bytes -= sizeof(*b);
            e.bytecode = 120;
            const mc_nbt_status status =
                mc_nbt_tree_copy_compound(&b->root, &remaining, &result, &e.nbt);
            if (status != MC_NBT_OK)
                fail(&e,
                     status == MC_NBT_LIMIT               ? MC_INVENTORY_NBT_LIMIT
                     : status == MC_NBT_ALLOCATION_FAILED ? MC_INVENTORY_NBT_ALLOCATION_FAILED
                                                          : MC_INVENTORY_NBT_CODEC_FAILED,
                     "Inventory NBT writer independent tree");
        }
    }
    if (b != NULL) {
        for (size_t i = 0; i < 40; ++i)
            mc_item_stack_release(b->held[i]);
        free(b);
    }
    if (active)
        inventory->active = false;
    const mc_inventory_nbt_status status = e.status;
    if (status == MC_INVENTORY_NBT_OK) {
        mc_inventory_nbt_error_init(&e);
        *out = result;
    } else
        mc_nbt_tree_free(result);
    if (error != NULL)
        *error = e;
    return status;
}
