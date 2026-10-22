#include "storage_internal.h"

#include "entity_internal.h"
#include "mc/storage/nbt/overlay.h"

#include <limits.h>
#include <stdlib.h>

static const uint16_t type_name[] = {'T', 'y', 'p', 'e'};
static const uint16_t items_name[] = {'I', 't', 'e', 'm', 's'};
static const uint16_t push_x_name[] = {'P', 'u', 's', 'h', 'X'};
static const uint16_t push_z_name[] = {'P', 'u', 's', 'h', 'Z'};
static const uint16_t fuel_name[] = {'F', 'u', 'e', 'l'};
static const uint16_t slot_name[] = {'S', 'l', 'o', 't'};
static const uint16_t id_name[] = {'i', 'd'};
static const uint16_t count_name[] = {'C', 'o', 'u', 'n', 't'};
static const uint16_t damage_name[] = {'D', 'a', 'm', 'a', 'g', 'e'};

static mc_entity_nbt_status fail(mc_entity_nbt_error *frame, mc_entity_nbt_status status,
                                 const char *operation) {
    frame->status = status;
    frame->operation = operation;
    return status;
}

static mc_entity_nbt_status nbt_fail(mc_entity_nbt_error *frame, mc_nbt_status status,
                                     const char *operation) {
    return fail(frame,
                status == MC_NBT_LIMIT               ? MC_ENTITY_NBT_LIMIT
                : status == MC_NBT_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                     : MC_ENTITY_NBT_NBT_FAILED,
                operation);
}

static mc_entity_nbt_status stack_fail(mc_entity_nbt_error *frame, mc_item_stack_status status,
                                       const char *operation) {
    frame->stack.status = status;
    return fail(frame,
                status == MC_ITEM_STACK_LIMIT               ? MC_ENTITY_NBT_LIMIT
                : status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                            : MC_ENTITY_NBT_STACK_FAILED,
                operation);
}

static bool add(size_t *sum, size_t bytes) {
    if (bytes > SIZE_MAX - *sum)
        return false;
    *sum += bytes;
    return true;
}

static bool sidecar_bytes(size_t length, size_t *out) {
    if (length > (SIZE_MAX - sizeof(mc_minecart_storage_sidecar)) / sizeof(const mc_nbt_tag *))
        return false;
    *out = sizeof(mc_minecart_storage_sidecar) + length * sizeof(const mc_nbt_tag *);
    return true;
}

static bool array_valid(const mc_minecart_stack_array *array) {
    return array == NULL || array->length == 27 || array->length == 36;
}

static void generation_release(mc_minecart_storage_generation *generation) {
    if (generation != NULL && --generation->references == 0) {
        mc_nbt_tree_free(generation->tree);
        free(generation);
    }
}

static void sidecar_release(mc_minecart_storage_sidecar *sidecar) {
    if (sidecar != NULL) {
        generation_release(sidecar->generation);
        free(sidecar);
    }
}

const mc_nbt_tag *mc_minecart_storage_occurrence_source(const mc_minecart_stack_array *array,
                                                        size_t index) {
    return array->storage == NULL ? NULL : array->storage->records[index];
}

void mc_minecart_storage_array_sources_retire(mc_minecart_stack_array *array) {
    mc_minecart_storage_sidecar *old = array->storage;
    array->storage = NULL;
    sidecar_release(old);
}

void mc_minecart_storage_occurrence_clear(mc_minecart_stack_array *array, size_t index) {
    mc_minecart_storage_sidecar *sidecar = array->storage;
    if (sidecar == NULL)
        return;
    sidecar->records[index] = NULL;
    for (size_t i = 0; i < sidecar->length; ++i) {
        if (sidecar->records[i] != NULL)
            return;
    }
    mc_minecart_storage_array_sources_retire(array);
}

void mc_minecart_storage_root_retire(mc_entity *cart) {
    mc_minecart_storage_generation *old = cart->minecart->root_generation;
    cart->minecart->root_generation = NULL;
    cart->storage_source = NULL;
    generation_release(old);
}

static bool generation_bytes(const mc_minecart_storage_generation *generation, size_t *bytes) {
    return generation == NULL || (generation->references != 0 && generation->tree != NULL &&
                                  add(bytes, sizeof(*generation)) &&
                                  add(bytes, mc_nbt_tree_allocation_bytes(generation->tree)));
}

mc_entity_nbt_status mc_minecart_storage_native_bytes(const mc_entity *cart, size_t *out,
                                                      mc_entity_nbt_error *frame) {
    if (!mc_entity_is_kind(cart, MC_ENTITY_CLASS_KIND_MINECART) || out == NULL)
        return fail(frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Minecart storage byte arguments");
    const mc_minecart_stack_array *array = cart->minecart->fields.h;
    const mc_minecart_storage_generation *root = cart->minecart->root_generation;
    if (!array_valid(array) || cart->storage_source != (root == NULL ? NULL : root->tree))
        return fail(frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Minecart storage owner shape");
    size_t bytes = mc_entity_native_bytes(cart);
    if (!generation_bytes(root, &bytes))
        return fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage root generation extent");
    if (array != NULL) {
        const mc_minecart_storage_sidecar *sidecar = array->storage;
        if (sidecar != NULL) {
            size_t extent;
            if (sidecar->length != array->length || sidecar->generation == NULL ||
                !sidecar_bytes(sidecar->length, &extent) || !add(&bytes, extent))
                return fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage sidecar extent");
            if (sidecar->generation != root && !generation_bytes(sidecar->generation, &bytes))
                return fail(frame, MC_ENTITY_NBT_LIMIT,
                            "Minecart storage retained generation extent");
        }
        for (size_t i = 0; i < array->length; ++i) {
            const mc_item_stack *stack = array->elements[i];
            if (stack == NULL)
                continue;
            bool seen = false;
            for (size_t j = 0; j < i; ++j)
                seen = seen || array->elements[j] == stack;
            if (seen)
                continue;
            size_t extent;
            const mc_item_stack_status status = mc_item_stack_native_bytes(stack, &extent);
            if (status != MC_ITEM_STACK_OK)
                return stack_fail(frame, status, "Minecart storage Stack byte inspection");
            if (!add(&bytes, extent))
                return fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage distinct Stack extent");
        }
    }
    *out = bytes;
    return MC_ENTITY_NBT_OK;
}

mc_entity_nbt_status mc_minecart_storage_preserve_source(mc_entity *cart,
                                                         const mc_nbt_tag *compound,
                                                         const mc_nbt_limits *requested,
                                                         mc_entity_nbt_error *frame) {
    if (!mc_entity_is_kind(cart, MC_ENTITY_CLASS_KIND_MINECART) || compound == NULL ||
        compound->type != MC_NBT_COMPOUND)
        return fail(frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Minecart storage provenance arguments");
    mc_nbt_limits limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    size_t live;
    if (mc_minecart_storage_native_bytes(cart, &live, frame) != MC_ENTITY_NBT_OK)
        return frame->status;
    if (live > limits.max_allocation_bytes ||
        sizeof(mc_minecart_storage_generation) > limits.max_allocation_bytes - live)
        return fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage new generation budget");
    mc_minecart_storage_generation *prepared = malloc(sizeof(*prepared));
    if (prepared == NULL)
        return fail(frame, MC_ENTITY_NBT_ALLOCATION_FAILED,
                    "Minecart storage generation allocation");
    prepared->references = 1;
    prepared->tree = NULL;
    limits.max_allocation_bytes -= live + sizeof(*prepared);
    const mc_nbt_status copied =
        mc_nbt_tree_copy_compound(compound, &limits, &prepared->tree, &frame->nbt);
    if (copied != MC_NBT_OK) {
        generation_release(prepared);
        return nbt_fail(frame, copied, "Minecart storage independent input tree");
    }
    const size_t tree_bytes = mc_nbt_tree_allocation_bytes(prepared->tree);
    mc_minecart_storage_sidecar *replacement = NULL;
    mc_minecart_stack_array *array = cart->minecart->fields.h;
    const bool inventory = cart->minecart->fields.d == 1;
    if (inventory) {
        frame->field = "Items";
        if (array == NULL) {
            fail(frame, MC_ENTITY_NBT_NULL_REFERENCE, "Minecart storage current inventory");
            goto failed;
        }
        const mc_nbt_tag *items =
            mc_nbt_compound_get(mc_nbt_tree_root(prepared->tree), items_name, 5);
        if (items != NULL && items->type != MC_NBT_LIST) {
            fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Minecart storage provenance Items");
            goto failed;
        }
        const mc_nbt_tag *winners[36] = {0};
        size_t winner_indices[36] = {0};
        const size_t count = items == NULL ? 0 : items->value.list.length;
        if (count > INT32_MAX) {
            fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage read List extent");
            goto failed;
        }
        for (size_t i = 0; i < count; ++i) {
            frame->index = i;
            frame->inventory.slot_selected = false;
            const mc_nbt_tag *record = items->value.list.items[i];
            if (record == NULL || record->type != MC_NBT_COMPOUND) {
                fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Minecart storage provenance record");
                goto failed;
            }
            const mc_nbt_tag *slot = mc_nbt_compound_get(record, slot_name, 4);
            if (slot != NULL && slot->type != MC_NBT_BYTE) {
                frame->field = "Slot";
                fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Minecart storage provenance Slot");
                goto failed;
            }
            const size_t decoded =
                (size_t)((slot == NULL ? 0 : (int32_t)slot->value.byte_value) & 255);
            /* Data coordinates only, not a selected/entered inventory method. */
            frame->inventory.slot_selected = true;
            frame->inventory.slot = (int32_t)decoded;
            /* Source Type1 read always allocates27, even if a caller later
             * supplies its metadata to an actual constructor h36. */
            if (decoded < 27 && decoded < array->length && array->elements[decoded] != NULL) {
                winners[decoded] = record;
                winner_indices[decoded] = i;
            }
        }
        bool any = false;
        for (size_t i = 0; i < array->length; ++i) {
            if (winners[i] == NULL)
                continue;
            any = true;
            frame->index = winner_indices[i];
            frame->inventory.slot_selected = true;
            frame->inventory.slot = (int32_t)i;
            mc_item_stack_phase phase;
            mc_item_stack_status status = mc_item_stack_phase_get(array->elements[i], &phase);
            if (status == MC_ITEM_STACK_OK && phase != MC_ITEM_STACK_READY)
                status = MC_ITEM_STACK_INVALID_PHASE;
            if (status != MC_ITEM_STACK_OK) {
                stack_fail(frame, status, "Minecart storage successfully read occurrence");
                goto failed;
            }
        }
        if (any) {
            frame->index = SIZE_MAX;
            frame->inventory.slot_selected = false;
            frame->inventory.slot = 0;
            size_t bytes;
            if (!sidecar_bytes(array->length, &bytes) || tree_bytes > limits.max_allocation_bytes ||
                bytes > limits.max_allocation_bytes - tree_bytes) {
                fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage prepared sidecar budget");
                goto failed;
            }
            replacement = malloc(bytes);
            if (replacement == NULL) {
                fail(frame, MC_ENTITY_NBT_ALLOCATION_FAILED, "Minecart storage sidecar allocation");
                goto failed;
            }
            replacement->generation = prepared;
            replacement->length = array->length;
            for (size_t i = 0; i < array->length; ++i)
                replacement->records[i] = winners[i];
            /* A new generation has one local/root hold. This second hold
             * is prepared before any visible attachment changes. */
            ++prepared->references;
        }
    }
    /* No allocation, source call or admission remains after this point. */
    mc_minecart_storage_generation *old_root = cart->minecart->root_generation;
    mc_minecart_storage_sidecar *old_sidecar = inventory ? array->storage : NULL;
    cart->minecart->root_generation = prepared;
    cart->storage_source = prepared->tree;
    if (inventory)
        array->storage = replacement;
    sidecar_release(old_sidecar);
    generation_release(old_root);
    frame->field = NULL;
    frame->index = SIZE_MAX;
    frame->inventory.slot_selected = false;
    frame->inventory.slot = 0;
    return MC_ENTITY_NBT_OK;
failed:
    sidecar_release(replacement);
    generation_release(prepared);
    return frame->status;
}

static bool named(mc_nbt_string name, const uint16_t *units, size_t length) {
    if (name.length != length)
        return false;
    for (size_t i = 0; i < length; ++i) {
        if (name.units[i] != units[i])
            return false;
    }
    return true;
}

static bool own_conditional(mc_nbt_string name) {
    return named(name, items_name, 5) || named(name, push_x_name, 5) ||
           named(name, push_z_name, 5) || named(name, fuel_name, 4);
}

static const mc_nbt_tag *same_name(const mc_nbt_tag *compound, mc_nbt_string name) {
    return compound == NULL ? NULL : mc_nbt_compound_get(compound, name.units, name.length);
}

static bool field_type(const mc_nbt_tag *compound, const uint16_t *units, size_t length,
                       mc_nbt_type type) {
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, units, length);
    return tag != NULL && tag->type == type;
}

mc_entity_nbt_status mc_minecart_storage_overlay_after_semantic(const mc_entity *cart,
                                                                const mc_nbt_tree *semantic,
                                                                const mc_nbt_limits *remaining,
                                                                mc_nbt_tree **out,
                                                                mc_entity_nbt_error *frame) {
    const mc_nbt_tag *known = mc_nbt_tree_root(semantic);
    if (known == NULL || known->type != MC_NBT_COMPOUND ||
        !field_type(known, type_name, 4, MC_NBT_INT))
        return fail(frame, MC_ENTITY_NBT_NBT_FAILED, "Minecart storage semantic root shape");
    const int32_t type = mc_nbt_compound_get(known, type_name, 4)->value.int_value;
    const size_t expected = type == 1 ? 10u : type == 2 ? 12u : 9u;
    if (known->value.compound.length != expected)
        return fail(frame, MC_ENTITY_NBT_NBT_FAILED, "Minecart storage semantic field count");
    const mc_nbt_tag *known_items = type == 1 ? mc_nbt_compound_get(known, items_name, 5) : NULL;
    const mc_minecart_stack_array *array = cart->minecart->fields.h;
    if (type == 1 && (known_items == NULL || known_items->type != MC_NBT_LIST ||
                      known_items->value.list.element_type != MC_NBT_COMPOUND ||
                      known_items->value.list.length > 36 || array == NULL))
        return fail(frame, MC_ENTITY_NBT_NBT_FAILED, "Minecart storage semantic Items shape");
    const size_t count = known_items == NULL ? 0 : known_items->value.list.length;
    const mc_nbt_tag *sources[36] = {0};
    size_t record_counts[36] = {0};
    size_t pointer_count = count;
    for (size_t i = 0; i < count; ++i) {
        frame->field = "Items";
        frame->index = i;
        frame->inventory.slot_selected = false;
        const mc_nbt_tag *record = known_items->value.list.items[i];
        if (record == NULL || record->type != MC_NBT_COMPOUND ||
            record->value.compound.length != 4 || !field_type(record, slot_name, 4, MC_NBT_BYTE) ||
            !field_type(record, id_name, 2, MC_NBT_SHORT) ||
            !field_type(record, count_name, 5, MC_NBT_BYTE) ||
            !field_type(record, damage_name, 6, MC_NBT_SHORT))
            return fail(frame, MC_ENTITY_NBT_NBT_FAILED, "Minecart storage semantic record shape");
        const mc_nbt_tag *slot = mc_nbt_compound_get(record, slot_name, 4);
        const size_t index = (size_t)((int32_t)slot->value.byte_value & 255);
        frame->inventory.slot_selected = true;
        frame->inventory.slot = (int32_t)index;
        if (index >= array->length)
            return fail(frame, MC_ENTITY_NBT_NBT_FAILED, "Minecart storage generated Slot extent");
        sources[i] = mc_minecart_storage_occurrence_source(array, index);
        const mc_nbt_status status =
            mc_nbt_compound_overlay_count(sources[i], record, &record_counts[i], &frame->nbt);
        if (status != MC_NBT_OK)
            return nbt_fail(frame, status, "Minecart storage record overlay extent");
        if (record_counts[i] > remaining->max_elements || !add(&pointer_count, record_counts[i]))
            return fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage record pointer extent");
    }
    const mc_nbt_tag *source = mc_entity_storage_source(cart);
    size_t kept = 0;
    if (source != NULL) {
        for (size_t i = 0; i < source->value.compound.length; ++i) {
            const mc_nbt_tag *field = source->value.compound.items[i];
            if (!own_conditional(field->name) || same_name(known, field->name) != NULL)
                ++kept;
        }
    }
    size_t root_count = kept;
    for (size_t i = 0; i < known->value.compound.length; ++i) {
        if (same_name(source, known->value.compound.items[i]->name) == NULL && !add(&root_count, 1))
            return fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage root overlay extent");
    }
    if (kept > remaining->max_elements || root_count > remaining->max_elements ||
        count > remaining->max_elements || !add(&pointer_count, kept) ||
        !add(&pointer_count, root_count) || pointer_count > SIZE_MAX / sizeof(const mc_nbt_tag *))
        return fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage complete pointer extent");
    const size_t scratch_bytes = pointer_count * sizeof(const mc_nbt_tag *);
    if (scratch_bytes > remaining->max_allocation_bytes)
        return fail(frame, MC_ENTITY_NBT_LIMIT, "Minecart storage overlay pointer budget");
    const mc_nbt_tag **scratch = malloc(scratch_bytes);
    if (scratch == NULL)
        return fail(frame, MC_ENTITY_NBT_ALLOCATION_FAILED, "Minecart storage overlay pointers");
    const mc_nbt_tag **list = scratch;
    size_t used = count;
    mc_nbt_tag records[36] = {0};
    mc_nbt_status status = MC_NBT_OK;
    for (size_t i = 0; i < count && status == MC_NBT_OK; ++i) {
        frame->field = "Items";
        frame->index = i;
        frame->inventory.slot_selected = true;
        frame->inventory.slot =
            (int32_t)mc_nbt_compound_get(known_items->value.list.items[i], slot_name, 4)
                ->value.byte_value &
            255;
        status =
            mc_nbt_compound_overlay(sources[i], known_items->value.list.items[i], scratch + used,
                                    record_counts[i], &records[i], &frame->nbt);
        if (status == MC_NBT_OK) {
            records[i].name = known_items->value.list.items[i]->name;
            list[i] = &records[i];
            used += record_counts[i];
        }
    }
    mc_nbt_tag items = {0};
    if (known_items != NULL) {
        items = *known_items;
        items.value.list.items = list;
    }
    const mc_nbt_tag **filtered_items = scratch + used;
    if (status == MC_NBT_OK && source != NULL) {
        for (size_t i = 0, index = 0; i < source->value.compound.length; ++i) {
            const mc_nbt_tag *field = source->value.compound.items[i];
            if (!own_conditional(field->name) || same_name(known, field->name) != NULL)
                filtered_items[index++] = field;
        }
    }
    used += kept;
    mc_nbt_tag filtered = {0};
    if (source != NULL) {
        filtered = *source;
        filtered.value.compound.items = filtered_items;
        filtered.value.compound.length = kept;
    }
    const mc_nbt_tag *known_children[12];
    for (size_t i = 0; i < known->value.compound.length; ++i) {
        const mc_nbt_tag *field = known->value.compound.items[i];
        known_children[i] = field == known_items ? &items : field;
    }
    mc_nbt_tag fields = *known;
    fields.value.compound.items = known_children;
    mc_nbt_tag result = {0};
    if (status == MC_NBT_OK) {
        frame->field = NULL;
        frame->index = SIZE_MAX;
        frame->inventory.slot_selected = false;
        frame->inventory.slot = 0;
        status = mc_nbt_compound_overlay(source == NULL ? NULL : &filtered, &fields, scratch + used,
                                         root_count, &result, &frame->nbt);
    }
    mc_nbt_tree *tree = NULL;
    if (status == MC_NBT_OK) {
        mc_nbt_limits limits = *remaining;
        limits.max_allocation_bytes -= scratch_bytes;
        status = mc_nbt_tree_copy_compound(&result, &limits, &tree, &frame->nbt);
    }
    free(scratch);
    if (status != MC_NBT_OK)
        return nbt_fail(frame, status, "Minecart storage independent overlay tree");
    *out = tree;
    return MC_ENTITY_NBT_OK;
}
