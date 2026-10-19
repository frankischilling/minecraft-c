#include "mc/item/stack/stack.h"

#include "mc/storage/nbt/tree.h"

#include "mc/core/java/java.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Native holds protect identity independently of container registration. */
struct mc_item_stack {
    mc_item_stack_fields fields;
    size_t holds;
    mc_item_stack_phase phase;
    mc_nbt_tree *source;
};

static const uint16_t item_stack_id_name[] = {'i', 'd'};
static const uint16_t item_stack_count_name[] = {'C', 'o', 'u', 'n', 't'};
static const uint16_t item_stack_damage_name[] = {'D', 'a', 'm', 'a', 'g', 'e'};

static mc_item_stack_status item_stack_error(mc_item_stack_error *error,
                                             mc_item_stack_status status, const char *field,
                                             mc_nbt_error nbt)
{
    if (error != NULL) {
        *error = (mc_item_stack_error){status, field, nbt};
    }
    return status;
}

static mc_item_stack_status item_stack_status(mc_item_stack_error *error,
                                              mc_item_stack_status status, const char *field)
{
    return item_stack_error(error, status, field, (mc_nbt_error){MC_NBT_OK, 0});
}

static mc_item_stack_status item_stack_nbt_error(mc_item_stack_error *error, mc_nbt_status status,
                                                 mc_nbt_error nbt)
{
    const mc_item_stack_status result = status == MC_NBT_LIMIT ? MC_ITEM_STACK_LIMIT
                                        : status == MC_NBT_ALLOCATION_FAILED
                                            ? MC_ITEM_STACK_ALLOCATION_FAILED
                                            : MC_ITEM_STACK_NBT_ERROR;
    return item_stack_error(error, result, NULL, nbt);
}

const char *mc_item_stack_status_string(mc_item_stack_status status)
{
    switch (status) {
        case MC_ITEM_STACK_OK:
            return "ok";
        case MC_ITEM_STACK_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_ITEM_STACK_INVALID_PHASE:
            return "invalid construction phase";
        case MC_ITEM_STACK_INVALID_FIELD:
            return "wrong item stack field type";
        case MC_ITEM_STACK_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_ITEM_STACK_LIMIT:
            return "resource limit";
        case MC_ITEM_STACK_NBT_ERROR:
            return "invalid NBT result";
    }
    return "unknown item stack status";
}

static mc_item_stack_status item_stack_ready(const mc_item_stack *stack)
{
    if (stack == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    return stack->phase == MC_ITEM_STACK_READY ? MC_ITEM_STACK_OK : MC_ITEM_STACK_INVALID_PHASE;
}

mc_item_stack_status mc_item_stack_allocate(mc_item_stack **out)
{
    if (out == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    mc_item_stack *stack = calloc(1, sizeof(*stack));
    if (stack == NULL) {
        return MC_ITEM_STACK_ALLOCATION_FAILED;
    }
    stack->holds = 1;
    stack->phase = MC_ITEM_STACK_RAW;
    *out = stack;
    return MC_ITEM_STACK_OK;
}

mc_item_stack_status mc_item_stack_construct(mc_item_stack *raw, int32_t id, int32_t count,
                                             int32_t damage)
{
    if (raw == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    if (raw->phase != MC_ITEM_STACK_RAW) {
        return MC_ITEM_STACK_INVALID_PHASE;
    }
    raw->fields.count = 0;
    raw->fields.id = id;
    raw->fields.count = count;
    raw->fields.damage = damage;
    raw->phase = MC_ITEM_STACK_READY;
    return MC_ITEM_STACK_OK;
}

static mc_item_stack_status item_stack_read_fields(mc_item_stack *stack, const mc_nbt_tag *compound,
                                                   mc_item_stack_error *error)
{
    if (compound == NULL || compound->type != MC_NBT_COMPOUND) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, "id");
    }
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, item_stack_id_name, 2);
    if (tag != NULL && tag->type != MC_NBT_SHORT) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_FIELD, "id");
    }
    stack->fields.id = tag == NULL ? 0 : tag->value.short_value;
    tag = mc_nbt_compound_get(compound, item_stack_count_name, 5);
    if (tag != NULL && tag->type != MC_NBT_BYTE) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_FIELD, "Count");
    }
    stack->fields.count = tag == NULL ? 0 : tag->value.byte_value;
    tag = mc_nbt_compound_get(compound, item_stack_damage_name, 6);
    if (tag != NULL && tag->type != MC_NBT_SHORT) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_FIELD, "Damage");
    }
    stack->fields.damage = tag == NULL ? 0 : tag->value.short_value;
    return item_stack_status(error, MC_ITEM_STACK_OK, NULL);
}

mc_item_stack_status mc_item_stack_construct_nbt(mc_item_stack *raw, const mc_nbt_tag *compound,
                                                 mc_item_stack_error *error)
{
    if (raw == NULL) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
    }
    if (raw->phase != MC_ITEM_STACK_RAW) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_PHASE, NULL);
    }
    raw->phase = MC_ITEM_STACK_FAILED_CONSTRUCTION;
    raw->fields.count = 0;
    const mc_item_stack_status status = item_stack_read_fields(raw, compound, error);
    if (status == MC_ITEM_STACK_OK) {
        raw->phase = MC_ITEM_STACK_READY;
    }
    return status;
}

mc_item_stack_status mc_item_stack_read_nbt(mc_item_stack *stack, const mc_nbt_tag *compound,
                                            mc_item_stack_error *error)
{
    const mc_item_stack_status status = item_stack_ready(stack);
    if (status != MC_ITEM_STACK_OK) {
        return item_stack_status(error, status, NULL);
    }
    return item_stack_read_fields(stack, compound, error);
}

mc_item_stack_status mc_item_stack_retain(mc_item_stack *stack)
{
    if (stack == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    if (stack->holds == SIZE_MAX) {
        return MC_ITEM_STACK_LIMIT;
    }
    ++stack->holds;
    return MC_ITEM_STACK_OK;
}

void mc_item_stack_release(mc_item_stack *stack)
{
    if (stack == NULL) {
        return;
    }
    if (--stack->holds == 0) {
        mc_nbt_tree_free(stack->source);
        free(stack);
    }
}

mc_item_stack_status mc_item_stack_fields_get(const mc_item_stack *stack, mc_item_stack_fields *out)
{
    if (stack == NULL || out == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    *out = stack->fields;
    return MC_ITEM_STACK_OK;
}

mc_item_stack_status mc_item_stack_phase_get(const mc_item_stack *stack, mc_item_stack_phase *out)
{
    if (stack == NULL || out == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    *out = stack->phase;
    return MC_ITEM_STACK_OK;
}

size_t mc_item_stack_object_bytes(void)
{
    return sizeof(mc_item_stack);
}

mc_item_stack_status mc_item_stack_native_bytes(const mc_item_stack *stack, size_t *out)
{
    if (stack == NULL || out == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    const size_t source_bytes = mc_nbt_tree_allocation_bytes(stack->source);
    if (source_bytes > SIZE_MAX - sizeof(*stack)) {
        return MC_ITEM_STACK_LIMIT;
    }
    *out = sizeof(*stack) + source_bytes;
    return MC_ITEM_STACK_OK;
}

mc_item_stack_status mc_item_stack_count_set(mc_item_stack *stack, int32_t count)
{
    const mc_item_stack_status status = item_stack_ready(stack);
    if (status == MC_ITEM_STACK_OK) {
        stack->fields.count = count;
    }
    return status;
}

mc_item_stack_status mc_item_stack_damage_set(mc_item_stack *stack, int32_t damage)
{
    const mc_item_stack_status status = item_stack_ready(stack);
    if (status == MC_ITEM_STACK_OK) {
        stack->fields.damage = damage;
    }
    return status;
}

mc_item_stack_status mc_item_stack_clone(const mc_item_stack *stack, mc_item_stack **out)
{
    if (out == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    mc_item_stack_status status = item_stack_ready(stack);
    if (status != MC_ITEM_STACK_OK) {
        return status;
    }
    mc_item_stack *copy;
    status = mc_item_stack_allocate(&copy);
    if (status != MC_ITEM_STACK_OK) {
        return status;
    }
    status =
        mc_item_stack_construct(copy, stack->fields.id, stack->fields.count, stack->fields.damage);
    if (status != MC_ITEM_STACK_OK) {
        mc_item_stack_release(copy);
        return status;
    }
    *out = copy;
    return MC_ITEM_STACK_OK;
}

mc_item_stack_status mc_item_stack_clone_nullable(const mc_item_stack *stack, mc_item_stack **out)
{
    if (out == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    if (stack == NULL) {
        *out = NULL;
        return MC_ITEM_STACK_OK;
    }
    return mc_item_stack_clone(stack, out);
}

mc_item_stack_status mc_item_stack_split(mc_item_stack *stack, int32_t amount, mc_item_stack **out)
{
    if (out == NULL) {
        return MC_ITEM_STACK_INVALID_ARGUMENT;
    }
    mc_item_stack_status status = item_stack_ready(stack);
    if (status != MC_ITEM_STACK_OK) {
        return status;
    }
    stack->fields.count = mc_java_i32_sub(stack->fields.count, amount);
    mc_item_stack *split;
    status = mc_item_stack_allocate(&split);
    if (status != MC_ITEM_STACK_OK) {
        return status;
    }
    status = mc_item_stack_construct(split, stack->fields.id, amount, stack->fields.damage);
    if (status != MC_ITEM_STACK_OK) {
        mc_item_stack_release(split);
        return status;
    }
    *out = split;
    return MC_ITEM_STACK_OK;
}

static mc_item_stack_status item_stack_limits(const mc_item_stack *stack,
                                              const mc_nbt_limits *requested,
                                              mc_nbt_limits *remaining, mc_item_stack_error *error)
{
    *remaining = requested == NULL ? mc_nbt_default_limits() : *requested;
    if (remaining->max_depth == 0 || remaining->max_depth > 128) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
    }
    if (sizeof(*stack) > remaining->max_allocation_bytes) {
        return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
    }
    remaining->max_allocation_bytes -= sizeof(*stack);
    const size_t source_bytes = mc_nbt_tree_allocation_bytes(stack->source);
    if (source_bytes > remaining->max_allocation_bytes) {
        return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
    }
    remaining->max_allocation_bytes -= source_bytes;
    return MC_ITEM_STACK_OK;
}

mc_item_stack_status mc_item_stack_preserve_source(mc_item_stack *stack, const mc_nbt_tag *compound,
                                                   const mc_nbt_limits *requested_limits,
                                                   mc_item_stack_error *error)
{
    mc_item_stack_status status = item_stack_ready(stack);
    if (status != MC_ITEM_STACK_OK) {
        return item_stack_status(error, status, NULL);
    }
    if (compound == NULL || compound->type != MC_NBT_COMPOUND) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
    }
    mc_nbt_limits limits;
    status = item_stack_limits(stack, requested_limits, &limits, error);
    if (status != MC_ITEM_STACK_OK) {
        return status;
    }
    mc_nbt_error nbt = {MC_NBT_OK, 0};
    mc_nbt_tree *source;
    const mc_nbt_status checked = mc_nbt_tree_copy_compound(compound, &limits, &source, &nbt);
    if (checked != MC_NBT_OK) {
        return item_stack_nbt_error(error, checked, nbt);
    }
    mc_nbt_tree *old_source = stack->source;
    stack->source = source;
    mc_nbt_tree_free(old_source);
    return item_stack_status(error, MC_ITEM_STACK_OK, NULL);
}

const mc_nbt_tag *mc_item_stack_source(const mc_item_stack *stack)
{
    return stack == NULL ? NULL : mc_nbt_tree_root(stack->source);
}

static bool item_stack_names_equal(mc_nbt_string left, mc_nbt_string right)
{
    return left.length == right.length &&
           (left.length == 0 ||
            memcmp(left.units, right.units, left.length * sizeof(uint16_t)) == 0);
}

static mc_nbt_string item_stack_field_name(size_t index)
{
    if (index == 0) {
        return (mc_nbt_string){item_stack_id_name, 2};
    }
    if (index == 1) {
        return (mc_nbt_string){item_stack_count_name, 5};
    }
    return (mc_nbt_string){item_stack_damage_name, 6};
}

mc_item_stack_status mc_item_stack_record_pointer_count(const mc_item_stack *stack,
                                                        const mc_nbt_tag *extra_fields,
                                                        size_t extra_count, size_t *out,
                                                        mc_item_stack_error *error)
{
    if (out == NULL || (extra_fields == NULL && extra_count != 0)) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
    }
    const mc_item_stack_status ready = item_stack_ready(stack);
    if (ready != MC_ITEM_STACK_OK) {
        return item_stack_status(error, ready, NULL);
    }
    if (extra_count > SIZE_MAX / sizeof(*extra_fields)) {
        return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
    }
    for (size_t i = 0; i < extra_count; ++i) {
        const mc_nbt_tag *extra = &extra_fields[i];
        if (extra->type <= MC_NBT_END || extra->type > MC_NBT_COMPOUND ||
            (extra->name.units == NULL && extra->name.length != 0)) {
            return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
        }
        if (extra->name.length > SIZE_MAX / sizeof(uint16_t)) {
            return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
        }
        for (size_t j = 0; j < 3; ++j) {
            if (item_stack_names_equal(extra->name, item_stack_field_name(j))) {
                return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
            }
        }
        for (size_t j = 0; j < i; ++j) {
            if (item_stack_names_equal(extra->name, extra_fields[j].name)) {
                return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
            }
        }
    }
    const mc_nbt_tag *source = mc_nbt_tree_root(stack->source);
    size_t count = source == NULL ? 0 : source->value.compound.length;
    for (size_t i = 0; i < 3; ++i) {
        const mc_nbt_string name = item_stack_field_name(i);
        if (source == NULL || mc_nbt_compound_get(source, name.units, name.length) == NULL) {
            if (count == SIZE_MAX) {
                return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
            }
            ++count;
        }
    }
    for (size_t i = 0; i < extra_count; ++i) {
        const mc_nbt_string name = extra_fields[i].name;
        if (source == NULL || mc_nbt_compound_get(source, name.units, name.length) == NULL) {
            if (count == SIZE_MAX) {
                return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
            }
            ++count;
        }
    }
    if (count > SIZE_MAX / sizeof(const mc_nbt_tag *)) {
        return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
    }
    *out = count;
    return item_stack_status(error, MC_ITEM_STACK_OK, NULL);
}

mc_item_stack_status
mc_item_stack_record_overlay(const mc_item_stack *stack, const mc_nbt_tag *extra_fields,
                             size_t extra_count, const mc_nbt_tag **child_scratch,
                             size_t scratch_count, mc_item_stack_record_view *out,
                             mc_item_stack_error *error)
{
    if (out == NULL) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
    }
    size_t count;
    const mc_item_stack_status planned =
        mc_item_stack_record_pointer_count(stack, extra_fields, extra_count, &count, error);
    if (planned != MC_ITEM_STACK_OK) {
        return planned;
    }
    if (child_scratch == NULL || scratch_count < count) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
    }

    /* All fallible preflight is complete. References below target FINAL out
     * storage, never a temporary view copied with dangling self-pointers. */
    mc_nbt_tag fields[3] = {0};
    fields[0].type = fields[2].type = MC_NBT_SHORT;
    fields[1].type = MC_NBT_BYTE;
    for (size_t i = 0; i < 3; ++i) {
        fields[i].name = item_stack_field_name(i);
        out->item_fields[i] = fields[i];
    }
    out->item_fields[0].value.short_value = mc_java_i16_narrow(stack->fields.id);
    out->item_fields[1].value.byte_value = mc_java_i8_narrow(stack->fields.count);
    out->item_fields[2].value.short_value = mc_java_i16_narrow(stack->fields.damage);
    const mc_nbt_tag *source = mc_nbt_tree_root(stack->source);
    size_t used = 0;
    if (source != NULL) {
        for (size_t i = 0; i < source->value.compound.length; ++i) {
            const mc_nbt_tag *old = source->value.compound.items[i];
            const mc_nbt_tag *replacement = old;
            bool found = false;
            for (size_t j = 0; j < 3; ++j) {
                if (item_stack_names_equal(old->name, out->item_fields[j].name)) {
                    replacement = &out->item_fields[j];
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (size_t j = 0; j < extra_count; ++j) {
                    if (item_stack_names_equal(old->name, extra_fields[j].name)) {
                        replacement = &extra_fields[j];
                        break;
                    }
                }
            }
            child_scratch[used++] = replacement;
        }
    }
    for (size_t i = 0; i < 3; ++i) {
        const mc_nbt_string name = out->item_fields[i].name;
        if (source == NULL || mc_nbt_compound_get(source, name.units, name.length) == NULL) {
            child_scratch[used++] = &out->item_fields[i];
        }
    }
    for (size_t i = 0; i < extra_count; ++i) {
        const mc_nbt_string name = extra_fields[i].name;
        if (source == NULL || mc_nbt_compound_get(source, name.units, name.length) == NULL) {
            child_scratch[used++] = &extra_fields[i];
        }
    }
    out->root = (mc_nbt_tag){0};
    out->root.type = MC_NBT_COMPOUND;
    if (source != NULL) {
        out->root.name = source->name;
    }
    out->root.value.compound.items = child_scratch;
    out->root.value.compound.length = used;
    return item_stack_status(error, MC_ITEM_STACK_OK, NULL);
}

static size_t item_stack_overlay(const mc_nbt_tag *baseline, const mc_nbt_tag fields[3],
                                 const mc_nbt_tag **items)
{
    bool used[3] = {false};
    size_t count = 0;
    if (baseline != NULL) {
        for (size_t i = 0; i < baseline->value.compound.length; ++i) {
            const mc_nbt_tag *old = baseline->value.compound.items[i];
            size_t replacement = 3;
            for (size_t j = 0; j < 3; ++j) {
                if (item_stack_names_equal(old->name, fields[j].name)) {
                    replacement = j;
                    break;
                }
            }
            if (replacement == 3) {
                items[count++] = old;
            } else {
                used[replacement] = true;
                items[count++] = &fields[replacement];
            }
        }
    }
    for (size_t i = 0; i < 3; ++i) {
        if (!used[i]) {
            items[count++] = &fields[i];
        }
    }
    return count;
}

mc_item_stack_status mc_item_stack_build(const mc_item_stack *stack,
                                         const mc_nbt_limits *requested_limits,
                                         mc_nbt_document **out, mc_item_stack_error *error)
{
    if (out == NULL) {
        return item_stack_status(error, MC_ITEM_STACK_INVALID_ARGUMENT, NULL);
    }
    mc_item_stack_status result = item_stack_ready(stack);
    if (result != MC_ITEM_STACK_OK) {
        return item_stack_status(error, result, NULL);
    }
    mc_nbt_limits limits;
    result = item_stack_limits(stack, requested_limits, &limits, error);
    if (result != MC_ITEM_STACK_OK) {
        return result;
    }
    const mc_nbt_tag *source = mc_nbt_tree_root(stack->source);
    const size_t old_count = source == NULL ? 0 : source->value.compound.length;
    if (old_count > limits.max_elements || old_count > SIZE_MAX - 3u) {
        return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
    }
    const size_t pointer_count = old_count + 3u;
    if (pointer_count > SIZE_MAX / sizeof(const mc_nbt_tag *)) {
        return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
    }
    const size_t scratch_bytes = pointer_count * sizeof(const mc_nbt_tag *);
    if (scratch_bytes > limits.max_allocation_bytes) {
        return item_stack_status(error, MC_ITEM_STACK_LIMIT, NULL);
    }
    const mc_nbt_tag **items = malloc(scratch_bytes);
    if (items == NULL) {
        return item_stack_status(error, MC_ITEM_STACK_ALLOCATION_FAILED, NULL);
    }
    limits.max_allocation_bytes -= scratch_bytes;
    mc_nbt_tag fields[3] = {0};
    fields[0].type = fields[2].type = MC_NBT_SHORT;
    fields[1].type = MC_NBT_BYTE;
    fields[0].name = (mc_nbt_string){item_stack_id_name, 2};
    fields[1].name = (mc_nbt_string){item_stack_count_name, 5};
    fields[2].name = (mc_nbt_string){item_stack_damage_name, 6};
    fields[0].value.short_value = mc_java_i16_narrow(stack->fields.id);
    fields[1].value.byte_value = mc_java_i8_narrow(stack->fields.count);
    fields[2].value.short_value = mc_java_i16_narrow(stack->fields.damage);
    mc_nbt_tag root = {0};
    root.type = MC_NBT_COMPOUND;
    if (source != NULL) {
        root.name = source->name;
    }
    root.value.compound.items = items;
    root.value.compound.length = item_stack_overlay(source, fields, items);
    mc_nbt_error nbt = {MC_NBT_OK, 0};
    size_t length = 0;
    mc_nbt_status status = mc_nbt_measure(&root, &limits, &length, &nbt);
    uint8_t *encoded = NULL;
    mc_nbt_document *document = NULL;
    if (status == MC_NBT_OK) {
        if (length > limits.max_allocation_bytes) {
            status = MC_NBT_LIMIT;
            nbt = (mc_nbt_error){status, 0};
        } else {
            encoded = malloc(length);
            if (encoded == NULL) {
                status = MC_NBT_ALLOCATION_FAILED;
                nbt = (mc_nbt_error){status, 0};
            }
        }
    }
    if (status == MC_NBT_OK) {
        limits.max_allocation_bytes -= length;
        size_t written = 0;
        status = mc_nbt_write(&root, &limits, encoded, length, &written, &nbt);
        if (status == MC_NBT_OK) {
            status = mc_nbt_read_compound(encoded, written, &limits, &document, NULL, &nbt);
        }
    }
    free(encoded);
    free(items);
    if (status != MC_NBT_OK) {
        return item_stack_nbt_error(error, status, nbt);
    }
    *out = document;
    return item_stack_status(error, MC_ITEM_STACK_OK, NULL);
}
