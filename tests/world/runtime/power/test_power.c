#define main mc_power_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/block/registry/horizontal_directions_internal.h"
#include "../../../../src/block/registry/runtime_internal.h"
#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/power/power_internal.h"
#include "mc/world/runtime/power/power.h"

/* Native contracts, not original observations or propagation tests. Two real
 * GAME constructors load populated (0,0) records. Registry-created Blocks and
 * actual stored bytes/nibbles supply every emitter and connection. Temporary
 * nullable source controls are restored before assertions. */
typedef enum power_call { WEAK_NEIGHBORS, WEAK_AT, STRONG_NEIGHBORS, STRONG_AT } power_call;

static const int32_t power_directions[] = {0, 1, 2, 3, 4, 5, -1, 6, INT32_MIN, INT32_MAX};

static void power_air(fixture *f, size_t side) {
    memset(f->states[side]->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(f->states[side]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
}

static bool power_cell(fixture *f, size_t side, int32_t x, int32_t y, int32_t z, uint8_t id,
                       int32_t metadata) {
    if (x < 0 || x >= 16 || z < 0 || z >= 16 || y < 0 || y >= 128)
        return false;
    f->states[side]->data->blocks[((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y] = id;
    bool changed = false;
    return mc_world_runtime_metadata_set_quiet(f->worlds[side], x, y, z, metadata, &changed,
                                               NULL) == MC_WORLD_STORAGE_OK;
}

static bool power_no_activation(const mc_world_nearby_error *e) {
    return !e->query.population.reached && e->query.population.current.provider_class == NULL &&
           e->query.population.origin.provider_class == NULL && e->entity.status == MC_ENTITY_OK &&
           e->entity.operation == NULL && e->tile.status == MC_WORLD_STORAGE_OK &&
           e->tile.operation == NULL && e->tile.bytecode == 0 && e->tile.chunk_bytecode == 0 &&
           e->tile.activation_bytecode == 0 && e->tile.activation_world_bytecode == 0 &&
           e->tile.load_bytecode == 0 && e->tile.load_builder_bytecode == 0 &&
           e->tile.registry.status == MC_REGISTRY_OK && e->tile.registry.initializer_pc == -1 &&
           e->tile.tile.item_index == SIZE_MAX && e->tile.tile.source_bytecode == 0 &&
           e->batch.index == SIZE_MAX && e->batch.publication.status == MC_WORLD_STORAGE_OK &&
           e->batch.publication.bytecode == 0 && e->batch.publication.operation == NULL &&
           e->load_entity.index == SIZE_MAX &&
           e->load_entity.native.status == MC_WORLD_STORAGE_OK &&
           e->load_entity.native.operation == NULL && e->activation_bytecode == 0 &&
           e->activation_world_bytecode == 0 && e->load_bytecode == 0 &&
           e->load_builder_bytecode == 0;
}

static bool power_neutral(const mc_world_power_error *e) {
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL &&
           e->cause.status == MC_WORLD_STORAGE_OK && e->cause.operation == NULL &&
           e->world_class == NULL && !e->weak_neighbors.entered && !e->weak_at.entered &&
           !e->strong_neighbors.entered && !e->strong_at.entered && !e->capability.entered &&
           !e->strong.entered && !e->weak.entered && !e->wire_connectivity.entered &&
           e->support.status == MC_WORLD_STORAGE_OK && e->support.bytecode == 0 &&
           e->access.query.status == MC_WORLD_STORAGE_OK && e->access.query.bytecode == 0 &&
           e->access.query.chunk_bytecode == 0 && e->access.query.provider_bytecode == 0 &&
           e->access.activation_bytecode == 0 && e->access.activation_world_bytecode == 0 &&
           e->access.load_bytecode == 0 && e->access.load_builder_bytecode == 0 &&
           e->access.slice == SIZE_MAX && e->access.index == SIZE_MAX &&
           e->registry.status == MC_REGISTRY_OK && e->registry.initializer_pc == -1 &&
           e->registry.active_initializer_pc == -1 && power_no_activation(&e->access);
}

static mc_world_storage_status power_held(mc_world_runtime *w, power_call method, int32_t x,
                                          int32_t y, int32_t z, int32_t side, bool *out,
                                          mc_world_power_error *error) {
    mc_world_storage_status status = mc_world_runtime_call_enter(w, NULL);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    switch (method) {
        case WEAK_NEIGHBORS:
            status = mc_world_power_neighbors_held(w, x, y, z, out, error);
            break;
        case WEAK_AT:
            status = mc_world_power_at_held(w, x, y, z, side, out, error);
            break;
        case STRONG_NEIGHBORS:
            status = mc_world_power_strong_neighbors_held(w, x, y, z, out, error);
            break;
        case STRONG_AT:
            status = mc_world_power_strong_at_held(w, x, y, z, side, out, error);
            break;
    }
    mc_world_runtime_call_leave(w);
    return status;
}

static mc_world_storage_status power_block(mc_world_runtime *w, const mc_block *block, bool strong,
                                           int32_t side, bool *out, mc_world_power_error *error) {
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    if (w != NULL)
        status = mc_world_runtime_call_enter(w, NULL);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = strong ? mc_world_block_strong_power_held(w, block, 8, 64, 8, side, out, error)
                    : mc_world_block_weak_power_held(w, block, 8, 64, 8, side, out, error);
    if (w != NULL)
        mc_world_runtime_call_leave(w);
    return status;
}

static bool power_query_is(mc_world_runtime *w, power_call method, int32_t x, int32_t y, int32_t z,
                           int32_t side, bool expected) {
    bool out = !expected;
    mc_world_power_error error;
    memset(&error, 0xa5, sizeof(error)); /* Must be overwritten, never adopted. */
    return power_held(w, method, x, y, z, side, &out, &error) == MC_WORLD_STORAGE_OK &&
           out == expected && power_neutral(&error) && w->active_calls == 0 && w->local_leases == 0;
}

static bool power_special(mc_block_kind kind) {
    return kind == MC_BLOCK_BV_BC || kind == MC_BLOCK_PH_JN || kind == MC_BLOCK_OI_IZ ||
           kind == MC_BLOCK_XR_PD || kind == MC_BLOCK_DB_BY || kind == MC_BLOCK_SM_LO ||
           kind == MC_BLOCK_WO_OI;
}

static bool power_capabilities(fixture *f, size_t side) {
    bool ok = false;
    bool seen[MC_BLOCK_KIND_COUNT] = {0};
    size_t kinds = 0;
    for (size_t id = 1; id < MC_REGISTRY_BLOCK_SLOTS; ++id) {
        const mc_block *const block = f->registry[side]->blocks.m[id];
        if (block == NULL || seen[block->kind])
            continue;
        seen[block->kind] = true;
        ++kinds;
        const bool expected = block->kind == MC_BLOCK_SM_LO
                                  ? block->subtype.sm.a
                                  : (power_special(block->kind) && block->kind != MC_BLOCK_WO_OI);
        bool out = !expected;
        mc_world_power_error error;
        memset(&error, 0xa5, sizeof(error));
        CHECK(mc_world_block_power_capability_held(block, &out, &error) == MC_WORLD_STORAGE_OK &&
              out == expected && power_neutral(&error));
        if (!power_special(block->kind)) {
            out = true;
            memset(&error, 0xa5, sizeof(error));
            CHECK(power_block(NULL, block, false, INT32_MAX, &out, &error) == MC_WORLD_STORAGE_OK &&
                  !out && power_neutral(&error));
            out = true;
            CHECK(power_block(NULL, block, true, -1, &out, &error) == MC_WORLD_STORAGE_OK && !out &&
                  power_neutral(&error));
        }
    }
    CHECK(kinds == (size_t)MC_BLOCK_KIND_COUNT && kinds == 69);
    bool out = true;
    mc_world_power_error error;
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_world_block_power_capability_held(NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out && !error.capability.entered && !error.strong.entered && !error.weak.entered &&
          !error.weak_neighbors.entered && error.registry.status == MC_REGISTRY_OK);
    CHECK(mc_world_block_power_capability_held(f->registry[side]->blocks.m[1], NULL, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          !error.capability.entered);
    ok = true;
cleanup:
    return ok;
}

/* Literal side masks independently state the paired orientation tables. */
static bool power_expected(uint8_t id, int32_t metadata, int32_t side, bool strong) {
    static const unsigned int button[8] = {0, 32, 16, 8, 4, 2, 0, 0};
    static const unsigned int lever[8] = {0, 32, 16, 8, 4, 2, 2, 0};
    static const unsigned int diode[4] = {8, 16, 4, 32};
    const bool conventional = side >= 0 && side < 6;
    switch (id) {
        case 70:
        case 72:
            return strong ? metadata != 0 && side == 1 : metadata > 0;
        case 28:
            return (metadata & 8) != 0 && (!strong || side == 1);
        case 77:
        case 69:
            if (!strong)
                return (metadata & 8) != 0;
            return conventional && (metadata & 8) != 0 &&
                   (((id == 77 ? button : lever)[metadata & 7] >> (unsigned int)side) & 1u);
        case 75:
        case 93:
            return false;
        case 76:
            return (!strong || side == 0) &&
                   !((metadata == 5 && side == 1) || (metadata == 3 && side == 3) ||
                     (metadata == 4 && side == 2) || (metadata == 1 && side == 5) ||
                     (metadata == 2 && side == 4));
        case 94:
            return conventional && ((diode[metadata & 3] >> (unsigned int)side) & 1u);
        case 55:
            return metadata != 0 && side >= 1 && side <= 5;
        default:
            return false;
    }
}

static bool power_emitter_matrix(fixture *f, size_t side) {
    bool ok = false;
    const uint8_t ids[] = {70, 72, 28, 77, 69, 75, 76, 93, 94, 55};
    power_air(f, side);
    for (size_t i = 0; i < COUNT(ids); ++i)
        for (int32_t metadata = 0; metadata < 16; ++metadata) {
            CHECK(power_cell(f, side, 8, 64, 8, ids[i], metadata));
            for (size_t j = 0; j < COUNT(power_directions); ++j) {
                const int32_t direction = power_directions[j];
                CHECK(power_query_is(f->worlds[side], WEAK_AT, 8, 64, 8, direction,
                                     power_expected(ids[i], metadata, direction, false)));
                CHECK(power_query_is(f->worlds[side], STRONG_AT, 8, 64, 8, direction,
                                     power_expected(ids[i], metadata, direction, true)));
            }
        }
    ok = true;
cleanup:
    return ok;
}

static bool power_guarded_metadata(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *chunk = f->chunks[side];
    const uint8_t ids[] = {70, 72, 28, 77, 69, 75, 76, 93, 94, 55};
    power_air(f, side);
    for (size_t i = 0; i < COUNT(ids); ++i) {
        CHECK(power_cell(f, side, 8, 64, 8, ids[i], 15));
        mc_world_chunk_nibble *const actual = chunk->e;
        bool out = true;
        mc_world_power_error error;
        memset(&error, 0xa5, sizeof(error));
        chunk->e = NULL;
        const mc_world_storage_status status = power_held(w, WEAK_AT, 8, 64, 8, 0, &out, &error);
        chunk->e = actual;
        if (ids[i] == 75 || ids[i] == 93)
            CHECK(status == MC_WORLD_STORAGE_OK && !out && power_neutral(&error));
        else {
            const unsigned int pc = ids[i] == 76 || ids[i] == 94 || ids[i] == 55 ? 14u : 5u;
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && error.weak_at.entered &&
                  error.weak_at.bytecode == 46 && error.weak.entered && error.weak.bytecode == pc &&
                  error.access.query.bytecode == 68 && error.access.query.chunk_bytecode == 7 &&
                  error.access.query.acquisition == MC_CHUNK_ACQUIRE_OK &&
                  error.access.query.registry.status == MC_REGISTRY_OK &&
                  error.registry.status == MC_REGISTRY_OK && !error.strong.entered &&
                  !error.wire_connectivity.entered && w->local_leases == 0 && w->active_calls == 0);
        }
    }
    CHECK(power_cell(f, side, 8, 64, 8, 76, 15));
    mc_world_chunk_nibble *const actual = chunk->e;
    bool out = true;
    mc_world_power_error error;
    chunk->e = NULL;
    const mc_world_storage_status early = power_held(w, STRONG_AT, 8, 64, 8, 1, &out, &error);
    chunk->e = actual;
    CHECK(early == MC_WORLD_STORAGE_OK && !out && power_neutral(&error));
    out = false;
    chunk->e = NULL;
    const mc_world_storage_status delegated = power_held(w, STRONG_AT, 8, 64, 8, 0, &out, &error);
    chunk->e = actual;
    CHECK(delegated == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !out &&
          error.strong_at.bytecode == 28 && error.strong.bytecode == 13 &&
          error.weak.bytecode == 14 && error.strong.entered && error.weak.entered &&
          strcmp(error.strong.declaring_class, side == 0 ? "db" : "by") == 0 &&
          error.access.query.chunk_bytecode == 7);
    CHECK(power_cell(f, side, 8, 64, 8, 94, 0));
    out = true;
    chunk->e = NULL;
    const mc_world_storage_status diode = power_held(w, STRONG_AT, 8, 64, 8, 6, &out, &error);
    chunk->e = actual;
    CHECK(diode == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && error.strong.bytecode == 8 &&
          error.weak.bytecode == 14 && error.strong.entered && error.weak.entered);
    mc_block *const wire = r->blocks.m[55];
    const bool a = wire->subtype.sm.a;
    wire->subtype.sm.a = false;
    out = true;
    const mc_world_storage_status wire_guard =
        power_block(NULL, wire, false, INT32_MAX, &out, &error);
    wire->subtype.sm.a = a;
    CHECK(wire_guard == MC_WORLD_STORAGE_OK && !out && power_neutral(&error));
    CHECK(
        power_block(NULL, r->blocks.m[75], false, 0, &out, &error) == MC_WORLD_STORAGE_OK && !out &&
        power_block(NULL, r->blocks.m[93], true, 0, &out, &error) == MC_WORLD_STORAGE_OK && !out &&
        power_block(NULL, r->blocks.m[76], true, -1, &out, &error) == MC_WORLD_STORAGE_OK && !out);
    ok = true;
cleanup:
    return ok;
}

static void power_neighbor(int32_t side, int32_t *x, int32_t *y, int32_t *z) {
    *x = 8;
    *y = 64;
    *z = 8;
    if (side == 0)
        --*y;
    if (side == 1)
        ++*y;
    if (side == 2)
        --*z;
    if (side == 3)
        ++*z;
    if (side == 4)
        --*x;
    if (side == 5)
        ++*x;
}

static bool power_source(fixture *f, size_t owner, int32_t direction, bool strong) {
    static const uint8_t ids[6] = {76, 70, 77, 77, 77, 77};
    static const int32_t metadata[6] = {0, 1, 12, 11, 10, 9};
    int32_t x, y, z;
    power_neighbor(direction, &x, &y, &z);
    return power_cell(f, owner, x, y, z, strong ? ids[direction] : 70,
                      strong ? metadata[direction] : 1);
}

static bool power_six_order(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    CHECK(f->registry[side]->blocks.m[255] == NULL);
    for (size_t mode = 0; mode < 2; ++mode)
        for (int32_t direction = 0; direction < 6; ++direction) {
            power_air(f, side);
            int32_t x, y, z;
            power_neighbor(direction, &x, &y, &z);
            CHECK(power_cell(f, side, x, y, z, 255, 0));
            if (direction < 5)
                CHECK(power_source(f, side, direction + 1, mode != 0));
            bool out = true;
            mc_world_power_error error;
            memset(&error, 0xa5, sizeof(error));
            const mc_world_storage_status status =
                mode == 0 ? mc_world_runtime_is_powered(w, 8, 64, 8, &out, &error)
                          : power_held(w, STRONG_NEIGHBORS, 8, 64, 8, 0, &out, &error);
            const mc_world_power_site *const parent =
                mode == 0 ? &error.weak_neighbors : &error.strong_neighbors;
            const mc_world_power_site *const leaf = mode == 0 ? &error.weak_at : &error.strong_at;
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && parent->entered &&
                  parent->bytecode == 7u + 15u * (unsigned int)direction && parent->side == -1 &&
                  parent->x == 8 && parent->y == 64 && parent->z == 8 && leaf->entered &&
                  leaf->bytecode == (mode == 0 ? 46u : 28u) && leaf->side == direction &&
                  leaf->x == x && leaf->y == y && leaf->z == z && !error.weak.entered &&
                  !error.strong.entered && !error.capability.entered &&
                  error.registry.status == MC_REGISTRY_OK && w->local_leases == 0 &&
                  w->active_calls == 0);
            if (direction > 0) {
                CHECK(power_source(f, side, direction - 1, mode != 0));
                CHECK(power_query_is(w, mode == 0 ? WEAK_NEIGHBORS : STRONG_NEIGHBORS, 8, 64, 8, 0,
                                     true));
            }
        }
    power_air(f, side);
    CHECK(power_query_is(w, WEAK_NEIGHBORS, 8, 64, 8, 0, false) &&
          power_query_is(w, STRONG_NEIGHBORS, 8, 64, 8, 0, false));
    /* The occupied real cache has native capacity1. A later different key
     * fails publication-room admission, but an earlier true skips it. */
    CHECK(power_cell(f, side, 15, 63, 8, 70, 1));
    bool out = false;
    mc_world_power_error error;
    CHECK(mc_world_runtime_is_powered(w, 15, 64, 8, &out, &error) == MC_WORLD_STORAGE_OK && out &&
          power_neutral(&error));
    CHECK(power_cell(f, side, 15, 63, 8, 0, 0));
    out = true;
    CHECK(mc_world_runtime_is_powered(w, 15, 64, 8, &out, &error) == MC_WORLD_STORAGE_LIMIT &&
          out && error.weak_neighbors.bytecode == 82 && error.weak_at.bytecode == 4 &&
          error.weak_at.x == 16 && error.weak_at.side == 5 && error.support.bytecode == 7 &&
          error.access.query.bytecode == 48 && error.access.query.provider_bytecode == 6 &&
          error.access.query.acquisition == MC_CHUNK_ACQUIRE_LIMIT &&
          error.access.query.registry.status == MC_REGISTRY_OK &&
          error.registry.status == MC_REGISTRY_OK && power_no_activation(&error.access) &&
          f->providers[side].cache->count == 1 && f->providers[side].cache->insertion_count == 1 &&
          mc_world_store_chunk_count(f->stores[side]) == 1 && !error.strong_neighbors.entered &&
          !error.strong_at.entered && !error.weak.entered && w->active_calls == 0);
    CHECK(power_cell(f, side, 15, 63, 8, 76, 0) &&
          power_query_is(w, STRONG_NEIGHBORS, 15, 64, 8, 0, true));
    CHECK(power_cell(f, side, 15, 63, 8, 0, 0));
    out = false;
    CHECK(power_held(w, STRONG_NEIGHBORS, 15, 64, 8, 0, &out, &error) == MC_WORLD_STORAGE_LIMIT &&
          !out && error.strong_neighbors.bytecode == 82 && error.strong_at.bytecode == 4 &&
          error.strong_at.x == 16 && error.strong_at.side == 5 &&
          error.access.query.acquisition == MC_CHUNK_ACQUIRE_LIMIT && error.support.bytecode == 0);
    ok = true;
cleanup:
    return ok;
}

static bool power_conductor_and_errors(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    power_air(f, side);
    CHECK(power_cell(f, side, 8, 63, 8, 1, 0) && power_cell(f, side, 8, 62, 8, 76, 0));
    CHECK(power_query_is(w, WEAK_AT, 8, 63, 8, INT32_MAX, true));
    bool out = false;
    mc_world_power_error error;
    CHECK(mc_world_runtime_is_powered(w, 8, 64, 8, &out, &error) == MC_WORLD_STORAGE_OK && out &&
          power_neutral(&error));
    mc_world_chunk_nibble *const e = f->chunks[side]->e;
    f->chunks[side]->e = NULL;
    const mc_world_storage_status nested = mc_world_runtime_is_powered(w, 8, 64, 8, &out, &error);
    f->chunks[side]->e = e;
    CHECK(nested == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          strcmp(error.world_class, side == 0 ? "fd" : "dj") == 0 &&
          error.weak_neighbors.bytecode == 7 && error.weak_at.bytecode == 14 &&
          error.strong_neighbors.bytecode == 7 && error.strong_at.bytecode == 28 &&
          error.weak_at.y == 63 && error.strong_neighbors.y == 63 && error.strong_at.y == 62 &&
          error.strong_at.side == 0 && error.strong.bytecode == 13 && error.weak.bytecode == 14 &&
          error.access.query.bytecode == 68 && error.access.query.chunk_bytecode == 7 &&
          error.access.query.acquisition == MC_CHUNK_ACQUIRE_OK &&
          error.registry.status == MC_REGISTRY_OK && !error.capability.entered &&
          !error.wire_connectivity.entered && power_no_activation(&error.access) &&
          w->local_leases == 0 && w->active_calls == 0);
    power_air(f, side);
    mc_block **const m = r->blocks.m;
    mc_world_provider *const provider = w->fields.v;
    r->blocks.m = NULL;
    w->fields.v = NULL;
    out = true;
    const mc_world_storage_status air_strong =
        power_held(w, STRONG_AT, 8, -1, 8, INT32_MIN, &out, &error);
    w->fields.v = provider;
    r->blocks.m = m;
    CHECK(air_strong == MC_WORLD_STORAGE_OK && !out && power_neutral(&error));
    r->blocks.m = NULL;
    w->fields.v = NULL;
    out = true;
    const mc_world_storage_status air_weak = power_held(w, WEAK_AT, 8, -1, 8, -1, &out, &error);
    w->fields.v = provider;
    r->blocks.m = m;
    CHECK(air_weak == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && error.weak_at.bytecode == 4 &&
          error.support.bytecode == 10 && error.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          error.access.query.bytecode == 0 && !error.strong_at.entered);
    /* NULL m capture succeeds. The reached getter failure precedes its array
     * access, and cannot inherit the earlier null-array Registry error. */
    r->blocks.m = NULL;
    w->fields.v = NULL;
    out = false;
    const mc_world_storage_status acquiring = power_held(w, WEAK_AT, 8, 64, 8, 0, &out, &error);
    w->fields.v = provider;
    r->blocks.m = m;
    CHECK(acquiring == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !out && error.weak_at.bytecode == 4 &&
          error.support.bytecode == 7 && error.access.query.bytecode == 48 &&
          error.access.query.provider_bytecode == 6 && error.access.query.chunk_bytecode == 0 &&
          error.registry.status == MC_REGISTRY_OK &&
          error.access.query.registry.status == MC_REGISTRY_OK &&
          error.registry.initializer_pc == -1 && error.access.slice == SIZE_MAX &&
          error.access.index == SIZE_MAX && error.access.activation_bytecode == 0 &&
          error.access.activation_world_bytecode == 0 && error.access.load_bytecode == 0 &&
          error.access.load_builder_bytecode == 0 && power_no_activation(&error.access));
    CHECK(power_cell(f, side, 8, 64, 8, 1, 0));
    r->blocks.m = NULL;
    out = true;
    const mc_world_storage_status array = power_held(w, STRONG_AT, 8, 64, 8, 0, &out, &error);
    r->blocks.m = m;
    CHECK(array == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && error.strong_at.bytecode == 21 &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE && !error.strong.entered);
    mc_block *const actual = m[1];
    m[1] = NULL;
    out = false;
    const mc_world_storage_status receiver = power_held(w, STRONG_AT, 8, 64, 8, 0, &out, &error);
    m[1] = actual;
    CHECK(receiver == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !out &&
          error.strong_at.bytecode == 28 && !error.strong.entered &&
          error.registry.status == MC_REGISTRY_OK);
    m[1] = NULL;
    out = true;
    const mc_world_storage_status weak_receiver = power_held(w, WEAK_AT, 8, 64, 8, 0, &out, &error);
    m[1] = actual;
    CHECK(weak_receiver == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          error.weak_at.bytecode == 46 && !error.weak.entered && error.support.bytecode == 0 &&
          error.registry.status == MC_REGISTRY_OK);
    power_air(f, side);
    const int32_t extremes[] = {INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < COUNT(extremes); ++i)
        CHECK(power_query_is(w, WEAK_NEIGHBORS, extremes[i], extremes[i], extremes[i], 0, false) &&
              power_query_is(w, STRONG_NEIGHBORS, extremes[i], extremes[i], extremes[i], 0, false));
    ok = true;
cleanup:
    return ok;
}

static bool power_wire_sides(fixture *f, size_t side, unsigned int horizontal) {
    for (int32_t direction = 0; direction < 6; ++direction)
        if (!power_query_is(f->worlds[side], WEAK_AT, 8, 64, 8, direction,
                            direction == 1 || ((horizontal >> (unsigned int)direction) & 1u)))
            return false;
    return true;
}

static bool power_wire_geometry(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    power_air(f, side);
    CHECK(power_cell(f, side, 8, 64, 8, 55, 15) && power_wire_sides(f, side, 60));
    CHECK(power_cell(f, side, 7, 64, 8, 55, 0) && power_wire_sides(f, side, 16));
    CHECK(power_cell(f, side, 9, 64, 8, 55, 0) && power_wire_sides(f, side, 48));
    CHECK(power_cell(f, side, 8, 64, 7, 55, 0) && power_wire_sides(f, side, 0));
    CHECK(power_cell(f, side, 7, 64, 8, 0, 0) && power_cell(f, side, 9, 64, 8, 0, 0) &&
          power_cell(f, side, 8, 64, 9, 55, 0) && power_wire_sides(f, side, 12));
    /* Below requires nonsolid adjacent; above requires support and open headroom. */
    power_air(f, side);
    CHECK(power_cell(f, side, 8, 64, 8, 55, 15) && power_cell(f, side, 7, 63, 8, 55, 0) &&
          power_wire_sides(f, side, 16));
    CHECK(power_cell(f, side, 7, 64, 8, 1, 0) && power_wire_sides(f, side, 60));
    CHECK(power_cell(f, side, 7, 63, 8, 0, 0) && power_cell(f, side, 7, 65, 8, 55, 0) &&
          power_wire_sides(f, side, 16));
    CHECK(power_cell(f, side, 8, 65, 8, 1, 0) && power_wire_sides(f, side, 60));
    /* True capability connects even with an ordinary false current output. */
    power_air(f, side);
    CHECK(power_cell(f, side, 8, 64, 8, 55, 15) && power_cell(f, side, 7, 64, 8, 70, 0) &&
          power_wire_sides(f, side, 16));
    const mc_registry_phase unrelated = r->direction_state.phase;
    const int32_t horizontal_b[4] = {2, 3, 0, 1};
    const uint8_t diode_ids[] = {93, 94};
    for (size_t i = 0; i < COUNT(diode_ids); ++i)
        for (int32_t metadata = 0; metadata < 16; ++metadata) {
            CHECK(power_cell(f, side, 7, 64, 8, diode_ids[i], metadata));
            CHECK(power_wire_sides(f, side, horizontal_b[metadata & 3] == 1 ? 16u : 60u));
        }
    CHECK(r->horizontal_state.phase == MC_REGISTRY_READY && r->horizontal_b != NULL &&
          r->direction_state.phase == unrelated);
    CHECK(power_cell(f, side, 7, 64, 8, 93, 3));
    int32_t *const b = r->horizontal_b;
    r->horizontal_b = NULL;
    bool out = true;
    mc_world_power_error error;
    memset(&error, 0xa5, sizeof(error));
    const mc_world_storage_status missing_b = power_held(w, STRONG_AT, 8, 64, 8, 4, &out, &error);
    r->horizontal_b = b;
    CHECK(missing_b == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          error.strong_at.bytecode == 28 && error.strong.bytecode == 17 &&
          error.weak.bytecode == 40 && error.wire_connectivity.bytecode == 87 &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          strcmp(error.wire_connectivity.declaring_class, side == 0 ? "sm" : "lo") == 0 &&
          !error.capability.entered && w->local_leases == 0 && w->active_calls == 0);
    ok = true;
cleanup:
    return ok;
}

static mc_block **power_named_cell(mc_registry_classes *r, uint8_t id) {
    mc_block **found = NULL;
    for (size_t i = 0; i < COUNT(r->blocks.named); ++i)
        if (r->blocks.named[i] == r->blocks.m[id]) {
            if (found != NULL)
                return NULL;
            found = &r->blocks.named[i];
        }
    return found;
}

static bool power_wire_namespace(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side], *other = f->registry[1u - side];
    mc_block *const wire = r->blocks.m[55];
    mc_block *const foreign_wire = other->blocks.m[55];
    mc_block **const aw = power_named_cell(r, 55);
    mc_block **const foreign_aw = power_named_cell(other, 55);
    CHECK(aw != NULL && foreign_aw != NULL && *aw == wire && *foreign_aw == foreign_wire);
    power_air(f, side);
    CHECK(power_cell(f, side, 8, 64, 8, 55, 15));
    bool out = false;
    mc_world_power_error error;
    *aw = NULL;
    const mc_world_storage_status foreign_success =
        power_block(w, foreign_wire, false, 4, &out, &error);
    *aw = wire;
    CHECK(foreign_success == MC_WORLD_STORAGE_OK && out && power_neutral(&error));
    *foreign_aw = NULL;
    out = false;
    const mc_world_storage_status own_success = power_block(w, wire, false, 4, &out, &error);
    *foreign_aw = foreign_wire;
    CHECK(own_success == MC_WORLD_STORAGE_OK && out && power_neutral(&error));
    *foreign_aw = NULL;
    out = false;
    const mc_world_storage_status foreign_failure =
        power_block(w, foreign_wire, true, 4, &out, &error);
    *foreign_aw = foreign_wire;
    CHECK(foreign_failure == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !out &&
          error.strong.bytecode == 17 && error.weak.bytecode == 40 &&
          error.wire_connectivity.bytecode == 16 && !error.weak_neighbors.entered &&
          strcmp(error.wire_connectivity.declaring_class, side == 0 ? "lo" : "sm") == 0 &&
          error.registry.status == MC_REGISTRY_OK && error.access.query.bytecode == 0);
    /* The live dust ID comparison precedes the air branch, even for ID0. */
    const int32_t bn = wire->fields.bn;
    wire->fields.bn = 0;
    out = true;
    const mc_world_storage_status dust_air = power_block(w, wire, false, 4, &out, &error);
    wire->fields.bn = bn;
    CHECK(dust_air == MC_WORLD_STORAGE_OK && !out && power_neutral(&error));
    mc_block **const bi = power_named_cell(r, 93), **const bj = power_named_cell(r, 94);
    CHECK(bi != NULL && bj != NULL);
    CHECK(power_cell(f, side, 7, 64, 8, 1, 0));
    mc_block *const actual_bi = *bi;
    *bi = NULL;
    out = true;
    const mc_world_storage_status first_diode = power_block(w, wire, false, 4, &out, &error);
    *bi = actual_bi;
    CHECK(first_diode == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && error.weak.bytecode == 40 &&
          error.wire_connectivity.bytecode == 50);
    CHECK(power_cell(f, side, 7, 64, 8, 94, 3));
    mc_block *const actual_bj = *bj;
    *bj = NULL;
    out = false;
    const mc_world_storage_status second_diode = power_block(w, wire, false, 4, &out, &error);
    *bj = actual_bj;
    CHECK(second_diode == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !out && error.weak.bytecode == 40 &&
          error.wire_connectivity.bytecode == 61);
    CHECK(power_cell(f, side, 7, 64, 8, 255, 0));
    out = true;
    const mc_world_storage_status capability_receiver =
        power_held(w, STRONG_AT, 8, 64, 8, 4, &out, &error);
    CHECK(capability_receiver == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          error.strong_at.bytecode == 28 && error.strong.bytecode == 17 &&
          error.weak.bytecode == 40 && error.wire_connectivity.bytecode == 37 &&
          !error.capability.entered && error.registry.status == MC_REGISTRY_OK);
    mc_block **const m = r->blocks.m;
    r->blocks.m = NULL;
    out = false;
    const mc_world_storage_status connectivity_array = power_block(w, wire, false, 4, &out, &error);
    r->blocks.m = m;
    CHECK(connectivity_array == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !out &&
          error.weak.bytecode == 40 && error.wire_connectivity.bytecode == 36 &&
          !error.capability.entered && error.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          error.registry.initializer_pc == -1 && !error.strong_at.entered &&
          !error.strong.entered && power_no_activation(&error.access));
    ok = true;
cleanup:
    return ok;
}

static bool power_lifetime(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    bool out = true;
    mc_world_power_error error;
    const mc_world_storage_error previous = *mc_world_runtime_last_error(w);
    CHECK(mc_world_runtime_is_powered(NULL, 0, 0, 0, &out, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out && !error.weak_neighbors.entered &&
          mc_world_runtime_is_powered(w, 0, 0, 0, NULL, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          !error.weak_neighbors.entered &&
          previous.status == mc_world_runtime_last_error(w)->status &&
          previous.operation == mc_world_runtime_last_error(w)->operation);
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    const bool busy =
        mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY &&
        mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY &&
        mc_world_runtime_classes_free(f->classes[side], NULL) == MC_WORLD_STORAGE_BUSY &&
        mc_registry_classes_free(f->registry[side], NULL) == MC_REGISTRY_BUSY &&
        w->active_calls == 1 && w->local_leases == 0;
    mc_world_runtime_call_leave(w);
    CHECK(busy && w->active_calls == 0);
    power_air(f, side);
    const bool remote = w->fields.B;
    mc_world_provider *const provider = w->fields.v;
    w->fields.B = true;
    w->fields.v = NULL;
    const mc_world_storage_status no_remote_guard =
        mc_world_runtime_is_powered(w, 8, 64, 8, &out, &error);
    w->fields.v = provider;
    w->fields.B = remote;
    CHECK(no_remote_guard == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          error.weak_neighbors.bytecode == 7 && error.weak_at.bytecode == 4 &&
          error.support.bytecode == 7 && error.access.query.provider_bytecode == 6 &&
          w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool power_retired(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_OK);
    f->terminal[side] = true;
    CHECK(retire_descriptors(f, side) &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_OK);
    f->world_retired[side] = true;
    const mc_world_storage_error previous = *mc_world_runtime_last_error(w);
    bool out = true;
    mc_world_power_error error;
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_world_runtime_is_powered(w, 8, 64, 8, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out && !error.weak_neighbors.entered && !error.weak_at.entered &&
          !error.strong_at.entered && !error.weak.entered &&
          error.registry.status == MC_REGISTRY_OK && w->active_calls == 0 &&
          previous.status == mc_world_runtime_last_error(w)->status &&
          previous.operation == mc_world_runtime_last_error(w)->operation);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    if (argc != 3 || strcmp(argv[1], "world-power") != 0 ||
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
    for (size_t side = 0; side < 2; ++side)
        CHECK(open_store(f, side, root) && create_world(f, side));
    for (size_t side = 0; side < 2; ++side) {
        mc_world_runtime *w = f->worlds[side];
        const mc_java_random random = *w->fields.r;
        const mc_world_reference_list *const r = w->fields.R, *const k = w->fields.K;
        const size_t bytes = w->construction.direct_allocation_bytes;
        mc_entity_class_snapshot before, after;
        CHECK(mc_entity_classes_get_snapshot(f->entity_classes[side], &before) == MC_ENTITY_OK);
        CHECK(power_capabilities(f, side) && power_emitter_matrix(f, side) &&
              power_guarded_metadata(f, side) && power_six_order(f, side) &&
              power_conductor_and_errors(f, side) && power_wire_geometry(f, side) &&
              power_wire_namespace(f, side) && power_lifetime(f, side));
        CHECK(mc_entity_classes_get_snapshot(f->entity_classes[side], &after) == MC_ENTITY_OK &&
              before.next_id == after.next_id &&
              before.bounds_pool_cursor == after.bounds_pool_cursor &&
              before.live_entities == after.live_entities &&
              before.allocation_bytes == after.allocation_bytes &&
              before.entity_class == after.entity_class &&
              before.bounds_class == after.bounds_class &&
              before.watcher_class == after.watcher_class &&
              mc_test_java_random_equal(w->fields.r, &random) && w->fields.R == r &&
              r->count == 0 && w->fields.K == k && k->count == 0 && w->active_calls == 0 &&
              w->local_leases == 0 && w->construction.direct_allocation_bytes == bytes &&
              f->providers[side].cache->count == 1 &&
              f->providers[side].cache->insertion_count == 1);
    }
    for (size_t side = 0; side < 2; ++side)
        CHECK(power_retired(f, side));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Power cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World power checks (world-power)\n", checks) < 0)
        result = 1;
    return result;
}
