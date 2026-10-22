#define main mc_rails_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/lighting_internal.h"
#include "../../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../../src/world/runtime/mutation/rails/rail_connections_internal.h"
#include "../../../../src/world/runtime/mutation/rails/rail_power_search_internal.h"

/* Constructed native contracts, not original observations or placement tests.
 * Every World, provider, stored Chunk, Registry Block and notifying write is
 * genuine. Literal setup writes actual source bytes/nibbles. Main topology
 * cases keep ordinary World.o=false and require the complete integrated hooks.
 * Every temporary negative source control is restored before CHECK. */
static unsigned int rail_cases;

static void rails_clear(fixture *f, size_t side) {
    memset(f->states[side]->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(f->chunks[side]->e->a, 0, MC_CHUNK_NIBBLE_BYTES);
}

static bool rails_cell(fixture *f, size_t side, int32_t x, int32_t y, int32_t z, uint8_t id,
                       uint8_t metadata) {
    if (x < 0 || x >= 16 || z < 0 || z >= 16 || y < 0 || y >= 128 || metadata > 15)
        return false;
    const size_t cell = ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
    const unsigned int shift = ((unsigned int)y & 1u) * 4u;
    uint8_t *const data = f->chunks[side]->e->a;
    f->states[side]->data->blocks[cell] = id;
    data[cell >> 1u] = (uint8_t)((data[cell >> 1u] & (uint8_t) ~(15u << shift)) |
                                 ((unsigned int)metadata << shift));
    return true;
}

static bool rails_supported(fixture *f, size_t side, int32_t x, int32_t y, int32_t z, uint8_t id,
                            uint8_t metadata) {
    return rails_cell(f, side, x, y - 1, z, 1, 0) && rails_cell(f, side, x, y, z, id, metadata);
}

static bool rails_metadata(fixture *f, size_t side, int32_t x, int32_t y, int32_t z,
                           int32_t expected) {
    int32_t value = INT32_MIN;
    return mc_world_runtime_metadata_get(f->worlds[side], x, y, z, &value, NULL) ==
               MC_WORLD_STORAGE_OK &&
           value == expected;
}

typedef struct rail_light_record_snapshot {
    uintptr_t identity;
    mc_world_light_region_fields fields;
} rail_light_record_snapshot;

typedef struct rail_light_snapshot {
    mc_world_light_list_view list;
    uintptr_t items_identity;
    rail_light_record_snapshot *records;
    size_t total, owned, nonlighting;
} rail_light_snapshot;

static void rails_light_snapshot_release(rail_light_snapshot *snapshot) {
    free(snapshot->records);
    *snapshot = (rail_light_snapshot){0};
}

/* Passive actual C ownership: every live request record and the entire retained
 * pointer capacity. Its constructor-owned identity stays in the conserved
 * baseline. Snapshot bookkeeping uses the test heap, never the World's ledger.
 * Bounds are copied as observed: coalescing may change old records. */
static bool rails_light_snapshot_get(const mc_world_runtime *world, rail_light_snapshot *out) {
    bool ok = false;
    rail_light_snapshot snapshot = {0};
    const size_t active_calls = world->active_calls, leases = world->local_leases;
    snapshot.total = world->construction.direct_allocation_bytes;
    CHECK(mc_world_runtime_light_list_get(world, &snapshot.list, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(snapshot.list.identity != NULL && snapshot.list.identity == world->fields.C);
    const mc_world_reference_list *const list = snapshot.list.identity;
    snapshot.items_identity = (uintptr_t)list->items;
    CHECK(snapshot.list.capacity <= SIZE_MAX / sizeof(*list->items));
    snapshot.owned = snapshot.list.capacity * sizeof(*list->items);
    CHECK(snapshot.list.count <= (SIZE_MAX - snapshot.owned) / sizeof(mc_world_light_region));
    snapshot.owned += snapshot.list.count * sizeof(mc_world_light_region);
    CHECK(snapshot.total >= snapshot.owned);
    snapshot.nonlighting = snapshot.total - snapshot.owned;
    CHECK(snapshot.list.count <= SIZE_MAX / sizeof(*snapshot.records));
    if (snapshot.list.count != 0) {
        snapshot.records = calloc(snapshot.list.count, sizeof(*snapshot.records));
        CHECK(snapshot.records != NULL);
    }
    for (size_t i = 0; i < snapshot.list.count; ++i) {
        const mc_world_light_region *region = NULL;
        CHECK(mc_world_runtime_light_at(world, i, &region, NULL) == MC_WORLD_STORAGE_OK &&
              region != NULL);
        const mc_world_light_region_fields *const fields =
            mc_world_runtime_light_region_fields_get(region);
        CHECK(fields != NULL && (fields->a == NULL || fields->a->classes == world->classes));
        snapshot.records[i].identity = (uintptr_t)region;
        snapshot.records[i].fields = *fields;
        for (size_t j = 0; j < i; ++j)
            CHECK(snapshot.records[i].identity != snapshot.records[j].identity);
    }
    mc_world_light_list_view after = {0};
    CHECK(mc_world_runtime_light_list_get(world, &after, NULL) == MC_WORLD_STORAGE_OK &&
          after.identity == snapshot.list.identity && after.count == snapshot.list.count &&
          after.capacity == snapshot.list.capacity &&
          (uintptr_t)after.identity->items == snapshot.items_identity &&
          world->construction.direct_allocation_bytes == snapshot.total &&
          world->active_calls == active_calls && world->local_leases == leases);
    *out = snapshot;
    snapshot.records = NULL;
    ok = true;
cleanup:
    rails_light_snapshot_release(&snapshot);
    return ok;
}

static bool rails_graph_neutral(const mc_rail_connections_error *e) {
    return e->mutation.status == MC_WORLD_STORAGE_OK && e->mutation.operation == NULL &&
           e->mutation.hook_class == NULL && e->mutation.cause_hook_class == NULL &&
           e->mutation.registry.status == MC_REGISTRY_OK &&
           e->mutation.registry.initializer_pc == -1 && !e->connection.entered &&
           !e->cause_connection.entered && !e->predicate.entered && !e->accessor.entered;
}

static bool rails_search_neutral(const mc_rail_power_search_error *e) {
    return e->mutation.status == MC_WORLD_STORAGE_OK && e->mutation.operation == NULL &&
           e->mutation.hook_class == NULL && e->mutation.cause_hook_class == NULL &&
           !e->search.entered && !e->verify.entered && !e->cause.entered &&
           e->access.nearby.query.status == MC_WORLD_STORAGE_OK &&
           e->power.status == MC_WORLD_STORAGE_OK && !e->power.weak_neighbors.entered &&
           e->mutation.registry.status == MC_REGISTRY_OK &&
           e->mutation.registry.initializer_pc == -1;
}

typedef struct rail_endpoint {
    int32_t x, y, z;
    uint8_t final_metadata;
} rail_endpoint;

typedef struct rail_layout {
    bool powered;
    uint8_t final_metadata;
    size_t count;
    rail_endpoint endpoints[4];
} rail_layout;

/* Literal source cases, not an algorithm that predicts or rebuilds the graph.
 * Initial endpoint15 has a genuine empty a(I) List; final values below assert
 * actual synchronous reconnect writes. Extra cardinal endpoints in the four-
 * way cases remain15 when the selected curve does not reconnect them. */
static const rail_layout rail_layouts[] = {
    {false, 0, 2, {{8, 64, 7, 0}, {8, 64, 9, 0}}},
    {false, 1, 2, {{7, 64, 8, 1}, {9, 64, 8, 1}}},
    {false, 2, 2, {{7, 64, 8, 1}, {9, 65, 8, 1}}},
    {false, 3, 2, {{7, 65, 8, 1}, {9, 64, 8, 1}}},
    {false, 4, 2, {{8, 65, 7, 0}, {8, 64, 9, 0}}},
    {false, 5, 2, {{8, 64, 7, 0}, {8, 65, 9, 0}}},
    {false, 6, 2, {{9, 64, 8, 1}, {8, 64, 9, 0}}},
    {false, 7, 2, {{7, 64, 8, 1}, {8, 64, 9, 0}}},
    {false, 8, 2, {{7, 64, 8, 1}, {8, 64, 7, 0}}},
    {false, 9, 2, {{9, 64, 8, 1}, {8, 64, 7, 0}}},
    {false, 6, 4, {{8, 64, 7, 15}, {8, 64, 9, 0}, {7, 64, 8, 15}, {9, 64, 8, 1}}},
    {true, 8, 4, {{8, 64, 7, 0}, {8, 64, 9, 15}, {7, 64, 8, 1}, {9, 64, 8, 15}}},
    {false, 5, 2, {{8, 65, 7, 0}, {8, 65, 9, 0}}},
    {false, 3, 2, {{7, 65, 8, 1}, {9, 65, 8, 1}}}};

static bool rails_layout_matrix(fixture *f, size_t side) {
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_rail_connections *helper = NULL;
    const size_t baseline = w->construction.direct_allocation_bytes;
    CHECK(!w->fields.o && !w->fields.B &&
          mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    for (size_t i = 0; i < COUNT(rail_layouts); ++i) {
        const rail_layout *const row = &rail_layouts[i];
        ++rail_cases;
        rails_clear(f, side);
        CHECK(rails_supported(f, side, 8, 64, 8, 66, 0));
        for (size_t j = 0; j < row->count; ++j)
            CHECK(rails_supported(f, side, row->endpoints[j].x, row->endpoints[j].y,
                                  row->endpoints[j].z, 66, 15));
        mc_rail_connections_error error;
        memset(&error, 0xa5, sizeof(error));
        CHECK(mc_rail_connections_create(f->registry[side], f->registry[side]->blocks.m[66], w, 8,
                                         64, 8, &helper, &error) == MC_WORLD_STORAGE_OK &&
              helper != NULL && rails_graph_neutral(&error));
        int32_t degree = -1;
        CHECK(mc_rail_connections_degree(helper, &degree, &error) == MC_WORLD_STORAGE_OK &&
              degree == (int32_t)row->count && rails_graph_neutral(&error));
        CHECK(mc_rail_connections_reshape(helper, row->powered, true, &error) ==
                  MC_WORLD_STORAGE_OK &&
              rails_graph_neutral(&error) &&
              rails_metadata(f, side, 8, 64, 8, row->final_metadata));
        for (size_t j = 0; j < row->count; ++j)
            CHECK(rails_metadata(f, side, row->endpoints[j].x, row->endpoints[j].y,
                                 row->endpoints[j].z, row->endpoints[j].final_metadata));
        mc_rail_connections_destroy(helper);
        helper = NULL;
        CHECK(w->construction.direct_allocation_bytes == baseline && w->active_calls == 1 &&
              w->local_leases == 0);
    }
    ok = true;
cleanup:
    mc_rail_connections_destroy(helper);
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

static bool rails_flagged_and_force(fixture *f, size_t side) {
    static const uint8_t retained[] = {1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9};
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_registry_classes *const r = f->registry[side];
    mc_rail_connections *helper = NULL;
    const size_t baseline = w->construction.direct_allocation_bytes;
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    for (uint8_t raw = 0; raw < 16; ++raw) {
        ++rail_cases;
        rails_clear(f, side);
        CHECK(rails_supported(f, side, 8, 64, 8, 28, raw) &&
              rails_supported(f, side, 9, 64, 8, 66, 15) &&
              rails_supported(f, side, 8, 64, 9, 66, 15));
        mc_rail_connections_error error;
        CHECK(mc_rail_connections_create(r, r->blocks.m[28], w, 8, 64, 8, &helper, &error) ==
                  MC_WORLD_STORAGE_OK &&
              rails_graph_neutral(&error));
        CHECK(mc_rail_connections_reshape(helper, (raw & 1u) != 0, true, &error) ==
                  MC_WORLD_STORAGE_OK &&
              rails_graph_neutral(&error) && rails_metadata(f, side, 8, 64, 8, retained[raw]) &&
              rails_metadata(f, side, 9, 64, 8, 1) && rails_metadata(f, side, 8, 64, 9, 15));
        mc_rail_connections_destroy(helper);
        helper = NULL;
        CHECK(w->construction.direct_allocation_bytes == baseline);
    }
    /* Equality skips the notifying setter; force reaches the real t-array
     * failure after its quiet write. Both calls retain/rebuild actual helpers. */
    ++rail_cases;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 8, 64, 8, 28, 9) && rails_supported(f, side, 7, 64, 8, 66, 15) &&
          rails_supported(f, side, 9, 64, 8, 66, 15));
    mc_rail_connections_error error;
    CHECK(mc_rail_connections_create(r, r->blocks.m[28], w, 8, 64, 8, &helper, &error) ==
          MC_WORLD_STORAGE_OK);
    bool *const actual_t = r->blocks.t;
    r->blocks.t = NULL;
    const mc_world_storage_status equal = mc_rail_connections_reshape(helper, false, false, &error);
    r->blocks.t = actual_t;
    CHECK(equal == MC_WORLD_STORAGE_OK && rails_graph_neutral(&error) &&
          rails_metadata(f, side, 8, 64, 8, 9) && rails_metadata(f, side, 7, 64, 8, 15) &&
          rails_metadata(f, side, 9, 64, 8, 15));
    ++rail_cases;
    r->blocks.t = NULL;
    const mc_world_storage_status forced = mc_rail_connections_reshape(helper, false, true, &error);
    r->blocks.t = actual_t;
    CHECK(forced == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.connection.entered &&
          error.connection.bytecode == 603 && error.mutation.wrapper_bytecode == 30 &&
          error.mutation.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          rails_metadata(f, side, 8, 64, 8, 9));
    ok = true;
cleanup:
    mc_rail_connections_destroy(helper);
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

static bool rails_prune_admission(fixture *f, size_t side) {
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_registry_classes *const r = f->registry[side];
    mc_rail_connections *helper = NULL;
    const size_t baseline = w->construction.direct_allocation_bytes;
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    ++rail_cases;
    rails_clear(f, side);
    /* Each endpoint's old straight List contains one absent coordinate and
     * one genuine reciprocal center. Prune must remove then replace, keeping
     * its next source index correct. Existing connection admission still runs
     * the actual reconnect and its notifying equal-metadata write. */
    CHECK(rails_supported(f, side, 8, 64, 8, 66, 1) && rails_supported(f, side, 7, 64, 8, 66, 1) &&
          rails_supported(f, side, 9, 64, 8, 66, 1));
    mc_rail_connections_error error;
    CHECK(mc_rail_connections_create(r, r->blocks.m[66], w, 8, 64, 8, &helper, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_rail_connections_reshape(helper, false, true, &error) == MC_WORLD_STORAGE_OK &&
          rails_graph_neutral(&error) && rails_metadata(f, side, 8, 64, 8, 1) &&
          rails_metadata(f, side, 7, 64, 8, 1) && rails_metadata(f, side, 9, 64, 8, 1));
    int32_t degree = -1;
    CHECK(mc_rail_connections_degree(helper, &degree, &error) == MC_WORLD_STORAGE_OK &&
          degree == 2);
    mc_rail_connections_destroy(helper);
    helper = NULL;
    CHECK(w->construction.direct_allocation_bytes == baseline);
    ++rail_cases;
    rails_clear(f, side);
    /* The north rail has two real reciprocal E/W connections, neither the
     * center. Presence-degree is1, but that full neighbor rejects a new link. */
    CHECK(rails_supported(f, side, 8, 64, 8, 66, 0) && rails_supported(f, side, 8, 64, 7, 66, 1) &&
          rails_supported(f, side, 7, 64, 7, 66, 1) && rails_supported(f, side, 9, 64, 7, 66, 1));
    CHECK(mc_rail_connections_create(r, r->blocks.m[66], w, 8, 64, 8, &helper, &error) ==
          MC_WORLD_STORAGE_OK);
    degree = -1;
    CHECK(mc_rail_connections_degree(helper, &degree, &error) == MC_WORLD_STORAGE_OK &&
          degree == 1);
    CHECK(mc_rail_connections_reshape(helper, false, true, &error) == MC_WORLD_STORAGE_OK &&
          rails_graph_neutral(&error) && rails_metadata(f, side, 8, 64, 8, 0) &&
          rails_metadata(f, side, 8, 64, 7, 1) && rails_metadata(f, side, 7, 64, 7, 1) &&
          rails_metadata(f, side, 9, 64, 7, 1));
    mc_rail_connections_destroy(helper);
    helper = NULL;
    CHECK(w->construction.direct_allocation_bytes == baseline);
    ok = true;
cleanup:
    mc_rail_connections_destroy(helper);
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

static bool rails_partial_and_budget(fixture *f, size_t side) {
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_registry_classes *const r = f->registry[side];
    mc_rail_connections *helper = NULL;
    const size_t baseline = w->construction.direct_allocation_bytes;
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 8, 64, 8, 66, 0) && rails_supported(f, side, 7, 64, 8, 66, 15) &&
          rails_supported(f, side, 9, 64, 8, 66, 15));
    mc_rail_connections_error error;
    CHECK(mc_rail_connections_create(r, r->blocks.m[66], w, 8, 64, 8, &helper, &error) ==
          MC_WORLD_STORAGE_OK);
    ++rail_cases;
    bool *const actual_t = r->blocks.t;
    r->blocks.t = NULL;
    const mc_world_storage_status partial =
        mc_rail_connections_reshape(helper, false, true, &error);
    r->blocks.t = actual_t;
    CHECK(partial == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.connection.bytecode == 603 &&
          error.mutation.wrapper_bytecode == 30 &&
          error.mutation.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          rails_metadata(f, side, 8, 64, 8, 1) && rails_metadata(f, side, 7, 64, 8, 15) &&
          rails_metadata(f, side, 9, 64, 8, 15));
    mc_rail_connections_destroy(helper);
    helper = NULL;
    CHECK(w->construction.direct_allocation_bytes == baseline);
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 8, 64, 8, 66, 0));
    CHECK(mc_rail_connections_create(r, r->blocks.m[66], w, 8, 64, 8, &helper, &error) ==
          MC_WORLD_STORAGE_OK);
    const size_t held = w->construction.direct_allocation_bytes;
    const size_t charge = held - baseline;
    const size_t limit = w->construction_limits.max_direct_allocation_bytes;
    CHECK(charge > 1 && held <= SIZE_MAX - charge);
    mc_rail_connections *out = helper; /* A real live owner, never a forged sentinel. */
    ++rail_cases;
    w->construction_limits.max_direct_allocation_bytes = held;
    const mc_world_storage_status no_new =
        mc_rail_connections_create(r, NULL, w, 8, 64, 8, &out, &error);
    w->construction_limits.max_direct_allocation_bytes = limit;
    CHECK(no_new == MC_WORLD_STORAGE_LIMIT && out == helper && !error.connection.entered &&
          w->construction.direct_allocation_bytes == held);
    ++rail_cases;
    w->construction_limits.max_direct_allocation_bytes = held + charge - 1u;
    const mc_world_storage_status constructor =
        mc_rail_connections_create(r, NULL, w, 8, 64, 8, &out, &error);
    w->construction_limits.max_direct_allocation_bytes = limit;
    CHECK(constructor == MC_WORLD_STORAGE_LIMIT && out == helper && error.connection.entered &&
          error.connection.bytecode == 102 && error.cause_connection.entered &&
          error.cause_connection.bytecode == 48 && w->construction.direct_allocation_bytes == held);
    ++rail_cases;
    w->construction_limits.max_direct_allocation_bytes = held;
    const mc_world_storage_status reshape =
        mc_rail_connections_reshape(helper, false, true, &error);
    w->construction_limits.max_direct_allocation_bytes = limit;
    CHECK(reshape == MC_WORLD_STORAGE_LIMIT && error.connection.bytecode == 15 &&
          error.cause_connection.entered && error.cause_connection.bytecode == 1 &&
          w->construction.direct_allocation_bytes == held);
    CHECK(mc_rail_connections_reshape(helper, false, false, &error) == MC_WORLD_STORAGE_OK &&
          rails_graph_neutral(&error));
    ok = true;
cleanup:
    mc_rail_connections_destroy(helper);
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

static bool rails_source_guard(fixture *f, size_t side) {
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_registry_classes *const r = f->registry[side];
    mc_rail_connections *helper = NULL;
    CHECK(!w->fields.o && mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 8, 64, 8, 66, 0));
    mc_rail_connections_error error;
    CHECK(mc_rail_connections_create(r, r->blocks.m[66], w, 8, 64, 8, &helper, &error) ==
          MC_WORLD_STORAGE_OK);
    mc_block **const actual_m = r->blocks.m;
    ++rail_cases;
    w->fields.o = true;
    r->blocks.m = NULL;
    const mc_world_storage_status guarded =
        mc_rail_connections_reshape(helper, false, true, &error);
    r->blocks.m = actual_m;
    w->fields.o = false;
    CHECK(guarded == MC_WORLD_STORAGE_OK && rails_graph_neutral(&error));
    ++rail_cases;
    r->blocks.m = NULL;
    const mc_world_storage_status ordinary =
        mc_rail_connections_reshape(helper, false, true, &error);
    r->blocks.m = actual_m;
    CHECK(ordinary == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.connection.bytecode == 603 &&
          error.mutation.neighbor_call_bytecode == 8 && error.mutation.neighbor_bytecode == 25 &&
          error.mutation.registry.status == MC_REGISTRY_NULL_REFERENCE && !w->fields.o);
    ok = true;
cleanup:
    mc_rail_connections_destroy(helper);
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

typedef struct rail_search_row {
    int32_t metadata;
    bool forward;
    int32_t x, y, z;
    uint8_t target_metadata;
} rail_search_row;

static const rail_search_row rail_search_rows[] = {
    {0, true, 8, 64, 9, 8},  {0, false, 8, 64, 7, 8}, {1, true, 7, 64, 8, 9},
    {1, false, 9, 64, 8, 9}, {2, true, 7, 64, 8, 9},  {2, false, 9, 65, 8, 9},
    {3, true, 7, 65, 8, 9},  {3, false, 9, 64, 8, 9}, {4, true, 8, 64, 9, 8},
    {4, false, 8, 65, 7, 8}, {5, true, 8, 65, 9, 8},  {5, false, 8, 64, 7, 8},
    {6, true, 8, 64, 8, 8},  {7, false, 8, 64, 8, 8}};

static bool rails_search_matrix(fixture *f, size_t side) {
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    const mc_block *const block = f->registry[side]->blocks.m[27];
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    for (size_t i = 0; i < COUNT(rail_search_rows); ++i) {
        const rail_search_row *const row = &rail_search_rows[i];
        ++rail_cases;
        rails_clear(f, side);
        CHECK(rails_supported(f, side, row->x, row->y, row->z, 27, row->target_metadata) &&
              rails_cell(f, side, row->x, row->y + 1, row->z, 69, 8));
        bool out = false;
        mc_rail_power_search_error error;
        memset(&error, 0xa5, sizeof(error));
        CHECK(mc_rail_power_search_held(w, block, 8, 64, 8, row->metadata, row->forward, 0, &out,
                                        &error) == MC_WORLD_STORAGE_OK &&
              out && rails_search_neutral(&error));
    }
    ++rail_cases;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 8, 63, 9, 27, 8) && rails_cell(f, side, 9, 63, 9, 69, 8));
    bool out = false;
    mc_rail_power_search_error error;
    CHECK(mc_rail_power_search_held(w, block, 8, 64, 8, 0, true, 0, &out, &error) ==
              MC_WORLD_STORAGE_OK &&
          out && rails_search_neutral(&error));
    ++rail_cases;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 9, 64, 8, 27, 9) && rails_cell(f, side, 9, 64, 7, 69, 8));
    out = true;
    CHECK(mc_rail_power_search_held(w, block, 8, 64, 8, 2, false, 0, &out, &error) ==
              MC_WORLD_STORAGE_OK &&
          !out && rails_search_neutral(&error));
    ok = true;
cleanup:
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

static mc_block **rails_named(fixture *f, size_t side, uint8_t id) {
    mc_registry_classes *const r = f->registry[side];
    mc_block **found = NULL;
    for (size_t i = 0; i < COUNT(r->blocks.named); ++i)
        if (r->blocks.named[i] == r->blocks.m[id]) {
            if (found != NULL)
                return NULL;
            found = &r->blocks.named[i];
        }
    return found;
}

static bool rails_dispatch_callers(fixture *f, size_t side) {
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    const size_t baseline = w->construction.direct_allocation_bytes;
    rail_light_snapshot light_before = {0}, light_added = {0}, light_later = {0};
    CHECK(rails_light_snapshot_get(w, &light_before) && light_before.total == baseline);
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    ++rail_cases;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 7, 64, 8, 66, 0));
    mc_world_mutation_error error = mc_world_mutation_empty_error();
    mc_world_chunk_nibble *const actual_e = f->chunks[side]->e;
    f->chunks[side]->e = NULL;
    const bool neighbors = mc_world_mutation_neighbors(w, 8, 64, 8, 1, &error);
    f->chunks[side]->e = actual_e;
    CHECK(!neighbors && error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.neighbor_call_bytecode == 8 && error.neighbor_bytecode == 41 &&
          (error.rail_sites_selected & MC_WORLD_RAIL_SITE_NEIGHBOR) != 0 &&
          error.rail_neighbor.entered && error.rail_neighbor.bytecode == 13 &&
          error.rail_lifecycle_cause.bytecode == 13 &&
          error.rail_access.nearby.query.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          w->active_calls == 1 && w->local_leases == 0);

    ++rail_cases;
    rails_clear(f, side);
    CHECK(rails_cell(f, side, 8, 63, 8, 1, 0));
    mc_block **const named = rails_named(f, side, 66);
    CHECK(named != NULL);
    mc_block *const actual_named = *named;
    bool out = true;
    *named = NULL;
    const mc_world_storage_status added =
        mc_world_runtime_block_metadata_set_quiet(w, 8, 64, 8, 66, 0, &out, &error);
    *named = actual_named;
    int32_t added_id = -1;
    CHECK(added == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(out);
    CHECK(error.bytecode == 68);
    CHECK(error.chunk_bytecode == 294);
    CHECK((error.rail_sites_selected & MC_WORLD_RAIL_SITE_ADDED) != 0);
    CHECK(error.rail_added.entered);
    CHECK(error.rail_added.bytecode == 14);
    CHECK(error.rail_predicate_cause.entered);
    CHECK(error.rail_predicate_cause.bytecode == 14);
    CHECK(error.registry.status == MC_REGISTRY_NULL_REFERENCE);
    CHECK(mc_world_runtime_block_get(w, 8, 64, 8, &added_id, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(added_id == 66);
    CHECK(rails_metadata(f, side, 8, 64, 8, 0));
    CHECK(rails_light_snapshot_get(w, &light_added));
    CHECK(light_added.list.identity == light_before.list.identity);
    CHECK(light_added.nonlighting == light_before.nonlighting);
    CHECK(w->active_calls == 1);
    CHECK(w->local_leases == 0);

    ++rail_cases;
    rails_clear(f, side);
    CHECK(rails_cell(f, side, 8, 64, 8, 1, 0) && rails_supported(f, side, 7, 64, 8, 66, 0) &&
          rails_cell(f, side, 9, 64, 8, 69, 0) && !f->registry[side]->blocks.t[1]);
    const mc_world_storage_status later = mc_world_runtime_metadata_set(w, 8, 64, 8, 1, &error);
    CHECK(later == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.wrapper_bytecode == 52 &&
          error.neighbor_call_bytecode == 19 && error.neighbor_bytecode == 41 &&
          error.hook_class != NULL && strcmp(error.hook_class, side == 0 ? "xr" : "pd") == 0 &&
          error.rail_sites_selected == 0 && rails_metadata(f, side, 8, 64, 8, 1) &&
          rails_light_snapshot_get(w, &light_later) &&
          light_later.list.identity == light_before.list.identity &&
          light_later.nonlighting == light_before.nonlighting && w->active_calls == 1 &&
          w->local_leases == 0);
    ok = true;
cleanup:
    rails_light_snapshot_release(&light_before);
    rails_light_snapshot_release(&light_added);
    rails_light_snapshot_release(&light_later);
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

static bool rails_search_order(fixture *f, size_t side) {
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_registry_classes *const r = f->registry[side];
    const mc_block *const block = r->blocks.m[27];
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 8, 64, 8, 27, 8) && rails_cell(f, side, 8, 65, 8, 69, 8));
    mc_rail_power_search_error error;
    mc_block **const actual_m = r->blocks.m;
    bool out = true;
    ++rail_cases;
    r->blocks.m = NULL;
    const mc_world_storage_status axis =
        mc_rail_power_verify_held(w, block, 8, 64, 8, true, 0, 1, &out, &error);
    r->blocks.m = actual_m;
    CHECK(axis == MC_WORLD_STORAGE_OK && !out && rails_search_neutral(&error));
    ++rail_cases;
    CHECK(rails_cell(f, side, 8, 64, 8, 27, 0));
    out = true;
    r->blocks.m = NULL;
    const mc_world_storage_status bit =
        mc_rail_power_verify_held(w, block, 8, 64, 8, true, 0, 0, &out, &error);
    r->blocks.m = actual_m;
    CHECK(bit == MC_WORLD_STORAGE_OK && !out && rails_search_neutral(&error));
    ++rail_cases;
    CHECK(rails_cell(f, side, 8, 64, 8, 27, 8));
    out = true;
    r->blocks.m = NULL;
    const mc_world_storage_status reached_power =
        mc_rail_power_verify_held(w, block, 8, 64, 8, true, 0, 0, &out, &error);
    r->blocks.m = actual_m;
    CHECK(reached_power == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          error.verify.bytecode == 101 && error.cause.bytecode == 101 &&
          error.power.weak_neighbors.entered &&
          error.power.registry.status == MC_REGISTRY_NULL_REFERENCE);
    mc_block **const actual_u = rails_named(f, side, 27);
    CHECK(actual_u != NULL);
    mc_block *const saved_u = *actual_u;
    mc_world_chunk_nibble *const actual_e = f->chunks[side]->e;
    ++rail_cases;
    out = true;
    *actual_u = NULL;
    f->chunks[side]->e = NULL;
    const mc_world_storage_status field =
        mc_rail_power_verify_held(w, block, 8, 64, 8, true, 0, 0, &out, &error);
    f->chunks[side]->e = actual_e;
    *actual_u = saved_u;
    CHECK(field == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && error.verify.bytecode == 15 &&
          error.mutation.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          error.access.nearby.query.bytecode == 0);
    ++rail_cases;
    mc_world_provider *const actual_v = w->fields.v;
    out = true;
    *actual_u = NULL;
    w->fields.v = NULL;
    const mc_world_storage_status first_id =
        mc_rail_power_verify_held(w, block, 8, 64, 8, true, 0, 0, &out, &error);
    w->fields.v = actual_v;
    *actual_u = saved_u;
    CHECK(first_id == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && error.verify.bytecode == 5 &&
          error.access.nearby.query.bytecode == 48 &&
          error.access.nearby.query.provider_bytecode == 6 &&
          error.mutation.registry.status == MC_REGISTRY_OK);
    ++rail_cases;
    const int32_t saved_bn = saved_u->fields.bn;
    CHECK(rails_cell(f, side, 8, 64, 8, 1, 8));
    out = false;
    saved_u->fields.bn = 1;
    const mc_world_storage_status live_alias =
        mc_rail_power_verify_held(w, block, 8, 64, 8, true, 0, 0, &out, &error);
    saved_u->fields.bn = saved_bn;
    CHECK(live_alias == MC_WORLD_STORAGE_OK && out && rails_search_neutral(&error));
    ok = true;
cleanup:
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

static bool rails_namespace_and_guards(fixture *f, size_t side) {
    bool ok = false, entered = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_registry_classes *const r = f->registry[side], *const other = f->registry[1u - side];
    const mc_block *const block = r->blocks.m[27], *const foreign = other->blocks.m[27];
    mc_rail_connections *helper = NULL;
    const size_t baseline = w->construction.direct_allocation_bytes;
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 8, 64, 8, 27, 8) && rails_cell(f, side, 8, 65, 8, 69, 8));
    mc_block **const u = rails_named(f, side, 27),
                     **const foreign_u = rails_named(f, 1u - side, 27);
    CHECK(u != NULL && foreign_u != NULL);
    mc_block *const saved_u = *u, *const saved_foreign_u = *foreign_u;
    bool out = false;
    mc_rail_power_search_error error;
    ++rail_cases;
    *u = NULL;
    const mc_world_storage_status foreign_ok =
        mc_rail_power_verify_held(w, foreign, 8, 64, 8, true, 0, 0, &out, &error);
    *u = saved_u;
    CHECK(foreign_ok == MC_WORLD_STORAGE_OK && out && rails_search_neutral(&error));
    ++rail_cases;
    out = true;
    *foreign_u = NULL;
    const mc_world_storage_status foreign_fail =
        mc_rail_power_verify_held(w, foreign, 8, 64, 8, true, 0, 0, &out, &error);
    *foreign_u = saved_foreign_u;
    CHECK(foreign_fail == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          error.verify.bytecode == 15 &&
          strcmp(error.verify.declaring_class, side == 0 ? "jj" : "pc") == 0);
    ++rail_cases;
    rails_clear(f, side);
    CHECK(rails_supported(f, side, 8, 64, 8, 66, 0));
    mc_block **const actual_m = r->blocks.m;
    mc_rail_connections_error graph;
    r->blocks.m = NULL;
    const mc_world_storage_status graph_foreign =
        mc_rail_connections_create(other, NULL, w, 8, 64, 8, &helper, &graph);
    r->blocks.m = actual_m;
    CHECK(graph_foreign == MC_WORLD_STORAGE_OK && helper != NULL && rails_graph_neutral(&graph));
    mc_rail_connections_destroy(helper);
    helper = NULL;
    ++rail_cases;
    out = true;
    CHECK(mc_rail_power_search_held(NULL, block, INT32_MAX, INT32_MIN, INT32_MAX, -1, false, 8,
                                    &out, &error) == MC_WORLD_STORAGE_OK &&
          !out && rails_search_neutral(&error));
    ++rail_cases;
    out = true;
    CHECK(mc_rail_power_search_held(NULL, block, 8, 64, 8, 0, true, INT32_MAX, &out, &error) ==
              MC_WORLD_STORAGE_OK &&
          !out && rails_search_neutral(&error));
    ++rail_cases;
    out = true;
    CHECK(mc_rail_power_search_held(NULL, block, 8, 64, 8, 0, true, -1, &out, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out && error.search.bytecode == 207 && error.verify.bytecode == 5 &&
          error.cause.bytecode == 5);
    ++rail_cases;
    out = true;
    CHECK(mc_rail_power_search_held(NULL, NULL, 8, 64, 8, 0, true, 8, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out && !error.search.entered && !error.verify.entered && !error.cause.entered);
    CHECK(w->construction.direct_allocation_bytes == baseline && w->active_calls == 1 &&
          w->local_leases == 0);
    ok = true;
cleanup:
    mc_rail_connections_destroy(helper);
    if (entered)
        mc_world_runtime_call_leave(w);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    rail_light_snapshot light_before[2] = {0}, light_after[2] = {0};
    char root[8192];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    if (argc != 3 || strcmp(argv[1], "world-rails") != 0 ||
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
    for (size_t side = 0; side < 2; ++side)
        CHECK(open_store(f, side, root) && create_world(f, side));
    for (size_t side = 0; side < 2; ++side) {
        mc_world_runtime *const w = f->worlds[side];
        const size_t baseline = w->construction.direct_allocation_bytes;
        const mc_java_random random = *w->fields.r;
        const mc_world_reference_list *const actual_r = w->fields.R, *const actual_k = w->fields.K;
        CHECK(rails_light_snapshot_get(w, &light_before[side]) &&
              light_before[side].total == baseline);
        CHECK(rails_layout_matrix(f, side) && rails_flagged_and_force(f, side) &&
              rails_prune_admission(f, side) && rails_partial_and_budget(f, side) &&
              rails_source_guard(f, side) && rails_dispatch_callers(f, side) &&
              rails_search_matrix(f, side) && rails_search_order(f, side) &&
              rails_namespace_and_guards(f, side));
        CHECK(rails_light_snapshot_get(w, &light_after[side]) &&
              light_after[side].list.identity == light_before[side].list.identity &&
              light_after[side].nonlighting == light_before[side].nonlighting &&
              w->active_calls == 0 && w->local_leases == 0 && !w->fields.o &&
              w->fields.R == actual_r && actual_r->count == 0 && w->fields.K == actual_k &&
              actual_k->count == 0 && mc_test_java_random_equal(w->fields.r, &random) &&
              f->providers[side].cache->count == 1 &&
              f->providers[side].cache->insertion_count == 1);
    }
    CHECK(rail_cases == 144);
    result = 0;
cleanup:
    for (size_t side = 0; side < 2; ++side) {
        rails_light_snapshot_release(&light_before[side]);
        rails_light_snapshot_release(&light_after[side]);
    }
    if (!cleanup_fixture(f)) {
        fputs("Rail cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u rail checks (%u literal cases, world-rails)\n", checks, rail_cases) < 0)
        result = 1;
    return result;
}
