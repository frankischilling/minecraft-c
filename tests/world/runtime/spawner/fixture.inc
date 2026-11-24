/* Genuine GAME/Region/Chunk helpers. Their other test entry is never run. */
#define MC_COLLISION_BOXES_MAIN mc_spawner_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "entity/construction_internal.h"
#include "mc/entity/factory/factory.h"
#include "mc/entity/player/player.h"
#include "mc/world/runtime/entity_tick.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"

typedef struct spawner_fixture {
    fixture *base;
    mc_player_session_classes *sessions;
    mc_player_session *session;
    mc_entity *player;
    mc_tile_entity *tile;
    mc_math_classes *math;
} spawner_fixture;

/* All terrain/Tile fields are authored wire inputs before the first Chunk
 * acquisition. This does not replace or repair any reached tick result. */
static bool spawner_record_input(fixture *f, const char *entity_name, int16_t delay,
                                 uint8_t floor_id) {
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
    mc_nbt_tag tile = {.type = MC_NBT_COMPOUND}, fields[6] = {{0}};
    const mc_nbt_tag *tile_fields[6], *tile_items[1] = {&tile};
    uint16_t field_names[6][16], id[16], name[64];
    const char *const tile_names[] = {"id", "x", "y", "z", "EntityId", "Delay"};
    CHECK(r != NULL && strlen(entity_name) < COUNT(name));
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[2].value.long_value = 0;
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            r->arrays[0][boxes_index(x, 63, z)] = floor_id;
    r->arrays[0][boxes_index(8, 64, 8)] = 52;
    memset(r->arrays[2], 255, MC_CHUNK_NIBBLE_BYTES);
    memset(r->arrays[4], 64, MC_CHUNK_HEIGHTMAP_BYTES);
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    for (size_t i = 0; i < COUNT(fields); ++i) {
        fields[i].name = ascii(tile_names[i], field_names[i]);
        tile_fields[i] = &fields[i];
    }
    fields[0].type = fields[4].type = MC_NBT_STRING;
    fields[0].value.string = ascii("MobSpawner", id);
    fields[4].value.string = ascii(entity_name, name);
    for (size_t i = 1; i <= 3; ++i) {
        fields[i].type = MC_NBT_INT;
        fields[i].value.int_value = i == 2 ? 64 : 8;
    }
    fields[5].type = MC_NBT_SHORT;
    fields[5].value.short_value = delay;
    tile.value.compound.items = tile_fields;
    tile.value.compound.length = COUNT(tile_fields);
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.items = tile_items;
    r->fields[10].value.list.length = 1;
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
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[0]), 0, 0, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool spawner_open_with_entity_limits(spawner_fixture *s, const char *root, const char *name,
                                            int16_t delay, uint8_t floor_id, bool near,
                                            const mc_entity_limits *entity_limits) {
    bool ok = false;
    const uint16_t title[] = {'S', 'p', 'a', 'w', 'n', 'e', 'r'};
    fixture *f = s->base = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_materials_create(&f->materials) &&
          mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, entity_limits,
                                   &f->entity_classes[0]) == MC_ENTITY_OK &&
          mc_math_classes_create(SIZE_MAX, &s->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(s->math, NULL) == MC_MATH_CLASS_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(s->math), &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          boxes_open_store(f, 0, root) && spawner_record_input(f, name, delay, floor_id));
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = 23, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_construct_world(f->worlds[0], &arguments, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
              MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    CHECK(mc_world_runtime_get(f->worlds[0], 0, 0, &f->states[0]) == MC_CHUNK_ACQUIRE_OK);
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], f->states[0]);
    CHECK(f->chunks[0] != NULL && f->states[0]->active &&
          mc_live_tile_entities_get(f->worlds[0], 8, 64, 8, &s->tile, NULL) ==
              MC_WORLD_STORAGE_OK &&
          s->tile != NULL && s->tile->state.spawner.delay == delay &&
          f->worlds[0]->fields.c->count == 1 && f->worlds[0]->fields.c->items[0] == s->tile);
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &s->sessions,
                                           NULL) == MC_PLAYER_OK &&
          mc_player_session_create(s->sessions, NULL, NULL, &s->session, NULL) == MC_PLAYER_OK &&
          mc_local_player_allocate(f->entity_classes[0], &s->player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(s->player, f->worlds[0], NULL, s->session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    mc_entity_position_rotation_error pose;
    CHECK(mc_entity_source_position_rotation(s->player, 1.5, near ? 64.0 : 100.0, 1.5, 0, 0,
                                             &pose) == MC_ENTITY_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], s->player, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted && f->worlds[0]->fields.d->count == 1 &&
          f->worlds[0]->fields.d->items[0] == s->player && f->worlds[0]->fields.b->count == 1);
    ok = true;
cleanup:
    return ok;
}

static bool spawner_open(spawner_fixture *s, const char *root, const char *name, int16_t delay,
                         uint8_t floor_id, bool near) {
    return spawner_open_with_entity_limits(s, root, name, delay, floor_id, near, NULL);
}

static bool spawner_close(spawner_fixture *s) {
    mc_tile_entity_release(s->tile);
    s->tile = NULL;
    mc_entity_release(s->player);
    s->player = NULL;
    mc_player_session_retire(&s->session);
    if (mc_player_session_classes_free(&s->sessions, NULL) != MC_PLAYER_OK)
        return false;
    if (!cleanup_fixture(s->base))
        return false;
    s->base = NULL;
    if (s->math != NULL && mc_math_classes_release(&s->math, NULL) != MC_MATH_CLASS_OK)
        return false;
    return true;
}
