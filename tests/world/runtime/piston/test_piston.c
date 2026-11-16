/* Genuine saved stable Piston/Lever -> XR -> JQ -> UK -> actual World.g. */
#define main mc_piston_gameplay_unused_publication_main
#include "../test_entity_publication.c"
#undef main

#include "mc/entity/nbt/factory.h"
#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/placement.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/mutation/piston_internal.h"
#include "world/runtime/tile_entities/piston_list_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"
#include "world/runtime/world_access_internal.h"

typedef struct piston_observer {
    bool fail_sound;
    size_t sounds;
} piston_observer;

static mc_world_storage_status passive_cell(void *p, mc_world_runtime *w, int32_t x, int32_t y,
                                            int32_t z, mc_world_storage_error *e) {
    (void)p;
    (void)w;
    (void)x;
    (void)y;
    (void)z;
    (void)e;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status passive_range(void *p, mc_world_runtime *w, int32_t x, int32_t y,
                                             int32_t z, int32_t a, int32_t b, int32_t c,
                                             mc_world_storage_error *e) {
    (void)a;
    (void)b;
    (void)c;
    return passive_cell(p, w, x, y, z, e);
}

static mc_world_storage_status passive_entity(void *p, mc_world_runtime *w, mc_entity *v,
                                              mc_world_storage_error *e) {
    (void)v;
    return passive_cell(p, w, 0, 0, 0, e);
}

static mc_world_storage_status passive_dirty(void *p, mc_world_runtime *w, int32_t x, int32_t y,
                                             int32_t z, mc_tile_entity *v,
                                             mc_world_storage_error *e) {
    (void)v;
    return passive_cell(p, w, x, y, z, e);
}

static mc_world_storage_status passive_sky(void *p, mc_world_runtime *w,
                                           mc_world_storage_error *e) {
    return passive_cell(p, w, 0, 0, 0, e);
}

static mc_world_storage_status passive_aux(void *p, mc_world_runtime *w, mc_entity *v, int32_t a,
                                           int32_t x, int32_t y, int32_t z, int32_t b,
                                           mc_world_storage_error *e) {
    (void)v;
    (void)a;
    (void)b;
    return passive_cell(p, w, x, y, z, e);
}

static mc_world_storage_status passive_particle(void *p, mc_world_runtime *w,
                                                const mc_registry_string *name, double x, double y,
                                                double z, double a, double b, double c,
                                                mc_world_storage_error *e) {
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)a;
    (void)b;
    (void)c;
    return passive_cell(p, w, 0, 0, 0, e);
}

static mc_world_storage_status piston_sound_observer(void *p, mc_world_runtime *w,
                                                     const mc_registry_string *name, double x,
                                                     double y, double z, float volume, float pitch,
                                                     mc_world_storage_error *e) {
    (void)w;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    piston_observer *observer = p;
    mc_registry_string_view text;
    static const uint16_t expected[] = {'t', 'i', 'l', 'e', '.', 'p', 'i', 's',
                                        't', 'o', 'n', '.', 'o', 'u', 't'};
    if (mc_registry_string_read(name, &text) && text.length == COUNT(expected) &&
        memcmp(text.units, expected, sizeof(expected)) == 0) {
        ++observer->sounds;
        if (observer->fail_sound) {
            *e = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Piston sound observer failure", 73};
            return MC_WORLD_STORAGE_IO_ERROR;
        }
    }
    return MC_WORLD_STORAGE_OK;
}

static size_t cell_at(int32_t x, int32_t y, int32_t z) {
    return ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
}

static void input_cell(fixture_record *r, int32_t x, int32_t y, int32_t z, uint8_t id,
                       uint8_t metadata) {
    const size_t index = cell_at(x, y, z);
    r->arrays[0][index] = id;
    const unsigned int shift = (unsigned int)(index & 1u) * 4u;
    r->arrays[1][index / 2u] =
        (uint8_t)((r->arrays[1][index / 2u] & ~(15u << shift)) | ((unsigned int)metadata << shift));
    if (id != 0 && r->arrays[4][(size_t)z * 16u + (size_t)x] < y + 1)
        r->arrays[4][(size_t)z * 16u + (size_t)x] = (uint8_t)(y + 1);
}

/* Pre-acquisition wire only. No MovingPiston/Tile/progress is authored. */
static bool input_record(fixture *f, int32_t base_id, size_t stones, uint8_t carried) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    uint8_t *wire = NULL;
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
    CHECK(r != NULL && stones <= 13);
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    r->fields[8].value.byte_value = 1;
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    memset(r->arrays[2], 255, MC_CHUNK_NIBBLE_BYTES);
    memset(r->arrays[4], 64, MC_CHUNK_HEIGHTMAP_BYTES);
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            input_cell(r, x, 63, z, 1, 0);
    input_cell(r, 8, 64, 1, (uint8_t)base_id, 3);
    input_cell(r, 7, 64, 1, 69, 4);
    input_cell(r, 7, 64, 2, 1, 0);
    for (size_t i = 0; i < stones; ++i)
        input_cell(r, 8, 64, (int32_t)i + 2, carried, 0);
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK);
    wire = malloc(length);
    CHECK(wire != NULL && mc_nbt_write(&r->root, NULL, wire, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[0]), 0, 0, wire, length,
                                 fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(wire);
    free(r);
    return ok;
}

static bool piston_store(fixture *f, const char *root, bool neighbor_control) {
    if (!neighbor_control)
        return open_store(f, 0, root);
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
    /* Pre-bootstrap native capacity for the authored lazy-neighbor control.
     * The other eight cases retain their original one-Chunk admission. */
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

static bool piston_fixture(fixture *f, const char *root, int32_t base_id, size_t stones,
                           uint8_t carried, bool neighbor_control) {
    bool ok = false;
    static const uint16_t title[] = {'P', 'i', 's', 't', 'o', 'n'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
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
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    CHECK(piston_store(f, root, neighbor_control) && input_record(f, base_id, stones, carried));
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          NULL, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_construct_world(f->worlds[0], &arguments, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
              MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    mc_chunk_state *loaded = NULL;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get(f->worlds[0], 0, 0, &loaded);
    if (f->providers[0].cache->insertion_count != 0)
        f->states[0] = f->providers[0].cache->insertions[0];
    CHECK(acquired == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded == f->states[0]);
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], loaded);
    CHECK(f->chunks[0] != NULL && mc_live_tile_entities_map_valid(f->chunks[0]) &&
          f->worlds[0]->fields.c->count == 0 && f->chunks[0]->l->count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool cell_is(fixture *f, int32_t z, int32_t id, int32_t data) {
    int32_t actual, metadata;
    return mc_world_runtime_block_get(f->worlds[0], 8, 64, z, &actual, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_metadata_get(f->worlds[0], 8, 64, z, &metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           actual == id && metadata == data;
}

static bool moving_is(fixture *f, int32_t z, int32_t id, bool extending, bool head,
                      uint32_t current, uint32_t previous) {
    mc_tile_entity *tile = NULL;
    mc_live_tile_entity_error error;
    mc_tile_entity_snapshot state;
    const bool ok =
        mc_live_tile_entities_get(f->worlds[0], 8, 64, z, &tile, &error) == MC_WORLD_STORAGE_OK &&
        tile != NULL && mc_tile_entity_get_snapshot(tile, &state) == MC_TILE_ENTITY_OK &&
        state.kind == MC_TILE_ENTITY_PISTON && state.world == f->worlds[0] && state.x == 8 &&
        state.y == 64 && state.z == z && !state.invalid && state.piston_block_id == id &&
        state.piston_facing == 3 && state.piston_extending == extending &&
        state.piston_render_head == head && state.piston_progress_bits == current &&
        state.piston_previous_progress_bits == previous &&
        mc_tile_entity_piston_code_classes_held(tile) == f->classes[0];
    mc_tile_entity_release(tile);
    return ok;
}

static bool activate(fixture *f, bool context, mc_world_placement_error *error,
                     mc_world_storage_status expected) {
    const mc_block *lever = NULL;
    bool out = false;
    const mc_world_block_activation_context source = {.block_code_classes = f->classes[0]};
    return mc_registry_block_get(f->registry[0], 69, &lever, NULL) == MC_REGISTRY_OK &&
           lever != NULL &&
           mc_world_block_activate_with_context(f->registry[0], lever, f->worlds[0], 7, 64, 1, NULL,
                                                context ? &source : NULL, &out,
                                                error) == expected &&
           (expected != MC_WORLD_STORAGE_OK || out);
}

static bool three_ticks(fixture *f, int32_t first_z, int32_t carried_id, bool extending, bool head,
                        size_t count) {
    bool ok = false;
    mc_world_entity_tick_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL && f->worlds[0]->fields.c->count == count);
    CHECK(mc_world_runtime_tick_entities(f->worlds[0], NULL, NULL, error) == MC_WORLD_STORAGE_OK &&
          !f->worlds[0]->fields.L &&
          moving_is(f, first_z, carried_id, extending, head, 0x3f000000u, 0) &&
          mc_world_entity_tick_error_retire(error));
    CHECK(mc_world_runtime_tick_entities(f->worlds[0], NULL, NULL, error) == MC_WORLD_STORAGE_OK &&
          moving_is(f, first_z, carried_id, extending, head, 0x3f800000u, 0x3f000000u) &&
          mc_world_entity_tick_error_retire(error));
    CHECK(mc_world_runtime_tick_entities(f->worlds[0], NULL, NULL, error) == MC_WORLD_STORAGE_OK &&
          f->worlds[0]->fields.c->count == 0 && f->chunks[0]->l->count == 0 &&
          mc_live_tile_entities_lists_valid(f->worlds[0]) &&
          mc_live_tile_entities_map_valid(f->chunks[0]) &&
          mc_world_entity_tick_error_retire(error));
    ok = true;
cleanup:
    if (error != NULL && !mc_world_entity_tick_error_retire(error))
        ok = false;
    free(error);
    return ok;
}

enum piston_case {
    NORMAL,
    STICKY,
    TWELVE,
    THIRTEEN,
    OBSIDIAN,
    BEDROCK,
    MISSING_CODE,
    SOUND_FAILURE,
    ALLOC_FAILURE
};

static const char *const case_names[] = {"normal-push",  "sticky-pull",   "twelve-push",
                                         "thirteen-cap", "obsidian",      "bedrock",
                                         "missing-code", "sound-failure", "factory-class-failure"};

typedef struct neighbor_producer {
    fixture *fixture;
    mc_world_access *access;
    mc_world_access_receipt *receipt;
    mc_entity *authored, *loaded;
    size_t calls, initial_entities;
    bool valid;
} neighbor_producer;

/* Declared native callback failure, not an original Throwable or parity case.
 * The receipt is created by the actual entered producer, never reconstructed
 * from a status/operation string by the notification caller. */
static mc_world_storage_status neighbor_entity_failure(void *context, mc_world_runtime *world,
                                                       mc_entity *entity,
                                                       mc_world_storage_error *cause,
                                                       mc_world_access_failure *failure) {
    neighbor_producer *p = context;
    ++p->calls;
    p->loaded = entity;
    p->valid = p->calls == 1 && world == p->fixture->worlds[0] && entity != NULL &&
               entity != p->authored && entity->base.world == world && entity->base.x == 24.5 &&
               entity->base.y == 64.0 && entity->base.z == 8.5 &&
               world->fields.b->count == p->initial_entities + 1 &&
               world->fields.b->items[p->initial_entities] == entity &&
               p->access->active_holds == 1;
    if (!p->valid)
        fprintf(stderr,
                "Neighbor producer: calls=%zu world-same=%d entity-present=%d distinct=%d "
                "entity-world-same=%d xyz=%.17g,%.17g,%.17g authored-y=%.17g "
                "list=%zu initial=%zu appended-same=%d active-holds=%zu\n",
                p->calls, world == p->fixture->worlds[0], entity != NULL, entity != p->authored,
                entity != NULL && entity->base.world == world,
                entity == NULL ? 0.0 : entity->base.x, entity == NULL ? 0.0 : entity->base.y,
                entity == NULL ? 0.0 : entity->base.z, p->authored->base.y, world->fields.b->count,
                p->initial_entities,
                world->fields.b->count > p->initial_entities &&
                    world->fields.b->items[p->initial_entities] == entity,
                p->access->active_holds);
    const mc_world_storage_status status =
        mc_world_access_receipt_create(p->access, NULL, &p->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Authored neighbor Chunk activation producer failure", 77};
    return cause->status;
}

static bool neighbor_input(fixture *f, mc_entity *entity) {
    bool ok = false, serialized = false;
    fixture_record *r = calloc(1, sizeof(*r));
    mc_nbt_tree *saved = NULL;
    uint8_t *wire = NULL;
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
    CHECK(r != NULL &&
          mc_entity_save_nbt(entity, NULL, &serialized, &saved, NULL) == MC_ENTITY_NBT_OK &&
          serialized);
    const mc_nbt_tag *entities[] = {mc_nbt_tree_root(saved)};
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    r->fields[0].value.int_value = 1;
    r->fields[8].value.byte_value = 1;
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[9].value.list.items = entities;
    r->fields[9].value.list.length = 1;
    memset(r->arrays[2], 255, MC_CHUNK_NIBBLE_BYTES);
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK);
    wire = malloc(length);
    CHECK(wire != NULL && mc_nbt_write(&r->root, NULL, wire, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[0]), 1, 0, wire, length,
                                 fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(wire);
    mc_nbt_tree_free(saved);
    free(r);
    return ok;
}

static bool neighbor_failure_control(fixture *f) {
    bool ok = false, registered = false;
    neighbor_producer producer = {.fixture = f, .initial_entities = f->worlds[0]->fields.b->count};
    mc_world_access_failure captured = {0};
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_chunk_state *neighbor = NULL;
    const mc_world_access_source_callbacks callbacks = {.entity_added = neighbor_entity_failure};
    mc_chunk_provider_cache_observation before, after;
    CHECK(error != NULL && mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK &&
          make_entity(f, 0, f->worlds[0], 24.5, 64.0, 8.5, &producer.authored) &&
          neighbor_input(f, producer.authored));
    CHECK(mc_world_access_source_create(&callbacks, &producer, &producer.access, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], producer.access, NULL) ==
              MC_WORLD_STORAGE_OK);
    registered = true;
    CHECK(mc_chunk_provider_cache_observe(f->worlds[0]->fields.v, &before));
    mc_world_mutation_error_init(error);
    /* First neighbor15 is already loaded; second17 imports authored Chunk1
     * and reaches its real Entity registration before a Block value returns. */
    const bool returned =
        mc_world_mutation_neighbors_with_code(f->classes[0], f->worlds[0], 16, 64, 8, 33, error);
    for (size_t i = 0; i < f->providers[0].cache->insertion_count; ++i) {
        mc_chunk_state *state = f->providers[0].cache->insertions[i];
        if (state->x == 1 && state->z == 0)
            neighbor = state;
    }
    if (!producer.valid)
        fprintf(stderr,
                "Neighbor import: returned=%d calls=%zu status=%d operation=%s query=%d "
                "query-cause=%s batch=%d tile=%d Registry=%d activation=%u/%u load=%u/%u\n",
                returned, producer.calls, (int)error->status,
                error->operation == NULL ? "(none)" : error->operation, (int)error->query.status,
                error->query.cause.operation == NULL ? "(none)" : error->query.cause.operation,
                (int)error->batch.publication.status, (int)error->tile_entity.status,
                (int)error->registry.status, error->activation_bytecode,
                error->activation_world_bytecode, error->load_bytecode,
                error->load_builder_bytecode);
    CHECK(!returned);
    CHECK(producer.valid);
    CHECK(producer.calls == 1);
    CHECK(neighbor != NULL);
    CHECK(neighbor->active);
    CHECK(f->worlds[0]->fields.b->count == producer.initial_entities + 1);
    CHECK(mc_chunk_provider_cache_observe(f->worlds[0]->fields.v, &after));
    if (after.hits + after.generic_gets != before.hits + before.generic_gets + 2)
        fprintf(stderr, "Neighbor control GETs: before=%llu/%llu after=%llu/%llu\n",
                (unsigned long long)before.hits, (unsigned long long)before.generic_gets,
                (unsigned long long)after.hits, (unsigned long long)after.generic_gets);
    CHECK(after.hits + after.generic_gets == before.hits + before.generic_gets + 2);
    CHECK(error->status == MC_WORLD_STORAGE_IO_ERROR && error->cause.system_error == 77 &&
          error->neighbor_call_bytecode == 19 && error->neighbor_bytecode == 22 &&
          error->query.status == MC_WORLD_STORAGE_IO_ERROR &&
          error->query.acquisition != MC_CHUNK_ACQUIRE_OK && error->activation_bytecode != 0 &&
          error->batch.index == 0 &&
          error->batch.publication.world_access_failure.receiver == producer.access &&
          error->batch.publication.world_access_failure.receipt == producer.receipt &&
          error->batch.publication.world_access_failure.render_global == NULL &&
          !error->batch.publication.world_access_failure.source_null_receiver);
    CHECK(mc_world_access_failure_retain(&error->batch.publication.world_access_failure,
                                         &captured) == MC_WORLD_STORAGE_OK &&
          captured.receipt == producer.receipt);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->worlds[0], producer.access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    registered = false;
    CHECK(mc_world_access_receipt_retire(&producer.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
          producer.receipt == captured.receipt &&
          mc_world_access_free(producer.access, NULL) == MC_WORLD_STORAGE_BUSY);
    const char *exception = "untouched";
    CHECK(!mc_world_access_failure_source_exception(&captured, &exception) &&
          strcmp(exception, "untouched") == 0);
    mc_world_access_failure_release(&captured);
    CHECK(mc_world_access_receipt_retire(&producer.receipt, NULL) == MC_WORLD_STORAGE_OK &&
          producer.receipt == NULL);
    ok = true;
cleanup:
    mc_world_access_failure_release(&captured);
    if (registered) {
        bool removed_on_failure = false;
        if (mc_world_runtime_remove_world_access(f->worlds[0], producer.access, &removed_on_failure,
                                                 NULL) != MC_WORLD_STORAGE_OK ||
            !removed_on_failure)
            ok = false;
    }
    if (mc_world_access_receipt_retire(&producer.receipt, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_access_free(producer.access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    free(error);
    /* Genuine terminal retirement drains both actual loaded Chunk graphs.
     * The inherited fixture tracks Chunk0; retire the additional real Chunk
     * only after Source Lists and provider descriptors have been retired. */
    if (neighbor != NULL) {
        if (mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->terminal[0] = true;
        if (!retire_descriptors(f, 0) ||
            mc_world_store_retire_chunk(f->stores[0], neighbor, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    return ok;
}

static bool run_case(const char *root, enum piston_case which) {
    bool ok = false, registered = false;
    fixture *f = calloc(1, sizeof(*f));
    mc_world_placement_error *error = calloc(1, sizeof(*error));
    mc_world_access *access = NULL;
    mc_tile_entity *sentinel = NULL;
    piston_observer observer = {.fail_sound = which == SOUND_FAILURE};
    const bool blocked = which == THIRTEEN || which == OBSIDIAN || which == BEDROCK;
    const int32_t base_id = which == STICKY ? 29 : 33;
    const size_t stones = which == TWELVE ? 12u : which == THIRTEEN ? 13u : 1u;
    const uint8_t carried = which == OBSIDIAN ? 49 : which == BEDROCK ? 7 : 1;
    CHECK(f != NULL && error != NULL &&
          piston_fixture(f, root, base_id, stones, carried, which == SOUND_FAILURE));
    if (which == ALLOC_FAILURE)
        CHECK(mc_tile_entity_piston_class_configure(f->worlds[0], 0) == MC_TILE_ENTITY_OK);
    const mc_world_access_callbacks callbacks = {.cell = passive_cell,
                                                 .range = passive_range,
                                                 .sound = piston_sound_observer,
                                                 .particle = passive_particle,
                                                 .entity_added = passive_entity,
                                                 .tile_entity_dirty = passive_dirty,
                                                 .entity_removed = passive_entity,
                                                 .sky_changed = passive_sky,
                                                 .auxiliary = passive_aux};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    const mc_world_storage_status expected = which == SOUND_FAILURE ? MC_WORLD_STORAGE_IO_ERROR
                                             : which == MISSING_CODE
                                                 ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                             : which == ALLOC_FAILURE ? MC_WORLD_STORAGE_LIMIT
                                                                      : MC_WORLD_STORAGE_OK;
    CHECK(activate(f, which != MISSING_CODE, error, expected));
    const mc_block *base = NULL;
    CHECK(mc_registry_block_get(f->registry[0], base_id, &base, NULL) == MC_REGISTRY_OK &&
          base != NULL);
    if (which == MISSING_CODE || which == SOUND_FAILURE || which == ALLOC_FAILURE) {
        CHECK(base->subtype.jq.b && cell_is(f, 1, base_id, 11) && error->mutation != NULL &&
              error->mutation->piston.event != 0 && !error->source_exception &&
              error->native_policy);
        if (which == MISSING_CODE || which == ALLOC_FAILURE)
            CHECK(error->mutation->piston.factory == 0 && f->worlds[0]->fields.c->count == 0 &&
                  cell_is(f, 3, 36, 0));
        else
            CHECK(observer.sounds == 1 && error->mutation->piston.event == 78 &&
                  f->worlds[0]->fields.c->count == 2 && moving_is(f, 2, 34, true, false, 0, 0) &&
                  moving_is(f, 3, 1, true, false, 0, 0));
        if (which == ALLOC_FAILURE) {
            const mc_tile_entity_error first = error->mutation->tile_entity.tile_entity;
            CHECK(first.status == MC_TILE_ENTITY_LIMIT && first.piston_initializer_bytecode == 0);
            CHECK(mc_tile_entity_allocate_in_classes(f->classes[0], MC_TILE_ENTITY_CHEST,
                                                     &sentinel) == MC_TILE_ENTITY_OK &&
                  sentinel != NULL);
            mc_tile_entity *unchanged = sentinel;
            mc_tile_entity_error repeated;
            mc_tile_entity_piston_class_snapshot before, after;
            const mc_java_random random = *f->worlds[0]->fields.r;
            const size_t loaded = mc_world_store_chunk_count(f->stores[0]);
            CHECK(mc_tile_entity_piston_class_get_snapshot(f->classes[0], &before) ==
                      MC_TILE_ENTITY_OK &&
                  before.piston_phase == MC_TILE_ENTITY_CLASS_FAILED &&
                  before.piston_failure == MC_TILE_ENTITY_LIMIT &&
                  mc_tile_entity_allocate_in_classes_detailed(f->classes[0], MC_TILE_ENTITY_PISTON,
                                                              &unchanged,
                                                              &repeated) == MC_TILE_ENTITY_LIMIT &&
                  unchanged == sentinel && repeated.status == first.status &&
                  repeated.initializer_bytecode == first.initializer_bytecode &&
                  repeated.piston_initializer_bytecode == first.piston_initializer_bytecode &&
                  repeated.registry.status == first.registry.status &&
                  mc_tile_entity_allocate_in_classes(f->classes[0], MC_TILE_ENTITY_PISTON,
                                                     &unchanged) == MC_TILE_ENTITY_LIMIT &&
                  unchanged == sentinel &&
                  mc_tile_entity_piston_class_get_snapshot(f->classes[0], &after) ==
                      MC_TILE_ENTITY_OK &&
                  after.piston_phase == before.piston_phase &&
                  after.piston_failure == before.piston_failure &&
                  after.piston_initializer_bytecode == before.piston_initializer_bytecode &&
                  after.live_tile_entities == before.live_tile_entities &&
                  after.allocation_bytes == before.allocation_bytes &&
                  mc_world_store_chunk_count(f->stores[0]) == loaded &&
                  memcmp(&random, f->worlds[0]->fields.r, sizeof(random)) == 0);
            mc_tile_entity_release(sentinel);
            sentinel = NULL;
        }
        const size_t tile_count = f->worlds[0]->fields.c->count;
        const mc_java_random random = *f->worlds[0]->fields.r;
        CHECK(mc_world_placement_error_retire(error) && base->subtype.jq.b &&
              f->worlds[0]->fields.c->count == tile_count &&
              memcmp(&random, f->worlds[0]->fields.r, sizeof(random)) == 0);
    } else {
        CHECK(mc_world_placement_error_retire(error) && !base->subtype.jq.b);
        if (blocked) {
            CHECK(cell_is(f, 1, base_id, 3) && f->worlds[0]->fields.c->count == 0 &&
                  observer.sounds == 0);
        } else {
            CHECK(cell_is(f, 1, base_id, 11) && moving_is(f, 2, 34, true, false, 0, 0) &&
                  f->worlds[0]->fields.c->count == stones + 1 && observer.sounds == 1);
            for (size_t i = 0; i < stones; ++i)
                CHECK(moving_is(f, (int32_t)i + 3, 1, true, false, 0, 0));
            CHECK(three_ticks(f, 2, 34, true, false, stones + 1) &&
                  cell_is(f, 2, 34, which == STICKY ? 11 : 3) &&
                  activate(f, true, error, MC_WORLD_STORAGE_OK) &&
                  mc_world_placement_error_retire(error));
            CHECK(moving_is(f, 1, base_id, false, true, 0, 0) &&
                  f->worlds[0]->fields.c->count == (which == STICKY ? 2u : 1u));
            CHECK(three_ticks(f, 1, base_id, false, true, which == STICKY ? 2u : 1u) &&
                  cell_is(f, 1, base_id, 3) && !base->subtype.jq.b &&
                  cell_is(f, 2, which == STICKY ? 1 : 0, 0));
            if (which != STICKY)
                CHECK(cell_is(f, 3, 1, 0));
        }
    }
    if (which == SOUND_FAILURE) {
        /* Its sound-prefix diagnostic has already retired. The following
         * independent native control terminates this World and therefore must
         * not leave an outer registration to remove after terminal discard. */
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed);
        registered = false;
        CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
        access = NULL;
        CHECK(neighbor_failure_control(f));
    }
    ok = true;
cleanup:
    mc_tile_entity_release(sentinel);
    if (error != NULL && !mc_world_placement_error_retire(error))
        ok = false;
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (access != NULL && mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (!cleanup_fixture(f))
        ok = false;
    free(error);
    if (ok)
        printf("PISTON_GAMEPLAY case=%s cleanup=1 source-field-repair=0\n", case_names[which]);
    return ok;
}

int main(int argc, char **argv) {
    char root[8192], path[8192];
    if (argc != 3 || strcmp(argv[1], "world-piston-gameplay") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    for (size_t i = 0; i < COUNT(case_names); ++i) {
        const int n = snprintf(path, sizeof(path), "%s/case%zu", root, i);
        if (n <= 0 || (size_t)n >= sizeof(path) || !run_case(path, (enum piston_case)i))
            return 1;
    }
    printf("PASS: Piston gameplay; cases=%zu; checks=%u; cleanup=1; source-field-repair=0\n",
           COUNT(case_names), checks);
    return 0;
}
