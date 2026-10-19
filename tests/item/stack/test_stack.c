#include "mc/item/stack/stack.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native ItemStack ownership and serialization contracts. */
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const uint16_t id_name[] = {'i', 'd'};
static const uint16_t count_name[] = {'C', 'o', 'u', 'n', 't'};
static const uint16_t damage_name[] = {'D', 'a', 'm', 'a', 'g', 'e'};
static const uint16_t slot_name[] = {'S', 'l', 'o', 't'};
static const uint16_t payload_name[] = {'P', 'a', 'y', 'l', 'o', 'a', 'd'};
static const uint16_t float_name[] = {'F'};
static const uint16_t root_name[] = {0x2603};

static const mc_nbt_tag *get(const mc_nbt_tag *root, const uint16_t *name, size_t length)
{
    return mc_nbt_compound_get(root, name, length);
}

static int construction_prefixes(void)
{
    int result = 1;
    mc_item_stack *raw = NULL, *live = NULL;
    mc_nbt_tag id = {.type = MC_NBT_SHORT, .name = {id_name, 2}, .value.short_value = -42};
    mc_nbt_tag bad_count = {.type = MC_NBT_INT, .name = {count_name, 5}, .value.int_value = 19};
    mc_nbt_tag damage = {.type = MC_NBT_SHORT, .name = {damage_name, 6}, .value.short_value = 5};
    const mc_nbt_tag *children[] = {&id, &bad_count, &damage};
    mc_nbt_tag input = {.type = MC_NBT_COMPOUND, .value.compound = {children, 3}};
    mc_item_stack_fields fields;
    mc_item_stack_phase phase;
    mc_item_stack_error error;
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct_nbt(raw, &input, &error) == MC_ITEM_STACK_INVALID_FIELD);
    CHECK(error.field != NULL && strcmp(error.field, "Count") == 0);
    CHECK(mc_item_stack_fields_get(raw, &fields) == MC_ITEM_STACK_OK);
    CHECK(fields.id == -42 && fields.count == 0 && fields.damage == 0 && fields.animation == 0);
    CHECK(mc_item_stack_phase_get(raw, &phase) == MC_ITEM_STACK_OK);
    CHECK(phase == MC_ITEM_STACK_FAILED_CONSTRUCTION);
    CHECK(mc_item_stack_construct(raw, 1, 2, 3) == MC_ITEM_STACK_INVALID_PHASE);
    CHECK(mc_item_stack_source(raw) == NULL);
    mc_item_stack_release(raw);
    raw = NULL;

    CHECK(mc_item_stack_allocate(&live) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct(live, 17, 19, 21) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_read_nbt(live, &input, &error) == MC_ITEM_STACK_INVALID_FIELD);
    CHECK(mc_item_stack_fields_get(live, &fields) == MC_ITEM_STACK_OK);
    CHECK(fields.id == -42 && fields.count == 19 && fields.damage == 21);
    CHECK(mc_item_stack_phase_get(live, &phase) == MC_ITEM_STACK_OK);
    CHECK(phase == MC_ITEM_STACK_READY);
    mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    CHECK(mc_item_stack_read_nbt(live, &empty, &error) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_fields_get(live, &fields) == MC_ITEM_STACK_OK);
    CHECK(fields.id == 0 && fields.count == 0 && fields.damage == 0 && fields.animation == 0);
    result = 0;
cleanup:
    mc_item_stack_release(raw);
    mc_item_stack_release(live);
    return result;
}

static int provenance_overlay_and_retirement(void)
{
    int status = 1;
    mc_item_stack *stack = NULL;
    mc_nbt_document *result = NULL;
    uint8_t *payload = malloc(3);
    CHECK(payload != NULL);
    payload[0] = 0x12;
    payload[1] = 0x34;
    payload[2] = 0x56;
    mc_nbt_tag old_slot = {.type = MC_NBT_BYTE, .name = {slot_name, 4}, .value.byte_value = 3};
    mc_nbt_tag opaque = {
        .type = MC_NBT_BYTE_ARRAY, .name = {payload_name, 7}, .value.byte_array = {payload, 3}};
    mc_nbt_tag nan = {
        .type = MC_NBT_FLOAT, .name = {float_name, 1}, .value.float_bits = UINT32_C(0x7f800001)};
    mc_nbt_tag id = {.type = MC_NBT_SHORT, .name = {id_name, 2}, .value.short_value = 7};
    mc_nbt_tag count = {.type = MC_NBT_BYTE, .name = {count_name, 5}, .value.byte_value = 9};
    mc_nbt_tag damage = {.type = MC_NBT_SHORT, .name = {damage_name, 6}, .value.short_value = 11};
    const mc_nbt_tag *children[] = {&old_slot, &opaque, &nan, &id, &count, &damage};
    mc_nbt_tag input = {
        .type = MC_NBT_COMPOUND, .name = {root_name, 1}, .value.compound = {children, 6}};
    mc_item_stack_error error;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct(stack, 65535, 255, 65536) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_preserve_source(stack, &input, NULL, &error) == MC_ITEM_STACK_OK);
    memset(payload, 0xa5, 3);
    free(payload);
    payload = NULL;
    opaque.value.byte_array.data = NULL;
    opaque.value.byte_array.length = 0;
    /* The source must survive caller storage retirement and later self-copy. */
    const mc_nbt_tag *source = mc_item_stack_source(stack);
    const mc_nbt_tag *saved_payload = get(source, payload_name, 7);
    CHECK(saved_payload != NULL && saved_payload->type == MC_NBT_BYTE_ARRAY);
    CHECK(saved_payload->value.byte_array.length == 3);
    CHECK(saved_payload->value.byte_array.data[0] == 0x12);
    CHECK(saved_payload->value.byte_array.data[1] == 0x34);
    CHECK(saved_payload->value.byte_array.data[2] == 0x56);
    CHECK(get(source, float_name, 1)->value.float_bits == UINT32_C(0x7f800001));

    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK);
    CHECK(fields.id == 65535 && fields.count == 255 && fields.damage == 65536);
    size_t live_bytes = 0;
    CHECK(mc_item_stack_native_bytes(stack, &live_bytes) == MC_ITEM_STACK_OK);
    CHECK(live_bytes > mc_item_stack_object_bytes());
    mc_nbt_limits limited = mc_nbt_default_limits();
    limited.max_allocation_bytes = live_bytes;
    CHECK(mc_item_stack_preserve_source(stack, source, &limited, &error) == MC_ITEM_STACK_LIMIT);
    CHECK(mc_item_stack_source(stack) == source);
    CHECK(mc_item_stack_preserve_source(stack, source, NULL, &error) == MC_ITEM_STACK_OK);
    source = mc_item_stack_source(stack);
    CHECK(get(source, payload_name, 7)->value.byte_array.data[2] == 0x56);

    mc_nbt_tag slot = {.type = MC_NBT_BYTE, .name = {slot_name, 4}, .value.byte_value = 17};
    size_t pointers = SIZE_MAX;
    CHECK(mc_item_stack_record_pointer_count(stack, &slot, 1, &pointers, &error) ==
          MC_ITEM_STACK_OK);
    CHECK(pointers == 6);
    const mc_nbt_tag *scratch[6];
    mc_item_stack_record_view view;
    memset(&view, 0, sizeof(view));
    mc_item_stack_record_view previous;
    memcpy(&previous, &view, sizeof(view));
    CHECK(mc_item_stack_record_overlay(stack, &slot, 1, scratch, 5, &view, &error) ==
          MC_ITEM_STACK_INVALID_ARGUMENT);
    CHECK(memcmp(&previous, &view, sizeof(view)) == 0);
    CHECK(mc_item_stack_record_overlay(stack, &slot, 1, scratch, 6, &view, &error) ==
          MC_ITEM_STACK_OK);
    CHECK(get(&view.root, slot_name, 4) == &slot);
    CHECK(get(&view.root, slot_name, 4)->value.byte_value == 17);
    CHECK(get(source, slot_name, 4)->value.byte_value == 3);
    CHECK(get(&view.root, id_name, 2) == &view.item_fields[0]);
    CHECK(get(&view.root, id_name, 2)->value.short_value == -1);
    CHECK(get(&view.root, count_name, 5)->value.byte_value == -1);
    CHECK(get(&view.root, damage_name, 6)->value.short_value == 0);
    CHECK(view.root.name.length == 1 && view.root.name.units[0] == 0x2603);
    mc_nbt_tag duplicate_extras[] = {slot, slot};
    pointers = 77;
    CHECK(mc_item_stack_record_pointer_count(stack, duplicate_extras, 2, &pointers, &error) ==
          MC_ITEM_STACK_INVALID_ARGUMENT);
    CHECK(pointers == 77);
    CHECK(mc_item_stack_record_pointer_count(stack, &id, 1, &pointers, &error) ==
          MC_ITEM_STACK_INVALID_ARGUMENT);
    CHECK(pointers == 77);

    CHECK(mc_item_stack_build(stack, &limited, &result, &error) == MC_ITEM_STACK_LIMIT);
    CHECK(result == NULL && mc_item_stack_source(stack) == source);
    limited = mc_nbt_default_limits();
    limited.max_input_bytes = 1;
    CHECK(mc_item_stack_build(stack, &limited, &result, &error) == MC_ITEM_STACK_LIMIT);
    CHECK(result == NULL && mc_item_stack_source(stack) == source);
    CHECK(mc_item_stack_build(stack, NULL, &result, &error) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_source(stack) == source);
    CHECK(get(source, float_name, 1)->value.float_bits == UINT32_C(0x7f800001));
    mc_item_stack_release(stack);
    stack = NULL;
    const mc_nbt_tag *built = mc_nbt_document_root(result);
    CHECK(built != NULL && built->name.length == 1 && built->name.units[0] == 0x2603);
    CHECK(get(built, id_name, 2)->type == MC_NBT_SHORT);
    CHECK(get(built, id_name, 2)->value.short_value == -1);
    CHECK(get(built, count_name, 5)->type == MC_NBT_BYTE);
    CHECK(get(built, count_name, 5)->value.byte_value == -1);
    CHECK(get(built, damage_name, 6)->value.short_value == 0);
    CHECK(get(built, slot_name, 4)->value.byte_value == 3);
    CHECK(get(built, payload_name, 7)->value.byte_array.data[0] == 0x12);
    CHECK(get(built, float_name, 1)->value.float_bits == UINT32_C(0x7fc00000));
    status = 0;
cleanup:
    free(payload);
    mc_item_stack_release(stack);
    mc_nbt_document_free(result);
    return status;
}

static int split_clone_and_alias(void)
{
    int result = 1;
    mc_nbt_tag input = {.type = MC_NBT_COMPOUND};
    mc_item_stack *source = NULL, *split = NULL, *clone = NULL, *alias = NULL, *nullable = NULL;
    mc_item_stack_error error;
    CHECK(mc_item_stack_allocate(&source) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct(source, 23, INT32_MIN, -17) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_preserve_source(source, &input, NULL, &error) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_retain(source) == MC_ITEM_STACK_OK);
    alias = source;
    CHECK(mc_item_stack_split(source, 1, &split) == MC_ITEM_STACK_OK);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(alias, &fields) == MC_ITEM_STACK_OK);
    CHECK(fields.count == INT32_MAX && fields.id == 23 && fields.damage == -17);
    CHECK(mc_item_stack_source(source) != NULL);
    CHECK(mc_item_stack_fields_get(split, &fields) == MC_ITEM_STACK_OK);
    CHECK(fields.count == 1 && fields.id == 23 && fields.damage == -17 && fields.animation == 0);
    CHECK(mc_item_stack_source(split) == NULL);
    CHECK(mc_item_stack_clone(source, &clone) == MC_ITEM_STACK_OK);
    CHECK(clone != source && mc_item_stack_source(clone) == NULL);
    CHECK(mc_item_stack_count_set(alias, 4) == MC_ITEM_STACK_OK);
    mc_item_stack_release(source);
    source = NULL;
    CHECK(mc_item_stack_fields_get(alias, &fields) == MC_ITEM_STACK_OK && fields.count == 4);
    CHECK(mc_item_stack_fields_get(clone, &fields) == MC_ITEM_STACK_OK &&
          fields.count == INT32_MAX);
    CHECK(mc_item_stack_clone_nullable(NULL, &nullable) == MC_ITEM_STACK_OK && nullable == NULL);
    mc_item_stack_release(NULL);
    result = 0;
cleanup:
    mc_item_stack_release(source);
    mc_item_stack_release(alias);
    mc_item_stack_release(clone);
    mc_item_stack_release(split);
    mc_item_stack_release(nullable);
    return result;
}

static int ignored_list_names(void)
{
    int result = 1;
    mc_item_stack *stack = NULL;
    const uint16_t list_name[] = {'L'};
    mc_nbt_tag child = {.type = MC_NBT_COMPOUND, .name = {NULL, SIZE_MAX}};
    const mc_nbt_tag *entries[] = {&child};
    mc_nbt_tag list = {
        .type = MC_NBT_LIST, .name = {list_name, 1}, .value.list = {MC_NBT_COMPOUND, entries, 1}};
    const mc_nbt_tag *children[] = {&list};
    mc_nbt_tag input = {.type = MC_NBT_COMPOUND, .value.compound = {children, 1}};
    mc_item_stack_error error;
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_input_bytes = 0; /* This operation parses no encoded bytes. */
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct(stack, 1, 2, 3) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_preserve_source(stack, &input, &limits, &error) == MC_ITEM_STACK_OK);
    const mc_nbt_tag *saved = get(mc_item_stack_source(stack), list_name, 1);
    CHECK(saved != NULL && saved->type == MC_NBT_LIST && saved->value.list.length == 1);
    CHECK(saved->value.list.items[0]->name.units == NULL);
    CHECK(saved->value.list.items[0]->name.length == 0);
    result = 0;
cleanup:
    mc_item_stack_release(stack);
    return result;
}

int main(void)
{
    if (construction_prefixes() || provenance_overlay_and_retirement() || split_clone_and_alias() ||
        ignored_list_names())
        return 1;
    printf("%u ItemStack checks passed\n", checks);
    return 0;
}
