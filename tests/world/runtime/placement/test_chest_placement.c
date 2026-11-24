/* Real World/Store/Chunk inputs for original E.a and its private h child.
 * Public outcomes and completed getter traces replay genuine original/JDI
 * observations. Additional native fault/ownership inputs remain separate. */
#define MC_COLLISION_BOXES_MAIN mc_chest_placement_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "../../../reference/world/chest_placement/native-observations.h"
#include "../../../reference/world/chest_placement/query-observations.h"
#include "mc/world/runtime/acquisition_error.h"
#include "mc/world/runtime/placement.h"

typedef struct chest_placement_fixture {
    fixture *base;
    mc_chunk_state *states[4];
    size_t count;
} chest_placement_fixture;

typedef struct chest_placement_snapshot {
    mc_chunk_data data[4];
    uint8_t blocks[4][MC_CHUNK_BLOCKS];
    mc_java_random random;
    const void *tile_classes;
    size_t tile_count, map_count, bindings;
} chest_placement_snapshot;

static bool chest_seed_record(fixture *f, int32_t cx, int32_t cz) {
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
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[0]), cx, cz, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool chest_placement_open(chest_placement_fixture *p, const char *root, size_t chunks) {
    bool ok = false;
    const uint16_t title[] = {'C', 'h', 'e', 's', 't', 'P', 'l', 'a', 'c', 'e'};
    p->base = calloc(1, sizeof(*p->base));
    fixture *f = p->base;
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
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
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          boxes_open_store(f, 0, root));
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = chunks;
    limits.max_scheduled_entries = 0;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_construct_world(f->worlds[0], &arguments, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
              MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    const int32_t xs[] = {0, 0, 1, 1}, zs[] = {0, 1, 0, 1};
    for (size_t i = 0; i < chunks; ++i)
        CHECK(chest_seed_record(f, xs[i], zs[i]));
    for (size_t i = 0; i < chunks; ++i) {
        CHECK(mc_world_runtime_get(f->worlds[0], xs[i], zs[i], &p->states[i]) ==
                  MC_CHUNK_ACQUIRE_OK &&
              p->states[i] != NULL && p->states[i]->active);
        ++p->count;
    }
    f->states[0] = p->states[0];
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], f->states[0]);
    CHECK(f->chunks[0] != NULL && p->count == chunks &&
          mc_world_store_chunk_count(f->stores[0]) == chunks);
    ok = true;
cleanup:
    return ok;
}

static bool chest_placement_close(chest_placement_fixture *p) {
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
    const bool closed = cleanup_fixture(f);
    if (closed)
        memset(p, 0, sizeof(*p));
    return closed;
}

static void chest_placement_air(chest_placement_fixture *p) {
    for (size_t i = 0; i < p->count; ++i)
        memset(p->states[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
}

static bool chest_placement_put(chest_placement_fixture *p, int32_t x, int32_t y, int32_t z,
                                int32_t id) {
    /* Match the original probe's put(): outside its four prepared Chunks or
     * physical Y array, it authors no cell regardless of the declared ID. The
     * invocation still receives the exact original World/coordinate operands;
     * every reference row is executed and its observed outcome compared. */
    if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127)
        return true;
    const size_t chunk = (size_t)(x >> 4) * 2u + (size_t)(z >> 4);
    if (chunk >= p->count || id < 0 || id > 255)
        return false;
    p->states[chunk]->data->blocks[boxes_index(x, y, z)] = (uint8_t)id;
    return true;
}

static bool chest_placement_observe(chest_placement_fixture *p, chest_placement_snapshot *s) {
    fixture *f = p->base;
    for (size_t i = 0; i < p->count; ++i) {
        memcpy(&s->data[i], p->states[i]->data, sizeof(s->data[i]));
        memcpy(s->blocks[i], p->states[i]->data->blocks, MC_CHUNK_BLOCKS);
    }
    memcpy(&s->random, f->worlds[0]->fields.r, sizeof(s->random));
    s->tile_classes = f->classes[0]->tile_entities;
    s->tile_count = f->worlds[0]->fields.c->count;
    s->map_count = 0;
    for (size_t i = 0; i < p->count; ++i) {
        mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[0], p->states[i]);
        if (chunk == NULL || chunk->l == NULL)
            return false;
        s->map_count += chunk->l->count;
    }
    s->bindings = f->worlds[0]->persistent_bindings;
    return true;
}

static bool chest_placement_same(chest_placement_fixture *p, const chest_placement_snapshot *s) {
    fixture *f = p->base;
    size_t maps = 0;
    for (size_t i = 0; i < p->count; ++i) {
        if (memcmp(&s->data[i], p->states[i]->data, sizeof(s->data[i])) != 0 ||
            memcmp(s->blocks[i], p->states[i]->data->blocks, MC_CHUNK_BLOCKS) != 0)
            return false;
        mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[0], p->states[i]);
        if (chunk == NULL || chunk->l == NULL)
            return false;
        maps += chunk->l->count;
    }
    return mc_test_java_random_equal(&s->random, f->worlds[0]->fields.r) &&
           s->tile_classes == f->classes[0]->tile_entities &&
           s->tile_count == f->worlds[0]->fields.c->count && s->map_count == maps &&
           s->bindings == f->worlds[0]->persistent_bindings;
}

static bool chest_placement_queries(const mc_world_chest_placement_detail *d, size_t count) {
    if (d == NULL || d->query_count != count || count > COUNT(d->queries))
        return false;
    for (size_t i = 0; i < count; ++i) {
        if (d->queries[i] == NULL || d->query_sites[i].class_name == NULL ||
            strcmp(d->query_sites[i].class_name, "e") != 0)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (d->queries[i] == d->queries[j])
                return false;
    }
    return true;
}

static bool chest_placement_exception(const char *actual, const char *original) {
    if (actual == NULL || original == NULL)
        return false;
    /* Java Class.getName uses dots; the native source error uses JVM internal
     * slashes. Compare every character after this representation conversion. */
    for (size_t i = 0;; ++i) {
        const char expected = original[i] == '.' ? '/' : original[i];
        if (actual[i] != expected)
            return false;
        if (expected == '\0')
            return true;
    }
}

static bool chest_placement_trace(const mc_world_chest_placement_detail *actual,
                                  const mc_chest_placement_trace_reference *original) {
    if (actual == NULL || actual->query_count != original->query_count ||
        actual->helper_count != original->helper_count ||
        original->query_count > COUNT(actual->queries)) {
        fprintf(stderr, "Original Chest trace extent mismatch profile=%s\n", original->profile);
        return false;
    }
    for (size_t i = 0; i < original->query_count; ++i) {
        const mc_chest_query_reference *query = &original->queries[i];
        const mc_world_placement_site *site = &actual->query_sites[i];
        if (actual->queries[i] == NULL || !actual->query_returned[i] ||
            actual->query_x[i] != query->x || actual->query_y[i] != query->y ||
            actual->query_z[i] != query->z || actual->query_values[i] != query->value ||
            site->class_name == NULL || strcmp(site->class_name, "e") != 0 ||
            site->method == NULL || strcmp(site->method, query->method) != 0 ||
            site->descriptor == NULL || strcmp(site->descriptor, "(Lfd;III)Z") != 0 ||
            site->pc != (unsigned int)query->pc ||
            actual->query_parent_pc[i] != (unsigned int)query->parent_pc) {
            fprintf(stderr,
                    "Original Chest query mismatch profile=%s ordinal=%zu "
                    "actual=(%d,%d,%d)->%d returned=%d %s:%u parent=%u "
                    "original=(%d,%d,%d)->%d %s:%d parent=%d\n",
                    original->profile, i, (int)actual->query_x[i], (int)actual->query_y[i],
                    (int)actual->query_z[i], (int)actual->query_values[i],
                    actual->query_returned[i], site->method == NULL ? "none" : site->method,
                    site->pc, actual->query_parent_pc[i], (int)query->x, (int)query->y,
                    (int)query->z, (int)query->value, query->method, (int)query->pc,
                    (int)query->parent_pc);
            return false;
        }
    }
    return true;
}

static bool chest_placement_reference(chest_placement_fixture *p) {
    bool ok = false;
    fixture *f = p->base;
    const mc_block *chest = NULL;
    mc_world_placement_error e = {0};
    chest_placement_snapshot *before = malloc(sizeof(*before));
    CHECK(before != NULL &&
          mc_registry_block_get(f->registry[0], 54, &chest, NULL) == MC_REGISTRY_OK &&
          chest != NULL &&
          COUNT(mc_chest_placement_references) == COUNT(mc_chest_placement_trace_references));
    for (size_t i = 0; i < COUNT(mc_chest_placement_references); ++i) {
        const mc_chest_placement_reference *r = &mc_chest_placement_references[i];
        const mc_chest_placement_trace_reference *trace = &mc_chest_placement_trace_references[i];
        CHECK(strcmp(r->profile, trace->profile) == 0);
        chest_placement_air(p);
        CHECK(chest_placement_put(p, r->x, r->y, r->z, r->target_id));
        for (size_t j = 0; j < r->chest_count; ++j)
            CHECK(chest_placement_put(p, r->chests[j][0], r->chests[j][1], r->chests[j][2], 54));
        CHECK(chest_placement_observe(p, before));
        bool value = !r->result;
        const mc_world_storage_status status = mc_world_block_can_place_base(
            f->registry[0], chest, r->null_world ? NULL : f->worlds[0], r->x, r->y, r->z, &value,
            &e);
        const bool matched = r->returned
                                 ? status == MC_WORLD_STORAGE_OK && e.returned && value == r->result
                                 : status != MC_WORLD_STORAGE_OK && !e.returned &&
                                       value == !r->result && e.source_exception &&
                                       chest_placement_exception(e.exception_class, r->exception);
        if (!matched)
            fprintf(stderr,
                    "Original Chest placement mismatch profile=%s status=%d returned=%d value=%d\n",
                    r->profile, (int)status, e.returned, value);
        /* Null World has no entered original FD.a: the source invocation
         * throws before entering the callee. The observed zero stays exact. */
        CHECK(matched && e.query_count == 0 && e.chest_placement != NULL &&
              chest_placement_trace(e.chest_placement, trace) &&
              mc_world_placement_error_retire(&e) && chest_placement_same(p, before));
    }
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    free(before);
    return ok;
}

static bool chest_placement_native(chest_placement_fixture *p) {
    bool ok = false;
    fixture *f = p->base;
    mc_world_runtime *world = f->worlds[0];
    const mc_block *chest = NULL, *dirt = NULL;
    mc_world_placement_error e = {0};
    chest_placement_snapshot *before = malloc(sizeof(*before));
    CHECK(before != NULL &&
          mc_registry_block_get(f->registry[0], 54, &chest, NULL) == MC_REGISTRY_OK &&
          chest != NULL &&
          mc_registry_block_get(f->registry[0], 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL);
    const int32_t xs[] = {7, 9, 8, 8}, zs[] = {8, 8, 7, 9};
    for (size_t orientation = 0; orientation < 5; ++orientation) {
        chest_placement_air(p);
        if (orientation < 4)
            CHECK(chest_placement_put(p, xs[orientation], 64, zs[orientation], 54));
        CHECK(chest_placement_observe(p, before));
        bool value = false;
        CHECK(mc_world_runtime_can_place(world, 54, 8, 64, 8, false, 1, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value && e.returned && e.query_count == 1 && e.queries[0] != NULL &&
              e.chest_placement != NULL && e.chest_placement->returned &&
              e.chest_placement->helper_count == 4 &&
              e.chest_placement->neighbor_count == (orientation < 4 ? 1 : 0) &&
              chest_placement_queries(e.chest_placement, orientation < 4 ? 12u : 8u));
        /* These counts/sites are linked to the verified bytecode, not an
         * independently observed original JVM query trace. Distinct retained
         * records must survive a refused overwrite of the pending call. */
        const mc_world_chest_placement_detail held = *e.chest_placement;
        const mc_world_chest_placement_detail *identity = e.chest_placement;
        const mc_world_runtime_acquisition_error *last = e.acquisition;
        bool untouched = true;
        CHECK(mc_world_block_can_place_base(f->registry[0], dirt, NULL, 0, 0, 0, &untouched, &e) ==
                  MC_WORLD_STORAGE_BUSY &&
              untouched && e.chest_placement == identity &&
              memcmp(&held, identity, sizeof(held)) == 0 && e.acquisition == last &&
              mc_world_placement_error_retire(&e) && chest_placement_same(p, before));
    }
    chest_placement_air(p);
    CHECK(chest_placement_put(p, 8, 64, 8, 3) && chest_placement_observe(p, before));
    bool value = false;
    CHECK(mc_world_block_can_place_base(f->registry[0], chest, world, 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value && mc_world_placement_error_retire(&e) && chest_placement_same(p, before));
    value = true;
    CHECK(mc_world_runtime_can_place(world, 54, 8, 64, 8, false, 1, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          !value && e.returned && e.chest_placement == NULL &&
          mc_world_placement_error_retire(&e) && chest_placement_same(p, before));
    chest_placement_air(p);
    CHECK(chest_placement_observe(p, before));
    value = false;
    CHECK(mc_world_block_can_place_base(f->registry[0], dirt, world, 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value && e.chest_placement == NULL && e.query_count == 1 &&
          mc_world_placement_error_retire(&e) && chest_placement_same(p, before));
    /* A real nullable provider field causes a reached failure. Restore the
     * authored source operand before any assertion/cleanup can leave scope. */
    mc_chunk_provider *provider = world->fields.v;
    world->fields.v = NULL;
    value = true;
    const mc_world_storage_status failed =
        mc_world_block_can_place_base(f->registry[0], chest, world, 8, 64, 8, &value, &e);
    world->fields.v = provider;
    CHECK(failed != MC_WORLD_STORAGE_OK && value && !e.returned && e.query_count == 0 &&
          chest_placement_queries(e.chest_placement, 1) &&
          e.acquisition == e.chest_placement->queries[0] &&
          e.chest_placement->query_sites[0].pc == 10 && !e.chest_placement->returned &&
          e.chest_placement->helper_count == 0 && mc_world_placement_error_retire(&e) &&
          chest_placement_same(p, before));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    free(before);
    return ok;
}

static bool chest_placement_failed_helper(const char *root, size_t orientation) {
    bool ok = false;
    chest_placement_fixture p = {0};
    mc_world_placement_error e = {0};
    CHECK(chest_placement_open(&p, root, 1));
    fixture *f = p.base;
    const mc_block *chest = NULL;
    CHECK(mc_registry_block_get(f->registry[0], 54, &chest, NULL) == MC_REGISTRY_OK &&
          chest != NULL);
    const int32_t xs[] = {1, 14, 8, 8}, zs[] = {8, 8, 1, 14};
    const int32_t nx[] = {0, 15, 8, 8}, nz[] = {8, 8, 0, 15};
    const unsigned int calls[] = {99, 115, 131, 147}, pcs[] = {24, 43, 62, 81};
    const size_t reached[] = {6, 8, 10, 12};
    chest_placement_air(&p);
    CHECK(chest_placement_put(&p, nx[orientation], 64, nz[orientation], 54));
    bool value = true;
    const mc_world_storage_status status = mc_world_block_can_place_base(
        f->registry[0], chest, f->worlds[0], xs[orientation], 64, zs[orientation], &value, &e);
    CHECK(status != MC_WORLD_STORAGE_OK && value && !e.returned && e.query_count == 0 &&
          chest_placement_queries(e.chest_placement, reached[orientation]) &&
          e.chest_placement->neighbor_count == 1 && !e.chest_placement->returned &&
          e.chest_placement->helper_count == orientation + 1 && e.site.class_name != NULL &&
          strcmp(e.site.class_name, "e") == 0 && e.site.method != NULL &&
          strcmp(e.site.method, "h") == 0 && e.site.pc == pcs[orientation] &&
          e.base.pc == calls[orientation] && e.chest_placement->caller.pc == calls[orientation]);
    for (size_t i = 0; i + 1 < e.chest_placement->query_count; ++i)
        CHECK(e.chest_placement->queries[i]->status == MC_CHUNK_ACQUIRE_OK);
    const mc_world_chest_placement_detail held = *e.chest_placement;
    bool untouched = false;
    CHECK(mc_world_block_can_place_base(f->registry[0], chest, NULL, 0, 0, 0, &untouched, &e) ==
              MC_WORLD_STORAGE_BUSY &&
          !untouched && memcmp(&held, e.chest_placement, sizeof(held)) == 0 &&
          mc_world_placement_error_retire(&e));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    if (!chest_placement_close(&p))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    chest_placement_fixture p = {0};
    char root[8192], child[8192];
    if (argc != 3 || strcmp(argv[1], "world-chest-placement") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    CHECK(chest_placement_open(&p, root, 4) && chest_placement_reference(&p) &&
          chest_placement_native(&p) && chest_placement_close(&p));
    for (size_t i = 0; i < 4; ++i) {
        const int written = snprintf(child, sizeof(child), "%s/failure%zu", root, i);
        CHECK(written > 0 && (size_t)written < sizeof(child) &&
              chest_placement_failed_helper(child, i));
    }
    result = 0;
cleanup:
    if (!chest_placement_close(&p))
        result = 1;
    if (result == 0)
        printf("PASS: %u World Chest placement predicate/source-order/ownership checks\n", checks);
    return result;
}
