/* Genuine saved-World fixtures; the original picking main is not executed. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define MC_PICKING_BODY_MAIN_NAME mc_chunk_compile_picker_fixture_main
int mc_chunk_compile_picker_fixture_main(int argc, char **argv);
#include "test_picking.c"
#undef main

#include "client/display/source_surface.h"
#include "client/render/blocks/internal.h"
#include "client/render_engine/chunk_statistics.h"
#include "client/render_engine/internal.h"
#include "client/world/render_global_access.h"
#include "client/world/source_chunk_compile_internal.h"
#include "mc/client/world/render_global_update.h"
#include "mc/world/runtime/propagation.h"
#include "world/chunk/object_state_internal.h"

static bool bound_dispatch_controls(picking_fixture *f) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    CHECK(mc_client_render_blocks_active_use(f->models, NULL));
    const size_t index = (8u << 11u) | (8u << 7u) | 64u;
    const uint8_t previous_metadata = f->chunks[0]->data->metadata[index / 2u];
    const int32_t ids[] = {27, 28, 66, 26};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(f->base.registry, ids[i], &block, NULL) == MC_REGISTRY_OK &&
              block != NULL);
        const mc_block_fields previous = *mc_block_fields_get(block);
        for (int32_t metadata = 0; metadata < 16; ++metadata) {
            CHECK(mc_chunk_nibbles_set(f->chunks[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64,
                                       8, metadata));
            mc_client_render_blocks fields;
            mc_client_render_blocks_construct_fields(&fields, cache, true);
            mc_cv_context c = {.fields = &fields, .cache = cache, .registry = f->base.registry};
            bool out = true;
            CHECK(!mc_cv_render(&c, block, 8, 64, 8, &out) && out && c.failed &&
                  c.failure.status == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY &&
                  c.failure.source_pc == (ids[i] == 26 ? 265u : 197u));
            const mc_block_fields *actual = mc_block_fields_get(block);
            CHECK(actual->bs == 0 && actual->bt == 0 && actual->bu == 0 && actual->bv == 1 &&
                  actual->bx == 1 &&
                  actual->bw == (ids[i] == 26                     ? .5625
                                 : metadata >= 2 && metadata <= 5 ? .625
                                                                  : .125));
        }
        if (ids[i] != 26) {
            CHECK(mc_block_render_bounds((mc_block *)block, .25f, .25f, .25f, .75f, .75f, .75f) ==
                  MC_REGISTRY_OK);
            mc_client_render_blocks fields;
            mc_client_render_blocks_construct_fields(&fields, NULL, true);
            mc_cv_context c = {.fields = &fields, .cache = cache, .registry = f->base.registry};
            bool out = true;
            CHECK(!mc_cv_render(&c, block, 8, 64, 8, &out) && out && c.depth == 2 &&
                  c.frames[0].pc == 15 && strcmp(c.failure.source_class, "pc") == 0 &&
                  c.failure.source_pc == 5 && !c.failure.native_policy &&
                  c.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                  mc_block_fields_get(block)->bw == .75);
        }
        CHECK(mc_block_render_bounds((mc_block *)block, (float)previous.bs, (float)previous.bt,
                                     (float)previous.bu, (float)previous.bv, (float)previous.bw,
                                     (float)previous.bx) == MC_REGISTRY_OK);
    }
    f->chunks[0]->data->metadata[index / 2u] = previous_metadata;
    const int32_t chest_ids[] = {54, 95};
    const char *const chest_classes[] = {"e", "ks"};
    for (size_t i = 0; i < 2; ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(f->base.registry, chest_ids[i], &block, NULL) ==
                  MC_REGISTRY_OK &&
              block != NULL);
        mc_client_render_blocks fields;
        mc_client_render_blocks_construct_fields(&fields, cache, true);
        mc_cv_context c = {.fields = &fields, .cache = cache, .registry = f->base.registry};
        int32_t out = 123;
        CHECK(mc_cv_push(&c, "b", "(Luu;IIIFFF)Z"));
        CHECK(!mc_cv_texture(&c, block, 8, 64, 8, 2, 454, &out) && out == 123 && c.depth == 2 &&
              c.frames[0].pc == 454 && strcmp(c.failure.source_class, chest_classes[i]) == 0 &&
              c.failure.source_pc == 0 &&
              c.failure.status == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY);
    }
    CHECK(mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    return true;
}

static bool cactus_list(mc_client_display *display, int32_t id, int32_t vertices,
                        const uint8_t **identity) {
    mc_client_source_list_state list;
    mc_client_source_batch_view batch;
    CHECK(mc_client_display_list_inspect(display, id, &list) && list.defined && !list.building &&
          list.commands == 7 && list.draws == 1 && list.vertices == (size_t)vertices &&
          mc_client_display_list_batch(display, id, 0, &batch) && batch.vertices == vertices &&
          batch.mode == 4 && batch.arrays == 7 && batch.byte_count == (size_t)vertices * 32u);
    size_t inset_vertices = 0;
    for (size_t i = (size_t)vertices - 30u; i < (size_t)vertices; ++i) {
        float vertex[5];
        memcpy(vertex, batch.bytes + i * 32u, sizeof(vertex));
        CHECK(vertex[0] >= 0 && vertex[0] <= 1 && vertex[1] >= 1 && vertex[1] <= 2 &&
              vertex[2] >= 0 && vertex[2] <= 1);
        const bool top = i < (size_t)vertices - 24u;
        CHECK(vertex[3] >= (top ? 5.0f : 6.0f) / 16.0f && vertex[3] < (top ? 6.0f : 7.0f) / 16.0f &&
              vertex[4] >= .25f && vertex[4] < .3125f);
        if (top)
            CHECK(vertex[1] == 2);
        else {
            CHECK(vertex[0] == .0625f || vertex[0] == .9375f || vertex[2] == .0625f ||
                  vertex[2] == .9375f);
            ++inset_vertices;
        }
    }
    CHECK(inset_vertices == 24);
    *identity = batch.bytes;
    return true;
}

static bool cactus_compile_controls(picking_fixture *f, mc_client_render_global *n,
                                    mc_client_source_dk *dk, int32_t base) {
    const size_t sand_index = (1u << 11u) | (1u << 7u) | 64u;
    const size_t cactus_index = sand_index + 1u;
    const uint8_t previous_sand = f->chunks[0]->data->blocks[sand_index];
    const uint8_t previous_cactus = f->chunks[0]->data->blocks[cactus_index];
    const mc_block *sand = NULL, *cactus = NULL;
    mc_block_render_description description;
    CHECK(mc_registry_block_get(f->base.registry, 12, &sand, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->base.registry, 81, &cactus, NULL) == MC_REGISTRY_OK &&
          sand != NULL && cactus != NULL && mc_block_render_opaque(sand) &&
          !mc_block_render_opaque(cactus) &&
          mc_block_render_describe(cactus, &description) == MC_REGISTRY_OK &&
          description.type == 13 && description.pass == 0);
    const mc_block_fields cactus_before = *mc_block_fields_get(cactus);
    CHECK(cactus_before.bs == 0 && cactus_before.bt == 0 && cactus_before.bu == 0 &&
          cactus_before.bv == 1 && cactus_before.bw == 1 && cactus_before.bx == 1 &&
          cactus_before.bm == 70);
    f->chunks[0]->data->blocks[sand_index] = 12;
    f->chunks[0]->data->blocks[cactus_index] = 81;
    int32_t entered_cactus_id = -1;
    mc_world_storage_error world_error;
    CHECK(mc_world_runtime_block_get(f->base.world, 1, 65, 1, &entered_cactus_id, &world_error) ==
              MC_WORLD_STORAGE_OK &&
          entered_cactus_id == 81);
    dk->fields.g = 2;
    dk->fields.r = 66;
    const size_t bindings = f->base.world->persistent_bindings;
    mc_client_render_global_error error;
    mc_client_render_global_update_call *call = NULL;
    mc_client_render_global_update_state state;
    uintptr_t published = 0;
    for (size_t attempt = 0; attempt < 2; ++attempt) {
        /* Invoke the actual DK.f dirty setter and N's real queue append. The
         * enclosing one-DK graph remains this fixture's declared native input. */
        mc_source_dk_dirty(dk);
        CHECK(dk->fields.u && dk->last_store_pc == 2 &&
              mc_render_global_pointer_append(
                  n, n->fields.m, dk, (mc_client_graphics_site){"n", "a", "()V", 571}, &error));
        CHECK(mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
        CHECK(mc_client_render_global_update_inspect(call, &state) && state.returned);
        CHECK(state.compile_attempts == 1 && state.completed_compiles == 1 &&
              state.remaining_count == 0 && state.compilation != NULL);
        CHECK(state.compilation->returned && state.compilation->pass == 0 &&
              state.compilation->pass_rendered && state.compilation->render_blocks != NULL);
        /* DK scans the entire 2x2x2 section, ending on an Air cell. Its CV
         * receipt still belongs to the last nonair call, Cactus at return65. */
        CHECK(state.compilation->block_id == 0 && state.compilation->render_blocks->returned &&
              state.compilation->render_blocks->source_pc == 65 &&
              !state.compilation->render_blocks->failed);
        CHECK(!state.compilation->list_open && !state.compilation->matrix_pushed &&
              !state.compilation->tessellator_started && !dk->fields.u && dk->fields.E &&
              !dk->fields.p[0] && dk->fields.p[1]);
        const uint8_t *current = NULL;
        CHECK(cactus_list(n->display, base, 66, &current));
        if (attempt != 0)
            CHECK((uintptr_t)current != published);
        published = (uintptr_t)current;
        printf("DK_CACTUS stage=%s block=%d last-scanned=%d cv-return=%u pass=%d vertices=66 "
               "inset-vertices=24 dirty=%d "
               "pass0-empty=%d pass1-empty=%d\n",
               attempt == 0 ? "compile" : "dirty", entered_cactus_id, state.compilation->block_id,
               state.compilation->render_blocks->source_pc, state.compilation->pass, dk->fields.u,
               dk->fields.p[0], dk->fields.p[1]);
        CHECK(mc_client_render_global_update_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              f->base.world->persistent_bindings == bindings);
        if (attempt == 0) {
            CHECK(mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK);
            CHECK(mc_client_render_global_update_inspect(call, &state) && state.returned &&
                  state.compile_attempts == 0 && state.completed_compiles == 0 &&
                  state.compilation == NULL && state.remaining_count == 0 && !dk->fields.u);
            CHECK(cactus_list(n->display, base, 66, &current) && (uintptr_t)current == published);
            puts("DK_CACTUS stage=reuse attempts=0 same-definition=1 vertices=66");
            CHECK(mc_client_render_global_update_retire(&call, &error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK);
        }
    }
    const mc_block_fields *after = mc_block_fields_get(cactus);
    mc_client_nw_state nw;
    CHECK(after->bs == cactus_before.bs && after->bt == cactus_before.bt &&
          after->bu == cactus_before.bu && after->bv == cactus_before.bv &&
          after->bw == cactus_before.bw && after->bx == cactus_before.bx &&
          mc_client_nw_inspect(f->graphics, &nw) && !nw.fields.w && nw.fields.s == 0 &&
          nw.fields.t == 0 && nw.fields.u == 0);
    /* Restore the original one-Stone input through a genuine replacement
     * compile, preserving the following existing failure's exact list oracle. */
    f->chunks[0]->data->blocks[sand_index] = previous_sand;
    f->chunks[0]->data->blocks[cactus_index] = previous_cactus;
    dk->fields.g = 1;
    dk->fields.r = 65;
    mc_source_dk_dirty(dk);
    CHECK(mc_render_global_pointer_append(
              n, n->fields.m, dk, (mc_client_graphics_site){"n", "a", "()V", 571}, &error) &&
          mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_update_inspect(call, &state) && state.returned &&
          state.completed_compiles == 1 && !dk->fields.u);
    mc_client_source_list_state restored;
    CHECK(mc_client_display_list_inspect(n->display, base, &restored) && restored.defined &&
          !restored.building && restored.commands == 7 && restored.draws == 1 &&
          restored.vertices == 36 &&
          mc_client_render_global_update_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          f->base.world->persistent_bindings == bindings);
    return true;
}

/* Independent source shape: cap and four crossing side quads. DK's NW
 * translation rebases (1,65,1) to (0,1,0); no Block bounds enter this shape. */
static bool torch_list(mc_client_display *display, int32_t id, int32_t metadata,
                       const uint8_t **identity) {
    mc_client_source_list_state list;
    mc_client_source_batch_view batch;
    CHECK(mc_client_display_list_inspect(display, id, &list) && list.defined && !list.building &&
          list.commands == 7 && list.draws == 1 && list.vertices == 66 &&
          mc_client_display_list_batch(display, id, 0, &batch) && batch.vertices == 66 &&
          batch.mode == 4 && batch.arrays == 7 && batch.byte_count == 66u * 32u);
    static const double torch_shape[20][3] = {
        {.4375, .625, .4375}, {.4375, .625, .5625}, {.5625, .625, .5625}, {.5625, .625, .4375},
        {.4375, 1, 0},        {.4375, 0, 0},        {.4375, 0, 1},        {.4375, 1, 1},
        {.5625, 1, 1},        {.5625, 0, 1},        {.5625, 0, 0},        {.5625, 1, 0},
        {0, 1, .5625},        {0, 0, .5625},        {1, 0, .5625},        {1, 1, .5625},
        {1, 1, .4375},        {1, 0, .4375},        {0, 0, .4375},        {0, 1, .4375}};
    static const size_t torch_triangles[6] = {0, 1, 2, 0, 2, 3};
    const double tilt = (double).4f, wall_offset = .5 - tilt;
    const double tilt_x = metadata == 1 ? -tilt : metadata == 2 ? tilt : 0;
    const double tilt_z = metadata == 3 ? -tilt : metadata == 4 ? tilt : 0;
    const double dx = metadata == 1 ? -wall_offset : metadata == 2 ? wall_offset : 0;
    const double dz = metadata == 3 ? -wall_offset : metadata == 4 ? wall_offset : 0;
    const double dy = metadata >= 1 && metadata <= 4 ? (double).2f : 0;
    for (size_t vertex_index = 0; vertex_index < 66; ++vertex_index) {
        float vertex[5];
        memcpy(vertex, batch.bytes + vertex_index * 32u, sizeof(vertex));
        if (vertex_index < 36) {
            CHECK(vertex[0] >= 0 && vertex[0] <= 1 && vertex[1] >= 0 && vertex[1] <= 1 &&
                  vertex[2] >= 0 && vertex[2] <= 1 && vertex[3] >= .125f && vertex[3] < .1875f &&
                  vertex[4] >= .0625f && vertex[4] < .125f);
            continue;
        }
        const size_t torch_vertex = vertex_index - 36;
        const size_t torch_quad = torch_vertex / 6,
                     torch_corner = torch_triangles[torch_vertex % 6];
        const double *torch_point = torch_shape[torch_quad * 4 + torch_corner];
        CHECK(vertex[0] == (float)(dx + torch_point[0] + tilt_x * (1 - torch_point[1])) &&
              vertex[1] == (float)(1 + dy + torch_point[1]) &&
              vertex[2] == (float)(dz + torch_point[2] + tilt_z * (1 - torch_point[1])));
        const bool high_u = torch_corner >= 2, high_v = torch_corner == 1 || torch_corner == 2;
        const float u0 = torch_quad == 0 ? 7.0f / 256 : 0;
        const float u1 = torch_quad == 0 ? 9.0f / 256 : 15.99f / 256;
        const float v0 = torch_quad == 0 ? 86.0f / 256 : 5.0f / 16;
        const float v1 = torch_quad == 0 ? 88.0f / 256 : (80.0f + 15.99f) / 256;
        uint32_t packed_color;
        memcpy(&packed_color, batch.bytes + vertex_index * 32u + 20u, sizeof(packed_color));
        CHECK(vertex[3] == (high_u ? u1 : u0) && vertex[4] == (high_v ? v1 : v0) &&
              packed_color == UINT32_MAX);
    }
    *identity = batch.bytes;
    return true;
}

static bool torch_compile_controls(picking_fixture *f, mc_client_render_global *n,
                                   mc_client_source_dk *dk, int32_t base) {
    const size_t sand_index = (1u << 11u) | (1u << 7u) | 64u;
    const size_t torch_index = sand_index + 1u;
    const uint8_t previous_sand = f->chunks[0]->data->blocks[sand_index];
    const uint8_t previous_torch = f->chunks[0]->data->blocks[torch_index];
    const uint8_t previous_metadata = f->chunks[0]->data->metadata[torch_index / 2u];
    const mc_block *sand = NULL, *torch = NULL;
    mc_block_render_description description;
    mc_registry_block_view registry;
    CHECK(mc_registry_block_get(f->base.registry, 12, &sand, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->base.registry, 50, &torch, NULL) == MC_REGISTRY_OK &&
          sand != NULL && torch != NULL && mc_block_render_opaque(sand) &&
          !mc_block_render_opaque(torch) &&
          mc_block_render_describe(torch, &description) == MC_REGISTRY_OK &&
          description.type == 2 && description.pass == 0 &&
          mc_registry_classes_inspect_blocks(f->base.registry, &registry, NULL) == MC_REGISTRY_OK &&
          registry.s != NULL && registry.s[50] > 0);
    const mc_block_fields before = *mc_block_fields_get(torch);
    CHECK(before.bn == 50 && before.bm == 80);
    f->chunks[0]->data->blocks[sand_index] = 12;
    f->chunks[0]->data->blocks[torch_index] = 50;
    dk->fields.g = 2;
    dk->fields.r = 66;
    const size_t bindings = f->base.world->persistent_bindings;
    mc_client_render_global_error error;
    mc_client_render_global_update_call *call = NULL;
    mc_client_render_global_update_state state;
    for (int32_t metadata = 1; metadata <= 5; ++metadata) {
        CHECK(mc_chunk_nibbles_set(f->chunks[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 1, 65, 1,
                                   metadata));
        int32_t actual_id = -1, actual_metadata = -1;
        mc_world_storage_error world_error;
        CHECK(mc_world_runtime_block_get(f->base.world, 1, 65, 1, &actual_id, &world_error) ==
                  MC_WORLD_STORAGE_OK &&
              actual_id == 50 &&
              mc_world_runtime_metadata_get(f->base.world, 1, 65, 1, &actual_metadata,
                                            &world_error) == MC_WORLD_STORAGE_OK &&
              actual_metadata == metadata);
        uintptr_t published = 0;
        uint8_t published_bytes[66u * 32u];
        for (size_t attempt = 0; attempt < 2; ++attempt) {
            mc_source_dk_dirty(dk);
            CHECK(dk->fields.u && dk->last_store_pc == 2 &&
                  mc_render_global_pointer_append(n, n->fields.m, dk,
                                                  (mc_client_graphics_site){"n", "a", "()V", 571},
                                                  &error) &&
                  mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
                      MC_CLIENT_RENDER_GLOBAL_OK &&
                  mc_client_render_global_update_inspect(call, &state) && state.returned &&
                  state.compile_attempts == 1 && state.completed_compiles == 1 &&
                  state.remaining_count == 0 && state.compilation != NULL);
            CHECK(state.compilation->returned && state.compilation->pass == 0 &&
                  state.compilation->pass_rendered && state.compilation->block_id == 0 &&
                  state.compilation->render_blocks != NULL &&
                  state.compilation->render_blocks->returned &&
                  state.compilation->render_blocks->source_pc == 114 &&
                  !state.compilation->render_blocks->failed && !state.compilation->list_open &&
                  !state.compilation->matrix_pushed && !state.compilation->tessellator_started &&
                  !dk->fields.u && dk->fields.E && !dk->fields.p[0] && dk->fields.p[1]);
            const uint8_t *current = NULL;
            CHECK(torch_list(n->display, base, metadata, &current));
            if (attempt != 0)
                CHECK((uintptr_t)current != published);
            published = (uintptr_t)current;
            memcpy(published_bytes, current, sizeof(published_bytes));
            printf(
                "DK_TORCH stage=%s block=%d metadata=%d cv-return=114 pass=0 vertices=66 "
                "torch-vertices=30 exact-position-uv-color=1 dirty=0 pass0-empty=0 pass1-empty=1\n",
                attempt == 0 ? "compile" : "dirty", actual_id, actual_metadata);
            CHECK(mc_client_render_global_update_retire(&call, &error) ==
                      MC_CLIENT_RENDER_GLOBAL_OK &&
                  f->base.world->persistent_bindings == bindings);
            if (attempt == 0) {
                CHECK(mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
                          MC_CLIENT_RENDER_GLOBAL_OK &&
                      mc_client_render_global_update_inspect(call, &state) && state.returned &&
                      state.compile_attempts == 0 && state.completed_compiles == 0 &&
                      state.compilation == NULL && state.remaining_count == 0 && !dk->fields.u &&
                      torch_list(n->display, base, metadata, &current) &&
                      (uintptr_t)current == published &&
                      memcmp(published_bytes, current, sizeof(published_bytes)) == 0);
                printf(
                    "DK_TORCH stage=reuse metadata=%d attempts=0 same-definition=1 vertices=66\n",
                    metadata);
                CHECK(mc_client_render_global_update_retire(&call, &error) ==
                      MC_CLIENT_RENDER_GLOBAL_OK);
            }
        }
        const mc_block_fields *after = mc_block_fields_get(torch);
        mc_client_nw_state nw;
        CHECK(after->bs == before.bs && after->bt == before.bt && after->bu == before.bu &&
              after->bv == before.bv && after->bw == before.bw && after->bx == before.bx &&
              mc_client_nw_inspect(f->graphics, &nw) && !nw.fields.w && nw.fields.s == 0 &&
              nw.fields.t == 0 && nw.fields.u == 0);
    }
    f->chunks[0]->data->blocks[sand_index] = previous_sand;
    f->chunks[0]->data->blocks[torch_index] = previous_torch;
    f->chunks[0]->data->metadata[torch_index / 2u] = previous_metadata;
    dk->fields.g = 1;
    dk->fields.r = 65;
    mc_source_dk_dirty(dk);
    CHECK(mc_render_global_pointer_append(
              n, n->fields.m, dk, (mc_client_graphics_site){"n", "a", "()V", 571}, &error) &&
          mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_update_inspect(call, &state) && state.returned &&
          state.completed_compiles == 1 && !dk->fields.u);
    mc_client_source_list_state restored;
    CHECK(mc_client_display_list_inspect(n->display, base, &restored) && restored.defined &&
          !restored.building && restored.commands == 7 && restored.draws == 1 &&
          restored.vertices == 36 &&
          mc_client_render_global_update_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          f->base.world->persistent_bindings == bindings);
    return true;
}

/* The wrapper projects these inputs/active words from executed original rows.
 * No expected geometry is obtained from the native fluid implementation. */
typedef struct fluid_reference_case {
    char name[64];
    int32_t id, metadata, pass, sky_subtraction, x, y, z, vertices;
    size_t cells;
    int32_t inputs[125][7];
    uint32_t words[36u * 8u];
} fluid_reference_case;

static bool fluid_reference_read(FILE *input, fluid_reference_case *reference) {
    char marker[32];
    CHECK(fscanf(input,
                 "%31s %63s %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32
                 " %" SCNd32 " %" SCNd32 " %zu",
                 marker, reference->name, &reference->id, &reference->metadata, &reference->pass,
                 &reference->sky_subtraction, &reference->x, &reference->y, &reference->z,
                 &reference->vertices, &reference->cells) == 11 &&
          strcmp(marker, "CASE") == 0 && reference->id >= 8 && reference->id <= 11 &&
          reference->metadata >= 0 && reference->metadata <= 15 && reference->pass >= 0 &&
          reference->pass <= 1 && reference->x == 8 && reference->y == 64 && reference->z == 8 &&
          reference->vertices > 0 && reference->vertices <= 36 && reference->vertices % 6 == 0 &&
          reference->cells == 125);
    for (size_t i = 0; i < reference->cells; ++i) {
        int32_t *row = reference->inputs[i];
        CHECK(fscanf(input,
                     "%" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32
                     " %" SCNd32,
                     &row[0], &row[1], &row[2], &row[3], &row[4], &row[5], &row[6]) == 7 &&
              row[0] >= 0 && row[0] < 16 && row[1] >= 0 && row[1] < 128 && row[2] >= 0 &&
              row[2] < 16 && row[3] >= 0 && row[3] < 256 && row[4] >= 0 && row[4] < 16 &&
              row[5] >= 0 && row[5] < 16 && row[6] >= 0 && row[6] < 16);
    }
    for (size_t i = 0; i < (size_t)reference->vertices * 8u; ++i)
        CHECK(fscanf(input, "%" SCNx32, &reference->words[i]) == 1);
    return true;
}

static bool fluid_reference_list(picking_fixture *f, mc_client_render_global *owner, int32_t list,
                                 const fluid_reference_case *reference,
                                 const uint32_t prior[36u * 8u], const uint8_t **identity) {
    mc_client_source_list_state definition;
    mc_client_source_batch_view batch;
    mc_client_nw_state actual;
    CHECK(mc_client_display_list_inspect(owner->display, list, &definition) && definition.defined &&
          !definition.building && definition.draws == 1 &&
          definition.vertices == (size_t)reference->vertices &&
          mc_client_display_list_batch(owner->display, list, 0, &batch) && batch.mode == 4 &&
          batch.arrays == 7 && batch.vertices == reference->vertices &&
          batch.byte_count == (size_t)reference->vertices * 32u &&
          mc_client_nw_inspect(f->graphics, &actual) && !actual.fields.w && actual.fields.s == 0 &&
          actual.fields.t == 0 && actual.fields.u == 0);
    for (size_t i = 0; i < batch.byte_count / 4u; ++i) {
        uint32_t word;
        memcpy(&word, batch.bytes + i * 4u, sizeof(word));
        const uint32_t backing = (uint32_t)actual.fields.g[i];
        CHECK(word == backing && word == (i % 8u < 6u ? reference->words[i] : prior[i]));
    }
    *identity = batch.bytes;
    return true;
}

static bool fluid_compile_controls(picking_fixture *f, mc_client_render_global *owner,
                                   mc_client_source_dk *dk, int32_t base, FILE *input) {
    char marker[32];
    size_t cases = 0;
    CHECK(fscanf(input, "%31s %zu", marker, &cases) == 2 && strcmp(marker, "FLUID_DK_V1") == 0 &&
          cases == 8);

    struct fluid_compile_work {
        fluid_reference_case reference;
        uint8_t blocks[4][MC_CHUNK_BLOCKS];
        uint8_t metadata[4][MC_CHUNK_NIBBLE_BYTES];
        uint8_t sky[4][MC_CHUNK_NIBBLE_BYTES], light[4][MC_CHUNK_NIBBLE_BYTES];
        uint32_t prior[36u * 8u];
        uint8_t definition[36u * 32u];
        mc_world_light_operation_error light_operation;
        mc_world_query_error query;
        mc_client_render_global_error error;
        mc_client_render_global_update_state update;
    } *work = calloc(1, sizeof(*work));

    CHECK(work != NULL && owner->access == NULL && owner->fields.o == NULL &&
          owner->o_length == 0 && owner->fields.k == NULL && owner->fields.m->count == 0 &&
          owner->fields.p == 0 && owner->fields.q == 0 && owner->fields.r == 0);
    const mc_client_source_dk_fields prior_dk = dk->fields;
    const int32_t prior_sky = f->base.world->fields.f;
    mc_math_classes *const prior_math = f->app->source_math_classes;
    for (size_t i = 0; i < 4; ++i) {
        CHECK(f->chunks[i] != NULL && f->chunks[i]->data != NULL);
        memcpy(work->blocks[i], f->chunks[i]->data->blocks, MC_CHUNK_BLOCKS);
        memcpy(work->metadata[i], f->chunks[i]->data->metadata, MC_CHUNK_NIBBLE_BYTES);
        memcpy(work->sky[i], f->chunks[i]->data->skylight, MC_CHUNK_NIBBLE_BYTES);
        memcpy(work->light[i], f->chunks[i]->data->block_light, MC_CHUNK_NIBBLE_BYTES);
    }
    const size_t bindings = f->base.world->persistent_bindings;
    CHECK(mc_render_global_access_create(owner, &owner->access, &work->error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_source_add(owner->access, f->base.world, &work->error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    owner->fields.k = f->base.world; /* Declared actual entered N receiver field. */
    owner->fields.p = owner->fields.r = 1;
    owner->fields.q = 8;
    owner->fields.o = mc_render_global_allocate(
        owner, 8u * sizeof(*owner->fields.o),
        (mc_client_graphics_site){"native-test", "fluid", "", -1}, &work->error);
    CHECK(owner->fields.o != NULL && f->base.world->persistent_bindings == bindings + 1u);
    owner->o_length = 8;
    owner->fields.o[4] = dk;
    f->app->source_math_classes = f->math;
    dk->fields.c = dk->fields.e = dk->fields.l = dk->fields.n = 8;
    dk->fields.d = dk->fields.m = 64;
    dk->fields.f = dk->fields.g = dk->fields.h = 1;
    dk->fields.q = dk->fields.s = 8;
    dk->fields.r = 64;
    CHECK(mc_entity_set_position(f->actors[0], 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    unsigned int inventory[4] = {0};
    for (size_t case_index = 0; case_index < cases; ++case_index) {
        fluid_reference_case *reference = &work->reference;
        CHECK(fluid_reference_read(input, reference));
        ++inventory[(size_t)(reference->id - 8)];
        for (size_t i = 0; i < 4; ++i) {
            memset(f->chunks[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
            memset(f->chunks[i]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
            memset(f->chunks[i]->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
            memset(f->chunks[i]->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
        }
        for (size_t i = 0; i < reference->cells; ++i) {
            const int32_t *row = reference->inputs[i];
            const size_t index = ((size_t)row[0] << 11u) | ((size_t)row[2] << 7u) | (size_t)row[1];
            f->chunks[0]->data->blocks[index] = (uint8_t)row[3];
            CHECK(mc_chunk_nibbles_set(f->chunks[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, row[0],
                                       row[1], row[2], row[4]) &&
                  mc_chunk_nibbles_set(f->chunks[0]->data->skylight, MC_CHUNK_NIBBLE_BYTES, row[0],
                                       row[1], row[2], row[5]) &&
                  mc_chunk_nibbles_set(f->chunks[0]->data->block_light, MC_CHUNK_NIBBLE_BYTES,
                                       row[0], row[1], row[2], row[6]));
        }
        f->base.world->fields.f = reference->sky_subtraction;
        int32_t actual_id = -1, actual_metadata = -1;
        mc_world_storage_error world_error;
        const mc_block *fluid = NULL;
        mc_block_render_description description;
        CHECK(mc_world_runtime_block_get(f->base.world, 8, 64, 8, &actual_id, &world_error) ==
                  MC_WORLD_STORAGE_OK &&
              actual_id == reference->id &&
              mc_world_runtime_metadata_get(f->base.world, 8, 64, 8, &actual_metadata,
                                            &world_error) == MC_WORLD_STORAGE_OK &&
              actual_metadata == reference->metadata &&
              mc_registry_block_get(f->base.registry, reference->id, &fluid, NULL) ==
                  MC_REGISTRY_OK &&
              mc_block_render_describe(fluid, &description) == MC_REGISTRY_OK &&
              description.type == 4 && description.pass == reference->pass);
        uintptr_t published = 0;
        for (size_t attempt = 0; attempt < 2; ++attempt) {
            mc_client_nw_state before;
            CHECK(mc_client_nw_inspect(f->graphics, &before));
            memcpy(work->prior, before.fields.g, sizeof(work->prior));
            if (attempt == 0) {
                mc_source_dk_dirty(dk);
                CHECK(mc_render_global_pointer_append(
                    owner, owner->fields.m, dk, (mc_client_graphics_site){"n", "a", "()V", 571},
                    &work->error));
            } else {
                const mc_world_light_type *block_light = NULL;
                int32_t saved = -1;
                CHECK(!dk->fields.u && owner->fields.m->count == 0 &&
                      mc_world_runtime_light_type_get(f->base.world->classes, MC_LIGHT_BLOCK,
                                                      &block_light,
                                                      &world_error) == MC_WORLD_STORAGE_OK &&
                      mc_world_runtime_saved_light(f->base.world, block_light, 8, 65, 8, &saved,
                                                   &work->query) == MC_WORLD_STORAGE_OK &&
                      mc_world_runtime_saved_light_set(f->base.world, block_light, 8, 65, 8, saved,
                                                       &work->light_operation) ==
                          MC_WORLD_STORAGE_OK &&
                      dk->fields.u && dk->last_store_pc == 2 && owner->fields.m->count == 1 &&
                      owner->fields.m->entries[0] == dk);
            }
            mc_client_render_global_update_call *call = NULL;
            CHECK(mc_client_render_global_update_begin(owner, f->actors[0], false, &call,
                                                       &work->error) ==
                      MC_CLIENT_RENDER_GLOBAL_OK &&
                  mc_client_render_global_update_inspect(call, &work->update) &&
                  work->update.returned && work->update.compile_attempts == 1 &&
                  work->update.completed_compiles == 1 && work->update.remaining_count == 0 &&
                  work->update.compilation != NULL);
            const mc_client_chunk_compile_state *compiled = work->update.compilation;
            CHECK(compiled->returned && compiled->pass_rendered &&
                  compiled->pass == reference->pass && compiled->block_id == reference->id &&
                  compiled->render_blocks != NULL && compiled->render_blocks->returned &&
                  !compiled->render_blocks->failed && compiled->render_blocks->source_pc == 48 &&
                  !compiled->renderer.m && !compiled->list_open && !compiled->matrix_pushed &&
                  !compiled->tessellator_started && !dk->fields.u && dk->fields.E &&
                  dk->fields.p[reference->pass] == false && dk->fields.p[1 - reference->pass]);
            const uint8_t *current = NULL;
            CHECK(fluid_reference_list(f, owner, base + reference->pass, reference, work->prior,
                                       &current));
            if (attempt != 0)
                CHECK((uintptr_t)current != published);
            published = (uintptr_t)current;
            memcpy(work->definition, current, (size_t)reference->vertices * 32u);
            printf("DK_FLUID profile=%s stage=%s block=%" PRId32 " metadata=%" PRId32
                   " pass=%" PRId32 " cv-return=48 vertices=%" PRId32
                   " exact-active-words=1 own-prior-inactive=1 world-notification=%d\n",
                   reference->name, attempt == 0 ? "compile" : "dirty", actual_id, actual_metadata,
                   reference->pass, reference->vertices, attempt != 0);
            CHECK(mc_client_render_global_update_retire(&call, &work->error) ==
                      MC_CLIENT_RENDER_GLOBAL_OK &&
                  f->base.world->persistent_bindings == bindings + 1u);
            if (attempt == 0) {
                CHECK(mc_client_render_global_update_begin(owner, f->actors[0], false, &call,
                                                           &work->error) ==
                          MC_CLIENT_RENDER_GLOBAL_OK &&
                      mc_client_render_global_update_inspect(call, &work->update) &&
                      work->update.returned && work->update.compile_attempts == 0 &&
                      work->update.completed_compiles == 0 && work->update.compilation == NULL &&
                      fluid_reference_list(f, owner, base + reference->pass, reference, work->prior,
                                           &current) &&
                      (uintptr_t)current == published &&
                      memcmp(work->definition, current, (size_t)reference->vertices * 32u) == 0 &&
                      mc_client_render_global_update_retire(&call, &work->error) ==
                          MC_CLIENT_RENDER_GLOBAL_OK);
                printf(
                    "DK_FLUID profile=%s stage=reuse attempts=0 same-definition=1 vertices=%" PRId32
                    "\n",
                    reference->name, reference->vertices);
            }
        }
    }
    for (size_t i = 0; i < 4; ++i)
        CHECK(inventory[i] == 2);
    CHECK(fscanf(input, "%31s", marker) == EOF &&
          mc_render_global_access_source_remove(owner->access, f->base.world, &work->error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_retire(&owner->access, &work->error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          f->base.world->persistent_bindings == bindings);
    owner->fields.k = NULL;
    owner->fields.o = NULL;
    owner->o_length = 0;
    owner->fields.p = owner->fields.q = owner->fields.r = 0;
    for (size_t i = 0; i < 4; ++i) {
        memcpy(f->chunks[i]->data->blocks, work->blocks[i], MC_CHUNK_BLOCKS);
        memcpy(f->chunks[i]->data->metadata, work->metadata[i], MC_CHUNK_NIBBLE_BYTES);
        memcpy(f->chunks[i]->data->skylight, work->sky[i], MC_CHUNK_NIBBLE_BYTES);
        memcpy(f->chunks[i]->data->block_light, work->light[i], MC_CHUNK_NIBBLE_BYTES);
    }
    f->base.world->fields.f = prior_sky;
    f->app->source_math_classes = prior_math;
    dk->fields.c = prior_dk.c;
    dk->fields.d = prior_dk.d;
    dk->fields.e = prior_dk.e;
    dk->fields.f = prior_dk.f;
    dk->fields.g = prior_dk.g;
    dk->fields.h = prior_dk.h;
    dk->fields.l = prior_dk.l;
    dk->fields.m = prior_dk.m;
    dk->fields.n = prior_dk.n;
    dk->fields.q = prior_dk.q;
    dk->fields.r = prior_dk.r;
    dk->fields.s = prior_dk.s;
    CHECK(mc_entity_set_position(f->actors[0], 2, 65, 2) == MC_ENTITY_OK);
    mc_source_dk_dirty(dk);
    mc_client_render_global_update_call *restore = NULL;
    CHECK(mc_render_global_pointer_append(owner, owner->fields.m, dk,
                                          (mc_client_graphics_site){"n", "a", "()V", 571},
                                          &work->error) &&
          mc_client_render_global_update_begin(owner, f->actors[0], false, &restore,
                                               &work->error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_update_inspect(restore, &work->update) && work->update.returned &&
          work->update.completed_compiles == 1 && !dk->fields.u);
    mc_client_source_list_state restored;
    CHECK(mc_client_display_list_inspect(owner->display, base, &restored) && restored.defined &&
          !restored.building && restored.vertices == 36 &&
          mc_client_render_global_update_retire(&restore, &work->error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          f->base.world->persistent_bindings == bindings);
    free(work);
    return true;
}

static bool compile_controls(picking_fixture *f, FILE *fluid_input) {
    mc_client_render_global *n = NULL;
    mc_client_render_global_error error;
    CHECK(mc_client_render_global_allocate(f->graphics, NULL, &n, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    f->app->entities = f->base.entities;
    f->app->world_classes = f->base.world_classes;
    f->app->render_classes = f->models;
    n->fields.t = f->app;
    n->fields.m = mc_render_global_allocate(
        n, sizeof(*n->fields.m), (mc_client_graphics_site){"n", "<init>", "()V", 4}, &error);
    n->fields.a = mc_render_global_allocate(
        n, sizeof(*n->fields.a), (mc_client_graphics_site){"n", "<init>", "()V", 4}, &error);
    CHECK(n->fields.m != NULL && n->fields.a != NULL);
    mc_client_source_dk *dk = mc_source_dk_allocate(n, &error);
    CHECK(dk != NULL);
    dk->fields.p = mc_render_global_allocate(
        n, 2 * sizeof(bool), (mc_client_graphics_site){"dk", "<init>", "()V", 16}, &error);
    dk->fields.B = mc_render_global_allocate(
        n, sizeof(*dk->fields.B), (mc_client_graphics_site){"dk", "<init>", "()V", 32}, &error);
    CHECK(dk->fields.p != NULL && dk->fields.B != NULL);
    CHECK(mc_world_runtime_binding_retain(f->base.world));
    dk->fields.a = f->base.world;
    dk->world_held = true;
    dk->fields.F = n->fields.a;
    dk->fields.c = dk->fields.e = 1;
    dk->fields.d = 128;
    dk->fields.f = dk->fields.g = dk->fields.h = 2;
    dk->fields.q = dk->fields.s = 2;
    dk->fields.r = 129;
    dk->fields.u = true;
    CHECK(mc_entity_set_position(f->actors[0], 2, 129, 2) == MC_ENTITY_OK);
    /* Prepared entered-child context, matching the original direct DK probe.
     * No N source call or queue mutation is credited for this native guard. */
    dk->fields.A = true;
    int32_t before, after;
    mc_client_graphics_error graphics;
    CHECK(mc_client_chunk_statistics_get(
        f->graphics, (mc_client_graphics_site){"dk", "a", "()V", 8}, &before, &graphics));
    mc_source_dk_compile *direct = NULL;
    n->active = true;
    CHECK(mc_source_dk_compile_entered(dk, &direct, &error));
    n->active = false;
    mc_client_chunk_compile_state direct_state;
    CHECK(mc_source_dk_compile_inspect(direct, &direct_state) && direct_state.returned);
    CHECK(mc_client_chunk_statistics_get(
        f->graphics, (mc_client_graphics_site){"dk", "a", "()V", 8}, &after, &graphics));
    printf("DK_EMPTY %d %d %d %d %d %d %" PRId32 " %zu %zu\n", direct_state.returned,
           dk->fields.p[0], dk->fields.p[1], dk->fields.A, dk->fields.E, dk->fields.u,
           mc_java_i32_sub(after, before), dk->fields.B->count, dk->fields.F->count);
    CHECK(mc_source_dk_compile_retire(&direct, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_render_global_pointer_append(n, n->fields.m, dk,
                                          (mc_client_graphics_site){"n", "a", "()V", 571}, &error));
    const size_t bindings = f->base.world->persistent_bindings;
    mc_client_render_global_update_call *call = NULL;
    mc_client_render_global_update_state state;
    CHECK(mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_update_inspect(call, &state) && state.returned && state.result &&
          state.source_pc == 697 && state.compile_attempts == 1 && state.completed_compiles == 1 &&
          state.remaining_count == 0 && state.compilation != NULL && state.compilation->returned &&
          state.compilation->source_pc == 626 && state.compilation->cache != NULL &&
          state.compilation->renderer.c == state.compilation->cache && !dk->fields.u &&
          dk->fields.E && !dk->fields.A && dk->fields.p[0] && dk->fields.p[1]);
    CHECK(mc_client_render_global_retire(&n, &error) == MC_CLIENT_RENDER_GLOBAL_BUSY);
    CHECK(mc_client_render_global_update_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL);
    CHECK(f->base.world->persistent_bindings == bindings);
    /* The actual static Minecraft.a remains NULL in this prepared body. Its
     * canonical v() therefore selects flat shading after real class activation. */
    CHECK(mc_client_application_classes_create(NULL, &f->app->source_namespace, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    /* A dirty one-Block section compiles into the real display list through
     * the SAME CV/EW/NW objects, then clears u only in the caller N. */
    mc_client_display_error display_error;
    CHECK(mc_client_display_create("Source ChunkRenderer compilation", 64, 64, true, &n->display,
                                   &display_error));
    mc_client_display_list_range *range = NULL;
    int32_t base;
    CHECK(mc_client_display_lists_generate(n->display, 3, 64, &range, &base, &display_error));
    dk->fields.C = base;
    dk->fields.d = 64;
    dk->fields.r = 65;
    dk->fields.l = dk->fields.n = 1;
    dk->fields.m = 64;
    dk->fields.u = true;
    CHECK(mc_entity_set_position(f->actors[0], 2, 65, 2) == MC_ENTITY_OK);
    const size_t index = (1u << 11u) | (1u << 7u) | 64u;
    const uint8_t old = f->chunks[0]->data->blocks[index];
    f->chunks[0]->data->blocks[index] = 1;
    CHECK(mc_render_global_pointer_append(n, n->fields.m, dk,
                                          (mc_client_graphics_site){"n", "a", "()V", 571}, &error));
    CHECK(mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_update_inspect(call, &state) && state.returned && state.result &&
          state.compilation != NULL && state.compilation->returned &&
          state.compilation->pass_rendered && state.compilation->render_blocks != NULL &&
          state.compilation->render_blocks->returned && !state.compilation->render_blocks->failed &&
          !state.compilation->renderer.m &&
          state.compilation->renderer.c == state.compilation->cache &&
          !state.compilation->list_open && !state.compilation->matrix_pushed &&
          !state.compilation->tessellator_started && !dk->fields.u && dk->fields.E &&
          !dk->fields.p[0] && dk->fields.p[1] && n->fields.m->count == 0);
    mc_client_source_list_state list;
    CHECK(mc_client_display_list_inspect(n->display, base, &list) && list.defined &&
          !list.building && list.commands == 7 && list.draws == 1 && list.vertices == 36);
    mc_client_nw_state nw_state;
    CHECK(mc_client_nw_inspect(f->graphics, &nw_state) && !nw_state.fields.w &&
          nw_state.fields.s == 0 && nw_state.fields.t == 0 && nw_state.fields.u == 0);
    CHECK(mc_client_render_global_update_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(f->base.world->persistent_bindings == bindings);
    CHECK(cactus_compile_controls(f, n, dk, base));
    CHECK(torch_compile_controls(f, n, dk, base));
    CHECK(fluid_compile_controls(f, n, dk, base, fluid_input));
    /* A remaining family still retains a real failed compile prefix. Native
     * cleanup preserves both the earlier list and the unfinished definition. */
    const mc_block *fire = NULL;
    mc_block_render_description fire_description;
    CHECK(mc_registry_block_get(f->base.registry, 51, &fire, NULL) == MC_REGISTRY_OK &&
          fire != NULL && mc_block_render_describe(fire, &fire_description) == MC_REGISTRY_OK &&
          fire_description.type == 3 && fire_description.pass == 0);
    f->chunks[0]->data->blocks[index] = 51;
    dk->fields.u = true;
    CHECK(mc_render_global_pointer_append(n, n->fields.m, dk,
                                          (mc_client_graphics_site){"n", "a", "()V", 571}, &error));
    CHECK(mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
          MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY);
    CHECK(mc_client_render_global_update_inspect(call, &state) && !state.returned &&
          state.source_pc == 442 && state.compile_attempts == 1 && state.completed_compiles == 0 &&
          state.compilation != NULL && state.compilation->source_pc == 464 &&
          state.compilation->block_id == 51 && state.compilation->list_open &&
          state.compilation->matrix_pushed && state.compilation->tessellator_started &&
          state.compilation->renderer.c == state.compilation->cache && n->fields.m->count == 1 &&
          n->fields.m->entries[0] == NULL && dk->fields.u &&
          state.compilation->render_blocks != NULL && state.compilation->render_blocks->failed &&
          state.compilation->render_blocks->failure.source_pc == 127);
    CHECK(mc_client_display_list_inspect(n->display, base, &list) && list.building &&
          list.pending_commands == 5 && list.pending_bytes != 0);
    CHECK(mc_client_render_global_update_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_display_list_inspect(n->display, base, &list) && list.building &&
          list.defined && list.commands == 7 && list.vertices == 36 && list.pending_commands == 5 &&
          list.pending_bytes != 0);
    f->chunks[0]->data->blocks[index] = old;
    CHECK(f->base.world->persistent_bindings == bindings);
    mc_client_display *display = n->display;
    CHECK(mc_client_display_lists_retire(&range, &display_error));
    CHECK(mc_client_render_global_retire(&n, &error) == MC_CLIENT_RENDER_GLOBAL_OK && n == NULL);
    CHECK(mc_client_display_destroy(display, &display_error));
    CHECK(mc_client_application_classes_release(&f->app->source_namespace, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    FILE *fluid_input = fopen(argv[3], "r");
    if (fluid_input == NULL)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !bound_dispatch_controls(f) || !compile_controls(f, fluid_input) ||
        !picking_case_close(f) || !picking_close(f))
        return 1;
    if (fclose(fluid_input) != 0)
        return 1;
    free(f);
    printf("Source DK: %u checks; empty and nonair completion, retained family failure\n", checks);
    return 0;
}
