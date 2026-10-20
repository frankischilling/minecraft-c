/* The genuine Tile fixture supplies construction, byte transport and checked
 * joint teardown. Its original main is compiled but never executed here. */
#define main mc_dungeon_tile_fixture_main
#include "../tiles/test_tiles.c"
#undef main

#include "mc/item/registry/runtime.h"
#include "mc/world/runtime/population/dungeon.h"
#include "mc/world/runtime/saving/saving.h"

/* Native source-derived contracts, not original/native parity. The rectangular
 * fixture has a full solid floor and roof. Positive seeds independently select
 * two prescribed first-trial wall sites; no second dungeon geometry algorithm
 * or successful World/backend substitute supplies expected terrain. */
enum { DG_X = 8, DG_Y = 64, DG_Z = 8, DG_SEARCH = 1000000 };
typedef struct dg_pick {
    int32_t selector, count, damage, variant;
    bool present;
    unsigned int allocation;
    mc_java_random before_new, after;
} dg_pick;
typedef struct dg_event {
    dg_pick pick;
    size_t group, fill;
    int32_t slot;
} dg_event;
typedef struct dg_plan {
    int64_t seed;
    int32_t rx, rz, mob, floor[9][9];
    mc_java_random prefix, before_fill[2], before_mob, after;
    dg_pick slots[2][27];
    dg_event events[16];
    size_t quota_count, quota_event;
} dg_plan;
typedef struct dg_image {
    uint8_t blocks[MC_CHUNK_BLOCKS];
    mc_chunk_data data;
    bool dirty, entities;
} dg_image;

static bool dg_rng_equal(const mc_java_random *a, const mc_java_random *b)
{
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           memcmp(&a->next_next_gaussian, &b->next_next_gaussian, sizeof(a->next_next_gaussian)) ==
               0;
}

static void dg_start(mc_java_random *random, int64_t seed)
{
    mc_java_random_seed(random, seed);
    (void)mc_java_random_next_gaussian(random); /* Genuine valid cached Gaussian. */
}

/* This table is the finite private-helper contract from er/da bytecode, not
 * values learned from the production helper or an original capture. */
static bool dg_pick_contract(mc_java_random *r, dg_pick *p)
{
    static const unsigned int allocations[] = {11, 27, 50, 66, 89, 112, 136, 162, 187, 220, 250};
    memset(p, 0, sizeof(*p));
    if (!mc_java_random_next_bounded(r, 11, &p->selector))
        return false;
    p->count = 1;
    p->damage = p->selector == 10 ? 3 : 0;
    p->present = true;
    p->allocation = allocations[(size_t)p->selector];
    const int32_t gate_bound = p->selector == 7   ? 100
                               : p->selector == 8 ? 2
                               : p->selector == 9 ? 10
                                                  : 0;
    if (gate_bound != 0) {
        int32_t gate;
        if (!mc_java_random_next_bounded(r, gate_bound, &gate))
            return false;
        p->present = gate == 0;
    }
    p->before_new = *r;
    if (p->present && (p->selector == 1 || p->selector == 3 || p->selector == 4 ||
                       p->selector == 5 || p->selector == 8)) {
        if (!mc_java_random_next_bounded(r, 4, &p->count))
            return false;
        ++p->count;
    }
    if (p->present && p->selector == 9 && !mc_java_random_next_bounded(r, 2, &p->variant))
        return false;
    p->after = *r;
    return true;
}

static bool dg_find_pick(int32_t selector, bool present, int32_t count, int32_t variant,
                         int64_t *seed, dg_pick *pick)
{
    for (int64_t candidate = 0; candidate < DG_SEARCH; ++candidate) {
        mc_java_random r;
        dg_start(&r, candidate);
        dg_pick p;
        if (!dg_pick_contract(&r, &p))
            return false;
        if (p.selector == selector && p.present == present && (count == 0 || p.count == count) &&
            (variant < 0 || p.variant == variant)) {
            *seed = candidate;
            *pick = p;
            return true;
        }
    }
    return false;
}

static bool dg_find_plan(int32_t rx, int32_t rz, bool chests, dg_plan *out)
{
    for (int64_t seed = 0; seed < DG_SEARCH; ++seed) {
        dg_plan p = {.seed = seed, .quota_event = SIZE_MAX};
        mc_java_random r;
        dg_start(&r, seed);
        int32_t a, b;
        if (!mc_java_random_next_bounded(&r, 2, &a) || !mc_java_random_next_bounded(&r, 2, &b))
            return false;
        if (a + 2 != rx || b + 2 != rz)
            continue;
        p.rx = rx;
        p.rz = rz;
        p.prefix = r;
        size_t moss = 0, floor_count = 0;
        for (int32_t x = 0; x < 2 * rx + 3; ++x)
            for (int32_t z = 0; z < 2 * rz + 3; ++z) {
                if (!mc_java_random_next_bounded(&r, 4, &p.floor[x][z]))
                    return false;
                moss += p.floor[x][z] != 0 ? 1u : 0u;
                ++floor_count;
            }
        if (moss == 0 || moss == floor_count || p.floor[0][0] == 0)
            continue;
        bool selected = true, occupied[2][27] = {{false}};
        size_t allocated = 0, repeat_count = 0;
        for (size_t group = 0; group < 2 && selected; ++group) {
            const size_t trials = chests ? 1 : 3;
            for (size_t trial = 0; trial < trials; ++trial) {
                if (!mc_java_random_next_bounded(&r, 2 * rx + 1, &a) ||
                    !mc_java_random_next_bounded(&r, 2 * rz + 1, &b))
                    return false;
                a -= rx;
                b -= rz;
                /* Strict interior sites have no solid side and all three
                 * trials fail. Positive sites have exactly the right/top wall,
                 * remain distant from one another and the left-side openings. */
                if (chests ? (a != (group == 0 ? rx : 0) || b != (group == 0 ? 0 : rz))
                           : (a <= -rx || a >= rx || b <= -rz || b >= rz)) {
                    selected = false;
                    break;
                }
            }
            if (!selected || !chests)
                continue;
            p.before_fill[group] = r;
            for (size_t fill = 0; fill < 8; ++fill) {
                const size_t event = group * 8 + fill;
                dg_event *e = &p.events[event];
                e->group = group;
                e->fill = fill;
                if (!dg_pick_contract(&r, &e->pick))
                    return false;
                if (!e->pick.present)
                    continue;
                if (repeat_count != 0 && p.quota_event == SIZE_MAX) {
                    p.quota_count = repeat_count;
                    p.quota_event = event;
                }
                if (!mc_java_random_next_bounded(&r, 27, &e->slot))
                    return false;
                ++allocated;
                if (occupied[group][(size_t)e->slot] && repeat_count == 0)
                    repeat_count = allocated;
                occupied[group][(size_t)e->slot] = true;
                p.slots[group][(size_t)e->slot] = e->pick;
            }
        }
        if (!selected || (chests && p.quota_event == SIZE_MAX))
            continue;
        p.before_mob = r;
        if (!mc_java_random_next_bounded(&r, 4, &p.mob))
            return false;
        p.after = r;
        *out = p;
        return true;
    }
    return false;
}

static mc_block **dg_block_cell(mc_registry_classes *r, const char *name)
{
    for (size_t i = 0; i < COUNT(r->blocks.named); ++i)
        if (strcmp(mc_registry_block_static_name(i), name) == 0)
            return &r->blocks.named[i];
    return NULL;
}

static mc_item **dg_item_cell(mc_registry_classes *r, const char *name)
{
    for (size_t i = 0; i < COUNT(r->items.named); ++i)
        if (strcmp(mc_registry_item_static_name(i), name) == 0)
            return &r->items.named[i];
    return NULL;
}

static bool dg_fields(fixture *f, size_t side, const dg_pick *p, mc_item_stack_fields *out)
{
    static const char *const names[] = {"ay", "m",  "S",  "R",  "K", "I",
                                        "au", "ar", "aA", "bd", "aU"};
    const mc_item *item = NULL;
    if (mc_registry_item_static_get(f->registry[side], names[(size_t)p->selector], &item, NULL) !=
            MC_REGISTRY_OK ||
        item == NULL)
        return false;
    int32_t id = mc_item_fields_get(item)->bf;
    if (p->selector == 9) {
        const mc_registry_item_array *array = NULL;
        if (mc_registry_item_c_get(f->registry[side], &array, NULL) != MC_REGISTRY_OK ||
            mc_registry_item_c_at(f->registry[side], array, mc_java_i32_add(id, p->variant), &item,
                                  NULL) != MC_REGISTRY_OK ||
            item == NULL)
            return false;
        id = mc_item_fields_get(item)->bf;
    }
    *out = (mc_item_stack_fields){.id = id, .count = p->count, .damage = p->damage};
    return true;
}

static bool dg_open(fixture *f, size_t slot, const char *root)
{
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'D', 'u', 'n', 'g', 'e', 'o', 'n'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 1;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {
        .seed = 23, .spawn_y = 64, .level_name = {title, COUNT(title)}, .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool dg_setup(fixture **out, const char *root)
{
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    *out = f; /* Partial real ownership remains available to checked cleanup. */
    CHECK(f != NULL);
    const uint16_t title[] = {'D', 'u', 'n', 'g', 'e', 'o', 'n'};
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
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_registry_classes_initialize_items(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
        mc_block **chest = dg_block_cell(f->registry[side], "av");
        mc_block **spawner = dg_block_cell(f->registry[side], "at");
        CHECK(chest != NULL && *chest != NULL && spawner != NULL && *spawner != NULL);
        f->chest_id[side] = mc_block_fields_get(*chest)->bn;
        f->spawner_id[side] = mc_block_fields_get(*spawner)->bn;
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(dg_open(f, slot, root) && create_world(f, slot));
    ok = true;
cleanup:
    return ok;
}

/* Seed a real encoded known-empty source chunk. Source bytes are prepared
 * before actual GAME decoding/normalization/activation, never as a fake cache. */
static bool dg_room(fixture *f, size_t slot, const dg_plan *p, size_t openings, int reject)
{
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    uint8_t *encoded = NULL;
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    mc_block **stone = dg_block_cell(f->registry[slot % 2], "u");
    CHECK(r != NULL && stone != NULL && *stone != NULL);
    const int32_t stone_id = mc_block_fields_get(*stone)->bn;
    CHECK(stone_id > 0 && stone_id < 256);
    memset(r->arrays[0], (unsigned char)stone_id, MC_CHUNK_BLOCKS);
    memset(r->arrays[4], 128, MC_CHUNK_HEIGHTMAP_BYTES);
    for (size_t i = 0; i < openings; ++i) {
        const int32_t x = i < (size_t)(2 * p->rz + 1) ? DG_X - p->rx - 1 : DG_X;
        const int32_t z =
            i < (size_t)(2 * p->rz + 1) ? DG_Z - p->rz + (int32_t)i : DG_Z - p->rz - 1;
        r->arrays[0][cell(x, DG_Y, z)] = 0;
        r->arrays[0][cell(x, DG_Y + 1, z)] = 0;
    }
    if (reject == 1 || reject == 2)
        r->arrays[0][cell(DG_X - p->rx - 1, reject == 1 ? DG_Y - 1 : DG_Y + 4, DG_Z - p->rz - 1)] =
            0;
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
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    size_t length = 0, written = 0;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = UINT32_MAX;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), 0, 0, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_get(f->worlds[slot], 0, 0, &f->states[slot]) == MC_CHUNK_ACQUIRE_OK);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], f->states[slot]);
    CHECK(f->chunks[slot] != NULL && f->states[slot]->active && f->chunks[slot]->l->count == 0 &&
          mc_live_tiles_map_valid(f->chunks[slot]));
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static void dg_snapshot(const mc_chunk_state *s, dg_image *out)
{
    memcpy(out->blocks, s->data->blocks, sizeof(out->blocks));
    out->data = *s->data;
    out->dirty = s->dirty;
    out->entities = s->has_entities;
}

static bool dg_unchanged(const mc_chunk_state *s, const dg_image *before)
{
    return memcmp(s->data->blocks, before->blocks, sizeof(before->blocks)) == 0 &&
           memcmp(s->data->metadata, before->data.metadata, sizeof(s->data->metadata)) == 0 &&
           memcmp(s->data->skylight, before->data.skylight, sizeof(s->data->skylight)) == 0 &&
           memcmp(s->data->block_light, before->data.block_light, sizeof(s->data->block_light)) ==
               0 &&
           memcmp(s->data->heightmap, before->data.heightmap, sizeof(s->data->heightmap)) == 0 &&
           s->dirty == before->dirty && s->has_entities == before->entities;
}

static bool dg_inventory(fixture *f, size_t slot, const dg_plan *p, bool reloaded)
{
    bool ok = false;
    mc_tile *tile = NULL;
    mc_item_stack *stack = NULL;
    mc_tile_name *name = NULL;
    static const char *const mobs[] = {"Skeleton", "Zombie", "Zombie", "Spider"};
    for (size_t group = 0; group < 2; ++group) {
        const int32_t x = group == 0 ? DG_X + p->rx : DG_X;
        const int32_t z = group == 0 ? DG_Z : DG_Z + p->rz;
        CHECK(mc_live_tiles_get(f->worlds[slot], x, DG_Y, z, &tile, NULL) == MC_WORLD_STORAGE_OK &&
              tile != NULL);
        mc_tile_snapshot snapshot;
        CHECK(mc_tile_get_snapshot(tile, &snapshot) == MC_TILE_OK &&
              snapshot.kind == MC_TILE_CHEST && snapshot.world == f->worlds[slot] &&
              snapshot.x == x && snapshot.y == DG_Y && snapshot.z == z && !snapshot.invalid &&
              snapshot.chest_capacity == (reloaded ? 27u : 36u));
        int32_t size = 0;
        CHECK(mc_tile_chest_size(tile, &size) == MC_TILE_OK && size == 27);
        for (int32_t index = 0; index < 27; ++index) {
            CHECK(mc_tile_chest_get(tile, index, &stack) == MC_TILE_OK);
            const dg_pick *expected = &p->slots[group][(size_t)index];
            if (!expected->present)
                CHECK(stack == NULL);
            else {
                mc_item_stack_fields actual, fields;
                CHECK(stack != NULL && dg_fields(f, slot % 2, expected, &fields) &&
                      mc_item_stack_fields_get(stack, &actual) == MC_ITEM_STACK_OK &&
                      actual.id == fields.id && actual.count == fields.count &&
                      actual.damage == fields.damage && actual.animation == 0 &&
                      (reloaded || mc_item_stack_source(stack) == NULL));
            }
            mc_item_stack_release(stack);
            stack = NULL;
        }
        mc_tile_release(tile);
        tile = NULL;
    }
    CHECK(mc_live_tiles_get(f->worlds[slot], DG_X, DG_Y, DG_Z, &tile, NULL) ==
              MC_WORLD_STORAGE_OK &&
          tile != NULL && mc_tile_spawner_get_name(tile, &name) == MC_TILE_OK &&
          name_is(name, mobs[(size_t)p->mob]));
    mc_tile_snapshot snapshot;
    CHECK(mc_tile_get_snapshot(tile, &snapshot) == MC_TILE_OK && snapshot.kind == MC_TILE_SPAWNER &&
          snapshot.spawner_delay == 20 && snapshot.world == f->worlds[slot] && !snapshot.invalid &&
          f->chunks[slot]->l->count == 3 && mc_live_tiles_map_valid(f->chunks[slot]) &&
          mc_live_tiles_lists_valid(f->worlds[slot]));
    ok = true;
cleanup:
    mc_tile_name_release(name);
    mc_item_stack_release(stack);
    mc_tile_release(tile);
    return ok;
}

static bool dg_roundtrip(fixture *f, size_t slot, const dg_plan *p)
{
    bool ok = false;
    mc_world_runtime *world = f->worlds[slot];
    mc_tile *old = NULL, *fresh = NULL;
    uint32_t growth = UINT32_MAX;
    mc_world_game_save_error saved;
    CHECK(mc_live_tiles_get(world, DG_X, DG_Y, DG_Z, &old, NULL) == MC_WORLD_STORAGE_OK &&
          old != NULL &&
          mc_world_runtime_write_game_chunk(world, f->states[slot], fixture_timestamp, &growth,
                                            &saved) == MC_WORLD_STORAGE_OK &&
          growth != UINT32_MAX && saved.status == MC_WORLD_STORAGE_OK);
    mc_chunk_cache *cache = f->providers[slot].cache;
    CHECK(cache->count == 1);
    bool added = false, more = true;
    CHECK(mc_chunk_pending_add(f->providers[slot].pending, cache->entries[0].key, &added) ==
              MC_CHUNK_ACQUIRE_OK &&
          added && mc_world_runtime_maintain(world, &more) == MC_CHUNK_LIFECYCLE_OK && !more &&
          cache->count == 0 && cache->insertion_count == 0 && !f->states[slot]->active);
    CHECK(mc_world_store_retire_chunk(f->stores[slot], f->states[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->states[slot] = NULL;
    f->chunks[slot] = NULL;
    CHECK(mc_world_runtime_get(world, 0, 0, &f->states[slot]) == MC_CHUNK_ACQUIRE_OK);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], f->states[slot]);
    CHECK(f->chunks[slot] != NULL && dg_inventory(f, slot, p, true) &&
          mc_live_tiles_get(world, DG_X, DG_Y, DG_Z, &fresh, NULL) == MC_WORLD_STORAGE_OK &&
          fresh != NULL && fresh != old);
    mc_tile_snapshot old_snapshot;
    CHECK(mc_tile_get_snapshot(old, &old_snapshot) == MC_TILE_OK && old_snapshot.invalid);
    ok = true;
cleanup:
    mc_tile_release(old);
    mc_tile_release(fresh);
    return ok;
}

static bool dg_whole(fixture *f, size_t side, const dg_plan *p, size_t openings, int reject,
                     bool roundtrip)
{
    bool ok = false, out = true;
    dg_image *before = malloc(sizeof(*before));
    mc_java_random random;
    mc_population_dungeon_error error;
    CHECK(before != NULL && dg_room(f, side, p, openings, reject));
    dg_snapshot(f->states[side], before);
    dg_start(&random, p->seed);
    CHECK(mc_population_dungeon_place(f->worlds[side], &random, NULL, DG_X, DG_Y, DG_Z, &out,
                                      &error) == MC_WORLD_STORAGE_OK &&
          error.status == MC_WORLD_STORAGE_OK && error.feature_class == NULL);
    if (reject != 0 || openings == 0 || openings == 6) {
        CHECK(!out && dg_rng_equal(&random, &p->prefix) && dg_unchanged(f->states[side], before) &&
              f->chunks[side]->l->count == 0);
    } else {
        CHECK(out && dg_rng_equal(&random, &p->after) && dg_inventory(f, side, p, false));
        const int32_t stone = mc_block_fields_get(*dg_block_cell(f->registry[side], "u"))->bn;
        const int32_t cobble = mc_block_fields_get(*dg_block_cell(f->registry[side], "x"))->bn;
        const int32_t moss = mc_block_fields_get(*dg_block_cell(f->registry[side], "ap"))->bn;
        for (int32_t x = 0; x < 2 * p->rx + 3; ++x)
            for (int32_t z = 0; z < 2 * p->rz + 3; ++z) {
                const int32_t xx = DG_X - p->rx - 1 + x, zz = DG_Z - p->rz - 1 + z;
                CHECK(f->states[side]->data->blocks[cell(xx, DG_Y - 1, zz)] ==
                          (uint8_t)(p->floor[x][z] == 0 ? cobble : moss) &&
                      f->states[side]->data->blocks[cell(xx, DG_Y + 4, zz)] == (uint8_t)stone);
            }
        if (roundtrip)
            CHECK(dg_roundtrip(f, side, p));
    }
    ok = true;
cleanup:
    free(before);
    return ok;
}

/* Helper contracts include every selector, all four variable counts, both
 * record choices, every probability miss and all four mob draws. */
static bool dg_helpers(fixture *f, size_t side)
{
    bool ok = false;
    mc_item_stack *stack = NULL;
    mc_tile_name *name = NULL;
    mc_population_dungeon_error error;
    for (int32_t selector = 0; selector < 11; ++selector) {
        const bool variable =
            selector == 1 || selector == 3 || selector == 4 || selector == 5 || selector == 8;
        for (int32_t count = 1; count <= (variable ? 4 : 1); ++count) {
            const int32_t variants = selector == 9 ? 2 : 1;
            for (int32_t variant = 0; variant < variants; ++variant) {
                int64_t seed;
                dg_pick p;
                CHECK(dg_find_pick(selector, true, count, variant, &seed, &p));
                mc_java_random r;
                dg_start(&r, seed);
                CHECK(mc_population_dungeon_loot(f->worlds[side], &r, NULL, &stack, &error) ==
                          MC_WORLD_STORAGE_OK &&
                      stack != NULL && dg_rng_equal(&r, &p.after));
                mc_item_stack_fields actual, expected;
                CHECK(dg_fields(f, side, &p, &expected) &&
                      mc_item_stack_fields_get(stack, &actual) == MC_ITEM_STACK_OK &&
                      actual.id == expected.id && actual.count == expected.count &&
                      actual.damage == expected.damage && actual.animation == 0 &&
                      mc_item_stack_source(stack) == NULL && error.status == MC_WORLD_STORAGE_OK);
                mc_item_stack_release(stack);
                stack = NULL;
            }
        }
    }
    const mc_population_dungeon_limits zero = {0};
    for (int32_t selector = 7; selector <= 9; ++selector) {
        int64_t seed;
        dg_pick p;
        CHECK(dg_find_pick(selector, false, 0, -1, &seed, &p));
        mc_java_random r;
        dg_start(&r, seed);
        CHECK(mc_population_dungeon_loot(f->worlds[side], &r, &zero, &stack, &error) ==
                  MC_WORLD_STORAGE_OK &&
              stack == NULL && dg_rng_equal(&r, &p.after));
    }
    static const char *const mobs[] = {"Skeleton", "Zombie", "Zombie", "Spider"};
    for (int32_t choice = 0; choice < 4; ++choice) {
        int64_t seed;
        mc_java_random expected;
        for (seed = 0; seed < DG_SEARCH; ++seed) {
            dg_start(&expected, seed);
            int32_t result;
            CHECK(mc_java_random_next_bounded(&expected, 4, &result));
            if (result == choice)
                break;
        }
        CHECK(seed < DG_SEARCH);
        mc_java_random r;
        dg_start(&r, seed);
        CHECK(mc_population_dungeon_mob(f->worlds[side], &r, &name, &error) ==
                  MC_WORLD_STORAGE_OK &&
              name != NULL && name_is(name, mobs[(size_t)choice]) && dg_rng_equal(&r, &expected));
        mc_tile_name *literal = NULL;
        const mc_world_storage_status status =
            mc_live_tiles_literal(f->worlds[side], mobs[(size_t)choice], &literal, NULL);
        const bool same = literal == name &&
                          mc_live_tiles_name_identity(literal) == mc_live_tiles_name_identity(name);
        mc_tile_name_release(literal);
        CHECK(status == MC_WORLD_STORAGE_OK && same);
        mc_tile_name_release(name);
        name = NULL;
    }
    ok = true;
cleanup:
    mc_item_stack_release(stack);
    mc_tile_name_release(name);
    return ok;
}

static bool dg_helper_failures(fixture *f, size_t side)
{
    bool ok = false;
    mc_item_stack *sentinel = NULL, *out = NULL;
    mc_population_dungeon_error error;
    int64_t seed;
    dg_pick p;
    CHECK(mc_item_stack_allocate(&sentinel) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(sentinel, 1, 1, 0) == MC_ITEM_STACK_OK &&
          dg_find_pick(1, true, 3, -1, &seed, &p));
    mc_item **m = dg_item_cell(f->registry[side], "m");
    mc_item **bd = dg_item_cell(f->registry[side], "bd");
    CHECK(m != NULL && *m != NULL && bd != NULL && *bd != NULL);
    mc_item *const saved_m = *m;
    mc_java_random r;
    dg_start(&r, seed);
    out = sentinel;
    *m = NULL;
    const mc_world_storage_status null_item =
        mc_population_dungeon_loot(f->worlds[side], &r, NULL, &out, &error);
    *m = saved_m; /* Restore input BEFORE any assertion, retain reached effects. */
    CHECK(null_item == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == sentinel &&
          error.loot_bytecode == 41 && error.allocation_bytecode == 27 &&
          error.stack_constructor_bytecode == 2 && error.registry.status == MC_REGISTRY_OK &&
          dg_rng_equal(&r, &p.after));
    for (size_t limit = 0; limit < 2; ++limit) {
        const mc_population_dungeon_limits bound = {limit == 0 ? 0
                                                               : mc_item_stack_object_bytes() - 1u};
        dg_start(&r, seed);
        *m = NULL;
        const mc_world_storage_status quota =
            mc_population_dungeon_loot(f->worlds[side], &r, &bound, &out, &error);
        *m = saved_m;
        CHECK(quota == MC_WORLD_STORAGE_LIMIT && out == sentinel && error.loot_bytecode == 27 &&
              error.allocation_bytecode == 27 && error.stack_constructor_bytecode == 0 &&
              error.registry.status == MC_REGISTRY_OK && dg_rng_equal(&r, &p.before_new));
    }
    CHECK(dg_find_pick(9, true, 1, 1, &seed, &p));
    mc_registry_item_array *const saved_c = f->registry[side]->items.c;
    mc_item *const saved_bd = *bd;
    dg_start(&r, seed);
    f->registry[side]->items.c = NULL;
    const mc_world_storage_status null_array =
        mc_population_dungeon_loot(f->worlds[side], &r, NULL, &out, &error);
    f->registry[side]->items.c = saved_c;
    CHECK(null_array == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == sentinel &&
          error.loot_bytecode == 239 && error.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          error.registry.initializer_pc == -1 && error.registry.constructor_pc == -1 &&
          dg_rng_equal(&r, &p.after));
    const mc_registry_error retained_registry = f->worlds[side]->last_registry_error;
    dg_start(&r, seed);
    f->registry[side]->items.c = NULL;
    *bd = NULL;
    const mc_world_storage_status null_bd =
        mc_population_dungeon_loot(f->worlds[side], &r, NULL, &out, &error);
    *bd = saved_bd;
    f->registry[side]->items.c = saved_c;
    CHECK(null_bd == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == sentinel &&
          error.loot_bytecode == 230 && error.registry.status == MC_REGISTRY_OK &&
          dg_rng_equal(&r, &p.before_new) &&
          f->worlds[side]->last_registry_error.status == retained_registry.status &&
          f->worlds[side]->last_registry_error.message == retained_registry.message &&
          f->worlds[side]->last_registry_error.initializer_pc == retained_registry.initializer_pc &&
          f->worlds[side]->last_registry_error.constructor_pc == retained_registry.constructor_pc);
    ok = true;
cleanup:
    if (out != sentinel)
        mc_item_stack_release(out);
    mc_item_stack_release(sentinel);
    return ok;
}

enum dg_failure {
    DG_NULL_RANDOM,
    DG_NULL_MOSS,
    DG_NULL_CHEST,
    DG_WRONG_CHEST,
    DG_NULL_SPAWNER,
    DG_WRONG_SPAWNER,
    DG_OBSERVER,
    DG_CUMULATIVE
};

static bool dg_failure_case(fixture *f, size_t side, const dg_plan *p, enum dg_failure which)
{
    bool ok = false, out = true;
    dg_image *before = malloc(sizeof(*before));
    mc_java_random r, expected;
    mc_tile *held_chest = NULL;
    mc_item_stack *held_stack = NULL;
    mc_population_dungeon_error error;
    CHECK(before != NULL && dg_room(f, side, p, 1, 0));
    dg_snapshot(f->states[side], before);
    dg_start(&r, p->seed);
    expected = p->prefix;
    mc_block **changed = NULL;
    mc_block *saved = NULL;
    mc_world_reference_list *const saved_u = f->worlds[side]->fields.u;
    mc_population_dungeon_limits bound;
    const mc_population_dungeon_limits *limits = NULL;
    if (which == DG_NULL_MOSS) {
        changed = dg_block_cell(f->registry[side], "ap");
        CHECK(changed != NULL);
        saved = *changed;
        *changed = NULL;
        int32_t draw;
        (void)mc_java_random_next_bounded(&expected, 4, &draw);
    } else if (which == DG_NULL_CHEST || which == DG_WRONG_CHEST) {
        changed = dg_block_cell(f->registry[side], "av");
        CHECK(changed != NULL);
        saved = *changed;
        *changed = *dg_block_cell(f->registry[side], which == DG_NULL_CHEST ? "x" : "at");
        expected = p->before_fill[0];
        if (which == DG_NULL_CHEST)
            for (size_t i = 0; i < 8; ++i)
                if (p->events[i].pick.present) {
                    expected = p->events[i].pick.after;
                    break;
                }
    } else if (which == DG_NULL_SPAWNER || which == DG_WRONG_SPAWNER) {
        changed = dg_block_cell(f->registry[side], "at");
        CHECK(changed != NULL);
        saved = *changed;
        *changed = *dg_block_cell(f->registry[side], which == DG_NULL_SPAWNER ? "x" : "av");
        expected = which == DG_NULL_SPAWNER ? p->after : p->before_mob;
    } else if (which == DG_OBSERVER) {
        f->worlds[side]->fields.u = NULL;
    } else if (which == DG_CUMULATIVE) {
        CHECK(p->quota_event < COUNT(p->events) &&
              p->quota_count <= SIZE_MAX / mc_item_stack_object_bytes());
        bound.max_stack_allocation_bytes = p->quota_count * mc_item_stack_object_bytes();
        limits = &bound;
        expected = p->events[p->quota_event].pick.before_new;
    }
    const mc_world_storage_status status =
        mc_population_dungeon_place(f->worlds[side], which == DG_NULL_RANDOM ? NULL : &r, limits,
                                    DG_X, DG_Y, DG_Z, &out, &error);
    if (changed != NULL)
        *changed = saved;
    f->worlds[side]->fields.u = saved_u;
    CHECK(status != MC_WORLD_STORAGE_OK && out && error.status == status &&
          error.cause.status == status);
    if (which == DG_NULL_RANDOM) {
        CHECK(error.bytecode == 5 && error.registry.status == MC_REGISTRY_OK &&
              dg_unchanged(f->states[side], before));
    } else {
        CHECK(dg_rng_equal(&r, &expected));
        if (which == DG_NULL_MOSS)
            CHECK(error.bytecode == 455 && error.registry.status == MC_REGISTRY_OK &&
                  !dg_unchanged(f->states[side], before));
        else if (which == DG_NULL_CHEST)
            CHECK(error.bytecode == 743 && f->chunks[side]->l->count == 0);
        else if (which == DG_WRONG_CHEST)
            CHECK(error.bytecode == 711 && f->chunks[side]->l->count == 1);
        else if (which == DG_NULL_SPAWNER)
            CHECK(error.bytecode == 812 && f->chunks[side]->l->count == 0);
        else if (which == DG_WRONG_SPAWNER)
            CHECK(error.bytecode == 800 && f->chunks[side]->l->count == 1);
        else if (which == DG_OBSERVER)
            CHECK(error.mutation.status != MC_WORLD_STORAGE_OK &&
                  error.mutation.observer_bytecode != 0 &&
                  error.registry.status == MC_REGISTRY_OK &&
                  !dg_unchanged(f->states[side], before));
        else if (which == DG_CUMULATIVE) {
            const dg_event *e = &p->events[p->quota_event];
            CHECK(status == MC_WORLD_STORAGE_LIMIT &&
                  error.allocation_bytecode == e->pick.allocation &&
                  error.chest_group == e->group && error.fill_index == e->fill &&
                  error.stack_constructor_bytecode == 0 &&
                  f->chunks[side]->l->count == e->group + 1u);
            dg_pick installed[2][27] = {0};
            for (size_t i = 0; i < p->quota_event; ++i) {
                const dg_event *prior = &p->events[i];
                if (prior->pick.present)
                    installed[prior->group][(size_t)prior->slot] = prior->pick;
            }
            for (size_t group = 0; group <= e->group; ++group) {
                CHECK(mc_live_tiles_get(f->worlds[side], group == 0 ? DG_X + p->rx : DG_X, DG_Y,
                                        group == 0 ? DG_Z : DG_Z + p->rz, &held_chest,
                                        NULL) == MC_WORLD_STORAGE_OK &&
                      held_chest != NULL);
                for (int32_t index = 0; index < 27; ++index) {
                    CHECK(mc_tile_chest_get(held_chest, index, &held_stack) == MC_TILE_OK);
                    const dg_pick *prior = &installed[group][(size_t)index];
                    if (!prior->present)
                        CHECK(held_stack == NULL);
                    else {
                        mc_item_stack_fields actual, fields;
                        CHECK(held_stack != NULL && dg_fields(f, side, prior, &fields) &&
                              mc_item_stack_fields_get(held_stack, &actual) == MC_ITEM_STACK_OK &&
                              actual.id == fields.id && actual.count == fields.count &&
                              actual.damage == fields.damage);
                    }
                    mc_item_stack_release(held_stack);
                    held_stack = NULL;
                }
                mc_tile_release(held_chest);
                held_chest = NULL;
            }
        }
    }
    CHECK(mc_live_tiles_map_valid(f->chunks[side]) && mc_live_tiles_lists_valid(f->worlds[side]));
    ok = true;
cleanup:
    mc_item_stack_release(held_stack);
    mc_tile_release(held_chest);
    free(before);
    return ok;
}

/* A declared helper-to-inventory API control: no artificial mid-generation
 * callback is installed just to change u after construction. */
static bool dg_dirty_prefix(fixture *f, size_t side)
{
    bool ok = false;
    mc_tile *chest = NULL;
    mc_item_stack *stack = NULL, *held = NULL;
    mc_population_dungeon_error helper;
    mc_live_tile_error dirty;
    mc_world_mutation_error mutation;
    bool changed = false;
    int64_t seed;
    dg_pick p;
    CHECK(dg_find_pick(1, true, 4, -1, &seed, &p) &&
          mc_world_runtime_block_set(f->worlds[side], DG_X, DG_Y, DG_Z, f->chest_id[side], &changed,
                                     &mutation) == MC_WORLD_STORAGE_OK &&
          changed &&
          mc_live_tiles_get(f->worlds[side], DG_X, DG_Y, DG_Z, &chest, NULL) ==
              MC_WORLD_STORAGE_OK &&
          chest != NULL);
    mc_java_random r;
    dg_start(&r, seed);
    CHECK(mc_population_dungeon_loot(f->worlds[side], &r, NULL, &stack, &helper) ==
              MC_WORLD_STORAGE_OK &&
          stack != NULL);
    mc_world_reference_list *const saved = f->worlds[side]->fields.u;
    f->worlds[side]->fields.u = NULL;
    const mc_tile_status status = mc_tile_chest_set_detailed(chest, 3, stack, &dirty);
    f->worlds[side]->fields.u = saved;
    CHECK(status == MC_TILE_WORLD_FAILED && dirty.inventory_bytecode == 31 &&
          dirty.status != MC_WORLD_STORAGE_OK && mc_tile_chest_get(chest, 3, &held) == MC_TILE_OK &&
          held == stack && dg_rng_equal(&r, &p.after));
    ok = true;
cleanup:
    mc_item_stack_release(held);
    mc_item_stack_release(stack);
    mc_tile_release(chest);
    return ok;
}

#ifndef MC_DUNGEON_TEST_MAIN
#define MC_DUNGEON_TEST_MAIN main
#endif
int MC_DUNGEON_TEST_MAIN(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char base[8192], root[8192];
    dg_plan positive[4], no_chests;
    if (argc != 3 || strcmp(argv[1], "world-dungeon") != 0 ||
        !decode_path(argv[2], base, sizeof(base)) || !mc_math_trig_init(&trig))
        return 1;
    for (int32_t rx = 2; rx <= 3; ++rx)
        for (int32_t rz = 2; rz <= 3; ++rz)
            CHECK(dg_find_plan(rx, rz, true, &positive[(size_t)((rx - 2) * 2 + rz - 2)]));
    CHECK(dg_find_plan(2, 2, false, &no_chests));
    for (unsigned int epoch = 0; epoch < 18; ++epoch) {
        const int n = snprintf(root, sizeof(root), "%s/epoch%u", base, epoch);
        CHECK(n > 0 && (size_t)n < sizeof(root) && dg_setup(&f, root));
        for (size_t side = 0; side < 2; ++side) {
            if (epoch < 4)
                CHECK(dg_whole(f, side, &positive[epoch], 1, 0, epoch == 0));
            else if (epoch == 4)
                CHECK(dg_whole(f, side, &positive[0], 5, 0, false));
            else if (epoch == 5 || epoch == 6)
                CHECK(dg_whole(f, side, &positive[0], epoch == 5 ? 0u : 6u, 0, false));
            else if (epoch == 7 || epoch == 8)
                CHECK(dg_whole(f, side, &positive[0], 1, epoch == 7 ? 1 : 2, false));
            else if (epoch < 17) {
                const enum dg_failure which = (enum dg_failure)(epoch - 9u);
                const dg_plan *p = which == DG_NULL_SPAWNER || which == DG_WRONG_SPAWNER
                                       ? &no_chests
                                       : &positive[0];
                CHECK(dg_failure_case(f, side, p, which));
            } else {
                CHECK(dg_helpers(f, side) && dg_helper_failures(f, side) &&
                      dg_room(f, side, &positive[0], 1, 0) && dg_dirty_prefix(f, side));
            }
        }
        if (!cleanup_fixture(f)) {
            fputs("Dungeon epoch cleanup refused; surviving real owners retained\n", stderr);
            return 1;
        }
        f = NULL;
    }
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Dungeon cleanup refused; surviving real owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u Dungeon native integration checks (world-dungeon)\n", checks) < 0)
        result = 1;
    return result;
}
