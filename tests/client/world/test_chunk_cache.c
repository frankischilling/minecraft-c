/* Reuse genuine client World/Registry/Chunk construction and checked teardown. */
#define MC_PICKING_BODY_MAIN_NAME mc_ew_picking_fixture_main
int mc_ew_picking_fixture_main(int argc, char **argv);
#include "test_picking.c"
#undef main

#include "mc/client/world/chunk_cache.h"
#include "world/chunk/object_state_internal.h"
/* Compile the actual canonical Store body once in this white-box test, as the
 * Display list tests do for Display. Its private identity lease counter is
 * needed for a deterministic native admission failure, not a fake getter or
 * copied Store layout. The linked static archive supplies the other owners. */
#include "../../../src/world/storage/store.c"
#include <limits.h>

static unsigned int ew_rows;

static const char *ew_exception(mc_client_chunk_cache_status status) {
    switch (status) {
        case MC_CLIENT_CHUNK_CACHE_SOURCE_NULL:
            return "NULL";
        case MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX:
            return "INDEX";
        case MC_CLIENT_CHUNK_CACHE_SOURCE_NEGATIVE_ARRAY:
            return "NEGATIVE";
        default:
            return NULL;
    }
}

static bool ew_integer(const char *name, mc_client_chunk_cache_status status, int32_t value) {
    const char *exception = ew_exception(status);
    CHECK(status == MC_CLIENT_CHUNK_CACHE_OK || exception != NULL);
    if (exception != NULL)
        printf("EW %s %s\n", name, exception);
    else
        printf("EW %s %" PRId32 "\n", name, value);
    ++ew_rows;
    return true;
}

static bool ew_float(const char *name, mc_client_chunk_cache_status status, float value) {
    const char *exception = ew_exception(status);
    CHECK(status == MC_CLIENT_CHUNK_CACHE_OK || exception != NULL);
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    if (exception != NULL)
        printf("EW %s %s\n", name, exception);
    else
        printf("EW %s %08" PRIx32 "\n", name, bits);
    ++ew_rows;
    return true;
}

static bool ew_constructor(const char *name, picking_fixture *f, mc_world_runtime *world,
                           int32_t x0, int32_t z0, int32_t x1, int32_t z1) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error error;
    mc_client_chunk_cache_status status =
        mc_client_chunk_cache_create(world, x0, INT32_MIN, z0, x1, INT32_MAX, z1, &cache, &error);
    CHECK(cache != NULL);
    const char *exception = ew_exception(status);
    CHECK(status == MC_CLIENT_CHUNK_CACHE_OK || exception != NULL);
    if (exception != NULL)
        printf("EW %s %s\n", name, exception);
    else {
        mc_client_chunk_cache_state state;
        CHECK(mc_client_chunk_cache_inspect(cache, &state) && state.returned && state.c != NULL);
        bool same = state.d == world;
        for (int32_t x = 0; x < state.rows; ++x)
            for (int32_t z = 0; z < state.columns; ++z) {
                const int32_t cx = (x0 >> 4) + x, cz = (z0 >> 4) + z;
                CHECK(cx >= 0 && cx < 2 && cz >= 0 && cz < 2);
                same = same && state.c[x][z] == f->chunks[(size_t)(cx * 2 + cz)];
            }
        const bool distinct = state.rows < 2 || state.c[0] != state.c[1];
        printf("EW %s %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %u %u\n", name, state.a,
               state.b, state.rows, state.rows == 0 ? -1 : state.columns, same ? 1u : 0u,
               distinct ? 1u : 0u);
    }
    ++ew_rows;
    CHECK(mc_client_chunk_cache_retire(&cache, &error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL);
    return true;
}

static bool ew_material_id(fixture *f, mc_material *material, int32_t *out) {
    for (int32_t i = 0; i <= (int32_t)MC_MATERIAL_B; ++i)
        if (mc_materials_get(f->materials, (mc_material_id)i) == material) {
            *out = i;
            return true;
        }
    CHECK(false);
    return false;
}

static bool ew_unpublished_occurrence(picking_fixture *f) {
    mc_world_runtime *world = f->base.world;
    mc_world_store *store = f->base.store;
    store_chunk *first = store_find_chunk(store, f->chunks[0]);
    store_chunk *second = store_find_chunk(store, f->chunks[1]);
    CHECK(first != NULL && second != NULL && first != second && first->leases < SIZE_MAX &&
          second->leases < SIZE_MAX);
    const size_t first_leases = first->leases, second_leases = second->leases;
    const size_t bindings = world->persistent_bindings;
    const size_t index = (8u << 11u) | (8u << 7u) | 64u;
    const uint8_t old_block = f->chunks[1]->data->blocks[index];
    const uint8_t old_metadata = f->chunks[1]->data->metadata[index / 2u];
    const int32_t prior_block = f->chunks[0]->data->blocks[index];
    f->chunks[1]->data->blocks[index] = 1;
    CHECK(mc_chunk_nibbles_set(f->chunks[1]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, 12));
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error error;
    /* Native prepared quota fault on the SAME real Store entry. Cached
     * World.get succeeds; EW's subsequent independent lease must refuse. */
    second->leases = SIZE_MAX;
    const mc_client_chunk_cache_status status =
        mc_client_chunk_cache_create(world, 0, -1, 0, 31, 129, 31, &cache, &error);
    second->leases = second_leases; /* Restore before any assertion or cleanup. */
    CHECK(status == MC_CLIENT_CHUNK_CACHE_LIMIT && cache != NULL && error.native_policy &&
          error.bytecode == 112 && error.child_bytecode == 0 &&
          error.acquisition.status == MC_CHUNK_ACQUIRE_OK &&
          error.storage.status == MC_WORLD_STORAGE_LIMIT && error.storage.operation != NULL &&
          strcmp(error.storage.operation, "chunk lease count") == 0);
    mc_client_chunk_cache_state state;
    CHECK(mc_client_chunk_cache_inspect(cache, &state) && !state.returned &&
          state.source_pc == 112 && state.last_store_pc == 115 && state.acquired == 1 &&
          state.leased == 1 && state.rows == 2 && state.columns == 2 &&
          state.c[0][0] == f->chunks[0] && state.c[0][1] == NULL && state.c[1][0] == NULL &&
          state.c[1][1] == NULL && first->leases == first_leases + 1u &&
          second->leases == second_leases && world->persistent_bindings == bindings + 1u);
    int32_t value = 91;
    CHECK(mc_client_chunk_cache_block(cache, 8, 64, 24, &value, &error) ==
              MC_CLIENT_CHUNK_CACHE_OK &&
          value == 0);
    value = 91;
    CHECK(mc_client_chunk_cache_metadata(cache, 8, 64, 24, &value, &error) ==
              MC_CLIENT_CHUNK_CACHE_SOURCE_NULL &&
          value == 91 && error.bytecode == 54 && !error.native_policy);
    CHECK(mc_client_chunk_cache_block(cache, 8, 64, 8, &value, &error) ==
              MC_CLIENT_CHUNK_CACHE_OK &&
          value == prior_block);
    CHECK(state.failure->status == MC_CLIENT_CHUNK_CACHE_LIMIT &&
          state.failure->storage.status == MC_WORLD_STORAGE_LIMIT &&
          strcmp(state.failure->storage.operation, "chunk lease count") == 0 &&
          first->leases == first_leases + 1u && second->leases == second_leases);
    CHECK(mc_client_chunk_cache_retire(&cache, &error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && first->leases == first_leases && second->leases == second_leases &&
          world->persistent_bindings == bindings);
    f->chunks[1]->data->blocks[index] = old_block;
    f->chunks[1]->data->metadata[index / 2u] = old_metadata;
    return true;
}

static bool ew_observe(picking_fixture *f) {
    CHECK(ew_constructor("CTOR_NULL", f, NULL, 0, 0, 15, 15));
    CHECK(ew_constructor("CTOR_ZERO_X", f, NULL, 16, 0, 0, 15));
    CHECK(ew_constructor("CTOR_ZERO_Z", f, NULL, 0, 16, 31, 0));
    CHECK(ew_constructor("CTOR_NEG_X", f, NULL, 32, 0, 0, 15));
    CHECK(ew_constructor("CTOR_NEG_Z", f, NULL, 16, 32, 0, 0));
    mc_world_runtime *world = f->base.world;
    CHECK(ew_constructor("CTOR_FOUR", f, world, 0, 0, 31, 31));
    CHECK(ew_constructor("CTOR_INCLUSIVE", f, world, 0, 0, 16, 16));
    CHECK(ew_constructor("CTOR_ONE", f, world, 31, 31, 31, 31));
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error error;
    CHECK(mc_client_chunk_cache_create(world, 0, -1, 0, 31, 129, 31, &cache, &error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    const size_t bindings = world->persistent_bindings;
    CHECK(bindings != 0);
    mc_client_chunk_cache_state state;
    CHECK(mc_client_chunk_cache_inspect(cache, &state) && state.acquired == 4 &&
          state.leased == 4 && state.c[0][0] == f->chunks[0] && state.c[0][1] == f->chunks[1] &&
          state.c[1][0] == f->chunks[2] && state.c[1][1] == f->chunks[3]);
    for (size_t i = 0; i < 4; ++i) {
        memset(f->chunks[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
        memset(f->chunks[i]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(f->chunks[i]->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(f->chunks[i]->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    }
    world->fields.f = 3;
    for (int32_t id = 0; id <= 96; ++id) {
        const int32_t y = id + 1;
        f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | (uint32_t)y] = (uint8_t)id;
        CHECK(mc_chunk_nibbles_set(f->chunks[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, y, 8,
                                   id & 15));
        CHECK(mc_chunk_nibbles_set(f->chunks[0]->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, y, 8,
                                   (id * 7) & 15));
        CHECK(mc_chunk_nibbles_set(f->chunks[0]->data->block_light, MC_CHUNK_NIBBLE_BYTES, 8, y, 8,
                                   (id * 3) & 15));
    }
    mc_chunk_provider *provider = world->fields.v;
    world->fields.v = NULL;
    for (int32_t id = 0; id <= 96; ++id) {
        const int32_t y = id + 1;
        int32_t value = -99;
        float fvalue = -99;
        bool bvalue = false;
        mc_material *material = NULL;
        char name[48];
#define EW_INT(prefix, expression)                                                                 \
    do {                                                                                           \
        snprintf(name, sizeof(name), prefix "_%" PRId32, id);                                      \
        const mc_client_chunk_cache_status s = (expression);                                       \
        CHECK(ew_integer(name, s, value));                                                         \
    } while (0)
#define EW_FLOAT(prefix, expression)                                                               \
    do {                                                                                           \
        snprintf(name, sizeof(name), prefix "_%" PRId32, id);                                      \
        const mc_client_chunk_cache_status s = (expression);                                       \
        CHECK(ew_float(name, s, fvalue));                                                          \
    } while (0)
        EW_INT("BLOCK", mc_client_chunk_cache_block(cache, 8, y, 8, &value, &error));
        EW_INT("META", mc_client_chunk_cache_metadata(cache, 8, y, 8, &value, &error));
        EW_INT("RAWLIGHT", mc_client_chunk_cache_light(cache, 8, y, 8, false, &value, &error));
        EW_INT("LIGHT", mc_client_chunk_cache_light(cache, 8, y, 8, true, &value, &error));
        EW_FLOAT("BRIGHT", mc_client_chunk_cache_brightness(cache, 8, y, 8, &fvalue, &error));
        EW_FLOAT("MINBRIGHT",
                 mc_client_chunk_cache_min_brightness(cache, 8, y, 8, 7, &fvalue, &error));
        CHECK(mc_client_chunk_cache_material(cache, 8, y, 8, &material, &error) ==
              MC_CLIENT_CHUNK_CACHE_OK);
        CHECK(ew_material_id(&f->base, material, &value));
        snprintf(name, sizeof(name), "MATERIAL_%" PRId32, id);
        CHECK(ew_integer(name, MC_CLIENT_CHUNK_CACHE_OK, value));
        CHECK(mc_client_chunk_cache_opaque(cache, 8, y, 8, &bvalue, &error) ==
              MC_CLIENT_CHUNK_CACHE_OK);
        snprintf(name, sizeof(name), "OPAQUE_%" PRId32, id);
        CHECK(ew_integer(name, MC_CLIENT_CHUNK_CACHE_OK, bvalue ? 1 : 0));
        CHECK(mc_client_chunk_cache_normal_cube(cache, 8, y, 8, &bvalue, &error) ==
              MC_CLIENT_CHUNK_CACHE_OK);
        snprintf(name, sizeof(name), "NORMAL_%" PRId32, id);
        CHECK(ew_integer(name, MC_CLIENT_CHUNK_CACHE_OK, bvalue ? 1 : 0));
#undef EW_INT
#undef EW_FLOAT
    }
    static const int32_t points[][3] = {{8, -1, 8},         {8, 128, 8},       {8, INT32_MIN, 8},
                                        {8, INT32_MAX, 8},  {-1, 64, 8},       {32, 64, 8},
                                        {8, 64, -1},        {8, 64, 32},       {-32000001, -1, 8},
                                        {32000000, -1, 8},  {8, -1, 32000000}, {8, -1, 32000001},
                                        {8, 128, 32000000}, {8, 128, 32000001}};
    for (size_t i = 0; i < sizeof(points) / sizeof(*points); ++i) {
        const int32_t *p = points[i];
        char name[48];
        int32_t value = -99;
        mc_client_chunk_cache_status status;
        snprintf(name, sizeof(name), "BOUND_BLOCK_%zu", i);
        status = mc_client_chunk_cache_block(cache, p[0], p[1], p[2], &value, &error);
        CHECK(ew_integer(name, status, value));
        snprintf(name, sizeof(name), "BOUND_META_%zu", i);
        status = mc_client_chunk_cache_metadata(cache, p[0], p[1], p[2], &value, &error);
        CHECK(ew_integer(name, status, value));
        snprintf(name, sizeof(name), "BOUND_LIGHT_%zu", i);
        status = mc_client_chunk_cache_light(cache, p[0], p[1], p[2], false, &value, &error);
        CHECK(ew_integer(name, status, value));
        mc_tile_entity *tile = NULL;
        status = mc_client_chunk_cache_tile_entity(cache, p[0], p[1], p[2], &tile, &error);
        const char *exception = ew_exception(status);
        CHECK(status == MC_CLIENT_CHUNK_CACHE_OK || exception != NULL);
        printf("EW BOUND_TILE_%zu %s\n", i,
               exception == NULL ? (tile == NULL ? "NULL_RESULT" : "TILE") : exception);
        ++ew_rows;
        mc_tile_entity_release(tile);
    }
    static const int32_t subtraction[] = {0, 3, 20, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < sizeof(subtraction) / sizeof(*subtraction); ++i) {
        world->fields.f = subtraction[i];
        char name[48];
        int32_t value = -99;
        snprintf(name, sizeof(name), "SUB_HIGH_%zu", i);
        mc_client_chunk_cache_status status =
            mc_client_chunk_cache_light(cache, 8, 128, 8, false, &value, &error);
        CHECK(ew_integer(name, status, value));
        snprintf(name, sizeof(name), "SUB_RAW_%zu", i);
        status = mc_client_chunk_cache_light(cache, 8, 4, 8, false, &value, &error);
        CHECK(ew_integer(name, status, value));
    }
    world->fields.f = 3;
    float unchanged = 9.25f;
    CHECK(mc_client_chunk_cache_brightness(cache, 32, 64, 8, &unchanged, &error) ==
              MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX &&
          unchanged == 9.25f && strcmp(error.method, "c") == 0 &&
          strcmp(error.descriptor, "(III)F") == 0 && error.bytecode == 14 &&
          error.nested_count == 2 && strcmp(error.nested[0].method, "d") == 0 &&
          error.nested[0].bytecode == 5 && strcmp(error.nested[1].descriptor, "(IIIZ)I") == 0 &&
          error.nested[1].bytecode == 254 && !error.native_policy);
    CHECK(mc_client_chunk_cache_inspect(cache, &state) && state.returned &&
          state.failure->status == MC_CLIENT_CHUNK_CACHE_OK && state.leased == 4 &&
          world->persistent_bindings == bindings);
    float fvalue = 0;
    CHECK(ew_float("MIN_INDEX",
                   mc_client_chunk_cache_min_brightness(cache, 8, 1, 8, 16, &fvalue, &error),
                   fvalue));
    mc_world_chunk_manager *manager = NULL;
    CHECK(mc_client_chunk_cache_manager(cache, &manager, &error) == MC_CLIENT_CHUNK_CACHE_OK);
    mc_world_provider_fields dimension;
    CHECK(mc_world_provider_fields_get(world->fields.t, &dimension) == MC_WORLD_PROVIDER_OK);
    CHECK(ew_integer("MANAGER", MC_CLIENT_CHUNK_CACHE_OK, manager == dimension.b ? 1 : 0));
    world->fields.v = provider;
    f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | 110u] = 54;
    mc_tile_entity *tile = NULL, *again = NULL;
    CHECK(mc_client_chunk_cache_tile_entity(cache, 8, 110, 8, &tile, &error) ==
              MC_CLIENT_CHUNK_CACHE_OK &&
          tile != NULL);
    world->fields.v = NULL;
    CHECK(mc_client_chunk_cache_tile_entity(cache, 8, 110, 8, &again, &error) ==
              MC_CLIENT_CHUNK_CACHE_OK &&
          again == tile);
    CHECK(ew_integer("TILE_SAME", MC_CLIENT_CHUNK_CACHE_OK, 1));
    mc_tile_entity_release(again);
    mc_tile_entity_release(tile);
    world->fields.v = provider;
    /* Invalidation cannot retire any captured identity while EW owns its
     * leases. Checked World free likewise remains BUSY through EW's hold. */
    CHECK(mc_world_store_retire_chunk(f->base.store, f->chunks[0], NULL) == MC_WORLD_STORAGE_BUSY);
    const size_t before_retire = world->persistent_bindings;
    CHECK(before_retire >= bindings &&
          mc_client_chunk_cache_retire(&cache, &error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && world->persistent_bindings == before_retire - 1u);
    CHECK(ew_unpublished_occurrence(f));
    CHECK(mc_client_chunk_cache_create(NULL, 16, 0, 32, 0, 0, 0, &cache, &error) ==
              MC_CLIENT_CHUNK_CACHE_SOURCE_NEGATIVE_ARRAY &&
          cache != NULL && mc_client_chunk_cache_inspect(cache, &state) &&
          state.last_store_pc == 21 && state.source_pc == 55 && state.a == 1 && state.b == 2 &&
          state.c == NULL && !state.returned);
    int32_t untouched = 91;
    CHECK(mc_client_chunk_cache_block(cache, 16, 64, 32, &untouched, &error) ==
              MC_CLIENT_CHUNK_CACHE_SOURCE_NULL &&
          untouched == 91 && error.bytecode == 46 && !error.native_policy &&
          state.failure->status == MC_CLIENT_CHUNK_CACHE_SOURCE_NEGATIVE_ARRAY);
    CHECK(mc_client_chunk_cache_retire(&cache, &error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 1;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !ew_observe(f) || ew_rows != 950 || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Source EW: %u checks; 950 original rows and canonical leases\n", checks);
    return 0;
}
