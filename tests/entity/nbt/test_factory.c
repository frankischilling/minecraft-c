#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

/* Reuse real constructor/profile helpers. Its old main and its explicit
 * mutation-control cases are defined but never called by this test. */
#define main mc_entity_constructor_fixture_main
#include "../test_entity.c"
#undef main

#include "mc/entity/nbt/factory.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

/* Native source-derived contracts only. No original observations, natural
 * entity state, encoded corpus replay or World/Store round trip is implied.
 * New code never writes phase/counter/holds/ledger/limits/Random inputs. Source
 * scalar/tag controls are explicitly prepared on genuine owners or views. */
typedef struct fc_record {
    mc_nbt_tag root, fields[11], vectors[8], stack[3];
    const mc_nbt_tag *items[11], *pos[3], *motion[3], *rotation[2], *stack_items[3];
    uint16_t names[14][16], class_name[48];
} fc_record;

typedef struct fc_guard {
    mc_entity_snapshot entity;
    mc_entity_bounds bounds;
    mc_entity_class_snapshot classes;
    mc_entity_registry_view registry;
    mc_entity_watcher_snapshot watcher;
    mc_java_random ordinary, math;
    uint64_t unique;
    size_t holds, borrowers;
} fc_guard;

static mc_nbt_string fc_ascii(const char *text, uint16_t *units) {
    const size_t length = strlen(text);
    for (size_t index = 0; index < length; ++index)
        units[index] = (uint16_t)(unsigned char)text[index];
    return (mc_nbt_string){units, length};
}

static bool fc_text(mc_nbt_string value, const char *expected) {
    if (value.length != strlen(expected) || (value.length != 0 && value.units == NULL))
        return false;
    for (size_t index = 0; index < value.length; ++index)
        if (value.units[index] != (unsigned char)expected[index])
            return false;
    return true;
}

static const mc_nbt_tag *fc_field(const mc_nbt_tag *root, const char *name) {
    uint16_t units[16];
    const mc_nbt_string key = fc_ascii(name, units);
    return mc_nbt_compound_get(root, key.units, key.length);
}

static void fc_record_init(fc_record *record, const char *class_name) {
    static const char *const names[] = {"id",   "Pos", "Motion",   "Rotation", "FallDistance",
                                        "Fire", "Air", "OnGround", "Health",   "Age",
                                        "Item", "id",  "Count",    "Damage"};
    static const mc_nbt_type types[] = {MC_NBT_STRING, MC_NBT_LIST,  MC_NBT_LIST,    MC_NBT_LIST,
                                        MC_NBT_FLOAT,  MC_NBT_SHORT, MC_NBT_SHORT,   MC_NBT_BYTE,
                                        MC_NBT_SHORT,  MC_NBT_SHORT, MC_NBT_COMPOUND};
    static const double values[] = {8.0, 16.5, 32.0, 20.0, 30.0, 40.0};
    memset(record, 0, sizeof(*record));
    record->root.type = MC_NBT_COMPOUND;
    record->root.value.compound.items = record->items;
    record->root.value.compound.length = 11;
    for (size_t index = 0; index < 11; ++index) {
        record->fields[index].name = fc_ascii(names[index], record->names[index]);
        record->fields[index].type = types[index];
        record->items[index] = &record->fields[index];
    }
    record->fields[0].value.string = fc_ascii(class_name, record->class_name);
    for (size_t index = 0; index < 6; ++index) {
        record->vectors[index].type = MC_NBT_DOUBLE;
        record->vectors[index].value.double_bits = double_bits(values[index]);
    }
    record->vectors[6].type = record->vectors[7].type = MC_NBT_FLOAT;
    record->vectors[6].value.float_bits = float_bits(725.0f);
    record->vectors[7].value.float_bits = float_bits(-725.0f);
    for (size_t index = 0; index < 3; ++index) {
        record->pos[index] = &record->vectors[index];
        record->motion[index] = &record->vectors[index + 3];
        record->stack_items[index] = &record->stack[index];
        record->stack[index].name = fc_ascii(names[index + 11], record->names[index + 11]);
        record->stack[index].type = index == 1 ? MC_NBT_BYTE : MC_NBT_SHORT;
    }
    record->rotation[0] = &record->vectors[6];
    record->rotation[1] = &record->vectors[7];
    record->fields[1].value.list.element_type = record->fields[2].value.list.element_type =
        MC_NBT_DOUBLE;
    record->fields[3].value.list.element_type = MC_NBT_FLOAT;
    record->fields[1].value.list.items = record->pos;
    record->fields[2].value.list.items = record->motion;
    record->fields[3].value.list.items = record->rotation;
    record->fields[1].value.list.length = record->fields[2].value.list.length = 3;
    record->fields[3].value.list.length = 2;
    record->fields[4].value.float_bits = float_bits(3.5f);
    record->fields[5].value.short_value = -19;
    record->fields[6].value.short_value = 32767;
    record->fields[7].value.byte_value = 1;
    record->fields[8].value.short_value = -1;
    record->fields[9].value.short_value = -17;
    record->fields[10].value.compound.items = record->stack_items;
    record->fields[10].value.compound.length = 3;
    record->stack[0].value.short_value = 17;
    record->stack[1].value.byte_value = 4;
    record->stack[2].value.short_value = 5;
}

static bool fc_string_equal(const char *left, const char *right) {
    return left == NULL || right == NULL ? left == right : strcmp(left, right) == 0;
}

static bool fc_registry_error_equal(const mc_entity_registry_error *left,
                                    const mc_entity_registry_error *right) {
    return left->status == right->status && left->cause == right->cause &&
           fc_string_equal(left->operation, right->operation) &&
           left->initializer_bytecode == right->initializer_bytecode &&
           left->registration_bytecode == right->registration_bytecode &&
           left->class_cause == right->class_cause;
}

static mc_entity_nbt_error fc_empty(void) {
    return (mc_entity_nbt_error){.status = MC_ENTITY_NBT_OK,
                                 .index = SIZE_MAX,
                                 .construction = {.status = MC_ENTITY_OK,
                                                  .profile = MC_JAVA_RANDOM_PROFILE_OK,
                                                  .item = MC_ITEM_STACK_OK,
                                                  .world = MC_WORLD_STORAGE_OK},
                                 .stack = {.status = MC_ITEM_STACK_OK, .nbt = {MC_NBT_OK, 0}},
                                 .nbt = {MC_NBT_OK, 0},
                                 .registry = {.status = MC_ENTITY_REGISTRY_OK,
                                              .cause = MC_ENTITY_REGISTRY_OK,
                                              .class_cause = MC_ENTITY_OK}};
}

static mc_entity_nbt_error fc_poison(void) {
    return (mc_entity_nbt_error){
        .status = MC_ENTITY_NBT_UNSUPPORTED_TYPE,
        .operation = "old operation",
        .field = "old field",
        .index = 97,
        .wrapper_bytecode = 91,
        .name_bytecode = 92,
        .factory_bytecode = 93,
        .base_bytecode = 94,
        .item_bytecode = 95,
        .stack_bytecode = 96,
        .initializer_bytecode = 97,
        .registration_bytecode = 98,
        .construction = {.status = MC_ENTITY_BUSY,
                         .operation = "old constructor",
                         .bytecode = 99,
                         .profile = MC_JAVA_RANDOM_PROFILE_LIMIT,
                         .item = MC_ITEM_STACK_LIMIT,
                         .world = MC_WORLD_STORAGE_LIMIT},
        .stack = {.status = MC_ITEM_STACK_LIMIT, .field = "old stack", .nbt = {MC_NBT_LIMIT, 101}},
        .nbt = {MC_NBT_INVALID_TREE, 102},
        .registry = {.status = MC_ENTITY_REGISTRY_BUSY,
                     .cause = MC_ENTITY_REGISTRY_LIMIT,
                     .operation = "old registry",
                     .initializer_bytecode = 103,
                     .registration_bytecode = 104,
                     .class_cause = MC_ENTITY_LIMIT}};
}

static bool fc_error_equal(const mc_entity_nbt_error *left, const mc_entity_nbt_error *right) {
    return left->status == right->status && fc_string_equal(left->operation, right->operation) &&
           fc_string_equal(left->field, right->field) && left->index == right->index &&
           left->wrapper_bytecode == right->wrapper_bytecode &&
           left->name_bytecode == right->name_bytecode &&
           left->factory_bytecode == right->factory_bytecode &&
           left->base_bytecode == right->base_bytecode &&
           left->item_bytecode == right->item_bytecode &&
           left->stack_bytecode == right->stack_bytecode &&
           left->initializer_bytecode == right->initializer_bytecode &&
           left->registration_bytecode == right->registration_bytecode &&
           left->construction.status == right->construction.status &&
           fc_string_equal(left->construction.operation, right->construction.operation) &&
           left->construction.bytecode == right->construction.bytecode &&
           left->construction.profile == right->construction.profile &&
           left->construction.item == right->construction.item &&
           left->construction.world == right->construction.world &&
           left->stack.status == right->stack.status &&
           fc_string_equal(left->stack.field, right->stack.field) &&
           left->stack.nbt.status == right->stack.nbt.status &&
           left->stack.nbt.offset == right->stack.nbt.offset &&
           left->nbt.status == right->nbt.status && left->nbt.offset == right->nbt.offset &&
           fc_registry_error_equal(&left->registry, &right->registry);
}

static bool fc_fresh(const mc_entity_nbt_error *error) {
    const mc_entity_nbt_error expected = fc_empty();
    return fc_error_equal(error, &expected);
}

static bool fc_classes_equal(const mc_entity_class_snapshot *left,
                             const mc_entity_class_snapshot *right) {
    return left->source == right->source && left->entity_class == right->entity_class &&
           left->bounds_class == right->bounds_class &&
           left->watcher_class == right->watcher_class && left->next_id == right->next_id &&
           left->bounds_pool_cursor == right->bounds_pool_cursor &&
           left->bounds_pool_identity == right->bounds_pool_identity &&
           left->watcher_types_identity == right->watcher_types_identity &&
           left->watcher_type_count == right->watcher_type_count &&
           left->live_entities == right->live_entities &&
           left->allocation_bytes == right->allocation_bytes;
}

static bool fc_registry_equal(const mc_entity_registry_view *left,
                              const mc_entity_registry_view *right) {
    if (left->phase != right->phase ||
        !fc_registry_error_equal(&left->first_failure, &right->first_failure))
        return false;
    for (size_t index = 0; index < 4; ++index)
        if (left->maps[index] != right->maps[index] || left->counts[index] != right->counts[index])
            return false;
    return true;
}

static bool fc_ready_registry(const mc_entity_registry_view *view) {
    if (view->phase != MC_ENTITY_CLASS_READY)
        return false;
    const mc_entity_registry_error empty = {0};
    if (!fc_registry_error_equal(&view->first_failure, &empty))
        return false;
    for (size_t index = 0; index < 4; ++index)
        if (view->maps[index] == NULL || view->counts[index] != 24)
            return false;
    return true;
}

static bool fc_capture(mc_entity *entity, fc_guard *guard) {
    if (mc_entity_get_snapshot(entity, &guard->entity) != MC_ENTITY_OK ||
        guard->entity.bounds == NULL || guard->entity.random == NULL ||
        mc_entity_classes_get_snapshot(entity->classes, &guard->classes) != MC_ENTITY_OK ||
        mc_entity_registry_inspect(entity->classes, &guard->registry) != MC_ENTITY_REGISTRY_OK ||
        mc_entity_watcher_get_snapshot(guard->entity.watcher, &guard->watcher) != MC_ENTITY_OK)
        return false;
    guard->bounds = *guard->entity.bounds;
    guard->ordinary = *guard->entity.random;
    guard->math = math_snapshot(entity->classes->profile);
    guard->unique = uniquifier(entity->classes->profile);
    guard->holds = entity->holds;
    guard->borrowers = entity->classes->borrowers;
    return true;
}

static bool fc_unchanged(mc_entity *entity, const fc_guard *guard) {
    fc_guard current;
    return fc_capture(entity, &current) && base_equal(&current.entity.base, &guard->entity.base) &&
           current.entity.source == guard->entity.source &&
           current.entity.phase == guard->entity.phase &&
           current.entity.base_bytecode == guard->entity.base_bytecode &&
           current.entity.item_bytecode == guard->entity.item_bytecode &&
           current.entity.item.b == guard->entity.item.b &&
           current.entity.item.c == guard->entity.item.c &&
           current.entity.item.e == guard->entity.item.e &&
           current.entity.item.f == guard->entity.item.f &&
           float_bits(current.entity.item.d) == float_bits(guard->entity.item.d) &&
           current.entity.stack == guard->entity.stack &&
           current.entity.bounds == guard->entity.bounds &&
           current.entity.random == guard->entity.random &&
           current.entity.watcher == guard->entity.watcher &&
           bounds_equal(&current.bounds, &guard->bounds) &&
           mc_test_java_random_equal(&current.ordinary, &guard->ordinary) &&
           mc_test_java_random_equal(&current.math, &guard->math) &&
           current.unique == guard->unique && current.holds == guard->holds &&
           current.borrowers == guard->borrowers &&
           fc_classes_equal(&current.classes, &guard->classes) &&
           fc_registry_equal(&current.registry, &guard->registry) &&
           current.watcher.record == guard->watcher.record &&
           current.watcher.size == guard->watcher.size &&
           current.watcher.dirty == guard->watcher.dirty &&
           current.watcher.index == guard->watcher.index &&
           current.watcher.type == guard->watcher.type &&
           current.watcher.byte_value == guard->watcher.byte_value &&
           current.watcher.record_dirty == guard->watcher.record_dirty;
}

static size_t fc_tags(const mc_nbt_tag *tag) {
    size_t count = 1;
    if (tag->type == MC_NBT_COMPOUND)
        for (size_t index = 0; index < tag->value.compound.length; ++index)
            count += fc_tags(tag->value.compound.items[index]);
    else if (tag->type == MC_NBT_LIST)
        for (size_t index = 0; index < tag->value.list.length; ++index)
            count += fc_tags(tag->value.list.items[index]);
    return count;
}

static bool fc_saved_record(const mc_nbt_tag *root) {
    static const char *const names[] = {"id",           "Pos",  "Motion", "Rotation",
                                        "FallDistance", "Fire", "Air",    "OnGround",
                                        "Health",       "Age",  "Item"};
    static const mc_nbt_type types[] = {MC_NBT_STRING, MC_NBT_LIST,  MC_NBT_LIST,    MC_NBT_LIST,
                                        MC_NBT_FLOAT,  MC_NBT_SHORT, MC_NBT_SHORT,   MC_NBT_BYTE,
                                        MC_NBT_SHORT,  MC_NBT_SHORT, MC_NBT_COMPOUND};
    if (root == NULL || root->type != MC_NBT_COMPOUND || root->value.compound.length != 11 ||
        fc_tags(root) != 23)
        return false;
    for (size_t index = 0; index < 11; ++index) {
        const mc_nbt_tag *tag = fc_field(root, names[index]);
        if (tag == NULL || tag->type != types[index])
            return false;
    }
    const mc_nbt_tag *pos = fc_field(root, "Pos"), *motion = fc_field(root, "Motion"),
                     *rotation = fc_field(root, "Rotation"), *item = fc_field(root, "Item");
    if (!fc_text(fc_field(root, "id")->value.string, "Item") || pos->value.list.length != 3 ||
        motion->value.list.length != 3 || rotation->value.list.length != 2 ||
        pos->value.list.element_type != MC_NBT_DOUBLE ||
        motion->value.list.element_type != MC_NBT_DOUBLE ||
        rotation->value.list.element_type != MC_NBT_FLOAT || item->value.compound.length != 3)
        return false;
    const double position[] = {8.0, 16.5, 32.0}, velocity[] = {20.0, 30.0, 40.0};
    for (size_t index = 0; index < 3; ++index)
        if (pos->value.list.items[index]->type != MC_NBT_DOUBLE ||
            motion->value.list.items[index]->type != MC_NBT_DOUBLE ||
            pos->value.list.items[index]->value.double_bits != double_bits(position[index]) ||
            motion->value.list.items[index]->value.double_bits != double_bits(velocity[index]))
            return false;
    const mc_nbt_tag *id = fc_field(item, "id"), *count = fc_field(item, "Count"),
                     *damage = fc_field(item, "Damage");
    return rotation->value.list.items[0]->type == MC_NBT_FLOAT &&
           rotation->value.list.items[1]->type == MC_NBT_FLOAT &&
           rotation->value.list.items[0]->value.float_bits == float_bits(725.0f) &&
           rotation->value.list.items[1]->value.float_bits == float_bits(-725.0f) &&
           fc_field(root, "FallDistance")->value.float_bits == float_bits(3.5f) &&
           fc_field(root, "Fire")->value.short_value == -19 &&
           fc_field(root, "Air")->value.short_value == 32767 &&
           fc_field(root, "OnGround")->value.byte_value == 1 &&
           fc_field(root, "Health")->value.short_value == -1 &&
           fc_field(root, "Age")->value.short_value == -17 && id != NULL && count != NULL &&
           damage != NULL && id->type == MC_NBT_SHORT && id->value.short_value == 17 &&
           count->type == MC_NBT_BYTE && count->value.byte_value == 4 &&
           damage->type == MC_NBT_SHORT && damage->value.short_value == 5;
}

static bool fc_loaded(mc_entity *entity, mc_entity_source source, int32_t id) {
    mc_entity_snapshot view;
    mc_item_stack_fields stack;
    const mc_entity_bounds box = {7.875, 16.375, 31.875, 8.125, 16.625, 32.125};
    mc_entity_base_fields expected = base_literals(id);
    expected.width = expected.height = 0.25f;
    expected.offset = 0.125f;
    expected.x = expected.aJ = expected.bl = 8.0;
    expected.y = expected.aK = expected.bm = 16.5;
    expected.z = expected.aL = expected.bn = 32.0;
    expected.yaw = 5.0f;
    expected.aT = -5.0f;
    expected.aU = 725.0f;
    expected.aV = -725.0f;
    expected.bk = 3.5f;
    expected.bv = -19;
    expected.bz = 32767;
    expected.aX = true;
    return mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK && view.source == source &&
           view.phase == MC_ENTITY_READY && view.base_bytecode == 231 && view.item_bytecode == 49 &&
           base_equal(&view.base, &expected) && view.bounds != NULL &&
           bounds_equal(view.bounds, &box) && fresh_random(view.random) &&
           watcher_is(&view, true) && view.stack != NULL && view.item.b == -17 &&
           view.item.f == 255 && view.item.c == 0 && view.item.e == 0 && view.item.d >= 0.0f &&
           view.item.d <= 6.283186f &&
           mc_item_stack_fields_get(view.stack, &stack) == MC_ITEM_STACK_OK && stack.id == 17 &&
           stack.count == 4 && stack.damage == 5 && stack.animation == 0;
}

static size_t fc_constructed_bytes(void) {
    /* Exact native owner requests for one real world-only Item constructor;
     * this is not an original JVM allocation-size/OOME expectation. */
    return sizeof(mc_entity_classes) + sizeof(mc_entity) + sizeof(mc_entity_bounds_pool) +
           sizeof(mc_entity_bounds) + sizeof(mc_java_random) + sizeof(mc_entity_type_map) +
           7u * sizeof(mc_entity_type_entry) + sizeof(mc_entity_watcher) +
           sizeof(mc_entity_watch_map) + sizeof(mc_entity_watch_record) +
           sizeof(mc_entity_watch_node);
}

static size_t fc_namespace_bytes(void) {
    return 4u * sizeof(mc_entity_registry_map) + 96u * sizeof(mc_entity_registry_node);
}

/* Capture only this process's bounded default-stdout write, with no callback
 * or production stream injection. Restore the real handle before assertions.
 * Output is at most66 platform bytes, so the pipe cannot fill during the call. */
static bool fc_capture_call(mc_entity_classes *classes, const mc_nbt_tag *compound, mc_entity **out,
                            mc_entity_nbt_error *error, mc_entity_nbt_status *status,
                            char bytes[80], size_t *length) {
    bool okay = false;
    size_t used = 0;
#ifdef _WIN32
    HANDLE read_handle = NULL, write_handle = NULL;
    const HANDLE original = GetStdHandle(STD_OUTPUT_HANDLE);
    bool replaced = false;
    if (!CreatePipe(&read_handle, &write_handle, NULL, 0) ||
        !SetStdHandle(STD_OUTPUT_HANDLE, write_handle))
        goto done;
    replaced = true;
    *status = mc_entity_factory_nbt(classes, NULL, compound, out, error);
    if (!SetStdHandle(STD_OUTPUT_HANDLE, original))
        goto done;
    replaced = false;
    if (!CloseHandle(write_handle))
        goto done;
    write_handle = NULL;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(read_handle, bytes + used, (DWORD)(80u - used), &got, NULL)) {
            if (GetLastError() == ERROR_BROKEN_PIPE)
                break;
            goto done;
        }
        if (got == 0)
            break;
        used += got;
        if (used == 80u)
            goto done;
    }
    okay = true;
done:
    if (replaced && !SetStdHandle(STD_OUTPUT_HANDLE, original))
        okay = false;
    if (write_handle != NULL && !CloseHandle(write_handle))
        okay = false;
    if (read_handle != NULL && !CloseHandle(read_handle))
        okay = false;
#else
    int ends[2] = {-1, -1}, original = -1;
    bool replaced = false;
    if (pipe(ends) != 0 || (original = dup(STDOUT_FILENO)) < 0 || dup2(ends[1], STDOUT_FILENO) < 0)
        goto done;
    replaced = true;
    *status = mc_entity_factory_nbt(classes, NULL, compound, out, error);
    if (dup2(original, STDOUT_FILENO) < 0)
        goto done;
    replaced = false;
    if (close(ends[1]) != 0)
        goto done;
    ends[1] = -1;
    for (;;) {
        const ssize_t got = read(ends[0], bytes + used, 80u - used);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            goto done;
        }
        if (got == 0)
            break;
        used += (size_t)got;
        if (used == 80u)
            goto done;
    }
    okay = true;
done:
    if (replaced && dup2(original, STDOUT_FILENO) < 0)
        okay = false;
    if (original >= 0 && close(original) != 0)
        okay = false;
    if (ends[0] >= 0 && close(ends[0]) != 0)
        okay = false;
    if (ends[1] >= 0 && close(ends[1]) != 0)
        okay = false;
#endif
    if (okay)
        *length = used;
    return okay;
}

static int fc_dead_namespace(mc_entity_source source, unsigned int control) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL;
    mc_nbt_tree *sentinel = NULL;
    mc_entity_class_snapshot before, after;
    mc_entity_registry_view cold, view, repeated;
    mc_entity_nbt_error error, expected;
    mc_entity_limits limit = {control == 0
                                  ? SIZE_MAX
                                  : fc_constructed_bytes() +
                                        (control == 2 ? 4u * sizeof(mc_entity_registry_map) +
                                                            2u * sizeof(mc_entity_registry_node)
                                                      : 0),
                              SIZE_MAX};
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(source, profile, &limit, &classes) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct_world(entity, NULL, NULL) == MC_ENTITY_OK);
    CHECK(mc_nbt_tree_copy_compound(&empty, NULL, &sentinel, NULL) == MC_NBT_OK);
    CHECK(mc_entity_classes_get_snapshot(classes, &before) == MC_ENTITY_OK &&
          before.allocation_bytes == fc_constructed_bytes() && before.next_id == 1);
    CHECK(mc_entity_registry_inspect(classes, &cold) == MC_ENTITY_REGISTRY_OK &&
          cold.phase == MC_ENTITY_CLASS_COLD);
    entity->base.be =
        true; /* Explicit source dead input on a real constructed Item, Stack still NULL. */
    const uint64_t unique = uniquifier(profile);
    const mc_java_random math = math_snapshot(profile);
    mc_nbt_tree *out = sentinel;
    bool serialized = true;
    if (control == 0) {
        for (unsigned int argument = 0; argument < 3; ++argument) {
            error = fc_poison();
            CHECK(mc_entity_save_nbt(
                      argument == 0 ? NULL : entity, NULL, argument == 1 ? NULL : &serialized,
                      argument == 2 ? NULL : &out, &error) == MC_ENTITY_NBT_INVALID_ARGUMENT &&
                  serialized && out == sentinel && uniquifier(profile) == unique &&
                  math_equal(profile, &math));
            expected = fc_empty();
            expected.status = MC_ENTITY_NBT_INVALID_ARGUMENT;
            expected.operation = error.operation;
            CHECK(error.operation != NULL && fc_error_equal(&error, &expected) &&
                  mc_entity_registry_inspect(classes, &view) == MC_ENTITY_REGISTRY_OK &&
                  fc_registry_equal(&view, &cold));
        }
    }
    error = fc_poison();
    const mc_entity_nbt_status status = mc_entity_save_nbt(entity, NULL, &serialized, &out, &error);
    CHECK(out == sentinel && uniquifier(profile) == unique && math_equal(profile, &math) &&
          entity->stack == NULL);
    CHECK(mc_entity_registry_inspect(classes, &view) == MC_ENTITY_REGISTRY_OK);
    if (control == 0) {
        CHECK(status == MC_ENTITY_NBT_OK && !serialized && fc_fresh(&error) &&
              fc_ready_registry(&view));
        CHECK(mc_entity_classes_get_snapshot(classes, &after) == MC_ENTITY_OK &&
              after.next_id == 1 && after.live_entities == 1 &&
              after.allocation_bytes == before.allocation_bytes + fc_namespace_bytes());
        mc_nbt_limits none = mc_nbt_default_limits();
        none.max_allocation_bytes = none.max_tags = none.max_elements = none.max_output_bytes =
            none.max_depth = 0;
        error = fc_poison();
        serialized = true;
        CHECK(mc_entity_save_nbt(entity, &none, &serialized, &out, &error) == MC_ENTITY_NBT_OK &&
              !serialized && out == sentinel && fc_fresh(&error));
        entity->base.be = false; /* Same actual source field; no constructor/counter reset. */
        fc_guard guard;
        CHECK(fc_capture(entity, &guard));
        serialized = true;
        error = fc_poison();
        CHECK(mc_entity_save_nbt(entity, NULL, &serialized, &out, &error) ==
                  MC_ENTITY_NBT_NULL_REFERENCE &&
              serialized && out == sentinel && fc_unchanged(entity, &guard));
        expected = fc_empty();
        expected.status = MC_ENTITY_NBT_NULL_REFERENCE;
        expected.operation = error.operation;
        expected.field = "Item";
        expected.wrapper_bytecode = 27;
        expected.name_bytecode = 7;
        expected.base_bytecode = 145;
        expected.item_bytecode = 37;
        CHECK(error.operation != NULL && fc_error_equal(&error, &expected));
    } else {
        const unsigned int initializer = control == 2 ? 46u : 0u,
                           registration = control == 2 ? 30u : 0u;
        CHECK(status == MC_ENTITY_NBT_LIMIT && serialized && view.phase == MC_ENTITY_CLASS_FAILED &&
              view.first_failure.status == MC_ENTITY_REGISTRY_LIMIT &&
              view.first_failure.cause == MC_ENTITY_REGISTRY_LIMIT &&
              view.first_failure.operation != NULL &&
              view.first_failure.class_cause == MC_ENTITY_OK &&
              view.first_failure.initializer_bytecode == initializer &&
              view.first_failure.registration_bytecode == registration);
        expected = fc_empty();
        expected.status = MC_ENTITY_NBT_LIMIT;
        expected.operation = error.operation;
        /* aj/ag invokestatic1 activates jc/ga before map.get body7,
         * including an already FAILED namespace's cached failure. */
        expected.wrapper_bytecode = 1;
        expected.name_bytecode = 1;
        expected.initializer_bytecode = initializer;
        expected.registration_bytecode = registration;
        expected.registry = view.first_failure;
        CHECK(error.operation != NULL && fc_error_equal(&error, &expected));
        for (size_t index = 0; index < 4; ++index)
            CHECK((control == 1 ? view.maps[index] == NULL : view.maps[index] != NULL) &&
                  view.counts[index] == (control == 2 && index < 2 ? 1u : 0u));
        CHECK(mc_entity_classes_get_snapshot(classes, &after) == MC_ENTITY_OK &&
              after.next_id == 1 && after.live_entities == 1 &&
              after.allocation_bytes == limit.max_allocation_bytes);
        error = fc_poison();
        CHECK(mc_entity_save_nbt(entity, NULL, &serialized, &out, &error) ==
                  MC_ENTITY_NBT_REGISTRY_FAILED &&
              serialized && out == sentinel && uniquifier(profile) == unique &&
              math_equal(profile, &math));
        expected.status = MC_ENTITY_NBT_REGISTRY_FAILED;
        expected.operation = error.operation;
        expected.registry.status = MC_ENTITY_REGISTRY_CLASS_FAILED;
        CHECK(fc_error_equal(&error, &expected) &&
              mc_entity_registry_inspect(classes, &repeated) == MC_ENTITY_REGISTRY_OK &&
              fc_registry_equal(&view, &repeated));
        CHECK(mc_entity_classes_get_snapshot(classes, &before) == MC_ENTITY_OK &&
              fc_classes_equal(&before, &after));
    }
    result = 0;
cleanup:
    mc_nbt_tree_free(sentinel);
    mc_entity_release(entity);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int fc_save_roundtrip(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *loaded_classes = NULL;
    mc_entity *entity = NULL, *loaded[3] = {NULL, NULL, NULL};
    mc_item_stack *stack = NULL, *kept = NULL;
    mc_nbt_tree *tree = NULL, *leaf = NULL, *bounded = NULL;
    mc_entity_nbt_error error, expected;
    fc_guard guard;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK &&
          mc_entity_classes_create(source, profile, NULL, &loaded_classes) == MC_ENTITY_OK);
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 17, 4, 5) == MC_ITEM_STACK_OK);
    CHECK(mc_item_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(entity, NULL, 8.0, 16.0, 32.0, stack, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_set_velocity(entity, 0, 20.0) == MC_ENTITY_OK &&
          mc_entity_set_velocity(entity, 1, 30.0) == MC_ENTITY_OK &&
          mc_entity_set_velocity(entity, 2, 40.0) == MC_ENTITY_OK);
    /* Explicit prepared source scalar inputs; no original/natural-state claim. */
    entity->base.y_size = 0.5f;
    entity->base.yaw = 725.0f;
    entity->base.aT = -725.0f;
    entity->base.bk = 3.5f;
    entity->base.bv = 65517;
    entity->base.bz = -32769;
    entity->base.aX = true;
    entity->item.f = 511;
    entity->item.b = 65519;
    mc_entity_registry_error registry_error;
    CHECK(mc_entity_classes_initialize_registry(classes, &registry_error) == MC_ENTITY_REGISTRY_OK);
    CHECK(fc_capture(entity, &guard));
    bool serialized = false;
    error = fc_poison();
    CHECK(mc_entity_save_nbt(entity, NULL, &serialized, &tree, &error) == MC_ENTITY_NBT_OK &&
          serialized && tree != NULL && fc_fresh(&error) &&
          fc_saved_record(mc_nbt_tree_root(tree)) && fc_unchanged(entity, &guard));
    CHECK(mc_entity_build_nbt(entity, NULL, &leaf, NULL) == MC_ENTITY_NBT_OK &&
          fc_unchanged(entity, &guard));
    const size_t leaf_bytes = mc_nbt_tree_allocation_bytes(leaf),
                 final_bytes = mc_nbt_tree_allocation_bytes(tree);
    size_t encoded = 0;
    mc_nbt_limits exact = mc_nbt_default_limits();
    exact.max_allocation_bytes = final_bytes;
    CHECK(mc_nbt_measure(mc_nbt_tree_root(tree), &exact, &encoded, NULL) == MC_NBT_OK &&
          encoded != 0);
    CHECK(leaf_bytes <= SIZE_MAX - final_bytes);
    for (unsigned int control = 0; control < 5; ++control) {
        mc_nbt_limits limit = mc_nbt_default_limits();
        if (control == 0)
            limit.max_tags = 22; /* Leaf22 fits, final23 does not. */
        if (control == 1)
            limit.max_elements = 10; /* Leaf10 root fields fit, final11 does not. */
        if (control == 2)
            limit.max_output_bytes = encoded - 1;
        if (control == 3)
            limit.max_allocation_bytes = final_bytes;
        if (control == 4)
            limit.max_allocation_bytes = leaf_bytes + final_bytes - 1;
        mc_nbt_tree *out = tree;
        serialized = false;
        error = fc_poison();
        CHECK(mc_entity_save_nbt(entity, &limit, &serialized, &out, &error) ==
                  MC_ENTITY_NBT_LIMIT &&
              !serialized && out == tree && fc_unchanged(entity, &guard) &&
              fc_saved_record(mc_nbt_tree_root(tree)));
        expected = fc_empty();
        expected.status = MC_ENTITY_NBT_LIMIT;
        expected.operation = error.operation;
        expected.wrapper_bytecode = 27;
        expected.name_bytecode = 7;
        expected.nbt = error.nbt;
        CHECK(error.operation != NULL && error.nbt.status == MC_NBT_LIMIT &&
              fc_error_equal(&error, &expected));
    }
    exact = mc_nbt_default_limits();
    exact.max_input_bytes = 0;
    exact.max_tags = 23;
    exact.max_elements = 11;
    exact.max_depth = 3;
    exact.max_output_bytes = encoded;
    exact.max_allocation_bytes = leaf_bytes + final_bytes;
    serialized = false;
    error = fc_poison();
    CHECK(mc_entity_save_nbt(entity, &exact, &serialized, &bounded, &error) == MC_ENTITY_NBT_OK &&
          serialized && bounded != tree && mc_nbt_tree_allocation_bytes(bounded) == final_bytes &&
          fc_fresh(&error) && fc_unchanged(entity, &guard));
    CHECK(mc_item_stack_count_set(stack, 9) == MC_ITEM_STACK_OK &&
          fc_saved_record(mc_nbt_tree_root(tree)));
    CHECK(mc_item_stack_count_set(stack, 4) == MC_ITEM_STACK_OK);
    CHECK(mc_entity_set_position(entity, 9.0, 18.0, 36.0) == MC_ENTITY_OK &&
          fc_saved_record(mc_nbt_tree_root(tree)));
    CHECK(mc_entity_set_position(entity, 8.0, 16.0, 32.0) == MC_ENTITY_OK);
    const mc_entity_class_literal *first_type = NULL, *other_type = NULL;
    CHECK(mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_ITEM, &first_type) ==
              MC_ENTITY_OK &&
          mc_entity_class_literal_get(loaded_classes, MC_ENTITY_CLASS_KIND_ITEM, &other_type) ==
              MC_ENTITY_OK &&
          first_type != other_type);
    for (size_t index = 0; index < 3; ++index) {
        mc_java_random math = math_snapshot(profile);
        const double phase_draw = mc_java_random_next_double(&math);
        const float phase =
            (float)((phase_draw * 3.141592653589793) * 2.0); /* Paired constructor16..28. */
        const uint64_t unique = uniquifier(profile);
        error = fc_poison();
        CHECK(mc_entity_factory_nbt(loaded_classes, NULL, mc_nbt_tree_root(tree), &loaded[index],
                                    &error) == MC_ENTITY_NBT_OK &&
              loaded[index] != NULL && fc_fresh(&error) &&
              fc_loaded(loaded[index], source, (int32_t)index) &&
              float_bits(loaded[index]->item.d) == float_bits(phase) &&
              math_equal(profile, &math) && uniquifier(profile) == advanced(unique, 1));
        CHECK(loaded[index]->stack != stack && loaded[index]->base.world == NULL &&
              loaded[index]->classes == loaded_classes);
        if (index == 0)
            CHECK(!mc_entity_equals(entity, loaded[0]) && mc_entity_hash_code(entity) == 0 &&
                  mc_entity_hash_code(loaded[0]) == 0);
        if (index != 0)
            CHECK(loaded[index] != loaded[index - 1] &&
                  loaded[index]->stack != loaded[index - 1]->stack &&
                  loaded[index]->random != loaded[index - 1]->random &&
                  !mc_entity_equals(loaded[index], loaded[index - 1]));
        if (index == 1) {
            /* The third factory reads the independent saved tree AFTER its
             * original Entity/Stack/namespace owner have genuinely retired. */
            CHECK(mc_item_entity_stack_get(loaded[0], &kept) == MC_ENTITY_OK &&
                  kept == loaded[0]->stack);
            mc_entity_release(entity);
            entity = NULL;
            mc_item_stack_release(stack);
            stack = NULL;
            CHECK(retire_classes(&classes) == 0 && fc_saved_record(mc_nbt_tree_root(tree)));
        }
    }
    CHECK(mc_item_stack_count_set(loaded[0]->stack, 12) == MC_ITEM_STACK_OK);
    mc_item_stack_fields values;
    CHECK(mc_item_stack_fields_get(loaded[1]->stack, &values) == MC_ITEM_STACK_OK &&
          values.count == 4 && fc_saved_record(mc_nbt_tree_root(tree)));
    mc_entity_release(loaded[0]);
    loaded[0] = NULL;
    CHECK(mc_item_stack_fields_get(kept, &values) == MC_ITEM_STACK_OK && values.id == 17 &&
          values.count == 12 && values.damage == 5);
    mc_entity_class_snapshot state;
    mc_entity_registry_view names;
    CHECK(mc_entity_classes_get_snapshot(loaded_classes, &state) == MC_ENTITY_OK &&
          state.next_id == 3 && state.live_entities == 2 &&
          mc_entity_registry_inspect(loaded_classes, &names) == MC_ENTITY_REGISTRY_OK &&
          fc_ready_registry(&names));
    result = 0;
cleanup:
    mc_nbt_tree_free(bounded);
    mc_nbt_tree_free(leaf);
    mc_nbt_tree_free(tree);
    mc_item_stack_release(kept);
    for (size_t index = 0; index < 3; ++index)
        mc_entity_release(loaded[index]);
    mc_entity_release(entity);
    mc_item_stack_release(stack);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (retire_classes(&loaded_classes) != 0)
        result = 1;
    if (classes == NULL && loaded_classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int fc_frontiers(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *sentinel = NULL;
    fc_record record;
    mc_entity_nbt_error error, expected;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK &&
          mc_item_entity_allocate(classes, &sentinel, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct_world(sentinel, NULL, NULL) == MC_ENTITY_OK);
    mc_entity_registry_error named;
    CHECK(mc_entity_classes_initialize_registry(classes, &named) == MC_ENTITY_REGISTRY_OK);
    fc_guard guard;
    CHECK(fc_capture(sentinel, &guard));
    fc_record_init(&record, "Item");
    for (unsigned int argument = 0; argument < 2; ++argument) {
        mc_entity *out = sentinel;
        error = fc_poison();
        CHECK(mc_entity_factory_nbt(argument == 0 ? NULL : classes, NULL, &record.root,
                                    argument == 1 ? NULL : &out,
                                    &error) == MC_ENTITY_NBT_INVALID_ARGUMENT &&
              out == sentinel && fc_unchanged(sentinel, &guard));
        expected = fc_empty();
        expected.status = MC_ENTITY_NBT_INVALID_ARGUMENT;
        expected.operation = error.operation;
        CHECK(error.operation != NULL && fc_error_equal(&error, &expected));
    }
    const char *const unknown[] = {"NoSuchEntity", "", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"};
    for (size_t index = 0; index < 3; ++index) {
        fc_record_init(&record, unknown[index]);
        if (index == 1) { /* Absent id is the source empty String. */
            record.root.value.compound.items = &record.items[1];
            record.root.value.compound.length = 10;
        }
        mc_entity *out = sentinel;
        char bytes[80];
        size_t length = 0;
        mc_entity_nbt_status status = MC_ENTITY_NBT_INVALID_ARGUMENT;
        error = fc_poison();
        CHECK(fc_capture_call(classes, &record.root, &out, &error, &status, bytes, &length));
#ifdef _WIN32
        const char *message =
            index == 0   ? "Skipping Entity with id NoSuchEntity\r\n"
            : index == 1 ? "Skipping Entity with id \r\n"
                         : "Skipping Entity with id AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n";
#else
        const char *message =
            index == 0   ? "Skipping Entity with id NoSuchEntity\n"
            : index == 1 ? "Skipping Entity with id \n"
                         : "Skipping Entity with id AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
#endif
        CHECK(status == MC_ENTITY_NBT_OK && out == NULL && fc_fresh(&error) &&
              length == strlen(message) && memcmp(bytes, message, length) == 0 &&
              fc_unchanged(sentinel, &guard));
    }
    for (unsigned int control = 0; control < 2; ++control) {
        fc_record_init(&record, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
        if (control == 1) {
            record.class_name[0] = UINT16_C(0x00e9); /* Explicit native non-ASCII name control. */
            record.fields[0].value.string.length = 1;
        }
        mc_entity *out = sentinel;
        char bytes[80];
        size_t length = 0;
        mc_entity_nbt_status status = MC_ENTITY_NBT_OK;
        error = fc_poison();
        CHECK(fc_capture_call(classes, &record.root, &out, &error, &status, bytes, &length));
        CHECK(status == MC_ENTITY_NBT_UNSUPPORTED_TYPE && out == sentinel && length == 0 &&
              fc_unchanged(sentinel, &guard));
        expected = fc_empty();
        expected.status = MC_ENTITY_NBT_UNSUPPORTED_TYPE;
        expected.operation = error.operation;
        expected.field = "id";
        expected.factory_bytecode = 99;
        CHECK(error.operation != NULL && fc_error_equal(&error, &expected));
    }
    const char *const known[] = {"Skeleton", "Zombie", "Spider", "Mob"};
    for (size_t index = 0; index < sizeof(known) / sizeof(known[0]); ++index) {
        fc_record_init(&record, known[index]);
        mc_entity *out = sentinel;
        error = fc_poison();
        CHECK(mc_entity_factory_nbt(classes, NULL, &record.root, &out, &error) ==
                  MC_ENTITY_NBT_UNSUPPORTED_TYPE &&
              out == sentinel && fc_unchanged(sentinel, &guard));
        expected = fc_empty();
        expected.status = MC_ENTITY_NBT_UNSUPPORTED_TYPE;
        expected.operation = error.operation;
        expected.field = "id";
        expected.factory_bytecode = 45;
        CHECK(error.operation != NULL && fc_error_equal(&error, &expected));
    }
    for (unsigned int control = 0; control < 3; ++control) {
        fc_record_init(&record, "Item");
        if (control == 0)
            record.fields[0].type = MC_NBT_INT;
        if (control == 1)
            record.root.type = MC_NBT_LIST;
        mc_entity *out = sentinel;
        error = fc_poison();
        const mc_entity_nbt_status status =
            mc_entity_factory_nbt(classes, NULL, control == 2 ? NULL : &record.root, &out, &error);
        CHECK(status == (control == 2 ? MC_ENTITY_NBT_NULL_REFERENCE : MC_ENTITY_NBT_WRONG_TAG) &&
              out == sentinel && fc_unchanged(sentinel, &guard));
        expected = fc_empty();
        expected.status = status;
        expected.operation = error.operation;
        expected.field = "id";
        expected.factory_bytecode = 57;
        expected.name_bytecode = 8;
        CHECK(error.operation != NULL && fc_error_equal(&error, &expected));
    }
    for (unsigned int control = 0; control < 4; ++control) {
        fc_record_init(&record, "Item");
        if (control == 0)
            record.vectors[4].type = MC_NBT_FLOAT;
        if (control == 1)
            record.fields[5].type = MC_NBT_INT;
        if (control == 2)
            record.stack[1].type = MC_NBT_SHORT;
        if (control == 3)
            record.fields[1].value.list.length = 0;
        mc_entity_class_snapshot before, after;
        CHECK(mc_entity_classes_get_snapshot(classes, &before) == MC_ENTITY_OK);
        mc_java_random math = math_snapshot(profile);
        (void)mc_java_random_next_double(&math);
        const uint64_t unique = uniquifier(profile);
        mc_entity *out = sentinel;
        error = fc_poison();
        const mc_entity_nbt_status wanted = control == 2   ? MC_ENTITY_NBT_STACK_FAILED
                                            : control == 3 ? MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS
                                                           : MC_ENTITY_NBT_WRONG_TAG;
        mc_entity_class_snapshot expected_classes = before;
        ++expected_classes
              .next_id; /* Expected source result only; actual owner is never assigned. */
        CHECK(mc_entity_factory_nbt(classes, NULL, &record.root, &out, &error) == wanted &&
              out == sentinel && mc_entity_classes_get_snapshot(classes, &after) == MC_ENTITY_OK &&
              fc_classes_equal(&after, &expected_classes) &&
              uniquifier(profile) == advanced(unique, 1) && math_equal(profile, &math));
        expected = fc_empty();
        expected.status = wanted;
        expected.operation = error.operation;
        expected.factory_bytecode = 66;
        if (control == 0) {
            expected.base_bytecode = 43;
            expected.field = "Motion";
            expected.index = 1;
        }
        if (control == 1) {
            expected.base_bytecode = 255;
            expected.field = "Fire";
        }
        if (control == 2) {
            expected.base_bytecode = 311;
            expected.item_bytecode = 37;
            expected.stack_bytecode = 14;
            expected.field = "Count";
            expected.stack.status = MC_ITEM_STACK_INVALID_FIELD;
            expected.stack.field = "Count";
        }
        if (control == 3) {
            expected.base_bytecode = 129;
            expected.field = "Pos";
            expected.index = 0;
        }
        CHECK(error.operation != NULL && fc_error_equal(&error, &expected));
        fc_guard retained = guard;
        retained.classes = expected_classes;
        retained.math = math;
        retained.unique = advanced(unique, 1);
        CHECK(fc_unchanged(sentinel, &retained));
    }
    result = 0;
cleanup:
    mc_entity_release(sentinel);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int fc_constructor_frontier(mc_entity_source source, unsigned int control) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *sentinel = NULL, *out = NULL;
    mc_entity_nbt_error error, expected;
    fc_record record;
    const size_t namespace_bytes = fc_namespace_bytes();
    mc_entity_limits limit = {SIZE_MAX, SIZE_MAX};
    if (control == 1)
        limit.max_live_entities = 0;
    if (control == 2)
        limit.max_allocation_bytes = sizeof(mc_entity_classes) + namespace_bytes +
                                     sizeof(mc_entity) + sizeof(mc_entity_bounds_pool) +
                                     sizeof(mc_entity_bounds);
    CHECK(mc_java_random_profile_create(control == 0 ? sizeof(mc_java_random_profile)
                                                     : profile_budget(),
                                        &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(source, profile, &limit, &classes) == MC_ENTITY_OK);
    if (control == 0)
        CHECK(mc_item_entity_allocate(classes, &sentinel, NULL) == MC_ENTITY_OK);
    fc_record_init(&record, "Item");
    const uint64_t unique = uniquifier(profile);
    const unsigned int attempts = control == 0 ? 2u : 1u;
    for (unsigned int index = 0; index < attempts; ++index) {
        out = sentinel;
        error = fc_poison();
        const mc_entity_nbt_status wanted =
            control == 0 ? MC_ENTITY_NBT_ENTITY_FAILED : MC_ENTITY_NBT_LIMIT;
        CHECK(mc_entity_factory_nbt(classes, NULL, &record.root, &out, &error) == wanted &&
              out == sentinel);
        expected = fc_empty();
        expected.status = wanted;
        expected.operation = error.operation;
        expected.field = "id";
        expected.factory_bytecode = 45;
        if (control == 0) {
            expected.construction.status = MC_ENTITY_PROFILE_FAILED;
            expected.construction.operation = "item constructor";
            expected.construction.bytecode = 16;
            expected.construction.profile =
                index == 0 ? MC_JAVA_RANDOM_PROFILE_LIMIT : MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED;
        } else {
            expected.construction.status = MC_ENTITY_LIMIT;
            expected.construction.operation =
                control == 1 ? "item allocation" : "entity constructor";
            expected.construction.bytecode = control == 1 ? 0 : 123;
        }
        CHECK(error.operation != NULL && fc_error_equal(&error, &expected));
        mc_entity_class_snapshot state;
        mc_entity_registry_view registry;
        CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
              state.entity_class == MC_ENTITY_CLASS_READY &&
              state.next_id == (control == 1 ? 0 : (int32_t)index + 1) &&
              state.live_entities == (sentinel == NULL ? 0u : 1u) &&
              mc_entity_registry_inspect(classes, &registry) == MC_ENTITY_REGISTRY_OK &&
              fc_ready_registry(&registry));
        CHECK(uniquifier(profile) == advanced(unique, control == 0 ? index + 1u : 0u));
        if (control == 0) {
            CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_FAILED) && profile->math == NULL &&
                  mc_java_random_profile_allocation_bytes(profile) ==
                      sizeof(mc_java_random_profile) &&
                  state.bounds_class == MC_ENTITY_CLASS_READY &&
                  state.watcher_class == MC_ENTITY_CLASS_READY &&
                  state.allocation_bytes == sizeof(mc_entity_classes) + namespace_bytes +
                                                sizeof(mc_entity) + sizeof(mc_entity_bounds_pool) +
                                                sizeof(mc_entity_type_map) +
                                                7u * sizeof(mc_entity_type_entry));
            mc_entity_snapshot raw;
            CHECK(mc_entity_get_snapshot(sentinel, &raw) == MC_ENTITY_OK && raw_is(&raw, source));
        } else {
            CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL &&
                  state.watcher_class == MC_ENTITY_CLASS_COLD &&
                  state.bounds_class ==
                      (control == 1 ? MC_ENTITY_CLASS_COLD : MC_ENTITY_CLASS_READY) &&
                  state.allocation_bytes ==
                      sizeof(mc_entity_classes) + namespace_bytes +
                          (control == 1 ? 0u : sizeof(mc_entity_bounds_pool)));
        }
    }
    result = 0;
cleanup:
    mc_entity_release(sentinel);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int fc_cold_factory(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL;
    fc_record record;
    mc_entity_nbt_error error;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    const uint64_t unique = uniquifier(profile);
    mc_entity_registry_view registry;
    mc_entity_class_snapshot state;
    CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) &&
          mc_entity_registry_inspect(classes, &registry) == MC_ENTITY_REGISTRY_OK &&
          registry.phase == MC_ENTITY_CLASS_COLD);
    fc_record_init(&record, "Item");
    error = fc_poison();
    CHECK(mc_entity_factory_nbt(classes, NULL, &record.root, &entity, &error) == MC_ENTITY_NBT_OK &&
          entity != NULL && fc_fresh(&error) && fc_loaded(entity, source, 0) &&
          uniquifier(profile) == advanced(unique, 2) &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_READY) &&
          mc_java_random_profile_allocation_bytes(profile) == profile_budget());
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 1 &&
          state.live_entities == 1 &&
          state.allocation_bytes == fc_constructed_bytes() + fc_namespace_bytes() &&
          mc_entity_registry_inspect(classes, &registry) == MC_ENTITY_REGISTRY_OK &&
          fc_ready_registry(&registry));
    result = 0;
cleanup:
    mc_entity_release(entity);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("Entity NBT factory tests require the qualified strict SSE environment\n", stderr);
        return 1;
    }
    const mc_entity_nbt_error empty = mc_entity_nbt_empty_error();
    if (!fc_fresh(&empty))
        return 1;
    for (unsigned int side = 0; side < 2; ++side) {
        const mc_entity_source source = side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
        if (fc_cold_factory(source) || fc_save_roundtrip(source) || fc_frontiers(source))
            return 1;
        for (unsigned int control = 0; control < 3; ++control)
            if (fc_dead_namespace(source, control) || fc_constructor_frontier(source, control))
                return 1;
    }
    printf("PASS: %u Entity Boolean-save and factory checks\n", checks);
    return 0;
}
