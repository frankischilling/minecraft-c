/* Reuse canonical namespace construction, not replacement renderer bodies. */
int mc_tile_renderer_draw_core_fixture_main(int argc, char **argv);
#define main mc_tile_renderer_draw_core_fixture_main
#include "../test_tile_entity_renderer.c"
#undef main

#include "client/render/tile_entity_renderer/draw_internal.h"
#include "world/runtime/runtime_internal.h"

#include <xmmintrin.h>

static bool draw_close(fixture *f) {
    CHECK(mc_client_tile_renderer_classes_retire(&f->classes, &f->error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_render_classes_free(f->models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_world_runtime_classes_free(f->worlds, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->profile);
    return true;
}

static bool end_draw(mc_client_tile_renderer_draw_call **call) {
    mc_client_tile_renderer_draw_error error;
    CHECK(mc_client_tile_renderer_draw_retire(call, &error) == MC_CLIENT_TILE_RENDERER_OK &&
          *call == NULL);
    CHECK(mc_client_tile_renderer_draw_retire(call, &error) == MC_CLIENT_TILE_RENDERER_OK);
    return true;
}

static bool distance_and_dispatch(void) {
    fixture f = {0};
    CHECK(fixture_open(&f));
    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_tile_entity *chest = NULL, *spawner = NULL, *piston = NULL;
    CHECK(mc_tile_entity_allocate_in_classes(f.worlds, MC_TILE_ENTITY_CHEST, &chest) ==
          MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_construct(chest) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_allocate_in_classes(f.worlds, MC_TILE_ENTITY_SPAWNER, &spawner) ==
          MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_construct(spawner) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_allocate_in_classes(f.worlds, MC_TILE_ENTITY_PISTON, &piston) ==
          MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_construct(piston) == MC_TILE_ENTITY_OK);
    const mc_client_manager_draw_services services = {0};
    mc_client_tile_renderer_draw_error error;
    mc_client_tile_renderer_draw_call *call = NULL;
    mc_client_tile_renderer_draw_state state;

    CHECK(mc_client_tile_renderer_draw_begin(f.manager, NULL, 0.5f, &services, NULL, &call,
                                             &error) == MC_CLIENT_TILE_RENDERER_NULL_REFERENCE);
    CHECK(call != NULL && error.source_exception && !error.native_policy &&
          strcmp(error.site.class_name, "ll") == 0 && error.site.pc == 13 &&
          error.parent.class_name == NULL);
    const mc_client_tile_renderer_draw_call *same = call;
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && !state.returned &&
          !state.distance_returned && state.gl_calls == 0 && state.failure != NULL);
    CHECK(mc_client_tile_renderer_dispatcher_retain(f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_BUSY);
    CHECK(mc_client_tile_renderer_classes_retire(&f.classes, &f.error) ==
          MC_CLIENT_TILE_RENDERER_BUSY);
    CHECK(mc_client_tile_renderer_draw_begin(f.manager, chest, 1.0f, &services, NULL, &call,
                                             &error) == MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT &&
          call == same);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.failure->source_exception &&
          state.failure->site.pc == 13);
    CHECK(end_draw(&call));

    CHECK(mc_client_tile_renderer_coordinates_draw_begin(f.manager, NULL, 1, 2, 3, 0.5f, &services,
                                                         NULL, &call,
                                                         &error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.returned &&
          state.coordinates_only && state.lookup_returned && !state.renderer_returned &&
          state.source_pc == 25 && state.gl_calls == 0);
    CHECK(end_draw(&call));
    CHECK(mc_client_tile_renderer_coordinates_draw_begin(f.manager, chest, 7, 8, 9, 0.5f, &services,
                                                         NULL, &call,
                                                         &error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.returned &&
          state.renderer_identity == 0 && state.x == 7 && state.y == 8 && state.z == 9 &&
          !state.brightness_returned && state.gl_calls == 0);
    CHECK(end_draw(&call));

    /* Authored caller camera fields make an exact 64-block center distance.
     * They are inputs, never a repair of a reached production outcome. */
    f.manager->fields.j = -63.5;
    f.manager->fields.k = f.manager->fields.l = 0.5;
    CHECK(mc_client_tile_renderer_draw_begin(f.manager, chest, 0.5f, &services, NULL, &call,
                                             &error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.returned &&
          state.distance_returned && state.distance == 4096.0 && state.distance_culled &&
          state.source_pc == 82 && !state.brightness_returned && state.gl_calls == 0);
    CHECK(end_draw(&call));
    f.manager->fields.j = -63.0;
    CHECK(mc_client_tile_renderer_draw_begin(f.manager, chest, 0.5f, &services, NULL, &call,
                                             &error) == MC_CLIENT_TILE_RENDERER_NULL_REFERENCE);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && !state.returned &&
          state.distance_returned && !state.distance_culled && !state.lookup_returned &&
          state.gl_calls == 0 && error.site.pc == 39 && error.source_exception &&
          !error.native_policy);
    CHECK(end_draw(&call));
    const uint64_t nan = UINT64_C(0x7ff8000000001234);
    memcpy(&f.manager->fields.j, &nan, sizeof(nan));
    CHECK(mc_client_tile_renderer_draw_begin(f.manager, chest, 0.5f, &services, NULL, &call,
                                             &error) == MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && state.returned &&
          state.distance_culled && !state.brightness_returned && state.gl_calls == 0);
    CHECK(end_draw(&call));

    f.manager->fields.j = 0.5;
    const unsigned int saved = _mm_getcsr();
    const unsigned int unsupported = (saved & ~0x6000u) | 0x6000u;
    _mm_setcsr(unsupported);
    const mc_client_tile_renderer_status fp =
        mc_client_tile_renderer_draw_begin(f.manager, chest, 0.5f, &services, NULL, &call, &error);
    const bool unchanged = _mm_getcsr() == unsupported;
    _mm_setcsr(saved); /* Caller restores its authored input. */
    CHECK(fp == MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP && unchanged && error.native_policy &&
          error.site.pc == 8 && strcmp(error.site.class_name, "ow") == 0);
    CHECK(end_draw(&call));

    CHECK(mc_client_tile_renderer_coordinates_draw_begin(f.manager, spawner, 1, 2, 3, 0.5f,
                                                         &services, NULL, &call, &error) ==
          MC_CLIENT_TILE_RENDERER_DISPLAY_FAILED);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && !state.returned &&
          state.lookup_returned && state.bridge_cast_returned && !state.matrix_pushed &&
          state.factory_calls == 0 && !state.cache_miss && state.gl_calls == 1 &&
          strcmp(error.site.class_name, "ag") == 0 && error.site.pc == 0);
    CHECK(end_draw(&call));
    CHECK(mc_client_tile_renderer_coordinates_draw_begin(f.manager, piston, 1, 2, 3, 0.5f,
                                                         &services, NULL, &call, &error) ==
          MC_CLIENT_TILE_RENDERER_BLOCKS_FAILED);
    CHECK(mc_client_tile_renderer_draw_inspect(call, &state) && !state.returned &&
          state.lookup_returned && state.bridge_cast_returned &&
          strcmp(state.renderer_class, "hy") == 0 && state.gl_calls == 0 &&
          strcmp(error.site.class_name, "hy") == 0 && error.site.pc == 0 &&
          error.registry.status != MC_REGISTRY_OK);
    CHECK(end_draw(&call));
    const mc_client_tile_renderer_draw_limits zero = {0};
    CHECK(mc_client_tile_renderer_draw_begin(f.manager, chest, 0.5f, &services, &zero, &call,
                                             &error) == MC_CLIENT_TILE_RENDERER_LIMIT &&
          call == NULL && error.native_policy);
    mc_tile_entity_release(chest);
    mc_tile_entity_release(spawner);
    mc_tile_entity_release(piston);
    CHECK(draw_close(&f));
    return true;
}

/* CPU-only entry of AG's actual cache/factory subbody. No GL/whole AG return
 * is claimed. Every call is independent; no failed draw is resumed. */
static bool cache_prefix(fixture *f, mc_tile_entity *tile, mc_client_tile_renderer *renderer,
                         bool hit, uintptr_t *identity, unsigned int *factory_calls) {
    mc_client_tile_renderer_draw_call *call = calloc(1, sizeof(*call));
    CHECK(call != NULL);
    call->dispatcher = f->manager;
    call->renderer = renderer;
    call->tile_entity = tile;
    tile_renderer_error_init(&call->error.owner);
    f->manager->active = f->classes->active = renderer->active = true;
    const bool returned = tile_draw_spawner_cache(call);
    f->manager->active = f->classes->active = renderer->active = false;
    CHECK(returned && call->error.status == MC_CLIENT_TILE_RENDERER_OK &&
          call->state.cache_hit == hit && call->state.cache_miss != hit &&
          call->state.gl_calls == 0 && !call->state.returned);
    if (identity != NULL)
        *identity = call->state.preview_identity;
    *factory_calls += call->state.factory_calls;
    mc_entity_release(call->preview);
    mc_entity_release(call->factory_result);
    mc_entity_release(call->map_value_pending);
    for (size_t i = 0; i < 3; ++i)
        mc_tile_entity_name_release(call->names[i]);
    free(call);
    return true;
}

static bool cache_controls(void) {
    fixture f = {0};
    CHECK(fixture_open(&f));
    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_tile_entity *tile = NULL;
    CHECK(mc_tile_entity_allocate_in_classes(f.worlds, MC_TILE_ENTITY_SPAWNER, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK);
    mc_client_tile_renderer *renderer = NULL;
    CHECK(mc_client_tile_renderer_lookup_tile(f.manager, tile, &renderer, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          renderer != NULL);
    static const uint16_t unknown[] = {'u', 'n', 'k', 'n', 'o', 'w', 'n'};
    mc_tile_entity_name *name = NULL;
    CHECK(mc_tile_entity_name_create(unknown, 7, &name) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_spawner_set_name(tile, name) == MC_TILE_ENTITY_OK);
    mc_tile_entity_name_release(name);
    unsigned int factory_calls = 0;
    uintptr_t identity = 1;
    CHECK(cache_prefix(&f, tile, renderer, false, &identity, &factory_calls) && identity == 0);
    CHECK(cache_prefix(&f, tile, renderer, false, &identity, &factory_calls) && identity == 0 &&
          factory_calls == 2 && renderer->spawner_map->size == 1);
    CHECK(mc_tile_entity_spawner_set_name(tile, NULL) == MC_TILE_ENTITY_OK);
    CHECK(cache_prefix(&f, tile, renderer, false, &identity, &factory_calls) && identity == 0);
    CHECK(cache_prefix(&f, tile, renderer, false, &identity, &factory_calls) && identity == 0 &&
          factory_calls == 4 && renderer->spawner_map->size == 2);
    CHECK(mc_tile_entity_name_create(NULL, 0, &name) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_spawner_set_name(tile, name) == MC_TILE_ENTITY_OK);
    mc_tile_entity_name_release(name);
    CHECK(cache_prefix(&f, tile, renderer, false, &identity, &factory_calls) && identity == 0 &&
          factory_calls == 5 && renderer->spawner_map->size == 3);

    static const uint16_t pig[] = {'P', 'i', 'g'};
    CHECK(mc_tile_entity_name_create(pig, 3, &name) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_spawner_set_name(tile, name) == MC_TILE_ENTITY_OK);
    mc_tile_entity_name_release(name);
    CHECK(cache_prefix(&f, tile, renderer, false, &identity, &factory_calls) && identity != 0 &&
          factory_calls == 6 && renderer->spawner_map->size == 4);
    const uintptr_t pig_identity = identity;
    /* A new equal String must hit the stored value without another JC call. */
    CHECK(mc_tile_entity_name_create(pig, 3, &name) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_spawner_set_name(tile, name) == MC_TILE_ENTITY_OK);
    mc_tile_entity_name_release(name);
    CHECK(cache_prefix(&f, tile, renderer, true, &identity, &factory_calls) &&
          identity == pig_identity && factory_calls == 6 && renderer->spawner_map->size == 4);
    mc_entity_class_snapshot entities;
    CHECK(mc_entity_classes_get_snapshot(f.entities, &entities) == MC_ENTITY_OK &&
          entities.live_entities == 1);
    mc_tile_entity_release(tile);
    CHECK(mc_client_tile_renderer_classes_retire(&f.classes, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_entity_classes_get_snapshot(f.entities, &entities) == MC_ENTITY_OK &&
          entities.live_entities == 0);
    CHECK(draw_close(&f));
    return true;
}

int main(void) {
    if (!distance_and_dispatch() || !cache_controls())
        return 1;
    printf("Source TileEntityRenderer draw CPU: checks=%u; ll=1; cache_prefix=1; gl_draw=0; "
           "cleanup=1\n",
           checks);
    return 0;
}
