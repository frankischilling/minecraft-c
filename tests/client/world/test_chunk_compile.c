/* Genuine saved-World fixtures; the original picking main is not executed. */
#define MC_PICKING_BODY_MAIN_NAME mc_chunk_compile_picker_fixture_main
int mc_chunk_compile_picker_fixture_main(int argc, char **argv);
#include "test_picking.c"
#undef main

#include "client/display/source_surface.h"
#include "client/render_engine/chunk_statistics.h"
#include "client/render_engine/internal.h"
#include "client/world/source_chunk_compile_internal.h"
#include "mc/client/world/render_global_update.h"
#include "world/chunk/object_state_internal.h"

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
    /* A dirty nonair section reaches the same real list/NW/cache and explicit
     * CV child, preserving the queue's earlier NULL slot and DK flags. */
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
          MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY);
    CHECK(mc_client_render_global_update_inspect(call, &state) && !state.returned &&
          state.source_pc == 442 && state.compile_attempts == 1 && state.completed_compiles == 0 &&
          state.compilation != NULL && state.compilation->source_pc == 464 &&
          state.compilation->block_id == 1 && state.compilation->list_open &&
          state.compilation->matrix_pushed && state.compilation->tessellator_started &&
          state.compilation->renderer.c == state.compilation->cache && n->fields.m->count == 1 &&
          n->fields.m->entries[0] == NULL && dk->fields.u);
    mc_client_source_list_state list;
    CHECK(mc_client_display_list_inspect(n->display, base, &list) && list.building &&
          list.pending_commands == 5 && list.pending_bytes != 0);
    CHECK(mc_client_render_global_update_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_display_list_inspect(n->display, base, &list) && list.building &&
          !list.defined && list.pending_commands == 5 && list.pending_bytes != 0);
    f->chunks[0]->data->blocks[index] = old;
    CHECK(f->base.world->persistent_bindings == bindings);
    mc_client_display *display = n->display;
    CHECK(mc_client_display_lists_retire(&range, &display_error));
    CHECK(mc_client_render_global_retire(&n, &error) == MC_CLIENT_RENDER_GLOBAL_OK && n == NULL);
    CHECK(mc_client_display_destroy(display, &display_error));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !compile_controls(f) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Source DK: %u checks; empty compile and retained nonair geometry frontier\n", checks);
    return 0;
}
