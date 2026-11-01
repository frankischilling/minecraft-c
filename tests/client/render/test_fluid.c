#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Genuine prepared Registry/World/EW/NW owners; the included main is unused. */
#define MC_PICKING_BODY_MAIN_NAME mc_fluid_picking_fixture_main
int mc_fluid_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/nw.h"
#include "world/chunk/object_state_internal.h"

static uint32_t fluid_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool fluid_prepare(mc_chunk_state *chunk, int32_t id, int32_t metadata, bool enclosed) {
    memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->skylight, 0xff, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    for (int32_t x = 7; x <= 9; ++x)
        for (int32_t y = enclosed ? 63 : 64; y <= (enclosed ? 65 : 64); ++y)
            for (int32_t z = 7; z <= 9; ++z) {
                chunk->data->blocks[((uint32_t)x << 11) | ((uint32_t)z << 7) | (uint32_t)y] =
                    (uint8_t)id;
                CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (uint8_t)metadata));
            }
    return true;
}

static bool fluid_controls(picking_fixture *f) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f->models, &model_error));
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(
        f->graphics, (mc_client_graphics_site){"native-test", "fluid", "()V", -1}, &nw, &graphics));
    CHECK(mc_client_nw_begin_quads(nw, &graphics));
    f->app->source_math_classes = f->math;
    for (int32_t id = 8; id <= 11; ++id) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(f->base.registry, id, &block, NULL) == MC_REGISTRY_OK &&
              block != NULL);
        const mc_block_fields before_block = block->fields;
        CHECK(fluid_prepare(f->chunks[0], id, 0, false));
        mc_client_render_blocks fields;
        mc_client_render_blocks_construct_fields(&fields, cache, true);
        fields.f = true;
        fields.m = true;
        fields.d = 17; /* Fluid top/sides ignore this; bottom's actual face honors it. */
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = f->base.registry,
                                 .models = f->models,
                                 .graphics = f->graphics,
                                 .application = f->app};
        CHECK(mc_cv_push(&context, "k", "(Luu;III)Z"));
        mc_client_nw_state state;
        mc_entity_vector_snapshot pool_before, pool_after;
        CHECK(mc_client_nw_inspect(f->graphics, &state) &&
              mc_entity_vector_get_snapshot(f->base.entities, &pool_before) == MC_ENTITY_OK);
        const int32_t first = state.fields.o;
        bool rendered = false;
        CHECK(mc_cv_fluid(&context, block, 8, 64, 8, &rendered) && rendered && !context.failed &&
              context.depth == 1 && context.source_pc == 1244 && context.last_store_pc == 1239);
        CHECK(mc_client_nw_inspect(f->graphics, &state) &&
              mc_entity_vector_get_snapshot(f->base.entities, &pool_after) == MC_ENTITY_OK &&
              pool_after.cursor == pool_before.cursor + 6 && state.fields.o == first + 36 * 8 &&
              state.fields.w && state.fields.r == 7);
        CHECK(fields.c == cache && fields.f && fields.m && fields.d == 17 &&
              block->fields.bt == 0 && block->fields.bw == 1 &&
              block->fields.bs == before_block.bs && block->fields.bu == before_block.bu &&
              block->fields.bv == before_block.bv && block->fields.bx == before_block.bx);
        /* Independent still-source oracle: homogeneous level0 is height8/9.
         * The stored Y word includes the source i2f/fadd before widening, so
         * checking 0x4281c71c also detects an accidental binary64 position sum. */
        const int32_t tile_x = (block->fields.bm & 15) << 4;
        const int32_t tile_y = block->fields.bm & 240;
        const float u0 = (float)tile_x / 256.0f, u1 = (float)(tile_x + 16) / 256.0f;
        const float v0 = (float)tile_y / 256.0f, v1 = (float)(tile_y + 16) / 256.0f;
        const float positions[4][2] = {{8, 8}, {8, 9}, {9, 9}, {9, 8}};
        const float uv[4][2] = {{u0, v0}, {u0, v1}, {u1, v1}, {u1, v0}};
        static const size_t triangle[] = {0, 1, 2, 0, 2, 3};
        for (size_t t = 0; t < 6; ++t) {
            const size_t i = triangle[t];
            const int32_t *word = state.fields.g + first + (int32_t)(t * 8);
            CHECK((uint32_t)word[0] == fluid_bits(positions[i][0]) &&
                  (uint32_t)word[1] == UINT32_C(0x4281c71c) &&
                  (uint32_t)word[2] == fluid_bits(positions[i][1]) &&
                  (uint32_t)word[3] == fluid_bits(uv[i][0]) &&
                  (uint32_t)word[4] == fluid_bits(uv[i][1]));
        }
        CHECK(fluid_prepare(f->chunks[0], id, 0, true));
        fields.f = false;
        ((mc_block *)block)->fields.bt = .25;
        ((mc_block *)block)->fields.bw = .75;
        mc_cv_context hidden = {.fields = &fields,
                                .cache = cache,
                                .registry = f->base.registry,
                                .models = f->models,
                                .graphics = f->graphics,
                                .application = f->app};
        CHECK(mc_cv_push(&hidden, "k", "(Luu;III)Z"));
        const int32_t unchanged = state.fields.o;
        CHECK(mc_entity_vector_get_snapshot(f->base.entities, &pool_before) == MC_ENTITY_OK);
        rendered = true;
        CHECK(mc_cv_fluid(&hidden, block, 8, 64, 8, &rendered) && !rendered && !hidden.failed &&
              hidden.source_pc == 215 && hidden.last_store_pc == 0);
        CHECK(mc_client_nw_inspect(f->graphics, &state) && state.fields.o == unchanged &&
              mc_entity_vector_get_snapshot(f->base.entities, &pool_after) == MC_ENTITY_OK &&
              pool_after.cursor == pool_before.cursor && block->fields.bt == .25 &&
              block->fields.bw == .75);
        ((mc_block *)block)->fields = before_block;
    }
    /* The first source NW active use precedes a null Block color receiver. */
    mc_client_render_blocks fields;
    mc_client_render_blocks_construct_fields(&fields, cache, true);
    mc_cv_context null = {.fields = &fields,
                          .cache = cache,
                          .registry = f->base.registry,
                          .models = f->models,
                          .graphics = f->graphics,
                          .application = f->app};
    CHECK(mc_cv_push(&null, "k", "(Luu;III)Z"));
    bool output = true;
    CHECK(!mc_cv_fluid(&null, NULL, 8, 64, 8, &output) && output && null.failed &&
          null.tessellator == nw && null.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          !null.failure.native_policy && null.source_pc == 14);
    f->app->source_math_classes = NULL;
    CHECK(mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !fluid_controls(f) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf(
        "Source fluid: %u checks; homogeneous water/lava geometry, hidden faces and pool effects\n",
        checks);
    return 0;
}
