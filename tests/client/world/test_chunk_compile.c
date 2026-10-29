/* Genuine saved-World fixtures; the original picking main is not executed. */
#define MC_PICKING_BODY_MAIN_NAME mc_chunk_compile_picker_fixture_main
int mc_chunk_compile_picker_fixture_main(int argc, char **argv);
#include "test_picking.c"
#undef main

#include "client/display/source_surface.h"
#include "client/render/blocks/internal.h"
#include "client/render_engine/chunk_statistics.h"
#include "client/render_engine/internal.h"
#include "client/world/source_chunk_compile_internal.h"
#include "mc/client/world/render_global_update.h"
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

static bool compile_controls(picking_fixture *f) {
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
    /* A remaining family still retains a real failed compile prefix. Native
     * cleanup preserves both the earlier list and the unfinished definition. */
    f->chunks[0]->data->blocks[index] = 50;
    dk->fields.u = true;
    CHECK(mc_render_global_pointer_append(n, n->fields.m, dk,
                                          (mc_client_graphics_site){"n", "a", "()V", 571}, &error));
    CHECK(mc_client_render_global_update_begin(n, f->actors[0], false, &call, &error) ==
          MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY);
    CHECK(mc_client_render_global_update_inspect(call, &state) && !state.returned &&
          state.source_pc == 442 && state.compile_attempts == 1 && state.completed_compiles == 0 &&
          state.compilation != NULL && state.compilation->source_pc == 464 &&
          state.compilation->block_id == 50 && state.compilation->list_open &&
          state.compilation->matrix_pushed && state.compilation->tessellator_started &&
          state.compilation->renderer.c == state.compilation->cache && n->fields.m->count == 1 &&
          n->fields.m->entries[0] == NULL && dk->fields.u &&
          state.compilation->render_blocks != NULL && state.compilation->render_blocks->failed &&
          state.compilation->render_blocks->failure.source_pc == 111);
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
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !bound_dispatch_controls(f) || !compile_controls(f) || !picking_case_close(f) ||
        !picking_close(f))
        return 1;
    free(f);
    printf("Source DK: %u checks; empty and nonair completion, retained family failure\n", checks);
    return 0;
}
