/* Keep the genuine GAME fixture and its named, unexecuted entry point. */
#define main mc_piston_finalize_fixture_main
#include "test_tiles.c"
#undef main

#include "../../../../src/world/runtime/tiles/tiles_internal.h"
#include "mc/world/runtime/tiles/piston.h"

/* Constructed native finalizer contracts, not tick/push qualification. Raw
 * progress and nullable source-reference inputs are explicitly marked below.
 * Every temporary source control is restored before CHECK can leave a scope.
 * No provider, Tile factory, collision result, hold or class phase is faked. */
static bool finalize_empty(const mc_world_mutation_error *e) {
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL &&
           e->cause.status == MC_WORLD_STORAGE_OK && e->cause.operation == NULL &&
           e->cause.system_error == 0 && e->piston_finalize_bytecode == 0 &&
           e->piston_removed_bytecode == 0 && e->cause_piston_finalize_bytecode == 0 &&
           e->cause_piston_removed_bytecode == 0 && e->wrapper_bytecode == 0 && e->bytecode == 0 &&
           e->chunk_bytecode == 0 && e->observer_bytecode == 0 && e->provider_bytecode == 0 &&
           e->hook_class == NULL && e->hook_method == NULL && e->hook_bytecode == 0 &&
           e->cause_hook_class == NULL && e->cause_hook_method == NULL &&
           e->cause_hook_bytecode == 0 && fresh_registry(&e->registry, MC_REGISTRY_OK) &&
           empty_tile_children(&e->tile);
}

static bool finalize_load(fixture *f, size_t slot) {
    bool ok = false;
    CHECK(seed_record(f, slot));
    mc_chunk_state *state = NULL;
    const mc_chunk_acquire_status status = mc_world_runtime_get(f->worlds[slot], 0, 0, &state);
    if (f->providers[slot].cache->insertion_count != 0)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(status == MC_CHUNK_ACQUIRE_OK && state != NULL && state == f->states[slot] &&
          state->active && state->terrain_populated && !state->dirty);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], state);
    CHECK(f->chunks[slot] != NULL && f->chunks[slot]->l->count == 0 &&
          f->worlds[slot]->fields.c->count == 0 && mc_live_tiles_map_valid(f->chunks[slot]) &&
          mc_live_tiles_lists_valid(f->worlds[slot]));
    ok = true;
cleanup:
    return ok;
}

static bool finalize_new(fixture *f, size_t side, int32_t id, int32_t data, mc_tile **out) {
    bool ok = false;
    mc_tile *tile = NULL;
    CHECK(mc_tile_allocate(f->worlds[side], MC_TILE_PISTON, &tile) == MC_TILE_OK);
    CHECK(mc_tile_piston_construct(tile, id, data, 5, true, true, NULL) == MC_TILE_OK &&
          tile->holds == 1 && tile->world == NULL && !tile->invalid &&
          mc_tile_source(tile) == NULL);
    *out = tile;
    tile = NULL;
    ok = true;
cleanup:
    mc_tile_release(tile);
    return ok;
}

/* All positive installations use the actual quiet setter, whose genuine moving
 * block added body returns without inventing a Tile, then the real Tile map. */
static bool finalize_install(fixture *f, size_t side, mc_tile *tile, int32_t x, int32_t metadata) {
    bool ok = false, changed = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_world_mutation_error mutation;
    mc_live_tile_error live;
    CHECK(mc_world_runtime_block_metadata_set_quiet(w, x, 40, 6, 36, metadata, &changed,
                                                    &mutation) == MC_WORLD_STORAGE_OK);
    CHECK(mc_live_tiles_set(w, x, 40, 6, tile, &live) == MC_WORLD_STORAGE_OK && tile->world == w &&
          tile->x == x && tile->y == 40 && tile->z == 6 && !tile->invalid && tile->holds == 3 &&
          mc_live_tiles_lists_valid(w) && mc_live_tiles_map_valid(f->chunks[side]));
    ok = true;
cleanup:
    return ok;
}

static bool finalize_cell_is(fixture *f, size_t side, int32_t x, int32_t id, int32_t metadata) {
    int32_t actual_id = INT32_MIN, actual_metadata = INT32_MIN;
    return mc_world_runtime_block_get(f->worlds[side], x, 40, 6, &actual_id, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_metadata_get(f->worlds[side], x, 40, 6, &actual_metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           actual_id == id && actual_metadata == metadata;
}

static bool finalize_receiver_and_guard(fixture *f, size_t side) {
    bool ok = false;
    mc_tile *tile = NULL, *raw = NULL, *chest = NULL;
    mc_world_mutation_error error;
    mc_tile_snapshot snapshot;
    mc_world_runtime *const w = f->worlds[side];
    const size_t bindings = w->persistent_bindings;
    CHECK(mc_tile_piston_finalize(NULL, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          error.piston_finalize_bytecode == 0 &&
          error.tile.tile.status == MC_TILE_INVALID_ARGUMENT);
    CHECK(mc_tile_allocate(w, MC_TILE_PISTON, &raw) == MC_TILE_OK &&
          mc_tile_piston_finalize(raw, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.piston_finalize_bytecode == 0 && error.tile.tile.status == MC_TILE_INVALID_PHASE &&
          raw->phase == MC_TILE_RAW && raw->holds == 1);
    CHECK(mc_tile_allocate(w, MC_TILE_CHEST, &chest) == MC_TILE_OK &&
          mc_tile_construct(chest) == MC_TILE_OK &&
          mc_tile_piston_finalize(chest, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.piston_finalize_bytecode == 0 && error.tile.tile.status == MC_TILE_INVALID_KIND &&
          chest->holds == 1 && !chest->invalid);
    CHECK(finalize_new(f, side, INT32_MIN, INT32_MAX, &tile));

    /* Exact representations include both signs and both NaN classes. These
     * independent private source-field inputs do not invoke a pretend tick. */
    static const struct {
        uint32_t previous;
        bool enters;
    } cases[] = {
        {UINT32_C(0x00000000), true},  {UINT32_C(0x80000000), true},  {UINT32_C(0x3e800000), true},
        {UINT32_C(0x3f7fffff), true},  {UINT32_C(0x3f800000), false}, {UINT32_C(0x3f800001), false},
        {UINT32_C(0x40000000), false}, {UINT32_C(0xbf800000), true},  {UINT32_C(0x00000001), true},
        {UINT32_C(0x80000001), true},  {UINT32_C(0x7f800000), false}, {UINT32_C(0xff800000), true},
        {UINT32_C(0x7fc12345), false}, {UINT32_C(0xffc12345), false}, {UINT32_C(0x7f812345), false},
        {UINT32_C(0xff812345), false}};

    for (size_t i = 0; i < COUNT(cases); ++i) {
        const uint32_t saved_current = tile->state.piston.progress_bits;
        const uint32_t saved_previous = tile->state.piston.previous_progress_bits;
        tile->state.piston.progress_bits = UINT32_C(0xff812346);
        tile->state.piston.previous_progress_bits = cases[i].previous;
        error = (mc_world_mutation_error){.piston_finalize_bytecode = 777,
                                          .piston_removed_bytecode = 778};
        const mc_world_storage_status status = mc_tile_piston_finalize(tile, &error);
        const mc_tile_status inspected = mc_tile_get_snapshot(tile, &snapshot);
        tile->state.piston.progress_bits = saved_current;
        tile->state.piston.previous_progress_bits = saved_previous;
        CHECK(inspected == MC_TILE_OK && snapshot.world == NULL && !snapshot.invalid &&
              snapshot.piston_block_id == INT32_MIN && snapshot.piston_block_data == INT32_MAX &&
              snapshot.piston_facing == 5 && snapshot.piston_extending &&
              snapshot.piston_render_head && tile->holds == 1 && mc_tile_source(tile) == NULL);
        if (cases[i].enters) {
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.status == status &&
                  error.piston_finalize_bytecode == 35 && error.piston_removed_bytecode == 0 &&
                  error.tile.status == status &&
                  snapshot.piston_progress_bits == UINT32_C(0x3f800000) &&
                  snapshot.piston_previous_progress_bits == UINT32_C(0x3f800000));
        } else {
            CHECK(status == MC_WORLD_STORAGE_OK && finalize_empty(&error) &&
                  snapshot.piston_progress_bits == UINT32_C(0xff812346) &&
                  snapshot.piston_previous_progress_bits == cases[i].previous);
        }
    }
    CHECK(mc_tile_bind_world(tile, w) == MC_TILE_OK &&
          mc_tile_set_coordinates(tile, 6, 40, 6) == MC_TILE_OK &&
          w->persistent_bindings == bindings + 1u);
    /* A genuine NaN guard must precede even a missing source World.v and an
     * unavailable parent Registry binding. The opaque Tile-class binding is
     * independent; this is not a PC61 missing-binding test. */
    const uint32_t saved_current = tile->state.piston.progress_bits;
    const uint32_t saved_previous = tile->state.piston.previous_progress_bits;
    mc_world_provider *const provider = w->fields.v;
    mc_registry_classes *const registry = f->classes[side]->registry;
    const mc_world_storage_error old_diagnostic = w->last_error;
    tile->state.piston.progress_bits = UINT32_C(0x7f812346);
    tile->state.piston.previous_progress_bits = UINT32_C(0xff812345);
    w->fields.v = NULL;
    f->classes[side]->registry = NULL;
    const mc_world_storage_status guarded = mc_tile_piston_finalize(tile, &error);
    const mc_tile_status inspected = mc_tile_get_snapshot(tile, &snapshot);
    f->classes[side]->registry = registry;
    w->fields.v = provider;
    tile->state.piston.progress_bits = saved_current;
    tile->state.piston.previous_progress_bits = saved_previous;
    CHECK(guarded == MC_WORLD_STORAGE_OK && finalize_empty(&error) && inspected == MC_TILE_OK &&
          snapshot.piston_progress_bits == UINT32_C(0x7f812346) &&
          snapshot.piston_previous_progress_bits == UINT32_C(0xff812345) && !snapshot.invalid &&
          w->active_calls == 0 && w->persistent_bindings == bindings + 1u &&
          w->last_error.status == old_diagnostic.status &&
          w->last_error.operation == old_diagnostic.operation &&
          w->last_error.system_error == old_diagnostic.system_error);
    mc_tile_release(tile);
    tile = NULL;
    CHECK(w->persistent_bindings == bindings);
    ok = true;
cleanup:
    mc_tile_release(tile);
    mc_tile_release(raw);
    mc_tile_release(chest);
    return ok;
}

static bool finalize_installed(fixture *f, size_t side) {
    bool ok = false;
    mc_tile *tile = NULL, *lookup = NULL;
    mc_world_runtime *const w = f->worlds[side];
    mc_world_mutation_error error;
    mc_live_tile_error live;
    mc_tile_snapshot snapshot;
    for (size_t mode = 0; mode < 3; ++mode) {
        const size_t active = w->fields.c->count, maps = f->chunks[side]->l->count;
        const size_t bindings = w->persistent_bindings;
        CHECK(finalize_new(f, side, 1, 11, &tile) && finalize_install(f, side, tile, 6, 2));
        if (mode == 2)
            CHECK(mc_live_tiles_set(w, 6, 40, 6, tile, &live) == MC_WORLD_STORAGE_OK &&
                  tile->holds == 4 && w->fields.c->count == active + 2u);
        const bool saved_l = w->fields.L;
        w->fields.L = mode == 1;
        const mc_world_storage_status status = mc_tile_piston_finalize(tile, &error);
        w->fields.L = saved_l;
        CHECK(status == MC_WORLD_STORAGE_OK && finalize_empty(&error) &&
              mc_tile_get_snapshot(tile, &snapshot) == MC_TILE_OK && snapshot.invalid &&
              snapshot.world == w && snapshot.x == 6 && snapshot.y == 40 && snapshot.z == 6 &&
              snapshot.piston_progress_bits == UINT32_C(0x3f800000) &&
              snapshot.piston_previous_progress_bits == UINT32_C(0x3f800000) &&
              snapshot.piston_block_id == 1 && snapshot.piston_block_data == 11 &&
              snapshot.piston_render_head && snapshot.piston_extending &&
              finalize_cell_is(f, side, 6, 1, 11));
        CHECK(f->chunks[side]->l->count == maps &&
              w->fields.c->count == active + (mode == 0 ? 0u : 1u) &&
              tile->holds == (mode == 0 ? 1u : 2u) && w->persistent_bindings == bindings + 1u &&
              w->active_calls == 0 && mc_live_tiles_map_valid(f->chunks[side]) &&
              mc_live_tiles_lists_valid(w));
        CHECK(mc_live_tiles_get(w, 6, 40, 6, &lookup, &live) == MC_WORLD_STORAGE_OK &&
              lookup == NULL);
        /* Reentry after previous reached1 is an actual no-op; in particular it
         * must not remove another c occurrence or repeat notifications. */
        const size_t retained_count = w->fields.c->count;
        CHECK(mc_tile_piston_finalize(tile, &error) == MC_WORLD_STORAGE_OK &&
              finalize_empty(&error) && w->fields.c->count == retained_count &&
              tile->holds == (mode == 0 ? 1u : 2u) && finalize_cell_is(f, side, 6, 1, 11));
        mc_tile_release(tile);
        tile = NULL;
        CHECK(w->persistent_bindings == bindings + (mode == 0 ? 0u : 1u));
    }
    ok = true;
cleanup:
    mc_tile_release(lookup);
    mc_tile_release(tile);
    return ok;
}

static bool finalize_different_and_false(fixture *f, size_t side) {
    bool ok = false, changed = true;
    mc_tile *self = NULL, *other = NULL;
    mc_world_runtime *const w = f->worlds[side];
    mc_world_mutation_error error;
    const size_t active = w->fields.c->count;
    CHECK(finalize_new(f, side, 1, 7, &self) &&
          mc_tile_allocate(w, MC_TILE_CHEST, &other) == MC_TILE_OK &&
          mc_tile_construct(other) == MC_TILE_OK && finalize_install(f, side, other, 7, 3));
    CHECK(mc_tile_bind_world(self, w) == MC_TILE_OK &&
          mc_tile_set_coordinates(self, 7, 40, 6) == MC_TILE_OK && self != other);
    CHECK(mc_tile_piston_finalize(self, &error) == MC_WORLD_STORAGE_OK && finalize_empty(&error) &&
          self->invalid && other->invalid && self->holds == 1 && other->holds == 1 &&
          self->world == w && other->world == w && w->fields.c->count == active &&
          f->chunks[side]->l->count == 0 && finalize_cell_is(f, side, 7, 1, 7));
    mc_tile_release(self);
    self = NULL;
    mc_tile_release(other);
    other = NULL;

    /* The current nonmoving ID suppresses the setter even for a nonsensical
     * carried ID. Nullable World.u is an explicit unreachable source input. */
    CHECK(finalize_new(f, side, INT32_MIN, INT32_MAX, &self) &&
          mc_tile_bind_world(self, w) == MC_TILE_OK &&
          mc_tile_set_coordinates(self, 7, 40, 6) == MC_TILE_OK);
    mc_world_reference_list *const observers = w->fields.u;
    w->fields.u = NULL;
    const mc_world_storage_status nonmoving = mc_tile_piston_finalize(self, &error);
    w->fields.u = observers;
    CHECK(nonmoving == MC_WORLD_STORAGE_OK && finalize_empty(&error) && self->invalid &&
          self->holds == 1 && finalize_cell_is(f, side, 7, 1, 7) && w->fields.c->count == active);
    mc_tile_release(self);
    self = NULL;

    /* Keep this retained moving Block away from later notifying restorations. */
    CHECK(finalize_new(f, side, 36, 2, &self) && finalize_install(f, side, self, 2, 2));
    CHECK(mc_world_runtime_block_metadata_set(w, 2, 40, 6, 36, 2, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          !changed && self->holds == 3 && !self->invalid);
    w->fields.u = NULL;
    const mc_world_storage_status ordinary_false = mc_tile_piston_finalize(self, &error);
    w->fields.u = observers;
    CHECK(ordinary_false == MC_WORLD_STORAGE_OK && finalize_empty(&error) && self->invalid &&
          self->holds == 1 && w->fields.c->count == active && f->chunks[side]->l->count == 0 &&
          finalize_cell_is(f, side, 2, 36, 2));
    CHECK(mc_live_tiles_lists_valid(w) && mc_live_tiles_map_valid(f->chunks[side]));
    ok = true;
cleanup:
    mc_tile_release(self);
    mc_tile_release(other);
    return ok;
}

static bool finalize_failure_prefixes(fixture *f, size_t side) {
    bool ok = false;
    mc_tile *tile = NULL;
    mc_world_runtime *const w = f->worlds[side];
    mc_world_mutation_error error;
    const size_t active = w->fields.c->count;
    CHECK(finalize_new(f, side, 1, 9, &tile) && finalize_install(f, side, tile, 9, 0));
    mc_world_provider *const provider = w->fields.v;
    mc_world_runtime *const other = f->worlds[side + 2u];
    mc_chunk_cache *const own_cache = f->providers[side].cache;
    mc_chunk_cache *const other_cache = f->providers[side + 2u].cache;
    const size_t own_count = own_cache->count, own_insertions = own_cache->insertion_count;
    const size_t other_count = other_cache->count, other_insertions = other_cache->insertion_count;
    const size_t own_pending = f->providers[side].pending->count;
    const size_t other_pending = f->providers[side + 2u].pending->count;
    const size_t own_bindings = w->persistent_bindings, other_bindings = other->persistent_bindings;
    CHECK(other != w && other->classes == w->classes && other->fields.v != NULL &&
          other->fields.v != provider && own_cache != other_cache && own_count == 1u &&
          other_count == 0 && other_insertions == 0 && other->fields.c->count == 0);
    /* Native source-reference fault using another complete, actual same-side
     * provider. No cache/Store coordinator is replaced or manufactured. */
    w->fields.v = other->fields.v;
    const mc_world_storage_status foreign_provider = mc_tile_piston_finalize(tile, &error);
    w->fields.v = provider;
    CHECK(foreign_provider == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.status == foreign_provider && error.piston_finalize_bytecode == 35 &&
          error.piston_removed_bytecode == 0 && error.provider_bytecode == 6 &&
          error.tile.provider_bytecode == 6 && error.tile.bytecode == 7 &&
          error.tile.remove_bytecode == 4 && error.tile.status == foreign_provider &&
          error.tile.cause.status == foreign_provider && error.operation != NULL &&
          strcmp(error.operation, "World Tile replacement provider binding") == 0 &&
          error.operation == error.tile.operation &&
          error.cause.operation == error.tile.cause.operation &&
          error.cause.system_error == error.tile.cause.system_error &&
          error.cause.status == error.tile.cause.status && error.tile.tile.status == MC_TILE_OK &&
          fresh_registry(&error.registry, MC_REGISTRY_OK) &&
          tile->state.piston.progress_bits == UINT32_C(0x3f800000) &&
          tile->state.piston.previous_progress_bits == UINT32_C(0x3f800000) && !tile->invalid &&
          tile->holds == 3 && w->fields.c->count == active + 1u && f->chunks[side]->l->count == 1 &&
          w->active_calls == 0 && other->active_calls == 0 &&
          w->persistent_bindings == own_bindings && other->persistent_bindings == other_bindings &&
          own_cache->count == own_count && own_cache->insertion_count == own_insertions &&
          other_cache->count == other_count && other_cache->insertion_count == other_insertions &&
          f->providers[side].pending->count == own_pending &&
          f->providers[side + 2u].pending->count == other_pending && other->fields.c->count == 0 &&
          mc_live_tiles_map_valid(f->chunks[side]) && mc_live_tiles_lists_valid(w) &&
          mc_live_tiles_lists_valid(other) && finalize_cell_is(f, side, 9, 36, 0));
    /* Independent NULL-v attempt: explicitly prepare only raw progress inputs,
     * snapshot the actual reached stores, then restore both controls before
     * any assertion. This is not a production retry or tick operation. */
    const uint32_t null_saved_current = tile->state.piston.progress_bits;
    const uint32_t null_saved_previous = tile->state.piston.previous_progress_bits;
    tile->state.piston.progress_bits = 0;
    tile->state.piston.previous_progress_bits = 0;
    w->fields.v = NULL; /* Native nullable World.v source-reference fault. */
    const mc_world_storage_status absent_provider = mc_tile_piston_finalize(tile, &error);
    mc_tile_snapshot null_snapshot;
    const mc_tile_status null_inspected = mc_tile_get_snapshot(tile, &null_snapshot);
    w->fields.v = provider;
    tile->state.piston.progress_bits = null_saved_current;
    tile->state.piston.previous_progress_bits = null_saved_previous;
    CHECK(absent_provider != MC_WORLD_STORAGE_OK && error.status == absent_provider &&
          error.piston_finalize_bytecode == 35 && error.tile.status == absent_provider &&
          null_inspected == MC_TILE_OK &&
          null_snapshot.piston_progress_bits == UINT32_C(0x3f800000) &&
          null_snapshot.piston_previous_progress_bits == UINT32_C(0x3f800000) && !tile->invalid &&
          tile->holds == 3 && w->fields.c->count == active + 1u && f->chunks[side]->l->count == 1 &&
          w->active_calls == 0 && finalize_cell_is(f, side, 9, 36, 0));
    /* Reset only the explicit prepared progress fields for this independent
     * attempt. This is not a supported production retry/tick operation. */
    const uint32_t current = tile->state.piston.progress_bits;
    const uint32_t previous = tile->state.piston.previous_progress_bits;
    tile->state.piston.progress_bits = 0;
    tile->state.piston.previous_progress_bits = 0;
    const mc_world_storage_status retried = mc_tile_piston_finalize(tile, &error);
    const bool retried_invalid = tile->invalid;
    tile->state.piston.progress_bits = current;
    tile->state.piston.previous_progress_bits = previous;
    CHECK(retried == MC_WORLD_STORAGE_OK && finalize_empty(&error) && retried_invalid &&
          tile->holds == 1 && w->fields.c->count == active && finalize_cell_is(f, side, 9, 1, 9));
    mc_tile_release(tile);
    tile = NULL;

    /* A real nonresident Chunk request against an already full native Store
     * cannot be replaced with air. No provider or capacity is substituted. */
    CHECK(finalize_new(f, side, 1, 0, &tile) && mc_tile_bind_world(tile, w) == MC_TILE_OK &&
          mc_tile_set_coordinates(tile, 32, 40, 6) == MC_TILE_OK);
    const size_t insertions = f->providers[side].cache->insertion_count;
    const mc_world_storage_status acquire = mc_tile_piston_finalize(tile, &error);
    CHECK(acquire != MC_WORLD_STORAGE_OK && error.status == acquire &&
          error.piston_finalize_bytecode == 35 && error.tile.status == acquire &&
          tile->state.piston.progress_bits == UINT32_C(0x3f800000) &&
          tile->state.piston.previous_progress_bits == UINT32_C(0x3f800000) && !tile->invalid &&
          tile->holds == 1 && w->fields.c->count == active &&
          f->providers[side].cache->insertion_count == insertions && w->active_calls == 0);
    mc_tile_release(tile);
    tile = NULL;

    CHECK(finalize_new(f, side, 1, 12, &tile) && finalize_install(f, side, tile, 10, 0));
    const mc_block *moving = NULL;
    CHECK(mc_registry_block_static_get(f->registry[side], "ad", &moving, NULL) == MC_REGISTRY_OK &&
          moving != NULL && moving->fields.bn == 36);
    size_t ad_index = COUNT(f->registry[side]->blocks.named);
    for (size_t i = 0; i < COUNT(f->registry[side]->blocks.named); ++i)
        if (strcmp(mc_registry_block_static_name(i), "ad") == 0)
            ad_index = i;
    CHECK(ad_index < COUNT(f->registry[side]->blocks.named) &&
          f->registry[side]->blocks.named[ad_index] == moving);
    mc_block *const saved_ad = f->registry[side]->blocks.named[ad_index];
    f->registry[side]->blocks.named[ad_index] = NULL; /* Actual live source static. */
    const mc_world_storage_status null_ad = mc_tile_piston_finalize(tile, &error);
    f->registry[side]->blocks.named[ad_index] = saved_ad;
    CHECK(null_ad == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.status == null_ad &&
          error.piston_finalize_bytecode == 64 &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE && error.operation != NULL &&
          strcmp(error.operation, "Piston finalizer live ad receiver is null") == 0 &&
          tile->invalid && tile->holds == 1 && w->fields.c->count == active &&
          f->chunks[side]->l->count == 0 &&
          tile->state.piston.progress_bits == UINT32_C(0x3f800000) &&
          tile->state.piston.previous_progress_bits == UINT32_C(0x3f800000) &&
          finalize_cell_is(f, side, 10, 36, 0) && w->active_calls == 0);
    mc_tile_release(tile);
    tile = NULL;

    /* World parent Registry is needed by the reached setter, independently of
     * the Tile-class Registry already used for ad61. Initial real map lookup
     * and removal complete without needing that parent Registry. */
    CHECK(finalize_new(f, side, 36, 4, &tile) && finalize_install(f, side, tile, 11, 0));
    mc_registry_classes *const registry = f->classes[side]->registry;
    f->classes[side]->registry = NULL;
    const mc_world_storage_status registry_failed = mc_tile_piston_finalize(tile, &error);
    f->classes[side]->registry = registry;
    CHECK(registry_failed != MC_WORLD_STORAGE_OK && error.status == registry_failed &&
          error.piston_finalize_bytecode == 94 && error.registry.status != MC_REGISTRY_OK &&
          tile->invalid && tile->holds == 1 && w->fields.c->count == active &&
          f->chunks[side]->l->count == 0 &&
          tile->state.piston.progress_bits == UINT32_C(0x3f800000) &&
          tile->state.piston.previous_progress_bits == UINT32_C(0x3f800000) &&
          finalize_cell_is(f, side, 11, 36, 0) && w->active_calls == 0);
    mc_tile_release(tile);
    tile = NULL;

    /* A late actual observer receiver failure preserves the real quiet
     * setter's ID/metadata prefix and all completed Tile removals. */
    CHECK(finalize_new(f, side, 36, 6, &tile) && finalize_install(f, side, tile, 12, 0));
    mc_world_reference_list *const observers = w->fields.u;
    w->fields.u = NULL;
    const mc_world_storage_status observer_failed = mc_tile_piston_finalize(tile, &error);
    w->fields.u = observers;
    CHECK(observer_failed == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.status == observer_failed && error.piston_finalize_bytecode == 94 &&
          error.observer_bytecode == 9 && error.operation != NULL &&
          strcmp(error.operation, "World observer size receiver metadata") == 0 && tile->invalid &&
          tile->holds == 1 && w->fields.c->count == active && f->chunks[side]->l->count == 0 &&
          finalize_cell_is(f, side, 12, 36, 6) &&
          tile->state.piston.progress_bits == UINT32_C(0x3f800000) &&
          tile->state.piston.previous_progress_bits == UINT32_C(0x3f800000) &&
          w->active_calls == 0);
    CHECK(mc_live_tiles_lists_valid(w) && mc_live_tiles_map_valid(f->chunks[side]));
    ok = true;
cleanup:
    mc_tile_release(tile);
    return ok;
}

static bool finalize_removed_hook(fixture *f, size_t side) {
    bool ok = false, changed = false, entered = false;
    mc_tile *tile = NULL;
    mc_world_runtime *const w = f->worlds[side];
    mc_world_mutation_error error;
    mc_live_tile_error live;
    const size_t active = w->fields.c->count;
    CHECK(!w->fields.B && !w->fields.L && w->active_calls == 0);
    CHECK(finalize_new(f, side, 4, 13, &tile) && finalize_install(f, side, tile, 13, 0));
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    CHECK(w->active_calls == 1 &&
          mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_BUSY);
    /* The actual outer Chunk store changes36->1 before invoking ut/mz.b.
     * Its fetched Piston enters the finalizer, whose ID58 now sees1 and must
     * suppress restoration of the different carried4/data13. */
    CHECK(mc_world_runtime_block_metadata_set_quiet(w, 13, 40, 6, 1, 5, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && finalize_empty(&error) && tile->invalid && tile->holds == 1 &&
          tile->state.piston.progress_bits == UINT32_C(0x3f800000) &&
          tile->state.piston.previous_progress_bits == UINT32_C(0x3f800000) &&
          w->fields.c->count == active && f->chunks[side]->l->count == 0 &&
          finalize_cell_is(f, side, 13, 1, 5) && w->active_calls == 1);
    mc_world_runtime_call_leave(w);
    entered = false;
    CHECK(w->active_calls == 0);
    mc_tile_release(tile);
    tile = NULL;

    /* A real non-Piston Tile takes the invokespecial container fallback. */
    CHECK(mc_tile_allocate(w, MC_TILE_CHEST, &tile) == MC_TILE_OK &&
          mc_tile_construct(tile) == MC_TILE_OK && finalize_install(f, side, tile, 14, 0));
    CHECK(mc_world_runtime_block_metadata_set_quiet(w, 14, 40, 6, 1, 3, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && finalize_empty(&error) && tile->invalid && tile->holds == 1 &&
          w->fields.c->count == active && f->chunks[side]->l->count == 0 &&
          finalize_cell_is(f, side, 14, 1, 3));
    mc_tile_release(tile);
    tile = NULL;

    CHECK(mc_world_runtime_block_metadata_set_quiet(w, 14, 40, 6, 36, 0, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(mc_live_tiles_get(w, 14, 40, 6, &tile, &live) == MC_WORLD_STORAGE_OK && tile == NULL);
    CHECK(mc_world_runtime_block_metadata_set_quiet(w, 14, 40, 6, 1, 8, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && finalize_empty(&error) && w->fields.c->count == active &&
          f->chunks[side]->l->count == 0 && finalize_cell_is(f, side, 14, 1, 8) &&
          w->active_calls == 0 && mc_live_tiles_lists_valid(w) &&
          mc_live_tiles_map_valid(f->chunks[side]));
    ok = true;
cleanup:
    if (entered)
        mc_world_runtime_call_leave(w);
    mc_tile_release(tile);
    return ok;
}

static bool finalize_last_owners(fixture *f, size_t side) {
    bool ok = false;
    mc_tile *tile = NULL;
    mc_world_runtime *const w = f->worlds[side];
    mc_world_mutation_error error;
    mc_tile_piston_class_snapshot before, after;
    const size_t bindings = w->persistent_bindings;
    CHECK(finalize_new(f, side, 1, 0, &tile) && finalize_install(f, side, tile, 5, 0));
    const bool saved_l = w->fields.L;
    w->fields.L = true;
    const mc_world_storage_status status = mc_tile_piston_finalize(tile, &error);
    w->fields.L = saved_l;
    CHECK(status == MC_WORLD_STORAGE_OK && finalize_empty(&error) && tile->holds == 2 &&
          tile->invalid && f->chunks[side]->l->count == 0 &&
          w->persistent_bindings == bindings + 1u);
    mc_tile_release(tile);
    tile = NULL; /* The invalid source c occurrence is now the last Tile hold. */
    CHECK(w->persistent_bindings == bindings + 1u && mc_live_tiles_lists_valid(w));
    CHECK(finalize_new(f, side, 1, 0, &f->aliases[side]) &&
          mc_tile_bind_world(f->aliases[side], w) == MC_TILE_OK && f->aliases[side]->holds == 1 &&
          mc_tile_piston_class_get_snapshot(f->classes[side], &before) == MC_TILE_OK &&
          before.live_tiles >= 2u);
    CHECK(mc_world_store_lease_chunk(f->stores[side], f->states[side], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_world_storage_status blocked = mc_world_runtime_retire_entity_containers(w, NULL);
    const mc_world_storage_status released =
        mc_world_store_release_chunk(f->stores[side], f->states[side], NULL);
    CHECK(blocked == MC_WORLD_STORAGE_BUSY && released == MC_WORLD_STORAGE_OK &&
          mc_live_tiles_lists_valid(w) && !f->terminal[side]);
    const mc_world_storage_status retired = mc_world_runtime_retire_entity_containers(w, NULL);
    if (retired == MC_WORLD_STORAGE_OK)
        f->terminal[side] = true;
    CHECK(retired == MC_WORLD_STORAGE_OK && w->fields.c->count == 0 && w->fields.G->count == 0 &&
          f->chunks[side]->l->count == 0 &&
          mc_tile_piston_class_get_snapshot(f->classes[side], &after) == MC_TILE_OK &&
          after.live_tiles == 1 && after.live_tiles < before.live_tiles &&
          w->persistent_bindings == 2u && mc_live_tiles_lists_valid(w));
    CHECK(retire_descriptors(f, side));
    const mc_world_storage_status world_retired = mc_world_runtime_retire_world(w, NULL);
    if (world_retired == MC_WORLD_STORAGE_OK)
        f->retired[side] = true;
    CHECK(world_retired == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY && f->aliases[side]->world == w &&
          f->aliases[side]->holds == 1);
    mc_tile_release(f->aliases[side]);
    f->aliases[side] = NULL;
    CHECK(w->persistent_bindings == 0 &&
          mc_tile_piston_class_get_snapshot(f->classes[side], &after) == MC_TILE_OK &&
          after.live_tiles == 0);
    ok = true;
cleanup:
    mc_tile_release(tile);
    return ok;
}

static bool finalize_disk_unchanged(fixture *f, size_t side) {
    bool ok = false;
    char path[8192], baseline_path[8192];
    mc_ws_file_bytes current = {0}, saved = {0};
    const int a = snprintf(path, sizeof(path), "%s/world%zu/region/r.0.0.mcr", f->root, side);
    const int b = snprintf(baseline_path, sizeof(baseline_path), "%s/seed-world%zu.region.bin",
                           f->root, side);
    CHECK(a > 0 && (size_t)a < sizeof(path) && b > 0 && (size_t)b < sizeof(baseline_path));
    CHECK(mc_ws_read_file(path, MC_WS_FILE_MAX_BYTES, &current, NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_read_file(baseline_path, MC_WS_FILE_MAX_BYTES, &saved, NULL) ==
              MC_WORLD_STORAGE_OK);
    CHECK(current.length == saved.length && current.length >= 8192u &&
          memcmp(current.data, saved.data, current.length) == 0);
    CHECK(current.data[4096] == 0xfe && current.data[4097] == 0xdc && current.data[4098] == 0xba &&
          current.data[4099] == 0x98);
    ok = true;
cleanup:
    mc_ws_free_file_bytes(&current);
    mc_ws_free_file_bytes(&saved);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'F', 'i', 'n', 'a', 'l', 'i', 'z', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-piston-finalization") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    f->root = root;
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
        const mc_block *chest = NULL, *spawner = NULL;
        CHECK(mc_registry_block_static_get(f->registry[side], "av", &chest, NULL) ==
                  MC_REGISTRY_OK &&
              mc_registry_block_static_get(f->registry[side], "at", &spawner, NULL) ==
                  MC_REGISTRY_OK &&
              chest != NULL && spawner != NULL);
        f->chest_id[side] = mc_block_fields_get(chest)->bn;
        f->spawner_id[side] = mc_block_fields_get(spawner)->bn;
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(open_store(f, slot, root) && create_world(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(finalize_load(f, side) && finalize_receiver_and_guard(f, side) &&
              finalize_installed(f, side) && finalize_different_and_false(f, side) &&
              finalize_failure_prefixes(f, side) && finalize_removed_hook(f, side) &&
              finalize_disk_unchanged(f, side) && finalize_last_owners(f, side));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Piston finalization cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u Piston finalization checks (world-piston-finalization)\n", checks) < 0)
        result = 1;
    return result;
}
