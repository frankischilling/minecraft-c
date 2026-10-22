#include "nbt_internal.h"

#include "entity_internal.h"
#include "inventory_internal.h"
#include "mc/core/java/java.h"

#include <limits.h>
#include <string.h>

static const uint16_t type_name[] = {'T', 'y', 'p', 'e'};
static const uint16_t push_x_name[] = {'P', 'u', 's', 'h', 'X'};
static const uint16_t push_z_name[] = {'P', 'u', 's', 'h', 'Z'};
static const uint16_t fuel_name[] = {'F', 'u', 'e', 'l'};
static const uint16_t items_name[] = {'I', 't', 'e', 'm', 's'};
static const uint16_t slot_name[] = {'S', 'l', 'o', 't'};
static const uint16_t id_name[] = {'i', 'd'};
static const uint16_t count_name[] = {'C', 'o', 'u', 'n', 't'};
static const uint16_t damage_name[] = {'D', 'a', 'm', 'a', 'g', 'e'};

static mc_minecart_site own_site(const mc_entity *cart, bool write) {
    const bool client = cart->classes->source == MC_ENTITY_CLIENT;
    return (mc_minecart_site){
        true, true, client ? "yl" : "pr",
        write ? (client ? "b(Lnu;)V" : "b(Liq;)V") : (client ? "a(Lnu;)V" : "a(Liq;)V"), 0};
}

static bool fail(mc_entity_nbt_error *frame, mc_entity_nbt_status status, const char *operation) {
    frame->status = status;
    frame->operation = operation;
    return false;
}

static bool inventory_fail(mc_entity_nbt_error *frame, mc_entity_nbt_status status,
                           mc_minecart_inventory_status inventory, const char *operation) {
    frame->inventory.status = inventory;
    frame->inventory.operation = operation;
    return fail(frame, status, operation);
}

/* Only the reached getter checks the receiver and the named value. No whole
 * input-tree validation moves a later cast ahead of an earlier store. */
static bool get(const mc_nbt_tag *compound, const uint16_t *name, size_t length, const char *field,
                mc_nbt_type type, const mc_nbt_tag **out, mc_entity_nbt_error *frame) {
    frame->field = field;
    if (compound == NULL)
        return fail(frame, MC_ENTITY_NBT_NULL_REFERENCE, "Minecart NBT Compound receiver");
    if (compound->type != MC_NBT_COMPOUND)
        return fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Minecart NBT Compound type");
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, name, length);
    if (tag != NULL && tag->type != type)
        return fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Minecart NBT named tag cast");
    *out = tag;
    return true;
}

static mc_entity_nbt_status stack_status(mc_item_stack_status status) {
    if (status == MC_ITEM_STACK_LIMIT)
        return MC_ENTITY_NBT_LIMIT;
    if (status == MC_ITEM_STACK_ALLOCATION_FAILED)
        return MC_ENTITY_NBT_ALLOCATION_FAILED;
    return MC_ENTITY_NBT_STACK_FAILED;
}

static bool stack_failure(mc_entity_nbt_error *frame, mc_item_stack_status status,
                          mc_minecart_inventory_stack_cause cause, const char *operation) {
    frame->inventory.stack_cause = cause;
    return inventory_fail(frame, stack_status(status),
                          status == MC_ITEM_STACK_LIMIT ? MC_MINECART_INVENTORY_LIMIT
                          : status == MC_ITEM_STACK_ALLOCATION_FAILED
                              ? MC_MINECART_INVENTORY_ALLOCATION_FAILED
                              : MC_MINECART_INVENTORY_STACK_FAILED,
                          operation);
}

bool mc_minecart_nbt_read_held(mc_entity *cart, const mc_nbt_tag *compound,
                               mc_entity_nbt_error *frame) {
    frame->minecart_read = own_site(cart, false);
    frame->index = SIZE_MAX;
    const mc_nbt_tag *tag;
    frame->minecart_read.bytecode = 4;
    if (!get(compound, type_name, 4, "Type", MC_NBT_INT, &tag, frame))
        return false;
    frame->minecart_read.bytecode = 7;
    cart->minecart->fields.d = tag == NULL ? 0 : tag->value.int_value;
    if (cart->minecart->fields.d == 2) {
        frame->minecart_read.bytecode = 22;
        if (!get(compound, push_x_name, 5, "PushX", MC_NBT_DOUBLE, &tag, frame))
            return false;
        const uint64_t x_bits = tag == NULL ? 0 : tag->value.double_bits;
        frame->minecart_read.bytecode = 25;
        memcpy(&cart->minecart->fields.f, &x_bits, sizeof(x_bits));
        frame->minecart_read.bytecode = 32;
        if (!get(compound, push_z_name, 5, "PushZ", MC_NBT_DOUBLE, &tag, frame))
            return false;
        const uint64_t z_bits = tag == NULL ? 0 : tag->value.double_bits;
        frame->minecart_read.bytecode = 35;
        memcpy(&cart->minecart->fields.g, &z_bits, sizeof(z_bits));
        frame->minecart_read.bytecode = 42;
        if (!get(compound, fuel_name, 4, "Fuel", MC_NBT_SHORT, &tag, frame))
            return false;
        frame->minecart_read.bytecode = 45;
        cart->minecart->fields.e = tag == NULL ? 0 : tag->value.short_value;
    } else if (cart->minecart->fields.d == 1) {
        frame->minecart_read.bytecode = 62;
        const mc_nbt_tag *items;
        if (!get(compound, items_name, 5, "Items", MC_NBT_LIST, &items, frame))
            return false;
        /* Actual size()68 returns27 without reading h or Type. Its successful
         * constant body adds no lower failed frame or native hold boundary. */
        frame->minecart_read.bytecode = 71;
        mc_minecart_stack_array *replacement = NULL;
        const mc_entity_status allocated = mc_minecart_inventory_array_allocate(
            cart, 27, "Minecart NBT inventory array", 71, &replacement, &frame->construction);
        if (allocated != MC_ENTITY_OK)
            return inventory_fail(
                frame,
                allocated == MC_ENTITY_LIMIT               ? MC_ENTITY_NBT_LIMIT
                : allocated == MC_ENTITY_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                           : MC_ENTITY_NBT_ENTITY_FAILED,
                allocated == MC_ENTITY_LIMIT               ? MC_MINECART_INVENTORY_LIMIT
                : allocated == MC_ENTITY_ALLOCATION_FAILED ? MC_MINECART_INVENTORY_ALLOCATION_FAILED
                                                           : MC_MINECART_INVENTORY_INVALID_ARGUMENT,
                "Minecart NBT inventory array");
        frame->minecart_read.bytecode = 74;
        mc_minecart_stack_array *old = mc_minecart_inventory_array_swap(cart, replacement);
        bool completed = false;
        for (size_t index = 0;; ++index) {
            frame->minecart_read.bytecode = 81;
            frame->field = "Items";
            frame->index = index;
            frame->inventory.slot_selected = false;
            /* An absent named List is the source's fresh empty List. A native
             * prepared span cannot represent a Java List size above INT_MAX. */
            const size_t length = items == NULL ? 0 : items->value.list.length;
            if (length > INT32_MAX) {
                inventory_fail(frame, MC_ENTITY_NBT_LIMIT, MC_MINECART_INVENTORY_LIMIT,
                               "Minecart NBT List size representation");
                break;
            }
            if (index >= length) {
                completed = true;
                break;
            }
            frame->minecart_read.bytecode = 89;
            const mc_nbt_tag *record = items->value.list.items[index];
            frame->minecart_read.bytecode = 92;
            if (record != NULL && record->type != MC_NBT_COMPOUND) {
                fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Minecart NBT Items element cast");
                break;
            }
            frame->minecart_read.bytecode = 101;
            if (!get(record, slot_name, 4, "Slot", MC_NBT_BYTE, &tag, frame))
                break;
            const int32_t slot = (tag == NULL ? 0 : (int32_t)tag->value.byte_value) & 255;
            frame->inventory.slot_selected = true;
            frame->inventory.slot = slot;
            frame->minecart_read.bytecode = 121;
            mc_minecart_stack_array *current = mc_minecart_inventory_array_current(cart);
            if (current == NULL) {
                inventory_fail(frame, MC_ENTITY_NBT_NULL_REFERENCE,
                               MC_MINECART_INVENTORY_NULL_REFERENCE,
                               "Minecart NBT inventory length receiver");
                break;
            }
            if ((size_t)slot >= mc_minecart_inventory_array_length(current))
                continue;
            /* Capture h126 and index129 before allocating/constructing the
             * Stack; the store consumes that allocation's independent hold. */
            mc_minecart_stack_array *target = mc_minecart_inventory_array_current(cart);
            mc_item_stack *stack = NULL;
            frame->minecart_read.bytecode = 131;
            mc_item_stack_status status = mc_item_stack_allocate(&stack);
            if (status != MC_ITEM_STACK_OK) {
                frame->stack = (mc_item_stack_error){.status = status};
                stack_failure(frame, status, MC_MINECART_INVENTORY_STACK_NONE,
                              "Minecart NBT new Stack");
                break;
            }
            frame->minecart_read.bytecode = 137;
            mc_item_stack_source_error lower;
            status = mc_item_stack_source_construct_nbt(stack, record, &lower);
            if (status != MC_ITEM_STACK_OK) {
                frame->stack = lower.cause;
                frame->stack_bytecode = lower.context.read.selected && lower.context.read.entered
                                            ? lower.context.read.bytecode
                                            : 0;
                frame->field = lower.cause.field;
                mc_minecart_inventory_stack_context(&frame->inventory, cart->classes->source,
                                                    MC_MINECART_INVENTORY_STACK_NBT_CONSTRUCTOR,
                                                    &lower);
                stack_failure(frame, status, MC_MINECART_INVENTORY_STACK_NBT_CONSTRUCTOR,
                              "Minecart NBT Stack constructor");
                mc_item_stack_release(stack);
                break;
            }
            frame->minecart_read.bytecode = 140;
            mc_minecart_inventory_array_store_owned(target, (size_t)slot, stack);
        }
        /* Retain the detached array and every occurrence through ALL input
         * reads, even a late failure whose input borrows an old Stack source.
         * The new h and its successful prefix are never rolled back. */
        mc_minecart_inventory_array_retire(cart, old);
        if (!completed)
            return false;
    }
    frame->minecart_read.bytecode = 147;
    return true;
}

static mc_nbt_string name(const uint16_t *units, size_t length) {
    return (mc_nbt_string){units, length};
}

static void double_tag(mc_nbt_tag *tag, const uint16_t *units, const double *value) {
    tag->type = MC_NBT_DOUBLE;
    tag->name = name(units, 5);
    memcpy(&tag->value.double_bits, value, sizeof(*value));
}

static mc_minecart_stack_array *write_array(mc_entity *cart, size_t index, uint16_t pc,
                                            mc_entity_nbt_error *frame) {
    frame->minecart_write.bytecode = pc;
    mc_minecart_stack_array *array = mc_minecart_inventory_array_current(cart);
    if (array == NULL) {
        inventory_fail(frame, MC_ENTITY_NBT_NULL_REFERENCE, MC_MINECART_INVENTORY_NULL_REFERENCE,
                       "Minecart NBT inventory receiver");
        return NULL;
    }
    if (index >= mc_minecart_inventory_array_length(array)) {
        inventory_fail(frame, MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS,
                       MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS, "Minecart NBT inventory index");
        return NULL;
    }
    return array;
}

bool mc_minecart_nbt_build_fields_held(mc_entity *cart, const mc_nbt_limits *limits,
                                       mc_nbt_tree **out, mc_entity_nbt_error *frame) {
    frame->minecart_write = own_site(cart, true);
    frame->index = SIZE_MAX;
    frame->field = "Type";
    mc_nbt_tag fields[4] = {0};
    const mc_nbt_tag *own_items[4] = {&fields[0], &fields[1], &fields[2], &fields[3]};
    mc_nbt_tag own = {.type = MC_NBT_COMPOUND, .value.compound = {.items = own_items, .length = 1}};
    frame->minecart_write.bytecode = 7;
    fields[0] = (mc_nbt_tag){.type = MC_NBT_INT,
                             .name = name(type_name, 4),
                             .value.int_value = cart->minecart->fields.d};
    /* These views cover every slot of both genuine h allocations: the
     * constructor's36 and NBT's27. They are bounded native stack scratch,
     * separate from Java temporary-map allocation/OOME semantics. */
    mc_nbt_tag records[36] = {0};
    mc_nbt_tag stack_fields[36][4] = {0};
    const mc_nbt_tag *record_items[36][4];
    const mc_nbt_tag *list_items[36];
    size_t count = 0;
    if (cart->minecart->fields.d == 2) {
        frame->minecart_write.bytecode = 25;
        frame->field = "PushX";
        double_tag(&fields[1], push_x_name, &cart->minecart->fields.f);
        frame->minecart_write.bytecode = 35;
        frame->field = "PushZ";
        double_tag(&fields[2], push_z_name, &cart->minecart->fields.g);
        frame->minecart_write.bytecode = 46;
        frame->field = "Fuel";
        fields[3] = (mc_nbt_tag){.type = MC_NBT_SHORT,
                                 .name = name(fuel_name, 4),
                                 .value.short_value = mc_java_i16_narrow(cart->minecart->fields.e)};
        own.value.compound.length = 4;
    } else if (cart->minecart->fields.d == 1) {
        frame->minecart_write.bytecode = 64;
        frame->field = "Items";
        for (size_t index = 0;; ++index) {
            frame->minecart_write.bytecode = 75;
            frame->index = index;
            frame->inventory.slot_selected = false;
            mc_minecart_stack_array *array = mc_minecart_inventory_array_current(cart);
            if (array == NULL)
                return inventory_fail(frame, MC_ENTITY_NBT_NULL_REFERENCE,
                                      MC_MINECART_INVENTORY_NULL_REFERENCE,
                                      "Minecart NBT inventory length receiver");
            const size_t length = mc_minecart_inventory_array_length(array);
            if (length > 36)
                return inventory_fail(frame, MC_ENTITY_NBT_LIMIT, MC_MINECART_INVENTORY_LIMIT,
                                      "Minecart NBT native inventory extent");
            if (index >= length)
                break;
            frame->inventory.slot_selected = true;
            frame->inventory.slot = (int32_t)index;
            array = write_array(cart, index, 84, frame);
            if (array == NULL)
                return false;
            if (mc_minecart_inventory_array_borrowed_at(array, index) == NULL)
                continue;
            frame->minecart_write.bytecode = 92;
            for (size_t part = 0; part < 4; ++part)
                record_items[count][part] = &stack_fields[count][part];
            records[count] =
                (mc_nbt_tag){.type = MC_NBT_COMPOUND,
                             .value.compound = {.items = record_items[count], .length = 4}};
            frame->minecart_write.bytecode = 103;
            stack_fields[count][0] =
                (mc_nbt_tag){.type = MC_NBT_BYTE,
                             .name = name(slot_name, 4),
                             .value.byte_value = mc_java_i8_narrow((int32_t)index)};
            array = write_array(cart, index, 111, frame);
            if (array == NULL)
                return false;
            mc_item_stack *stack = mc_minecart_inventory_array_borrowed_at(array, index);
            frame->minecart_write.bytecode = 114;
            /* A completed earlier sibling writer is not the selected lower
             * frame of this receiver's native ownership failure. */
            frame->stack_bytecode = 0;
            if (stack == NULL)
                return inventory_fail(frame, MC_ENTITY_NBT_NULL_REFERENCE,
                                      MC_MINECART_INVENTORY_NULL_REFERENCE,
                                      "Minecart NBT Stack writer receiver");
            mc_item_stack_status status = mc_item_stack_retain(stack);
            if (status != MC_ITEM_STACK_OK) {
                frame->stack = (mc_item_stack_error){.status = status};
                return stack_failure(frame, status, MC_MINECART_INVENTORY_STACK_RETAIN,
                                     "Minecart NBT Stack writer ownership");
            }
            mc_item_stack_fields values;
            status = mc_item_stack_fields_get(stack, &values);
            mc_item_stack_release(stack);
            if (status != MC_ITEM_STACK_OK) {
                frame->stack = (mc_item_stack_error){.status = status};
                return stack_failure(frame, status, MC_MINECART_INVENTORY_STACK_FIELDS,
                                     "Minecart NBT Stack writer fields");
            }
            frame->stack_bytecode = 8;
            stack_fields[count][1] =
                (mc_nbt_tag){.type = MC_NBT_SHORT,
                             .name = name(id_name, 2),
                             .value.short_value = mc_java_i16_narrow(values.id)};
            frame->stack_bytecode = 19;
            stack_fields[count][2] =
                (mc_nbt_tag){.type = MC_NBT_BYTE,
                             .name = name(count_name, 5),
                             .value.byte_value = mc_java_i8_narrow(values.count)};
            frame->stack_bytecode = 30;
            stack_fields[count][3] =
                (mc_nbt_tag){.type = MC_NBT_SHORT,
                             .name = name(damage_name, 6),
                             .value.short_value = mc_java_i16_narrow(values.damage)};
            frame->stack_bytecode = 34;
            frame->minecart_write.bytecode = 121;
            list_items[count] = &records[count];
            ++count;
        }
        frame->minecart_write.bytecode = 134;
        frame->index = SIZE_MAX;
        frame->inventory.slot_selected = false;
        fields[1] = (mc_nbt_tag){
            .type = MC_NBT_LIST,
            .name = name(items_name, 5),
            .value.list = {.element_type = MC_NBT_COMPOUND, .items = list_items, .length = count}};
        own.value.compound.length = 2;
    }
    frame->minecart_write.bytecode = 137;
    frame->field = NULL;
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(&own, limits, out, &frame->nbt);
    if (copied != MC_NBT_OK)
        return fail(frame,
                    copied == MC_NBT_LIMIT               ? MC_ENTITY_NBT_LIMIT
                    : copied == MC_NBT_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                         : MC_ENTITY_NBT_NBT_FAILED,
                    "Minecart NBT independent own-field tree");
    return true;
}
