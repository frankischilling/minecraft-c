#include "mc/storage/nbt/overlay.h"
#include "mc/storage/nbt/tree.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#define SPAN(array)                                                                                \
    {                                                                                              \
        (array), sizeof(array) / sizeof((array)[0])                                                \
    }

static int error_is(mc_nbt_error error, mc_nbt_status status)
{
    return error.status == status && error.offset == 0;
}

static int name_is(mc_nbt_string actual, const uint16_t *units, size_t length)
{
    return actual.length == length &&
           (length == 0 || memcmp(actual.units, units, length * sizeof(*units)) == 0);
}

static int test_fixed_order_and_owners(void)
{
    static const uint16_t source_name[] = {'R', 0, 0xd800};
    static const uint16_t fields_name[] = {'G'};
    static const uint16_t raw_name[] = {'r', 'a', 'w'};
    static const uint16_t count_name[] = {'C', 'o', 'u', 'n', 't'};
    static const uint16_t count_more_name[] = {'C', 'o', 'u', 'n', 't', 'M', 'o', 'r', 'e'};
    static const uint16_t opaque_name[] = {'o', 'p', 'a', 'q', 'u', 'e'};
    static const uint16_t inner_name[] = {'i'};
    static const uint16_t old_wide_name[] = {0x4e2d, 0, 0x03a9};
    static const uint16_t new_wide_name[] = {0x4e2d, 0, 0x03a9};
    static const uint16_t near_wide_name[] = {0x4e2d, 0, 0x03bb};
    static const uint16_t blob_name[] = {'b', 'l', 'o', 'b'};
    static const uint16_t item_name[] = {'I', 't', 'e', 'm'};
    static const uint8_t blob[] = {0, 255, 128, 1};
    const mc_nbt_tag inner = {.type = MC_NBT_DOUBLE,
                              .name = SPAN(inner_name),
                              .value.double_bits = UINT64_C(0x7ff0000000000042)};
    const mc_nbt_tag *inner_items[] = {&inner};
    const mc_nbt_tag source_tags[] = {
        {.type = MC_NBT_FLOAT, .name = SPAN(raw_name), .value.float_bits = UINT32_C(0x7f812345)},
        {.type = MC_NBT_BYTE, .name = SPAN(count_name), .value.byte_value = 2},
        {.type = MC_NBT_COMPOUND,
         .name = SPAN(opaque_name),
         .value.compound = {.items = inner_items, .length = 1}},
        {.type = MC_NBT_SHORT, .name = SPAN(old_wide_name), .value.short_value = 7},
        {.type = MC_NBT_BYTE_ARRAY,
         .name = SPAN(blob_name),
         .value.byte_array = {.data = blob, .length = sizeof(blob)}},
    };
    const mc_nbt_tag *source_items[] = {&source_tags[0], &source_tags[1], &source_tags[2],
                                        &source_tags[3], &source_tags[4]};
    const mc_nbt_tag source = {.type = MC_NBT_COMPOUND,
                               .name = SPAN(source_name),
                               .value.compound = {.items = source_items, .length = 5}};
    /* Generated order differs from the two replacement positions. A prefix
     * and a name differing after embedded NUL must remain distinct keys. */
    const mc_nbt_tag fields_tags[] = {
        {.type = MC_NBT_SHORT, .name = SPAN(new_wide_name), .value.short_value = -8},
        {.type = MC_NBT_INT, .name = SPAN(count_more_name), .value.int_value = 99},
        {.type = MC_NBT_BYTE, .name = SPAN(count_name), .value.byte_value = -3},
        {.type = MC_NBT_FLOAT, .name = {NULL, 0}, .value.float_bits = UINT32_C(0x80000000)},
        {.type = MC_NBT_INT, .name = SPAN(near_wide_name), .value.int_value = 66},
    };
    const mc_nbt_tag *fields_items[] = {&fields_tags[0], &fields_tags[1], &fields_tags[2],
                                        &fields_tags[3], &fields_tags[4]};
    const mc_nbt_tag fields = {.type = MC_NBT_COMPOUND,
                               .name = SPAN(fields_name),
                               .value.compound = {.items = fields_items, .length = 5}};
    mc_nbt_tree *source_owner = NULL;
    mc_nbt_tree *fields_owner = NULL;
    mc_nbt_tree *independent = NULL;
    mc_nbt_error error = {MC_NBT_TRUNCATED, 123};
    CHECK(mc_nbt_tree_copy_compound(&source, NULL, &source_owner, &error) == MC_NBT_OK);
    CHECK(mc_nbt_tree_copy_compound(&fields, NULL, &fields_owner, &error) == MC_NBT_OK);
    const mc_nbt_tag *s = mc_nbt_tree_root(source_owner);
    const mc_nbt_tag *f = mc_nbt_tree_root(fields_owner);
    CHECK(s != &source && f != &fields);
    CHECK(s->value.compound.length == 5 && f->value.compound.length == 5);
    const mc_nbt_tag *scratch[10];
    for (size_t i = 0; i < 10; ++i)
        scratch[i] = &inner;
    size_t count = 73;
    CHECK(mc_nbt_compound_overlay_count(s, f, &count, &error) == MC_NBT_OK);
    CHECK(count == 8 && error_is(error, MC_NBT_OK));
    mc_nbt_tag merged = {.type = MC_NBT_BYTE, .value.byte_value = 43};
    CHECK(mc_nbt_compound_overlay(s, f, scratch, 10, &merged, &error) == MC_NBT_OK);
    CHECK(error_is(error, MC_NBT_OK));
    CHECK(merged.type == MC_NBT_COMPOUND && merged.value.compound.items == scratch);
    CHECK(merged.value.compound.length == 8);
    CHECK(merged.name.units == s->name.units && merged.name.length == s->name.length);
    /* Literal expected sequence, independent of the implementation's loops. */
    CHECK(scratch[0] == s->value.compound.items[0]);
    CHECK(scratch[1] == f->value.compound.items[2]);
    CHECK(scratch[2] == s->value.compound.items[2]);
    CHECK(scratch[3] == f->value.compound.items[0]);
    CHECK(scratch[4] == s->value.compound.items[4]);
    CHECK(scratch[5] == f->value.compound.items[1]);
    CHECK(scratch[6] == f->value.compound.items[3]);
    CHECK(scratch[7] == f->value.compound.items[4]);
    CHECK(scratch[8] == &inner && scratch[9] == &inner);
    CHECK(scratch[0]->value.float_bits == UINT32_C(0x7f812345));
    CHECK(scratch[1]->value.byte_value == -3);
    CHECK(scratch[2]->value.compound.items == s->value.compound.items[2]->value.compound.items);
    CHECK(scratch[2]->value.compound.items[0]->value.double_bits == UINT64_C(0x7ff0000000000042));
    CHECK(scratch[3]->value.short_value == -8);
    CHECK(scratch[4]->value.byte_array.data == s->value.compound.items[4]->value.byte_array.data);
    CHECK(memcmp(scratch[4]->value.byte_array.data, blob, sizeof(blob)) == 0);
    CHECK(scratch[5]->value.int_value == 99);
    CHECK(scratch[6]->value.float_bits == UINT32_C(0x80000000));
    CHECK(scratch[7]->value.int_value == 66);
    CHECK(s->value.compound.items[1]->value.byte_value == 2);
    CHECK(s->value.compound.items[3]->value.short_value == 7);
    CHECK(name_is(s->name, source_name, 3) && name_is(f->name, fields_name, 1));
    CHECK(s->value.compound.length == 5 && f->value.compound.length == 5);

    /* Native storage composes a borrowed Item field, then copies while both
     * owners and scratch remain live. Renaming does not mutate either owner. */
    merged.name = (mc_nbt_string)SPAN(item_name);
    CHECK(name_is(s->name, source_name, 3) && name_is(f->name, fields_name, 1));
    CHECK(mc_nbt_tree_copy_compound(&merged, NULL, &independent, &error) == MC_NBT_OK);
    const mc_nbt_tag *copy = mc_nbt_tree_root(independent);
    CHECK(copy != &merged && copy->value.compound.items != scratch);
    CHECK(name_is(copy->name, item_name, 4) && copy->name.units != item_name);
    CHECK(copy->value.compound.items[0] != scratch[0]);
    CHECK(copy->value.compound.items[2]->value.compound.items != scratch[2]->value.compound.items);
    CHECK(copy->value.compound.items[4]->value.byte_array.data !=
          scratch[4]->value.byte_array.data);
    mc_nbt_tree_free(source_owner);
    mc_nbt_tree_free(fields_owner);
    source_owner = NULL;
    fields_owner = NULL;
    /* All further observations use only the independently owned copy. */
    CHECK(copy->value.compound.length == 8 && name_is(copy->name, item_name, 4));
    CHECK(name_is(copy->value.compound.items[0]->name, raw_name, 3));
    CHECK(copy->value.compound.items[0]->value.float_bits == UINT32_C(0x7f812345));
    CHECK(name_is(copy->value.compound.items[1]->name, count_name, 5));
    CHECK(copy->value.compound.items[1]->value.byte_value == -3);
    CHECK(name_is(copy->value.compound.items[2]->name, opaque_name, 6));
    CHECK(copy->value.compound.items[2]->value.compound.items[0]->value.double_bits ==
          UINT64_C(0x7ff0000000000042));
    CHECK(name_is(copy->value.compound.items[3]->name, new_wide_name, 3));
    CHECK(copy->value.compound.items[3]->value.short_value == -8);
    CHECK(name_is(copy->value.compound.items[4]->name, blob_name, 4));
    CHECK(copy->value.compound.items[4]->value.byte_array.length == sizeof(blob));
    CHECK(memcmp(copy->value.compound.items[4]->value.byte_array.data, blob, sizeof(blob)) == 0);
    CHECK(name_is(copy->value.compound.items[5]->name, count_more_name, 9));
    CHECK(copy->value.compound.items[5]->value.int_value == 99);
    CHECK(copy->value.compound.items[6]->name.length == 0);
    CHECK(copy->value.compound.items[6]->value.float_bits == UINT32_C(0x80000000));
    CHECK(name_is(copy->value.compound.items[7]->name, near_wide_name, 3));
    CHECK(copy->value.compound.items[7]->value.int_value == 66);
    mc_nbt_tree_free(independent);
    return 0;
}

static int test_empty_null_and_shared_inputs(void)
{
    static const uint16_t source_name[] = {'S'};
    static const uint16_t fields_name[] = {'F'};
    static const uint16_t a_name[] = {'a'};
    static const uint16_t unused_zero_name[] = {'z'};
    const mc_nbt_tag a = {.type = MC_NBT_INT, .name = SPAN(a_name), .value.int_value = 17};
    const mc_nbt_tag empty_old = {.type = MC_NBT_BYTE, .name = {NULL, 0}, .value.byte_value = 1};
    const mc_nbt_tag empty_new = {
        .type = MC_NBT_SHORT, .name = {unused_zero_name, 0}, .value.short_value = 9};
    const mc_nbt_tag *old_items[] = {&a, &empty_old};
    const mc_nbt_tag *new_items[] = {&empty_new};
    const mc_nbt_tag source = {.type = MC_NBT_COMPOUND,
                               .name = SPAN(source_name),
                               .value.compound = {.items = old_items, .length = 2}};
    const mc_nbt_tag fields = {.type = MC_NBT_COMPOUND,
                               .name = SPAN(fields_name),
                               .value.compound = {.items = new_items, .length = 1}};
    const mc_nbt_tag empty_source = {.type = MC_NBT_COMPOUND, .name = SPAN(source_name)};
    const mc_nbt_tag empty_fields = {.type = MC_NBT_COMPOUND, .name = SPAN(fields_name)};
    mc_nbt_tag out;
    mc_nbt_error error = {MC_NBT_LIMIT, SIZE_MAX};
    size_t count = 91;
    CHECK(mc_nbt_compound_overlay_count(NULL, &empty_fields, &count, &error) == MC_NBT_OK);
    CHECK(count == 0 && error_is(error, MC_NBT_OK));
    CHECK(mc_nbt_compound_overlay(NULL, &empty_fields, NULL, 0, &out, &error) == MC_NBT_OK);
    CHECK(out.type == MC_NBT_COMPOUND && out.value.compound.length == 0);
    CHECK(out.value.compound.items == NULL && out.name.units == fields_name);
    CHECK(error_is(error, MC_NBT_OK));
    CHECK(mc_nbt_compound_overlay(&empty_source, &empty_fields, NULL, 0, &out, NULL) == MC_NBT_OK);
    CHECK(out.value.compound.length == 0 && out.name.units == source_name);
    const mc_nbt_tag *scratch[3] = {&a, &a, &a};
    CHECK(mc_nbt_compound_overlay(&empty_source, &empty_fields, scratch, 3, &out, &error) ==
          MC_NBT_OK);
    CHECK(out.value.compound.items == scratch && out.value.compound.length == 0);
    CHECK(scratch[0] == &a && scratch[1] == &a && scratch[2] == &a);
    CHECK(mc_nbt_compound_overlay_count(NULL, &fields, &count, NULL) == MC_NBT_OK && count == 1);
    CHECK(mc_nbt_compound_overlay(NULL, &fields, scratch, 3, &out, NULL) == MC_NBT_OK);
    CHECK(out.name.units == fields_name && out.value.compound.length == 1);
    CHECK(scratch[0] == &empty_new && scratch[1] == &a && scratch[2] == &a);
    CHECK(mc_nbt_compound_overlay_count(&source, &empty_fields, &count, NULL) == MC_NBT_OK &&
          count == 2);
    CHECK(mc_nbt_compound_overlay(&source, &empty_fields, scratch, 2, &out, NULL) == MC_NBT_OK);
    CHECK(out.name.units == source_name && out.value.compound.length == 2);
    CHECK(scratch[0] == &a && scratch[1] == &empty_old && scratch[2] == &a);
    CHECK(mc_nbt_compound_overlay_count(&source, &fields, &count, NULL) == MC_NBT_OK && count == 2);
    CHECK(mc_nbt_compound_overlay(&source, &fields, scratch, 2, &out, &error) == MC_NBT_OK);
    CHECK(scratch[0] == &a && scratch[1] == &empty_new && scratch[2] == &a);
    CHECK(out.name.units == source_name && error_is(error, MC_NBT_OK));
    CHECK(empty_old.value.byte_value == 1 && empty_old.name.units == NULL);
    CHECK(empty_new.name.units == unused_zero_name && empty_new.name.length == 0);
    /* No distinct-owner assumption: both immutable arguments may be identical. */
    CHECK(mc_nbt_compound_overlay_count(&source, &source, &count, NULL) == MC_NBT_OK && count == 2);
    CHECK(mc_nbt_compound_overlay(&source, &source, scratch, 2, &out, &error) == MC_NBT_OK);
    CHECK(scratch[0] == &a && scratch[1] == &empty_old && scratch[2] == &a);
    CHECK(out.name.units == source_name && error_is(error, MC_NBT_OK));
    return 0;
}

static int test_disjoint_scratch_composition(void)
{
    static const uint16_t item_name[] = {'I', 't', 'e', 'm'};
    static const uint16_t old_item_root_name[] = {'x'};
    static const uint16_t custom_name[] = {'u'};
    static const uint16_t count_name[] = {'C', 'o', 'u', 'n', 't'};
    static const uint16_t id_name[] = {'i', 'd'};
    static const uint16_t extra_name[] = {'e'};
    const mc_nbt_tag custom = {
        .type = MC_NBT_LONG, .name = SPAN(custom_name), .value.long_value = -9};
    const mc_nbt_tag old_count = {
        .type = MC_NBT_BYTE, .name = SPAN(count_name), .value.byte_value = 1};
    const mc_nbt_tag new_count = {
        .type = MC_NBT_BYTE, .name = SPAN(count_name), .value.byte_value = 5};
    const mc_nbt_tag id = {.type = MC_NBT_SHORT, .name = SPAN(id_name), .value.short_value = 4};
    const mc_nbt_tag extra = {.type = MC_NBT_INT, .name = SPAN(extra_name), .value.int_value = 8};
    const mc_nbt_tag *old_item_items[] = {&custom, &old_count};
    const mc_nbt_tag *new_item_items[] = {&new_count, &id};
    const mc_nbt_tag old_item = {.type = MC_NBT_COMPOUND,
                                 .name = SPAN(old_item_root_name),
                                 .value.compound = {.items = old_item_items, .length = 2}};
    const mc_nbt_tag new_item = {.type = MC_NBT_COMPOUND,
                                 .name = SPAN(item_name),
                                 .value.compound = {.items = new_item_items, .length = 2}};
    const mc_nbt_tag *old_outer_items[] = {&extra, &new_item};
    const mc_nbt_tag old_outer = {.type = MC_NBT_COMPOUND,
                                  .value.compound = {.items = old_outer_items, .length = 2}};
    const mc_nbt_tag *scratch[6] = {&extra, &extra, &extra, &extra, &extra, &extra};
    mc_nbt_tag item_out;
    mc_nbt_tag outer_out;
    CHECK(mc_nbt_compound_overlay(&old_item, &new_item, scratch, 3, &item_out, NULL) == MC_NBT_OK);
    CHECK(scratch[0] == &custom && scratch[1] == &new_count && scratch[2] == &id);
    CHECK(item_out.name.units == old_item_root_name);
    item_out.name = new_item.name;
    const mc_nbt_tag *new_outer_items[] = {&item_out};
    const mc_nbt_tag new_outer = {.type = MC_NBT_COMPOUND,
                                  .value.compound = {.items = new_outer_items, .length = 1}};
    CHECK(mc_nbt_compound_overlay(&old_outer, &new_outer, scratch + 3, 2, &outer_out, NULL) ==
          MC_NBT_OK);
    CHECK(scratch[0] == &custom && scratch[1] == &new_count && scratch[2] == &id);
    CHECK(scratch[3] == &extra && scratch[4] == &item_out && scratch[5] == &extra);
    CHECK(outer_out.value.compound.items == scratch + 3 && outer_out.value.compound.length == 2);
    CHECK(item_out.value.compound.items == scratch && item_out.value.compound.length == 3);
    mc_nbt_tree *copy = NULL;
    CHECK(mc_nbt_tree_copy_compound(&outer_out, NULL, &copy, NULL) == MC_NBT_OK);
    const mc_nbt_tag *root = mc_nbt_tree_root(copy);
    CHECK(root->value.compound.length == 2);
    CHECK(root->value.compound.items[0]->value.int_value == 8);
    const mc_nbt_tag *copied_item = root->value.compound.items[1];
    CHECK(name_is(copied_item->name, item_name, 4));
    CHECK(copied_item->value.compound.length == 3);
    CHECK(copied_item->value.compound.items[0]->value.long_value == -9);
    CHECK(copied_item->value.compound.items[1]->value.byte_value == 5);
    CHECK(copied_item->value.compound.items[2]->value.short_value == 4);
    mc_nbt_tree_free(copy);
    return 0;
}

static int expect_shape_failure(const mc_nbt_tag *source, const mc_nbt_tag *fields,
                                mc_nbt_status expected)
{
    const mc_nbt_tag sentinel = {.type = MC_NBT_BYTE, .value.byte_value = 21};
    const mc_nbt_tag *scratch[4] = {&sentinel, &sentinel, &sentinel, &sentinel};
    mc_nbt_tag out;
    unsigned char before[sizeof(out)];
    memset(&out, 0xa5, sizeof(out));
    memcpy(before, &out, sizeof(out));
    size_t count = 73;
    mc_nbt_error error = {MC_NBT_TRUNCATED, SIZE_MAX};
    CHECK(mc_nbt_compound_overlay_count(source, fields, &count, &error) == expected);
    CHECK(count == 73 && error_is(error, expected));
    error = (mc_nbt_error){MC_NBT_TRUNCATED, SIZE_MAX};
    CHECK(mc_nbt_compound_overlay(source, fields, scratch, 4, &out, &error) == expected);
    CHECK(error_is(error, expected) && memcmp(before, &out, sizeof(out)) == 0);
    CHECK(scratch[0] == &sentinel && scratch[1] == &sentinel);
    CHECK(scratch[2] == &sentinel && scratch[3] == &sentinel);
    CHECK(mc_nbt_compound_overlay_count(source, fields, &count, NULL) == expected && count == 73);
    return 0;
}

static int test_shallow_failures(void)
{
    static const uint16_t name[] = {'v'};
    const mc_nbt_tag good = {.type = MC_NBT_INT, .name = SPAN(name), .value.int_value = 1};
    const mc_nbt_tag *good_items[] = {&good};
    const mc_nbt_tag fields = {.type = MC_NBT_COMPOUND,
                               .value.compound = {.items = good_items, .length = 1}};
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    CHECK(expect_shape_failure(NULL, NULL, MC_NBT_INVALID_ARGUMENT) == 0);
    CHECK(expect_shape_failure(NULL, &good, MC_NBT_ROOT_TYPE) == 0);
    CHECK(expect_shape_failure(&good, &fields, MC_NBT_ROOT_TYPE) == 0);
    mc_nbt_tag bad = {.type = MC_NBT_COMPOUND, .value.compound = {.items = NULL, .length = 1}};
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_TREE) == 0);
    CHECK(expect_shape_failure(&bad, &fields, MC_NBT_INVALID_TREE) == 0);
    const mc_nbt_tag *bad_items[] = {NULL};
    bad.value.compound.items = bad_items;
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_TREE) == 0);
    mc_nbt_tag child = {.type = MC_NBT_END, .name = SPAN(name)};
    bad_items[0] = &child;
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_TREE) == 0);
    child.type = (mc_nbt_type)11;
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_TYPE) == 0);
    child.type = (mc_nbt_type)-1;
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_TYPE) == 0);
    child.type = MC_NBT_INT;
    child.name = (mc_nbt_string){NULL, 1};
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_TREE) == 0);
    child.name = (mc_nbt_string){name, (size_t)UINT16_MAX + 1};
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_LENGTH) == 0);
    bad = empty;
    bad.name = (mc_nbt_string){NULL, 1};
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_TREE) == 0);
    bad.name = (mc_nbt_string){name, (size_t)UINT16_MAX + 1};
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_INVALID_LENGTH) == 0);
    /* Rejected metadata controls: the byte extent is rejected before a span
     * access. Neither tiny backing array claims to be a huge valid owner. */
    bad = (mc_nbt_tag){.type = MC_NBT_COMPOUND,
                       .value.compound = {.items = good_items, .length = SIZE_MAX}};
    CHECK(expect_shape_failure(NULL, &bad, MC_NBT_LIMIT) == 0);
    CHECK(expect_shape_failure(&bad, &fields, MC_NBT_LIMIT) == 0);

    size_t count = 73;
    mc_nbt_error error = {MC_NBT_TRUNCATED, SIZE_MAX};
    CHECK(mc_nbt_compound_overlay_count(NULL, &fields, NULL, &error) == MC_NBT_INVALID_ARGUMENT);
    CHECK(error_is(error, MC_NBT_INVALID_ARGUMENT));
    CHECK(mc_nbt_compound_overlay_count(NULL, &fields, &count, NULL) == MC_NBT_OK && count == 1);
    const mc_nbt_tag *scratch[3] = {&good, &good, &good};
    mc_nbt_tag out;
    unsigned char before[sizeof(out)];
    memset(&out, 0x5a, sizeof(out));
    memcpy(before, &out, sizeof(out));
    CHECK(mc_nbt_compound_overlay(NULL, &fields, scratch, 3, NULL, &error) ==
          MC_NBT_INVALID_ARGUMENT);
    CHECK(error_is(error, MC_NBT_INVALID_ARGUMENT));
    CHECK(scratch[0] == &good && scratch[1] == &good && scratch[2] == &good);
    CHECK(mc_nbt_compound_overlay(NULL, &fields, scratch, 0, &out, &error) ==
          MC_NBT_BUFFER_TOO_SMALL);
    CHECK(error_is(error, MC_NBT_BUFFER_TOO_SMALL) && memcmp(before, &out, sizeof(out)) == 0);
    CHECK(scratch[0] == &good && scratch[1] == &good && scratch[2] == &good);
    CHECK(mc_nbt_compound_overlay(NULL, &fields, NULL, 0, &out, &error) == MC_NBT_BUFFER_TOO_SMALL);
    CHECK(error_is(error, MC_NBT_BUFFER_TOO_SMALL) && memcmp(before, &out, sizeof(out)) == 0);
    CHECK(mc_nbt_compound_overlay(NULL, &fields, NULL, 1, &out, &error) == MC_NBT_INVALID_ARGUMENT);
    CHECK(error_is(error, MC_NBT_INVALID_ARGUMENT) && memcmp(before, &out, sizeof(out)) == 0);
    CHECK(mc_nbt_compound_overlay(NULL, &empty, NULL, 1, &out, &error) == MC_NBT_INVALID_ARGUMENT);
    CHECK(error_is(error, MC_NBT_INVALID_ARGUMENT) && memcmp(before, &out, sizeof(out)) == 0);
    CHECK(mc_nbt_compound_overlay(NULL, &fields, scratch, SIZE_MAX, &out, &error) == MC_NBT_LIMIT);
    CHECK(error_is(error, MC_NBT_LIMIT) && memcmp(before, &out, sizeof(out)) == 0);
    CHECK(scratch[0] == &good && scratch[1] == &good && scratch[2] == &good);
    CHECK(mc_nbt_compound_overlay(NULL, &fields, scratch, 1, &out, &error) == MC_NBT_OK);
    CHECK(error_is(error, MC_NBT_OK) && out.value.compound.length == 1);
    CHECK(scratch[0] == &good && scratch[1] == &good && scratch[2] == &good);
    return 0;
}

static int test_deferred_descendant_validation(void)
{
    static const uint16_t unknown_name[] = {'u'};
    const mc_nbt_tag malformed_descendant = {
        .type = MC_NBT_LIST,
        .name = SPAN(unknown_name),
        .value.list = {.element_type = MC_NBT_INT, .items = NULL, .length = 1},
    };
    const mc_nbt_tag *items[] = {&malformed_descendant};
    const mc_nbt_tag source = {.type = MC_NBT_COMPOUND,
                               .value.compound = {.items = items, .length = 1}};
    const mc_nbt_tag fields = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *scratch[2] = {&fields, &fields};
    mc_nbt_tag out;
    mc_nbt_error error = {MC_NBT_TRUNCATED, SIZE_MAX};
    size_t count = 79;
    CHECK(mc_nbt_compound_overlay_count(&source, &fields, &count, &error) == MC_NBT_OK);
    CHECK(count == 1 && error_is(error, MC_NBT_OK));
    CHECK(mc_nbt_compound_overlay(&source, &fields, scratch, 2, &out, &error) == MC_NBT_OK);
    CHECK(scratch[0] == &malformed_descendant && scratch[1] == &fields);
    CHECK(out.value.compound.length == 1 && error_is(error, MC_NBT_OK));
    /* The outer owner validates payloads. This explicit malformed-descendant
     * input is never described as a decoded tree or successful stored record. */
    mc_nbt_tree *existing = NULL;
    CHECK(mc_nbt_tree_copy_compound(&fields, NULL, &existing, NULL) == MC_NBT_OK);
    mc_nbt_tree *copy = existing;
    CHECK(mc_nbt_tree_copy_compound(&out, NULL, &copy, &error) == MC_NBT_INVALID_TREE);
    CHECK(error.status == MC_NBT_INVALID_TREE && copy == existing);
    CHECK(mc_nbt_tree_root(existing)->value.compound.length == 0);
    CHECK(scratch[0] == &malformed_descendant && scratch[1] == &fields);
    mc_nbt_tree_free(existing);
    return 0;
}

int main(void)
{
    if (test_fixed_order_and_owners() || test_empty_null_and_shared_inputs() ||
        test_disjoint_scratch_composition() || test_shallow_failures() ||
        test_deferred_descendant_validation())
        return 1;
    printf("NBT Compound overlay: %u checks passed\n", checks);
    return 0;
}
