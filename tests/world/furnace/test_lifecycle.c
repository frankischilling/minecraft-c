/* Exact finite post-step replay of the independently captured original FD.g
 * Furnace flow. No claim of replaying its separate 196-phase JDI journal. */
#define MC_FURNACE_TICK_MAIN mc_lifecycle_unused_furnace_tick_main
#include "../runtime/tile_entities/test_furnace_tick.c"
#undef MC_FURNACE_TICK_MAIN

#include "../../reference/world/furnace_lifecycle/native-observations.h"

typedef struct lifecycle_fixture {
    fixture *base;
    mc_chunk_state *states[4];
    mc_world_chunk *chunks[4];
    const char *tile_names[8], *stack_names[8];
    mc_tile_entity *tiles[8];
    mc_item_stack *stacks[8];
    size_t tile_count, stack_count;
} lifecycle_fixture;

typedef struct lifecycle_record {
    fixture_record base;
    mc_nbt_tag tile, tile_fields[7], items[2], item_fields[2][4];
    const mc_nbt_tag *tile_values[1], *tile_children[7], *item_values[2], *item_children[2][4];
    uint16_t tile_names[7][16], item_names[2][4][16], id_units[7];
} lifecycle_record;

static bool lifecycle_seed(lifecycle_fixture *p, int32_t x, int32_t z) {
    bool ok = false;
    fixture *f = p->base;
    lifecycle_record *record = calloc(1, sizeof(*record));
    uint8_t *wire = NULL;
    size_t bytes = 0, written = 0;
    CHECK(record != NULL);
    fixture_record *r = &record->base;
    const char *names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->fields[i].type = types[i];
        r->level_fields[i] = &r->fields[i];
    }
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(names);
    r->fields[0].value.int_value = x;
    r->fields[1].value.int_value = z;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[3 + i].value.byte_array.data = r->arrays[i];
        r->fields[3 + i].value.byte_array.length = lengths[i];
    }
    r->fields[8].value.byte_value = 1;
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    if (x == 0 && z == 0) {
        const size_t at = boxes_index(8, 64, 8);
        r->arrays[0][at] = 61;
        r->arrays[1][at / 2] = 3;
        record->tile.type = MC_NBT_COMPOUND;
        const char *tile_names[] = {"id", "x", "y", "z", "BurnTime", "CookTime", "Items"};
        for (size_t i = 0; i < COUNT(tile_names); ++i) {
            record->tile_fields[i].name = ascii(tile_names[i], record->tile_names[i]);
            record->tile_children[i] = &record->tile_fields[i];
        }
        record->tile_fields[0].type = MC_NBT_STRING;
        record->tile_fields[0].value.string = ascii("Furnace", record->id_units);
        const int32_t xyz[] = {8, 64, 8};
        for (size_t i = 0; i < 3; ++i) {
            record->tile_fields[1 + i].type = MC_NBT_INT;
            record->tile_fields[1 + i].value.int_value = xyz[i];
        }
        record->tile_fields[4].type = record->tile_fields[5].type = MC_NBT_SHORT;
        record->tile_fields[6].type = MC_NBT_LIST;
        record->tile_fields[6].value.list.element_type = MC_NBT_COMPOUND;
        record->tile_fields[6].value.list.length = 2;
        record->tile_fields[6].value.list.items = record->item_values;
        const char *keys[] = {"Slot", "id", "Count", "Damage"};
        for (size_t i = 0; i < 2; ++i) {
            record->items[i].type = MC_NBT_COMPOUND;
            record->items[i].value.compound.items = record->item_children[i];
            record->items[i].value.compound.length = 4;
            record->item_values[i] = &record->items[i];
            for (size_t j = 0; j < 4; ++j) {
                mc_nbt_tag *tag = &record->item_fields[i][j];
                tag->name = ascii(keys[j], record->item_names[i][j]);
                tag->type = j == 0 || j == 2 ? MC_NBT_BYTE : MC_NBT_SHORT;
                record->item_children[i][j] = tag;
            }
            record->item_fields[i][0].value.byte_value = (int8_t)i;
            record->item_fields[i][1].value.short_value = (int16_t)(i == 0 ? 15 : 263);
            record->item_fields[i][2].value.byte_value = 2;
        }
        record->tile.value.compound.items = record->tile_children;
        record->tile.value.compound.length = COUNT(tile_names);
        record->tile_values[0] = &record->tile;
        r->fields[10].value.list.length = 1;
        r->fields[10].value.list.items = record->tile_values;
    }
    CHECK(mc_nbt_measure(&r->root, NULL, &bytes, NULL) == MC_NBT_OK && bytes != 0);
    wire = malloc(bytes);
    CHECK(wire != NULL && mc_nbt_write(&r->root, NULL, wire, bytes, &written, NULL) == MC_NBT_OK &&
          written == bytes);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[0]), x, z, wire, bytes,
                                 0, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(wire);
    free(record);
    return ok;
}

static bool lifecycle_open(lifecycle_fixture *p, const char *root) {
    bool ok = false;
    fixture *f = p->base;
    const uint16_t title[] = {'P', 'i', 'c', 'k'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
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
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    char path[8192];
    const int n = snprintf(path, sizeof(path), "%s/world0", root);
    uint8_t map[256];
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    CHECK(n > 0 && (size_t)n < sizeof(path));
    mc_world_store_options storage = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    storage.limits.max_loaded_chunks = 4;
    CHECK(mc_world_store_open(path, f->regions, &storage, &f->stores[0], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[0], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 4; ++i)
        CHECK(lifecycle_seed(p, mc_furnace_lifecycle_chunk_open_order[i][0],
                             mc_furnace_lifecycle_chunk_open_order[i][1]));
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
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
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_world_runtime_get(f->worlds[0], mc_furnace_lifecycle_chunk_open_order[i][0],
                                   mc_furnace_lifecycle_chunk_open_order[i][1],
                                   &p->states[i]) == MC_CHUNK_ACQUIRE_OK);
        p->chunks[i] = mc_world_store_chunk_object(f->stores[0], p->states[i]);
        CHECK(p->chunks[i] != NULL && p->states[i]->active && p->states[i]->terrain_populated &&
              !p->states[i]->dirty);
    }
    f->states[0] = p->states[0];
    f->chunks[0] = p->chunks[0];
    CHECK(f->providers[0].cache->count == 4 && f->providers[0].cache->insertion_count == 4 &&
          mc_world_store_chunk_count(f->stores[0]) == 4);
    ok = true;
cleanup:
    return ok;
}

static bool lifecycle_prepared(lifecycle_fixture *p) {
    bool ok = false;
    fixture *f = p->base;
    mc_world_provider_fields provider;
    mc_world_info_fields info;
    CHECK(!f->worlds[0]->fields.B &&
          mc_world_provider_fields_get(f->worlds[0]->fields.t, &provider) == MC_WORLD_PROVIDER_OK &&
          provider.a == f->worlds[0] && !provider.c && !provider.d &&
          provider.e == mc_furnace_lifecycle_no_sky && provider.g == 0 &&
          mc_world_info_fields_get(f->worlds[0]->fields.x, &info) == MC_WORLD_INFO_OK &&
          info.value.seed == mc_furnace_lifecycle_seed &&
          info.value.time == mc_furnace_lifecycle_time && info.value.spawn_x == 0 &&
          info.value.spawn_y == 64 && info.value.spawn_z == 0);
    for (size_t i = 0; i < 4; ++i) {
        mc_world_chunk_fields fields;
        CHECK(mc_world_chunk_fields_get(p->chunks[i], &fields) == MC_WORLD_STORAGE_OK && fields.c &&
              fields.d == f->worlds[0] && fields.i == 0 &&
              fields.j == mc_furnace_lifecycle_chunk_open_order[i][0] &&
              fields.k == mc_furnace_lifecycle_chunk_open_order[i][1] && fields.n && !fields.o &&
              !fields.p && !fields.q && fields.r == 0 &&
              f->providers[0].cache->insertions[i] == p->states[i]);
        const mc_chunk_data *data = p->states[i]->data;
        for (size_t at = 0; at < MC_CHUNK_BLOCKS; ++at)
            CHECK(data->blocks[at] == (i == 0 && at == boxes_index(8, 64, 8) ? 61 : 0));
        for (size_t at = 0; at < MC_CHUNK_NIBBLE_BYTES; ++at)
            CHECK(data->metadata[at] == (i == 0 && at == boxes_index(8, 64, 8) / 2 ? 3 : 0) &&
                  data->skylight[at] == 0 && data->block_light[at] == 0);
        for (size_t at = 0; at < MC_CHUNK_HEIGHTMAP_BYTES; ++at)
            CHECK(data->heightmap[at] == 0);
    }
    ok = true;
cleanup:
    return ok;
}

static bool lifecycle_tile(lifecycle_fixture *p, const char *name, mc_tile_entity *actual) {
    bool ok = false;
    CHECK(name != NULL && actual != NULL && actual->kind == MC_TILE_ENTITY_FURNACE);
    size_t index = 0;
    while (index < p->tile_count && strcmp(p->tile_names[index], name) != 0)
        ++index;
    if (index == p->tile_count) {
        CHECK(index < COUNT(p->tiles));
        for (size_t i = 0; i < index; ++i)
            CHECK(p->tiles[i] != actual);
        CHECK(mc_tile_entity_retain(actual) == MC_TILE_ENTITY_OK);
        p->tile_names[index] = name;
        p->tiles[index] = actual;
        ++p->tile_count;
    } else
        CHECK(p->tiles[index] == actual);
    ok = true;
cleanup:
    return ok;
}

static bool lifecycle_stack(lifecycle_fixture *p, const char *name, mc_item_stack *actual) {
    bool ok = false;
    if (name == NULL) {
        CHECK(actual == NULL);
        return true;
    }
    CHECK(actual != NULL);
    size_t index = 0;
    while (index < p->stack_count && strcmp(p->stack_names[index], name) != 0)
        ++index;
    if (index == p->stack_count) {
        CHECK(index < COUNT(p->stacks));
        for (size_t i = 0; i < index; ++i)
            CHECK(p->stacks[i] != actual);
        CHECK(mc_item_stack_retain(actual) == MC_ITEM_STACK_OK);
        p->stack_names[index] = name;
        p->stacks[index] = actual;
        ++p->stack_count;
    } else
        CHECK(p->stacks[index] == actual);
    ok = true;
cleanup:
    return ok;
}

static bool lifecycle_state(lifecycle_fixture *p,
                            const mc_furnace_lifecycle_state_reference *expected) {
    bool ok = false;
    fixture *f = p->base;
    mc_world_runtime *w = f->worlds[0];
    CHECK(p->states[0]->data->blocks[boxes_index(8, 64, 8)] == expected->block &&
          (p->states[0]->data->metadata[boxes_index(8, 64, 8) / 2] & 15) == expected->metadata &&
          w->fields.L == expected->iterating && f->registry[0]->tc_c == expected->suppression &&
          w->fields.c->count == expected->active_count &&
          w->fields.G->count == expected->pending_count);
    for (size_t i = 0; i < expected->active_count; ++i)
        CHECK(lifecycle_tile(p, expected->active[i], w->fields.c->items[i]));
    for (size_t i = 0; i < expected->pending_count; ++i)
        CHECK(lifecycle_tile(p, expected->pending[i], w->fields.G->items[i]));
    size_t mapped = 0;
    for (size_t c = 0; c < 4; ++c) {
        CHECK(p->chunks[c]->l->count == (c == 0 ? expected->map_count : 0));
        for (size_t i = 0; i < p->chunks[c]->l->count; ++i) {
            mc_live_tile_entity_map_entry entry = {0};
            CHECK(mc_live_tile_entities_map_entry_at(p->chunks[c], i, &entry) ==
                      MC_WORLD_STORAGE_OK &&
                  entry.x == 8 && entry.y == 64 && entry.z == 8 && mapped < expected->map_count &&
                  lifecycle_tile(p, expected->map[mapped], entry.tile_entity));
            ++mapped;
        }
    }
    CHECK(mapped == expected->map_count && p->tile_count == expected->tile_count);
    for (size_t i = 0; i < expected->tile_count; ++i) {
        const mc_furnace_lifecycle_tile_reference *v = &expected->tiles[i];
        size_t index = 0;
        while (index < p->tile_count && strcmp(p->tile_names[index], v->identity) != 0)
            ++index;
        CHECK(index < p->tile_count);
        mc_tile_entity *tile = p->tiles[index];
        CHECK(tile->invalid == v->invalid && (tile->world == w) == v->world_same &&
              tile->x == v->xyz[0] && tile->y == v->xyz[1] && tile->z == v->xyz[2] &&
              furnace_fields(tile, v->fields[0], v->fields[1], v->fields[2]) &&
              tile->state.furnace.capacity == 3 && tile->state.furnace.slots != NULL);
        for (size_t j = 0; j < 3; ++j)
            CHECK(lifecycle_stack(p, v->slots[j], tile->state.furnace.slots[j]));
    }
    CHECK(p->stack_count == expected->stack_count);
    for (size_t i = 0; i < expected->stack_count; ++i) {
        const mc_furnace_lifecycle_stack_reference *v = &expected->stacks[i];
        size_t index = 0;
        while (index < p->stack_count && strcmp(p->stack_names[index], v->identity) != 0)
            ++index;
        mc_item_stack_fields actual;
        CHECK(index < p->stack_count &&
              mc_item_stack_fields_get(p->stacks[index], &actual) == MC_ITEM_STACK_OK &&
              actual.id == v->id && actual.count == v->count && actual.damage == v->damage &&
              actual.animation == v->animation);
    }
    ok = true;
cleanup:
    return ok;
}

static bool lifecycle_replay(lifecycle_fixture *p) {
    bool ok = false;
    mc_world_entity_tick_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL && COUNT(mc_furnace_lifecycle_references) == 7);
    for (size_t i = 0; i < COUNT(mc_furnace_lifecycle_references); ++i) {
        const mc_furnace_lifecycle_reference *v = &mc_furnace_lifecycle_references[i];
        if (v->author_cook)
            CHECK(p->tile_count != 0 &&
                  mc_tile_entity_furnace_field_set(p->tiles[0], MC_FURNACE_COOK_PROGRESS,
                                                   v->cook) == MC_TILE_ENTITY_OK);
        if (v->author_burn)
            CHECK(p->tile_count != 0 &&
                  mc_tile_entity_furnace_field_set(p->tiles[0], MC_FURNACE_BURN_REMAINING,
                                                   v->burn) == MC_TILE_ENTITY_OK);
        if (v->author_fuel_null)
            CHECK(p->tile_count != 0 &&
                  mc_tile_entity_furnace_set(p->tiles[0], 1, NULL) == MC_TILE_ENTITY_OK);
        CHECK(lifecycle_state(p, &v->before));
        if (strcmp(v->operation, "world-g") == 0) {
            const mc_world_storage_status status =
                mc_world_runtime_tick_entities(p->base->worlds[0], NULL, NULL, error);
            if (status != MC_WORLD_STORAGE_OK)
                fprintf(stderr, "Original Furnace lifecycle step=%s status=%d operation=%s\n",
                        v->name, status, error->operation == NULL ? "none" : error->operation);
            CHECK(v->returned && v->exception == NULL && status == MC_WORLD_STORAGE_OK &&
                  mc_world_entity_tick_error_retire(error));
        } else
            CHECK(strcmp(v->operation, "snapshot") == 0 && v->returned && v->exception == NULL);
        CHECK(lifecycle_state(p, &v->after));
        printf("FURNACE_LIFECYCLE step=%s original-post-state=1 active=%zu pending=%zu map=%zu\n",
               v->name, v->after.active_count, v->after.pending_count, v->after.map_count);
    }
    ok = true;
cleanup:
    if (error != NULL && !mc_world_entity_tick_error_retire(error))
        ok = false;
    free(error);
    return ok;
}

static bool lifecycle_close(lifecycle_fixture *p) {
    /* Observation roots preserve detached aliases, never source membership.
     * Drop all of them before actual checked native terminal collection. */
    for (size_t i = 0; i < p->stack_count; ++i)
        mc_item_stack_release(p->stacks[i]);
    for (size_t i = 0; i < p->tile_count; ++i)
        mc_tile_entity_release(p->tiles[i]);
    p->stack_count = p->tile_count = 0;
    fixture *f = p->base;
    if (f == NULL)
        return true;
    if (f->worlds[0] != NULL) {
        if (f->bound[0] && !f->terminal[0]) {
            if (mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->terminal[0] = true;
        }
        if (!retire_descriptors(f, 0))
            return false;
        if (!f->world_retired[0]) {
            if (mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
                return false;
            f->world_retired[0] = true;
        }
        while (mc_world_store_chunk_count(f->stores[0]) != 0) {
            mc_chunk_state *state = NULL;
            if (mc_world_store_chunk_at(f->stores[0], 0, &state, NULL) != MC_WORLD_STORAGE_OK ||
                mc_world_store_retire_chunk(f->stores[0], state, NULL) != MC_WORLD_STORAGE_OK)
                return false;
        }
        f->states[0] = NULL;
        f->chunks[0] = NULL;
    }
    const bool ok = cleanup_fixture(f);
    if (ok)
        p->base = NULL;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    lifecycle_fixture p = {0};
    if (argc != 3 || strcmp(argv[1], "furnace-lifecycle-reference") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    p.base = calloc(1, sizeof(*p.base));
    CHECK(p.base != NULL && lifecycle_open(&p, root) && lifecycle_prepared(&p) &&
          lifecycle_replay(&p));
    result = 0;
cleanup:
    if (!lifecycle_close(&p)) {
        fputs("Original Furnace lifecycle checked cleanup refused; surviving owners retained\n",
              stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u checks; 7 original Furnace World post-step projections; native cleanup=1; "
               "phase-trace-parity=0\n",
               checks);
    return result;
}
