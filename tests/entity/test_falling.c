/* Reuse actual constructor/profile inspection helpers. The old fixture main
 * and its mutation-control tests are compiled but never executed here. */
#define main mc_falling_constructor_fixture_main
#include "test_entity.c"
#undef main

#include "mc/entity/nbt/storage.h"
#include "mc/world/runtime/entities.h"

#include <stdlib.h>

/* Source-derived native contracts for ju/gl constructors and NBT. Scalar/tag
 * inputs below are explicit controls on actual owners. No phase, ID, Random,
 * class limit after creation, hold, ledger or membership is assigned. */
typedef struct ff_record {
    mc_nbt_tag root, fields[10], vectors[8];
    const mc_nbt_tag *items[10], *pos[3], *motion[3], *rotation[2];
    uint16_t names[10][16], class_name[16], root_name[3];
} ff_record;

typedef struct ff_guard {
    mc_entity_snapshot entity;
    mc_entity_bounds bounds;
    mc_entity_class_snapshot classes;
    mc_entity_watcher_snapshot watcher;
    mc_java_random random;
    uint64_t unique;
    size_t holds, borrowers;
} ff_guard;

static mc_nbt_string ff_ascii(const char *text, uint16_t *units)
{
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, length};
}

static bool ff_text(mc_nbt_string text, const char *expected)
{
    if (text.length != strlen(expected) || (text.length != 0 && text.units == NULL))
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != (unsigned char)expected[i])
            return false;
    return true;
}

static const mc_nbt_tag *ff_field(const mc_nbt_tag *root, const char *name)
{
    uint16_t units[16];
    const mc_nbt_string key = ff_ascii(name, units);
    return mc_nbt_compound_get(root, key.units, key.length);
}

static bool ff_class_same(const mc_entity_class_snapshot *a, const mc_entity_class_snapshot *b)
{
    return a->source == b->source && a->entity_class == b->entity_class &&
           a->bounds_class == b->bounds_class && a->watcher_class == b->watcher_class &&
           a->next_id == b->next_id && a->bounds_pool_cursor == b->bounds_pool_cursor &&
           a->bounds_pool_identity == b->bounds_pool_identity &&
           a->watcher_types_identity == b->watcher_types_identity &&
           a->watcher_type_count == b->watcher_type_count && a->live_entities == b->live_entities &&
           a->allocation_bytes == b->allocation_bytes;
}

static bool ff_capture(mc_entity *entity, ff_guard *g)
{
    if (mc_entity_get_snapshot(entity, &g->entity) != MC_ENTITY_OK || g->entity.bounds == NULL ||
        g->entity.random == NULL ||
        mc_entity_classes_get_snapshot(entity->classes, &g->classes) != MC_ENTITY_OK ||
        mc_entity_watcher_get_snapshot(g->entity.watcher, &g->watcher) != MC_ENTITY_OK)
        return false;
    g->bounds = *g->entity.bounds;
    g->random = *g->entity.random;
    g->unique = uniquifier(entity->classes->profile);
    g->holds = entity->holds;
    g->borrowers = entity->classes->borrowers;
    return holder_is(entity->classes->profile, MC_JAVA_RANDOM_HOLDER_COLD) &&
           entity->classes->profile->math == NULL;
}

static bool ff_unchanged(mc_entity *entity, const ff_guard *g)
{
    ff_guard now;
    return ff_capture(entity, &now) && base_equal(&now.entity.base, &g->entity.base) &&
           now.entity.source == g->entity.source &&
           now.entity.class_literal == g->entity.class_literal &&
           now.entity.phase == g->entity.phase &&
           now.entity.base_bytecode == g->entity.base_bytecode &&
           now.entity.item_bytecode == g->entity.item_bytecode &&
           now.entity.falling_bytecode == g->entity.falling_bytecode &&
           now.entity.falling.a == g->entity.falling.a &&
           now.entity.falling.b == g->entity.falling.b && now.entity.stack == NULL &&
           now.entity.item.b == 0 && now.entity.item.e == 0 && now.entity.item.c == 0 &&
           now.entity.item.f == 0 && float_bits(now.entity.item.d) == 0 &&
           now.entity.bounds == g->entity.bounds && now.entity.random == g->entity.random &&
           now.entity.watcher == g->entity.watcher && bounds_equal(&now.bounds, &g->bounds) &&
           mc_test_java_random_equal(&now.random, &g->random) &&
           ff_class_same(&now.classes, &g->classes) && now.watcher.dirty == g->watcher.dirty &&
           now.watcher.size == g->watcher.size && now.watcher.record == g->watcher.record &&
           now.watcher.index == g->watcher.index && now.watcher.type == g->watcher.type &&
           now.watcher.byte_value == g->watcher.byte_value &&
           now.watcher.record_dirty == g->watcher.record_dirty && now.unique == g->unique &&
           now.holds == g->holds && now.borrowers == g->borrowers;
}

static bool ff_nbt_fresh(const mc_entity_nbt_error *e)
{
    return e->status == MC_ENTITY_NBT_OK && e->operation == NULL && e->field == NULL &&
           e->index == SIZE_MAX && e->wrapper_bytecode == 0 && e->name_bytecode == 0 &&
           e->factory_bytecode == 0 && e->base_bytecode == 0 && e->item_bytecode == 0 &&
           e->falling_bytecode == 0 && e->stack_bytecode == 0 && e->initializer_bytecode == 0 &&
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

static mc_entity_nbt_error ff_poison(void)
{
    return (mc_entity_nbt_error){
        .status = MC_ENTITY_NBT_UNSUPPORTED_TYPE,
        .operation = "old",
        .field = "old",
        .index = 99,
        .wrapper_bytecode = 91,
        .name_bytecode = 92,
        .factory_bytecode = 93,
        .base_bytecode = 94,
        .item_bytecode = 95,
        .falling_bytecode = 96,
        .stack_bytecode = 97,
        .initializer_bytecode = 98,
        .registration_bytecode = 99,
        .construction = {.status = MC_ENTITY_LIMIT,
                         .operation = "old",
                         .bytecode = 90,
                         .profile = MC_JAVA_RANDOM_PROFILE_LIMIT,
                         .item = MC_ITEM_STACK_LIMIT,
                         .world = MC_WORLD_STORAGE_LIMIT},
        .stack = {.status = MC_ITEM_STACK_LIMIT, .field = "old", .nbt = {MC_NBT_LIMIT, 88}},
        .nbt = {MC_NBT_LIMIT, 87},
        .registry = {.status = MC_ENTITY_REGISTRY_BUSY,
                     .cause = MC_ENTITY_REGISTRY_LIMIT,
                     .operation = "old",
                     .initializer_bytecode = 86,
                     .registration_bytecode = 85,
                     .class_cause = MC_ENTITY_LIMIT}};
}

static size_t ff_direct_bytes(void)
{
    return sizeof(mc_entity) + sizeof(mc_entity_bounds) + sizeof(mc_java_random) +
           sizeof(mc_entity_watcher) + sizeof(mc_entity_watch_map) +
           sizeof(mc_entity_watch_record) + sizeof(mc_entity_watch_node);
}

static size_t ff_static_bytes(void)
{
    return sizeof(mc_entity_classes) + sizeof(mc_entity_bounds_pool) + sizeof(mc_entity_type_map) +
           7u * sizeof(mc_entity_type_entry);
}

static mc_entity_bounds ff_box(double x, double y, double z, bool full)
{
    /* Source-derived constructor bounds, not a movement/voxel oracle. */
    const double half = full ? (double)0.49f : (double)0.3f;
    const double minimum = full ? y - (double)0.49f : y;
    return (mc_entity_bounds){
        x - half, minimum, z - half, x + half, minimum + (full ? (double)0.98f : (double)1.8f),
        z + half};
}

static bool ff_ready(const mc_entity_snapshot *v, mc_entity_source source, int32_t instance,
                     int32_t carried, bool full, double x, double y, double z)
{
    mc_entity_base_fields expected = base_literals(instance);
    if (full) {
        expected.aF = true;
        expected.width = expected.height = 0.98f;
        expected.offset = 0.49f;
        expected.x = expected.aJ = x;
        expected.y = expected.aK = y;
        expected.z = expected.aL = z;
    }
    const mc_entity_bounds bounds = ff_box(x, y, z, full);
    return v->source == source && v->phase == MC_ENTITY_READY && v->base_bytecode == 231 &&
           v->item_bytecode == 0 && v->falling_bytecode == (full ? 80 : 10) &&
           base_equal(&v->base, &expected) && v->falling.a == carried && v->falling.b == 0 &&
           v->item.b == 0 && v->item.e == 0 && v->item.c == 0 && v->item.f == 0 &&
           float_bits(v->item.d) == 0 && v->stack == NULL && v->bounds != NULL &&
           bounds_equal(v->bounds, &bounds) && fresh_random(v->random) && watcher_is(v, true);
}

static int ff_constructors(mc_entity_source source)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entities[4] = {NULL}, *held = NULL, *item = NULL;
    mc_item_stack *stack_sentinel = NULL;
    mc_entity_snapshot values[4] = {{0}}, raw = {0};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = {0};
    const mc_entity_class_literal *type = NULL, *item_type = NULL;
    const int32_t carried[] = {0, INT32_MIN, INT32_MAX, -257};
    const double positions[4][3] = {
        {0, 0, 0}, {-1.25, -2.5, 4.75}, {-16777216.0, 1.5, 16777216.0}, {0.5, -0.25, -0.5}};
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_item_stack_allocate(&stack_sentinel) == MC_ITEM_STACK_OK);
    const uint64_t unique = uniquifier(profile);
    CHECK(mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_FALLING_SAND, &type) ==
              MC_ENTITY_OK &&
          mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_ITEM, &item_type) ==
              MC_ENTITY_OK &&
          type != item_type);
    mc_entity_class_literal_view literal = {0};
    CHECK(mc_entity_class_literal_inspect(type, &literal) == MC_ENTITY_OK &&
          literal.owner == classes && literal.source == source &&
          literal.kind == MC_ENTITY_CLASS_KIND_FALLING_SAND &&
          strcmp(literal.client_name, "ju") == 0 && strcmp(literal.server_name, "gl") == 0);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_falling_entity_allocate(classes, &entities[i], &error) == MC_ENTITY_OK &&
              mc_entity_get_snapshot(entities[i], &raw) == MC_ENTITY_OK && raw_is(&raw, source) &&
              raw.class_literal == type && raw.falling.a == 0 && raw.falling.b == 0 &&
              raw.falling_bytecode == 0 &&
              mc_entity_is_kind(entities[i], MC_ENTITY_CLASS_KIND_FALLING_SAND) &&
              !mc_entity_is_kind(entities[i], MC_ENTITY_CLASS_KIND_ITEM));
        CHECK(mc_item_entity_construct_world(entities[i], NULL, &error) ==
                  MC_ENTITY_INVALID_ARGUMENT &&
              mc_entity_get_snapshot(entities[i], &raw) == MC_ENTITY_OK && raw_is(&raw, source) &&
              raw.class_literal == type && raw.falling_bytecode == 0 &&
              uniquifier(profile) == advanced(unique, (unsigned int)i));
        CHECK((i == 0 ? mc_falling_entity_construct_world(entities[i], NULL, &error)
                      : mc_falling_entity_construct(entities[i], NULL, positions[i][0],
                                                    positions[i][1], positions[i][2], carried[i],
                                                    &error)) == MC_ENTITY_OK);
        CHECK(error.status == MC_ENTITY_OK && error.operation == NULL && error.bytecode == 0 &&
              error.profile == MC_JAVA_RANDOM_PROFILE_OK && error.item == MC_ITEM_STACK_OK &&
              error.world == MC_WORLD_STORAGE_OK);
        CHECK(mc_entity_get_snapshot(entities[i], &values[i]) == MC_ENTITY_OK &&
              values[i].class_literal == type &&
              ff_ready(&values[i], source, (int32_t)i, carried[i], i != 0, positions[i][0],
                       positions[i][1], positions[i][2]));
        CHECK(uniquifier(profile) == advanced(unique, (unsigned int)i + 1u) &&
              holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL &&
              mc_entity_native_bytes(entities[i]) == ff_direct_bytes());
        /* Real independent caller Stack is only an output sentinel; it is
         * never attached to or required by this FallingSand. */
        mc_item_stack *stack = stack_sentinel;
        CHECK(mc_item_entity_stack_get(entities[i], &stack) == MC_ENTITY_INVALID_ARGUMENT &&
              stack == stack_sentinel &&
              mc_entity_item_pickup_delay_set(entities[i], 9) == MC_ENTITY_INVALID_ARGUMENT);
        CHECK(mc_falling_entity_construct_world(entities[i], NULL, &error) ==
                  MC_ENTITY_INVALID_PHASE &&
              mc_entity_get_snapshot(entities[i], &raw) == MC_ENTITY_OK &&
              base_equal(&raw.base, &values[i].base) && raw.falling.a == carried[i] &&
              raw.falling.b == 0 && uniquifier(profile) == advanced(unique, (unsigned int)i + 1u));
        for (size_t j = 0; j < i; ++j)
            CHECK(values[j].bounds != values[i].bounds && values[j].random != values[i].random &&
                  values[j].watcher != values[i].watcher &&
                  !mc_entity_equals(entities[j], entities[i]));
    }
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 4 &&
          state.live_entities == 4 &&
          state.allocation_bytes == ff_static_bytes() + 4u * ff_direct_bytes() &&
          state.bounds_class == MC_ENTITY_CLASS_READY &&
          state.watcher_class == MC_ENTITY_CLASS_READY && state.watcher_type_count == 7 &&
          state.bounds_pool_cursor == 0);
    CHECK(mc_entity_retain(entities[1]) == MC_ENTITY_OK);
    held = entities[1];
    CHECK(mc_entity_equals(held, entities[1]) && mc_entity_hash_code(held) == 1 &&
          mc_entity_classes_free(classes) == MC_ENTITY_BUSY);
    mc_entity_release(entities[1]);
    entities[1] = NULL;
    CHECK(mc_entity_get_snapshot(held, &raw) == MC_ENTITY_OK && raw.class_literal == type &&
          raw.falling.a == INT32_MIN);
    CHECK(mc_item_entity_allocate(classes, &item, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(item, NULL, &error) == MC_ENTITY_INVALID_ARGUMENT &&
          mc_entity_get_snapshot(item, &raw) == MC_ENTITY_OK && raw_is(&raw, source) &&
          raw.class_literal == item_type && uniquifier(profile) == advanced(unique, 4));
    result = 0;
cleanup:
    mc_item_stack_release(stack_sentinel);
    mc_entity_release(item);
    mc_entity_release(held);
    for (size_t i = 0; i < 4; ++i)
        mc_entity_release(entities[i]);
    if (classes != NULL) {
        mc_entity_class_snapshot after = {0};
        if (mc_entity_classes_get_snapshot(classes, &after) != MC_ENTITY_OK ||
            after.live_entities != 0 ||
            (result == 0 && after.allocation_bytes != ff_static_bytes()))
            result = 1;
    }
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int ff_limits(mc_entity_source source)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL, *later = NULL;
    mc_entity_snapshot view = {0};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = {0};
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const uint64_t unique = uniquifier(profile);
    const mc_entity_limits none = {SIZE_MAX, 0};
    CHECK(mc_entity_classes_create(source, profile, &none, &classes) == MC_ENTITY_OK &&
          mc_falling_entity_allocate(classes, &entity, &error) == MC_ENTITY_LIMIT &&
          entity == NULL);
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
          state.live_entities == 0 && state.next_id == 0 &&
          state.entity_class == MC_ENTITY_CLASS_READY &&
          state.allocation_bytes == sizeof(mc_entity_classes) && uniquifier(profile) == unique);
    CHECK(retire_classes(&classes) == 0);
    const mc_entity_limits exact_raw = {sizeof(mc_entity_classes) + sizeof(mc_entity), 1};
    CHECK(mc_entity_classes_create(source, profile, &exact_raw, &classes) == MC_ENTITY_OK &&
          mc_falling_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK);
    mc_entity *output = entity; /* Borrowed sentinel, not a second native hold. */
    CHECK(mc_falling_entity_allocate(classes, &output, &error) == MC_ENTITY_LIMIT &&
          output == entity);
    CHECK(mc_falling_entity_construct(entity, NULL, 1, 2, 3, INT32_MAX, &error) ==
              MC_ENTITY_LIMIT &&
          error.bytecode == 0 && strcmp(error.operation, "bounds class initialization") == 0);
    CHECK(mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK &&
          view.phase == MC_ENTITY_FAILED_CONSTRUCTION && view.base_bytecode == 24 &&
          view.falling_bytecode == 0 && view.item_bytecode == 0 && view.base.id == 0 &&
          view.base.aE == 1.0 && view.falling.a == 0 && view.falling.b == 0 &&
          view.bounds == NULL && view.random == NULL && view.watcher == NULL && view.stack == NULL);
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 1 &&
          state.bounds_class == MC_ENTITY_CLASS_FAILED &&
          state.watcher_class == MC_ENTITY_CLASS_COLD &&
          state.allocation_bytes == exact_raw.max_allocation_bytes &&
          uniquifier(profile) == unique);
    mc_entity_release(entity);
    entity = NULL;
    CHECK(mc_falling_entity_allocate(classes, &later, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(later, NULL, &error) == MC_ENTITY_CLASS_FAILED_STATUS &&
          error.bytecode == 0 && strcmp(error.operation, "bounds class initialization") == 0);
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 2 &&
          state.bounds_class == MC_ENTITY_CLASS_FAILED && state.live_entities == 1 &&
          uniquifier(profile) == unique);
    mc_entity_release(later);
    later = NULL;
    CHECK(retire_classes(&classes) == 0);
    /* Immutable creation budget reaches the real default Random, then fails
     * cold watcher-class allocation. No budget or latched phase is repaired. */
    const mc_entity_limits late = {sizeof(mc_entity_classes) + sizeof(mc_entity) +
                                       sizeof(mc_entity_bounds_pool) + sizeof(mc_entity_bounds) +
                                       sizeof(mc_java_random),
                                   1};
    CHECK(mc_entity_classes_create(source, profile, &late, &classes) == MC_ENTITY_OK &&
          mc_falling_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(entity, NULL, &error) == MC_ENTITY_LIMIT &&
          error.bytecode == 0 && strcmp(error.operation, "watcher class initialization") == 0);
    CHECK(mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK &&
          view.phase == MC_ENTITY_FAILED_CONSTRUCTION && view.base_bytecode == 179 &&
          view.falling_bytecode == 0 && view.item_bytecode == 0 && view.bounds != NULL &&
          fresh_random(view.random) && view.watcher == NULL && view.stack == NULL &&
          view.falling.a == 0 && view.falling.b == 0);
    const mc_entity_base_fields expected = base_literals(0);
    CHECK(base_equal(&view.base, &expected) &&
          mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 1 &&
          state.bounds_class == MC_ENTITY_CLASS_READY &&
          state.watcher_class == MC_ENTITY_CLASS_FAILED &&
          state.allocation_bytes == late.max_allocation_bytes &&
          uniquifier(profile) == advanced(unique, 1) &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
    result = 0;
cleanup:
    mc_entity_release(entity);
    mc_entity_release(later);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static void ff_record_init(ff_record *r)
{
    const char *const names[] = {"Extra",    "Tile",         "id",   "Pos", "Motion",
                                 "Rotation", "FallDistance", "Fire", "Air", "OnGround"};
    const mc_nbt_type types[] = {MC_NBT_DOUBLE, MC_NBT_BYTE, MC_NBT_STRING, MC_NBT_LIST,
                                 MC_NBT_LIST,   MC_NBT_LIST, MC_NBT_FLOAT,  MC_NBT_SHORT,
                                 MC_NBT_SHORT,  MC_NBT_BYTE};
    const double values[] = {-8.25, -16.5, 32.75, 11.0, -0.0, -10.0};
    memset(r, 0, sizeof(*r));
    r->root.type = MC_NBT_COMPOUND;
    r->root_name[0] = 'F';
    r->root_name[1] = 0;
    r->root_name[2] = 0xd800;
    r->root.name = (mc_nbt_string){r->root_name, 3};
    r->root.value.compound.items = r->items;
    r->root.value.compound.length = 10;
    for (size_t i = 0; i < 10; ++i) {
        r->fields[i].name = ff_ascii(names[i], r->names[i]);
        r->fields[i].type = types[i];
        r->items[i] = &r->fields[i];
    }
    r->fields[0].value.double_bits = UINT64_C(0x7ff0000000000042);
    r->fields[1].value.byte_value = -1;
    r->fields[2].value.string = ff_ascii("FallingSand", r->class_name);
    for (size_t i = 0; i < 6; ++i) {
        r->vectors[i].type = MC_NBT_DOUBLE;
        r->vectors[i].value.double_bits = double_bits(values[i]);
    }
    r->vectors[6].type = r->vectors[7].type = MC_NBT_FLOAT;
    r->vectors[6].value.float_bits = float_bits(725.0f);
    r->vectors[7].value.float_bits = float_bits(-725.0f);
    for (size_t i = 0; i < 3; ++i) {
        r->pos[i] = &r->vectors[i];
        r->motion[i] = &r->vectors[i + 3];
    }
    r->rotation[0] = &r->vectors[6];
    r->rotation[1] = &r->vectors[7];
    for (size_t i = 3; i <= 5; ++i) {
        r->fields[i].value.list.element_type = i == 5 ? MC_NBT_FLOAT : MC_NBT_DOUBLE;
        r->fields[i].value.list.length = i == 5 ? 2u : 3u;
    }
    r->fields[3].value.list.items = r->pos;
    r->fields[4].value.list.items = r->motion;
    r->fields[5].value.list.items = r->rotation;
    r->fields[6].value.float_bits = float_bits(3.5f);
    r->fields[7].value.short_value = -19;
    r->fields[8].value.short_value = 298;
    r->fields[9].value.byte_value = -2;
}

static bool ff_read_values(const mc_entity_snapshot *v, int32_t carried, int32_t age)
{
    return v->falling.a == carried && v->falling.b == age && v->stack == NULL && v->item.b == 0 &&
           v->item.f == 0 && v->item.e == 0 && v->item.c == 0 && float_bits(v->item.d) == 0 &&
           v->base.x == -8.25 && v->base.y == -16.5 && v->base.z == 32.75 && v->base.aJ == -8.25 &&
           v->base.aK == -16.5 && v->base.aL == 32.75 && v->base.bl == -8.25 &&
           v->base.bm == -16.5 && v->base.bn == 32.75 && double_bits(v->base.velocity_x) == 0 &&
           double_bits(v->base.velocity_y) == UINT64_C(0x8000000000000000) &&
           v->base.velocity_z == -10.0 && float_bits(v->base.yaw) == float_bits(5.0f) &&
           float_bits(v->base.aT) == float_bits(-5.0f) && v->base.aU == 725.0f &&
           v->base.aV == -725.0f && v->base.bk == 3.5f && v->base.bv == -19 && v->base.bz == 298 &&
           v->base.aX;
}

static bool ff_known(const mc_nbt_tag *root, bool named, int8_t tile)
{
    static const char *const names[] = {"Pos",  "Motion", "Rotation", "FallDistance",
                                        "Fire", "Air",    "OnGround", "Tile"};
    if (root == NULL || root->type != MC_NBT_COMPOUND ||
        root->value.compound.length != (named ? 9u : 8u))
        return false;
    size_t offset = 0;
    if (named) {
        const mc_nbt_tag *id = root->value.compound.items[0];
        if (id->type != MC_NBT_STRING || !ff_text(id->name, "id") ||
            !ff_text(id->value.string, "FallingSand"))
            return false;
        offset = 1;
    }
    for (size_t i = 0; i < 8; ++i)
        if (!ff_text(root->value.compound.items[i + offset]->name, names[i]))
            return false;
    const mc_nbt_tag *value = ff_field(root, "Tile");
    return value != NULL && value->type == MC_NBT_BYTE && value->value.byte_value == tile &&
           ff_field(root, "Health") == NULL && ff_field(root, "Age") == NULL &&
           ff_field(root, "Item") == NULL && ff_field(root, "Extra") == NULL;
}

static bool ff_saved_constructor(const mc_nbt_tag *root)
{
    const mc_nbt_tag *pos = ff_field(root, "Pos"), *motion = ff_field(root, "Motion");
    const mc_nbt_tag *rotation = ff_field(root, "Rotation");
    const double expected[] = {-1.25, -2.5, 4.75};
    if (pos == NULL || motion == NULL || rotation == NULL || pos->type != MC_NBT_LIST ||
        motion->type != MC_NBT_LIST || rotation->type != MC_NBT_LIST ||
        pos->value.list.element_type != MC_NBT_DOUBLE ||
        motion->value.list.element_type != MC_NBT_DOUBLE ||
        rotation->value.list.element_type != MC_NBT_FLOAT || pos->value.list.length != 3 ||
        motion->value.list.length != 3 || rotation->value.list.length != 2)
        return false;
    for (size_t i = 0; i < 3; ++i)
        if (pos->value.list.items[i]->type != MC_NBT_DOUBLE ||
            pos->value.list.items[i]->value.double_bits != double_bits(expected[i]) ||
            motion->value.list.items[i]->type != MC_NBT_DOUBLE ||
            motion->value.list.items[i]->value.double_bits != 0)
            return false;
    for (size_t i = 0; i < 2; ++i)
        if (rotation->value.list.items[i]->type != MC_NBT_FLOAT ||
            rotation->value.list.items[i]->value.float_bits != 0)
            return false;
    const mc_nbt_tag *fall = ff_field(root, "FallDistance"), *fire = ff_field(root, "Fire");
    const mc_nbt_tag *air = ff_field(root, "Air"), *ground = ff_field(root, "OnGround");
    return fall != NULL && fall->type == MC_NBT_FLOAT && fall->value.float_bits == 0 &&
           fire != NULL && fire->type == MC_NBT_SHORT && fire->value.short_value == 0 &&
           air != NULL && air->type == MC_NBT_SHORT && air->value.short_value == 300 &&
           ground != NULL && ground->type == MC_NBT_BYTE && ground->value.byte_value == 0;
}

static int ff_nbt(mc_entity_source source)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL, *loaded[2] = {NULL};
    mc_nbt_tree *leaf = NULL, *named = NULL, *extra = NULL, *saved = NULL;
    mc_entity_nbt_error error = {0};
    mc_entity_snapshot view = {0};
    ff_record record;
    ff_guard guard;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK &&
          mc_falling_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct(entity, NULL, -1.25, -2.5, 4.75, INT32_MAX, NULL) ==
              MC_ENTITY_OK);
    /* Real source age is a legitimate prepared scalar. It must never become
     * Item Age or be silently reset by semantic read on this existing owner. */
    entity->falling.b = 77;
    CHECK(ff_capture(entity, &guard));
    error = ff_poison();
    CHECK(mc_entity_build_nbt(entity, NULL, &leaf, &error) == MC_ENTITY_NBT_OK &&
          ff_nbt_fresh(&error) && ff_unchanged(entity, &guard) &&
          ff_known(mc_nbt_tree_root(leaf), false, -1) &&
          ff_saved_constructor(mc_nbt_tree_root(leaf)));
    mc_nbt_limits limit = mc_nbt_default_limits();
    limit.max_tags = 16;     /* Seven fields+Tile+eight vector scalars+root =17. */
    mc_nbt_tree *out = leaf; /* Borrowed sentinel. */
    error = ff_poison();
    CHECK(mc_entity_build_nbt(entity, &limit, &out, &error) == MC_ENTITY_NBT_LIMIT && out == leaf &&
          error.base_bytecode == 148 && error.falling_bytecode == 11 && error.item_bytecode == 0 &&
          error.stack_bytecode == 0 && error.nbt.status == MC_NBT_LIMIT &&
          ff_unchanged(entity, &guard));
    limit = mc_nbt_default_limits();
    limit.max_tags = 17;
    limit.max_elements = 8;
    limit.max_depth = 3;
    CHECK(mc_entity_build_nbt(entity, &limit, &extra, &error) == MC_ENTITY_NBT_OK &&
          extra != leaf && ff_nbt_fresh(&error) && ff_known(mc_nbt_tree_root(extra), false, -1));
    mc_nbt_tree_free(extra);
    extra = NULL;
    bool serialized = false;
    CHECK(mc_entity_save_nbt(entity, NULL, &serialized, &named, &error) == MC_ENTITY_NBT_OK &&
          serialized && ff_nbt_fresh(&error) && ff_known(mc_nbt_tree_root(named), true, -1) &&
          ff_saved_constructor(mc_nbt_tree_root(named)));
    /* Name active use legitimately allocates the actual namespace. Re-capture
     * after that effect; later saves/reads do not get a false unchanged oracle. */
    CHECK(ff_capture(entity, &guard));
    limit = mc_nbt_default_limits();
    limit.max_tags = 17;
    out = named;
    serialized = false;
    CHECK(mc_entity_save_nbt(entity, &limit, &serialized, &out, &error) == MC_ENTITY_NBT_LIMIT &&
          !serialized && out == named && error.wrapper_bytecode == 27 && error.name_bytecode == 7 &&
          error.nbt.status == MC_NBT_LIMIT && ff_unchanged(entity, &guard));
    const size_t leaf_bytes = mc_nbt_tree_allocation_bytes(leaf),
                 named_bytes = mc_nbt_tree_allocation_bytes(named);
    CHECK(leaf_bytes <= SIZE_MAX - named_bytes);
    limit = mc_nbt_default_limits();
    limit.max_allocation_bytes = leaf_bytes + named_bytes - 1u;
    out = named;
    serialized = false;
    CHECK(mc_entity_save_nbt(entity, &limit, &serialized, &out, &error) == MC_ENTITY_NBT_LIMIT &&
          !serialized && out == named && ff_unchanged(entity, &guard));
    limit = mc_nbt_default_limits();
    limit.max_tags = 18;
    limit.max_elements = 9;
    limit.max_depth = 3;
    limit.max_allocation_bytes = leaf_bytes + named_bytes;
    CHECK(mc_entity_save_nbt(entity, &limit, &serialized, &extra, &error) == MC_ENTITY_NBT_OK &&
          serialized && ff_nbt_fresh(&error) && ff_known(mc_nbt_tree_root(extra), true, -1) &&
          ff_unchanged(entity, &guard));
    mc_nbt_tree_free(extra);
    extra = NULL;
    ff_record_init(&record);
    /* Wrong reached Byte retains all completed base stores and old a/b. */
    record.fields[1].type = MC_NBT_SHORT;
    error = ff_poison();
    CHECK(mc_entity_read_nbt(entity, &record.root, &error) == MC_ENTITY_NBT_WRONG_TAG &&
          error.base_bytecode == 311 && error.falling_bytecode == 4 && error.item_bytecode == 0 &&
          error.stack_bytecode == 0 && error.field != NULL && strcmp(error.field, "Tile") == 0);
    CHECK(mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK &&
          ff_read_values(&view, INT32_MAX, 77) && view.bounds == guard.entity.bounds &&
          view.random == guard.entity.random && view.watcher == guard.entity.watcher &&
          mc_test_java_random_equal(view.random, &guard.random) &&
          uniquifier(profile) == guard.unique && view.falling_bytecode == 80);
    const mc_entity_bounds read_box = ff_box(-8.25, -16.5, 32.75, true);
    CHECK(bounds_equal(view.bounds, &read_box));
    record.fields[1].type = MC_NBT_BYTE;
    const int8_t bytes[] = {INT8_MIN, -1, 0, INT8_MAX};
    const int32_t expected[] = {128, 255, 0, 127};
    for (size_t i = 0; i < 4; ++i) {
        record.fields[1].value.byte_value = bytes[i];
        CHECK(mc_entity_read_nbt(entity, &record.root, &error) == MC_ENTITY_NBT_OK &&
              ff_nbt_fresh(&error) && mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK &&
              ff_read_values(&view, expected[i], 77));
    }
    const mc_nbt_tag *without_tile[9];
    size_t used = 0;
    for (size_t i = 0; i < 10; ++i)
        if (i != 1)
            without_tile[used++] = record.items[i];
    mc_nbt_tag missing = record.root;
    missing.value.compound.items = without_tile;
    missing.value.compound.length = used;
    CHECK(mc_entity_read_nbt(entity, &missing, &error) == MC_ENTITY_NBT_OK &&
          ff_nbt_fresh(&error) && mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK &&
          ff_read_values(&view, 0, 77));
    record.fields[1].value.byte_value = -1;
    const uint64_t unique = uniquifier(profile);
    mc_entity_class_snapshot before = {0}, after = {0};
    CHECK(mc_entity_classes_get_snapshot(classes, &before) == MC_ENTITY_OK);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_entity_factory_nbt(classes, NULL, &record.root, &loaded[i], &error) ==
                  MC_ENTITY_NBT_OK &&
              loaded[i] != NULL && ff_nbt_fresh(&error) &&
              mc_entity_get_snapshot(loaded[i], &view) == MC_ENTITY_OK &&
              ff_read_values(&view, 255, 0) && view.base.id == before.next_id + (int32_t)i &&
              view.base.width == 0.6f && view.base.height == 1.8f &&
              float_bits(view.base.offset) == 0 && !view.base.aF && view.base_bytecode == 231 &&
              view.falling_bytecode == 10 && view.item_bytecode == 0 &&
              view.class_literal == guard.entity.class_literal &&
              loaded[i]->storage_source == NULL &&
              uniquifier(profile) == advanced(unique, (unsigned int)i + 1u) &&
              holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
        const mc_entity_bounds box = ff_box(-8.25, -16.5, 32.75, false);
        CHECK(bounds_equal(view.bounds, &box) && !mc_entity_equals(entity, loaded[i]));
    }
    CHECK(loaded[0] != loaded[1] && loaded[0]->bounds != loaded[1]->bounds &&
          loaded[0]->random != loaded[1]->random && loaded[0]->watcher != loaded[1]->watcher &&
          !mc_entity_equals(loaded[0], loaded[1]));
    CHECK(mc_entity_classes_get_snapshot(classes, &before) == MC_ENTITY_OK);
    record.fields[1].type = MC_NBT_SHORT;
    mc_entity *candidate = loaded[0];
    CHECK(mc_entity_factory_nbt(classes, NULL, &record.root, &candidate, &error) ==
              MC_ENTITY_NBT_WRONG_TAG &&
          candidate == loaded[0] && error.factory_bytecode == 66 && error.base_bytecode == 311 &&
          error.falling_bytecode == 4 && error.item_bytecode == 0 && error.field != NULL &&
          strcmp(error.field, "Tile") == 0);
    CHECK(mc_entity_classes_get_snapshot(classes, &after) == MC_ENTITY_OK &&
          after.next_id == before.next_id + 1 && after.live_entities == before.live_entities &&
          after.allocation_bytes == before.allocation_bytes &&
          uniquifier(profile) == advanced(unique, 3));
    record.fields[1].type = MC_NBT_BYTE;
    CHECK(mc_entity_preserve_source(loaded[0], &record.root, NULL, &error) == MC_ENTITY_NBT_OK &&
          ff_nbt_fresh(&error));
    const mc_nbt_tag *source_root = mc_entity_storage_source(loaded[0]);
    CHECK(source_root != NULL && source_root != &record.root &&
          source_root->value.compound.length == 10 && source_root->name.length == 3 &&
          source_root->name.units[0] == 'F' && source_root->name.units[1] == 0 &&
          source_root->name.units[2] == 0xd800 &&
          ff_text(source_root->value.compound.items[0]->name, "Extra") &&
          ff_field(source_root, "Extra")->value.double_bits == UINT64_C(0x7ff0000000000042) &&
          loaded[0]->stack == NULL);
    size_t live = 0;
    CHECK(mc_entity_storage_native_bytes(loaded[0], &live, &error) == MC_ENTITY_NBT_OK &&
          ff_nbt_fresh(&error) &&
          live == mc_entity_native_bytes(loaded[0]) +
                      mc_nbt_tree_allocation_bytes(loaded[0]->storage_source));
    limit = mc_nbt_default_limits();
    limit.max_allocation_bytes = live;
    CHECK(mc_entity_preserve_source(loaded[0], &record.root, &limit, &error) ==
              MC_ENTITY_NBT_LIMIT &&
          mc_entity_storage_source(loaded[0]) == source_root);
    loaded[0]->falling.a = INT32_C(0x12345678); /* Prepared actual carried-ID control. */
    loaded[0]->falling.b = 93;
    CHECK(mc_entity_save_storage_nbt(loaded[0], NULL, &serialized, &saved, &error) ==
              MC_ENTITY_NBT_OK &&
          serialized && ff_nbt_fresh(&error));
    const mc_nbt_tag *output = mc_nbt_tree_root(saved);
    CHECK(output != NULL && output->value.compound.length == 10 && output->name.length == 3 &&
          output->name.units[0] == 'F' && output->name.units[1] == 0 &&
          output->name.units[2] == 0xd800 &&
          ff_text(output->value.compound.items[0]->name, "Extra") &&
          ff_text(output->value.compound.items[1]->name, "Tile") &&
          ff_field(output, "Extra")->value.double_bits == UINT64_C(0x7ff0000000000042) &&
          ff_field(output, "Tile")->type == MC_NBT_BYTE &&
          ff_field(output, "Tile")->value.byte_value == 120 && ff_field(output, "Item") == NULL &&
          ff_field(output, "Age") == NULL && ff_field(output, "Health") == NULL &&
          loaded[0]->falling.a == INT32_C(0x12345678) && loaded[0]->falling.b == 93 &&
          mc_entity_storage_source(loaded[0]) == source_root);
    CHECK(ff_capture(loaded[0], &guard));
    loaded[0]->base.be = true; /* Legitimate source dead input, restored before CHECK. */
    out = saved;
    serialized = true;
    limit = mc_nbt_default_limits();
    limit.max_allocation_bytes = 0;
    const mc_entity_nbt_status dead =
        mc_entity_save_storage_nbt(loaded[0], &limit, &serialized, &out, &error);
    loaded[0]->base.be = false;
    CHECK(dead == MC_ENTITY_NBT_OK && !serialized && out == saved && ff_nbt_fresh(&error) &&
          ff_unchanged(loaded[0], &guard) && mc_entity_storage_source(loaded[0]) == source_root);
    CHECK(ff_known(mc_nbt_tree_root(named), true, -1) &&
          ff_saved_constructor(mc_nbt_tree_root(named)));
    /* Independent earlier output survives later real source/owner mutations. */
    result = 0;
cleanup:
    mc_nbt_tree_free(saved);
    mc_nbt_tree_free(extra);
    mc_nbt_tree_free(named);
    mc_nbt_tree_free(leaf);
    mc_entity_release(loaded[0]);
    mc_entity_release(loaded[1]);
    mc_entity_release(entity);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int ff_dead_namespace(mc_entity_source source)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL;
    mc_entity_nbt_error error = {0};
    mc_entity_registry_view view = {0};
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_entity_limits exact = {ff_static_bytes() + ff_direct_bytes(), 1};
    CHECK(mc_entity_classes_create(source, profile, &exact, &classes) == MC_ENTITY_OK &&
          mc_falling_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(entity, NULL, NULL) == MC_ENTITY_OK);
    entity->base.be = true; /* Real source dead flag cannot bypass name active use. */
    const uint64_t unique = uniquifier(profile);
    bool serialized = true;
    mc_nbt_tree *out = NULL;
    CHECK(mc_entity_save_nbt(entity, NULL, &serialized, &out, &error) == MC_ENTITY_NBT_LIMIT &&
          serialized && out == NULL && error.wrapper_bytecode == 1 && error.name_bytecode == 1 &&
          error.base_bytecode == 0 && error.item_bytecode == 0 && error.falling_bytecode == 0 &&
          error.registry.status == MC_ENTITY_REGISTRY_LIMIT &&
          error.registry.cause == MC_ENTITY_REGISTRY_LIMIT &&
          error.registry.initializer_bytecode == 0 && error.initializer_bytecode == 0 &&
          error.registration_bytecode == 0);
    CHECK(mc_entity_registry_inspect(classes, &view) == MC_ENTITY_REGISTRY_OK &&
          view.phase == MC_ENTITY_CLASS_FAILED &&
          view.first_failure.status == MC_ENTITY_REGISTRY_LIMIT &&
          view.first_failure.cause == MC_ENTITY_REGISTRY_LIMIT && view.maps[0] == NULL &&
          view.maps[1] == NULL && view.maps[2] == NULL && view.maps[3] == NULL);
    const mc_entity_registry_error original = error.registry;
    CHECK(mc_entity_save_nbt(entity, NULL, &serialized, &out, &error) ==
              MC_ENTITY_NBT_REGISTRY_FAILED &&
          serialized && out == NULL && error.wrapper_bytecode == 1 && error.name_bytecode == 1 &&
          error.registry.status == MC_ENTITY_REGISTRY_CLASS_FAILED &&
          error.registry.cause == original.cause &&
          error.registry.operation == original.operation &&
          error.registry.initializer_bytecode == original.initializer_bytecode &&
          error.registry.registration_bytecode == original.registration_bytecode &&
          error.registry.class_cause == original.class_cause && uniquifier(profile) == unique &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
    result = 0;
cleanup:
    mc_entity_release(entity);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int ff_world_binding(mc_entity_source source)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_world_runtime_classes *world_classes = NULL;
    mc_world_runtime *world = NULL;
    mc_entity *entity = NULL, *held = NULL, *loaded = NULL;
    mc_math_trig trig;
    mc_entity_snapshot snapshot = {0};
    mc_world_runtime_construction construction = {0};
    mc_entity_nbt_error error = {0};
    ff_record record;
    CHECK(mc_math_trig_init(&trig) &&
          mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK &&
          mc_world_runtime_classes_create(
              source == MC_ENTITY_CLIENT ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, profile,
              &trig, &world_classes, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_entities(world_classes, classes) == MC_ENTITY_OK);
    /* Actual allocated RAW GAME World is sufficient for the constructor's
     * profile/retained World binding. No READY constructor/Store is invented. */
    CHECK(mc_world_runtime_allocate_world(
              source == MC_ENTITY_CLIENT ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                         : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
              world_classes, NULL, &world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_construction_get(world, &construction) == MC_WORLD_STORAGE_OK &&
          construction.phase == MC_WORLD_RUNTIME_RAW && !construction.contexts_bound &&
          !construction.cache_registered);
    const uint64_t unique = uniquifier(profile);
    CHECK(mc_falling_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct(entity, world, -0.5, 1.5, 2.5, -1, NULL) == MC_ENTITY_OK &&
          mc_entity_get_snapshot(entity, &snapshot) == MC_ENTITY_OK &&
          snapshot.base.world == world && snapshot.falling.a == -1 && snapshot.falling.b == 0 &&
          snapshot.stack == NULL && snapshot.base_bytecode == 231 &&
          snapshot.falling_bytecode == 80);
    ff_record_init(&record);
    CHECK(
        mc_entity_factory_nbt(classes, world, &record.root, &loaded, &error) == MC_ENTITY_NBT_OK &&
        loaded != NULL && ff_nbt_fresh(&error) &&
        mc_entity_get_snapshot(loaded, &snapshot) == MC_ENTITY_OK && snapshot.base.world == world &&
        snapshot.base.id == 1 && ff_read_values(&snapshot, 255, 0) &&
        snapshot.falling_bytecode == 10 && snapshot.base.width == 0.6f &&
        snapshot.base.height == 1.8f && uniquifier(profile) == advanced(unique, 2));
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_entity_retain(entity) == MC_ENTITY_OK);
    held = entity;
    mc_entity_release(entity);
    entity = NULL;
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_entity_get_snapshot(held, &snapshot) == MC_ENTITY_OK && snapshot.base.world == world);
    mc_entity_release(held);
    held = NULL;
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY);
    mc_entity_release(loaded);
    loaded = NULL;
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    world = NULL;
    CHECK(mc_world_runtime_classes_free(world_classes, NULL) == MC_WORLD_STORAGE_OK);
    world_classes = NULL;
    CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
    result = 0;
cleanup:
    mc_entity_release(entity);
    mc_entity_release(held);
    mc_entity_release(loaded);
    bool released = mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK;
    if (!released)
        result = 1;
    if (released && mc_world_runtime_classes_free(world_classes, NULL) != MC_WORLD_STORAGE_OK) {
        released = false;
        result = 1;
    }
    if (released && retire_classes(&classes) != 0)
        result = 1;
    if (released && classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

int main(void)
{
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fprintf(stderr, "falling tests require the qualified strict SSE environment\n");
        return 1;
    }
    for (unsigned int side = 0; side < 2; ++side) {
        const mc_entity_source source = side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
        if (ff_constructors(source) || ff_limits(source) || ff_nbt(source) ||
            ff_dead_namespace(source) || ff_world_binding(source))
            return 1;
    }
    printf("FallingSand construction/NBT: %u checks passed\n", checks);
    return 0;
}
