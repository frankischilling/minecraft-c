/* Reuse genuine Store/Registry/Entity ownership and codec helpers only. The
 * renamed publication main is not run. These are native source contracts,
 * not an original-JAR replay or a queue-draining game loop. */
#define main mc_pressure_publication_fixture_main
#include "test_entity_publication.c"
#undef main

#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/world/runtime/mutation/mutation_internal.h"
#include "mc/world/runtime/contact.h"
#include "mc/world/runtime/scheduling.h"

static mc_chunk_state *plate_extra[WORLD_COUNT][3];

static bool plate_at(mc_world_runtime *world, int32_t x, int32_t metadata) {
    int32_t id = -1, value = -1;
    return mc_world_runtime_block_get(world, x, 64, 8, &id, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_metadata_get(world, x, 64, 8, &value, NULL) == MC_WORLD_STORAGE_OK &&
           id == (x == 10 ? 70 : 72) && value == metadata;
}

static bool plate_counts(mc_world_runtime *world, int32_t ordered, int32_t membership) {
    int32_t e = -1, f = -1;
    return mc_scheduled_ordered_size(world->fields.E, &e) == MC_SCHEDULED_ORDERED_OK &&
           mc_scheduled_membership_size(world->fields.F, &f) == MC_SCHEDULED_MEMBERSHIP_OK &&
           e == ordered && f == membership;
}

static bool plate_clear(const mc_world_mutation_error *error) {
    return error->status == MC_WORLD_STORAGE_OK && error->hook_class == NULL &&
           error->hook_method == NULL && error->pressure_contact_bytecode == 0 &&
           error->pressure_tick_bytecode == 0 && error->pressure_bytecode == 0 &&
           error->pressure_neighbor_bytecode == 0 && error->pressure_removed_bytecode == 0;
}

/* The generic virtual dispatch receives the same genuinely held World that
 * the scheduler would supply. No immediate scheduling or recursion is used. */
static bool plate_tick(mc_world_runtime *world, const mc_block *block,
                       mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed = mc_world_mutation_block_tick(world, block, 8, 64, 8, NULL, error);
    mc_world_runtime_call_leave(world);
    return completed;
}

static bool plate_update(mc_world_runtime *world, const mc_block *block,
                         mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed = mc_world_mutation_pressure_update(world, block, 8, 64, 8, error);
    mc_world_runtime_call_leave(world);
    return completed;
}

static bool plate_neighbor(mc_world_runtime *world, const mc_block *block,
                           mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed = mc_world_mutation_block_neighbor(world, block, 8, 64, 8, 123, error);
    mc_world_runtime_call_leave(world);
    return completed;
}

static bool plate_removed(mc_world_runtime *world, const mc_block *block,
                          mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed = mc_world_mutation_block_removed(world, block, 8, 64, 8, error);
    mc_world_runtime_call_leave(world);
    return completed;
}

static bool plate_open_store(fixture *f, size_t slot, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 4;
    if (slot >= 6)
        options.limits.nbt.max_allocation_bytes = 256u * 1024u;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool plate_seed(fixture *f, size_t slot, int32_t cx, int32_t cz) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    uint8_t *encoded = NULL;
    size_t length = 0, written = 0;
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    CHECK(r != NULL);
    r->root.type = MC_NBT_COMPOUND;
    r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[0].value.int_value = cx;
    r->fields[1].value.int_value = cz;
    r->fields[2].value.long_value = -123;
    if (cx == 0 && cz == 0) {
        r->arrays[0][((size_t)8 << 11u) | ((size_t)8 << 7u) | 63u] = 1;
        r->arrays[0][((size_t)10 << 11u) | ((size_t)8 << 7u) | 63u] = 1;
    }
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3u].value.byte_array.data = r->arrays[i];
        r->fields[i + 3u].value.byte_array.length = lengths[i];
    }
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), cx, cz, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool plate_remember(fixture *f, size_t slot) {
    const mc_chunk_cache *cache = f->providers[slot].cache;
    if (!mc_chunk_cache_validate(cache) || cache->insertion_count > 4)
        return false;
    for (size_t i = 0; i < cache->insertion_count; ++i) {
        mc_chunk_state *state = cache->insertions[i];
        if (state->x == 0 && state->z == 0) {
            f->states[slot] = state;
            f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], state);
        } else {
            size_t j = 0;
            while (j < COUNT(plate_extra[slot]) && plate_extra[slot][j] != NULL &&
                   plate_extra[slot][j] != state)
                ++j;
            if (j == COUNT(plate_extra[slot]))
                return false;
            plate_extra[slot][j] = state;
        }
    }
    return true;
}

static bool plate_world(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    limits.max_scheduled_entries = slot >= 6 ? 1 : 8;
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_runtime_allocate_world(slot % 2u == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                         : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[slot % 2u], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction state;
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.mode == MC_WORLD_RUNTIME_GAME && state.phase == MC_WORLD_RUNTIME_READY &&
          state.contexts_bound && state.cache_registered);
    CHECK(mc_world_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z)
            CHECK(plate_seed(f, slot, x, z));
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state *loaded = NULL;
            const mc_chunk_acquire_status status =
                mc_world_runtime_get(f->worlds[slot], x, z, &loaded);
            CHECK(plate_remember(f, slot));
            CHECK(status == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded->active &&
                  loaded->terrain_populated && loaded->x == x && loaded->z == z);
        }
    CHECK(f->states[slot] != NULL && f->chunks[slot] != NULL &&
          f->providers[slot].cache->count == 4 && f->providers[slot].cache->insertion_count == 4 &&
          mc_world_store_chunk_count(f->stores[slot]) == 4);
    mc_world_runtime *world = f->worlds[slot];
    CHECK(!world->fields.B && !world->fields.o);
    world->fields.a = false; /* Actual deferred mode; never cap immediate recursion. */
    f->world_lists[slot] = world->fields.b;
    bool changed = false;
    mc_world_mutation_error error = mc_world_mutation_empty_error();
    /* Ordinary real setters execute the new added hook and neighbor callbacks.
     * Stone below each plate is real serialized Chunk input, not a provider. */
    CHECK(mc_world_runtime_block_metadata_set(world, 8, 64, 8, 72, 0, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && plate_at(world, 8, 0));
    CHECK(mc_world_runtime_block_metadata_set(world, 10, 64, 8, 70, 0, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && plate_at(world, 10, 0));
    CHECK(plate_counts(world, 0, 0));
    ok = true;
cleanup:
    return ok;
}

static bool plate_publish(fixture *f, size_t slot, bool falling, mc_entity **out) {
    bool ok = false, inserted = false;
    mc_entity *entity = NULL;
    if (falling) {
        CHECK(f->entity_count < COUNT(f->entities) &&
              mc_falling_entity_allocate(f->entity_classes[slot % 2u], &entity, NULL) ==
                  MC_ENTITY_OK);
        f->entities[f->entity_count++] = entity;
        CHECK(mc_falling_entity_construct(entity, f->worlds[slot], 8.5, 64.0, 8.5, 12, NULL) ==
              MC_ENTITY_OK);
    } else {
        CHECK(make_entity(f, slot % 2u, f->worlds[slot], 8.5, 64.0, 8.5, &entity));
    }
    CHECK(mc_world_runtime_insert_entity(f->worlds[slot], entity, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    *out = entity;
    ok = true;
cleanup:
    return ok;
}

static bool plate_lifecycle(fixture *f, size_t side) {
    bool ok = false, inserted = false;
    mc_world_runtime *world = f->worlds[side];
    mc_entity_classes *classes = f->entity_classes[side];
    const mc_block *wood = f->registry[side]->blocks.m[72];
    mc_entity *item = NULL, *falling = NULL;
    mc_world_entity_contact_error contact;
    mc_world_mutation_error tick;
    CHECK(plate_publish(f, side, false, &item) && plate_publish(f, side, true, &falling));
    CHECK(mc_world_runtime_insert_entity(world, item, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted); /* A second genuine occurrence must remain a duplicate. */
    const int32_t cursor = classes->bounds_cursor;
    const size_t holds = classes->bounds_holds;
    mc_world_schedule_class_state before, after;
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &contact) ==
              MC_WORLD_STORAGE_OK &&
          contact.status == MC_WORLD_STORAGE_OK && plate_at(world, 8, 1) &&
          plate_counts(world, 1, 1) && classes->bounds_cursor == cursor + 1 &&
          classes->bounds_holds == holds && world->fields.R->count == 3 &&
          world->fields.R->items[0] == item && world->fields.R->items[1] == falling &&
          world->fields.R->items[2] == item && world->local_leases == 0 &&
          world->active_calls == 0);
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.initialized && after.next == (before.initialized ? before.next : 0) + 1);
    mc_scheduled_tick *queued = NULL;
    CHECK(mc_scheduled_ordered_first(world->fields.E, &queued) == MC_SCHEDULED_ORDERED_OK);
    mc_scheduled_tick_fields fields;
    const bool queued_value =
        mc_scheduled_tick_fields_get(queued, &fields) == MC_SCHEDULED_TICK_OK && fields.x == 8 &&
        fields.y == 64 && fields.z == 8 && fields.block == 72 && fields.due_time == 20;
    mc_scheduled_tick_release(queued);
    CHECK(queued_value);
    const int64_t sequence = after.next;
    /* Contact metadata==1 guard must not query or allocate a new scheduled record. */
    CHECK(mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &contact) ==
              MC_WORLD_STORAGE_OK &&
          classes->bounds_cursor == cursor + 1 && plate_counts(world, 1, 1));
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.next == sequence);
    /* The actual tick's unused Random may be NULL. Found membership reschedules
     * through real duplicate F.contains, consuming sequence without a second key. */
    CHECK(plate_tick(world, wood, &tick) && plate_clear(&tick) &&
          classes->bounds_cursor == cursor + 2 && plate_counts(world, 1, 1));
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.next == sequence + 1);
    /* Move actual bounds, keeping real publication and duplicate memberships.
     * Release must follow the query result, not the contact argument or lists. */
    CHECK(mc_entity_set_position(item, 12.5, 64.0, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(falling, 12.5, 64.0, 8.5) == MC_ENTITY_OK);
    CHECK(plate_tick(world, wood, &tick) && plate_clear(&tick) && plate_at(world, 8, 0) &&
          world->fields.R->count == 0 && classes->bounds_cursor == cursor + 3 &&
          classes->bounds_holds == holds && plate_counts(world, 1, 1));
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.next == sequence + 1);
    CHECK(plate_tick(world, wood, &tick) && plate_clear(&tick) &&
          classes->bounds_cursor == cursor + 3); /* Off tick guard. */
    mc_world_provider *const provider = world->fields.v;
    world->fields.B = true;
    world->fields.v = NULL;
    const mc_world_storage_status remote_contact =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &contact);
    const bool remote_tick = plate_tick(world, wood, &tick);
    world->fields.v = provider;
    world->fields.B = false;
    CHECK(remote_contact == MC_WORLD_STORAGE_OK && remote_tick && plate_clear(&tick) &&
          classes->bounds_cursor == cursor + 3 && world->local_leases == 0 &&
          world->active_calls == 0);
    ok = true;
cleanup:
    return ok;
}

static bool plate_frontiers(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *world = f->worlds[slot];
    mc_registry_classes *registry = f->registry[side];
    mc_entity_classes *classes = f->entity_classes[side];
    mc_block *wood = registry->blocks.m[72];
    const mc_block *stone = registry->blocks.m[70];
    mc_world_entity_contact_error error;
    mc_world_mutation_error mutation;
    const size_t holds = classes->bounds_holds;
    int32_t cursor = classes->bounds_cursor;
    mc_world_schedule_class_state before, after;
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error) ==
              MC_WORLD_STORAGE_OK &&
          plate_at(world, 8, 0) && plate_counts(world, 0, 0) &&
          classes->bounds_cursor == cursor + 1 && classes->bounds_holds == holds &&
          world->fields.R->count == 0);
    cursor = classes->bounds_cursor;
    CHECK(mc_world_runtime_entity_contact(world, stone, 10, 64, 8, NULL, &error) ==
              MC_WORLD_STORAGE_OK &&
          classes->bounds_cursor == cursor + 1 && classes->bounds_holds == holds &&
          world->local_leases == 0 && plate_at(world, 10, 0) && plate_counts(world, 0, 0));
    const mc_entity_bounds *box = classes->bounds_pool->elements[(size_t)cursor];
    CHECK(box->min_x == 10.125 && box->min_y == 64.0 && box->min_z == 8.125 &&
          box->max_x == 10.875 && box->max_y == 64.25 && box->max_z == 8.875);

    /* Real empty Class queries preserve the pool prefix. y128 skips metadata
     * acquisitions for the first pair; extreme X/Z do so for the others.
     * No fabricated Class result or distant Chunk is made. Literal doubles
     * distinguish binary32 evaluation and the wrapped int x+1/z+1 prefix. */
    const struct {
        int32_t x, y, z;
        mc_entity_bounds expected;
    } coordinates[] = {{16777217,
                        128,
                        -16777217,
                        {16777216.0, 128.0, -16777216.0, 16777218.0, 128.25, -16777216.0}},
                       {INT32_MAX,
                        64,
                        INT32_MIN,
                        {2147483648.0, 64.0, -2147483648.0, -2147483648.0, 64.25, -2147483648.0}},
                       {INT32_MIN,
                        64,
                        INT32_MAX,
                        {-2147483648.0, 64.0, 2147483648.0, -2147483648.0, 64.25, -2147483648.0}}};

    for (size_t i = 0; i < COUNT(coordinates); ++i) {
        cursor = classes->bounds_cursor;
        CHECK(mc_world_runtime_entity_contact(world, stone, coordinates[i].x, coordinates[i].y,
                                              coordinates[i].z, NULL,
                                              &error) == MC_WORLD_STORAGE_OK &&
              classes->bounds_cursor == cursor + 1 && classes->bounds_holds == holds &&
              classes->bounds_pool != NULL && cursor >= 0 &&
              (size_t)cursor < classes->bounds_pool->length);
        box = classes->bounds_pool->elements[(size_t)cursor];
        const mc_entity_bounds *expected = &coordinates[i].expected;
        CHECK(box != NULL && box->min_x == expected->min_x && box->min_y == expected->min_y &&
              box->min_z == expected->min_z && box->max_x == expected->max_x &&
              box->max_y == expected->max_y && box->max_z == expected->max_z &&
              f->providers[slot].cache->count == 4 &&
              f->providers[slot].cache->insertion_count == 4 &&
              mc_world_store_chunk_count(f->stores[slot]) == 4 && world->fields.R->count == 0 &&
              world->local_leases == 0 && world->active_calls == 0);
    }
    /* Actual candidates reach passive Class filtering. They geometrically
     * overlap the plate but are neither Living nor Player instances. */
    mc_entity *item = NULL, *falling = NULL;
    CHECK(plate_publish(f, slot, false, &item) && plate_publish(f, slot, true, &falling));
    cursor = classes->bounds_cursor;
    const size_t query_bytes = world->construction.direct_allocation_bytes;
    CHECK(mc_world_runtime_entity_contact(world, stone, 8, 64, 8, NULL, &error) ==
              MC_WORLD_STORAGE_OK &&
          classes->bounds_cursor == cursor + 1 && classes->bounds_holds == holds &&
          plate_at(world, 8, 0) && world->construction.direct_allocation_bytes == query_bytes &&
          world->fields.R->count == 0 && world->local_leases == 0 && item->holds == 3 &&
          falling->holds == 3);
    const mc_pressure_category *const category = wood->subtype.bv.a;
    cursor = classes->bounds_cursor;
    wood->subtype.bv.a = registry->pressure_c;
    const mc_world_storage_status player =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    wood->subtype.bv.a = category;
    CHECK(player == MC_WORLD_STORAGE_OK && classes->bounds_cursor == cursor + 1 &&
          classes->bounds_holds == holds && plate_at(world, 8, 0) &&
          world->construction.direct_allocation_bytes == query_bytes && world->local_leases == 0 &&
          world->fields.R->count == 0 && item->holds == 3 && falling->holds == 3);
    /* Literal resolution belongs to the Block's defining namespace and is
     * reached before either Class branch allocates its pooled query box. */
    cursor = classes->bounds_cursor;
    mc_entity_classes *const defining_entities = registry->entities;
    registry->entities = NULL;
    const mc_world_storage_status missing_mob =
        mc_world_runtime_entity_contact(world, stone, 10, 64, 8, NULL, &error);
    registry->entities = defining_entities;
    CHECK(missing_mob == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 27 &&
          error.helper_bytecode == 98 && error.mutation.pressure_bytecode == 98 &&
          classes->bounds_cursor == cursor && classes->bounds_holds == holds);
    wood->subtype.bv.a = registry->pressure_c;
    registry->entities = NULL;
    const mc_world_storage_status missing_player =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    registry->entities = defining_entities;
    wood->subtype.bv.a = category;
    CHECK(missing_player == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 27 &&
          error.helper_bytecode == 157 && error.mutation.pressure_bytecode == 157 &&
          classes->bounds_cursor == cursor && classes->bounds_holds == holds &&
          world->local_leases == 0);
    cursor = classes->bounds_cursor;
    wood->subtype.bv.a = NULL; /* Actual unmatched nullable source field. */
    const mc_world_storage_status unmatched =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    wood->subtype.bv.a = category;
    CHECK(unmatched == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.helper_bytecode == 207 &&
          error.mutation.pressure_bytecode == 207 && classes->bounds_cursor == cursor &&
          classes->bounds_holds == holds);
    /* Separate IFs replace a genuinely nonempty R selection with the later
     * empty Class result. The old R contents remain independently owned. */
    mc_pressure_category *const pressure_b = registry->pressure_b;
    registry->pressure_b = registry->pressure_a;
    const mc_world_storage_status alias =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    registry->pressure_b = pressure_b;
    CHECK(alias == MC_WORLD_STORAGE_OK && classes->bounds_cursor == cursor + 2 &&
          classes->bounds_holds == holds && world->fields.R->count == 2 &&
          world->fields.R->items[0] == item && world->fields.R->items[1] == falling &&
          world->local_leases == 0 && plate_at(world, 8, 0) && plate_counts(world, 0, 0) &&
          item->holds == 4 && falling->holds == 4);
    cursor = classes->bounds_cursor;
    mc_pressure_category *const pressure_c = registry->pressure_c;
    registry->pressure_b = registry->pressure_a;
    registry->pressure_c = registry->pressure_a;
    const mc_world_storage_status all_aliases =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    registry->pressure_b = pressure_b;
    registry->pressure_c = pressure_c;
    CHECK(all_aliases == MC_WORLD_STORAGE_OK && classes->bounds_cursor == cursor + 3 &&
          classes->bounds_holds == holds && world->local_leases == 0 &&
          world->fields.R->count == 2 && world->fields.R->items[0] == item &&
          world->fields.R->items[1] == falling && item->holds == 4 && falling->holds == 4 &&
          plate_at(world, 8, 0) && plate_counts(world, 0, 0));
    CHECK(mc_entity_set_position(item, 12.5, 64.0, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(falling, 12.5, 64.0, 8.5) == MC_ENTITY_OK);
    cursor = classes->bounds_cursor;
    const mc_registry_phase phase = registry->blocks_phase;
    CHECK(registry->pressure_state.phase == MC_REGISTRY_READY);
    registry->blocks_phase = MC_REGISTRY_INITIALIZING;
    const mc_world_storage_status no_block_preflight =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    registry->blocks_phase = phase;
    CHECK(no_block_preflight == MC_WORLD_STORAGE_OK && classes->bounds_cursor == cursor + 1 &&
          registry->pressure_state.phase == MC_REGISTRY_READY && plate_at(world, 8, 0) &&
          world->fields.R->count == 0 && item->holds == 3 && falling->holds == 3);
    cursor = classes->bounds_cursor;
    mc_world_provider *const provider = world->fields.v;
    world->fields.v = NULL;
    const mc_world_storage_status outer_metadata =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    world->fields.v = provider;
    CHECK(outer_metadata == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 13 &&
          error.helper_bytecode == 0 && error.metadata.nearby.query.bytecode == 48 &&
          error.metadata.nearby.query.provider_bytecode == 6 &&
          error.mutation.pressure_bytecode == 0 && classes->bounds_cursor == cursor);
    world->fields.v = NULL;
    const bool private_metadata = plate_update(world, wood, &mutation);
    world->fields.v = provider;
    CHECK(!private_metadata && mutation.pressure_bytecode == 5 &&
          mutation.pressure_contact_bytecode == 0 && mutation.pressure_tick_bytecode == 0 &&
          mutation.nearby.query.bytecode == 48 && mutation.nearby.query.provider_bytecode == 6 &&
          classes->bounds_cursor == cursor);
    world->fields.v = NULL;
    const bool removed_metadata = plate_removed(world, wood, &mutation);
    world->fields.v = provider;
    CHECK(!removed_metadata && mutation.pressure_removed_bytecode == 5 &&
          mutation.nearby.query.bytecode == 48 && mutation.nearby.query.provider_bytecode == 6 &&
          classes->bounds_cursor == cursor && world->local_leases == 0 && world->active_calls == 0);
    mc_world_reference_list *const actual_r = world->fields.R;
    world->fields.R = NULL;
    const mc_world_storage_status query =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    world->fields.R = actual_r;
    CHECK(query == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 27 &&
          error.helper_bytecode == 82 && error.mutation.pressure_bytecode == 82 &&
          error.mutation.nearby.query.bytecode == 4 && classes->bounds_cursor == cursor + 1 &&
          classes->bounds_holds == holds && world->local_leases == 0 && world->active_calls == 0);
    cursor = classes->bounds_cursor;
    world->fields.R = NULL;
    const mc_world_storage_status independent_class =
        mc_world_runtime_entity_contact(world, stone, 10, 64, 8, NULL, &error);
    world->fields.R = actual_r;
    CHECK(independent_class == MC_WORLD_STORAGE_OK && classes->bounds_cursor == cursor + 1 &&
          classes->bounds_holds == holds && world->local_leases == 0 && world->active_calls == 0 &&
          plate_at(world, 10, 0) && world->fields.R->count == 0);
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.initialized == before.initialized && after.next == before.next &&
          plate_counts(world, 0, 0));
    CHECK(plate_neighbor(world, wood, &mutation) && plate_clear(&mutation) &&
          plate_at(world, 8, 0)); /* Real solid support returns before metadata/drop. */
    /* Removed metadata>0 is not the contact's metadata==1 test. No invented
     * neighbor: use the actual nullable Block.m source field and restore it. */
    mc_block **const blocks = registry->blocks.m;
    registry->blocks.m = NULL;
    const bool off_removed = plate_removed(world, wood, &mutation);
    registry->blocks.m = blocks;
    CHECK(off_removed && plate_clear(&mutation));
    bool changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, 8, 64, 8, 2, &changed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    registry->blocks.m = NULL;
    const bool positive_removed = plate_removed(world, wood, &mutation);
    registry->blocks.m = blocks;
    CHECK(!positive_removed && mutation.pressure_removed_bytecode == 24 &&
          mutation.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          mutation.neighbor_call_bytecode == 8 && plate_at(world, 8, 2));
    /* Real support loss reaches metadata35, inherited drop38 and notifying
     * air47. B is not a pressure-neighbor guard: only the inherited drop skips
     * publication in a remote World. The plate must still be removed. */
    CHECK(mc_world_runtime_block_set_quiet(world, 8, 63, 8, 0, &changed, &mutation) ==
              MC_WORLD_STORAGE_OK &&
          changed && plate_at(world, 8, 2));
    world->fields.B = true;
    const bool unsupported_neighbor = plate_neighbor(world, wood, &mutation);
    world->fields.B = false;
    int32_t removed_id = -1, removed_metadata_value = -1;
    CHECK(unsupported_neighbor && plate_clear(&mutation) &&
          mc_world_runtime_block_get(world, 8, 64, 8, &removed_id, NULL) == MC_WORLD_STORAGE_OK &&
          removed_id == 0 &&
          mc_world_runtime_metadata_get(world, 8, 64, 8, &removed_metadata_value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed_metadata_value == 0 && world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool plate_observer_and_time(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 4u;
    mc_world_runtime *world = f->worlds[slot];
    const mc_block *wood = f->registry[side]->blocks.m[72];
    mc_entity *actor = NULL;
    mc_world_entity_contact_error error;
    mc_world_mutation_error tick;
    CHECK(plate_publish(f, slot, false, &actor));
    mc_registry_block_t_view flags;
    bool observes = false;
    CHECK(mc_registry_block_t_get(f->registry[side], &flags, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_t_at(&flags, 72, &observes, NULL) == MC_REGISTRY_OK);
    mc_world_schedule_class_state before, after;
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &before) == MC_WORLD_STORAGE_OK);
    mc_world_reference_list *const observers = world->fields.u;
    world->fields.u = NULL;
    const mc_world_storage_status rising =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    world->fields.u = observers;
    CHECK(rising == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 27 &&
          error.helper_bytecode == (observes ? 234u : 272u) &&
          error.mutation.pressure_contact_bytecode == 27 &&
          error.mutation.pressure_bytecode == (observes ? 234u : 272u) &&
          error.mutation.observer_bytecode == 9 && plate_at(world, 8, 1) &&
          plate_counts(world, 0, 0) && world->local_leases == 0 && world->active_calls == 0);
    CHECK(mc_entity_set_position(actor, 12.5, 64.0, 8.5) == MC_ENTITY_OK);
    world->fields.u = NULL;
    const bool falling = plate_tick(world, wood, &tick);
    world->fields.u = observers;
    CHECK(!falling && tick.pressure_tick_bytecode == 26 &&
          tick.pressure_bytecode == (observes ? 320u : 358u) && tick.observer_bytecode == 9 &&
          plate_at(world, 8, 0) && plate_counts(world, 0, 0));
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.initialized == before.initialized && after.next == before.next);
    CHECK(mc_entity_set_position(actor, 8.5, 64.0, 8.5) == MC_ENTITY_OK);
    mc_world_info *const info = world->fields.x;
    world->fields.x = NULL;
    const mc_world_storage_status time =
        mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error);
    world->fields.x = info;
    CHECK(time == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 27 &&
          error.helper_bytecode == 408 && error.mutation.pressure_bytecode == 408 &&
          error.mutation.schedule.bytecode == 191 && error.mutation.schedule_bytecode == 191 &&
          error.mutation.schedule.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          plate_at(world, 8, 1) && plate_counts(world, 0, 0) && world->local_leases == 0 &&
          world->active_calls == 0);
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next + 1);
    ok = true;
cleanup:
    return ok;
}

static bool plate_schedule_prefix(fixture *f, size_t side) {
    bool ok = false, added = false, removed = false;
    const size_t slot = side + 6u;
    mc_world_runtime *world = f->worlds[slot];
    const mc_block *wood = f->registry[side]->blocks.m[72];
    mc_scheduled_tick *extra = NULL, *query = NULL, *first = NULL;
    mc_scheduled_tick_sequence independent = {9000};
    mc_entity *actor = NULL;
    mc_world_entity_contact_error error;
    CHECK(plate_publish(f, slot, false, &actor));
    /* Genuine lower-owner APIs fill one-entry queues. No fake List, queue
     * count, backing pointer or World ownership is injected. */
    CHECK(mc_scheduled_tick_create(&independent, 17, 2, 19, 0, &extra) == MC_SCHEDULED_TICK_OK &&
          mc_scheduled_membership_add(world->fields.F, extra, &added) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          added &&
          mc_scheduled_ordered_add(world->fields.E, extra, &added) == MC_SCHEDULED_ORDERED_OK &&
          added && plate_counts(world, 1, 1));
    mc_world_schedule_class_state before, after;
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          error.bytecode == 27 && error.helper_bytecode == 408 &&
          error.mutation.pressure_contact_bytecode == 27 &&
          error.mutation.pressure_bytecode == 408 && error.mutation.schedule.bytecode == 219 &&
          error.mutation.schedule.membership == MC_SCHEDULED_MEMBERSHIP_LIMIT &&
          error.mutation.schedule.ordered == MC_SCHEDULED_ORDERED_OK && plate_at(world, 8, 1) &&
          plate_counts(world, 1, 1));
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next + 1);
    bool changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, 8, 64, 8, 0, &changed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          changed &&
          mc_scheduled_membership_remove(world->fields.F, extra, &removed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          removed && plate_counts(world, 1, 0));
    CHECK(mc_world_runtime_entity_contact(world, wood, 8, 64, 8, NULL, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          error.helper_bytecode == 408 && error.mutation.schedule.bytecode == 231 &&
          error.mutation.schedule.membership == MC_SCHEDULED_MEMBERSHIP_OK &&
          error.mutation.schedule.ordered == MC_SCHEDULED_ORDERED_LIMIT && plate_at(world, 8, 1) &&
          plate_counts(world, 1, 1));
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next + 2);
    CHECK(mc_scheduled_ordered_first(world->fields.E, &first) == MC_SCHEDULED_ORDERED_OK &&
          first == extra);
    CHECK(mc_scheduled_tick_create(&independent, 8, 64, 8, 72, &query) == MC_SCHEDULED_TICK_OK);
    bool contained = false;
    CHECK(mc_scheduled_membership_contains(world->fields.F, query, &contained) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          contained && world->active_calls == 0 && world->local_leases == 0 &&
          f->entity_classes[side]->bounds_holds == 0);
    ok = true;
cleanup:
    mc_scheduled_tick_release(first);
    mc_scheduled_tick_release(query);
    mc_scheduled_tick_release(extra);
    return ok;
}

static bool plate_cleanup(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        if (f->worlds[slot] == NULL)
            continue;
        if (!f->terminal[slot]) {
            if (mc_world_runtime_retire_entity_containers(f->worlds[slot], NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->terminal[slot] = true;
        }
        if (!retire_descriptors(f, slot))
            return false;
        if (!f->world_retired[slot]) {
            if (mc_world_runtime_retire_world(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK)
                return false;
            f->world_retired[slot] = true;
        }
        for (size_t i = 0; i < COUNT(plate_extra[slot]); ++i)
            if (plate_extra[slot][i] != NULL) {
                if (mc_world_store_retire_chunk(f->stores[slot], plate_extra[slot][i], NULL) !=
                    MC_WORLD_STORAGE_OK)
                    return false;
                plate_extra[slot][i] = NULL;
            }
    }
    return cleanup_fixture(f);
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'P', 'l', 'a', 't', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-pressure-plate") != 0 ||
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
        CHECK(plate_open_store(f, slot, root) && plate_world(f, slot));
    for (size_t side = 0; side < 2; ++side) {
        CHECK(plate_lifecycle(f, side));
        CHECK(plate_frontiers(f, side));
        CHECK(plate_observer_and_time(f, side));
        CHECK(plate_schedule_prefix(f, side));
    }
    result = 0;
cleanup:
    if (!plate_cleanup(f)) {
        fputs("pressure-plate cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u pressure-plate checks (world-pressure-plate)\n", checks) < 0)
        result = 1;
    return result;
}
