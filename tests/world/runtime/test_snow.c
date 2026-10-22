/* Constructed native snow contracts. The included transport/ownership fixture
 * supplies genuine Worlds, stores and terminal disposal. Its fluid main is not
 * run. All sixteen independent input Chunks are serialized before loading. */
#define main mc_fluids_reference_main
#include "test_fluids.c"
#undef main

#include "../../../src/world/chunk/object_state_internal.h"

static input_cell snow_input(int32_t x, int32_t y, int32_t z) {
    if (z == 4 && y == 10) {
        switch (x) {
            case 5:
                return (input_cell){1, 0};
            case 6:
                return (input_cell){18, 0};
            case 7:
                return (input_cell){44, 0};
            case 8:
                return (input_cell){43, 0};
            case 9:
                return (input_cell){20, 0};
            case 10:
                return (input_cell){9, 0};
            case 11:
                return (input_cell){51, 0};
            default:
                break;
        }
    }
    if (z == 9) {
        if (x == 9 && (y == 23 || y == 71))
            return (input_cell){1, 0};
        if ((x == 9 && (y == 24 || y == 56 || y == 72 || y == 88)) ||
            (x == 22 && (y == 24 || y == 56 || y == 72 || y == 88 || y == 104)))
            return (input_cell){78, (uint8_t)((uint32_t)y & 7u)};
        if (x == 9 && y >= 40 && y <= 42)
            return (input_cell){78, (uint8_t)(y - 37)};
        if (x == 22 && (y == 40 || y == 41))
            return (input_cell){78, 7};
        if (x == 21 && y == 41)
            return (input_cell){66, 0};
    }
    if (x == 9 && y == 0 && z == 22)
        return (input_cell){78, 6};
    return (input_cell){0, 0};
}

static bool snow_seed(fixture *f, size_t slot, int32_t cx, int32_t cz) {
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
    bool prepared_ok = true;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t y = 0; y < 128; ++y) {
                const int32_t wx = cx * 16 + x, wz = cz * 16 + z;
                const input_cell v = snow_input(wx, y, wz);
                r->arrays[0][cell(x, y, z)] = v.id;
                if (!mc_chunk_nibbles_set(r->arrays[1], MC_CHUNK_NIBBLE_BYTES, x, y, z, v.metadata))
                    prepared_ok = false;
                /* Sky15 with Block11 must not melt; Sky0 with Block12 must.
                 * Input light is never installed by editing a live payload. */
                if (wz == 9 && v.id == 78) {
                    const int32_t light = y == 56 ? 11 : y == 72 || y == 88 || y == 104 ? 12 : 0;
                    const int32_t sky = y == 56 ? 15 : 0;
                    if (!mc_chunk_nibbles_set(r->arrays[2], MC_CHUNK_NIBBLE_BYTES, x, y, z, sky) ||
                        !mc_chunk_nibbles_set(r->arrays[3], MC_CHUNK_NIBBLE_BYTES, x, y, z, light))
                        prepared_ok = false;
                }
            }
    CHECK(prepared_ok);
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

static bool snow_load(fixture *f, size_t slot) {
    bool ok = false;
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state *state = NULL;
            const mc_chunk_acquire_status status =
                mc_world_runtime_get(f->worlds[slot], x, z, &state);
            CHECK(remember(f, slot));
            CHECK(status == MC_CHUNK_ACQUIRE_OK && state != NULL && state->active &&
                  state->terrain_populated && state->x == x && state->z == z);
            f->chunks[slot][x][z] = state;
        }
    CHECK(f->providers[slot].cache->count == 4 && f->providers[slot].cache->insertion_count == 4 &&
          mc_world_store_chunk_count(f->stores[slot]) == 4);
    ok = true;
cleanup:
    return ok;
}

/* Each internal body receives a genuine held World. The generic virtual
 * neighbor/tick dispatch remains part of the tested path. */
static bool snow_support_call(mc_world_runtime *w, const mc_block *snow, int32_t x, int32_t y,
                              int32_t z, bool *out, mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(w, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed = mc_world_mutation_snow_can_place(w, snow, x, y, z, out, error);
    mc_world_runtime_call_leave(w);
    return completed;
}

static bool snow_neighbor_call(mc_world_runtime *w, const mc_block *snow, int32_t x, int32_t y,
                               int32_t z, int32_t source, mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(w, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed = mc_world_mutation_block_neighbor(w, snow, x, y, z, source, error);
    mc_world_runtime_call_leave(w);
    return completed;
}

static bool snow_tick_call(mc_world_runtime *w, const mc_block *snow, int32_t x, int32_t y,
                           int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(w, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool completed = mc_world_mutation_block_tick(w, snow, x, y, z, random, error);
    mc_world_runtime_call_leave(w);
    return completed;
}

static bool snow_clear(const mc_world_mutation_error *e) {
    return e->status == MC_WORLD_STORAGE_OK && e->hook_class == NULL && e->hook_method == NULL &&
           e->hook_bytecode == 0 && e->snow_neighbor_bytecode == 0 && e->snow_bytecode == 0 &&
           e->snow_support_bytecode == 0 && e->snow_tick_bytecode == 0;
}

static bool snow_supports(fixture *f, size_t slot, const mc_block *snow) {
    bool ok = false, out = true;
    mc_world_runtime *w = f->worlds[slot];
    mc_registry_classes *r = f->registry[slot % 2u];
    mc_world_mutation_error e = mc_world_mutation_empty_error();
    mc_block **const array = r->blocks.m;
    CHECK(array != NULL && array[1] != NULL && array[18] != NULL && array[43] != NULL &&
          array[44] != NULL);

    /* A real air result skips Block.m entirely. */
    r->blocks.m = NULL;
    const bool air = snow_support_call(w, snow, 4, 11, 4, &out, &e);
    r->blocks.m = array;
    CHECK(air && !out && snow_clear(&e));
    out = false;
    CHECK(snow_support_call(w, snow, 5, 11, 4, &out, &e) && out && snow_clear(&e));
    out = true;
    CHECK(snow_support_call(w, snow, 7, 11, 4, &out, &e) && !out && snow_clear(&e));
    out = false;
    CHECK(snow_support_call(w, snow, 8, 11, 4, &out, &e) && out && snow_clear(&e));
    mc_block *slab = array[44];
    const bool original_slab = slab->subtype.ys.b, slab_o = r->blocks.o[44];
    slab->subtype.ys.b = true;
    r->blocks.o[44] = false;
    out = false;
    const bool live_slab = snow_support_call(w, snow, 7, 11, 4, &out, &e);
    slab->subtype.ys.b = original_slab;
    r->blocks.o[44] = slab_o;
    CHECK(live_slab && out && snow_clear(&e));
    for (int32_t x = 9; x <= 11; ++x) {
        out = true;
        CHECK(snow_support_call(w, snow, x, 11, 4, &out, &e) && !out && snow_clear(&e));
    }

    /* Leaves' virtual body reads live nr.b. The constructor's cached o value
     * and nullable Material cannot substitute for that first call. */
    mc_block *leaves = array[18];
    const bool original_leaf = leaves->nr_b, original_o = r->blocks.o[18];
    mc_material *const leaf_material = leaves->fields.bA;
    leaves->nr_b = true;
    r->blocks.o[18] = true;
    leaves->fields.bA = NULL;
    out = true;
    const bool transparent = snow_support_call(w, snow, 6, 11, 4, &out, &e);
    leaves->nr_b = original_leaf;
    r->blocks.o[18] = original_o;
    leaves->fields.bA = leaf_material;
    CHECK(transparent && !out && snow_clear(&e));
    leaves->nr_b = false;
    r->blocks.o[18] = false;
    out = false;
    const bool opaque = snow_support_call(w, snow, 6, 11, 4, &out, &e);
    leaves->nr_b = original_leaf;
    r->blocks.o[18] = original_o;
    CHECK(opaque && out && snow_clear(&e));

    /* An opaque Block with liquid Material must fail the second predicate.
     * Keep the exact live field and restore it before inspecting the result. */
    mc_block *stone = array[1];
    mc_material *const material = stone->fields.bA;
    stone->fields.bA = array[9]->fields.bA;
    out = true;
    const bool liquid = snow_support_call(w, snow, 5, 11, 4, &out, &e);
    stone->fields.bA = material;
    CHECK(liquid && !out && snow_clear(&e));

    /* An independently reached World material query uses its own actual
     * class binding. The successful first opacity must not hide this failure. */
    mc_registry_classes *const binding = w->classes->registry;
    w->classes->registry = NULL;
    out = true;
    const bool independent = snow_support_call(w, snow, 5, 11, 4, &out, &e);
    w->classes->registry = binding;
    CHECK(!independent && out && e.snow_support_bytecode == 38 && e.hook_bytecode == 38 &&
          e.material.bytecode == 18 && e.material.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.material.registry.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          e.registry.status == e.material.registry.status &&
          same_cause(&e.cause, &e.material.cause));

    out = true;
    r->blocks.m = NULL;
    const bool no_array = snow_support_call(w, snow, 5, 11, 4, &out, &e);
    r->blocks.m = array;
    CHECK(!no_array && out && e.snow_support_bytecode == 22 &&
          e.registry.status == MC_REGISTRY_NULL_REFERENCE);
    r->blocks.m[1] = NULL;
    const bool no_block = snow_support_call(w, snow, 5, 11, 4, &out, &e);
    r->blocks.m[1] = stone;
    CHECK(!no_block && out && e.snow_support_bytecode == 23 &&
          e.registry.status == MC_REGISTRY_NULL_REFERENCE);
    stone->fields.bA = NULL;
    const bool no_material = snow_support_call(w, snow, 5, 11, 4, &out, &e);
    stone->fields.bA = material;
    CHECK(!no_material && out && e.snow_support_bytecode == 41 && e.hook_bytecode == 41 &&
          e.registry.status == MC_REGISTRY_NULL_REFERENCE && e.material.bytecode == 0 &&
          e.material.status == MC_WORLD_STORAGE_OK);

    mc_world_provider *const provider = w->fields.v;
    w->fields.v = NULL;
    const bool no_provider = snow_support_call(w, snow, 5, 11, 4, &out, &e);
    w->fields.v = provider;
    CHECK(!no_provider && out && e.snow_support_bytecode == 7 &&
          e.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e.registry.status == MC_REGISTRY_OK &&
          e.material.bytecode == 0 && same_cause(&w->last_error, &e.cause));
    CHECK(w->active_calls == 0 && w->local_leases == 0 && f->providers[slot].cache->count == 4);
    ok = true;
cleanup:
    return ok;
}

static bool snow_neighbors(fixture *f, size_t slot, const mc_block *snow) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_world_mutation_error e = mc_world_mutation_empty_error();
    CHECK(at(w, 9, 24, 9, 78, 0));
    CHECK(snow_neighbor_call(w, snow, 9, 24, 9, INT32_MIN, &e) && snow_clear(&e) &&
          at(w, 9, 24, 9, 78, 0));
    CHECK(snow_neighbor_call(w, snow, 9, 24, 9, INT32_MAX, &e) && snow_clear(&e) &&
          at(w, 9, 24, 9, 78, 0));

    /* Zero quantity does not dereference World.r or initialize Item/Entity.
     * B=false also prevents the drop's remote early return hiding that body. */
    mc_java_random *const random = w->fields.r;
    const bool remote = w->fields.B;
    w->fields.r = NULL;
    w->fields.B = false;
    const bool removed = snow_neighbor_call(w, snow, 22, 24, 9, -987654321, &e);
    w->fields.r = random;
    w->fields.B = remote;
    CHECK(removed && snow_clear(&e) && at(w, 22, 24, 9, 0, 0));
    CHECK(snow_neighbor_call(w, snow, 9, 40, 9, 0, &e) && snow_clear(&e) && at(w, 9, 40, 9, 0, 0) &&
          at(w, 9, 41, 9, 0, 0) && at(w, 9, 42, 9, 0, 0));

    /* Constructed null-Random failure after both snow writes. Their zero
     * quantities skip Random, but the unsupported rail's actual quantity1
     * reaches probability nextFloat32 before its notifying-air write171.
     * Restore the real field before inspecting the failure or prior effects.
     * This is not ordinary positive rail-drop/publication coverage. */
    CHECK(at(w, 21, 40, 9, 0, 0) && at(w, 21, 41, 9, 66, 0));
    w->fields.r = NULL;
    const bool rail_drop = snow_neighbor_call(w, snow, 22, 40, 9, 78, &e);
    w->fields.r = random;
    CHECK(!rail_drop && e.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.snow_neighbor_bytecode == 6 && e.snow_bytecode == 35 && e.snow_support_bytecode == 0 &&
          e.hook_bytecode == 6 && e.hook_class != NULL &&
          strcmp(e.hook_class, slot % 2u == 0 ? "jr" : "gj") == 0 && e.hook_method != NULL &&
          strcmp(e.hook_method, slot % 2u == 0 ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V") == 0 &&
          e.cause_hook_class != NULL &&
          strcmp(e.cause_hook_class, slot % 2u == 0 ? "pc" : "jj") == 0 &&
          e.cause_hook_method != NULL &&
          strcmp(e.cause_hook_method, slot % 2u == 0 ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V") == 0 &&
          e.cause_hook_bytecode == 162 && e.neighbor_bytecode == 41 &&
          e.neighbor_call_bytecode == 8 && e.operation != NULL &&
          strcmp(e.operation, "Block drop Random receiver is null") == 0 &&
          e.cause.status == e.status && e.cause.operation == e.operation &&
          e.cause.system_error == 0 && same_cause(&w->last_error, &e.cause));
    CHECK(e.drop_class != NULL && strcmp(e.drop_class, slot % 2u == 0 ? "uu" : "na") == 0 &&
          e.drop_method != NULL &&
          strcmp(e.drop_method, slot % 2u == 0 ? "a(Lfd;IIIIF)V" : "a(Ldj;IIIIF)V") == 0 &&
          e.drop_bytecode == 32 && e.drop_call_bytecode == 13 && e.drop_spawn_bytecode == 0 &&
          e.item == MC_ITEM_STACK_OK && e.entity.status == MC_ENTITY_OK &&
          e.entity.operation == NULL && e.entity.bytecode == 0 &&
          e.publication.status == MC_WORLD_STORAGE_OK && e.publication.operation == NULL &&
          e.registry.status == MC_REGISTRY_OK && e.registry.initializer_pc == -1 &&
          e.registry.block_id == -1);
    CHECK(e.rail_sites_selected ==
              (MC_WORLD_RAIL_SITE_NEIGHBOR | MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE) &&
          e.rail_neighbor.entered && e.rail_neighbor.declaring_class != NULL &&
          strcmp(e.rail_neighbor.declaring_class, slot % 2u == 0 ? "pc" : "jj") == 0 &&
          e.rail_neighbor.method != NULL &&
          strcmp(e.rail_neighbor.method, slot % 2u == 0 ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V") == 0 &&
          e.rail_neighbor.bytecode == 162 && e.rail_neighbor.x == 21 && e.rail_neighbor.y == 41 &&
          e.rail_neighbor.z == 9 && e.rail_lifecycle_cause.entered &&
          e.rail_lifecycle_cause.declaring_class == e.rail_neighbor.declaring_class &&
          e.rail_lifecycle_cause.method == e.rail_neighbor.method &&
          e.rail_lifecycle_cause.bytecode == 162 && e.rail_lifecycle_cause.x == 21 &&
          e.rail_lifecycle_cause.y == 41 && e.rail_lifecycle_cause.z == 9 &&
          e.rail_support.status == MC_WORLD_STORAGE_OK && e.rail_support.operation == NULL &&
          e.rail_access.nearby.query.status == MC_WORLD_STORAGE_OK &&
          e.rail_access.nearby.query.operation == NULL && at(w, 22, 40, 9, 0, 0) &&
          at(w, 22, 41, 9, 0, 0) && at(w, 21, 40, 9, 0, 0) && at(w, 21, 41, 9, 66, 0));

    mc_world_reference_list *const observers = w->fields.u;
    w->fields.u = NULL;
    const bool observed = snow_neighbor_call(w, snow, 9, 56, 9, 0, &e);
    w->fields.u = observers;
    CHECK(!observed && e.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.snow_neighbor_bytecode == 6 && e.snow_bytecode == 35 && e.observer_bytecode == 9 &&
          at(w, 9, 56, 9, 0, 0));

    /* At y0, support queries y-1 and returns air before provider use. The next
     * source operation is metadata23, so this genuine null provider fails
     * before the final drop26. No Chunk-state bytes are damaged to force it. */
    mc_world_provider *const provider = w->fields.v;
    w->fields.v = NULL;
    const bool metadata = snow_neighbor_call(w, snow, 9, 0, 22, 0, &e);
    w->fields.v = provider;
    CHECK(!metadata && e.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.snow_neighbor_bytecode == 6 && e.snow_bytecode == 23 && e.snow_support_bytecode == 0 &&
          e.drop_class == NULL && e.drop_call_bytecode == 0 && e.cause_hook_bytecode == 23 &&
          at(w, 9, 0, 22, 78, 6));

    w->fields.v = NULL;
    const bool support = snow_neighbor_call(w, snow, 9, 24, 9, 0, &e);
    w->fields.v = provider;
    CHECK(!support && e.snow_neighbor_bytecode == 6 && e.snow_bytecode == 6 &&
          e.snow_support_bytecode == 7 && e.hook_bytecode == 6 && e.cause_hook_class != NULL &&
          e.cause_hook_bytecode == 7 && e.cause_hook_method != NULL &&
          strcmp(e.cause_hook_method, slot % 2u == 0 ? "a(Lfd;III)Z" : "a(Ldj;III)Z") == 0 &&
          at(w, 9, 24, 9, 78, 0));
    /* Wrapped y-1 and the bounded getters return air/zero here. The actual
     * notifying setter returns ordinary false, which the hook discards. */
    CHECK(snow_neighbor_call(w, snow, INT32_MAX, INT32_MIN, 22, INT32_MIN, &e) && snow_clear(&e));
    CHECK(snow_neighbor_call(w, snow, 9, INT32_MIN, 22, INT32_MAX, &e) && snow_clear(&e));
    CHECK(w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool snow_ticks(fixture *f, size_t slot, const mc_block *snow) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_registry_classes *r = f->registry[slot % 2u];
    mc_world_mutation_error e = mc_world_mutation_empty_error();
    mc_java_random supplied;
    mc_java_random_seed(&supplied, -719);
    const mc_java_random original = supplied;
    CHECK(at(w, 22, 56, 9, 78, 0) && at(w, 22, 55, 9, 0, 0));
    mc_block **const array = r->blocks.m;
    r->blocks.m = NULL;
    const bool dark = snow_tick_call(w, snow, 22, 56, 9, &supplied, &e);
    r->blocks.m = array;
    CHECK(dark && snow_clear(&e) && at(w, 22, 56, 9, 78, 0));
    CHECK(snow_tick_call(w, snow, 9, 72, 9, NULL, &e) && snow_clear(&e) && at(w, 9, 72, 9, 0, 0) &&
          at(w, 9, 71, 9, 1, 0));

    mc_java_random *const random = w->fields.r;
    const bool remote = w->fields.B;
    w->fields.r = NULL;
    w->fields.B = false;
    const bool unused_random = snow_tick_call(w, snow, 22, 72, 9, NULL, &e);
    w->fields.r = random;
    w->fields.B = remote;
    CHECK(unused_random && snow_clear(&e) && at(w, 22, 72, 9, 0, 0));

    /* Remote suppresses the inherited drop, not this whole melting tick. */
    w->fields.B = true;
    const bool remote_removed = snow_tick_call(w, snow, 22, 104, 9, NULL, &e);
    w->fields.B = remote;
    CHECK(remote_removed && snow_clear(&e) && at(w, 22, 104, 9, 0, 0));

    mc_world_reference_list *const observers = w->fields.u;
    w->fields.u = NULL;
    const bool notified = snow_tick_call(w, snow, 9, 88, 9, &supplied, &e);
    w->fields.u = observers;
    CHECK(!notified && e.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.snow_tick_bytecode == 39 && e.observer_bytecode == 9 && at(w, 9, 88, 9, 0, 0));

    /* Fail at the actual saved BlockLight Nibble. Preserve the entire child
     * query frame, not only its top-level status or a fabricated light value. */
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[slot], f->chunks[slot][1][0]);
    CHECK(chunk != NULL && chunk->g != NULL);
    mc_world_chunk_nibble *const block_light = chunk->g;
    chunk->g = NULL;
    const bool query = snow_tick_call(w, snow, 22, 88, 9, &supplied, &e);
    chunk->g = block_light;
    CHECK(!query && e.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e.snow_tick_bytecode == 8 &&
          e.query.status == e.status && e.query.bytecode == 105 && e.query.chunk_bytecode == 34 &&
          e.query.provider_bytecode == 0 && e.query.acquisition == MC_CHUNK_ACQUIRE_OK &&
          e.query.registry.status == MC_REGISTRY_OK && !e.query.population.reached &&
          same_cause(&e.cause, &e.query.cause) && e.operation == e.query.operation &&
          at(w, 22, 88, 9, 78, 0));

    mc_world_provider *const provider = w->fields.v;
    w->fields.v = NULL;
    const bool presence = snow_tick_call(w, snow, 22, 88, 9, &supplied, &e);
    w->fields.v = provider;
    CHECK(!presence && e.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.snow_tick_bytecode == 8 && e.query.status == e.status && e.query.bytecode == 74 &&
          e.query.provider_bytecode == 6 && e.query.chunk_bytecode == 0 &&
          e.query.acquisition == MC_CHUNK_ACQUIRE_OK && e.query.registry.status == MC_REGISTRY_OK &&
          !e.query.population.reached && same_cause(&e.cause, &e.query.cause) &&
          at(w, 22, 88, 9, 78, 0));

    mc_world_runtime_classes *const classes = w->classes;
    w->classes = NULL;
    const bool type = snow_tick_call(w, snow, 22, 88, 9, &supplied, &e);
    w->classes = classes;
    CHECK(!type && e.status == MC_WORLD_STORAGE_INVALID_ARGUMENT && e.snow_tick_bytecode == 1 &&
          e.query.bytecode == 0 && e.query.status == MC_WORLD_STORAGE_OK &&
          e.drop_call_bytecode == 0 && at(w, 22, 88, 9, 78, 0));

    /* A nullable actual Block static is captured at tick1. The same NULL
     * compares equal to the live Block static in Chunk and still reads g. */
    const mc_world_light_type *const light = classes->light_statics[1];
    classes->light_statics[1] = NULL;
    const bool nullable = snow_tick_call(w, snow, 22, 88, 9, NULL, &e);
    classes->light_statics[1] = light;
    CHECK(nullable && snow_clear(&e) && at(w, 22, 88, 9, 0, 0));
    CHECK(mc_test_java_random_equal(&supplied, &original) && w->active_calls == 0 &&
          w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool snow_entities_equal(const mc_entity_class_snapshot *a,
                                const mc_entity_class_snapshot *b) {
    return a->source == b->source && a->entity_class == b->entity_class &&
           a->bounds_class == b->bounds_class && a->watcher_class == b->watcher_class &&
           a->next_id == b->next_id && a->bounds_pool_cursor == b->bounds_pool_cursor &&
           a->bounds_pool_identity == b->bounds_pool_identity &&
           a->watcher_types_identity == b->watcher_types_identity &&
           a->watcher_type_count == b->watcher_type_count && a->live_entities == b->live_entities &&
           a->allocation_bytes == b->allocation_bytes;
}

static bool snow_world(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_registry_classes *r = f->registry[slot % 2u];
    const mc_block *snow = NULL;
    const mc_java_random world_random = *w->fields.r, math = math_snapshot(f);
    const uint64_t uniquifier = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    const size_t profile_bytes = f->profile->allocated_bytes;
    const mc_registry_phase items = r->item_states[MC_ITEM_GM_EJ].phase;
    mc_entity_class_snapshot before = {0}, after = {0};
    schedule_state scheduled_before = {0};
    CHECK(!w->fields.B && !w->fields.o &&
          mc_registry_block_get(r, 78, &snow, NULL) == MC_REGISTRY_OK && snow != NULL &&
          snow->kind == MC_BLOCK_JR_GJ && scheduled(w, &scheduled_before) &&
          mc_entity_classes_get_snapshot(f->entities[slot % 2u], &before) == MC_ENTITY_OK);
    CHECK(snow_supports(f, slot, snow));
    CHECK(snow_neighbors(f, slot, snow));
    CHECK(snow_ticks(f, slot, snow));
    CHECK(mc_entity_classes_get_snapshot(f->entities[slot % 2u], &after) == MC_ENTITY_OK &&
          snow_entities_equal(&before, &after) && r->item_states[MC_ITEM_GM_EJ].phase == items &&
          mc_test_java_random_equal(w->fields.r, &world_random) && math_is(f, &math) &&
          atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == uniquifier &&
          f->profile->allocated_bytes == profile_bytes && delta(w, &scheduled_before, 0, 0) &&
          w->fields.b->count == 0 && w->active_calls == 0 && w->local_leases == 0 &&
          f->providers[slot].cache->count == 4 && f->providers[slot].cache->insertion_count == 4 &&
          mc_world_store_chunk_count(f->stores[slot]) == 4);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'n', 'o', 'w'};
    if (argc != 3 || strcmp(argv[1], "world-snow") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        CHECK(open_store(f, i, root));
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entities[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entities[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              f->sources[side].opens == 3 && f->sources[side].closes == 1 && !f->sources[side].bad);
    }
    for (size_t slot = 0; slot < COUNT(f->worlds); ++slot)
        CHECK(create_world(f, slot));
    for (size_t slot = 0; slot < COUNT(f->worlds); ++slot)
        for (int32_t x = 0; x < 2; ++x)
            for (int32_t z = 0; z < 2; ++z)
                CHECK(snow_seed(f, slot, x, z));
    for (size_t slot = 0; slot < COUNT(f->worlds); ++slot)
        CHECK(snow_load(f, slot));
    double warmup;
    CHECK(mc_java_math_random(f->profile, &warmup) == MC_JAVA_RANDOM_PROFILE_OK &&
          atomic_load_explicit(&f->profile->holder_phase, memory_order_acquire) ==
              MC_JAVA_RANDOM_HOLDER_READY &&
          f->profile->math != NULL);
    for (size_t slot = 0; slot < COUNT(f->worlds); ++slot)
        CHECK(snow_world(f, slot));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("snow cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World snow checks (world-snow)\n", checks) < 0)
        result = 1;
    return result;
}
