/* Reuse real Registry/World/Store/Chunk setup and cleanup, not its old main.
 * Compile this translation unit alone; do not link test_entity_publication.c. */
#define main mc_entity_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/activation_internal.h"
#include "../../../../src/world/runtime/entity_publication_internal.h"

/* Constructed source contracts, not original/native replay fixtures. Local
 * typed views are explicitly borrowed controls, never installed in a Chunk or
 * adopted as an owning ledger. Actual b/D destinations always retain their
 * genuine constructor owners; temporary source controls are restored BEFORE
 * CHECK can branch to cleanup. No phase/count/hold/limit/RNG field is forged. */
typedef struct batch_mark {
    mc_entity_snapshot state;
    mc_java_random random;
} batch_mark;

static bool batch_mark_get(mc_entity *entity, batch_mark *mark) {
    if (mc_entity_get_snapshot(entity, &mark->state) != MC_ENTITY_OK)
        return false;
    if (mark->state.random != NULL)
        mark->random = *mark->state.random;
    return true;
}

static bool batch_entity_unchanged(mc_entity *entity, const batch_mark *mark) {
    mc_entity_snapshot now;
    return mc_entity_get_snapshot(entity, &now) == MC_ENTITY_OK && now.phase == mark->state.phase &&
           now.base_bytecode == mark->state.base_bytecode &&
           now.item_bytecode == mark->state.item_bytecode && now.base.id == mark->state.base.id &&
           now.base.world == mark->state.base.world && now.base.x == mark->state.base.x &&
           now.base.y == mark->state.base.y && now.base.z == mark->state.base.z &&
           now.base.bF == mark->state.base.bF && now.base.bG == mark->state.base.bG &&
           now.base.bH == mark->state.base.bH && now.base.bI == mark->state.base.bI &&
           now.bounds == mark->state.bounds && now.random == mark->state.random &&
           now.watcher == mark->state.watcher && now.stack == mark->state.stack &&
           (now.random == NULL || mc_test_java_random_equal(now.random, &mark->random));
}

static bool batch_clear(const mc_world_entity_batch_error *error) {
    return error->index == SIZE_MAX && error->publication.status == MC_WORLD_STORAGE_OK &&
           error->publication.bytecode == 0 && error->publication.world_access_bytecode == 0 &&
           error->publication.cause.status == MC_WORLD_STORAGE_OK &&
           error->publication.entity.status == MC_ENTITY_OK;
}

static bool batch_values(const mc_world_reference_list *list, size_t prefix, mc_entity *const *tail,
                         size_t count) {
    if (list == NULL || list->count != prefix + count)
        return false;
    for (size_t i = 0; i < count; ++i)
        if (list->items[prefix + i] != tail[i])
            return false;
    return true;
}

/* Fixture-held typed values/source storage remain live throughout. This is the
 * real native World call guard, with no WorldAccess/provider/allocator callback. */
static mc_world_storage_status batch_call(mc_world_runtime *world,
                                          const mc_world_chunk_entity_list *source, bool deferred,
                                          mc_world_entity_batch_error *error) {
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        *error = mc_world_entity_batch_empty_error();
        error->publication.status = status;
        error->publication.cause = cause;
        return status;
    }
    status = mc_world_runtime_entity_batch_held(world, source, deferred, error);
    mc_world_runtime_call_leave(world);
    return status;
}

static bool batch_actual_order(fixture *f, size_t side, mc_entity **primary) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_entity *a = NULL, *b = NULL;
    CHECK(make_entity(f, side, w, 1.5, 16.0, 1.5, &a) &&
          make_entity(f, side, NULL, 2.5, 16.0, 2.5, &b));
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[side], a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[side], a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[side], b, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity *once[] = {a, a, b};
    mc_entity *twice[] = {a, a, b, a, a, b};
    CHECK(slice_is(f, side, 1, once, COUNT(once)) && world_is(f, side, NULL, 0) && a->holds == 3 &&
          b->holds == 2);
    batch_mark a_before, b_before;
    CHECK(batch_mark_get(a, &a_before) && batch_mark_get(b, &b_before));
    mc_world_reference_list *const actual_u = w->fields.u;
    mc_world_activation_error detail;
    w->fields.u = NULL;
    const mc_world_storage_status failed =
        mc_world_runtime_register_chunk_entities(f->chunks[side], 1, &detail);
    w->fields.u = actual_u;
    CHECK(failed == MC_WORLD_STORAGE_UNSUPPORTED_STATE && detail.bytecode == 42 &&
          detail.world_bytecode == 34 && detail.batch.index == 0 &&
          detail.batch.publication.bytecode == 34 &&
          detail.batch.publication.world_access_bytecode == 7 &&
          detail.batch.publication.entity.status == MC_ENTITY_OK &&
          batch_values(w->fields.b, 0, once, COUNT(once)) && a->holds == 5 && b->holds == 3);
    /* A per-Entity insertion/WorldAccess loop would have appended only a here.
     * The second actual call appends another WHOLE batch; no rollback/retry. */
    CHECK(mc_world_runtime_register_chunk_entities(f->chunks[side], 1, &detail) ==
              MC_WORLD_STORAGE_OK &&
          batch_clear(&detail.batch) && batch_values(w->fields.b, 0, twice, COUNT(twice)) &&
          a->holds == 7 && b->holds == 4);
    w->fields.u = NULL; /* Deferral must not invoke the WorldAccess at all. */
    const mc_world_storage_status deferred =
        mc_world_runtime_defer_chunk_entities(f->chunks[side], 1, &detail);
    w->fields.u = actual_u;
    CHECK(deferred == MC_WORLD_STORAGE_OK && batch_clear(&detail.batch) &&
          batch_values(w->fields.D, 0, once, COUNT(once)) && a->holds == 9 && b->holds == 5);
    CHECK(mc_world_runtime_defer_chunk_entities(f->chunks[side], 1, &detail) ==
              MC_WORLD_STORAGE_OK &&
          batch_values(w->fields.D, 0, twice, COUNT(twice)) && a->holds == 11 && b->holds == 6 &&
          batch_entity_unchanged(a, &a_before) && batch_entity_unchanged(b, &b_before) &&
          slice_is(f, side, 1, once, COUNT(once)) && mc_world_runtime_entity_list_valid(w));
    *primary = a;
    ok = true;
cleanup:
    return ok;
}

static bool batch_cross_world_raw(fixture *f, size_t side) {
    bool ok = false;
    const size_t source_slot = side + 2u, target_slot = side;
    mc_world_runtime *target = f->worlds[target_slot];
    mc_entity *raw = NULL, *moved = NULL;
    CHECK(f->entity_count < COUNT(f->entities));
    CHECK(mc_item_entity_allocate(f->entity_classes[side], &raw, NULL) == MC_ENTITY_OK);
    f->entities[f->entity_count++] = raw;
    CHECK(make_entity(f, side, f->worlds[source_slot], 1.5, 0.0, 1.5, &moved));
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[source_slot], raw, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[source_slot], raw, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[source_slot], moved, NULL) ==
              MC_WORLD_STORAGE_OK);
    /* Genuine position method after insertion: source membership remains in
     * Chunk0 while current position is far outside it. Batch must not relocate. */
    CHECK(mc_entity_set_position(moved, -513.5, 64.5, 8192.125) == MC_ENTITY_OK);
    mc_entity *expected[] = {raw, raw, moved};
    batch_mark raw_before, moved_before;
    CHECK(slice_is(f, source_slot, 0, expected, COUNT(expected)) &&
          batch_mark_get(raw, &raw_before) && raw_before.state.phase == MC_ENTITY_RAW &&
          raw_before.state.base.world == NULL && raw_before.state.random == NULL &&
          batch_mark_get(moved, &moved_before));
    const size_t b_count = target->fields.b->count, d_count = target->fields.D->count;
    void *b_prefix[6], *d_prefix[6];
    CHECK(b_count == COUNT(b_prefix) && d_count == COUNT(d_prefix));
    memcpy(b_prefix, target->fields.b->items, sizeof(b_prefix));
    memcpy(d_prefix, target->fields.D->items, sizeof(d_prefix));
    const size_t raw_holds = raw->holds, moved_holds = moved->holds;
    mc_world_entity_batch_error error;
    CHECK(batch_call(target, f->slices[source_slot][0], false, &error) == MC_WORLD_STORAGE_OK &&
          batch_clear(&error) &&
          batch_values(target->fields.b, b_count, expected, COUNT(expected)) &&
          raw->holds == raw_holds + 2u && moved->holds == moved_holds + 1u);
    CHECK(
        batch_call(target, f->slices[source_slot][0], true, &error) == MC_WORLD_STORAGE_OK &&
        batch_clear(&error) && batch_values(target->fields.D, d_count, expected, COUNT(expected)) &&
        raw->holds == raw_holds + 4u && moved->holds == moved_holds + 2u &&
        memcmp(b_prefix, target->fields.b->items, sizeof(b_prefix)) == 0 &&
        memcmp(d_prefix, target->fields.D->items, sizeof(d_prefix)) == 0 &&
        batch_entity_unchanged(raw, &raw_before) && batch_entity_unchanged(moved, &moved_before) &&
        target->classes->entities == raw->classes && moved_before.state.base.world != target &&
        mc_world_runtime_entity_list_valid(target));
    ok = true;
cleanup:
    return ok;
}

static bool batch_nullable_control(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side + 2u];
    mc_entity *entity = NULL;
    CHECK(make_entity(f, side, NULL, -8192.5, 400.0, 8192.5, &entity));
    batch_mark before;
    CHECK(batch_mark_get(entity, &before) && !before.state.base.bF && entity->holds == 1);
    mc_entity *values[] = {entity, NULL, entity};
    const mc_world_chunk_entity_list borrowed = {values, COUNT(values), COUNT(values)};
    mc_world_entity_batch_error error = {.index = 1};
    CHECK(batch_call(w, &borrowed, false, &error) == MC_WORLD_STORAGE_OK && batch_clear(&error) &&
          batch_values(w->fields.b, 0, values, COUNT(values)) && entity->holds == 3);
    CHECK(batch_call(w, &borrowed, true, &error) == MC_WORLD_STORAGE_OK && batch_clear(&error) &&
          batch_values(w->fields.D, 0, values, COUNT(values)) && entity->holds == 5 &&
          mc_world_runtime_entity_list_valid(w));
    void *world_access_items[] = {NULL};
    mc_world_reference_list world_access = {world_access_items, COUNT(world_access_items),
                                            COUNT(world_access_items)};
    mc_world_reference_list *const actual_u = w->fields.u;
    w->fields.u = &world_access;
    const mc_world_storage_status failed = batch_call(w, &borrowed, false, &error);
    w->fields.u = actual_u;
    CHECK(failed == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.index == 0 &&
          error.publication.bytecode == 34 && error.publication.world_access_bytecode == 29 &&
          batch_values(w->fields.b, 3, values, COUNT(values)) && entity->holds == 7 &&
          batch_entity_unchanged(entity, &before));
    int nonobserver = 1;
    world_access_items[0] = &nonobserver;
    w->fields.u = &world_access;
    const mc_world_storage_status cast = batch_call(w, &borrowed, false, &error);
    w->fields.u = actual_u;
    CHECK(cast == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.index == 0 &&
          error.publication.bytecode == 34 && error.publication.world_access_bytecode == 25 &&
          batch_values(w->fields.b, 6, values, COUNT(values)) && entity->holds == 9 &&
          batch_entity_unchanged(entity, &before) && mc_world_runtime_entity_list_valid(w));
    ok = true;
cleanup:
    return ok;
}

static bool batch_limits_and_empty_frontier(fixture *f, size_t side, mc_entity *primary) {
    bool ok = false;
    mc_world_runtime *limited = f->worlds[side + 4u], *w = f->worlds[side + 6u];
    const size_t holds = primary->holds;
    const size_t bytes = limited->construction.direct_allocation_bytes;
    CHECK(bytes == limited->construction_limits.max_direct_allocation_bytes);
    mc_world_entity_batch_error error;
    CHECK(batch_call(limited, f->slices[side][1], false, &error) == MC_WORLD_STORAGE_LIMIT &&
          error.index == SIZE_MAX && error.publication.bytecode == 5 &&
          limited->fields.b->count == 0 && limited->fields.b->capacity == 0 &&
          primary->holds == holds && limited->construction.direct_allocation_bytes == bytes);
    CHECK(batch_call(limited, f->slices[side][1], true, &error) == MC_WORLD_STORAGE_LIMIT &&
          error.index == SIZE_MAX && error.publication.bytecode == 5 &&
          limited->fields.D->count == 0 && limited->fields.D->capacity == 0 &&
          primary->holds == holds && limited->construction.direct_allocation_bytes == bytes);
    CHECK(batch_call(limited, f->slices[side][7], false, &error) == MC_WORLD_STORAGE_OK &&
          batch_clear(&error) && limited->construction.direct_allocation_bytes == bytes);

    int untyped = 123;
    void *items[] = {&untyped};
    mc_world_reference_list foreign = {items, COUNT(items), COUNT(items)};
    mc_world_reference_list *const actual_b = w->fields.b, *const actual_d = w->fields.D;
    mc_world_reference_list *const actual_u = w->fields.u;
    const size_t w_bytes = w->construction.direct_allocation_bytes;
    w->fields.b = &foreign;
    w->fields.u = NULL;
    const mc_world_storage_status empty = batch_call(w, f->slices[side][7], false, &error);
    w->fields.b = actual_b;
    w->fields.u = actual_u;
    CHECK(empty == MC_WORLD_STORAGE_OK && batch_clear(&error) && foreign.count == 1 &&
          foreign.items[0] == &untyped && actual_b->count == 0 && primary->holds == holds &&
          w->construction.direct_allocation_bytes == w_bytes);
    w->fields.b = &foreign;
    const mc_world_storage_status refused = batch_call(w, f->slices[side][1], false, &error);
    w->fields.b = actual_b;
    CHECK(refused == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.index == SIZE_MAX &&
          error.publication.bytecode == 5 && foreign.count == 1 && foreign.items[0] == &untyped &&
          actual_b->count == 0 && primary->holds == holds);
    w->fields.D = &foreign;
    const mc_world_storage_status d_refused = batch_call(w, f->slices[side][1], true, &error);
    w->fields.D = actual_d;
    CHECK(d_refused == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.index == SIZE_MAX &&
          error.publication.bytecode == 5 && actual_d->count == 0 && foreign.count == 1);
    w->fields.b = NULL;
    const mc_world_storage_status null_destination = batch_call(w, NULL, false, &error);
    w->fields.b = actual_b;
    CHECK(null_destination == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.index == SIZE_MAX &&
          error.publication.bytecode == 5 && actual_b->count == 0);
    CHECK(batch_call(w, NULL, true, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.index == SIZE_MAX && error.publication.bytecode == 5 && actual_d->count == 0 &&
          mc_world_runtime_entity_list_valid(w) && primary->holds == holds &&
          w->construction.direct_allocation_bytes == w_bytes);
    ok = true;
cleanup:
    return ok;
}

static bool batch_combined_retirement(fixture *f, size_t side, mc_entity *primary) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    const size_t b_count = w->fields.b->count, d_count = w->fields.D->count;
    const size_t holds = primary->holds, bytes = w->construction.direct_allocation_bytes;
    CHECK(b_count != 0 && d_count != 0 && holds == 11);
    CHECK(mc_world_store_lease_chunk(f->stores[side], f->states[side], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->leased[side] = true;
    const mc_world_storage_status blocked = mc_world_runtime_retire_entity_containers(w, NULL);
    const mc_world_storage_status released =
        mc_world_store_release_chunk(f->stores[side], f->states[side], NULL);
    if (released == MC_WORLD_STORAGE_OK)
        f->leased[side] = false;
    CHECK(blocked == MC_WORLD_STORAGE_BUSY && released == MC_WORLD_STORAGE_OK &&
          primary->holds == holds && w->fields.b->count == b_count &&
          w->fields.D->count == d_count && !w->entity_containers_retired &&
          w->construction.direct_allocation_bytes == bytes);
    int untyped = 1;
    void *items[] = {&untyped};
    mc_world_reference_list unknown = {items, COUNT(items), COUNT(items)};
    mc_world_reference_list *const actual_u = w->fields.u;
    w->fields.u = &unknown;
    const mc_world_storage_status admission = mc_world_runtime_retire_entity_containers(w, NULL);
    w->fields.u = actual_u;
    CHECK(admission == MC_WORLD_STORAGE_UNSUPPORTED_STATE && primary->holds == holds &&
          w->fields.b->count == b_count && w->fields.D->count == d_count &&
          !w->entity_containers_retired && w->construction.direct_allocation_bytes == bytes);
    batch_mark before;
    CHECK(batch_mark_get(primary, &before));
    CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_OK);
    f->terminal[side] = true;
    CHECK(w->entity_containers_retired && w->fields.b->count == 0 && w->fields.D->count == 0 &&
          primary->holds == 1 && batch_entity_unchanged(primary, &before) &&
          mc_world_runtime_entity_list_valid(w) && f->states[side]->active &&
          f->states[side]->has_entities);
    mc_world_activation_error activation;
    CHECK(mc_world_runtime_register_chunk_entities(f->chunks[side], 1, &activation) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          batch_clear(&activation.batch) && primary->holds == 1 && w->fields.D->count == 0);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'B', 'a', 't', 'c', 'h'};
    if (argc != 3 || strcmp(argv[1], "world-entity-batch") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(open_store(f, slot, root) && create_world(f, slot));
    mc_entity *primary[2] = {0};
    for (size_t side = 0; side < 2; ++side)
        CHECK(batch_actual_order(f, side, &primary[side]));
    for (size_t side = 0; side < 2; ++side)
        CHECK(batch_cross_world_raw(f, side) && batch_nullable_control(f, side) &&
              batch_limits_and_empty_frontier(f, side, primary[side]));
    for (size_t side = 0; side < 2; ++side)
        CHECK(batch_combined_retirement(f, side, primary[side]));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("batch cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u World Entity batch checks (world-entity-batch)\n", checks) < 0)
        result = 1;
    return result;
}
