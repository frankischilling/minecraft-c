/* Semantic inventory/NBT source contracts. Real constructors and Stack owners;
 * malformed immutable input tags are labelled controls, not observations.
 * Native provenance/GAME round trips belong to the next implementation stage. */
#include "../../../src/entity/minecart/entity_internal.h"
#include "mc/entity/minecart/inventory.h"
#include "mc/entity/nbt/factory.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks, cases;
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        const bool ok_ = !!(condition);                                                            \
        if (!ok_) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *classes;
    mc_entity *cart;
} fixture;

enum { POS, MOTION, ROTATION, FALL, FIRE, AIR, GROUND, TYPE, ITEMS, PUSH_X, PUSH_Z, FUEL, ID };

typedef struct record {
    mc_nbt_tag root, fields[13], vectors[8];
    const mc_nbt_tag *root_items[13], *pos[3], *motion[3], *rotation[2];
    uint16_t names[13][16], id_value[16];
    mc_nbt_tag entries[8], entry_fields[8][4];
    const mc_nbt_tag *list_items[8], *entry_items[8][4];
} record;

static uint64_t bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static mc_nbt_string ascii(const char *value, uint16_t *storage) {
    const size_t length = strlen(value);
    for (size_t i = 0; i < length; ++i)
        storage[i] = (uint16_t)(unsigned char)value[i];
    return (mc_nbt_string){storage, length};
}

static const mc_nbt_tag *field(const mc_nbt_tag *root, const char *name) {
    uint16_t units[16];
    if (root == NULL || root->type != MC_NBT_COMPOUND)
        return NULL;
    const mc_nbt_string key = ascii(name, units);
    return mc_nbt_compound_get(root, key.units, key.length);
}

static void record_init(record *r, int32_t type) {
    static const char *const names[] = {"Pos",   "Motion",   "Rotation", "FallDistance", "Fire",
                                        "Air",   "OnGround", "Type",     "Items",        "PushX",
                                        "PushZ", "Fuel",     "id"};
    static const mc_nbt_type types[] = {MC_NBT_LIST,  MC_NBT_LIST,   MC_NBT_LIST,   MC_NBT_FLOAT,
                                        MC_NBT_SHORT, MC_NBT_SHORT,  MC_NBT_BYTE,   MC_NBT_INT,
                                        MC_NBT_LIST,  MC_NBT_DOUBLE, MC_NBT_DOUBLE, MC_NBT_SHORT,
                                        MC_NBT_STRING};
    static const uint16_t slot_name[] = {'S', 'l', 'o', 't'}, id_name[] = {'i', 'd'},
                          count_name[] = {'C', 'o', 'u', 'n', 't'},
                          damage_name[] = {'D', 'a', 'm', 'a', 'g', 'e'};
    memset(r, 0, sizeof(*r));
    r->root.type = MC_NBT_COMPOUND;
    r->root.value.compound.items = r->root_items;
    r->root.value.compound.length = 12;
    for (size_t i = 0; i < COUNT(r->fields); ++i) {
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->fields[i].type = types[i];
        r->root_items[i] = &r->fields[i];
    }
    for (size_t i = 0; i < 6; ++i) {
        r->vectors[i].type = MC_NBT_DOUBLE;
        r->vectors[i].value.double_bits = bits(i < 3 ? (double)(i + 1u) : 0.0);
    }
    for (size_t i = 0; i < 3; ++i) {
        r->pos[i] = &r->vectors[i];
        r->motion[i] = &r->vectors[i + 3u];
    }
    r->vectors[6].type = r->vectors[7].type = MC_NBT_FLOAT;
    r->rotation[0] = &r->vectors[6];
    r->rotation[1] = &r->vectors[7];
    r->fields[POS].value.list.element_type = MC_NBT_DOUBLE;
    r->fields[POS].value.list.items = r->pos;
    r->fields[POS].value.list.length = 3;
    r->fields[MOTION].value.list.element_type = MC_NBT_DOUBLE;
    r->fields[MOTION].value.list.items = r->motion;
    r->fields[MOTION].value.list.length = 3;
    r->fields[ROTATION].value.list.element_type = MC_NBT_FLOAT;
    r->fields[ROTATION].value.list.items = r->rotation;
    r->fields[ROTATION].value.list.length = 2;
    r->fields[AIR].value.short_value = 300;
    r->fields[TYPE].value.int_value = type;
    r->fields[ITEMS].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[ITEMS].value.list.items = r->list_items;
    r->fields[ITEMS].value.list.length = 0;
    r->fields[PUSH_X].value.double_bits = bits(1.5);
    r->fields[PUSH_Z].value.double_bits = bits(-2.5);
    r->fields[FUEL].value.short_value = 7;
    r->fields[ID].value.string = ascii("Minecart", r->id_value);
    for (size_t i = 0; i < COUNT(r->entries); ++i) {
        mc_nbt_tag *f = r->entry_fields[i];
        f[0] = (mc_nbt_tag){.type = MC_NBT_BYTE, .name = {slot_name, COUNT(slot_name)}};
        f[1] = (mc_nbt_tag){.type = MC_NBT_SHORT, .name = {id_name, COUNT(id_name)}};
        f[2] = (mc_nbt_tag){.type = MC_NBT_BYTE, .name = {count_name, COUNT(count_name)}};
        f[3] = (mc_nbt_tag){.type = MC_NBT_SHORT, .name = {damage_name, COUNT(damage_name)}};
        for (size_t j = 0; j < 4; ++j)
            r->entry_items[i][j] = &f[j];
        r->entries[i] = (mc_nbt_tag){.type = MC_NBT_COMPOUND,
                                     .value.compound = {.items = r->entry_items[i], .length = 4}};
        r->list_items[i] = &r->entries[i];
    }
}

static void entry(record *r, size_t index, int32_t slot, int32_t id, int32_t count,
                  int32_t damage) {
    r->entry_fields[index][0].value.byte_value = mc_java_i8_narrow(slot);
    r->entry_fields[index][1].value.short_value = mc_java_i16_narrow(id);
    r->entry_fields[index][2].value.byte_value = mc_java_i8_narrow(count);
    r->entry_fields[index][3].value.short_value = mc_java_i16_narrow(damage);
    if (r->fields[ITEMS].value.list.length <= index)
        r->fields[ITEMS].value.list.length = index + 1u;
}

static bool open_fixture(fixture *f, mc_entity_source source, int32_t type) {
    return mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
           mc_entity_classes_create(source, f->profile, NULL, &f->classes) == MC_ENTITY_OK &&
           mc_minecart_entity_allocate(f->classes, &f->cart, NULL) == MC_ENTITY_OK &&
           mc_minecart_entity_construct(f->cart, NULL, 3.0, 5.0, 7.0, type, NULL) == MC_ENTITY_OK;
}

static bool close_fixture(fixture *f) {
    mc_entity_release(f->cart);
    f->cart = NULL;
    if (mc_entity_classes_free(f->classes) != MC_ENTITY_OK)
        return false;
    f->classes = NULL;
    mc_java_random_profile_free(f->profile);
    f->profile = NULL;
    return true;
}

static bool make_stack(mc_item_stack **out, int32_t id, int32_t count, int32_t damage) {
    return mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
           mc_item_stack_construct(*out, id, count, damage) == MC_ITEM_STACK_OK;
}

static bool stack_is(const mc_item_stack *stack, int32_t id, int32_t count, int32_t damage) {
    mc_item_stack_fields actual;
    return mc_item_stack_fields_get(stack, &actual) == MC_ITEM_STACK_OK && actual.id == id &&
           actual.count == count && actual.damage == damage;
}

static bool stack_phase(const mc_item_stack *stack, mc_item_stack_phase expected) {
    mc_item_stack_phase actual;
    return mc_item_stack_phase_get(stack, &actual) == MC_ITEM_STACK_OK && actual == expected;
}

static bool tag_is(const mc_nbt_tag *root, const char *name, mc_nbt_type type) {
    const mc_nbt_tag *tag = field(root, name);
    return tag != NULL && tag->type == type;
}

static bool read_site(const mc_entity_nbt_error *error, mc_entity_source source, uint16_t pc) {
    if (error->late_hook != MC_ENTITY_NBT_LATE_CART)
        return false;
    const mc_minecart_site *s = &error->minecart_read;
    return error->base_bytecode == 311 && s->selected && s->entered && s->declaring_class != NULL &&
           strcmp(s->declaring_class, source == MC_ENTITY_CLIENT ? "yl" : "pr") == 0 &&
           s->method != NULL &&
           strcmp(s->method, source == MC_ENTITY_CLIENT ? "a(Lnu;)V" : "a(Liq;)V") == 0 &&
           s->bytecode == pc;
}

static bool write_site(const mc_entity_nbt_error *error, mc_entity_source source, uint16_t pc) {
    if (error->late_hook != MC_ENTITY_NBT_LATE_CART)
        return false;
    const mc_minecart_site *s = &error->minecart_write;
    return error->base_bytecode == 145 && s->selected && s->entered && s->declaring_class != NULL &&
           strcmp(s->declaring_class, source == MC_ENTITY_CLIENT ? "yl" : "pr") == 0 &&
           s->method != NULL &&
           strcmp(s->method, source == MC_ENTITY_CLIENT ? "b(Lnu;)V" : "b(Liq;)V") == 0 &&
           s->bytecode == pc;
}

static bool fresh(const mc_entity_nbt_error *error) {
    return error->late_hook == MC_ENTITY_NBT_LATE_NONE && error->status == MC_ENTITY_NBT_OK &&
           error->operation == NULL && error->field == NULL && error->index == SIZE_MAX &&
           error->base_bytecode == 0 && !error->minecart_read.selected &&
           !error->minecart_read.entered && !error->minecart_write.selected &&
           !error->minecart_write.entered && error->stack.status == MC_ITEM_STACK_OK &&
           error->stack.field == NULL && error->construction.status == MC_ENTITY_OK &&
           error->nbt.status == MC_NBT_OK;
}

static int conditional_types(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    record r;
    mc_item_stack *old = NULL, *held = NULL;
    mc_minecart_entity_snapshot before, after;
    mc_entity_nbt_error error;
    mc_minecart_inventory_error inventory;
    mc_nbt_tree *saved = NULL;
    ++cases;
    CHECK(open_fixture(&f, source, 0));
    CHECK(make_stack(&old, 7, 3, 9));
    CHECK(mc_minecart_inventory_set(f.cart, 35, old, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &before) == MC_ENTITY_OK);
    record_init(&r, 2);
    r.fields[PUSH_X].value.double_bits = UINT64_C(0x7ff8deadbeef0001);
    r.fields[PUSH_Z].value.double_bits = UINT64_C(0x8000000000000000);
    r.fields[FUEL].value.short_value = INT16_MIN;
    /* Type2 must not touch the deliberately wrong Items tag. */
    r.fields[ITEMS].type = MC_NBT_BYTE;
    CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_OK && fresh(&error));
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
    CHECK(after.fields.h == before.fields.h && after.inventory_length == 36 &&
          after.inventory[35] == old && after.fields.d == 2);
    CHECK(bits(after.fields.f) == UINT64_C(0x7ff8deadbeef0001));
    CHECK(bits(after.fields.g) == UINT64_C(0x8000000000000000) && after.fields.e == INT16_MIN);
    CHECK(mc_entity_build_nbt(f.cart, NULL, &saved, &error) == MC_ENTITY_NBT_OK && fresh(&error));
    const mc_nbt_tag *root = mc_nbt_tree_root(saved);
    CHECK(root != NULL && root->type == MC_NBT_COMPOUND && root->value.compound.length == 11 &&
          field(root, "Items") == NULL);
    CHECK(tag_is(root, "PushX", MC_NBT_DOUBLE) && tag_is(root, "PushZ", MC_NBT_DOUBLE));
    CHECK(tag_is(root, "Fuel", MC_NBT_SHORT));
    CHECK(field(root, "PushX")->value.double_bits == UINT64_C(0x7ff8deadbeef0001));
    CHECK(field(root, "PushZ")->value.double_bits == UINT64_C(0x8000000000000000));
    CHECK(field(root, "Fuel")->type == MC_NBT_SHORT &&
          field(root, "Fuel")->value.short_value == INT16_MIN);
    mc_nbt_tree_free(saved);
    saved = NULL;
    /* Explicit own-field input on the genuine owner exercises the writer's
     * int-to-short45 independently of the reader's already-short input. This
     * is a source-field control, restored before any assertion. */
    const int32_t fuel_before = f.cart->minecart->fields.e;
    f.cart->minecart->fields.e = INT32_MAX;
    const mc_entity_nbt_status narrowed = mc_entity_build_nbt(f.cart, NULL, &saved, &error);
    f.cart->minecart->fields.e = fuel_before;
    CHECK(narrowed == MC_ENTITY_NBT_OK);
    CHECK(tag_is(mc_nbt_tree_root(saved), "Fuel", MC_NBT_SHORT));
    CHECK(field(mc_nbt_tree_root(saved), "Fuel")->value.short_value == -1);
    mc_nbt_tree_free(saved);
    saved = NULL;
    before = after;
    record_init(&r, INT32_MIN);
    r.fields[ITEMS].type = r.fields[PUSH_X].type = r.fields[PUSH_Z].type = r.fields[FUEL].type =
        MC_NBT_BYTE;
    CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_OK && fresh(&error));
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
    CHECK(after.fields.d == INT32_MIN && after.fields.h == before.fields.h &&
          bits(after.fields.f) == bits(before.fields.f) &&
          bits(after.fields.g) == bits(before.fields.g) && after.fields.e == before.fields.e);
    CHECK(mc_entity_build_nbt(f.cart, NULL, &saved, &error) == MC_ENTITY_NBT_OK);
    root = mc_nbt_tree_root(saved);
    CHECK(root->value.compound.length == 8 && field(root, "Type")->value.int_value == INT32_MIN);
    CHECK(field(root, "Items") == NULL && field(root, "PushX") == NULL &&
          field(root, "PushZ") == NULL && field(root, "Fuel") == NULL);
    CHECK(mc_minecart_inventory_get(f.cart, 35, &held, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(held == old);
    /* Missing Type uses the actual Int getter's default zero. The stale
     * inventory/push/fuel fields remain, with no implicit constructor reset. */
    mc_item_stack_release(held);
    held = NULL;
    r.root.value.compound.length = 7;
    CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_OK);
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
    CHECK(after.fields.d == 0 && after.fields.h == before.fields.h &&
          bits(after.fields.f) == bits(before.fields.f) &&
          bits(after.fields.g) == bits(before.fields.g) && after.fields.e == before.fields.e);
    result = 0;
cleanup:
    mc_nbt_tree_free(saved);
    mc_item_stack_release(held);
    mc_item_stack_release(old);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

static int type2_failure_prefixes(mc_entity_source source) {
    static const size_t bad_fields[] = {TYPE, PUSH_X, PUSH_Z, FUEL};
    static const uint16_t pcs[] = {4, 22, 32, 42};
    for (size_t cut = 0; cut < COUNT(bad_fields); ++cut) {
        int result = 1;
        fixture f = {0};
        record r;
        mc_minecart_entity_snapshot before, after;
        mc_entity_nbt_error error;
        ++cases;
        CHECK(open_fixture(&f, source, -17));
        record_init(&r, 2);
        CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_OK);
        r.fields[TYPE].value.int_value = -17;
        CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_OK);
        CHECK(mc_minecart_entity_get_snapshot(f.cart, &before) == MC_ENTITY_OK);
        r.fields[TYPE].value.int_value = 2;
        r.fields[PUSH_X].value.double_bits = bits(9.5);
        r.fields[PUSH_Z].value.double_bits = bits(-10.5);
        r.fields[FUEL].value.short_value = -19;
        r.fields[bad_fields[cut]].type = MC_NBT_BYTE;
        CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_WRONG_TAG);
        CHECK(read_site(&error, source, pcs[cut]));
        CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
        CHECK(after.fields.h == before.fields.h && after.fields.e == before.fields.e);
        CHECK(bits(after.fields.f) == (cut >= 2 ? bits(9.5) : bits(before.fields.f)));
        CHECK(bits(after.fields.g) == (cut >= 3 ? bits(-10.5) : bits(before.fields.g)));
        CHECK(after.fields.d == (cut == 0 ? before.fields.d : 2));
        result = 0;
    cleanup:
        if (!close_fixture(&f))
            result = 1;
        if (result != 0)
            return result;
    }
    return 0;
}

static int missing_wrong_items_and_bounds(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    record r;
    mc_item_stack *old = NULL, *held = NULL;
    mc_minecart_entity_snapshot before, after;
    mc_entity_nbt_error error;
    mc_minecart_inventory_error inventory;
    mc_nbt_tree *saved = NULL;
    uintptr_t old_h = 0;
    ++cases;
    CHECK(open_fixture(&f, source, 1));
    CHECK(make_stack(&old, 31, 9, 11));
    CHECK(mc_minecart_inventory_set(f.cart, 35, old, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_entity_build_nbt(f.cart, NULL, &saved, &error) == MC_ENTITY_NBT_OK);
    const mc_nbt_tag *items = field(mc_nbt_tree_root(saved), "Items");
    CHECK(items != NULL && items->value.list.length == 1);
    CHECK(field(items->value.list.items[0], "Slot")->value.byte_value == 35);
    /* Reload that actual independent fresh save: slot35 is ignored only after
     * the actual replacement length27 is published. */
    CHECK(mc_entity_read_nbt(f.cart, mc_nbt_tree_root(saved), &error) == MC_ENTITY_NBT_OK);
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
    CHECK(after.inventory_length == 27);
    CHECK(mc_minecart_inventory_get(f.cart, 35, &held, &inventory) ==
          MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS);
    CHECK(held == NULL);
    CHECK(mc_minecart_inventory_get(f.cart, 26, &held, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(held == NULL);
    CHECK(stack_is(old, 31, 9, 11));
    record_init(&r, 1);
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &before) == MC_ENTITY_OK);
    old_h = (uintptr_t)before.fields.h;
    r.fields[ITEMS].type = MC_NBT_INT;
    CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_WRONG_TAG);
    CHECK(read_site(&error, source, 62));
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
    CHECK((uintptr_t)after.fields.h == old_h && after.fields.d == 1);
    /* Missing Items is a normal empty list, not a failed getter or retained h. */
    r.root.value.compound.length = 8;
    CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_OK && fresh(&error));
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
    CHECK(after.inventory_length == 27 && (uintptr_t)after.fields.h != old_h);
    result = 0;
cleanup:
    mc_nbt_tree_free(saved);
    mc_item_stack_release(held);
    mc_item_stack_release(old);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

static int duplicate_and_stack_failures(mc_entity_source source) {
    static const unsigned int stack_pcs[] = {4, 14, 24};
    static const char *const names[] = {"id", "Count", "Damage"};
    for (size_t cut = 0; cut < COUNT(stack_pcs); ++cut) {
        int result = 1;
        fixture f = {0};
        record r;
        mc_item_stack *held = NULL;
        mc_minecart_entity_snapshot before, after;
        mc_entity_nbt_error error;
        mc_minecart_inventory_error inventory;
        uintptr_t old_h = 0;
        ++cases;
        CHECK(open_fixture(&f, source, 0));
        CHECK(mc_minecart_entity_get_snapshot(f.cart, &before) == MC_ENTITY_OK);
        old_h = (uintptr_t)before.fields.h;
        record_init(&r, 1);
        entry(&r, 0, 4, 10, 1, 2);
        entry(&r, 1, 4, 20, 3, 4);
        entry(&r, 2, 27, 99, 5, 6);
        r.entry_fields[2][1].type = MC_NBT_BYTE; /* Ignored slot: do not construct. */
        entry(&r, 3, 5, 30, 7, 8);
        r.entry_fields[3][cut + 1u].type = cut == 1 ? MC_NBT_SHORT : MC_NBT_BYTE;
        CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_STACK_FAILED);
        CHECK(read_site(&error, source, 137) && error.index == 3 &&
              error.stack_bytecode == stack_pcs[cut]);
        CHECK(error.stack.status == MC_ITEM_STACK_INVALID_FIELD && error.stack.field != NULL &&
              strcmp(error.stack.field, names[cut]) == 0);
        CHECK(error.late_hook == MC_ENTITY_NBT_LATE_CART && error.inventory.slot_selected &&
              error.inventory.slot == 5);
        CHECK(error.late_hook == MC_ENTITY_NBT_LATE_CART &&
              error.inventory.stack_cause == MC_MINECART_INVENTORY_STACK_NBT_CONSTRUCTOR);
        CHECK(error.late_hook == MC_ENTITY_NBT_LATE_CART &&
              error.inventory.stack_constructor.selected &&
              error.inventory.stack_constructor.entered &&
              error.inventory.stack_constructor.bytecode == 11);
        CHECK(error.late_hook == MC_ENTITY_NBT_LATE_CART && error.inventory.stack_read.selected &&
              error.inventory.stack_read.entered &&
              error.inventory.stack_read.bytecode == stack_pcs[cut]);
        CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
        CHECK(after.fields.d == 1 && after.inventory_length == 27 &&
              (uintptr_t)after.fields.h != old_h);
        CHECK(mc_minecart_inventory_get(f.cart, 4, &held, &inventory) == MC_MINECART_INVENTORY_OK);
        CHECK(stack_is(held, 20, 3, 4));
        mc_item_stack_release(held);
        held = NULL;
        CHECK(mc_minecart_inventory_get(f.cart, 5, &held, &inventory) == MC_MINECART_INVENTORY_OK);
        CHECK(held == NULL);
        result = 0;
    cleanup:
        mc_item_stack_release(held);
        if (!close_fixture(&f))
            result = 1;
        if (result != 0)
            return result;
    }
    return 0;
}

static int list_controls(mc_entity_source source) {
    for (unsigned int cut = 0; cut < 5; ++cut) {
        int result = 1;
        fixture f = {0};
        record r;
        mc_item_stack *held = NULL;
        mc_entity_nbt_error error;
        mc_minecart_inventory_error inventory;
        mc_minecart_entity_snapshot after;
        ++cases;
        CHECK(open_fixture(&f, source, 1));
        record_init(&r, 1);
        entry(&r, 0, 0, 8, 9, 10);
        if (cut == 0)
            r.list_items[0] = NULL;
        else if (cut == 1)
            r.entries[0].type = MC_NBT_BYTE;
        else if (cut == 2)
            r.entry_fields[0][0].type = MC_NBT_SHORT;
        else if (cut == 3) {
            /* Byte -1 becomes255 and is ignored before its bad Stack id. */
            r.entry_fields[0][0].value.byte_value = -1;
            r.entry_fields[0][1].type = MC_NBT_BYTE;
        } else {
            /* Missing Slot defaults to0. Its Compound still contains the real
             * id/Count/Damage fields, rather than an expected Stack object. */
            for (size_t i = 0; i < 3; ++i)
                r.entry_items[0][i] = &r.entry_fields[0][i + 1u];
            r.entries[0].value.compound.length = 3;
        }
        const mc_entity_nbt_status status = mc_entity_read_nbt(f.cart, &r.root, &error);
        if (cut >= 3) {
            CHECK(status == MC_ENTITY_NBT_OK && fresh(&error));
        } else {
            CHECK(status == (cut == 0 ? MC_ENTITY_NBT_NULL_REFERENCE : MC_ENTITY_NBT_WRONG_TAG));
            CHECK(read_site(&error, source, cut == 1 ? 92 : 101));
            CHECK(error.index == 0);
            CHECK(error.late_hook == MC_ENTITY_NBT_LATE_CART && !error.inventory.slot_selected);
        }
        CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
        CHECK(after.inventory_length == 27);
        CHECK(mc_minecart_inventory_get(f.cart, 0, &held, &inventory) == MC_MINECART_INVENTORY_OK);
        if (cut == 4)
            CHECK(stack_is(held, 8, 9, 10));
        else
            CHECK(held == NULL);
        result = 0;
    cleanup:
        mc_item_stack_release(held);
        if (!close_fixture(&f))
            result = 1;
        if (result != 0)
            return result;
    }
    return 0;
}

static int borrowed_old_input(mc_entity_source source, bool later_failure) {
    int result = 1;
    fixture f = {0};
    record r;
    mc_item_stack *old = NULL, *held = NULL;
    mc_item_stack_error stack_error;
    mc_minecart_inventory_error inventory;
    mc_entity_nbt_error error;
    mc_minecart_entity_snapshot after;
    const mc_nbt_tag *borrowed = NULL;
    ++cases;
    CHECK(open_fixture(&f, source, 0));
    record_init(&r, 1);
    entry(&r, 0, 0, 41, 6, 9);
    entry(&r, 1, 1, 42, 7, 10);
    if (later_failure)
        r.entry_fields[1][3].type = MC_NBT_BYTE;
    CHECK(make_stack(&old, 3, 2, 1));
    CHECK(mc_item_stack_preserve_source(old, &r.root, NULL, &stack_error) == MC_ITEM_STACK_OK);
    CHECK(mc_minecart_inventory_set(f.cart, 35, old, &inventory) == MC_MINECART_INVENTORY_OK);
    borrowed = mc_item_stack_source(old);
    CHECK(borrowed != NULL);
    /* The only hold keeping this input tree alive now belongs to old h[35].
     * No copy of the input or extra test hold may mask a premature retirement. */
    mc_item_stack_release(old);
    old = NULL;
    const mc_entity_nbt_status status = mc_entity_read_nbt(f.cart, borrowed, &error);
    borrowed = NULL; /* Expired after return: never read the detached old owner. */
    if (later_failure) {
        CHECK(status == MC_ENTITY_NBT_STACK_FAILED);
        CHECK(read_site(&error, source, 137) && error.index == 1 && error.stack_bytecode == 24);
    } else {
        CHECK(status == MC_ENTITY_NBT_OK && fresh(&error));
    }
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
    CHECK(after.fields.d == 1 && after.inventory_length == 27);
    CHECK(mc_minecart_inventory_get(f.cart, 0, &held, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(stack_is(held, 41, 6, 9));
    mc_item_stack_release(held);
    held = NULL;
    CHECK(mc_minecart_inventory_get(f.cart, 1, &held, &inventory) == MC_MINECART_INVENTORY_OK);
    if (later_failure)
        CHECK(held == NULL);
    else
        CHECK(stack_is(held, 42, 7, 10));
    result = 0;
cleanup:
    mc_item_stack_release(held);
    mc_item_stack_release(old);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

static int array_allocation_prefix(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    record r;
    mc_entity_class_snapshot before, after;
    mc_minecart_entity_snapshot cart;
    mc_entity_nbt_error error;
    uintptr_t old_h = 0;
    ++cases;
    CHECK(open_fixture(&f, source, 2));
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &cart) == MC_ENTITY_OK);
    old_h = (uintptr_t)cart.fields.h;
    CHECK(mc_entity_classes_get_snapshot(f.classes, &before) == MC_ENTITY_OK);
    record_init(&r, 1);
    /* Restored native class-ceiling control, not Java heap/OOME evidence. The
     * getter and Type store run before the actual replacement request71. */
    const size_t saved_limit = f.classes->limits.max_allocation_bytes;
    f.classes->limits.max_allocation_bytes = before.allocation_bytes;
    const mc_entity_nbt_status status = mc_entity_read_nbt(f.cart, &r.root, &error);
    f.classes->limits.max_allocation_bytes = saved_limit;
    CHECK(status == MC_ENTITY_NBT_LIMIT);
    CHECK(read_site(&error, source, 71));
    CHECK(error.construction.status == MC_ENTITY_LIMIT && error.construction.bytecode == 71);
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &cart) == MC_ENTITY_OK);
    CHECK(cart.fields.d == 1 && cart.inventory_length == 36 && (uintptr_t)cart.fields.h == old_h);
    CHECK(mc_entity_classes_get_snapshot(f.classes, &after) == MC_ENTITY_OK);
    CHECK(after.allocation_bytes == before.allocation_bytes &&
          after.live_entities == before.live_entities);
    CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_OK && fresh(&error));
    CHECK(mc_minecart_entity_get_snapshot(f.cart, &cart) == MC_ENTITY_OK);
    CHECK(cart.inventory_length == 27);
    result = 0;
cleanup:
    if (!close_fixture(&f))
        result = 1;
    return result;
}

static int fresh_save_all_phases(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    record r;
    mc_item_stack *ready = NULL, *raw = NULL, *failed = NULL;
    mc_item_stack_error stack_error;
    mc_minecart_inventory_error inventory;
    mc_entity_nbt_error error;
    mc_nbt_tree *tree = NULL, *boolean_tree = NULL;
    const mc_nbt_tag *provenance = NULL;
    bool serialized = false;
    ++cases;
    CHECK(open_fixture(&f, source, 1));
    CHECK(make_stack(&ready, 65537, 257, -65537));
    record_init(&r, 1);
    CHECK(mc_item_stack_preserve_source(ready, &r.root, NULL, &stack_error) == MC_ITEM_STACK_OK);
    provenance = mc_item_stack_source(ready);
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK);
    entry(&r, 0, 2, -12, -9, 0);
    r.entry_fields[0][3].type = MC_NBT_BYTE;
    CHECK(mc_item_stack_allocate(&failed) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct_nbt(failed, &r.entries[0], &stack_error) ==
          MC_ITEM_STACK_INVALID_FIELD);
    CHECK(mc_minecart_inventory_set(f.cart, 0, ready, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_item_stack_count_set(ready, 257) == MC_ITEM_STACK_OK);
    CHECK(mc_minecart_inventory_set(f.cart, 1, raw, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_minecart_inventory_set(f.cart, 2, failed, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_entity_build_nbt(f.cart, NULL, &tree, &error) == MC_ENTITY_NBT_OK && fresh(&error));
    const mc_nbt_tag *root = mc_nbt_tree_root(tree), *items = field(root, "Items");
    CHECK(root != NULL && root->type == MC_NBT_COMPOUND && root->value.compound.length == 9 &&
          field(root, "id") == NULL);
    CHECK(items != NULL && items->type == MC_NBT_LIST && items->value.list.length == 3);
    const mc_nbt_tag *a = items->value.list.items[0], *b = items->value.list.items[1],
                     *c = items->value.list.items[2];
    CHECK(a != NULL && b != NULL && c != NULL && a->type == MC_NBT_COMPOUND &&
          b->type == MC_NBT_COMPOUND && c->type == MC_NBT_COMPOUND);
    CHECK(a->value.compound.length == 4 && b->value.compound.length == 4 &&
          c->value.compound.length == 4 && field(a, "Type") == NULL && field(a, "Items") == NULL &&
          field(a, "Pos") == NULL);
    CHECK(tag_is(a, "Slot", MC_NBT_BYTE) && tag_is(a, "id", MC_NBT_SHORT) &&
          tag_is(a, "Count", MC_NBT_BYTE) && tag_is(a, "Damage", MC_NBT_SHORT));
    CHECK(tag_is(b, "Slot", MC_NBT_BYTE) && tag_is(b, "id", MC_NBT_SHORT) &&
          tag_is(b, "Count", MC_NBT_BYTE) && tag_is(b, "Damage", MC_NBT_SHORT));
    CHECK(tag_is(c, "Slot", MC_NBT_BYTE) && tag_is(c, "id", MC_NBT_SHORT) &&
          tag_is(c, "Count", MC_NBT_BYTE) && tag_is(c, "Damage", MC_NBT_SHORT));
    CHECK(field(a, "Slot")->value.byte_value == 0 && field(a, "id")->value.short_value == 1 &&
          field(a, "Count")->value.byte_value == 1 && field(a, "Damage")->value.short_value == -1);
    CHECK(field(b, "Slot")->value.byte_value == 1 && field(b, "id")->value.short_value == 0 &&
          field(b, "Count")->value.byte_value == 0 && field(b, "Damage")->value.short_value == 0);
    CHECK(field(c, "Slot")->value.byte_value == 2 && field(c, "id")->value.short_value == -12 &&
          field(c, "Count")->value.byte_value == -9 && field(c, "Damage")->value.short_value == 0);
    CHECK(mc_item_stack_source(ready) == provenance);
    CHECK(stack_phase(raw, MC_ITEM_STACK_RAW) &&
          stack_phase(failed, MC_ITEM_STACK_FAILED_CONSTRUCTION));
    CHECK(mc_entity_save_nbt(f.cart, NULL, &serialized, &boolean_tree, &error) == MC_ENTITY_NBT_OK);
    CHECK(serialized && mc_nbt_tree_root(boolean_tree)->value.compound.length == 10);
    const mc_nbt_tag *id = field(mc_nbt_tree_root(boolean_tree), "id");
    static const uint16_t minecart[] = {'M', 'i', 'n', 'e', 'c', 'a', 'r', 't'};
    CHECK(id != NULL && id->type == MC_NBT_STRING && id->value.string.units != NULL &&
          id->value.string.length == COUNT(minecart) &&
          memcmp(id->value.string.units, minecart, sizeof(minecart)) == 0);
    mc_nbt_limits tiny = mc_nbt_default_limits();
    tiny.max_allocation_bytes = 0;
    mc_nbt_tree *unchanged = tree;
    CHECK(mc_entity_build_nbt(f.cart, &tiny, &unchanged, &error) == MC_ENTITY_NBT_LIMIT);
    CHECK(unchanged == tree && mc_item_stack_source(ready) == provenance);
    /* Genuine nullable source-field control, restored before checking status.
     * It reaches own length75 after base145/Type7, and cannot be replaced by a
     * native READY/provenance preflight. Independent output stays unchanged. */
    mc_minecart_stack_array *saved_h = f.cart->minecart->fields.h;
    f.cart->minecart->fields.h = NULL;
    const mc_entity_nbt_status null_h = mc_entity_build_nbt(f.cart, NULL, &unchanged, &error);
    f.cart->minecart->fields.h = saved_h;
    CHECK(null_h == MC_ENTITY_NBT_NULL_REFERENCE && write_site(&error, source, 75));
    CHECK(unchanged == tree && mc_item_stack_source(ready) == provenance);
    result = 0;
cleanup:
    mc_nbt_tree_free(boolean_tree);
    mc_nbt_tree_free(tree);
    mc_item_stack_release(ready);
    mc_item_stack_release(raw);
    mc_item_stack_release(failed);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

static int factory_and_base_frontiers(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    record r;
    mc_entity *first = NULL, *second = NULL;
    mc_item_stack *held = NULL;
    mc_entity_nbt_error error;
    mc_minecart_inventory_error inventory;
    mc_minecart_entity_snapshot cart;
    mc_entity_class_snapshot before, after;
    mc_entity_class_literal_view literal;
    ++cases;
    CHECK(open_fixture(&f, source, 0));
    record_init(&r, 1);
    r.root.value.compound.length = 13;
    entry(&r, 0, 26, 72, 5, 9);
    CHECK(mc_entity_factory_nbt(f.classes, NULL, &r.root, &first, &error) == MC_ENTITY_NBT_OK);
    CHECK(mc_entity_factory_nbt(f.classes, NULL, &r.root, &second, &error) == MC_ENTITY_NBT_OK);
    CHECK(first != NULL && second != NULL && first != second && first != f.cart &&
          second != f.cart);
    CHECK(mc_minecart_entity_get_snapshot(first, &cart) == MC_ENTITY_OK);
    CHECK(cart.inventory_length == 27 && cart.fields.d == 1);
    CHECK(mc_entity_class_literal_inspect(cart.class_literal, &literal) == MC_ENTITY_OK);
    CHECK(literal.owner == f.classes && literal.source == source &&
          literal.kind == MC_ENTITY_CLASS_KIND_MINECART);
    CHECK(mc_minecart_inventory_get(first, 26, &held, &inventory) == MC_MINECART_INVENTORY_OK);
    CHECK(stack_is(held, 72, 5, 9));
    mc_item_stack_release(held);
    held = NULL;
    CHECK(mc_entity_classes_get_snapshot(f.classes, &before) == MC_ENTITY_OK);
    r.entry_fields[0][2].type = MC_NBT_SHORT;
    mc_entity *unchanged = f.cart;
    CHECK(mc_entity_factory_nbt(f.classes, NULL, &r.root, &unchanged, &error) ==
          MC_ENTITY_NBT_STACK_FAILED);
    CHECK(unchanged == f.cart && error.factory_bytecode == 66 && read_site(&error, source, 137) &&
          error.stack_bytecode == 14);
    CHECK(mc_entity_classes_get_snapshot(f.classes, &after) == MC_ENTITY_OK);
    CHECK(after.live_entities == before.live_entities &&
          after.next_id == mc_java_i32_add(before.next_id, 1));
    /* Existing known-but-unsupported Boat remains distinct from unknown-id
     * normal NULL and never becomes a fake Minecart/Entity receiver. */
    r.fields[ID].value.string = ascii("Boat", r.id_value);
    CHECK(mc_entity_factory_nbt(f.classes, NULL, &r.root, &unchanged, &error) ==
          MC_ENTITY_NBT_UNSUPPORTED_TYPE);
    CHECK(unchanged == f.cart && error.factory_bytecode == 45);
    /* A base failure must occur before selecting the own Minecart hook. */
    record_init(&r, 1);
    r.fields[MOTION].value.list.length = 0;
    CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS);
    CHECK(error.late_hook == MC_ENTITY_NBT_LATE_NONE && error.base_bytecode == 25 &&
          !error.minecart_read.selected && !error.minecart_read.entered);
    result = 0;
cleanup:
    mc_item_stack_release(held);
    mc_entity_release(first);
    mc_entity_release(second);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

static int raw_cart_semantic_frontier(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    mc_entity *raw = NULL;
    mc_entity_snapshot before, after;
    mc_nbt_tree *tree = NULL;
    mc_entity_nbt_error error;
    record r;
    ++cases;
    CHECK(open_fixture(&f, source, 0));
    CHECK(mc_minecart_entity_allocate(f.classes, &raw, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(raw, &before) == MC_ENTITY_OK);
    CHECK(before.phase == MC_ENTITY_RAW);
    CHECK(mc_entity_build_nbt(raw, NULL, &tree, &error) == MC_ENTITY_NBT_OK && fresh(&error));
    const mc_nbt_tag *root = mc_nbt_tree_root(tree);
    CHECK(root != NULL && root->value.compound.length == 8 && tag_is(root, "Type", MC_NBT_INT) &&
          field(root, "Type")->value.int_value == 0 && field(root, "Items") == NULL);
    CHECK(mc_entity_get_snapshot(raw, &after) == MC_ENTITY_OK);
    CHECK(after.phase == before.phase && after.bounds == before.bounds &&
          after.random == before.random && after.watcher == before.watcher);
    record_init(&r, 1);
    CHECK(mc_entity_read_nbt(raw, &r.root, &error) == MC_ENTITY_NBT_NULL_REFERENCE);
    CHECK(error.late_hook == MC_ENTITY_NBT_LATE_NONE && error.base_bytecode == 294 &&
          !error.minecart_read.selected && !error.minecart_read.entered);
    CHECK(mc_entity_get_snapshot(raw, &after) == MC_ENTITY_OK);
    CHECK(after.phase == MC_ENTITY_RAW && after.base.x == 1.0 && after.base.y == 2.0 &&
          after.base.z == 3.0 && after.bounds == NULL);
    result = 0;
cleanup:
    mc_nbt_tree_free(tree);
    mc_entity_release(raw);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("Minecart NBT contracts require strict SSE arithmetic\n", stderr);
        return 1;
    }
    for (mc_entity_source source = MC_ENTITY_CLIENT; source <= MC_ENTITY_SERVER; ++source)
        if (conditional_types(source) || type2_failure_prefixes(source) ||
            missing_wrong_items_and_bounds(source) || duplicate_and_stack_failures(source) ||
            list_controls(source) || borrowed_old_input(source, false) ||
            borrowed_old_input(source, true) || array_allocation_prefix(source) ||
            fresh_save_all_phases(source) || factory_and_base_frontiers(source) ||
            raw_cart_semantic_frontier(source))
            return 1;
    if (cases != 40u) {
        fprintf(stderr, "unexpected source-case census: %u\n", cases);
        return 1;
    }
    printf("PASS: %u Minecart semantic NBT checks in %u source cases\n", checks, cases);
    return 0;
}
