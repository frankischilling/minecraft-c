#define main mc_class_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/collision/class_entities_internal.h"
#include "mc/world/runtime/collision/class_entities.h"

/* Native contracts with actual loaded Chunks and Item/Falling constructors.
 * Temporary nullable source references are restored before every assertion.
 * No fake Class, actor, result, provider or List storage is supplied. */
static size_t class_header_bytes[2];
static const mc_entity_bounds class_box = {2.0, 0.0, 2.0, 13.0, 31.0, 13.0};
static const mc_entity_bounds class_miss = {40.0, 0.0, 40.0, 41.0, 31.0, 41.0};

static bool class_world(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime_construction state = {0};
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    if (slot >= 4)
        limits.max_direct_allocation_bytes =
            f->baseline_world_bytes[slot % 2u] +
            (slot < 6 ? class_header_bytes[slot % 2u] + 8u * (sizeof(void *) + sizeof(mc_entity *))
                      : 0u);
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(slot % 2u == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                         : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[slot % 2u], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    if (slot < 2)
        f->baseline_world_bytes[slot] = state.direct_allocation_bytes;
    if (slot >= 4)
        CHECK(state.direct_allocation_bytes == f->baseline_world_bytes[slot % 2u]);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(seed_record(f, slot));
    mc_chunk_state *loaded = NULL;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get(f->worlds[slot], 0, 0, &loaded);
    /* Record a genuine published owner even if a later activation failed. */
    CHECK(mc_chunk_cache_validate(f->providers[slot].cache));
    if (f->providers[slot].cache->insertion_count == 1)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(acquired == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded == f->states[slot] &&
          loaded->active && loaded->terrain_populated && !loaded->dirty && loaded->x == 0 &&
          loaded->z == 0);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], loaded);
    mc_world_chunk_fields fields = {0};
    CHECK(f->chunks[slot] != NULL &&
          mc_world_chunk_fields_get(f->chunks[slot], &fields) == MC_WORLD_STORAGE_OK &&
          fields.d == f->worlds[slot] && !fields.q && f->providers[slot].cache->count == 1 &&
          f->providers[slot].cache->insertion_count == 1 &&
          mc_world_store_chunk_count(f->stores[slot]) == 1);
    bool distinct = true;
    for (size_t i = 0; i < 8; ++i) {
        CHECK(mc_world_chunk_slice_at(fields.m, i, &f->slices[slot][i]) == MC_WORLD_STORAGE_OK &&
              f->slices[slot][i] != NULL);
        for (size_t j = 0; j < i; ++j)
            if (f->slices[slot][i] == f->slices[slot][j])
                distinct = false;
    }
    CHECK(distinct);
    f->world_lists[slot] = f->worlds[slot]->fields.b;
    CHECK(f->world_lists[slot] != NULL && f->world_lists[slot]->count == 0);
    f->initial_chunk_bytes[slot] = mc_world_chunk_allocation_bytes(f->chunks[slot]);
    ok = true;
cleanup:
    return ok;
}

static bool class_values(const mc_world_class_entity_list *list, mc_entity *const *expected,
                         size_t count) {
    size_t got = SIZE_MAX;
    if (mc_world_class_entity_list_count(list, &got) != MC_WORLD_STORAGE_OK || got != count)
        return false;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        if (mc_world_class_entity_list_get(list, i, &entity) != MC_WORLD_STORAGE_OK ||
            entity != expected[i])
            return false;
    }
    return true;
}

static bool class_query_is(mc_world_runtime *world, const mc_entity_class_literal *target,
                           const mc_entity_bounds *bounds, mc_entity *const *expected,
                           size_t count) {
    mc_world_class_entity_list *list = NULL;
    mc_world_nearby_error error = mc_world_nearby_empty_error();
    const mc_world_storage_status status =
        mc_world_runtime_class_entities(world, target, bounds, &list, &error);
    const bool same = status == MC_WORLD_STORAGE_OK && class_values(list, expected, count) &&
                      error.query.bytecode == 0 && error.query.chunk_bytecode == 0 &&
                      error.slice == SIZE_MAX && error.index == SIZE_MAX &&
                      error.entity.status == MC_ENTITY_OK;
    mc_world_class_entity_list_release(list);
    return same;
}

static mc_world_storage_status class_gather(mc_world_runtime *world, mc_chunk_state *state,
                                            const mc_entity_class_literal *target,
                                            const mc_entity_bounds *bounds,
                                            mc_world_class_entity_list *destination,
                                            mc_world_nearby_error *error) {
    *error = mc_world_nearby_empty_error();
    mc_world_storage_status status = mc_world_runtime_call_enter(world, NULL);
    if (status == MC_WORLD_STORAGE_OK) {
        status =
            mc_world_class_entities_gather_held(world, state, target, bounds, destination, error);
        mc_world_runtime_call_leave(world);
    }
    return status;
}

static bool class_falling(fixture *f, size_t side, mc_entity **out) {
    bool ok = false;
    mc_entity *entity = NULL;
    CHECK(f->entity_count < COUNT(f->entities) &&
          mc_falling_entity_allocate(f->entity_classes[side], &entity, NULL) == MC_ENTITY_OK);
    f->entities[f->entity_count++] = entity;
    CHECK(mc_falling_entity_construct(entity, NULL, 8.0, 16.0, 8.0, 12, NULL) == MC_ENTITY_OK);
    *out = entity;
    ok = true;
cleanup:
    return ok;
}

static bool class_empty_ownership(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_world_class_entity_list *first = NULL, *second = NULL;
    const size_t bytes = w->construction.direct_allocation_bytes;
    mc_world_nearby_error error;
    CHECK(mc_world_runtime_class_entities(w, NULL, &class_box, &first, &error) ==
              MC_WORLD_STORAGE_OK &&
          class_values(first, NULL, 0));
    class_header_bytes[side] = w->construction.direct_allocation_bytes - bytes;
    CHECK(class_header_bytes[side] != 0 &&
          mc_world_runtime_class_entities(w, NULL, &class_box, &second, &error) ==
              MC_WORLD_STORAGE_OK &&
          class_values(second, NULL, 0) && first != second &&
          mc_world_class_entity_list_identity(first) !=
              mc_world_class_entity_list_identity(second) &&
          mc_world_class_entity_list_identity(first) != w->fields.R &&
          mc_world_class_entity_list_identity(second) != w->fields.R && w->local_leases == 2 &&
          w->construction.direct_allocation_bytes == bytes + 2u * class_header_bytes[side]);
    CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY);
    size_t untouched = 123;
    CHECK(mc_world_class_entity_list_count(NULL, &untouched) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          untouched == 123 &&
          mc_world_class_entity_list_count(first, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_world_class_entity_list_identity(NULL) == NULL);
    mc_world_class_entity_list_release(first);
    first = NULL;
    CHECK(w->local_leases == 1 && class_values(second, NULL, 0) &&
          w->construction.direct_allocation_bytes == bytes + class_header_bytes[side]);
    mc_world_class_entity_list_release(second);
    second = NULL;
    mc_world_class_entity_list_release(NULL);
    CHECK(w->local_leases == 0 && w->construction.direct_allocation_bytes == bytes);
    ok = true;
cleanup:
    mc_world_class_entity_list_release(first);
    mc_world_class_entity_list_release(second);
    return ok;
}

static bool class_order_and_filter(fixture *f, size_t side) {
    bool ok = false, inserted = false;
    mc_world_runtime *w = f->worlds[side];
    mc_entity *item = NULL, *falling = NULL;
    mc_world_class_entity_list *all = NULL, *items = NULL;
    mc_entity_classes *foreign = NULL;
    mc_world_entity_list_view rview = {0};
    const mc_entity_class_literal *entity_class = NULL, *item_class = NULL, *falling_class = NULL;
    const mc_entity_class_literal *mob = NULL, *player = NULL, *minecart = NULL,
                                  *foreign_item = NULL;
    CHECK(mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_ENTITY,
                                      &entity_class) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_ITEM,
                                      &item_class) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_FALLING_SAND,
                                      &falling_class) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
                                      &mob) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_PLAYER,
                                      &player) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_MINECART,
                                      &minecart) == MC_ENTITY_OK);
    CHECK(make_entity(f, side, w, 8.0, 8.0, 8.0, &item) && class_falling(f, side, &falling));
    CHECK(mc_world_runtime_insert_entity(w, item, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted &&
          mc_world_runtime_chunk_add_entity(f->chunks[side], item, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_insert_entity(w, falling, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted);
    mc_entity *all_values[] = {item, item, falling};
    mc_entity *item_values[] = {item, item};
    mc_entity *falling_values[] = {falling};
    mc_world_nearby_error error;
    CHECK(mc_world_runtime_nearby_entities(w, NULL, &class_box, &rview, &error) ==
              MC_WORLD_STORAGE_OK &&
          rview.identity == w->fields.R && w->fields.R->count == 3 &&
          w->fields.R->items[0] == item && w->fields.R->items[1] == item &&
          w->fields.R->items[2] == falling);
    const mc_world_reference_list *const ridentity = w->fields.R;
    void **const ritems = ridentity->items;
    const size_t item_holds = item->holds, falling_holds = falling->holds;
    const size_t bytes = w->construction.direct_allocation_bytes;
    const mc_java_random random = *item->random;
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->entity_classes[side], &before) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_class_entities(w, entity_class, &class_box, &all, &error) ==
              MC_WORLD_STORAGE_OK &&
          class_values(all, all_values, COUNT(all_values)) &&
          mc_world_runtime_class_entities(w, item_class, &class_box, &items, &error) ==
              MC_WORLD_STORAGE_OK &&
          class_values(items, item_values, COUNT(item_values)) &&
          mc_world_class_entity_list_identity(all) != ridentity &&
          mc_world_class_entity_list_identity(items) != ridentity &&
          mc_world_class_entity_list_identity(all) != mc_world_class_entity_list_identity(items) &&
          item->holds == item_holds + 4 && falling->holds == falling_holds + 1 &&
          w->local_leases == 3);
    CHECK(class_query_is(w, falling_class, &class_box, falling_values, 1) &&
          class_query_is(w, mob, &class_box, NULL, 0) &&
          class_query_is(w, player, &class_box, NULL, 0) &&
          class_query_is(w, minecart, &class_box, NULL, 0));
    CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                   NULL, &foreign) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(foreign, MC_ENTITY_CLASS_KIND_ITEM, &foreign_item) ==
              MC_ENTITY_OK &&
          foreign_item != item_class && class_query_is(w, foreign_item, &class_box, NULL, 0));
    CHECK(w->fields.R == ridentity && ridentity->items == ritems && ridentity->count == 3 &&
          ridentity->items[0] == item && ridentity->items[1] == item &&
          ridentity->items[2] == falling && class_values(all, all_values, 3) &&
          class_values(items, item_values, 2) && item->holds == item_holds + 4 &&
          falling->holds == falling_holds + 1 && mc_test_java_random_equal(item->random, &random));
    mc_entity *sentinel = falling;
    CHECK(mc_world_class_entity_list_get(items, 2, &sentinel) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          sentinel == falling &&
          mc_world_class_entity_list_get(items, 0, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    mc_world_entity_list_view_release(&rview);
    CHECK(mc_world_runtime_nearby_entities(w, NULL, &class_miss, &rview, &error) ==
              MC_WORLD_STORAGE_OK &&
          ridentity->count == 0 && class_values(all, all_values, 3) &&
          class_values(items, item_values, 2) && item->holds == item_holds + 2 &&
          falling->holds == falling_holds);
    mc_world_class_entity_list_release(all);
    all = NULL;
    mc_world_class_entity_list_release(items);
    items = NULL;
    mc_world_entity_list_view_release(&rview);
    CHECK(item->holds == item_holds - 2 && falling->holds == falling_holds - 1 &&
          w->construction.direct_allocation_bytes == bytes && w->local_leases == 0 &&
          w->active_calls == 0 &&
          mc_entity_classes_get_snapshot(f->entity_classes[side], &after) == MC_ENTITY_OK &&
          before.entity_class == after.entity_class && before.bounds_class == after.bounds_class &&
          before.watcher_class == after.watcher_class && before.next_id == after.next_id &&
          before.bounds_pool_cursor == after.bounds_pool_cursor &&
          before.allocation_bytes == after.allocation_bytes);
    ok = true;
cleanup:
    mc_world_class_entity_list_release(all);
    mc_world_class_entity_list_release(items);
    mc_world_entity_list_view_release(&rview);
    if (foreign != NULL && mc_entity_classes_free(foreign) != MC_ENTITY_OK)
        ok = false;
    return ok;
}

static bool class_null_order_and_prefix(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *a = NULL, *b = NULL;
    mc_world_class_entity_list *destination = NULL, *foreign = NULL;
    const mc_entity_class_literal *item = NULL, *mob = NULL;
    CHECK(mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_ITEM, &item) ==
              MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
                                      &mob) == MC_ENTITY_OK);
    CHECK(make_entity(f, side, NULL, 8.0, 8.0, 8.0, &a) &&
          make_entity(f, side, NULL, 8.0, 16.0, 8.0, &b) &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], b, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_nearby_error error = mc_world_nearby_empty_error();
    CHECK(mc_world_runtime_class_entities(w, mob, &class_box, &destination, &error) ==
              MC_WORLD_STORAGE_OK &&
          class_values(destination, NULL, 0) &&
          mc_world_runtime_class_entities(f->worlds[side], mob, &class_box, &foreign, &error) ==
              MC_WORLD_STORAGE_OK &&
          class_values(foreign, NULL, 0));
    mc_world_class_entity_list *unchanged = destination; /* Non-owning out sentinel. */
    const size_t bytes = w->construction.direct_allocation_bytes;
    CHECK(mc_world_runtime_class_entities(w, NULL, NULL, &unchanged, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.bytecode == 1 && unchanged == destination &&
          w->construction.direct_allocation_bytes == bytes);
    error = mc_world_nearby_empty_error();
    CHECK(w->fallback.blank &&
          mc_world_class_entities_gather_held(NULL, &w->fallback, NULL, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_OK &&
          error.query.chunk_bytecode == 0);
    CHECK(class_gather(w, f->states[slot], NULL, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 1);
    CHECK(class_gather(w, f->states[slot], NULL, &class_box, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 116 && error.entity.status == MC_ENTITY_STATE_ERROR &&
          error.slice == 0 && error.index == 0);
    mc_world_chunk_entity_list *const slice = f->chunks[slot]->m->items[0];
    mc_entity *const actual = slice->items[0];
    slice->items[0] = NULL;
    const mc_world_storage_status null_candidate =
        class_gather(w, f->states[slot], NULL, &class_box, NULL, &error);
    slice->items[0] = actual;
    CHECK(null_candidate == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 113 && error.entity.status == MC_ENTITY_STATE_ERROR &&
          error.slice == 0 && error.index == 0 && a->holds == 2 && b->holds == 2);
    mc_entity_bounds *const a_bounds = a->bounds;
    a->bounds = NULL;
    const mc_world_storage_status nonmatch =
        class_gather(w, f->states[slot], mob, &class_box, NULL, &error);
    a->bounds = a_bounds;
    CHECK(nonmatch == MC_WORLD_STORAGE_OK && class_values(destination, NULL, 0));
    a->bounds = NULL;
    const mc_world_storage_status matching =
        class_gather(w, f->states[slot], item, &class_box, destination, &error);
    a->bounds = a_bounds;
    CHECK(matching == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.chunk_bytecode == 128 &&
          error.slice == 0 && error.index == 0 && class_values(destination, NULL, 0));
    CHECK(class_gather(w, f->states[slot], item, &class_box, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 137 &&
          class_gather(w, f->states[slot], item, &class_box, foreign, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 137 && class_values(foreign, NULL, 0));
    mc_entity_bounds *const b_bounds = b->bounds;
    b->bounds = NULL;
    const mc_world_storage_status partial =
        class_gather(w, f->states[slot], item, &class_box, destination, &error);
    b->bounds = b_bounds;
    mc_entity *one[] = {a};
    CHECK(partial == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.chunk_bytecode == 128 &&
          error.slice == 1 && error.index == 0 && class_values(destination, one, 1) &&
          a->holds == 3 && b->holds == 2);
    const size_t partial_bytes = w->construction.direct_allocation_bytes;
    b->bounds = NULL;
    const mc_world_storage_status failed_query =
        mc_world_runtime_class_entities(w, item, &class_box, &unchanged, &error);
    b->bounds = b_bounds;
    CHECK(failed_query == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.bytecode == 120 &&
          error.query.chunk_bytecode == 128 && error.slice == 1 && error.index == 0 &&
          unchanged == destination && class_values(destination, one, 1) && a->holds == 3 &&
          b->holds == 2 && w->construction.direct_allocation_bytes == partial_bytes &&
          w->local_leases == 1);
    mc_world_chunk_list_array *const actual_m = f->chunks[slot]->m;
    f->chunks[slot]->m = NULL;
    const mc_world_storage_status null_m =
        class_gather(w, f->states[slot], item, &class_box, destination, &error);
    f->chunks[slot]->m = actual_m;
    CHECK(null_m == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.chunk_bytecode == 48 &&
          class_values(destination, one, 1));
    mc_chunk_provider *const provider = w->fields.v;
    w->fields.v = NULL;
    const mc_world_storage_status null_provider =
        mc_world_runtime_class_entities(w, item, &class_box, &unchanged, &error);
    w->fields.v = provider;
    CHECK(null_provider == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.bytecode == 102 &&
          error.query.provider_bytecode == 6 && unchanged == destination &&
          w->construction.direct_allocation_bytes == partial_bytes &&
          f->providers[slot].cache->count == 1 && f->providers[slot].cache->insertion_count == 1);
    CHECK(class_query_is(w, NULL, &class_miss, NULL, 0) &&
          class_gather(w, f->states[slot], NULL, &class_miss, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 116);
    mc_world_class_entity_list_release(destination);
    destination = NULL;
    mc_world_class_entity_list_release(foreign);
    foreign = NULL;
    CHECK(w->local_leases == 0 && w->active_calls == 0 && a->holds == 2 && b->holds == 2 &&
          w->fields.R->count == 0 &&
          w->construction.direct_allocation_bytes == bytes - class_header_bytes[side]);
    ok = true;
cleanup:
    mc_world_class_entity_list_release(destination);
    mc_world_class_entity_list_release(foreign);
    return ok;
}

static bool class_native_budget(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 4u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *a = NULL;
    mc_world_class_entity_list *out = NULL, *empty = NULL;
    const mc_entity_class_literal *item = NULL, *mob = NULL;
    CHECK(make_entity(f, side, NULL, 8.0, 8.0, 8.0, &a) &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_ITEM, &item) ==
              MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
                                      &mob) == MC_ENTITY_OK);
    for (size_t i = 0; i < 5; ++i)
        CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], a, NULL) == MC_WORLD_STORAGE_OK);
    const size_t bytes = w->construction.direct_allocation_bytes;
    mc_world_nearby_error error;
    /* Capacity8 alone fits the limit. Growing from capacity4 must charge both
     * old and new arrays until publication, so the fifth append still fails. */
    CHECK(mc_world_runtime_class_entities(w, item, &class_box, &out, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          out == NULL && error.query.bytecode == 120 && error.query.chunk_bytecode == 137 &&
          error.slice == 0 && error.index == 4 && error.entity.status == MC_ENTITY_OK &&
          a->holds == 6 && w->construction.direct_allocation_bytes == bytes &&
          w->local_leases == 0 && w->active_calls == 0 && w->fields.R->count == 0);
    CHECK(mc_world_runtime_class_entities(w, mob, &class_box, &empty, &error) ==
              MC_WORLD_STORAGE_OK &&
          class_values(empty, NULL, 0) &&
          w->construction.direct_allocation_bytes == bytes + class_header_bytes[side] &&
          a->holds == 6 && w->local_leases == 1);
    mc_world_class_entity_list_release(empty);
    empty = NULL;
    CHECK(w->construction.direct_allocation_bytes == bytes && w->local_leases == 0);
    CHECK(mc_world_runtime_class_entities(f->worlds[side + 6u], NULL, &class_miss, &out, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          out == NULL && error.query.bytecode == 67 && error.query.chunk_bytecode == 0 &&
          f->worlds[side + 6u]->local_leases == 0 &&
          f->worlds[side + 6u]->construction.direct_allocation_bytes ==
              f->baseline_world_bytes[side]);
    ok = true;
cleanup:
    mc_world_class_entity_list_release(out);
    mc_world_class_entity_list_release(empty);
    return ok;
}

static bool class_native_overflow(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *const a = f->chunks[slot]->m->items[0]->items[0];
    mc_entity *const b = f->chunks[slot]->m->items[1]->items[0];
    const mc_entity_class_literal *item = NULL, *mob = NULL;
    mc_world_class_entity_list *out = NULL;
    mc_world_nearby_error error;
    CHECK(mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_ITEM, &item) ==
              MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entity_classes[side], MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
                                      &mob) == MC_ENTITY_OK);
    const size_t bytes = w->construction.direct_allocation_bytes;
    const size_t a_holds = a->holds, b_holds = b->holds, leases = w->local_leases;
    /* Explicit native invariant controls, restored before assertions. Finite
     * runs cannot accumulate SIZE_MAX real holds; these claim no Java OOM. */
    b->holds = SIZE_MAX;
    const mc_world_storage_status hold_limit =
        mc_world_runtime_class_entities(w, item, &class_box, &out, &error);
    b->holds = b_holds;
    CHECK(hold_limit == MC_WORLD_STORAGE_LIMIT && out == NULL && error.query.bytecode == 120 &&
          error.query.chunk_bytecode == 137 && error.entity.status == MC_ENTITY_LIMIT &&
          error.slice == 1 && error.index == 0 && a->holds == a_holds && b->holds == b_holds &&
          w->construction.direct_allocation_bytes == bytes && w->local_leases == leases &&
          w->active_calls == 0);
    w->local_leases = SIZE_MAX;
    const mc_world_storage_status lease_limit =
        mc_world_runtime_class_entities(w, mob, &class_box, &out, &error);
    w->local_leases = leases;
    CHECK(lease_limit == MC_WORLD_STORAGE_LIMIT && out == NULL && error.query.bytecode == 137 &&
          error.query.chunk_bytecode == 0 && error.slice == SIZE_MAX && error.index == SIZE_MAX &&
          a->holds == a_holds && b->holds == b_holds &&
          w->construction.direct_allocation_bytes == bytes && w->active_calls == 0);
    ok = true;
cleanup:
    mc_world_class_entity_list_release(out);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    if (argc != 3 || strcmp(argv[1], "world-class-entities") != 0 ||
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
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        CHECK(open_store(f, slot, root) && class_world(f, slot));
        if (slot < 2)
            CHECK(class_empty_ownership(f, slot));
    }
    for (size_t side = 0; side < 2; ++side)
        CHECK(class_order_and_filter(f, side) && class_null_order_and_prefix(f, side) &&
              class_native_budget(f, side) && class_native_overflow(f, side));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Class query cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u Class Entity query checks (world-class-entities)\n", checks) < 0)
        result = 1;
    return result;
}
