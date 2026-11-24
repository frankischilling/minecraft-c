#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define MC_PICKING_BODY_MAIN_NAME mc_chest_texture_picking_fixture_main
int mc_chest_texture_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"

static void chest_set(picking_fixture *f, int32_t x, int32_t z, uint8_t id) {
    const size_t chunk = (size_t)((x >> 4) * 2 + (z >> 4));
    f->chunks[chunk]->data->blocks[((size_t)(x & 15) << 11u) | ((size_t)(z & 15) << 7u) | 64u] = id;
}

static int32_t chest_get(void *owner, int32_t x, int32_t y, int32_t z) {
    picking_fixture *f = owner;
    const size_t chunk = (size_t)((x >> 4) * 2 + (z >> 4));
    return f->chunks[chunk]
        ->data->blocks[((size_t)(x & 15) << 11u) | ((size_t)(z & 15) << 7u) | (size_t)y];
}

static unsigned chest_unused_queries;

static int32_t chest_unused_metadata(void *owner, int32_t x, int32_t y, int32_t z) {
    (void)owner;
    (void)x;
    (void)y;
    (void)z;
    ++chest_unused_queries;
    return 0;
}

static mc_material *chest_unused_material(void *owner, int32_t x, int32_t y, int32_t z) {
    (void)owner;
    (void)x;
    (void)y;
    (void)z;
    ++chest_unused_queries;
    return NULL;
}

static void chest_clear(picking_fixture *f) {
    for (size_t i = 0; i < 4; ++i)
        memset(f->chunks[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
}

typedef struct legacy_chest_probe {
    mc_block *block;
    size_t calls, mutate_at, swap_at;
    int32_t ids[6], coordinates[6][3], new_id, new_texture;
    bool *opacity;
} legacy_chest_probe;

static int32_t legacy_chest_get(void *owner, int32_t x, int32_t y, int32_t z) {
    legacy_chest_probe *p = owner;
    const size_t i = p->calls++;
    if (i >= 6)
        return 0;
    p->coordinates[i][0] = x;
    p->coordinates[i][1] = y;
    p->coordinates[i][2] = z;
    if (p->calls == p->mutate_at) {
        p->block->fields.bn = p->new_id;
        p->block->fields.bm = p->new_texture;
    }
    if (p->calls == p->swap_at)
        p->block->classes->blocks.o = p->opacity;
    return p->ids[i];
}

static bool legacy_chest_controls(picking_fixture *f, const mc_block *const blocks[2]) {
    bool clear[256] = {0}, east_solid[256] = {0};
    east_solid[1] = true;
    for (size_t kind = 0; kind < 2; ++kind) {
        mc_block *block = (mc_block *)blocks[kind];
        const mc_block_fields saved = block->fields;
        bool *opaque = block->classes->blocks.o;
        block->classes->blocks.o = clear;
        legacy_chest_probe p = {
            .block = block, .mutate_at = 4, .new_id = saved.bn, .new_texture = INT32_MAX};
        mc_block_render_access access = {.owner = &p,
                                         .block = legacy_chest_get,
                                         .metadata = chest_unused_metadata,
                                         .material = chest_unused_material};
        int32_t value = 123456;
        const mc_registry_status status =
            mc_block_render_texture(block, &access, INT32_MIN, 64, INT32_MAX, 3, &value);
        block->fields = saved;
        block->classes->blocks.o = opaque;
        CHECK(status == MC_REGISTRY_OK && value == INT32_MIN && p.calls == 4 &&
              p.coordinates[0][0] == INT32_MIN && p.coordinates[0][2] == INT32_MAX - 1 &&
              p.coordinates[1][2] == INT32_MIN && p.coordinates[2][0] == INT32_MAX &&
              p.coordinates[3][0] == INT32_MIN + 1);
        p = (legacy_chest_probe){.block = block};
        block->fields.bm = INT32_MIN;
        const mc_registry_status top = mc_block_render_texture(block, &access, 0, 0, 0, 1, &value);
        block->fields = saved;
        CHECK(top == MC_REGISTRY_OK && value == INT32_MAX && p.calls == 0);
    }
    mc_block *block = (mc_block *)blocks[0];
    const mc_block_fields saved = block->fields;
    bool *opaque = block->classes->blocks.o;
    for (size_t axis = 0; axis < 2; ++axis) {
        legacy_chest_probe p = {
            .block = block, .mutate_at = 5, .new_id = 99, .new_texture = INT32_MAX};
        p.ids[axis == 0 ? 0 : 2] = saved.bn;
        const mc_block_render_access access = {.owner = &p,
                                               .block = legacy_chest_get,
                                               .metadata = chest_unused_metadata,
                                               .material = chest_unused_material};
        block->classes->blocks.o = clear;
        int32_t value = 123456;
        const mc_registry_status status =
            mc_block_render_texture(block, &access, 8, 64, 8, axis == 0 ? 4 : 3, &value);
        block->fields = saved;
        block->classes->blocks.o = opaque;
        CHECK(status == MC_REGISTRY_OK && p.calls == 6 &&
              value == (axis == 0 ? INT32_MIN + 31 : INT32_MIN + 15) && p.coordinates[4][0] == 7 &&
              p.coordinates[4][2] == 7 && p.coordinates[5][0] == 9 && p.coordinates[5][2] == 9);
    }
    legacy_chest_probe p = {
        .block = block, .swap_at = 6, .opacity = east_solid, .ids = {54, 0, 0, 1, 0, 0}};
    const mc_block_render_access access = {.owner = &p,
                                           .block = legacy_chest_get,
                                           .metadata = chest_unused_metadata,
                                           .material = chest_unused_material};
    block->classes->blocks.o = clear;
    int32_t value = 123456;
    const mc_registry_status status = mc_block_render_texture(block, &access, 8, 64, 8, 4, &value);
    block->fields = saved;
    block->classes->blocks.o = opaque;
    CHECK(status == MC_REGISTRY_OK && p.calls == 6 && value == saved.bm + 16);
    (void)f;
    return true;
}

static bool chest_texture_controls(picking_fixture *f) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    mc_registry_classes *registry = f->base.registry;
    const mc_block *blocks[2] = {0};
    CHECK(mc_registry_block_get(registry, 54, &blocks[0], NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(registry, 95, &blocks[1], NULL) == MC_REGISTRY_OK);
    CHECK(blocks[0] != NULL && blocks[0]->kind == MC_BLOCK_CHEST && blocks[1] != NULL &&
          blocks[1]->kind == MC_BLOCK_LOCKED_CHEST);
    const mc_block_render_access oracle = {.owner = f,
                                           .block = chest_get,
                                           .metadata = chest_unused_metadata,
                                           .material = chest_unused_material};
    const int32_t sides[] = {-1, 0, 1, 2, 3, 4, 5, 6, INT32_MIN};
    const int32_t positions[8][2] = {{0, -1},  {0, 1},  {-1, 0}, {1, 0},
                                     {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    size_t rows = 0;
    for (size_t kind = 0; kind < 2; ++kind)
        for (unsigned profile = 0; profile < 81; ++profile) {
            chest_clear(f);
            unsigned digits = profile;
            for (size_t i = 0; i < 4; ++i) {
                const uint8_t id = digits % 3 == 0   ? 0
                                   : digits % 3 == 1 ? 1
                                                     : (uint8_t)(kind ? 95 : 54);
                digits /= 3;
                chest_set(f, 16 + positions[i][0], 16 + positions[i][1], id);
            }
            for (size_t i = 4; i < 8; ++i)
                chest_set(f, 16 + positions[i][0], 16 + positions[i][1],
                          profile & (1u << (i - 4)) ? 1 : 0);
            for (size_t side = 0; side < sizeof sides / sizeof *sides; ++side) {
                int32_t expected, actual = 123456;
                CHECK(mc_block_render_texture(blocks[kind], &oracle, 16, 64, 16, sides[side],
                                              &expected) == MC_REGISTRY_OK);
                mc_client_render_blocks fields;
                mc_client_render_blocks_construct_fields(&fields, cache, true);
                mc_cv_context c = {.fields = &fields, .cache = cache, .registry = registry};
                CHECK(mc_cv_push(&c, "b", "(Luu;III)Z") &&
                      mc_cv_texture(&c, blocks[kind], 16, 64, 16, sides[side], 777, &actual) &&
                      actual == expected && c.depth == 1 && !c.failed);
                ++rows;
            }
        }

    /* Each missing actual lm block plane is chosen so its first reached EW
     * query is exactly the selected E call. If a failed query were ignored,
     * the later deliberately NULL uu.o would replace the first failure. */
    static const struct {
        int32_t x, z, chunk, join;
        unsigned int pc;
    } faults[] = {{16, 16, 2, 0, 32},  {16, 16, 3, 0, 46},  {16, 16, 1, 0, 60},
                  {15, 16, 3, 0, 74},  {16, 16, 0, 1, 156}, {15, 16, 2, 1, 188},
                  {16, 16, 0, 2, 390}, {16, 15, 1, 2, 421}};

    for (size_t i = 0; i < sizeof faults / sizeof *faults; ++i) {
        chest_clear(f);
        if (faults[i].join == 1)
            chest_set(f, faults[i].x, faults[i].z - 1, 54);
        else if (faults[i].join == 2)
            chest_set(f, faults[i].x - 1, faults[i].z, 54);
        mc_client_render_blocks fields;
        mc_client_render_blocks_construct_fields(&fields, cache, true);
        mc_cv_context c = {.fields = &fields, .cache = cache, .registry = registry};
        CHECK(mc_cv_push(&c, "b", "(Luu;III)Z"));
        uint8_t *plane = f->chunks[faults[i].chunk]->data->blocks;
        bool *opaque = registry->blocks.o;
        f->chunks[faults[i].chunk]->data->blocks = NULL;
        registry->blocks.o = NULL;
        int32_t untouched = 123456;
        const bool complete = mc_cv_texture(&c, blocks[0], faults[i].x, 64, faults[i].z,
                                            faults[i].join == 1 ? 4 : 2, 777, &untouched);
        f->chunks[faults[i].chunk]->data->blocks = plane;
        registry->blocks.o = opaque;
        CHECK(!complete && untouched == 123456 && c.failed && c.depth == 2 &&
              c.frames[0].pc == 777 && strcmp(c.frames[1].class_name, "e") == 0 &&
              c.frames[1].pc == faults[i].pc && c.source_pc == faults[i].pc &&
              c.cache_error.status != MC_CLIENT_CHUNK_CACHE_OK);
    }
    chest_clear(f);
    for (size_t kind = 0; kind < 2; ++kind) {
        mc_client_render_blocks fields;
        mc_client_render_blocks_construct_fields(&fields, cache, true);
        mc_cv_context c = {.fields = &fields, .cache = cache, .registry = registry};
        bool *opaque = registry->blocks.o;
        registry->blocks.o = NULL;
        int32_t value = 123456;
        const bool complete = mc_cv_texture(&c, blocks[kind], 16, 64, 16, 3, 777, &value);
        registry->blocks.o = opaque;
        CHECK(!complete && value == 123456 &&
              c.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
              !c.failure.native_policy && c.source_pc == (kind ? 89u : 557u));
        mc_client_render_blocks_construct_fields(&fields, NULL, true);
        c = (mc_cv_context){.fields = &fields, .cache = cache, .registry = registry};
        CHECK(mc_cv_texture(&c, blocks[kind], 16, 64, 16, 1, 777, &value) && value == 25);
        c = (mc_cv_context){.fields = &fields, .cache = cache, .registry = registry};
        value = 123456;
        CHECK(!mc_cv_texture(&c, blocks[kind], 16, 64, 16, -1, 777, &value) && value == 123456 &&
              c.source_pc == 32 && !c.failure.native_policy);
    }
    CHECK(chest_unused_queries == 0);
    CHECK(legacy_chest_controls(f, blocks));
    CHECK(mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    printf("Chest checked texture: %zu shared scalar projections;8 actual EW failure prefixes\n",
           rows);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !chest_texture_controls(f) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Chest texture controls passed: %u\n", checks);
    return 0;
}
