#include "mc/block/registry/registry.h"
#include "mc/core/java/random_profile.h"
#include "mc/world/runtime/construction.h"

#include "../../../../src/core/java/object_identity_profile_internal.h"
#include "../../../../src/world/runtime/tile_entities/identity_set_internal.h"
#include "../../../../src/world/runtime/tile_entities/tile_entities_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static unsigned checks;
static bool retirement_failed;
static mc_math_trig trig;

/* Native unit inputs, not original-process hashes, startup words or encounter
 * orders. Collision cells below are authored BEFORE their first Set use, on
 * actual allocated Tiles. No World, constructor or serialized field supplies
 * them. Private retain-overflow controls are restored before any cleanup. */
typedef struct fixture {
    mc_world_runtime_classes *world_classes;
    mc_tile_entity_classes *classes;
    mc_registry_classes *registry;
    mc_java_identity_thread *main_thread;
    mc_tile_entity *tiles[384];
    size_t count;
    mc_world_info_side side;
} fixture;

static bool words_equal(mc_java_identity_thread a, mc_java_identity_thread b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static mc_tile_entity *new_tile(fixture *f, mc_tile_entity_kind kind, uint32_t authored_hash) {
    mc_tile_entity *tile = NULL;
    if (f->count == COUNT(f->tiles) ||
        mc_tile_entity_allocate_in_classes(f->world_classes, kind, &tile) != MC_TILE_ENTITY_OK)
        return NULL;
    if (tile->holds != 1 || tile->identity.hash != 0 || tile->phase != MC_TILE_ENTITY_RAW) {
        mc_tile_entity_release(tile);
        return NULL;
    }
    tile->identity.hash = authored_hash;
    f->tiles[f->count++] = tile;
    return tile;
}

static bool retire_set(mc_tile_identity_set **set) {
    if (mc_tile_identity_set_retire(set, NULL) == MC_TILE_IDENTITY_SET_OK)
        return true;
    fputs("Tile identity Set retirement refused; dependencies retained\n", stderr);
    retirement_failed = true;
    return false;
}

static bool retire_snapshot(mc_tile_identity_set_snapshot **snapshot) {
    if (mc_tile_identity_set_snapshot_retire(snapshot, NULL) == MC_TILE_IDENTITY_SET_OK)
        return true;
    retirement_failed = true;
    return false;
}

/* Copy the actual complete toArray result before retiring its independent
 * holds. Never infer a traversal from insertion order or native addresses. */
static bool order(mc_tile_identity_set *set, mc_tile_entity **items, size_t capacity,
                  size_t *count) {
    bool ok = false;
    mc_tile_identity_set_snapshot *snapshot = NULL;
    mc_tile_identity_set_snapshot_view view;
    CHECK(mc_tile_identity_set_to_array(set, &snapshot, NULL) == MC_TILE_IDENTITY_SET_OK);
    CHECK(mc_tile_identity_set_snapshot_inspect(snapshot, &view, NULL) == MC_TILE_IDENTITY_SET_OK);
    CHECK(view.count <= capacity);
    for (size_t i = 0; i < view.count; ++i)
        items[i] = view.items[i];
    *count = view.count;
    ok = true;
cleanup:
    return retire_snapshot(&snapshot) && ok;
}

static bool exact_order(mc_tile_identity_set *set, mc_tile_entity *const *expected, size_t count) {
    bool ok = false;
    mc_tile_entity *actual[192];
    size_t n = SIZE_MAX;
    CHECK(order(set, actual, COUNT(actual), &n) && n == count);
    for (size_t i = 0; i < count; ++i)
        CHECK(actual[i] == expected[i]);
    ok = true;
cleanup:
    return ok;
}

static bool same_shape(mc_tile_identity_set_view a, mc_tile_identity_set_view b) {
    return a.size == b.size && a.capacity == b.capacity && a.threshold == b.threshold &&
           a.mod_count == b.mod_count && a.retained_bytes == b.retained_bytes &&
           a.snapshot_count == b.snapshot_count && a.live_snapshots == b.live_snapshots &&
           a.failed_snapshots == b.failed_snapshots && a.snapshot_bytes == b.snapshot_bytes;
}

static bool identity_leaf(fixture *f) {
    bool ok = false;
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_java_identity_thread saved;
    mc_tile_entity *tile = NULL;
    const mc_tile_entity_class_literal *literal = NULL;
    uint32_t hash = UINT32_C(0x77777777);
    mc_tile_entity_piston_class_snapshot before, after;
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->world_classes, &before) == MC_TILE_ENTITY_OK);
    for (int i = 0; i < MC_TILE_ENTITY_CLASS_KIND_COUNT; ++i) {
        const char *name = NULL;
        mc_tile_entity_class_literal_view view;
        CHECK(mc_tile_entity_class_literal_get(f->classes, (mc_tile_entity_class_kind)i,
                                               &literal) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_class_literal_inspect(literal, &view) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_class_literal_source_name(literal, &name) == MC_TILE_ENTITY_OK &&
              strcmp(name, f->side == MC_WORLD_INFO_CLIENT ? view.client_name : view.server_name) ==
                  0);
        CHECK(mc_tile_entity_class_literal_identity_hash(literal, &thread, &hash) ==
                  MC_TILE_ENTITY_OK &&
              hash > 0 && hash <= UINT32_C(0x7fffffff));
        saved = thread;
        const uint32_t cached = hash;
        CHECK(mc_tile_entity_class_literal_identity_hash(literal, &thread, &hash) ==
                  MC_TILE_ENTITY_OK &&
              hash == cached && words_equal(thread, saved));
        hash = UINT32_C(0x77777777);
        CHECK(mc_tile_entity_class_literal_identity_hash(literal, NULL, &hash) ==
                  MC_TILE_ENTITY_INVALID_ARGUMENT &&
              hash == UINT32_C(0x77777777));
        CHECK(mc_tile_entity_class_literal_identity_hash(literal, &thread, NULL) ==
                  MC_TILE_ENTITY_INVALID_ARGUMENT &&
              words_equal(thread, saved));
    }
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->world_classes, &after) == MC_TILE_ENTITY_OK &&
          after.base_phase == before.base_phase && after.piston_phase == before.piston_phase &&
          after.base_name_count == before.base_name_count &&
          after.list_identity == before.list_identity &&
          after.allocation_bytes == before.allocation_bytes &&
          after.live_tile_entities == before.live_tile_entities);
    thread = (mc_java_identity_thread){1, 2, 3, 4};
    tile = new_tile(f, MC_TILE_ENTITY_PISTON, 0);
    CHECK(tile != NULL &&
          mc_tile_entity_identity_hash(tile, &thread, NULL) == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          tile->identity.hash == 0 && words_equal(thread, (mc_java_identity_thread){1, 2, 3, 4}));
    CHECK(mc_tile_entity_identity_hash(tile, NULL, &hash) == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          hash == UINT32_C(0x77777777) && tile->identity.hash == 0);
    CHECK(mc_tile_entity_identity_hash(tile, &thread, &hash) == MC_TILE_ENTITY_OK &&
          hash == UINT32_C(0x80d) && tile->identity.hash == hash &&
          words_equal(thread, (mc_java_identity_thread){2, 3, 4, UINT32_C(0x80d)}));
    saved = thread;
    CHECK(mc_tile_entity_piston_construct(tile, 1, 2, 3, true, false, NULL) == MC_TILE_ENTITY_OK &&
          tile->phase == MC_TILE_ENTITY_READY && tile->identity.hash == hash &&
          mc_tile_entity_identity_hash(tile, &thread, &hash) == MC_TILE_ENTITY_OK &&
          hash == UINT32_C(0x80d) && words_equal(thread, saved));
    saved = *f->main_thread;
    CHECK(mc_tile_entity_identity_hash(tile, f->main_thread, &hash) == MC_TILE_ENTITY_OK &&
          hash == UINT32_C(0x80d) && words_equal(*f->main_thread, saved));
    CHECK(mc_tile_entity_identity_hash(NULL, NULL, &hash) == MC_TILE_ENTITY_OK && hash == 0);
    CHECK(mc_tile_entity_identity_hash(NULL, &thread, NULL) == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          words_equal(thread, (mc_java_identity_thread){2, 3, 4, UINT32_C(0x80d)}));
    /* A separate fresh Tile reaches the actual application's main Thread. */
    tile = new_tile(f, MC_TILE_ENTITY_CHEST, 0);
    CHECK(tile != NULL &&
          mc_tile_entity_identity_hash(tile, f->main_thread, &hash) == MC_TILE_ENTITY_OK &&
          hash != 0 && tile->identity.hash == hash && !words_equal(*f->main_thread, saved));
    saved = *f->main_thread;
    CHECK(mc_tile_entity_identity_hash(tile, f->main_thread, &hash) == MC_TILE_ENTITY_OK &&
          words_equal(*f->main_thread, saved));
    ok = true;
cleanup:
    return ok;
}

static bool arguments_and_hash_timing(fixture *f) {
    bool ok = false;
    mc_tile_identity_set *set = NULL;
    mc_tile_identity_set_error error;
    mc_tile_identity_set_view view, saved_view;
    mc_tile_identity_set_snapshot_view snapshot_view = {.items = NULL, .count = 39};
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_tile_entity *a = new_tile(f, MC_TILE_ENTITY_CHEST, 0);
    mc_tile_entity *b = new_tile(f, MC_TILE_ENTITY_PISTON, 0);
    bool result = true;
    CHECK(a != NULL && b != NULL);
    CHECK(mc_tile_identity_set_create(NULL, NULL, &error) == MC_TILE_IDENTITY_SET_INVALID_ARGUMENT);
    CHECK(mc_tile_identity_set_create(NULL, &set, &error) == MC_TILE_IDENTITY_SET_OK &&
          error.status == MC_TILE_IDENTITY_SET_OK && error.operation == NULL);
    CHECK(mc_tile_identity_set_inspect(set, &saved_view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          saved_view.size == 0 && saved_view.capacity == 0 && saved_view.threshold == 0 &&
          saved_view.mod_count == 0 && saved_view.retained_bytes != 0);
    CHECK(mc_tile_identity_set_add(NULL, a, &thread, &result, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          result && a->identity.hash == 0);
    CHECK(mc_tile_identity_set_add(set, a, &thread, NULL, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          a->identity.hash == 0);
    CHECK(mc_tile_identity_set_remove(set, a, &thread, NULL, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          a->identity.hash == 0);
    CHECK(mc_tile_identity_set_contains(set, a, &thread, NULL, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          words_equal(thread, (mc_java_identity_thread){1, 2, 3, 4}));
    CHECK(mc_tile_identity_set_contains(set, a, NULL, &result, &error) == MC_TILE_IDENTITY_SET_OK &&
          !result && a->identity.hash == 0);
    result = true;
    CHECK(mc_tile_identity_set_remove(set, a, &thread, &result, &error) ==
              MC_TILE_IDENTITY_SET_OK &&
          !result && a->identity.hash == UINT32_C(0x80d) && a->holds == 1 &&
          words_equal(thread, (mc_java_identity_thread){2, 3, 4, UINT32_C(0x80d)}));
    CHECK(mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          same_shape(view, saved_view));
    CHECK(mc_tile_identity_set_add(set, NULL, NULL, &result, &error) == MC_TILE_IDENTITY_SET_OK &&
          result &&
          mc_tile_identity_set_add(set, NULL, NULL, &result, &error) == MC_TILE_IDENTITY_SET_OK &&
          !result);
    CHECK(mc_tile_identity_set_remove(set, NULL, NULL, &result, &error) ==
              MC_TILE_IDENTITY_SET_OK &&
          result && mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.size == 0 && view.capacity == 16 && view.threshold == 12 && view.mod_count == 2);
    CHECK(mc_tile_identity_set_contains(set, b, &thread, &result, &error) ==
              MC_TILE_IDENTITY_SET_OK &&
          !result && b->identity.hash == UINT32_C(0x181f) &&
          words_equal(thread, (mc_java_identity_thread){3, 4, UINT32_C(0x80d), UINT32_C(0x181f)}));
    result = true;
    CHECK(mc_tile_identity_set_contains(set, b, NULL, &result, &error) ==
              MC_TILE_IDENTITY_SET_TILE_FAILED &&
          error.tile_status == MC_TILE_ENTITY_INVALID_ARGUMENT && result);
    saved_view = view;
    CHECK(mc_tile_identity_set_inspect(NULL, &view, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          same_shape(view, saved_view));
    CHECK(mc_tile_identity_set_inspect(set, NULL, &error) == MC_TILE_IDENTITY_SET_INVALID_ARGUMENT);
    CHECK(mc_tile_identity_set_snapshot_inspect(NULL, &snapshot_view, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          snapshot_view.items == NULL && snapshot_view.count == 39);
    CHECK(mc_tile_identity_set_snapshot_retire(NULL, &error) ==
          MC_TILE_IDENTITY_SET_INVALID_ARGUMENT);
    CHECK(mc_tile_identity_set_retire(NULL, &error) == MC_TILE_IDENTITY_SET_INVALID_ARGUMENT);
    CHECK(mc_tile_identity_set_add(set, a, &thread, &result, NULL) == MC_TILE_IDENTITY_SET_OK &&
          result && a->holds == 2);
    CHECK(mc_tile_identity_set_add(set, a, &thread, &result, NULL) == MC_TILE_IDENTITY_SET_OK &&
          !result && a->holds == 2);
    CHECK(mc_tile_identity_set_remove(set, a, &thread, &result, NULL) == MC_TILE_IDENTITY_SET_OK &&
          result && a->holds == 1);
    ok = true;
cleanup:
    return retire_set(&set) && ok;
}

typedef struct allocation {
    void *pointer;
    size_t bytes;
    bool live;
} allocation;

typedef struct allocator_state {
    allocation records[512];
    size_t count, requests, fail_at, releases, live_bytes;
    bool invalid;
} allocator_state;

static void *allocate(void *context, size_t bytes) {
    allocator_state *state = context;
    ++state->requests;
    if (state->fail_at != 0 && state->requests == state->fail_at)
        return NULL;
    if (state->count == COUNT(state->records)) {
        state->invalid = true;
        return NULL;
    }
    void *pointer = malloc(bytes);
    if (pointer == NULL)
        return NULL;
    memset(pointer, 0xa5, bytes);
    state->records[state->count++] = (allocation){pointer, bytes, true};
    state->live_bytes += bytes;
    return pointer;
}

static void deallocate(void *context, void *storage) {
    allocator_state *state = context;
    for (size_t i = state->count; i != 0; --i) {
        allocation *entry = &state->records[i - 1];
        if (entry->live && entry->pointer == storage) {
            entry->live = false;
            state->live_bytes -= entry->bytes;
            ++state->releases;
            free(storage);
            return;
        }
    }
    state->invalid = true;
}

static bool limits_and_allocation(fixture *f) {
    bool ok = false;
    mc_tile_identity_set *set = NULL;
    mc_tile_identity_set_snapshot *snapshot = NULL;
    mc_tile_identity_set_error error;
    mc_tile_identity_set_view before, after;
    mc_tile_identity_set_limits limits = mc_tile_identity_set_default_limits();
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_tile_entity *a = new_tile(f, MC_TILE_ENTITY_CHEST, 0);
    mc_tile_entity *b = new_tile(f, MC_TILE_ENTITY_CHEST, 0);
    mc_tile_entity *c = new_tile(f, MC_TILE_ENTITY_CHEST, 0);
    mc_tile_entity *d = new_tile(f, MC_TILE_ENTITY_CHEST, 0);
    allocator_state state = {0};
    const mc_tile_identity_set_allocator allocator = {&state, allocate, deallocate};
    bool changed = true;
    CHECK(a != NULL && b != NULL && c != NULL && d != NULL);
    CHECK(limits.max_entries == 65536 && limits.max_retained_bytes == 8u * 1024u * 1024u &&
          limits.max_allocation_bytes == 16u * 1024u * 1024u);
    limits.max_entries = (size_t)INT32_MAX + 1u;
    CHECK(mc_tile_identity_set_create(&limits, &set, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          set == NULL);
    limits = mc_tile_identity_set_default_limits();
    limits.max_entries = 0;
    CHECK(mc_tile_identity_set_create(&limits, &set, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_inspect(set, &before, NULL) == MC_TILE_IDENTITY_SET_OK);
    CHECK(mc_tile_identity_set_add(set, a, &thread, &changed, &error) ==
              MC_TILE_IDENTITY_SET_LIMIT &&
          changed && a->identity.hash == UINT32_C(0x80d) && a->holds == 1 &&
          words_equal(thread, (mc_java_identity_thread){2, 3, 4, UINT32_C(0x80d)}));
    CHECK(mc_tile_identity_set_inspect(set, &after, NULL) == MC_TILE_IDENTITY_SET_OK &&
          same_shape(before, after) && before.peak_bytes == after.peak_bytes);
    const size_t owner_bytes = before.retained_bytes;
    CHECK(retire_set(&set));
    limits = mc_tile_identity_set_default_limits();
    limits.max_retained_bytes = owner_bytes;
    CHECK(mc_tile_identity_set_create(&limits, &set, NULL) == MC_TILE_IDENTITY_SET_OK);
    CHECK(mc_tile_identity_set_add(set, b, &thread, &changed, &error) ==
              MC_TILE_IDENTITY_SET_LIMIT &&
          changed && b->identity.hash == UINT32_C(0x181f) && b->holds == 1 &&
          mc_tile_identity_set_inspect(set, &after, NULL) == MC_TILE_IDENTITY_SET_OK &&
          after.retained_bytes == owner_bytes && after.capacity == 0);
    CHECK(mc_tile_identity_set_to_array(set, &snapshot, &error) == MC_TILE_IDENTITY_SET_LIMIT &&
          snapshot == NULL);
    CHECK(retire_set(&set));
    limits = mc_tile_identity_set_default_limits();
    limits.max_allocation_bytes = owner_bytes - 1;
    CHECK(mc_tile_identity_set_create(&limits, &set, &error) == MC_TILE_IDENTITY_SET_LIMIT &&
          set == NULL);
    CHECK(mc_tile_identity_set_create_with_allocator(NULL, NULL, &set, &error) ==
          MC_TILE_IDENTITY_SET_INVALID_ARGUMENT);
    state.fail_at = 1;
    CHECK(mc_tile_identity_set_create_with_allocator(NULL, &allocator, &set, &error) ==
              MC_TILE_IDENTITY_SET_ALLOCATION_FAILED &&
          set == NULL && state.live_bytes == 0);
    state.fail_at = 0;
    CHECK(mc_tile_identity_set_create_with_allocator(NULL, &allocator, &set, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_inspect(set, &before, NULL) == MC_TILE_IDENTITY_SET_OK);
    state.fail_at = state.requests + 1; /* Node allocation, after the fresh hash. */
    CHECK(mc_tile_identity_set_add(set, c, &thread, &changed, &error) ==
              MC_TILE_IDENTITY_SET_ALLOCATION_FAILED &&
          changed && c->identity.hash == 4 && c->holds == 1 && state.live_bytes == owner_bytes);
    state.fail_at = state.requests + 2; /* Node succeeds, initial table refuses. */
    CHECK(mc_tile_identity_set_add(set, d, &thread, &changed, &error) ==
              MC_TILE_IDENTITY_SET_ALLOCATION_FAILED &&
          changed && d->identity.hash == UINT32_C(0x2020) && d->holds == 1 &&
          state.live_bytes == owner_bytes);
    CHECK(mc_tile_identity_set_inspect(set, &after, NULL) == MC_TILE_IDENTITY_SET_OK &&
          same_shape(before, after) && after.peak_bytes > before.peak_bytes);
    state.fail_at = 0;
    const mc_java_identity_thread saved = thread;
    CHECK(mc_tile_identity_set_add(set, d, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          changed && d->holds == 2 && words_equal(thread, saved));
    CHECK(mc_tile_identity_set_inspect(set, &before, NULL) == MC_TILE_IDENTITY_SET_OK &&
          before.retained_bytes == state.live_bytes);
    state.fail_at = state.requests + 1;
    CHECK(mc_tile_identity_set_to_array(set, &snapshot, &error) ==
              MC_TILE_IDENTITY_SET_ALLOCATION_FAILED &&
          snapshot == NULL && d->holds == 2 &&
          mc_tile_identity_set_inspect(set, &after, NULL) == MC_TILE_IDENTITY_SET_OK &&
          same_shape(before, after));
    state.fail_at = 0;
    CHECK(retire_set(&set) && state.live_bytes == 0 && !state.invalid);
    ok = true;
cleanup:
    (void)retire_snapshot(&snapshot);
    if (!retire_set(&set))
        return false;
    if (state.live_bytes != 0 || state.invalid)
        return false;
    return ok;
}

static bool bulk_branches(fixture *f) {
    bool ok = false;
    mc_tile_identity_set *set = NULL, *other = NULL;
    mc_tile_identity_set_error error;
    mc_tile_identity_set_view view;
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_tile_entity *a = new_tile(f, MC_TILE_ENTITY_CHEST, 1);
    mc_tile_entity *b = new_tile(f, MC_TILE_ENTITY_PISTON, 2);
    mc_tile_entity *c = new_tile(f, MC_TILE_ENTITY_FURNACE, 0);
    bool changed = true, found = false;
    const mc_tile_identity_set_span empty = {NULL, 0};
    const mc_tile_identity_set_span invalid = {NULL, 1};
    CHECK(a != NULL && b != NULL && c != NULL &&
          mc_tile_identity_set_create(NULL, &set, NULL) == MC_TILE_IDENTITY_SET_OK);
    CHECK(mc_tile_identity_set_remove_all_span(set, NULL, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_NULL_COLLECTION &&
          changed);
    CHECK(mc_tile_identity_set_remove_all_set(set, NULL, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_NULL_COLLECTION &&
          changed);
    CHECK(mc_tile_identity_set_add_all_span(set, NULL, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_NULL_COLLECTION &&
          changed);
    CHECK(mc_tile_identity_set_add_all_span(set, &invalid, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          changed);
    CHECK(mc_tile_identity_set_remove_all_span(set, &invalid, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_INVALID_ARGUMENT &&
          changed);
    CHECK(mc_tile_identity_set_add_all_span(set, &empty, NULL, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          !changed);
    mc_tile_entity *items[] = {a, a, b, NULL, NULL};
    mc_tile_identity_set_span span = {items, COUNT(items)};
    CHECK(mc_tile_identity_set_add_all_span(set, &span, &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && a->holds == 2 && b->holds == 2 &&
          mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.size == 3 && view.mod_count == 3);
    CHECK(mc_tile_identity_set_add_all_span(set, &span, &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          !changed && a->holds == 2 && b->holds == 2);
    mc_tile_entity *null_only[] = {NULL};
    span = (mc_tile_identity_set_span){null_only, 1};
    CHECK(mc_tile_identity_set_remove_all_span(set, &span, NULL, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed);
    /* Count is two, including a duplicate. Equal size MUST use this iterator
     * and ArrayList.contains(pointer==), which needs no current Thread. */
    mc_tile_entity *duplicates[] = {a, a};
    span = (mc_tile_identity_set_span){duplicates, COUNT(duplicates)};
    CHECK(mc_tile_identity_set_remove_all_span(set, &span, NULL, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && a->holds == 1 && b->holds == 2);
    CHECK(mc_tile_identity_set_contains(set, b, &thread, &found, NULL) == MC_TILE_IDENTITY_SET_OK &&
          found);
    CHECK(mc_tile_identity_set_add(set, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK);
    mc_tile_entity *one[] = {a};
    span = (mc_tile_identity_set_span){one, 1};
    changed = false;
    CHECK(mc_tile_identity_set_remove_all_span(set, &span, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_TILE_FAILED &&
          !changed && error.index == 0 && error.completed == 0 && !error.changed_prefix &&
          error.tile_status == MC_TILE_ENTITY_INVALID_ARGUMENT && a->holds == 2);
    CHECK(mc_tile_identity_set_remove_all_span(set, &span, &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && a->holds == 1);
    /* The larger other Set forces contains(other), not ArrayList.contains. */
    CHECK(mc_tile_identity_set_create(NULL, &other, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(other, b, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(other, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK);
    changed = false;
    CHECK(mc_tile_identity_set_remove_all_set(set, other, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_TILE_FAILED &&
          !changed && error.index == 0 && b->holds == 3);
    CHECK(mc_tile_identity_set_remove_all_set(set, other, &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && b->holds == 2);
    CHECK(mc_tile_identity_set_remove_all_set(other, other, &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && a->holds == 1 && b->holds == 1 &&
          mc_tile_identity_set_inspect(other, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.size == 0 && view.capacity == 16);
    CHECK(mc_tile_identity_set_add(set, NULL, NULL, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(set, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(other, NULL, NULL, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(other, b, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK);
    /* Equal-size Set iterator removes NULL first, then reaches the genuine
     * nonnull hash admission failure. Earlier removal and its modCount survive. */
    changed = false;
    CHECK(mc_tile_identity_set_remove_all_set(set, other, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_TILE_FAILED &&
          !changed && error.index == 1 && error.completed == 1 && error.changed_prefix &&
          a->holds == 2 &&
          mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.size == 1);
    /* A nonmember in a larger ArrayList never has its identity evaluated. */
    mc_tile_entity *linear[] = {a, c};
    span = (mc_tile_identity_set_span){linear, COUNT(linear)};
    CHECK(mc_tile_identity_set_remove_all_span(set, &span, NULL, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && c->identity.hash == 0 && a->holds == 1 &&
          words_equal(thread, (mc_java_identity_thread){1, 2, 3, 4}));
    CHECK(mc_tile_identity_set_remove(other, NULL, NULL, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed &&
          mc_tile_identity_set_add(set, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(set, b, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(set, NULL, NULL, &changed, NULL) == MC_TILE_IDENTITY_SET_OK);
    /* Strictly larger this Set iterates the other Set and uses ordinary
     * remove, including its nonnull-key current-Thread admission. */
    changed = false;
    CHECK(mc_tile_identity_set_remove_all_set(set, other, NULL, &changed, &error) ==
              MC_TILE_IDENTITY_SET_TILE_FAILED &&
          !changed && error.index == 0 && !error.changed_prefix && b->holds == 3);
    CHECK(mc_tile_identity_set_remove_all_set(set, other, &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && b->holds == 2 &&
          mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.size == 2);
    ok = true;
cleanup: {
    const bool first = retire_set(&set);
    const bool second = retire_set(&other);
    return first && second && ok;
}
}

static bool bulk_add_prefix(fixture *f) {
    bool ok = false;
    mc_tile_identity_set *set = NULL;
    mc_tile_identity_set_error error;
    mc_tile_identity_set_limits limits = mc_tile_identity_set_default_limits();
    mc_tile_identity_set_view view;
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_tile_entity *a = new_tile(f, MC_TILE_ENTITY_CHEST, 0);
    mc_tile_entity *b = new_tile(f, MC_TILE_ENTITY_CHEST, 0);
    bool changed = false;
    CHECK(a != NULL && b != NULL);
    limits.max_entries = 1;
    CHECK(mc_tile_identity_set_create(&limits, &set, NULL) == MC_TILE_IDENTITY_SET_OK);
    mc_tile_entity *items[] = {a, a, b};
    const mc_tile_identity_set_span span = {items, COUNT(items)};
    CHECK(mc_tile_identity_set_add_all_span(set, &span, &thread, &changed, &error) ==
              MC_TILE_IDENTITY_SET_LIMIT &&
          !changed && error.index == 2 && error.completed == 2 && error.changed_prefix &&
          a->identity.hash == UINT32_C(0x80d) && b->identity.hash == UINT32_C(0x181f) &&
          a->holds == 2 && b->holds == 1 &&
          words_equal(thread, (mc_java_identity_thread){3, 4, UINT32_C(0x80d), UINT32_C(0x181f)}));
    CHECK(mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.size == 1 && view.mod_count == 1);
    CHECK(mc_tile_identity_set_add(set, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          !changed && a->holds == 2);
    CHECK(mc_tile_identity_set_remove(set, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          changed &&
          mc_tile_identity_set_add(set, b, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          changed);
    ok = true;
cleanup:
    return retire_set(&set) && ok;
}

static bool equal_hash_tree(fixture *f) {
    bool ok = false;
    mc_tile_identity_set *set = NULL;
    mc_tile_identity_set_view view;
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_tile_entity *keys[11] = {0}, *expected[11], *before[11], *actual[11];
    size_t count = 0;
    bool changed = false, found = false;
    CHECK(mc_tile_identity_set_create(NULL, &set, NULL) == MC_TILE_IDENTITY_SET_OK);
    for (size_t i = 0; i < COUNT(keys); ++i) {
        keys[i] = new_tile(f, MC_TILE_ENTITY_CHEST, 64);
        CHECK(keys[i] != NULL &&
              mc_tile_identity_set_add(set, keys[i], &thread, &changed, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              changed && keys[i]->holds == 2);
        CHECK(mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
              view.capacity == (i < 8    ? 16u
                                : i == 8 ? 32u
                                         : 64u));
    }
    /* Eleven equal full hashes, same class, take the <= left arm. Treeify
     * moves the balanced root (fourth insertion) ahead of the original chain. */
    static const size_t initial[] = {3, 0, 1, 2, 4, 5, 6, 7, 8, 9, 10};
    for (size_t i = 0; i < COUNT(initial); ++i)
        expected[i] = keys[initial[i]];
    CHECK(exact_order(set, expected, COUNT(initial)));
    for (size_t i = 0; i < COUNT(keys); ++i) {
        CHECK(mc_tile_identity_set_contains(set, keys[i], &thread, &found, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              found);
        CHECK(mc_tile_identity_set_add(set, keys[i], &thread, &changed, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              !changed && keys[i]->holds == 2);
    }
    mc_tile_entity *absent = new_tile(f, MC_TILE_ENTITY_CHEST, 64);
    CHECK(absent != NULL &&
          mc_tile_identity_set_contains(set, absent, &thread, &found, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          !found &&
          mc_tile_identity_set_remove(set, absent, &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          !changed);
    CHECK(order(set, before, COUNT(before), &count) && count == COUNT(keys));
    /* Duplicate elements force the iterator branch. Even root deletion must
     * leave the survivor next chain in precisely its prior order. */
    mc_tile_entity *remove_items[11];
    for (size_t i = 0; i < COUNT(remove_items); ++i)
        remove_items[i] = keys[0];
    mc_tile_identity_set_span span = {remove_items, COUNT(remove_items)};
    CHECK(mc_tile_identity_set_remove_all_span(set, &span, NULL, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && keys[0]->holds == 1);
    size_t n = 0;
    for (size_t i = 0; i < count; ++i)
        if (before[i] != keys[0])
            expected[n++] = before[i];
    CHECK(exact_order(set, expected, n));
    CHECK(mc_tile_identity_set_add(set, keys[0], &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && order(set, actual, COUNT(actual), &count) && count == COUNT(keys));
    bool adjacent = false;
    for (size_t i = 1; i < count; ++i)
        if (actual[i - 1] == keys[10] && actual[i] == keys[0])
            adjacent = true;
    CHECK(adjacent); /* Equal-class/raw tie is <=, never native address order. */
    CHECK(mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.size == 11 && view.capacity == 64 && view.mod_count == 13);
    /* Now delete the current root and every second remaining element through
     * iterator removal, retaining tree state rather than rebuilding survivors. */
    CHECK(order(set, before, COUNT(before), &count));
    for (size_t i = 0; i < COUNT(remove_items); ++i)
        remove_items[i] = before[(i % 6u) * 2u];
    CHECK(mc_tile_identity_set_remove_all_span(set, &span, NULL, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed);
    n = 0;
    for (size_t i = 1; i < count; i += 2)
        expected[n++] = before[i];
    CHECK(exact_order(set, expected, n));
    for (size_t i = 0; i < COUNT(keys); ++i) {
        bool retained = false;
        for (size_t j = 0; j < n; ++j)
            retained = retained || expected[j] == keys[i];
        CHECK(mc_tile_identity_set_contains(set, keys[i], &thread, &found, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              found == retained);
        CHECK(mc_tile_identity_set_add(set, keys[i], &thread, &changed, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              changed == !retained);
    }
    CHECK(mc_tile_identity_set_remove_all_set(set, set, &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed && mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.size == 0 && view.capacity == 64 &&
          words_equal(thread, (mc_java_identity_thread){1, 2, 3, 4}));
    for (size_t i = 0; i < COUNT(keys); ++i)
        CHECK(keys[i]->holds == 1);
    ok = true;
cleanup:
    return retire_set(&set) && ok;
}

static bool mixed_class_tree(fixture *f) {
    bool ok = false;
    mc_tile_identity_set *set = NULL;
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_tile_entity *keys[16] = {0}, *before[16], *expected[16];
    const mc_tile_entity_kind kinds[] = {MC_TILE_ENTITY_PISTON, MC_TILE_ENTITY_FURNACE,
                                         MC_TILE_ENTITY_CHEST, MC_TILE_ENTITY_SPAWNER};
    size_t count = 0;
    bool changed = false, found = false;
    CHECK(mc_tile_identity_set_create(NULL, &set, NULL) == MC_TILE_IDENTITY_SET_OK);
    for (size_t i = 0; i < COUNT(keys); ++i) {
        keys[i] = new_tile(f, kinds[i % COUNT(kinds)], 128);
        CHECK(keys[i] != NULL &&
              mc_tile_identity_set_add(set, keys[i], &thread, &changed, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              changed);
    }
    /* Authored insertion order cycles uk/sk/js/cy (mu/ln/gk/bx on server).
     * Class-name comparison precedes the equal-raw-hash <= tie. Treeify's
     * root is the first Chest, then each later tree insertion joins the
     * next chain immediately after its actual search parent. Ignoring class
     * names would instead choose the fourth insertion as the first root. */
    static const size_t class_order[] = {2, 0, 1, 3, 4, 5, 6, 7, 11, 15, 8, 12, 9, 13, 10, 14};
    for (size_t i = 0; i < COUNT(class_order); ++i)
        expected[i] = keys[class_order[i]];
    CHECK(exact_order(set, expected, COUNT(class_order)));
    for (size_t i = 0; i < COUNT(keys); ++i) {
        CHECK(mc_tile_identity_set_contains(set, keys[i], &thread, &found, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              found);
    }
    CHECK(order(set, before, COUNT(before), &count) && count == COUNT(keys));
    mc_tile_entity *items[16];
    size_t n = 0;
    for (size_t i = 0; i < COUNT(items); ++i)
        items[i] = before[0];
    const mc_tile_identity_set_span span = {items, COUNT(items)};
    CHECK(mc_tile_identity_set_remove_all_span(set, &span, NULL, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed);
    for (size_t i = 1; i < count; ++i)
        expected[n++] = before[i];
    CHECK(exact_order(set, expected, n));
    CHECK(mc_tile_identity_set_add(set, before[0], &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed);
    /* All four actual getClass literals participate in equal-full-hash ties;
     * searches and ordinary removals must find every subtree after rotations. */
    for (size_t step = 0; step < COUNT(keys); ++step) {
        const size_t i = (step * 5u) % COUNT(keys);
        CHECK(mc_tile_identity_set_remove(set, keys[i], &thread, &changed, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              changed && keys[i]->holds == 1);
        CHECK(mc_tile_identity_set_contains(set, keys[i], &thread, &found, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              !found);
    }
    CHECK(words_equal(thread, (mc_java_identity_thread){1, 2, 3, 4}));
    ok = true;
cleanup:
    return retire_set(&set) && ok;
}

static bool tree_split(fixture *f) {
    bool ok = false;
    mc_tile_identity_set *set = NULL;
    mc_tile_identity_set_view view;
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_tile_entity *keys[12] = {0}, *fillers[37] = {0}, *expected[49];
    bool changed = false;
    CHECK(mc_tile_identity_set_create(NULL, &set, NULL) == MC_TILE_IDENTITY_SET_OK);
    for (size_t i = 0; i < COUNT(keys); ++i) {
        keys[i] = new_tile(f, MC_TILE_ENTITY_CHEST, (uint32_t)(i + 1) * 64u);
        CHECK(keys[i] != NULL &&
              mc_tile_identity_set_add(set, keys[i], &thread, &changed, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              changed);
    }
    CHECK(mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.capacity == 64 && view.threshold == 48 && view.size == 12);
    for (size_t i = 0; i < COUNT(fillers); ++i) {
        fillers[i] = new_tile(f, MC_TILE_ENTITY_CHEST, (uint32_t)i + 1u);
        CHECK(fillers[i] != NULL &&
              mc_tile_identity_set_add(set, fillers[i], &thread, &changed, NULL) ==
                  MC_TILE_IDENTITY_SET_OK &&
              changed);
    }
    CHECK(mc_tile_identity_set_inspect(set, &view, NULL) == MC_TILE_IDENTITY_SET_OK &&
          view.capacity == 128 && view.threshold == 96 && view.size == 49 && view.mod_count == 49);
    /* Resize stably splits the tree's next chain by bit64 into two six-node
     * plain bins. The root formerly moved to front remains first in low bin. */
    static const size_t low[] = {3, 1, 5, 7, 9, 11};
    static const size_t high[] = {0, 2, 4, 6, 8, 10};
    size_t n = 0;
    for (size_t i = 0; i < COUNT(low); ++i)
        expected[n++] = keys[low[i]];
    for (size_t i = 0; i < COUNT(fillers); ++i)
        expected[n++] = fillers[i];
    for (size_t i = 0; i < COUNT(high); ++i)
        expected[n++] = keys[high[i]];
    CHECK(exact_order(set, expected, n));
    CHECK(mc_tile_identity_set_remove(set, keys[3], &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed &&
          mc_tile_identity_set_add(set, keys[3], &thread, &changed, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          changed);
    for (size_t i = 0; i + 1 < COUNT(low); ++i)
        expected[i] = keys[low[i + 1]];
    expected[COUNT(low) - 1] = keys[3];
    CHECK(exact_order(set, expected, n));
    CHECK(words_equal(thread, (mc_java_identity_thread){1, 2, 3, 4}));
    ok = true;
cleanup:
    return retire_set(&set) && ok;
}

static bool snapshots_and_retains(fixture *f) {
    bool ok = false;
    mc_tile_identity_set *set = NULL;
    mc_tile_identity_set_snapshot *first = NULL, *second = NULL;
    mc_tile_identity_set_snapshot_view snapshot_view;
    mc_tile_identity_set_view before, after;
    mc_tile_identity_set_error error;
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_tile_entity *a = new_tile(f, MC_TILE_ENTITY_CHEST, 1);
    mc_tile_entity *b = new_tile(f, MC_TILE_ENTITY_PISTON, 2);
    mc_tile_entity *overflow = new_tile(f, MC_TILE_ENTITY_FURNACE, 0);
    mc_tile_entity *saturated = NULL;
    size_t restored_holds = 0;
    bool changed = false;
    CHECK(a != NULL && b != NULL && overflow != NULL &&
          mc_tile_identity_set_create(NULL, &set, NULL) == MC_TILE_IDENTITY_SET_OK);
    CHECK(mc_tile_identity_set_to_array(set, &first, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_snapshot_inspect(first, &snapshot_view, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          snapshot_view.count == 0 &&
          mc_tile_identity_set_inspect(set, &before, NULL) == MC_TILE_IDENTITY_SET_OK &&
          before.size == 0 && before.capacity == 0 && before.snapshot_count == 1 &&
          before.live_snapshots == 1 && before.snapshot_bytes > 0);
    CHECK(mc_tile_identity_set_retire(&set, &error) == MC_TILE_IDENTITY_SET_BUSY && set != NULL &&
          mc_tile_identity_set_inspect(set, &after, NULL) == MC_TILE_IDENTITY_SET_OK &&
          same_shape(before, after));
    CHECK(retire_snapshot(&first) && first == NULL && retire_snapshot(&first));
    /* Saturate only the native hold counter, before an ordinary add. The
     * fresh hash must survive retain refusal; native shape must not change. */
    CHECK(mc_tile_identity_set_inspect(set, &before, NULL) == MC_TILE_IDENTITY_SET_OK);
    saturated = overflow;
    restored_holds = overflow->holds;
    overflow->holds = SIZE_MAX;
    const mc_tile_identity_set_status add_status =
        mc_tile_identity_set_add(set, overflow, &thread, &changed, &error);
    overflow->holds = restored_holds;
    saturated = NULL;
    CHECK(add_status == MC_TILE_IDENTITY_SET_TILE_FAILED &&
          error.tile_status == MC_TILE_ENTITY_LIMIT && !changed &&
          overflow->identity.hash == UINT32_C(0x80d) && overflow->holds == 1 &&
          mc_tile_identity_set_inspect(set, &after, NULL) == MC_TILE_IDENTITY_SET_OK &&
          same_shape(before, after));
    CHECK(mc_tile_identity_set_add(set, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(set, b, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_add(set, NULL, NULL, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          a->holds == 2 && b->holds == 2);
    CHECK(mc_tile_identity_set_to_array(set, &first, NULL) == MC_TILE_IDENTITY_SET_OK &&
          mc_tile_identity_set_to_array(set, &second, NULL) == MC_TILE_IDENTITY_SET_OK &&
          a->holds == 4 && b->holds == 4 &&
          mc_tile_identity_set_snapshot_inspect(first, &snapshot_view, NULL) ==
              MC_TILE_IDENTITY_SET_OK &&
          snapshot_view.count == 3 && snapshot_view.items[0] == NULL &&
          snapshot_view.items[1] == a && snapshot_view.items[2] == b);
    CHECK(mc_tile_identity_set_remove(set, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          changed && a->holds == 3 && snapshot_view.items[1] == a &&
          mc_tile_identity_set_retire(&set, &error) == MC_TILE_IDENTITY_SET_BUSY && set != NULL);
    CHECK(retire_snapshot(&first) && a->holds == 2 && b->holds == 3);
    CHECK(retire_snapshot(&second) && a->holds == 1 && b->holds == 2);
    CHECK(mc_tile_identity_set_add(set, a, &thread, &changed, NULL) == MC_TILE_IDENTITY_SET_OK &&
          changed && a->holds == 2);
    CHECK(mc_tile_identity_set_inspect(set, &before, NULL) == MC_TILE_IDENTITY_SET_OK &&
          before.snapshot_count == 0 && before.snapshot_bytes == 0);
    saturated = b;
    restored_holds = b->holds;
    b->holds = SIZE_MAX;
    const mc_tile_identity_set_status snapshot_status =
        mc_tile_identity_set_to_array(set, &first, &error);
    b->holds = restored_holds;
    saturated = NULL;
    CHECK(snapshot_status == MC_TILE_IDENTITY_SET_TILE_FAILED && first == NULL &&
          error.tile_status == MC_TILE_ENTITY_LIMIT && error.index == 2 && error.completed == 2 &&
          a->holds == 3 && b->holds == 2);
    CHECK(mc_tile_identity_set_inspect(set, &after, NULL) == MC_TILE_IDENTITY_SET_OK &&
          after.size == before.size && after.capacity == before.capacity &&
          after.mod_count == before.mod_count && after.snapshot_count == 1 &&
          after.failed_snapshots == 1 && after.live_snapshots == 0 && after.snapshot_bytes > 0 &&
          after.retained_bytes == before.retained_bytes + after.snapshot_bytes);
    CHECK(mc_tile_identity_set_to_array(set, &second, NULL) == MC_TILE_IDENTITY_SET_OK &&
          a->holds == 4 && b->holds == 3 &&
          mc_tile_identity_set_retire(&set, &error) == MC_TILE_IDENTITY_SET_BUSY);
    CHECK(retire_snapshot(&second) && a->holds == 3 && b->holds == 2);
    CHECK(retire_set(&set) && set == NULL && a->holds == 1 && b->holds == 1 && retire_set(&set));
    ok = true;
cleanup:
    if (saturated != NULL)
        saturated->holds = restored_holds;
    {
        const bool first_retired = retire_snapshot(&first);
        const bool second_retired = retire_snapshot(&second);
        const bool set_retired = retire_set(&set);
        return first_retired && second_retired && set_retired && ok;
    }
}

static bool run_side(mc_world_info_side side, mc_java_random_profile *profile,
                     mc_materials *materials) {
    bool ok = false;
    fixture f = {.main_thread = mc_java_random_profile_main_identity(profile), .side = side};
    const mc_registry_options options = {.side = side == MC_WORLD_INFO_CLIENT ? MC_REGISTRY_CLIENT
                                                                              : MC_REGISTRY_SERVER,
                                         .random_profile = profile,
                                         .materials = materials};
    CHECK(f.main_thread != NULL &&
          mc_registry_classes_allocate(&options, &f.registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(side, profile, &trig, &f.world_classes, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f.world_classes, f.registry, NULL) ==
              MC_REGISTRY_OK &&
          mc_tile_entity_classes_borrow(f.world_classes, &f.classes) == MC_TILE_ENTITY_OK);
    CHECK(identity_leaf(&f) && arguments_and_hash_timing(&f) && limits_and_allocation(&f) &&
          bulk_branches(&f) && bulk_add_prefix(&f) && equal_hash_tree(&f) && mixed_class_tree(&f) &&
          tree_split(&f) && snapshots_and_retains(&f));
    CHECK(mc_world_runtime_classes_free(f.world_classes, NULL) == MC_WORLD_STORAGE_BUSY);
    ok = true;
cleanup:
    if (retirement_failed)
        return false;
    for (size_t i = 0; i < f.count; ++i) {
        if (f.tiles[i]->holds != 1) {
            fprintf(stderr, "Tile identity test leaked holds: index=%zu holds=%zu\n", i,
                    f.tiles[i]->holds);
            retirement_failed = true;
            return false;
        }
    }
    for (size_t i = 0; i < f.count; ++i)
        mc_tile_entity_release(f.tiles[i]);
    if (mc_tile_entity_classes_release(&f.classes) != MC_TILE_ENTITY_OK ||
        mc_world_runtime_classes_free(f.world_classes, NULL) != MC_WORLD_STORAGE_OK) {
        retirement_failed = true;
        return false;
    }
    if (mc_registry_classes_free(f.registry, NULL) != MC_REGISTRY_OK) {
        retirement_failed = true;
        return false;
    }
    return ok;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    CHECK(mc_math_trig_init(&trig) && mc_materials_create(&materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(run_side(MC_WORLD_INFO_CLIENT, profile, materials) &&
          run_side(MC_WORLD_INFO_SERVER, profile, materials));
    result = 0;
cleanup:
    if (retirement_failed)
        return 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    if (result == 0 &&
        printf("PASS: %u Tile identity-set checks (authored native states)\n", checks) < 0)
        result = 1;
    return result;
}
