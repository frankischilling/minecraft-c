#include "mc/block/registry/runtime.h"
#include "mc/entity/minecart/minecart.h"
#include "mc/entity/movement/movement.h"
#include "mc/entity/nbt/nbt.h"
#include "mc/storage/region/region.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/collision/class_entities.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/scheduling.h"

#include "../../../../src/block/registry/registry_internal.h"
#include "../../../../src/entity/entity_internal.h"
#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../../src/world/runtime/runtime_internal.h"
#include "../../../../src/world/storage/objects_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        const bool passed = !!(expression);                                                        \
        if (!passed) {                                                                             \
            fprintf(stderr, "FAIL minecart.detector:%d: %s\n", __LINE__, #expression);             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Constructed native contracts, not original observations. Two ordinary GAME
 * Worlds each acquire four real stored normal Chunks. Positioned Minecarts,
 * canonical Classes, List occurrences, notifications and schedules are actual
 * production owners/calls. No test source is included and no gameplay callback,
 * Entity kind/phase/bounds/hold, query result or queue implementation is forged.
 * Temporary nullable source references are restored before every assertion. */
static unsigned int checks, cases;
static mc_math_trig trig;
static const uint32_t fixture_timestamp = UINT32_C(0xfedcba98);
static const int32_t chunk_coordinates[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
static const mc_entity_bounds detector_box = {8.125, 64.0, 8.125, 8.875, 64.25, 8.875};
static const mc_entity_bounds miss_box = {10.125, 64.0, 10.125, 10.875, 64.25, 10.875};
static const mc_entity_bounds moved_box = {12.125, 64.0, 12.125, 12.875, 64.25, 12.875};
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct resource_stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} resource_stream;

typedef struct resource_owner {
    resource_stream streams[3];
    mc_registry_resources api;
    size_t opens, closes;
    bool bad;
} resource_owner;

typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_world_regions *regions;
    mc_world_info_name *name;
    resource_owner resources[2];
    mc_registry_classes *registry[2];
    mc_entity_classes *entities[2];
    mc_world_runtime_classes *classes[2];
    mc_world_provider *dimensions[2];
    mc_world_store *stores[2];
    mc_world_runtime *worlds[2];
    mc_chunk_provider_fields providers[2];
    mc_chunk_state *states[2][4];
    mc_world_chunk *chunks[2][4];
    mc_entity *carts[2];
    size_t bindings_before_cart[2];
    bool bound[2], terminal[2], descriptors_retired[2], world_retired[2];
} fixture;

typedef struct chunk_record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} chunk_record;

static mc_java_properties_status resource_read(void *context, uint8_t *out, size_t capacity,
                                               size_t *count) {
    resource_stream *s = context;
    if (s == NULL || out == NULL || capacity == 0 || count == NULL || s->closed ||
        s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17u)
        take = 17u;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status resource_open(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resource_owner *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = (mc_java_properties_input){&r->streams[r->opens++], resource_read};
    return MC_REGISTRY_OK;
}

static mc_registry_status resource_close(void *context, const mc_java_properties_input *input) {
    resource_owner *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != resource_read) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    resource_owner *r = context;
    if (input == NULL || input->read != resource_read)
        r->bad = true;
    return false;
}

static mc_nbt_string ascii(const char *text, uint16_t *units) {
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, length};
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2u * i + j];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                              : 16u;
            if (digit > 15u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        out[i] = (char)(unsigned char)value;
    }
    out[length / 2u] = '\0';
    return true;
}

/* Small independent adaptation of the existing real publication fixture:
 * ordinary NBT codec -> actual region write -> actual Store/provider load. */
static bool seed_chunk(fixture *f, size_t side, int32_t x, int32_t z) {
    bool ok = false;
    chunk_record *r = calloc(1, sizeof(*r));
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
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[0].value.int_value = x;
    r->fields[1].value.int_value = z;
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3u].value.byte_array.data = r->arrays[i];
        r->fields[i + 3u].value.byte_array.length = lengths[i];
    }
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
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
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[side]), x, z, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool open_world(fixture *f, size_t side, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t block_map[256];
    const uint16_t title[] = {'M', 'i', 'n', 'e', 'c', 'a', 'r', 't'};
    const int length = snprintf(path, sizeof(path), "%s/world%zu", root, side);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    for (size_t i = 0; i < COUNT(block_map); ++i)
        block_map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = block_map,
                                      .block_map_length = sizeof(block_map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 4;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[side], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[side], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < COUNT(chunk_coordinates); ++i)
        CHECK(seed_chunk(f, side, chunk_coordinates[i][0], chunk_coordinates[i][1]));
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    limits.max_scheduled_entries = 8;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[side], NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(side == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                    : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[side], &limits, &f->worlds[side],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[side], .name = f->name, .seed = 23, .dimension = f->dimensions[side]};
    CHECK(mc_world_runtime_construct_world(f->worlds[side], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction state = {0};
    CHECK(mc_world_runtime_construction_get(f->worlds[side], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[side]->fields.v, &f->providers[side], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[side] = true;
    for (size_t i = 0; i < COUNT(chunk_coordinates); ++i) {
        mc_chunk_state *loaded = NULL;
        const mc_chunk_acquire_status status = mc_world_runtime_get(
            f->worlds[side], chunk_coordinates[i][0], chunk_coordinates[i][1], &loaded);
        /* Capture each genuine published owner even if later activation failed. */
        mc_chunk_cache *cache = f->providers[side].cache;
        CHECK(mc_chunk_cache_validate(cache));
        if (cache->insertion_count == i + 1u)
            f->states[side][i] = cache->insertions[i];
        CHECK(status == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded == f->states[side][i] &&
              loaded->active && loaded->terrain_populated && !loaded->dirty &&
              loaded->x == chunk_coordinates[i][0] && loaded->z == chunk_coordinates[i][1]);
        f->chunks[side][i] = mc_world_store_chunk_object(f->stores[side], loaded);
        mc_world_chunk_fields fields = {0};
        CHECK(f->chunks[side][i] != NULL &&
              mc_world_chunk_fields_get(f->chunks[side][i], &fields) == MC_WORLD_STORAGE_OK &&
              fields.d == f->worlds[side] && fields.c && !fields.q && fields.m != NULL);
    }
    CHECK(f->providers[side].cache->count == 4 && f->providers[side].cache->insertion_count == 4 &&
          mc_world_store_chunk_count(f->stores[side]) == 4 && !f->worlds[side]->fields.o &&
          !f->worlds[side]->fields.B && !f->worlds[side]->fields.a);
    f->bindings_before_cart[side] = f->worlds[side]->persistent_bindings;
    ok = true;
cleanup:
    return ok;
}

static bool cell(fixture *f, size_t side, int32_t x, int32_t y, int32_t z, uint8_t id,
                 uint8_t metadata) {
    if (x < 0 || x >= 16 || z < 0 || z >= 16 || y < 0 || y >= 128 || metadata > 15)
        return false;
    const size_t index = ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
    const unsigned int shift = ((unsigned int)y & 1u) * 4u;
    uint8_t *const bytes = f->chunks[side][0]->e->a;
    f->states[side][0]->data->blocks[index] = id;
    bytes[index >> 1u] = (uint8_t)((bytes[index >> 1u] & (uint8_t) ~(15u << shift)) |
                                   ((unsigned int)metadata << shift));
    return true;
}

static bool metadata_is(fixture *f, size_t side, int32_t expected) {
    int32_t value = INT32_MIN;
    return mc_world_runtime_metadata_get(f->worlds[side], 8, 64, 8, &value, NULL) ==
               MC_WORLD_STORAGE_OK &&
           value == expected;
}

static bool list_is(const mc_world_class_entity_list *list, mc_entity *entity, size_t count) {
    size_t length = SIZE_MAX;
    if (mc_world_class_entity_list_count(list, &length) != MC_WORLD_STORAGE_OK || length != count)
        return false;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *value = NULL;
        if (mc_world_class_entity_list_get(list, i, &value) != MC_WORLD_STORAGE_OK ||
            value != entity)
            return false;
    }
    return true;
}

static bool queues_are(const mc_world_runtime *world, int32_t ordered, int32_t membership) {
    int32_t actual_e = -1, actual_f = -1;
    return mc_scheduled_ordered_size(world->fields.E, &actual_e) == MC_SCHEDULED_ORDERED_OK &&
           mc_scheduled_membership_size(world->fields.F, &actual_f) == MC_SCHEDULED_MEMBERSHIP_OK &&
           actual_e == ordered && actual_f == membership;
}

static bool neutral(const mc_world_mutation_error *error) {
    return error->status == MC_WORLD_STORAGE_OK && error->operation == NULL &&
           error->hook_class == NULL && error->cause_hook_class == NULL &&
           error->rail_sites_selected == 0 && error->registry.status == MC_REGISTRY_OK &&
           error->registry.initializer_pc == -1;
}

static bool tick(mc_world_runtime *world, const mc_block *detector,
                 mc_world_mutation_error *error) {
    *error = mc_world_mutation_empty_error();
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    const bool completed = mc_world_mutation_block_tick(world, detector, 8, 64, 8, NULL, error);
    mc_world_runtime_call_leave(world);
    return completed;
}

static bool publication_and_queries(fixture *f, size_t side) {
    bool ok = false, inserted = false;
    mc_world_runtime *const world = f->worlds[side];
    mc_world_class_entity_list *first = NULL, *second = NULL, *empty = NULL;
    mc_entity_classes *foreign = NULL;
    const mc_entity_class_literal *cart_class = NULL, *entity_class = NULL, *foreign_class = NULL;
    mc_world_nearby_error error = mc_world_nearby_empty_error();
    mc_entity_snapshot snapshot = {0};
    ++cases;
    CHECK(mc_minecart_entity_allocate(f->entities[side], &f->carts[side], NULL) == MC_ENTITY_OK);
    CHECK(mc_minecart_entity_construct(f->carts[side], world, 8.5, 64.0, 8.5, 2, NULL) ==
          MC_ENTITY_OK);
    mc_entity *const cart = f->carts[side];
    CHECK(mc_entity_class_literal_get(f->entities[side], MC_ENTITY_CLASS_KIND_MINECART,
                                      &cart_class) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(f->entities[side], MC_ENTITY_CLASS_KIND_ENTITY,
                                      &entity_class) == MC_ENTITY_OK &&
          mc_entity_get_snapshot(cart, &snapshot) == MC_ENTITY_OK &&
          snapshot.phase == MC_ENTITY_READY && snapshot.class_literal == cart_class &&
          snapshot.base.world == world && snapshot.bounds != NULL &&
          mc_entity_bounds_intersects(snapshot.bounds, &detector_box) && cart->holds == 1);
    CHECK(mc_world_runtime_insert_entity(world, cart, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted && cart->holds == 3 && world->fields.b->count == 1 &&
          world->fields.b->items[0] == cart && cart->base.bF && cart->base.bG == 0 &&
          cart->base.bH == 4 && cart->base.bI == 0);
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[side][0], cart, NULL) ==
              MC_WORLD_STORAGE_OK &&
          cart->holds == 4 && world->fields.b->count == 1);

    ++cases;
    CHECK(mc_world_runtime_class_entities(world, cart_class, &detector_box, &first, &error) ==
              MC_WORLD_STORAGE_OK &&
          list_is(first, cart, 2) && cart->holds == 6 && world->local_leases == 1 &&
          error.query.status == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_class_entities(world, entity_class, &detector_box, &second, &error) ==
              MC_WORLD_STORAGE_OK &&
          list_is(second, cart, 2) && cart->holds == 8 && world->local_leases == 2 &&
          first != second &&
          mc_world_class_entity_list_identity(first) !=
              mc_world_class_entity_list_identity(second) &&
          mc_world_class_entity_list_identity(first) != world->fields.R &&
          mc_world_class_entity_list_identity(second) != world->fields.R);
    CHECK(mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_retire_world(world, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY);
    mc_entity *sentinel = cart;
    CHECK(mc_world_class_entity_list_get(first, 2, &sentinel) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          sentinel == cart && list_is(first, cart, 2));

    ++cases;
    CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                   NULL, &foreign) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(foreign, MC_ENTITY_CLASS_KIND_MINECART, &foreign_class) ==
              MC_ENTITY_OK &&
          foreign_class != cart_class);
    CHECK(mc_world_runtime_class_entities(world, foreign_class, &detector_box, &empty, &error) ==
              MC_WORLD_STORAGE_OK &&
          list_is(empty, NULL, 0) && cart->holds == 8 && world->local_leases == 3 &&
          list_is(first, cart, 2));
    mc_world_class_entity_list_release(empty);
    empty = NULL;

    ++cases;
    CHECK(mc_world_runtime_class_entities(world, cart_class, &miss_box, &empty, &error) ==
              MC_WORLD_STORAGE_OK &&
          list_is(empty, NULL, 0) && cart->holds == 8 && list_is(first, cart, 2) &&
          list_is(second, cart, 2));
    mc_world_class_entity_list_release(empty);
    empty = NULL;
    mc_world_class_entity_list_release(second);
    second = NULL;
    CHECK(cart->holds == 6 && world->local_leases == 1 && list_is(first, cart, 2));
    mc_world_class_entity_list_release(first);
    first = NULL;
    CHECK(cart->holds == 4 && world->local_leases == 0 && world->active_calls == 0 &&
          mc_entity_classes_free(foreign) == MC_ENTITY_OK);
    foreign = NULL;
    ok = true;
cleanup:
    mc_world_class_entity_list_release(empty);
    mc_world_class_entity_list_release(second);
    mc_world_class_entity_list_release(first);
    if (foreign != NULL && mc_entity_classes_free(foreign) != MC_ENTITY_OK)
        ok = false;
    return ok;
}

static bool unsupported_frontiers(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    mc_entity *const cart = f->carts[side];
    mc_entity_snapshot before = {0}, after = {0};
    mc_entity_move_error move;
    mc_entity_move_error_init(&move);
    mc_entity_nbt_error nbt = mc_entity_nbt_empty_error();
    mc_nbt_tree *tree = NULL;
    mc_world_collision_list_view collision = {0};
    mc_world_entity_list_view nearby = {0};
    mc_world_collision_error error = mc_world_collision_empty_error();
    ++cases;
    CHECK(mc_entity_get_snapshot(cart, &before) == MC_ENTITY_OK);
    const mc_entity_bounds bounds = *before.bounds;
    CHECK(mc_entity_move(cart, 0.0, 0.0, 0.0, NULL, &move) == MC_ENTITY_STATE_ERROR &&
          move.bytecode == 0 && move.operation != NULL &&
          strcmp(move.operation, "Entity movement concrete virtuals") == 0);
    CHECK(mc_entity_build_nbt(cart, NULL, &tree, &nbt) == MC_ENTITY_NBT_OK && tree != NULL &&
          nbt.status == MC_ENTITY_NBT_OK && nbt.operation == NULL);
    const mc_nbt_tag *root = mc_nbt_tree_root(tree);
    CHECK(root != NULL && root->type == MC_NBT_COMPOUND && root->value.compound.length == 11);
    static const uint16_t type_name[] = {'T', 'y', 'p', 'e'};
    static const uint16_t push_x_name[] = {'P', 'u', 's', 'h', 'X'};
    static const uint16_t push_z_name[] = {'P', 'u', 's', 'h', 'Z'};
    static const uint16_t fuel_name[] = {'F', 'u', 'e', 'l'};
    static const uint16_t items_name[] = {'I', 't', 'e', 'm', 's'};
    const mc_nbt_tag *type = mc_nbt_compound_get(root, type_name, COUNT(type_name));
    const mc_nbt_tag *push_x = mc_nbt_compound_get(root, push_x_name, COUNT(push_x_name));
    const mc_nbt_tag *push_z = mc_nbt_compound_get(root, push_z_name, COUNT(push_z_name));
    const mc_nbt_tag *fuel = mc_nbt_compound_get(root, fuel_name, COUNT(fuel_name));
    CHECK(type != NULL && type->type == MC_NBT_INT && type->value.int_value == 2);
    CHECK(push_x != NULL && push_x->type == MC_NBT_DOUBLE &&
          push_x->value.double_bits == UINT64_C(0));
    CHECK(push_z != NULL && push_z->type == MC_NBT_DOUBLE &&
          push_z->value.double_bits == UINT64_C(0));
    CHECK(fuel != NULL && fuel->type == MC_NBT_SHORT && fuel->value.short_value == 0);
    CHECK(mc_nbt_compound_get(root, items_name, COUNT(items_name)) == NULL);
    CHECK(mc_world_runtime_collision_boxes(world, NULL, &detector_box, &collision, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          collision.owner == NULL && error.world_bytecode == 218 &&
          error.nearby.query.operation != NULL &&
          strcmp(error.nearby.query.operation, "Collision Entity concrete virtual") == 0);
    /* Retire only the actual mutable R contents through another ordinary query.
     * The failed collision query must not pretend Minecart virtuals return null. */
    CHECK(mc_world_runtime_nearby_entities(world, NULL, &miss_box, &nearby, NULL) ==
          MC_WORLD_STORAGE_OK);
    mc_world_entity_list_view_release(&nearby);
    CHECK(mc_entity_get_snapshot(cart, &after) == MC_ENTITY_OK && after.phase == before.phase &&
          after.class_literal == before.class_literal && after.base.x == before.base.x &&
          after.base.y == before.base.y && after.base.z == before.base.z &&
          after.bounds == before.bounds && memcmp(after.bounds, &bounds, sizeof(bounds)) == 0 &&
          cart->holds == 4 && world->local_leases == 0 && world->active_calls == 0);
    ok = true;
cleanup:
    mc_world_entity_list_view_release(&nearby);
    mc_world_collision_list_view_release(&collision);
    mc_nbt_tree_free(tree);
    return ok;
}

static bool notification_prefix(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    const mc_block *const detector = f->registry[side]->blocks.m[28];
    mc_world_entity_contact_error error = mc_world_entity_contact_empty_error();
    mc_world_schedule_class_state before = {0}, after = {0};
    ++cases;
    CHECK(cell(f, side, 8, 63, 8, 1, 0) && cell(f, side, 8, 64, 8, 28, 0) &&
          cell(f, side, 7, 64, 8, 69, 0) &&
          mc_world_runtime_schedule_class_get(f->classes[side], &before) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_status status =
        mc_world_runtime_entity_contact(world, detector, 8, 64, 8, NULL, &error);
    const bool restored = cell(f, side, 7, 64, 8, 0, 0);
    CHECK(restored && status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 35 &&
          error.helper_bytecode == 105 && error.mutation.rail_detector_update.bytecode == 105 &&
          error.mutation.rail_detector_cause.bytecode == 105 &&
          error.mutation.wrapper_bytecode == 40 && error.mutation.neighbor_call_bytecode == 8 &&
          error.mutation.neighbor_bytecode == 41 && error.mutation.cause_hook_bytecode == 0 &&
          error.mutation.cause_hook_class != NULL &&
          strcmp(error.mutation.cause_hook_class, side == 0 ? "xr" : "pd") == 0);
    CHECK(metadata_is(f, side, 8) && queues_are(world, 0, 0) &&
          mc_world_runtime_schedule_class_get(f->classes[side], &after) == MC_WORLD_STORAGE_OK &&
          after.initialized == before.initialized && after.next == before.next &&
          f->carts[side]->holds == 4 && world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool scheduling_prefix(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    const mc_block *const detector = f->registry[side]->blocks.m[28];
    mc_world_entity_contact_error error = mc_world_entity_contact_empty_error();
    mc_world_schedule_class_state before = {0}, after = {0};
    const mc_scheduled_tick *entries[1] = {NULL};
    mc_scheduled_tick *held = NULL;
    size_t written = 0;
    mc_scheduled_tick_fields fields = {0};
    int64_t time = -1;
    ++cases;
    CHECK(cell(f, side, 8, 64, 8, 28, 0) && queues_are(world, 0, 0) &&
          mc_world_info_time_get(world->fields.x, &time) == MC_WORLD_INFO_OK &&
          mc_world_runtime_schedule_class_get(f->classes[side], &before) == MC_WORLD_STORAGE_OK);
    mc_scheduled_ordered *const actual_e = world->fields.E;
    world->fields.E = NULL;
    const mc_world_storage_status status =
        mc_world_runtime_entity_contact(world, detector, 8, 64, 8, NULL, &error);
    world->fields.E = actual_e;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 35 &&
          error.helper_bytecode == 225 && error.mutation.rail_detector_update.bytecode == 225 &&
          error.mutation.rail_detector_cause.bytecode == 225 &&
          error.mutation.schedule.bytecode == 231 &&
          error.mutation.schedule.membership == MC_SCHEDULED_MEMBERSHIP_OK &&
          error.mutation.schedule.ordered == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          metadata_is(f, side, 8) && queues_are(world, 0, 1));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[side], &after) == MC_WORLD_STORAGE_OK &&
          after.initialized && after.next == before.next + 1 &&
          mc_scheduled_membership_snapshot(world->fields.F, entries, COUNT(entries), &written) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          written == 1 &&
          mc_scheduled_tick_fields_get(entries[0], &fields) == MC_SCHEDULED_TICK_OK &&
          fields.x == 8 && fields.y == 64 && fields.z == 8 && fields.block == 28 &&
          fields.due_time == time + 20 && fields.sequence == before.next);
    CHECK(mc_scheduled_tick_retain((mc_scheduled_tick *)entries[0]) == MC_SCHEDULED_TICK_OK);
    held = (mc_scheduled_tick *)entries[0];
    bool removed = false;
    CHECK(mc_scheduled_membership_remove(world->fields.F, held, &removed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          removed && queues_are(world, 0, 0));
    CHECK(f->carts[side]->holds == 4 && world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    mc_scheduled_tick_release(held);
    return ok;
}

static bool positive_and_deactivation(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    mc_entity *const cart = f->carts[side];
    const mc_block *const detector = f->registry[side]->blocks.m[28];
    mc_world_entity_contact_error contact = mc_world_entity_contact_empty_error();
    mc_world_mutation_error mutation = mc_world_mutation_empty_error();
    mc_world_schedule_class_state before = {0}, after = {0};
    mc_scheduled_tick *first = NULL, *same = NULL;
    mc_scheduled_tick_fields fields = {0}, after_fields = {0};
    int64_t time = -1;
    ++cases;
    CHECK(cell(f, side, 8, 64, 8, 28, 0) && queues_are(world, 0, 0) &&
          mc_world_info_time_get(world->fields.x, &time) == MC_WORLD_INFO_OK &&
          mc_world_runtime_schedule_class_get(f->classes[side], &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_entity_contact(world, detector, 8, 64, 8, NULL, &contact) ==
              MC_WORLD_STORAGE_OK &&
          contact.status == MC_WORLD_STORAGE_OK && contact.operation == NULL &&
          contact.block == NULL && neutral(&contact.mutation) && metadata_is(f, side, 8) &&
          queues_are(world, 1, 1));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[side], &after) == MC_WORLD_STORAGE_OK &&
          after.initialized && after.next == before.next + 1 &&
          mc_scheduled_ordered_first(world->fields.E, &first) == MC_SCHEDULED_ORDERED_OK &&
          mc_scheduled_tick_fields_get(first, &fields) == MC_SCHEDULED_TICK_OK && fields.x == 8 &&
          fields.y == 64 && fields.z == 8 && fields.block == 28 && fields.due_time == time + 20 &&
          fields.sequence == before.next);

    ++cases;
    before = after;
    CHECK(mc_world_runtime_entity_contact(world, detector, 8, 64, 8, NULL, &contact) ==
              MC_WORLD_STORAGE_OK &&
          neutral(&contact.mutation) && queues_are(world, 1, 1) &&
          mc_world_runtime_schedule_class_get(f->classes[side], &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next);

    ++cases;
    CHECK(tick(world, detector, &mutation) && neutral(&mutation) && metadata_is(f, side, 8) &&
          queues_are(world, 1, 1) &&
          mc_world_runtime_schedule_class_get(f->classes[side], &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next + 1 &&
          mc_scheduled_ordered_first(world->fields.E, &same) == MC_SCHEDULED_ORDERED_OK &&
          same == first &&
          mc_scheduled_tick_fields_get(same, &after_fields) == MC_SCHEDULED_TICK_OK &&
          after_fields.due_time == fields.due_time && after_fields.sequence == fields.sequence);
    mc_scheduled_tick_release(same);
    same = NULL;

    ++cases;
    before = after;
    /* Existing generic position setter changes the same actual bounds; it is
     * not a Minecart move/tick, Chunk relinking or rail-motion implementation. */
    CHECK(mc_entity_set_position(cart, 12.5, 64.0 + (double)cart->base.offset, 12.5) ==
          MC_ENTITY_OK);
    CHECK(!mc_entity_bounds_intersects(cart->bounds, &detector_box) &&
          mc_entity_bounds_intersects(cart->bounds, &moved_box) &&
          tick(world, detector, &mutation) && neutral(&mutation) && metadata_is(f, side, 0) &&
          queues_are(world, 1, 1) &&
          mc_world_runtime_schedule_class_get(f->classes[side], &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next && cart->holds == 4 && world->local_leases == 0 &&
          world->active_calls == 0 && !world->fields.o && !world->fields.B);
    CHECK(mc_scheduled_tick_fields_get(first, &after_fields) == MC_SCHEDULED_TICK_OK &&
          after_fields.due_time == fields.due_time && after_fields.sequence == fields.sequence &&
          after_fields.x == fields.x && after_fields.y == fields.y && after_fields.z == fields.z &&
          after_fields.block == fields.block);
    ok = true;
cleanup:
    mc_scheduled_tick_release(same);
    mc_scheduled_tick_release(first);
    return ok;
}

static bool final_occurrence_lifetime(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    mc_entity *cart = f->carts[side];
    mc_world_class_entity_list *list = NULL;
    const mc_entity_class_literal *target = NULL;
    mc_entity_snapshot state = {0};
    mc_entity_class_snapshot classes = {0};
    ++cases;
    CHECK(mc_entity_class_literal_get(f->entities[side], MC_ENTITY_CLASS_KIND_MINECART, &target) ==
              MC_ENTITY_OK &&
          mc_world_runtime_class_entities(world, target, &moved_box, &list, NULL) ==
              MC_WORLD_STORAGE_OK &&
          list_is(list, cart, 2) && cart->holds == 6 && world->local_leases == 1);
    mc_entity_release(f->carts[side]);
    f->carts[side] = NULL;
    CHECK(cart->holds == 5 && list_is(list, cart, 2) &&
          mc_entity_classes_get_snapshot(f->entities[side], &classes) == MC_ENTITY_OK &&
          classes.live_entities == 1 &&
          mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_BUSY);
    mc_world_class_entity_list_release(list);
    list = NULL;
    CHECK(cart->holds == 3 && mc_entity_get_snapshot(cart, &state) == MC_ENTITY_OK &&
          state.phase == MC_ENTITY_READY && state.base.world == world &&
          world->persistent_bindings == f->bindings_before_cart[side] + 1u &&
          world->local_leases == 0);
    /* Do not inspect the borrowed Entity or its snapshots after this final
     * three-occurrence retirement can release its last native hold. */
    cart = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_OK);
    f->terminal[side] = true;
    CHECK(world->persistent_bindings == f->bindings_before_cart[side] &&
          mc_entity_classes_get_snapshot(f->entities[side], &classes) == MC_ENTITY_OK &&
          classes.live_entities == 0 && world->active_calls == 0 && world->local_leases == 0);
    ok = true;
cleanup:
    mc_world_class_entity_list_release(list);
    return ok;
}

static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t side = 0; side < 2; ++side) {
        mc_world_runtime *const world = f->worlds[side];
        if (world == NULL)
            continue;
        if (f->bound[side] && !f->terminal[side]) {
            if (mc_world_runtime_retire_entity_containers(world, NULL) != MC_WORLD_STORAGE_OK)
                return false;
            f->terminal[side] = true;
        }
        if (f->bound[side] && !f->descriptors_retired[side]) {
            mc_chunk_cache *cache = f->providers[side].cache;
            if (!mc_chunk_cache_validate(cache) ||
                !mc_chunk_pending_validate(f->providers[side].pending) ||
                mc_chunk_pending_clear(f->providers[side].pending) != MC_CHUNK_ACQUIRE_OK ||
                !mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                     cache->insertion_capacity))
                return false;
            f->descriptors_retired[side] = true;
        }
        if (!f->world_retired[side]) {
            if (mc_world_runtime_retire_world(world, NULL) != MC_WORLD_STORAGE_OK)
                return false;
            f->world_retired[side] = true;
        }
        for (size_t i = 0; i < COUNT(chunk_coordinates); ++i) {
            if (f->states[side][i] != NULL &&
                mc_world_store_retire_chunk(f->stores[side], f->states[side][i], NULL) !=
                    MC_WORLD_STORAGE_OK)
                return false;
            f->states[side][i] = NULL;
            f->chunks[side][i] = NULL;
        }
    }
    for (size_t side = 0; side < 2; ++side) {
        mc_entity_release(f->carts[side]);
        f->carts[side] = NULL;
        if (f->worlds[side] != NULL &&
            mc_world_runtime_free(f->worlds[side], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        mc_world_provider_release(f->dimensions[side]);
        if (mc_world_runtime_classes_free(f->classes[side], NULL) != MC_WORLD_STORAGE_OK ||
            mc_registry_classes_free(f->registry[side], NULL) != MC_REGISTRY_OK ||
            mc_entity_classes_free(f->entities[side]) != MC_ENTITY_OK)
            return false;
        mc_world_store_free(f->stores[side]);
    }
    mc_world_info_name_release(f->name);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'M', 'i', 'n', 'e', 'c', 'a', 'r', 't'};
    if (argc != 3 || strcmp(argv[1], "world-minecart-detector") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resource_owner *r = &f->resources[side];
        r->streams[0] = (resource_stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (resource_stream){.bytes = language};
        r->streams[2] = (resource_stream){.bytes = language};
        r->api = (mc_registry_resources){r, resource_open, resource_close, resource_ready};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entities[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entities[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
        CHECK(open_world(f, side, root));
        CHECK(publication_and_queries(f, side));
        CHECK(unsupported_frontiers(f, side));
        CHECK(notification_prefix(f, side));
        CHECK(scheduling_prefix(f, side));
        CHECK(positive_and_deactivation(f, side));
        CHECK(final_occurrence_lifetime(f, side));
    }
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Minecart detector cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u Minecart detector checks (%u literal cases, "
                              "world-minecart-detector)\n",
                              checks, cases) < 0)
        result = 1;
    return result;
}
