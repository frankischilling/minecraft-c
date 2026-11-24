/* Authored native UJ bound/PO gate controls over genuine prepared GAME
 * owners. This does not claim a rendered frame or original GL equality. */
#define MC_PICKING_BODY_MAIN_NAME mc_sign_blocks_picking_fixture_main
int mc_sign_blocks_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/tile_entity_renderer/draw_internal.h"
#include "world/runtime/collision/blocks_internal.h"

static const float sign_seed_bounds[6] = {.125f, .25f, .375f, .875f, .75f, .625f};

static bool sign_bounds_are(const mc_block *block, const double expected[6]) {
    const double actual[] = {block->fields.bs, block->fields.bt, block->fields.bu,
                             block->fields.bv, block->fields.bw, block->fields.bx};
    CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
    return true;
}

static bool sign_seed(mc_block *block) {
    CHECK(mc_block_render_bounds(block, sign_seed_bounds[0], sign_seed_bounds[1],
                                 sign_seed_bounds[2], sign_seed_bounds[3], sign_seed_bounds[4],
                                 sign_seed_bounds[5]) == MC_REGISTRY_OK);
    return true;
}

static void sign_context(picking_fixture *f, mc_client_chunk_cache *cache, mc_cv_context *call,
                         mc_client_render_blocks *fields, const void *receiver) {
    memset(call, 0, sizeof(*call));
    mc_client_render_blocks_construct_fields(fields, receiver, true);
    call->fields = fields;
    call->cache = cache;
    call->world = f->base.world;
    call->registry = f->base.registry;
    call->models = f->models;
    call->code_classes = f->base.world_classes;
}

static bool sign_bound_controls(picking_fixture *f) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    mc_cv_context *call = calloc(1, sizeof(*call));
    mc_world_collision_error *original_body = calloc(1, sizeof(*original_body));
    CHECK(call != NULL && original_body != NULL);
    const mc_block *raw[2];
    CHECK(mc_registry_block_get(f->base.registry, 63, &raw[0], NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->base.registry, 68, &raw[1], NULL) == MC_REGISTRY_OK &&
          raw[0] != NULL && raw[1] != NULL);
    const mc_block_fields saved[2] = {raw[0]->fields, raw[1]->fields};
    mc_client_render_blocks fields;
    for (size_t kind = 0; kind < 2; ++kind) {
        mc_block *block = (mc_block *)(void *)raw[kind];
        CHECK(block->kind == MC_BLOCK_SIGN && block->subtype.uj.b == (kind == 0));
        for (int32_t metadata = 0; metadata < 16; ++metadata) {
            CHECK(mc_chunk_nibbles_set(f->chunks[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64,
                                       8, (uint8_t)metadata));
            CHECK(sign_seed(block));
            memset(original_body, 0, sizeof(*original_body));
            CHECK(mc_world_collision_block_state_shape_in_classes_held(
                      f->base.world_classes, f->base.world, block, 8, 64, 8, original_body) ==
                  MC_WORLD_STORAGE_OK);
            const double expected[] = {block->fields.bs, block->fields.bt, block->fields.bu,
                                       block->fields.bv, block->fields.bw, block->fields.bx};
            const void *receivers[] = {f->base.world, cache, NULL};
            const size_t receiver_count = kind == 0 ? 3u : 2u;
            for (size_t receiver = 0; receiver < receiver_count; ++receiver) {
                CHECK(sign_seed(block));
                sign_context(f, cache, call, &fields, receivers[receiver]);
                /* The actual dispatcher captures render type -1, invokes UJ,
                 * then returns false without beginning NW or emitting faces. */
                bool rendered = true;
                CHECK(mc_cv_render(call, block, 8, 64, 8, &rendered) && !rendered &&
                      call->returned && !call->failed && call->depth == 0 &&
                      call->source_pc == 323 && sign_bounds_are(block, expected));
                CHECK(mc_cv_retire(call, NULL));
            }
        }
        if (kind != 0) {
            CHECK(sign_seed(block));
            sign_context(f, cache, call, &fields, NULL);
            bool rendered = true;
            CHECK(!mc_cv_render(call, block, 8, 64, 8, &rendered) && rendered && call->failed &&
                  call->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                  !call->failure.native_policy && call->failure.source_pc == 13 &&
                  strcmp(call->failure.source_class, "uj") == 0 && !call->returned);
            const double unchanged[] = {.125, .25, .375, .875, .75, .625};
            CHECK(sign_bounds_are(block, unchanged) && mc_cv_retire(call, NULL));
        }
        block->fields = saved[kind];
    }
    free(original_body);
    free(call);
    CHECK(mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    return true;
}

static bool sign_draw_gates(picking_fixture *f) {
    mc_client_tile_renderer_classes *classes = NULL;
    mc_client_tile_renderer_dispatcher *manager = NULL;
    mc_client_tile_renderer_error owner;
    CHECK(mc_client_tile_renderer_classes_bind(f->models, f->base.world_classes, f->base.profile,
                                               NULL, &classes,
                                               &owner) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_static_get(classes, &manager, &owner) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_tile_entity *tile = NULL;
    CHECK(mc_tile_entity_allocate_in_classes(f->base.world_classes, MC_TILE_ENTITY_SIGN, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK);
    const mc_client_manager_draw_services services = {.graphics = f->graphics, .math = f->math};
    mc_client_tile_renderer_draw_call *call = NULL;
    mc_client_tile_renderer_draw_error error;
    mc_client_tile_renderer_draw_state state;
    CHECK(mc_client_tile_renderer_coordinates_draw_begin(manager, tile, 1, 2, 3, .5f, &services,
                                                         NULL, &call, &error) ==
          MC_CLIENT_TILE_RENDERER_NULL_REFERENCE);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.lookup_returned &&
          state.bridge_cast_returned && !state.sign_block_returned && !state.matrix_pushed &&
          state.gl_calls == 0 && error.source_exception && !error.native_policy &&
          strcmp(error.site.class_name, "po") == 0 && error.site.pc == 1 &&
          strcmp(error.child.class_name, "ow") == 0 && error.child.pc == 19);
    const size_t header = sizeof(*call);
    CHECK(mc_client_tile_renderer_classes_retire(&classes, &owner) == MC_CLIENT_TILE_RENDERER_BUSY);
    CHECK(mc_client_tile_renderer_draw_retire(&call, &error) == MC_CLIENT_TILE_RENDERER_OK &&
          call == NULL);
    mc_client_tile_renderer_draw_limits limit = mc_client_tile_renderer_draw_default_limits();
    limit.max_bytes = header;
    CHECK(mc_client_tile_renderer_coordinates_draw_begin(manager, tile, 1, 2, 3, .5f, &services,
                                                         &limit, &call,
                                                         &error) == MC_CLIENT_TILE_RENDERER_LIMIT);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.bridge_cast_returned &&
          state.gl_calls == 0 && call->sign == NULL && state.allocation_bytes == header &&
          !state.sign_block_returned && !state.returned && error.native_policy);
    CHECK(mc_client_tile_renderer_draw_retire(&call, &error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_tile_entity_bind_world(tile, f->base.world) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_set_coordinates(tile, 8, 64, 8) == MC_TILE_ENTITY_OK);
    const size_t bindings = f->base.world->persistent_bindings;
    f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = 63;
    CHECK(mc_client_tile_renderer_coordinates_draw_begin(manager, tile, 1, 2, 3, .5f, &services,
                                                         NULL, &call, &error) ==
          MC_CLIENT_TILE_RENDERER_DISPLAY_FAILED);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.sign_block_returned &&
          !state.sign_metadata_returned && !state.matrix_pushed && state.gl_calls == 1 &&
          error.site.pc == 6 && !state.returned &&
          f->base.world->persistent_bindings == bindings + 1u);
    CHECK(mc_client_tile_renderer_draw_retire(&call, &error) == MC_CLIENT_TILE_RENDERER_OK &&
          f->base.world->persistent_bindings == bindings);
    mc_tile_entity_release(tile);
    CHECK(mc_client_tile_renderer_classes_retire(&classes, &owner) == MC_CLIENT_TILE_RENDERER_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !sign_bound_controls(f) || !sign_draw_gates(f) || !picking_case_close(f) ||
        !picking_close(f))
        return 1;
    free(f);
    printf("PASS: %u Source Sign bounds/draw gate checks (CPU only)\n", checks);
    return 0;
}
