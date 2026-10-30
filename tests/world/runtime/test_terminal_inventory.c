/* Native-only admission controls; the imported constructor test is not run. */
int mc_terminal_inventory_publication_fixture_main(int argc, char **argv);
#define main mc_terminal_inventory_publication_fixture_main
#include "test_entity_publication.c"
#undef main

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/chunk/object_state_internal.h"
#include "../../../src/world/runtime/collision/boxes_internal.h"
#include "../../../src/world/runtime/tile_entities/piston_list_internal.h"
#include "../../../src/world/runtime/tile_entities/tile_entities_internal.h"
#include "mc/world/runtime/collision/class_entities.h"

typedef struct terminal_piece {
    const void *identity;
    size_t bytes;
    unsigned char *before;
} terminal_piece;

typedef struct terminal_snapshot {
    terminal_piece pieces[160];
    size_t count;
    mc_world_provider *provider;
    mc_world_provider_fields provider_fields;
    mc_world_provider_state provider_state;
    mc_tile_entity_piston_class_snapshot tile_class;
    mc_world_provider *second_provider;
    mc_world_provider_fields second_provider_fields;
    mc_world_provider_state second_provider_state;
    size_t pending_history, store_chunks, store_reservations;
} terminal_snapshot;

static bool terminal_piece_save(terminal_snapshot *s, const void *identity, size_t bytes) {
    if (identity == NULL || bytes == 0)
        return identity == NULL ? bytes == 0 : true;
    for (size_t i = 0; i < s->count; ++i)
        if (s->pieces[i].identity == identity)
            return s->pieces[i].bytes == bytes;
    if (s->count == COUNT(s->pieces) || bytes > 4u * 1024u * 1024u)
        return false;
    unsigned char *copy = malloc(bytes);
    if (copy == NULL)
        return false;
    memcpy(copy, identity, bytes);
    s->pieces[s->count++] = (terminal_piece){identity, bytes, copy};
    return true;
}

static void terminal_snapshot_drop(terminal_snapshot *s) {
    for (size_t i = 0; i < s->count; ++i)
        free(s->pieces[i].before);
    memset(s, 0, sizeof(*s));
}

static bool terminal_tile_class_same(const mc_tile_entity_piston_class_snapshot *a,
                                     const mc_tile_entity_piston_class_snapshot *b) {
    return a->base_phase == b->base_phase && a->piston_phase == b->piston_phase &&
           a->base_initializer_bytecode == b->base_initializer_bytecode &&
           a->piston_initializer_bytecode == b->piston_initializer_bytecode &&
           a->base_failure == b->base_failure && a->piston_failure == b->piston_failure &&
           a->base_name_count == b->base_name_count && a->base_class_count == b->base_class_count &&
           a->list_identity == b->list_identity && a->list_count == b->list_count &&
           a->list_capacity == b->list_capacity && a->allocation_bytes == b->allocation_bytes &&
           a->live_tile_entities == b->live_tile_entities &&
           a->max_allocation_bytes == b->max_allocation_bytes &&
           a->class_literal_borrows == b->class_literal_borrows &&
           a->list_mod_count_bits == b->list_mod_count_bits;
}

static bool terminal_provider_same(mc_world_provider *provider,
                                   const mc_world_provider_fields *before,
                                   const mc_world_provider_state *state_before) {
    mc_world_provider_fields fields;
    mc_world_provider_state state;
    return mc_world_provider_fields_get(provider, &fields) == MC_WORLD_PROVIDER_OK &&
           mc_world_provider_state_get(provider, &state) == MC_WORLD_PROVIDER_OK &&
           fields.a == before->a && fields.b == before->b && fields.c == before->c &&
           fields.d == before->d && fields.e == before->e && fields.f == before->f &&
           fields.g == before->g && fields.h == before->h && state.kind == state_before->kind &&
           state.phase == state_before->phase && state.holds == state_before->holds &&
           state.constructor_pc == state_before->constructor_pc &&
           state.bind_pc == state_before->bind_pc && state.setup_pc == state_before->setup_pc &&
           state.direct_allocation_bytes == state_before->direct_allocation_bytes;
}

static bool terminal_snapshot_take(fixture *f, mc_entity *entity, terminal_snapshot *s) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[0];
    memset(s, 0, sizeof(*s));
    CHECK(terminal_piece_save(s, w, sizeof(*w)) &&
          terminal_piece_save(s, f->classes[0], sizeof(*f->classes[0])) &&
          terminal_piece_save(s, f->entity_classes[0], sizeof(*f->entity_classes[0])) &&
          terminal_piece_save(s, f->registry[0], sizeof(*f->registry[0])) &&
          terminal_piece_save(s, f->profile, sizeof(*f->profile)));
    const mc_world_entity_list_ledger *ledgers[] = {&w->entity_membership, &w->entity_deferral,
                                                    &w->player_membership, &w->weather_membership,
                                                    &w->nearby_entities};
    for (size_t i = 0; i < COUNT(ledgers); ++i) {
        const mc_world_entity_list_ledger *l = ledgers[i];
        CHECK(l->list != NULL && l->list->capacity <= 1024 && l->capacity <= 1024 &&
              terminal_piece_save(s, l->list, sizeof(*l->list)) &&
              terminal_piece_save(s, l->list->items, l->list->capacity * sizeof(*l->list->items)) &&
              terminal_piece_save(s, l->entries, l->capacity * sizeof(*l->entries)));
    }
    for (size_t selected = 0; selected < 2; ++selected) {
        const mc_tile_entity_list_ledger *l = &w->tile_entity_membership[selected];
        CHECK(l->identity != NULL && l->capacity <= 1024 && l->identity->capacity <= 1024 &&
              terminal_piece_save(s, l->identity, sizeof(*l->identity)) &&
              terminal_piece_save(s, l->identity->items,
                                  l->identity->capacity * sizeof(*l->identity->items)) &&
              terminal_piece_save(s, l->holds, l->capacity * sizeof(*l->holds)));
        for (size_t i = 0; i < l->count; ++i)
            CHECK(terminal_piece_save(s, l->holds[i], sizeof(*l->holds[i])));
    }
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->classes[0], &s->tile_class) ==
          MC_TILE_ENTITY_OK);
    if (s->tile_class.list_identity != NULL) {
        CHECK(s->tile_class.list_capacity <= 1024 &&
              terminal_piece_save(s, s->tile_class.list_identity,
                                  sizeof(*s->tile_class.list_identity)) &&
              terminal_piece_save(s, s->tile_class.list_identity->items,
                                  s->tile_class.list_capacity * sizeof(void *)));
    }
    const mc_world_collision_list_ledger *boxes = &w->collision_boxes;
    CHECK(boxes->list != NULL && boxes->capacity <= 1024 && boxes->list->capacity <= 1024 &&
          terminal_piece_save(s, boxes->list, sizeof(*boxes->list)) &&
          terminal_piece_save(s, boxes->list->items,
                              boxes->list->capacity * sizeof(*boxes->list->items)) &&
          terminal_piece_save(s, boxes->entries, boxes->capacity * sizeof(*boxes->entries)));
    mc_chunk_cache *cache = f->providers[0].cache;
    mc_chunk_pending *pending = f->providers[0].pending;
    CHECK(cache != NULL && pending != NULL && cache->capacity <= 1024 &&
          cache->insertion_capacity <= 1024 && pending->capacity <= 1024 &&
          terminal_piece_save(s, cache, sizeof(*cache)) &&
          terminal_piece_save(s, cache->entries, cache->capacity * sizeof(*cache->entries)) &&
          terminal_piece_save(s, cache->insertions,
                              cache->insertion_capacity * sizeof(*cache->insertions)) &&
          terminal_piece_save(s, pending, sizeof(*pending)) &&
          terminal_piece_save(s, pending->keys, pending->capacity * sizeof(*pending->keys)));
    s->pending_history = mc_java_int_set_size(pending->history);
    s->store_chunks = mc_world_store_chunk_count(f->stores[0]);
    s->store_reservations = mc_world_store_reservation_count(f->stores[0]);
    if (f->chunks[0] != NULL) {
        mc_world_chunk *c = f->chunks[0];
        mc_chunk_data *data = f->states[0]->data;
        CHECK(terminal_piece_save(s, c, sizeof(*c)) &&
              terminal_piece_save(s, f->states[0], sizeof(*f->states[0])) &&
              terminal_piece_save(s, data, sizeof(*data)) &&
              terminal_piece_save(s, data->blocks, MC_CHUNK_BLOCKS) &&
              terminal_piece_save(s, data->metadata, MC_CHUNK_NIBBLE_BYTES) &&
              terminal_piece_save(s, data->skylight, MC_CHUNK_NIBBLE_BYTES) &&
              terminal_piece_save(s, data->block_light, MC_CHUNK_NIBBLE_BYTES) &&
              terminal_piece_save(s, data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES));
        for (size_t i = 0; i < CHUNK_ENTITY_SLICES; ++i)
            CHECK(c->entities[i].capacity <= 1024 &&
                  terminal_piece_save(s, c->entities[i].items,
                                      c->entities[i].capacity * sizeof(*c->entities[i].items)));
    }
    if (entity != NULL) {
        CHECK(terminal_piece_save(s, entity, sizeof(*entity)) &&
              terminal_piece_save(s, entity->bounds, sizeof(*entity->bounds)) &&
              terminal_piece_save(s, entity->random, sizeof(*entity->random)));
    }
    if (f->worlds[2] != NULL) {
        mc_world_runtime *second = f->worlds[2];
        CHECK(terminal_piece_save(s, second, sizeof(*second)) &&
              terminal_piece_save(s, f->providers[2].cache, sizeof(*f->providers[2].cache)) &&
              terminal_piece_save(s, f->providers[2].pending, sizeof(*f->providers[2].pending)));
        const mc_world_entity_list_ledger *other[] = {
            &second->entity_membership, &second->entity_deferral, &second->player_membership,
            &second->weather_membership, &second->nearby_entities};
        for (size_t i = 0; i < COUNT(other); ++i) {
            const mc_world_entity_list_ledger *l = other[i];
            CHECK(l->list != NULL && l->capacity <= 1024 && l->list->capacity <= 1024 &&
                  terminal_piece_save(s, l->list, sizeof(*l->list)) &&
                  terminal_piece_save(s, l->list->items, l->list->capacity * sizeof(void *)) &&
                  terminal_piece_save(s, l->entries, l->capacity * sizeof(*l->entries)));
        }
        CHECK(f->chunks[2] != NULL && terminal_piece_save(s, f->chunks[2], sizeof(*f->chunks[2])) &&
              terminal_piece_save(s, f->states[2], sizeof(*f->states[2])));
        for (size_t i = 0; i < CHUNK_ENTITY_SLICES; ++i)
            CHECK(terminal_piece_save(s, f->chunks[2]->entities[i].items,
                                      f->chunks[2]->entities[i].capacity * sizeof(mc_entity *)));
        s->second_provider = second->fields.t;
        CHECK(mc_world_provider_fields_get(s->second_provider, &s->second_provider_fields) ==
                  MC_WORLD_PROVIDER_OK &&
              mc_world_provider_state_get(s->second_provider, &s->second_provider_state) ==
                  MC_WORLD_PROVIDER_OK);
    }
    s->provider = w->fields.t;
    CHECK(mc_world_provider_fields_get(s->provider, &s->provider_fields) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_state_get(s->provider, &s->provider_state) == MC_WORLD_PROVIDER_OK &&
          terminal_piece_save(s, s->provider_fields.f, 16 * sizeof(*s->provider_fields.f)) &&
          terminal_piece_save(s, s->provider_fields.h, 4 * sizeof(*s->provider_fields.h)));
    ok = true;
cleanup:
    if (!ok)
        terminal_snapshot_drop(s);
    return ok;
}

static bool terminal_snapshot_same(fixture *f, const terminal_snapshot *s) {
    for (size_t i = 0; i < s->count; ++i)
        if (memcmp(s->pieces[i].identity, s->pieces[i].before, s->pieces[i].bytes) != 0)
            return false;
    mc_tile_entity_piston_class_snapshot tile_class;
    if (mc_tile_entity_piston_class_get_snapshot(f->classes[0], &tile_class) != MC_TILE_ENTITY_OK ||
        !terminal_tile_class_same(&s->tile_class, &tile_class) ||
        (s->second_provider != NULL &&
         !terminal_provider_same(s->second_provider, &s->second_provider_fields,
                                 &s->second_provider_state)))
        return false;
    mc_world_provider_fields fields;
    mc_world_provider_state state;
    if (mc_world_provider_fields_get(s->provider, &fields) != MC_WORLD_PROVIDER_OK ||
        mc_world_provider_state_get(s->provider, &state) != MC_WORLD_PROVIDER_OK)
        return false;
    const mc_world_provider_fields *b = &s->provider_fields;
    const mc_world_provider_state *p = &s->provider_state;
    return fields.a == b->a && fields.b == b->b && fields.c == b->c && fields.d == b->d &&
           fields.e == b->e && fields.f == b->f && fields.g == b->g && fields.h == b->h &&
           state.kind == p->kind && state.phase == p->phase && state.holds == p->holds &&
           state.constructor_pc == p->constructor_pc && state.bind_pc == p->bind_pc &&
           state.setup_pc == p->setup_pc &&
           state.direct_allocation_bytes == p->direct_allocation_bytes &&
           mc_world_store_chunk_count(f->stores[0]) == s->store_chunks &&
           mc_world_store_reservation_count(f->stores[0]) == s->store_reservations &&
           mc_java_int_set_size(f->providers[0].pending->history) == s->pending_history;
}

static bool terminal_attempt_checked(fixture *f, mc_entity *observed,
                                     const mc_world_terminal_limits *limits,
                                     mc_world_storage_status expected, bool null_world,
                                     bool null_output, bool null_error,
                                     mc_world_terminal_inventory *success,
                                     const char *expected_operation) {
    bool ok = false;
    terminal_snapshot snapshot = {0};
    mc_world_terminal_inventory out = {.entities = 101,
                                       .tile_entities = 102,
                                       .entity_occurrences = 103,
                                       .tile_entity_occurrences = 104,
                                       .expected_world_bindings = 105,
                                       .piston_discard_required = true};
    unsigned char sentinel[sizeof(out)];
    memcpy(sentinel, &out, sizeof(out));
    mc_world_storage_error error = {0};
    CHECK(terminal_snapshot_take(f, observed, &snapshot));
    const mc_world_storage_status status =
        mc_world_runtime_terminal_admit(null_world ? NULL : f->worlds[0], limits,
                                        null_output ? NULL : &out, null_error ? NULL : &error);
    CHECK(status == expected && terminal_snapshot_same(f, &snapshot));
    if (expected != MC_WORLD_STORAGE_OK && !null_output)
        CHECK(memcmp(&out, sentinel, sizeof(out)) == 0);
    if (!null_error)
        CHECK(error.status == expected);
    if (expected_operation != NULL)
        CHECK(!null_error && error.operation != NULL &&
              strcmp(error.operation, expected_operation) == 0);
    if (expected == MC_WORLD_STORAGE_OK && success != NULL)
        *success = out;
    ok = true;
cleanup:
    terminal_snapshot_drop(&snapshot);
    return ok;
}

static bool terminal_attempt(fixture *f, mc_entity *observed,
                             const mc_world_terminal_limits *limits,
                             mc_world_storage_status expected, bool null_world, bool null_output,
                             bool null_error, mc_world_terminal_inventory *success) {
    return terminal_attempt_checked(f, observed, limits, expected, null_world, null_output,
                                    null_error, success, NULL);
}

static bool terminal_busy_reason(fixture *f, mc_entity *observed, const char *operation) {
    return terminal_attempt_checked(f, observed, NULL, MC_WORLD_STORAGE_BUSY, false, false, false,
                                    NULL, operation);
}

/* Keep the original real Store metadata, with one additional public admission
 * slot for a reservation or generic owner beside the selected GAME Chunk. */
static bool terminal_open_store(fixture *f, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    const int n = snprintf(path, sizeof(path), "%s/world0", root);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 2;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[0], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[0], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool terminal_open(fixture *f, const char *root) {
    bool ok = false;
    const uint16_t title[] = {'I', 'n', 'v', 'e', 'n', 't', 'o', 'r', 'y'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK &&
          mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = (stream){.bytes = language};
    r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          r->opens == 3 && r->closes == 1 && !r->bad && terminal_open_store(f, root));
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[0], .name = f->name, .seed = 23, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_construct_world(f->worlds[0], &args, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
              MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    f->world_lists[0] = f->worlds[0]->fields.b;
    CHECK(f->worlds[0]->construction.phase == MC_WORLD_RUNTIME_READY &&
          f->worlds[0]->construction.mode == MC_WORLD_RUNTIME_GAME &&
          mc_world_provider_world(f->dimensions[0]) == f->worlds[0] &&
          mc_world_provider_hold_count(f->dimensions[0]) == 2 &&
          mc_world_store_chunk_count(f->stores[0]) == 0);
    ok = true;
cleanup:
    return ok;
}

static bool terminal_load_chunk(fixture *f) {
    bool ok = false;
    CHECK(seed_record(f, 0));
    CHECK(mc_world_runtime_get(f->worlds[0], 0, 0, &f->states[0]) == MC_CHUNK_ACQUIRE_OK &&
          f->states[0] != NULL && f->states[0]->active && f->states[0]->terrain_populated);
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], f->states[0]);
    CHECK(f->chunks[0] != NULL && f->chunks[0]->d == f->worlds[0] &&
          mc_world_store_chunk_count(f->stores[0]) == 1);
    for (size_t i = 0; i < CHUNK_ENTITY_SLICES; ++i)
        CHECK(mc_world_chunk_slice_at(f->chunks[0]->m, i, &f->slices[0][i]) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

/* The normal attempt snapshots the selected graph and output sentinel. Also
 * preserve the actual extra caller-owned descriptor/backing or decoded owner. */
static bool terminal_store_extra_attempt(fixture *f, const mc_chunk_cache *cache,
                                         const mc_chunk_state *generic,
                                         mc_world_storage_status expected,
                                         mc_world_terminal_inventory *inventory) {
    bool ok = false;
    terminal_snapshot extra = {0};
    if (cache != NULL) {
        CHECK(terminal_piece_save(&extra, cache, sizeof(*cache)) &&
              terminal_piece_save(&extra, cache->entries,
                                  cache->capacity * sizeof(*cache->entries)) &&
              terminal_piece_save(&extra, cache->insertions,
                                  cache->insertion_capacity * sizeof(*cache->insertions)));
    }
    if (generic != NULL) {
        const mc_chunk_data *data = generic->data;
        CHECK(data != NULL && terminal_piece_save(&extra, generic, sizeof(*generic)) &&
              terminal_piece_save(&extra, data, sizeof(*data)) &&
              terminal_piece_save(&extra, data->blocks, MC_CHUNK_BLOCKS) &&
              terminal_piece_save(&extra, data->metadata, MC_CHUNK_NIBBLE_BYTES) &&
              terminal_piece_save(&extra, data->skylight, MC_CHUNK_NIBBLE_BYTES) &&
              terminal_piece_save(&extra, data->block_light, MC_CHUNK_NIBBLE_BYTES) &&
              terminal_piece_save(&extra, data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES));
    }
    CHECK(terminal_attempt(f, NULL, NULL, expected, false, false, false, inventory));
    for (size_t i = 0; i < extra.count; ++i)
        CHECK(memcmp(extra.pieces[i].identity, extra.pieces[i].before, extra.pieces[i].bytes) == 0);
    ok = true;
cleanup:
    terminal_snapshot_drop(&extra);
    return ok;
}

static bool terminal_store_topology_controls(fixture *f) {
    bool ok = false, registered = false, generic_leased = false;
    mc_chunk_cache cache = {0};
    mc_chunk_cache_entry entries[2] = {0};
    mc_chunk_state *insertions[2] = {0};
    mc_world_store_reservation *reservation = NULL;
    mc_chunk_state *generic = NULL;
    mc_world_terminal_inventory inventory;
    CHECK(f->states[0] != NULL && f->chunks[0] != NULL && f->chunks[0]->d == f->worlds[0] &&
          mc_world_store_chunk_count(f->stores[0]) == 1 &&
          mc_world_store_reservation_count(f->stores[0]) == 0);

    /* A real, valid EMPTY registration is still a second cache namespace. */
    CHECK(mc_chunk_cache_init(&cache, entries, COUNT(entries), insertions, COUNT(insertions)) &&
          mc_chunk_cache_validate(&cache) && cache.count == 0 && cache.insertion_count == 0);
    CHECK(mc_world_store_register_cache(f->stores[0], &cache, NULL, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    CHECK(terminal_store_extra_attempt(f, &cache, NULL, MC_WORLD_STORAGE_BUSY, NULL) &&
          mc_chunk_cache_validate(&cache) && cache.count == 0 && cache.insertion_count == 0);
    CHECK(mc_world_store_unregister_cache(f->stores[0], &cache, NULL) == MC_WORLD_STORAGE_OK);
    registered = false;
    CHECK(terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory) &&
          inventory.entities == 0 && inventory.tile_entities == 0 &&
          inventory.expected_world_bindings == 1);

    CHECK(mc_world_store_reserve_chunk(f->stores[0], 0, &reservation, NULL) ==
              MC_WORLD_STORAGE_OK &&
          reservation != NULL && mc_world_store_reservation_count(f->stores[0]) == 1 &&
          mc_world_store_chunk_count(f->stores[0]) == 1);
    CHECK(terminal_store_extra_attempt(f, NULL, NULL, MC_WORLD_STORAGE_BUSY, NULL) &&
          mc_world_store_reservation_count(f->stores[0]) == 1);
    CHECK(mc_world_store_cancel_reservation(f->stores[0], reservation, NULL) ==
          MC_WORLD_STORAGE_OK);
    reservation = NULL;
    CHECK(mc_world_store_reservation_count(f->stores[0]) == 0 &&
          terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory) &&
          inventory.entities == 0 && inventory.expected_world_bindings == 1);

    /* Decode the existing authored empty NBT through the generic storage API;
     * this is a second actual owner, without a runtime Chunk/World binding. */
    CHECK(mc_world_store_load_chunk(f->stores[0], 0, 0, 0, &generic, NULL) == MC_WORLD_STORAGE_OK &&
          generic != NULL && generic != f->states[0] && !generic->active &&
          mc_world_store_owns_chunk(f->stores[0], generic) &&
          mc_world_store_chunk_object(f->stores[0], generic) == NULL &&
          mc_world_store_chunk_count(f->stores[0]) == 2);
    CHECK(mc_world_store_lease_chunk(f->stores[0], generic, NULL) == MC_WORLD_STORAGE_OK);
    generic_leased = true;
    CHECK(terminal_store_extra_attempt(f, NULL, generic, MC_WORLD_STORAGE_BUSY, NULL));
    CHECK(mc_world_store_release_chunk(f->stores[0], generic, NULL) == MC_WORLD_STORAGE_OK);
    generic_leased = false;
    CHECK(terminal_store_extra_attempt(f, NULL, generic, MC_WORLD_STORAGE_OK, &inventory) &&
          mc_world_store_owns_chunk(f->stores[0], generic) &&
          mc_world_store_chunk_count(f->stores[0]) == 2 && inventory.entities == 0 &&
          inventory.expected_world_bindings == 1);
    CHECK(mc_world_store_retire_chunk(f->stores[0], generic, NULL) == MC_WORLD_STORAGE_OK);
    generic = NULL;
    CHECK(mc_world_store_chunk_count(f->stores[0]) == 1 &&
          terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory) &&
          f->chunks[0]->d == f->worlds[0] && f->states[0]->active);
    ok = true;
cleanup:
    if (generic_leased &&
        mc_world_store_release_chunk(f->stores[0], generic, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (generic != NULL &&
        mc_world_store_retire_chunk(f->stores[0], generic, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (reservation != NULL &&
        mc_world_store_cancel_reservation(f->stores[0], reservation, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (registered &&
        mc_world_store_unregister_cache(f->stores[0], &cache, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    return ok;
}

static bool terminal_bounds_append(mc_world_runtime *w, mc_entity *entity) {
    const mc_world_storage_status entered = mc_world_runtime_call_enter(w, NULL);
    if (entered != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_collision_error error = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_collision_append_entity_held(w, w->fields.K, entity, entity->bounds, &error);
    mc_world_runtime_call_leave(w);
    return status == MC_WORLD_STORAGE_OK;
}

static bool terminal_secondary_open(fixture *f, const char *root) {
    bool ok = false;
    CHECK(open_store(f, 2, root));
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[2], NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes[0],
                                          &limits, &f->worlds[2], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[2], .name = f->name, .seed = 23, .dimension = f->dimensions[2]};
    CHECK(mc_world_runtime_construct_world(f->worlds[2], &arguments, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(f->worlds[2]->fields.v, &f->providers[2], NULL) ==
              MC_WORLD_STORAGE_OK);
    f->bound[2] = true;
    CHECK(f->worlds[2]->classes == f->classes[0] &&
          f->worlds[2]->construction.phase == MC_WORLD_RUNTIME_READY &&
          f->dimensions[2] != f->worlds[0]->fields.t &&
          mc_world_provider_world(f->dimensions[2]) == f->worlds[2] &&
          f->classes[0]->live_worlds == 2 && seed_record(f, 2));
    CHECK(mc_world_runtime_get(f->worlds[2], 0, 0, &f->states[2]) == MC_CHUNK_ACQUIRE_OK &&
          f->states[2] != NULL && f->states[2]->active && f->states[2]->terrain_populated);
    f->chunks[2] = mc_world_store_chunk_object(f->stores[2], f->states[2]);
    CHECK(f->chunks[2] != NULL && f->chunks[2]->d == f->worlds[2]);
    ok = true;
cleanup:
    return ok;
}

static bool terminal_secondary_close(fixture *f) {
    if (f == NULL)
        return true;
    if (f->worlds[2] != NULL) {
        if (mc_world_runtime_retire_entity_containers(f->worlds[2], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->terminal[2] = true;
        if (!retire_descriptors(f, 2))
            return false;
        mc_world_provider_unbind(f->dimensions[2]);
        if (mc_world_runtime_retire_world(f->worlds[2], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        if (f->states[2] != NULL &&
            mc_world_store_retire_chunk(f->stores[2], f->states[2], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->states[2] = NULL;
        f->chunks[2] = NULL;
        if (mc_world_runtime_free(f->worlds[2], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->worlds[2] = NULL;
    }
    mc_world_provider_release(f->dimensions[2]);
    f->dimensions[2] = NULL;
    return f->stores[2] == NULL ||
           mc_world_store_free_checked(&f->stores[2], NULL) == MC_WORLD_STORAGE_OK;
}

static bool terminal_tile_and_piston_controls(fixture *f, const char *root, mc_entity *entity) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[0];
    const bool original_l = w->fields.L;
    bool l_input_entered = false;
    mc_tile_entity *piston = NULL, *external_tile = NULL, *unlisted = NULL;
    mc_world_entity_list_view single = {0};
    mc_world_terminal_inventory inventory;
    mc_live_tile_entity_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL && !original_l && entity->holds == 8 && w->fields.c->count == 0 &&
          w->fields.G->count == 0 && f->chunks[0]->l->count == 0);
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry[0], &blocks, NULL) == MC_REGISTRY_OK);
    const mc_block *moving = NULL;
    for (size_t i = 1; i < 256; ++i)
        if (blocks.m[i] != NULL && blocks.m[i]->kind == MC_BLOCK_MOVING_PISTON) {
            CHECK(moving == NULL);
            moving = blocks.m[i];
        }
    CHECK(moving != NULL && mc_block_fields_get(moving)->bn > 0 &&
          mc_block_fields_get(moving)->bn < 256);
    /* Declared real voxel input, not a placement/push/tick result. The actual
     * registered MovingPiston block admits the separately constructed Tile. */
    f->states[0]->data->blocks[(8u * 16u + 8u) * 128u + 8u] =
        (uint8_t)mc_block_fields_get(moving)->bn;
    CHECK(mc_tile_entity_allocate(w, MC_TILE_ENTITY_PISTON, &piston) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(piston, 0, 3, 5, false, false, NULL) ==
              MC_TILE_ENTITY_OK &&
          mc_live_tile_entities_set(w, 8, 8, 8, piston, error) == MC_WORLD_STORAGE_OK &&
          w->fields.c->count == 1 && f->chunks[0]->tile_entity_map.count == 1);
    /* Authored SOURCE World.L branch input, following existing Tile controls.
     * It is restored before every audit/snapshot and every failure cleanup.
     * No full Tile iteration or source tick is claimed. */
    w->fields.L = true;
    l_input_entered = true;
    const mc_world_storage_status registered =
        mc_live_tile_entities_chunk_register(f->chunks[0], error);
    w->fields.L = original_l;
    l_input_entered = false;
    CHECK(registered == MC_WORLD_STORAGE_OK && w->fields.G->count == 1 &&
          mc_live_tile_entities_lists_valid(w) && mc_live_tile_entities_map_valid(f->chunks[0]) &&
          w->fields.c->items[0] == piston && w->fields.G->items[0] == piston && piston->holds == 4);
    mc_tile_entity *listed = piston; /* Borrow from the three real containers. */
    mc_tile_entity_release(piston);  /* End the actual initial caller occurrence. */
    piston = NULL;
    CHECK(listed->holds == 3 && listed->world == w &&
          terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory) &&
          inventory.entities == 1 && inventory.entity_occurrences == 8 &&
          inventory.tile_entities == 1 && inventory.tile_entity_occurrences == 3 &&
          inventory.expected_world_bindings == 3 && !inventory.piston_discard_required);
    CHECK(mc_tile_entity_retain(listed) == MC_TILE_ENTITY_OK);
    external_tile = listed;
    CHECK(terminal_busy_reason(f, entity, "World terminal external Entity or TileEntity hold"));
    mc_tile_entity_release(external_tile);
    external_tile = NULL;
    CHECK(terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory));
    CHECK(mc_tile_entity_allocate(w, MC_TILE_ENTITY_CHEST, &unlisted) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(unlisted) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_bind_world(unlisted, w) == MC_TILE_ENTITY_OK && unlisted->world == w &&
          w->persistent_bindings == 4 &&
          terminal_busy_reason(f, entity, "World terminal unexplained direct World binding"));
    mc_tile_entity_release(unlisted);
    unlisted = NULL;
    CHECK(w->persistent_bindings == 3 &&
          terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory));

    /* A genuine second World supplies a one-occurrence nearby view. The same
     * Entity may be inserted into a different World without changing its real
     * construction World; this is admitted by the existing insertion API.
     * No supplied List/view, source count or Entity hold is fabricated. */
    CHECK(terminal_secondary_open(f, root));
    bool inserted = false;
    const mc_entity_bounds bounds = {7.0, 7.0, 7.0, 9.0, 9.0, 9.0};
    size_t count = 0;
    CHECK(mc_world_runtime_insert_entity(f->worlds[2], entity, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted &&
          mc_world_runtime_nearby_entities(f->worlds[2], NULL, &bounds, &single, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_entity_list_view_count(&single, &count) == MC_WORLD_STORAGE_OK && count == 1);
    CHECK(mc_tile_entity_retain(listed) == MC_TILE_ENTITY_OK);
    piston = listed;
    mc_piston_list_error list_error = {0};
    CHECK(mc_tile_entity_piston_entities_append(piston, &single, &list_error) ==
          MC_WORLD_STORAGE_OK);
    mc_world_entity_list_view_release(&single);
    mc_tile_entity_release(piston);
    piston = NULL;
    mc_tile_entity_piston_class_snapshot classes;
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->classes[0], &classes) == MC_TILE_ENTITY_OK &&
          classes.live_tile_entities == 1 && classes.list_count == 1 &&
          classes.list_identity->items[0] == entity && entity->base.world == w &&
          entity->holds == 12 && f->classes[0]->live_worlds == 2 &&
          terminal_busy_reason(f, entity,
                               "World terminal Piston proof requires the sole live World"));
    /* The append is a direct native List control, not an original Piston push.
     * End the second World's real three Entity occurrences and full graph. */
    CHECK(terminal_secondary_close(f) && f->classes[0]->live_worlds == 1 && entity->holds == 9 &&
          terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory) &&
          inventory.entities == 1 && inventory.entity_occurrences == 9 &&
          inventory.tile_entities == 1 && inventory.tile_entity_occurrences == 3 &&
          inventory.expected_world_bindings == 3 && inventory.piston_discard_required);
    CHECK(mc_tile_entity_allocate_in_classes(f->classes[0], MC_TILE_ENTITY_CHEST, &unlisted) ==
              MC_TILE_ENTITY_OK &&
          unlisted->phase == MC_TILE_ENTITY_RAW && unlisted->world == NULL &&
          unlisted->holds == 1 &&
          mc_tile_entity_piston_class_get_snapshot(f->classes[0], &classes) == MC_TILE_ENTITY_OK &&
          classes.live_tile_entities == 2 && w->persistent_bindings == 3 &&
          terminal_busy_reason(f, entity, "World terminal unaccounted class TileEntity owner"));
    mc_tile_entity_release(unlisted);
    unlisted = NULL;
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->classes[0], &classes) == MC_TILE_ENTITY_OK &&
          classes.live_tile_entities == 1 && classes.list_count == 1 && listed->holds == 3 &&
          entity->holds == 9 &&
          terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory) &&
          inventory.tile_entities == 1 && inventory.tile_entity_occurrences == 3 &&
          inventory.entity_occurrences == 9 && inventory.expected_world_bindings == 3 &&
          inventory.piston_discard_required && f->sources[0].opens == 3 &&
          f->sources[0].closes == 1 && !f->sources[0].bad);
    ok = true;
cleanup:
    if (l_input_entered)
        w->fields.L = original_l;
    mc_world_entity_list_view_release(&single);
    mc_tile_entity_release(external_tile);
    mc_tile_entity_release(unlisted);
    mc_tile_entity_release(piston);
    free(error);
    return ok;
}

static bool terminal_close(fixture *f) {
    if (f == NULL)
        return true;
    if (!terminal_secondary_close(f))
        return false;
    if (f->leased[0] &&
        mc_world_store_release_chunk(f->stores[0], f->states[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    f->leased[0] = false;
    if (f->worlds[0] != NULL) {
        if (mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->terminal[0] = true;
        mc_tile_entity_piston_class_snapshot classes;
        if (mc_tile_entity_piston_class_get_snapshot(f->classes[0], &classes) !=
                MC_TILE_ENTITY_OK ||
            classes.live_tile_entities != 0 ||
            mc_tile_entity_piston_class_discard_entities(f->classes[0], NULL) !=
                MC_WORLD_STORAGE_OK ||
            mc_tile_entity_piston_class_get_snapshot(f->classes[0], &classes) !=
                MC_TILE_ENTITY_OK ||
            classes.list_count != 0)
            return false;
        if (!retire_descriptors(f, 0) ||
            mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        if (f->states[0] != NULL &&
            mc_world_store_retire_chunk(f->stores[0], f->states[0], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    for (size_t i = 0; i < f->entity_count; ++i)
        mc_entity_release(f->entities[i]);
    if (f->worlds[0] != NULL && mc_world_runtime_free(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_provider_release(f->dimensions[0]);
    if ((f->classes[0] != NULL &&
         mc_world_runtime_classes_free(f->classes[0], NULL) != MC_WORLD_STORAGE_OK) ||
        (f->registry[0] != NULL &&
         mc_registry_classes_free(f->registry[0], NULL) != MC_REGISTRY_OK) ||
        (f->entity_classes[0] != NULL &&
         mc_entity_classes_free(f->entity_classes[0]) != MC_ENTITY_OK))
        return false;
    mc_item_stack_release(f->stack);
    mc_world_info_name_release(f->name);
    if (f->stores[0] != NULL &&
        mc_world_store_free_checked(&f->stores[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_world_provider *extra = NULL;
    mc_entity *external_entity = NULL;
    mc_world_class_entity_list *query = NULL;
    mc_world_entity_list_view nearby = {0};
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-terminal-inventory") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && terminal_open(f, root));
    mc_world_runtime *w = f->worlds[0];
    mc_world_terminal_inventory inventory;
    /* The caller's supplied provider occurrence is genuinely external. */
    CHECK(terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_BUSY, false, false, false, NULL));
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL; /* End only the caller's occurrence, not World.t. */
    CHECK(mc_world_provider_hold_count(w->fields.t) == 1 &&
          terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_OK, false, false, true, &inventory) &&
          inventory.entities == 0 && inventory.tile_entities == 0 &&
          inventory.entity_occurrences == 0 && inventory.tile_entity_occurrences == 0 &&
          inventory.expected_world_bindings == 1 && !inventory.piston_discard_required);
    CHECK(terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_INVALID_ARGUMENT, true, false, false,
                           NULL) &&
          terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_INVALID_ARGUMENT, false, true, false,
                           NULL));
    CHECK(terminal_load_chunk(f));
    CHECK(terminal_store_topology_controls(f));
    const mc_world_terminal_limits defaults = mc_world_runtime_terminal_default_limits();
    CHECK(defaults.max_entries > 0 && defaults.max_bytes > 0);
    mc_world_terminal_limits limits = defaults;
    limits.max_entries = 0;
    CHECK(terminal_attempt(f, NULL, &limits, MC_WORLD_STORAGE_LIMIT, false, false, false, NULL));
    limits = defaults;
    limits.max_bytes = 0;
    CHECK(terminal_attempt(f, NULL, &limits, MC_WORLD_STORAGE_LIMIT, false, false, true, NULL) &&
          terminal_attempt(f, NULL, &defaults, MC_WORLD_STORAGE_OK, false, false, false,
                           &inventory) &&
          inventory.entities == 0 && inventory.expected_world_bindings == 1);
    CHECK(mc_world_provider_retain(w->fields.t) == MC_WORLD_PROVIDER_OK);
    extra = w->fields.t;
    CHECK(terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_BUSY, false, false, false, NULL));
    mc_world_provider_release(extra);
    extra = NULL;
    CHECK(mc_world_provider_create(0, NULL, &extra, NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_bind(extra, w, NULL) == MC_WORLD_PROVIDER_OK &&
          terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_BUSY, false, false, false, NULL) &&
          mc_world_provider_world(extra) == w && mc_world_provider_hold_count(extra) == 1);
    mc_world_provider_unbind(extra);
    mc_world_provider_release(extra);
    extra = NULL;
    CHECK(terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory));
    mc_entity *entity = NULL;
    const size_t unlisted = f->entity_count;
    CHECK(make_entity(f, 0, w, 8.0, 8.0, 8.0, &entity) &&
          terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_BUSY, false, false, false, NULL));
    mc_entity_release(f->entities[unlisted]);
    f->entities[unlisted] = NULL;
    entity = NULL;
    CHECK(terminal_attempt(f, NULL, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory));
    const size_t original = f->entity_count;
    CHECK(make_entity(f, 0, w, 8.0, 8.0, 8.0, &entity));
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(w, entity, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted &&
          mc_world_runtime_insert_entity(w, entity, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted);
    const mc_entity_bounds bounds = {7.0, 7.0, 7.0, 9.0, 9.0, 9.0};
    CHECK(mc_world_runtime_nearby_entities(w, NULL, &bounds, &nearby, NULL) == MC_WORLD_STORAGE_OK);
    size_t count = SIZE_MAX;
    CHECK(mc_world_entity_list_view_count(&nearby, &count) == MC_WORLD_STORAGE_OK && count == 2);
    mc_world_entity_list_view_release(&nearby);
    /* Native permanent-box provenance, not a fabricated Item.f override. */
    CHECK(terminal_bounds_append(w, entity) && terminal_bounds_append(w, entity));
    mc_entity_release(f->entities[original]);
    f->entities[original] = NULL;
    CHECK(entity->holds == 8 && w->entity_membership.count == 2 && w->nearby_entities.count == 2 &&
          w->collision_boxes.count == 2 && f->chunks[0]->entities[0].count == 2 &&
          terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory) &&
          inventory.entities == 1 && inventory.entity_occurrences == 8 &&
          inventory.tile_entities == 0 && inventory.tile_entity_occurrences == 0 &&
          inventory.expected_world_bindings == 2 && !inventory.piston_discard_required);
    limits = defaults;
    limits.max_entries = 1; /* One actual Chunk plus one Entity already exceed it. */
    CHECK(terminal_attempt(f, entity, &limits, MC_WORLD_STORAGE_LIMIT, false, false, false, NULL));
    limits = defaults;
    limits.max_bytes = 1;
    CHECK(terminal_attempt(f, entity, &limits, MC_WORLD_STORAGE_LIMIT, false, false, false, NULL));
    CHECK(mc_entity_retain(entity) == MC_ENTITY_OK);
    external_entity = entity;
    CHECK(terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_BUSY, false, false, false, NULL));
    mc_entity_release(external_entity); /* End only this real external extra occurrence. */
    external_entity = NULL;
    CHECK(terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory));
    CHECK(mc_world_store_lease_chunk(f->stores[0], f->states[0], NULL) == MC_WORLD_STORAGE_OK);
    f->leased[0] = true;
    CHECK(terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_BUSY, false, false, false, NULL));
    CHECK(mc_world_store_release_chunk(f->stores[0], f->states[0], NULL) == MC_WORLD_STORAGE_OK);
    f->leased[0] = false;
    const mc_entity_class_literal *kind = NULL;
    CHECK(mc_entity_class_literal_get(f->entity_classes[0], MC_ENTITY_CLASS_KIND_ITEM, &kind) ==
              MC_ENTITY_OK &&
          mc_world_runtime_class_entities(w, kind, &bounds, &query, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_class_entity_list_count(query, &count) == MC_WORLD_STORAGE_OK && count == 2 &&
          terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_BUSY, false, false, false, NULL));
    mc_world_class_entity_list_release(query);
    query = NULL;
    CHECK(entity->holds == 8 && w->local_leases == 0 &&
          terminal_attempt(f, entity, NULL, MC_WORLD_STORAGE_OK, false, false, false, &inventory) &&
          inventory.entities == 1 && inventory.entity_occurrences == 8 &&
          w->construction.phase == MC_WORLD_RUNTIME_READY && !w->entity_containers_retired &&
          !w->tile_entity_containers_retired && mc_world_provider_world(w->fields.t) == w);
    CHECK(terminal_tile_and_piston_controls(f, root, entity));
    result = 0;
cleanup:
    mc_world_entity_list_view_release(&nearby);
    mc_world_class_entity_list_release(query);
    mc_entity_release(external_entity);
    if (extra != NULL) {
        /* A retained same-provider occurrence is not independently bound. */
        if (f == NULL || f->worlds[0] == NULL || extra != f->worlds[0]->fields.t)
            mc_world_provider_unbind(extra);
        mc_world_provider_release(extra);
    }
    if (!terminal_close(f))
        result = 1;
    if (result == 0)
        printf("PASS: %u World terminal inventory checks (world-terminal-inventory)\n", checks);
    return result;
}
