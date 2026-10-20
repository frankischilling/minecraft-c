/* Reuse real constructor/profile inspection helpers; the old main is never run.
 * Expected NBT values and PCs are independently taken from paired sn/lq.d/e,
 * hl/ez.b/a and jc/ga clinit, recorded in live-entities-nbt-source-plan-a.md. */
#define main mc_entity_constructor_fixture_main
#include "../test_entity.c"
#undef main

#include "mc/entity/nbt/nbt.h"
#include "mc/entity/registry/registry.h"

#include <stdlib.h>

/* These are constructed native leaf contracts, not original/control replay,
 * natural entities, factory dispatch or a World/Store round trip. All source
 * scalar inputs below are explicitly prepared on actual constructed owners.
 * No ID, phase, hold, class ledger, Random or membership input is assigned. */
typedef struct en_record {
    mc_nbt_tag root, fields[10], vectors[8], stack[3];
    const mc_nbt_tag *root_items[10], *pos[3], *motion[3], *rotation[2], *stack_items[3];
    uint16_t names[13][16];
} en_record;

typedef struct en_fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *classes;
    mc_entity *entity;
} en_fixture;

typedef struct en_guard {
    mc_entity_snapshot entity;
    mc_entity_bounds bounds;
    mc_entity_class_snapshot classes;
    mc_java_random ordinary, math;
    mc_entity_watcher_snapshot watcher;
    uint64_t unique;
} en_guard;

static mc_nbt_string en_ascii(const char *text, uint16_t *units)
{
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, length};
}

static bool en_text(mc_nbt_string value, const char *expected)
{
    if (value.length != strlen(expected) || (value.length != 0 && value.units == NULL))
        return false;
    for (size_t i = 0; i < value.length; ++i)
        if (value.units[i] != (unsigned char)expected[i])
            return false;
    return true;
}

static const mc_nbt_tag *en_field(const mc_nbt_tag *root, const char *name)
{
    uint16_t units[16];
    return mc_nbt_compound_get(root, units, en_ascii(name, units).length);
}

static void en_record_init(en_record *r)
{
    const char *const names[] = {"Pos", "Motion",   "Rotation", "FallDistance", "Fire",
                                 "Air", "OnGround", "Health",   "Age",          "Item",
                                 "id",  "Count",    "Damage"};
    const mc_nbt_type types[] = {MC_NBT_LIST,  MC_NBT_LIST,    MC_NBT_LIST, MC_NBT_FLOAT,
                                 MC_NBT_SHORT, MC_NBT_SHORT,   MC_NBT_BYTE, MC_NBT_SHORT,
                                 MC_NBT_SHORT, MC_NBT_COMPOUND};
    const double doubles[] = {8.0, 16.0, 32.0, 20.0, 30.0, 40.0};
    memset(r, 0, sizeof(*r));
    r->root.type = MC_NBT_COMPOUND;
    r->root.value.compound.items = r->root_items;
    r->root.value.compound.length = 10;
    for (size_t i = 0; i < 10; ++i) {
        r->fields[i].name = en_ascii(names[i], r->names[i]);
        r->fields[i].type = types[i];
        r->root_items[i] = &r->fields[i];
    }
    for (size_t i = 0; i < 6; ++i) {
        r->vectors[i].type = MC_NBT_DOUBLE;
        r->vectors[i].value.double_bits = double_bits(doubles[i]);
    }
    r->vectors[6].type = r->vectors[7].type = MC_NBT_FLOAT;
    r->vectors[6].value.float_bits = float_bits(725.0f);
    r->vectors[7].value.float_bits = float_bits(-725.0f);
    for (size_t i = 0; i < 3; ++i) {
        r->pos[i] = &r->vectors[i];
        r->motion[i] = &r->vectors[i + 3];
        r->stack_items[i] = &r->stack[i];
        r->stack[i].name = en_ascii(names[i + 10], r->names[i + 10]);
        r->stack[i].type = i == 1 ? MC_NBT_BYTE : MC_NBT_SHORT;
    }
    r->rotation[0] = &r->vectors[6];
    r->rotation[1] = &r->vectors[7];
    r->fields[0].value.list.element_type = r->fields[1].value.list.element_type = MC_NBT_DOUBLE;
    r->fields[2].value.list.element_type = MC_NBT_FLOAT;
    r->fields[0].value.list.items = r->pos;
    r->fields[1].value.list.items = r->motion;
    r->fields[2].value.list.items = r->rotation;
    r->fields[0].value.list.length = r->fields[1].value.list.length = 3;
    r->fields[2].value.list.length = 2;
    r->fields[3].value.float_bits = float_bits(3.5f);
    r->fields[4].value.short_value = -19;
    r->fields[5].value.short_value = 298;
    r->fields[6].value.byte_value = -2;
    r->fields[7].value.short_value = 511;
    r->fields[8].value.short_value = -17;
    r->fields[9].value.compound.items = r->stack_items;
    r->fields[9].value.compound.length = 3;
    r->stack[0].value.short_value = -7;
    r->stack[1].value.byte_value = -3;
    r->stack[2].value.short_value = -11;
}

static bool en_open(en_fixture *f, mc_entity_source source)
{
    return mc_java_random_profile_create(profile_budget(), &f->profile) ==
               MC_JAVA_RANDOM_PROFILE_OK &&
           mc_entity_classes_create(source, f->profile, NULL, &f->classes) == MC_ENTITY_OK &&
           mc_item_entity_allocate(f->classes, &f->entity, NULL) == MC_ENTITY_OK &&
           mc_item_entity_construct_world(f->entity, NULL, NULL) == MC_ENTITY_OK;
}

static bool en_close(en_fixture *f)
{
    mc_entity_release(f->entity);
    f->entity = NULL;
    if (retire_classes(&f->classes) != 0)
        return false;
    mc_java_random_profile_free(f->profile);
    f->profile = NULL;
    return true;
}

static bool en_capture(en_fixture *f, en_guard *g)
{
    if (mc_entity_get_snapshot(f->entity, &g->entity) != MC_ENTITY_OK || g->entity.bounds == NULL ||
        g->entity.random == NULL ||
        mc_entity_classes_get_snapshot(f->classes, &g->classes) != MC_ENTITY_OK ||
        mc_entity_watcher_get_snapshot(g->entity.watcher, &g->watcher) != MC_ENTITY_OK)
        return false;
    g->bounds = *g->entity.bounds;
    g->ordinary = *g->entity.random;
    g->math = math_snapshot(f->profile);
    g->unique = uniquifier(f->profile);
    return true;
}

static bool en_unrelated(en_fixture *f, const en_guard *g)
{
    mc_entity_snapshot e;
    mc_entity_class_snapshot c;
    mc_entity_watcher_snapshot w;
    return mc_entity_get_snapshot(f->entity, &e) == MC_ENTITY_OK &&
           mc_entity_classes_get_snapshot(f->classes, &c) == MC_ENTITY_OK &&
           mc_entity_watcher_get_snapshot(e.watcher, &w) == MC_ENTITY_OK &&
           e.phase == g->entity.phase && e.base_bytecode == g->entity.base_bytecode &&
           e.item_bytecode == g->entity.item_bytecode && e.base.world == NULL &&
           e.base.id == g->entity.base.id && e.bounds == g->entity.bounds &&
           e.random == g->entity.random && e.watcher == g->entity.watcher &&
           mc_test_java_random_equal(e.random, &g->ordinary) && math_equal(f->profile, &g->math) &&
           uniquifier(f->profile) == g->unique && c.source == g->classes.source &&
           c.entity_class == g->classes.entity_class && c.bounds_class == g->classes.bounds_class &&
           c.watcher_class == g->classes.watcher_class && c.next_id == g->classes.next_id &&
           c.bounds_pool_cursor == g->classes.bounds_pool_cursor &&
           c.bounds_pool_identity == g->classes.bounds_pool_identity &&
           c.watcher_types_identity == g->classes.watcher_types_identity &&
           c.watcher_type_count == g->classes.watcher_type_count &&
           c.live_entities == g->classes.live_entities &&
           c.allocation_bytes == g->classes.allocation_bytes && w.record == g->watcher.record &&
           w.size == g->watcher.size && w.dirty == g->watcher.dirty &&
           w.index == g->watcher.index && w.type == g->watcher.type &&
           w.byte_value == g->watcher.byte_value && w.record_dirty == g->watcher.record_dirty &&
           e.item.c == g->entity.item.c && e.item.e == g->entity.item.e &&
           float_bits(e.item.d) == float_bits(g->entity.item.d);
}

static bool en_fresh(const mc_entity_nbt_error *e)
{
    return e->status == MC_ENTITY_NBT_OK && e->operation == NULL && e->field == NULL &&
           e->index == SIZE_MAX && e->wrapper_bytecode == 0 && e->name_bytecode == 0 &&
           e->factory_bytecode == 0 && e->base_bytecode == 0 && e->item_bytecode == 0 &&
           e->stack_bytecode == 0 && e->initializer_bytecode == 0 &&
           e->registration_bytecode == 0 && e->construction.status == MC_ENTITY_OK &&
           e->construction.operation == NULL && e->construction.bytecode == 0 &&
           e->construction.profile == MC_JAVA_RANDOM_PROFILE_OK &&
           e->construction.item == MC_ITEM_STACK_OK &&
           e->construction.world == MC_WORLD_STORAGE_OK && e->stack.status == MC_ITEM_STACK_OK &&
           e->stack.field == NULL && e->stack.nbt.status == MC_NBT_OK && e->stack.nbt.offset == 0 &&
           e->nbt.status == MC_NBT_OK && e->nbt.offset == 0 &&
           e->registry.status == MC_ENTITY_REGISTRY_OK &&
           e->registry.cause == MC_ENTITY_REGISTRY_OK && e->registry.operation == NULL &&
           e->registry.initializer_bytecode == 0 && e->registry.registration_bytecode == 0 &&
           e->registry.class_cause == MC_ENTITY_OK;
}

static bool en_frame(const mc_entity_nbt_error *e, mc_entity_nbt_status status, unsigned int base,
                     unsigned int item, unsigned int stack, const char *field, size_t index)
{
    if (e->status != status || e->operation == NULL || e->base_bytecode != base ||
        e->item_bytecode != item || e->stack_bytecode != stack || e->index != index ||
        e->field == NULL || strcmp(e->field, field) != 0)
        return false;
    mc_entity_nbt_error unselected = *e;
    unselected.status = MC_ENTITY_NBT_OK;
    unselected.operation = unselected.field = NULL;
    unselected.base_bytecode = unselected.item_bytecode = unselected.stack_bytecode = 0;
    unselected.index = SIZE_MAX;
    if (status == MC_ENTITY_NBT_STACK_FAILED) {
        if (unselected.stack.status != MC_ITEM_STACK_INVALID_FIELD ||
            unselected.stack.field == NULL || strcmp(unselected.stack.field, field) != 0)
            return false;
        unselected.stack.status = MC_ITEM_STACK_OK;
        unselected.stack.field = NULL;
    }
    return en_fresh(&unselected);
}

static void en_motion_expected(mc_entity_base_fields *b, size_t completed, bool clamped)
{
    if (completed > 0)
        b->velocity_x = clamped ? 0.0 : 20.0;
    if (completed > 1)
        b->velocity_y = clamped ? 0.0 : 30.0;
    if (completed > 2)
        b->velocity_z = clamped ? 0.0 : 40.0;
}

static void en_position_expected(mc_entity_base_fields *b, size_t completed)
{
    if (completed > 0)
        b->x = b->bl = b->aJ = 8.0;
    if (completed > 1)
        b->y = b->bm = b->aK = 16.0;
    if (completed > 2)
        b->z = b->bn = b->aL = 32.0;
}

static void en_rotation_expected(mc_entity_base_fields *b, size_t completed, bool reduced)
{
    if (completed > 0) {
        b->yaw = reduced ? 5.0f : 725.0f;
        b->aU = 725.0f;
    }
    if (completed > 1) {
        b->aT = reduced ? -5.0f : -725.0f;
        b->aV = -725.0f;
    }
}

static size_t en_tag_count(const mc_nbt_tag *tag)
{
    size_t count = 1;
    if (tag->type == MC_NBT_COMPOUND)
        for (size_t i = 0; i < tag->value.compound.length; ++i)
            count += en_tag_count(tag->value.compound.items[i]);
    if (tag->type == MC_NBT_LIST)
        for (size_t i = 0; i < tag->value.list.length; ++i)
            count += en_tag_count(tag->value.list.items[i]);
    return count;
}

static bool en_shape(const mc_nbt_tag *root)
{
    const char *const names[] = {"Pos", "Motion",   "Rotation", "FallDistance", "Fire",
                                 "Air", "OnGround", "Health",   "Age",          "Item"};
    const mc_nbt_type types[] = {MC_NBT_LIST,  MC_NBT_LIST,    MC_NBT_LIST, MC_NBT_FLOAT,
                                 MC_NBT_SHORT, MC_NBT_SHORT,   MC_NBT_BYTE, MC_NBT_SHORT,
                                 MC_NBT_SHORT, MC_NBT_COMPOUND};
    if (root == NULL || root->type != MC_NBT_COMPOUND || root->value.compound.length != 10)
        return false;
    for (size_t i = 0; i < 10; ++i) {
        const mc_nbt_tag *tag = en_field(root, names[i]);
        if (tag == NULL || tag->type != types[i])
            return false;
        if (i < 3) {
            const size_t length = i == 2 ? 2u : 3u;
            const mc_nbt_type type = i == 2 ? MC_NBT_FLOAT : MC_NBT_DOUBLE;
            if (tag->value.list.length != length || tag->value.list.element_type != type)
                return false;
            for (size_t j = 0; j < length; ++j)
                if (tag->value.list.items[j] == NULL || tag->value.list.items[j]->type != type)
                    return false;
        }
    }
    const mc_nbt_tag *item = en_field(root, "Item"), *id = en_field(item, "id"),
                     *count = en_field(item, "Count"), *damage = en_field(item, "Damage");
    return item->value.compound.length == 3 && id != NULL && id->type == MC_NBT_SHORT &&
           count != NULL && count->type == MC_NBT_BYTE && damage != NULL &&
           damage->type == MC_NBT_SHORT;
}

static int en_write(mc_entity_source source)
{
    int result = 1;
    en_fixture f = {0};
    en_record r;
    en_guard before;
    mc_nbt_tree *tree = NULL, *extra = NULL;
    mc_nbt_document *document = NULL;
    uint8_t *encoded = NULL;
    mc_entity_nbt_error error;
    CHECK(en_open(&f, source));
    en_record_init(&r);
    /* The genuine world-only constructor leaves Stack NULL. Late Item37 is
     * reached after base145, with independent output unchanged. */
    CHECK(en_capture(&f, &before));
    CHECK(mc_entity_build_nbt(f.entity, NULL, &tree, &error) == MC_ENTITY_NBT_NULL_REFERENCE &&
          tree == NULL &&
          en_frame(&error, MC_ENTITY_NBT_NULL_REFERENCE, 145, 37, 0, "Item", SIZE_MAX) &&
          en_unrelated(&f, &before));
    CHECK(mc_entity_read_nbt(f.entity, &r.root, &error) == MC_ENTITY_NBT_OK && en_fresh(&error));
    /* Explicit source-field fixture inputs, on a real constructed owner. */
    f.entity->base.y = 16777217.0;
    f.entity->base.y_size = 0.5f;
    f.entity->base.bv = 65535;
    f.entity->base.bz = 65537;
    f.entity->item.f = 384;
    f.entity->item.b = 98305;
    CHECK(en_capture(&f, &before));
    CHECK(mc_entity_build_nbt(f.entity, NULL, &tree, &error) == MC_ENTITY_NBT_OK && tree != NULL &&
          en_fresh(&error));
    const mc_nbt_tag *root = mc_nbt_tree_root(tree), *pos = en_field(root, "Pos"),
                     *motion = en_field(root, "Motion"), *rotation = en_field(root, "Rotation"),
                     *item = en_field(root, "Item");
    CHECK(en_shape(root) && en_tag_count(root) == 22 && en_field(root, "id") == NULL &&
          pos != NULL && motion != NULL && rotation != NULL && item != NULL);
    CHECK(pos->type == MC_NBT_LIST && pos->value.list.element_type == MC_NBT_DOUBLE &&
          pos->value.list.length == 3 &&
          pos->value.list.items[0]->value.double_bits == double_bits(8.0) &&
          pos->value.list.items[1]->value.double_bits == double_bits(16777217.5) &&
          pos->value.list.items[2]->value.double_bits == double_bits(32.0));
    CHECK(motion->type == MC_NBT_LIST && motion->value.list.length == 3 &&
          motion->value.list.items[0]->value.double_bits == 0 &&
          motion->value.list.items[1]->value.double_bits == 0 &&
          motion->value.list.items[2]->value.double_bits == 0 && rotation->value.list.length == 2 &&
          rotation->value.list.items[0]->value.float_bits == float_bits(5.0f) &&
          rotation->value.list.items[1]->value.float_bits == float_bits(-5.0f));
    CHECK(en_field(root, "FallDistance")->value.float_bits == float_bits(3.5f) &&
          en_field(root, "Fire")->type == MC_NBT_SHORT &&
          en_field(root, "Fire")->value.short_value == -1 &&
          en_field(root, "Air")->value.short_value == 1 &&
          en_field(root, "OnGround")->value.byte_value == 1 &&
          en_field(root, "Health")->type == MC_NBT_SHORT &&
          en_field(root, "Health")->value.short_value == -128 &&
          en_field(root, "Age")->value.short_value == -32767 && item->type == MC_NBT_COMPOUND &&
          item->value.compound.length == 3 && en_field(item, "id")->value.short_value == -7 &&
          en_field(item, "Count")->value.byte_value == -3 &&
          en_field(item, "Damage")->value.short_value == -11);
    mc_entity_snapshot after;
    CHECK(mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK &&
          base_equal(&after.base, &before.entity.base) && after.stack == before.entity.stack &&
          after.item.f == before.entity.item.f && after.item.b == before.entity.item.b &&
          bounds_equal(after.bounds, &before.bounds) && en_unrelated(&f, &before));
    size_t length = 0, written = 0, consumed = 0;
    CHECK(mc_nbt_measure(root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length &&
          mc_nbt_read_compound(encoded, written, NULL, &document, &consumed, NULL) == MC_NBT_OK &&
          consumed == written && en_tag_count(mc_nbt_document_root(document)) == 22 &&
          en_field(mc_nbt_document_root(document), "id") == NULL);
    for (size_t control = 0; control < 5; ++control) {
        mc_nbt_limits limits = mc_nbt_default_limits();
        if (control == 0)
            limits.max_tags = 21;
        if (control == 1)
            limits.max_elements = 9;
        if (control == 2)
            limits.max_depth = 2;
        if (control == 3)
            limits.max_output_bytes = length - 1;
        if (control == 4)
            limits.max_allocation_bytes = mc_nbt_tree_allocation_bytes(tree) - 1;
        mc_nbt_tree *out = tree;
        CHECK(mc_entity_build_nbt(f.entity, &limits, &out, &error) == MC_ENTITY_NBT_LIMIT &&
              out == tree && error.base_bytecode == 148 && error.item_bytecode == 43 &&
              error.stack_bytecode == 34 && error.nbt.status == MC_NBT_LIMIT &&
              en_unrelated(&f, &before));
    }
    mc_nbt_limits exact = mc_nbt_default_limits();
    exact.max_input_bytes = 1; /* Unused by independent tree construction. */
    exact.max_tags = 22;
    exact.max_elements = 10;
    exact.max_depth = 3;
    exact.max_output_bytes = length;
    CHECK(mc_entity_build_nbt(f.entity, &exact, &extra, &error) == MC_ENTITY_NBT_OK &&
          extra != tree && en_fresh(&error) && en_unrelated(&f, &before));
    result = 0;
cleanup:
    free(encoded);
    mc_nbt_document_free(document);
    mc_nbt_tree_free(extra);
    mc_nbt_tree_free(tree);
    if (!en_close(&f))
        result = 1;
    return result;
}

/* A table of independent source failure boundaries. completed is the number
 * of assignments BEFORE the selected vector getter/cast/value receiver. */
static int en_vector_failure(mc_entity_source source, size_t group, size_t selected,
                             unsigned int control)
{
    int result = 1;
    en_fixture f = {0};
    en_record r;
    en_guard before;
    mc_entity_nbt_error error;
    CHECK(en_open(&f, source));
    en_record_init(&r);
    CHECK(en_capture(&f, &before));
    const mc_nbt_tag **items = group == 0 ? r.motion : group == 1 ? r.pos : r.rotation;
    const size_t field_index = group == 0 ? 1 : group == 1 ? 0 : 2;
    const size_t offset = group == 0 ? 3 : group == 1 ? 0 : 6;
    const char *const field_name = group == 0 ? "Motion" : group == 1 ? "Pos" : "Rotation";
    static const unsigned int get_pcs[3][3] = {{25, 40, 55}, {129, 154, 179}, {204, 225, 0}};
    static const unsigned int cast_pcs[3][3] = {{28, 43, 58}, {132, 157, 182}, {207, 228, 0}};
    static const unsigned int value_pcs[3][3] = {{31, 46, 61}, {135, 160, 185}, {210, 231, 0}};
    if (control == 0)
        r.fields[field_index].value.list.length = selected;
    if (control == 1)
        r.vectors[offset + selected].type = MC_NBT_INT;
    if (control == 2)
        items[selected] = NULL;
    const mc_entity_nbt_status expected_status = control == 0   ? MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS
                                                 : control == 1 ? MC_ENTITY_NBT_WRONG_TAG
                                                                : MC_ENTITY_NBT_NULL_REFERENCE;
    const unsigned int pc = control == 0   ? get_pcs[group][selected]
                            : control == 1 ? cast_pcs[group][selected]
                                           : value_pcs[group][selected];
    const mc_entity_nbt_status status = mc_entity_read_nbt(f.entity, &r.root, &error);
    mc_entity_base_fields expected = before.entity.base;
    if (group == 0)
        en_motion_expected(&expected, selected, false);
    else {
        en_motion_expected(&expected, 3, true);
        en_position_expected(&expected, group == 1 ? selected : 3);
        if (group == 2)
            en_rotation_expected(&expected, selected, false);
    }
    mc_entity_snapshot after;
    CHECK(status == expected_status && en_frame(&error, status, pc, 0, 0, field_name, selected) &&
          mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK &&
          base_equal(&after.base, &expected) && after.stack == before.entity.stack &&
          after.item.f == before.entity.item.f && after.item.b == before.entity.item.b &&
          bounds_equal(after.bounds, &before.bounds) && en_unrelated(&f, &before));
    result = 0;
cleanup:
    if (!en_close(&f))
        result = 1;
    return result;
}

static int en_getter_order(mc_entity_source source)
{
    int result = 1;
    en_fixture f = {0};
    en_record r;
    en_guard before;
    mc_entity_nbt_error error;
    CHECK(en_open(&f, source) && en_capture(&f, &before));
    for (size_t field_index = 0; field_index < 3; ++field_index) {
        en_record_init(&r);
        r.fields[field_index].type = MC_NBT_INT;
        /* Earlier missing Pos/Motion do not perform an indexed access before
         * the wrong later named List has been fetched. */
        if (field_index != 0)
            r.root_items[0] = &r.fields[3];
        if (field_index == 2)
            r.root_items[1] = &r.fields[4];
        r.root.value.compound.length = 3;
        /* Avoid duplicate names while making the earlier List absent. */
        CHECK(mc_entity_read_nbt(f.entity, &r.root, &error) == MC_ENTITY_NBT_WRONG_TAG &&
              en_frame(&error, MC_ENTITY_NBT_WRONG_TAG,
                       field_index == 0   ? 3u
                       : field_index == 1 ? 10u
                                          : 17u,
                       0, 0,
                       field_index == 0   ? "Pos"
                       : field_index == 1 ? "Motion"
                                          : "Rotation",
                       SIZE_MAX));
        mc_entity_snapshot after;
        CHECK(mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK &&
              base_equal(&after.base, &before.entity.base) &&
              bounds_equal(after.bounds, &before.bounds) && en_unrelated(&f, &before));
    }
    en_record_init(&r);
    r.root.value.compound.length = 0;
    CHECK(mc_entity_read_nbt(f.entity, &r.root, &error) == MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS &&
          en_frame(&error, MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS, 25, 0, 0, "Motion", 0));
    CHECK(mc_entity_read_nbt(f.entity, NULL, &error) == MC_ENTITY_NBT_NULL_REFERENCE &&
          en_frame(&error, MC_ENTITY_NBT_NULL_REFERENCE, 3, 0, 0, "Pos", SIZE_MAX));
    result = 0;
cleanup:
    if (!en_close(&f))
        result = 1;
    return result;
}

static int en_scalar_failure(mc_entity_source source, size_t selected)
{
    int result = 1;
    en_fixture f = {0};
    en_record r;
    en_guard before;
    mc_entity_nbt_error error;
    static const unsigned int pcs[] = {245, 255, 265, 275, 4, 18, 27};
    const char *const names[] = {"FallDistance", "Fire", "Air", "OnGround",
                                 "Health",       "Age",  "Item"};
    CHECK(en_open(&f, source));
    en_record_init(&r);
    CHECK(en_capture(&f, &before));
    r.fields[selected + 3].type = MC_NBT_INT;
    const mc_entity_nbt_status status = mc_entity_read_nbt(f.entity, &r.root, &error);
    mc_entity_base_fields expected = before.entity.base;
    en_motion_expected(&expected, 3, true);
    en_position_expected(&expected, 3);
    en_rotation_expected(&expected, 2, selected >= 4);
    if (selected > 0)
        expected.bk = 3.5f;
    if (selected > 1)
        expected.bv = -19;
    if (selected > 2)
        expected.bz = 298;
    if (selected > 3)
        expected.aX = true;
    const mc_entity_bounds new_bounds = {7.875, 15.875, 31.875, 8.125, 16.125, 32.125};
    mc_entity_snapshot after;
    CHECK(status == MC_ENTITY_NBT_WRONG_TAG &&
          en_frame(&error, status, selected < 4 ? pcs[selected] : 311,
                   selected < 4 ? 0 : pcs[selected], 0, names[selected], SIZE_MAX) &&
          mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK &&
          base_equal(&after.base, &expected) &&
          after.item.f == (selected > 4 ? 255 : before.entity.item.f) &&
          after.item.b == (selected > 5 ? -17 : before.entity.item.b) &&
          after.stack == before.entity.stack &&
          bounds_equal(after.bounds, selected < 4 ? &before.bounds : &new_bounds) &&
          en_unrelated(&f, &before));
    result = 0;
cleanup:
    if (!en_close(&f))
        result = 1;
    return result;
}

static int en_read_success(mc_entity_source source)
{
    int result = 1;
    en_fixture f = {0};
    en_record r;
    en_guard before;
    mc_item_stack *old = NULL, *now = NULL;
    mc_entity_nbt_error error;
    CHECK(en_open(&f, source));
    en_record_init(&r);
    /* Explicit nonzero source y_size remains present on read, and contributes
     * to the same existing bounds rather than being subtracted from Pos. */
    f.entity->base.y_size = 0.5f;
    CHECK(en_capture(&f, &before));
    CHECK(mc_entity_read_nbt(f.entity, &r.root, &error) == MC_ENTITY_NBT_OK && en_fresh(&error));
    mc_entity_snapshot after;
    mc_entity_base_fields expected = before.entity.base;
    en_motion_expected(&expected, 3, true);
    en_position_expected(&expected, 3);
    en_rotation_expected(&expected, 2, true);
    expected.bk = 3.5f;
    expected.bv = -19;
    expected.bz = 298;
    expected.aX = true;
    const mc_entity_bounds bounds = {7.875, 16.375, 31.875, 8.125, 16.625, 32.125};
    mc_item_stack_fields stack;
    mc_item_stack_phase stack_phase;
    CHECK(mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK &&
          base_equal(&after.base, &expected) && bounds_equal(after.bounds, &bounds) &&
          after.item.f == 255 && after.item.b == -17 && after.stack != NULL &&
          mc_item_stack_fields_get(after.stack, &stack) == MC_ITEM_STACK_OK && stack.id == -7 &&
          stack.count == -3 && stack.damage == -11 && stack.animation == 0 &&
          mc_item_stack_phase_get(after.stack, &stack_phase) == MC_ITEM_STACK_OK &&
          stack_phase == MC_ITEM_STACK_READY && en_unrelated(&f, &before));
    CHECK(mc_item_entity_stack_get(f.entity, &old) == MC_ENTITY_OK && old == after.stack);
    /* Real missing Item creates a new zero Stack. Externally held old owner
     * stays usable; this is the actual reference-replacement boundary40. */
    r.root.value.compound.length = 9;
    CHECK(mc_entity_read_nbt(f.entity, &r.root, &error) == MC_ENTITY_NBT_OK && en_fresh(&error) &&
          mc_item_entity_stack_get(f.entity, &now) == MC_ENTITY_OK && now != NULL && now != old &&
          mc_item_stack_fields_get(now, &stack) == MC_ITEM_STACK_OK && stack.id == 0 &&
          stack.count == 0 && stack.damage == 0 && stack.animation == 0 &&
          mc_item_stack_phase_get(now, &stack_phase) == MC_ITEM_STACK_OK &&
          stack_phase == MC_ITEM_STACK_READY &&
          mc_item_stack_fields_get(old, &stack) == MC_ITEM_STACK_OK && stack.id == -7 &&
          stack.count == -3);
    /* Missing scalars are source zero/defaults, after mandatory vectors. */
    r.root.value.compound.length = 3;
    CHECK(mc_entity_read_nbt(f.entity, &r.root, &error) == MC_ENTITY_NBT_OK && en_fresh(&error) &&
          mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK && after.base.bv == 0 &&
          after.base.bz == 0 && float_bits(after.base.bk) == 0 && !after.base.aX &&
          after.item.f == 0 && after.item.b == 0 && en_unrelated(&f, &before));
    result = 0;
cleanup:
    mc_item_stack_release(now);
    mc_item_stack_release(old);
    if (!en_close(&f))
        result = 1;
    return result;
}

static int en_stack_failure(mc_entity_source source, size_t selected)
{
    int result = 1;
    en_fixture f = {0};
    en_record r;
    en_guard before;
    mc_item_stack *old = NULL;
    mc_entity_nbt_error error;
    CHECK(en_open(&f, source));
    en_record_init(&r);
    CHECK(mc_entity_read_nbt(f.entity, &r.root, NULL) == MC_ENTITY_NBT_OK &&
          mc_item_entity_stack_get(f.entity, &old) == MC_ENTITY_OK && old != NULL &&
          en_capture(&f, &before));
    r.fields[7].value.short_value = 128;
    r.fields[8].value.short_value = -123;
    r.stack[selected].type = MC_NBT_INT;
    const mc_entity_nbt_status status = mc_entity_read_nbt(f.entity, &r.root, &error);
    const char *const names[] = {"id", "Count", "Damage"};
    const unsigned int pcs[] = {4, 14, 24};
    mc_entity_snapshot after;
    mc_item_stack_fields fields;
    CHECK(status == MC_ENTITY_NBT_STACK_FAILED &&
          en_frame(&error, status, 311, 37, pcs[selected], names[selected], SIZE_MAX) &&
          mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK && after.stack == old &&
          after.item.f == 128 && after.item.b == -123 &&
          mc_item_stack_fields_get(old, &fields) == MC_ITEM_STACK_OK && fields.id == -7 &&
          fields.count == -3 && fields.damage == -11 && en_unrelated(&f, &before));
    result = 0;
cleanup:
    mc_item_stack_release(old);
    if (!en_close(&f))
        result = 1;
    return result;
}

static int en_motion_edges(mc_entity_source source)
{
    int result = 1;
    en_fixture f = {0};
    en_record r;
    en_guard before;
    mc_entity_nbt_error error;
    /* Literal source outcomes, not a copy of the production clamp predicate. */
    static const uint64_t inputs[][3] = {
        {UINT64_C(0x7ff8000000000042), UINT64_C(0x4024000000000000), UINT64_C(0xc024000000000000)},
        {UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000), UINT64_C(0x8000000000000000)},
        {UINT64_C(0x4024000000000001), UINT64_C(0xc024000000000001), UINT64_C(0x4023ffffffffffff)}};
    static const uint64_t expected[][3] = {
        {UINT64_C(0x7ff8000000000042), UINT64_C(0x4024000000000000), UINT64_C(0xc024000000000000)},
        {0, 0, UINT64_C(0x8000000000000000)},
        {0, 0, UINT64_C(0x4023ffffffffffff)}};
    CHECK(en_open(&f, source) && en_capture(&f, &before));
    for (size_t row = 0; row < 3; ++row) {
        en_record_init(&r);
        for (size_t axis = 0; axis < 3; ++axis)
            r.vectors[axis + 3].value.double_bits = inputs[row][axis];
        CHECK(mc_entity_read_nbt(f.entity, &r.root, &error) == MC_ENTITY_NBT_OK &&
              en_fresh(&error));
        mc_entity_snapshot after;
        CHECK(mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK &&
              double_bits(after.base.velocity_x) == expected[row][0] &&
              double_bits(after.base.velocity_y) == expected[row][1] &&
              double_bits(after.base.velocity_z) == expected[row][2] && en_unrelated(&f, &before));
    }
    /* Nonfinite angles remain in previous fields, while current infinity
     * reaches float frem -> NaN. Signed-zero current/previous is preserved. */
    en_record_init(&r);
    r.vectors[6].value.float_bits = UINT32_C(0x7f800000);
    r.vectors[7].value.float_bits = UINT32_C(0x80000000);
    CHECK(mc_entity_read_nbt(f.entity, &r.root, &error) == MC_ENTITY_NBT_OK);
    mc_entity_snapshot after;
    CHECK(mc_entity_get_snapshot(f.entity, &after) == MC_ENTITY_OK &&
          (float_bits(after.base.yaw) & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000) &&
          float_bits(after.base.aU) == UINT32_C(0x7f800000) &&
          float_bits(after.base.aT) == UINT32_C(0x80000000) &&
          float_bits(after.base.aV) == UINT32_C(0x80000000) && en_unrelated(&f, &before));
    result = 0;
cleanup:
    if (!en_close(&f))
        result = 1;
    return result;
}

static int en_stack_phases(mc_entity_source source, unsigned int which)
{
    int result = 1;
    en_fixture f = {0};
    en_record r;
    mc_item_stack *stack = NULL;
    mc_nbt_tree *tree = NULL;
    mc_item_stack_phase phase;
    mc_entity_nbt_error error;
    CHECK(mc_java_random_profile_create(profile_budget(), &f.profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, f.profile, NULL, &f.classes) == MC_ENTITY_OK &&
          mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK);
    en_record_init(&r);
    if (which == 1)
        CHECK(mc_item_stack_construct(stack, 98305, 511, 65538) == MC_ITEM_STACK_OK);
    if (which == 2) {
        r.stack[1].type = MC_NBT_INT;
        CHECK(mc_item_stack_construct_nbt(stack, &r.fields[9], NULL) ==
              MC_ITEM_STACK_INVALID_FIELD);
    }
    CHECK(mc_item_stack_phase_get(stack, &phase) == MC_ITEM_STACK_OK &&
          phase == (which == 0   ? MC_ITEM_STACK_RAW
                    : which == 1 ? MC_ITEM_STACK_READY
                                 : MC_ITEM_STACK_FAILED_CONSTRUCTION) &&
          mc_item_entity_allocate(f.classes, &f.entity, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(f.entity, NULL, 8.0, 16.0, 32.0, stack, NULL) == MC_ENTITY_OK);
    en_guard before;
    CHECK(en_capture(&f, &before) && before.entity.stack == stack &&
          mc_entity_build_nbt(f.entity, NULL, &tree, &error) == MC_ENTITY_NBT_OK &&
          en_fresh(&error));
    const mc_nbt_tag *item = en_field(mc_nbt_tree_root(tree), "Item");
    CHECK(en_shape(mc_nbt_tree_root(tree)) && item != NULL &&
          en_field(item, "id")->value.short_value == (which == 0   ? 0
                                                      : which == 1 ? -32767
                                                                   : -7) &&
          en_field(item, "Count")->value.byte_value == (which == 1 ? -1 : 0) &&
          en_field(item, "Damage")->value.short_value == (which == 1 ? 2 : 0) &&
          en_unrelated(&f, &before) && mc_item_stack_phase_get(stack, &phase) == MC_ITEM_STACK_OK &&
          phase == (which == 0   ? MC_ITEM_STACK_RAW
                    : which == 1 ? MC_ITEM_STACK_READY
                                 : MC_ITEM_STACK_FAILED_CONSTRUCTION));
    result = 0;
cleanup:
    mc_nbt_tree_free(tree);
    mc_item_stack_release(stack);
    if (!en_close(&f))
        result = 1;
    return result;
}

typedef struct en_mapping {
    mc_entity_class_kind kind;
    const char *name, *client, *server;
    int32_t id;
} en_mapping;

static const en_mapping en_mappings[] = {
    {MC_ENTITY_CLASS_KIND_ARROW, "Arrow", "sl", "lp", 10},
    {MC_ENTITY_CLASS_KIND_SNOWBALL, "Snowball", "by", "be", 11},
    {MC_ENTITY_CLASS_KIND_ITEM, "Item", "hl", "ez", 1},
    {MC_ENTITY_CLASS_KIND_PAINTING, "Painting", "qv", "kf", 9},
    {MC_ENTITY_CLASS_KIND_MOB, "Mob", "ls", "hl", 48},
    {MC_ENTITY_CLASS_KIND_MONSTER, "Monster", "gz", "ep", 49},
    {MC_ENTITY_CLASS_KIND_CREEPER, "Creeper", "gb", "ec", 50},
    {MC_ENTITY_CLASS_KIND_SKELETON, "Skeleton", "fr", "dv", 51},
    {MC_ENTITY_CLASS_KIND_SPIDER, "Spider", "cn", "bn", 52},
    {MC_ENTITY_CLASS_KIND_GIANT, "Giant", "nt", "ip", 53},
    {MC_ENTITY_CLASS_KIND_ZOMBIE, "Zombie", "uz", "ne", 54},
    {MC_ENTITY_CLASS_KIND_SLIME, "Slime", "uw", "nb", 55},
    {MC_ENTITY_CLASS_KIND_GHAST, "Ghast", "bp", "aw", 56},
    {MC_ENTITY_CLASS_KIND_PIG_ZOMBIE, "PigZombie", "ya", "pi", 57},
    {MC_ENTITY_CLASS_KIND_PIG, "Pig", "wh", "oc", 90},
    {MC_ENTITY_CLASS_KIND_SHEEP, "Sheep", "dl", "ce", 91},
    {MC_ENTITY_CLASS_KIND_COW, "Cow", "bx", "bd", 92},
    {MC_ENTITY_CLASS_KIND_CHICKEN, "Chicken", "ww", "or", 93},
    {MC_ENTITY_CLASS_KIND_SQUID, "Squid", "xt", "pf", 94},
    {MC_ENTITY_CLASS_KIND_WOLF, "Wolf", "gi", "eh", 95},
    {MC_ENTITY_CLASS_KIND_PRIMED_TNT, "PrimedTnt", "qw", "kg", 20},
    {MC_ENTITY_CLASS_KIND_FALLING_SAND, "FallingSand", "ju", "gl", 21},
    {MC_ENTITY_CLASS_KIND_MINECART, "Minecart", "yl", "pr", 40},
    {MC_ENTITY_CLASS_KIND_BOAT, "Boat", "fz", "eb", 41}};

static bool en_registry_fresh(const mc_entity_registry_error *error)
{
    return error->status == MC_ENTITY_REGISTRY_OK && error->cause == MC_ENTITY_REGISTRY_OK &&
           error->operation == NULL && error->initializer_bytecode == 0 &&
           error->registration_bytecode == 0 && error->class_cause == MC_ENTITY_OK;
}

static bool en_cold_classes(const mc_entity_class_snapshot *snapshot)
{
    return snapshot->entity_class == MC_ENTITY_CLASS_COLD &&
           snapshot->bounds_class == MC_ENTITY_CLASS_COLD &&
           snapshot->watcher_class == MC_ENTITY_CLASS_COLD && snapshot->next_id == 0 &&
           snapshot->live_entities == 0 && snapshot->bounds_pool_cursor == 0 &&
           snapshot->bounds_pool_identity == NULL && snapshot->watcher_types_identity == NULL &&
           snapshot->watcher_type_count == 0;
}

static int en_registry(mc_entity_source source)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *foreign = NULL, *limited = NULL;
    const mc_entity_class_literal *literals[24] = {0};
    mc_entity_class_snapshot initial, after;
    mc_entity_registry_view view;
    mc_entity_registry_error error;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK &&
          mc_entity_classes_create(source, profile, NULL, &foreign) == MC_ENTITY_OK &&
          mc_entity_classes_get_snapshot(classes, &initial) == MC_ENTITY_OK &&
          en_cold_classes(&initial));
    const uint64_t unique = uniquifier(profile);
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
    CHECK(mc_entity_registry_inspect(classes, &view) == MC_ENTITY_REGISTRY_OK &&
          view.phase == MC_ENTITY_CLASS_COLD && en_registry_fresh(&view.first_failure));
    for (size_t i = 0; i < 4; ++i)
        CHECK(view.maps[i] == NULL && view.counts[i] == 0);
    for (size_t i = 0; i < 24; ++i) {
        mc_entity_class_literal_view literal;
        const mc_entity_class_literal *again = NULL;
        CHECK(mc_entity_class_literal_get(classes, en_mappings[i].kind, &literals[i]) ==
                  MC_ENTITY_OK &&
              literals[i] != NULL &&
              mc_entity_class_literal_get(classes, en_mappings[i].kind, &again) == MC_ENTITY_OK &&
              again == literals[i] &&
              mc_entity_class_literal_inspect(literals[i], &literal) == MC_ENTITY_OK &&
              literal.owner == classes && literal.source == source &&
              literal.kind == en_mappings[i].kind &&
              strcmp(literal.client_name, en_mappings[i].client) == 0 &&
              strcmp(literal.server_name, en_mappings[i].server) == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(literals[j] != literals[i]);
    }
    const mc_entity_class_literal *unchanged = literals[0];
    CHECK(mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_COUNT, &unchanged) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          unchanged == literals[0] &&
          mc_entity_classes_get_snapshot(classes, &after) == MC_ENTITY_OK &&
          en_cold_classes(&after) && after.allocation_bytes == initial.allocation_bytes &&
          mc_entity_registry_inspect(classes, &view) == MC_ENTITY_REGISTRY_OK &&
          view.phase == MC_ENTITY_CLASS_COLD && uniquifier(profile) == unique &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
    CHECK(mc_entity_classes_initialize_registry(classes, &error) == MC_ENTITY_REGISTRY_OK &&
          en_registry_fresh(&error) &&
          mc_entity_registry_inspect(classes, &view) == MC_ENTITY_REGISTRY_OK &&
          view.phase == MC_ENTITY_CLASS_READY);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(view.maps[i] != NULL && view.counts[i] == 24);
        for (size_t j = 0; j < i; ++j)
            CHECK(view.maps[i] != view.maps[j]);
    }
    for (size_t i = 0; i < 24; ++i) {
        uint16_t units[16];
        const mc_entity_class_literal *named = NULL, *numeric = NULL;
        mc_nbt_string name = {0};
        CHECK(mc_entity_registry_named_class(classes, en_ascii(en_mappings[i].name, units), &named,
                                             &error) == MC_ENTITY_REGISTRY_OK &&
              named == literals[i] && en_registry_fresh(&error) &&
              mc_entity_registry_numeric_class(classes, en_mappings[i].id, &numeric, &error) ==
                  MC_ENTITY_REGISTRY_OK &&
              numeric == literals[i] && en_registry_fresh(&error) &&
              mc_entity_registry_class_name(classes, literals[i], &name, &error) ==
                  MC_ENTITY_REGISTRY_OK &&
              en_text(name, en_mappings[i].name) && en_registry_fresh(&error));
    }
    const char *const misses[] = {"item", "Player", "", "Missing"};
    for (size_t i = 0; i < 4; ++i) {
        uint16_t units[16];
        unchanged = literals[0];
        CHECK(mc_entity_registry_named_class(classes, en_ascii(misses[i], units), &unchanged,
                                             &error) == MC_ENTITY_REGISTRY_OK &&
              unchanged == NULL && en_registry_fresh(&error));
    }
    const int32_t numeric_misses[] = {INT32_MIN, -1, 0, 2, INT32_MAX};
    for (size_t i = 0; i < 5; ++i) {
        unchanged = literals[0];
        CHECK(mc_entity_registry_numeric_class(classes, numeric_misses[i], &unchanged, &error) ==
                  MC_ENTITY_REGISTRY_OK &&
              unchanged == NULL && en_registry_fresh(&error));
    }
    const mc_entity_class_literal *foreign_item = NULL;
    mc_nbt_string absent = {0};
    CHECK(mc_entity_class_literal_get(foreign, MC_ENTITY_CLASS_KIND_ITEM, &foreign_item) ==
              MC_ENTITY_OK &&
          foreign_item != literals[2] &&
          mc_entity_registry_class_name(classes, foreign_item, &absent, &error) ==
              MC_ENTITY_REGISTRY_OK &&
          absent.units == NULL && absent.length == 0 && en_registry_fresh(&error));
    unchanged = literals[0];
    CHECK(mc_entity_registry_named_class(classes, (mc_nbt_string){NULL, 1}, &unchanged, &error) ==
              MC_ENTITY_REGISTRY_INVALID_ARGUMENT &&
          unchanged == literals[0]);
    CHECK(mc_entity_classes_initialize_registry(classes, &error) == MC_ENTITY_REGISTRY_OK &&
          en_registry_fresh(&error) &&
          mc_entity_classes_get_snapshot(classes, &after) == MC_ENTITY_OK &&
          en_cold_classes(&after) && after.allocation_bytes > initial.allocation_bytes &&
          uniquifier(profile) == unique && holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) &&
          profile->math == NULL &&
          mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    /* Derive a genuine creation limit from measured owner allocations. No
     * ledger/limit is changed after creating either independent owner. One
     * byte below complete usage must preserve the source-ordered final put
     * prefix: 24 a/b/c mappings and 23 d mappings. */
    const size_t complete_bytes = after.allocation_bytes;
    for (size_t control = 0; control < 2; ++control) {
        const mc_entity_limits limits = {
            control == 0 ? initial.allocation_bytes : complete_bytes - 1, SIZE_MAX};
        CHECK(mc_entity_classes_create(source, profile, &limits, &limited) == MC_ENTITY_OK);
        CHECK(mc_entity_classes_initialize_registry(limited, &error) == MC_ENTITY_REGISTRY_LIMIT &&
              error.cause == MC_ENTITY_REGISTRY_LIMIT && error.class_cause == MC_ENTITY_OK &&
              error.initializer_bytecode == (control == 0                 ? 0u
                                             : source == MC_ENTITY_CLIENT ? 253u
                                                                          : 252u) &&
              error.registration_bytecode == (control == 0 ? 0u : 44u));
        const mc_entity_registry_error first = error;
        mc_entity_registry_view partial;
        mc_entity_class_snapshot partial_class;
        CHECK(mc_entity_registry_inspect(limited, &partial) == MC_ENTITY_REGISTRY_OK &&
              partial.phase == MC_ENTITY_CLASS_FAILED &&
              partial.first_failure.status == first.status &&
              partial.first_failure.cause == first.cause &&
              partial.first_failure.operation == first.operation &&
              partial.first_failure.initializer_bytecode == first.initializer_bytecode &&
              partial.first_failure.registration_bytecode == first.registration_bytecode &&
              mc_entity_classes_get_snapshot(limited, &partial_class) == MC_ENTITY_OK &&
              en_cold_classes(&partial_class));
        for (size_t i = 0; i < 4; ++i)
            CHECK(partial.counts[i] == (control == 0 ? 0u
                                        : i == 3     ? 23u
                                                     : 24u) &&
                  (partial.maps[i] != NULL) == (control != 0));
        unchanged = literals[0];
        uint16_t units[16];
        CHECK(mc_entity_registry_named_class(limited, en_ascii("Arrow", units), &unchanged,
                                             &error) == MC_ENTITY_REGISTRY_CLASS_FAILED &&
              unchanged == literals[0] && error.status == MC_ENTITY_REGISTRY_CLASS_FAILED &&
              error.cause == first.cause && error.operation == first.operation &&
              error.initializer_bytecode == first.initializer_bytecode &&
              error.registration_bytecode == first.registration_bytecode &&
              error.class_cause == first.class_cause);
        mc_entity_registry_view cached;
        CHECK(mc_entity_registry_inspect(limited, &cached) == MC_ENTITY_REGISTRY_OK &&
              cached.phase == MC_ENTITY_CLASS_FAILED &&
              mc_entity_classes_get_snapshot(limited, &after) == MC_ENTITY_OK &&
              after.allocation_bytes == partial_class.allocation_bytes && en_cold_classes(&after) &&
              uniquifier(profile) == unique && profile->math == NULL);
        for (size_t i = 0; i < 4; ++i)
            CHECK(cached.maps[i] == partial.maps[i] && cached.counts[i] == partial.counts[i]);
        CHECK(retire_classes(&limited) == 0);
    }
    result = 0;
cleanup:
    if (retire_classes(&limited) != 0)
        result = 1;
    if (retire_classes(&foreign) != 0)
        result = 1;
    if (retire_classes(&classes) != 0)
        result = 1;
    if (limited == NULL && foreign == NULL && classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

int main(void)
{
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("Entity NBT tests require the qualified strict SSE environment\n", stderr);
        return 1;
    }
    for (unsigned int side = 0; side < 2; ++side) {
        const mc_entity_source source = side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
        if (en_registry(source) || en_write(source) || en_getter_order(source) ||
            en_read_success(source) || en_motion_edges(source))
            return 1;
        for (size_t group = 0; group < 3; ++group)
            for (size_t index = 0; index < (group == 2 ? 2u : 3u); ++index)
                for (unsigned int control = 0; control < 3; ++control)
                    if (en_vector_failure(source, group, index, control))
                        return 1;
        for (size_t field = 0; field < 7; ++field)
            if (en_scalar_failure(source, field))
                return 1;
        for (size_t field = 0; field < 3; ++field)
            if (en_stack_failure(source, field))
                return 1;
        for (unsigned int phase = 0; phase < 3; ++phase)
            if (en_stack_phases(source, phase))
                return 1;
    }
    printf("PASS: %u Entity NBT leaf and registry checks\n", checks);
    return 0;
}
