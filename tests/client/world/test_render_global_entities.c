/* Native composed controls on a second SOURCE-constructed N and the actual
 * completed loaded App. No original composed-N equality is claimed here. */
#include "test_render_global_entities.h"
#include "client/application_state.h"
#include "client/display/staged.h"
#include "client/render/model_graph_internal.h"
#include "client/render/tile_entity_renderer/internal.h"
#include "client/render_engine/internal.h"
#include "client/startup/application_startup_internal.h"
#include "client/world/render_global_internal.h"
#include "entity/entity_internal.h"
#include "mc/client/world/render_global_entities.h"
#include "mc/entity/living/sleeping.h"
#include "mc/entity/living/view.h"
#include "mc/entity/render_distance.h"
#include "mc/world/runtime/render_entities.h"

#include <emmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "N entities check%u at%d: %s\n", checks, __LINE__, #value);            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct controls {
    mc_client_render_global *owner;
    mc_client_render_global_entities_call *call;
    mc_client_render_global_world_call *attachment;
    mc_client_source_frustum *frustum;
    mc_entity_vector *eye;
    mc_client_render_global_error error;
    mc_client_culling_error culling;
    mc_entity_view_error view;
    mc_client_render_global_entities_state state;
    mc_client_render_global_world_state world_state;
    mc_client_tile_renderer_classes *tiles;
    mc_client_tile_renderer_state tiles_state;
    mc_client_manager_state manager_state;
    mc_client_tile_renderer_consumer foreign_tiles;
    mc_client_manager_consumer foreign_manager;
} controls;

static bool same_math(const mc_math_class_state *a, const mc_math_class_state *b) {
    return a->phase == b->phase && a->holds == b->holds && a->maximum_bytes == b->maximum_bytes &&
           a->last_store_pc == b->last_store_pc && a->source_a == b->source_a;
}

static bool same_pool(const mc_entity_vector_snapshot *a, const mc_entity_vector_snapshot *b) {
    return a->phase == b->phase && a->cursor == b->cursor && a->pool_identity == b->pool_identity &&
           a->size == b->size && a->capacity == b->capacity && a->holds == b->holds;
}

int mc_test_render_global_entities(client_application *app, mc_math_classes *math) {
    int result = 1;
    controls *c = calloc(1, sizeof(*c));
    bool settings_changed = false;
    size_t observed_frustum_checks = 0, observed_camera_skips = 0;
    mc_game_settings_state settings;
    mc_client_render_global_entities_services services;
    checks = 0;
    CHECK(c != NULL && app != NULL && app->startup != NULL && app->active_world != NULL &&
          app->i != NULL && app->i == app->h && app->render_engine != NULL &&
          app->font_renderer != NULL && app->display != NULL && app->world_classes != NULL &&
          math != NULL && app->source_math_classes == math);
    services = (mc_client_render_global_entities_services){app->render_classes,
                                                           app->world_classes,
                                                           app->profile,
                                                           app->registry,
                                                           app->startup->manager_classes,
                                                           math,
                                                           app->source_namespace};
    CHECK(services.manager_classes != NULL && app->startup->engine_classes != NULL &&
          mc_client_render_global_allocate(app->startup->engine_classes, NULL, &c->owner,
                                           &c->error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_construct(c->owner, app, app->render_engine, app->display,
                                            &c->error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          c->owner->source_returned && c->owner->fields.I == 2 && c->owner->fields.t == app &&
          c->owner->fields.l == app->render_engine && c->owner->fields.k == NULL);

    const void *initial_ll = app->render_classes->ll;
    const void *initial_th = app->render_classes->th;
    mc_math_class_state math_before, math_after;
    mc_entity_vector_snapshot pool_before, pool_after;
    CHECK(mc_math_classes_state(math, &math_before, NULL) == MC_MATH_CLASS_OK &&
          mc_entity_vector_get_snapshot(app->entities, &pool_before) == MC_ENTITY_OK);
    for (int32_t remaining = 1; remaining >= 0; --remaining) {
        const unsigned int saved_controls = _mm_getcsr();
        if (remaining == 1)
            _mm_setcsr(saved_controls |
                       (1u << 15)); /* Declared FTZ input, delay must not sample. */
        const mc_client_render_global_status delayed = mc_client_render_global_entities_begin(
            c->owner, NULL, NULL, 0.5f, NULL, &c->call, &c->error);
        _mm_setcsr(saved_controls);
        CHECK(delayed == MC_CLIENT_RENDER_GLOBAL_OK &&
              mc_client_render_global_entities_inspect(c->call, &c->state) && c->state.returned &&
              c->state.delayed && c->state.source_pc == 17 && c->state.last_store_pc == 14 &&
              c->state.initial_delay == remaining + 1 && c->state.remaining_delay == remaining &&
              c->owner->fields.I == remaining && !c->state.ll_returned && !c->state.th_returned &&
              c->state.ll == NULL && c->state.th == NULL && !c->owner->tile_consumer.attached &&
              !c->owner->entity_manager_consumer.attached);
        CHECK(mc_client_render_global_terminal_detach(c->owner, &c->error) ==
                  MC_CLIENT_RENDER_GLOBAL_BUSY &&
              c->owner->phase == MC_CLIENT_RENDER_GLOBAL_READY &&
              mc_client_render_global_entities_retire(&c->call, &c->error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              c->call == NULL);
    }
    CHECK(app->render_classes->ll == initial_ll && app->render_classes->th == initial_th &&
          mc_math_classes_state(math, &math_after, NULL) == MC_MATH_CLASS_OK &&
          same_math(&math_before, &math_after) &&
          mc_entity_vector_get_snapshot(app->entities, &pool_after) == MC_ENTITY_OK &&
          same_pool(&pool_before, &pool_after));
    CHECK(mc_client_render_global_entities_begin(c->owner, NULL, NULL, 0.5f, &services, &c->call,
                                                 &c->error) ==
              MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          mc_client_render_global_entities_inspect(c->call, &c->state) && !c->state.returned &&
          !c->state.delayed && c->state.source_pc == 249 && c->state.last_store_pc == 242 &&
          c->state.ll_returned && c->state.th_returned && c->state.counters_cleared &&
          c->state.origins_stored && c->state.origins_written == 6 && c->state.ll != NULL &&
          c->state.th != NULL && c->owner->fields.I == 0 && c->owner->fields.J == 0 &&
          c->owner->fields.K == 0 && c->owner->fields.L == 0 && c->owner->tile_consumer.attached &&
          c->owner->entity_manager_consumer.attached);
    CHECK(mc_client_tile_entity_renderer_classes_existing(app->render_classes, app->registry,
                                                          app->world_classes, &c->tiles,
                                                          NULL) == MC_CLIENT_TILE_RENDERER_OK &&
          c->tiles != NULL && mc_client_tile_renderer_inspect(c->tiles, &c->tiles_state) &&
          mc_client_render_manager_inspect(services.manager_classes, &c->manager_state) &&
          c->state.ll == c->tiles_state.a && c->state.th == c->manager_state.a &&
          c->tiles_state.fields.f == NULL && c->tiles_state.fields.g == app->i &&
          c->tiles_state.fields.e == app->render_engine &&
          c->tiles_state.fields.n == app->font_renderer && c->manager_state.fields.g == NULL &&
          c->manager_state.fields.h == app->i && c->manager_state.fields.e == app->render_engine &&
          c->manager_state.fields.p == app->font_renderer &&
          c->manager_state.fields.k == app->settings && c->tiles_state.b == c->manager_state.b &&
          c->tiles_state.c == c->manager_state.c && c->tiles_state.d == c->manager_state.d);
    CHECK(mc_client_render_global_entities_retire(&c->call, &c->error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          c->call == NULL);

    CHECK(mc_client_render_global_world_begin(c->owner, app->active_world, &c->attachment) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_world_inspect(c->attachment, &c->world_state) &&
          c->world_state.source_returned && c->world_state.rebuild_returned &&
          c->owner->fields.k == app->active_world && c->owner->fields.I == 2 &&
          c->owner->tile_consumer.attached && c->owner->entity_manager_consumer.attached);
    CHECK(mc_client_render_global_world_retire(&c->attachment) == MC_CLIENT_RENDER_GLOBAL_OK &&
          c->attachment == NULL);
    for (int32_t remaining = 1; remaining >= 0; --remaining) {
        CHECK(mc_client_render_global_entities_begin(c->owner, NULL, NULL, 0.5f, NULL, &c->call,
                                                     &c->error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              mc_client_render_global_entities_inspect(c->call, &c->state) && c->state.delayed &&
              c->state.returned && c->state.source_pc == 17 &&
              c->state.initial_delay == remaining + 1 && c->state.remaining_delay == remaining &&
              c->owner->fields.I == remaining);
        CHECK(mc_client_render_global_entities_retire(&c->call, &c->error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              c->call == NULL);
    }

    const mc_world_reference_list *ordinary = NULL, *weather = NULL;
    int32_t entity_count = -1, weather_count = -1;
    CHECK(mc_world_runtime_render_entities_list(app->active_world, &ordinary, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_render_weather_list(app->active_world, &weather, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_render_entity_list_count(app->active_world, ordinary, &entity_count,
                                                    NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_render_entity_list_count(app->active_world, weather, &weather_count,
                                                    NULL) == MC_WORLD_STORAGE_OK &&
          entity_count > 0 && weather_count == 0 && c->owner->fields.a != NULL &&
          c->owner->fields.a->count == 0);
    for (int32_t i = 0; i < entity_count; ++i) {
        mc_entity *actual = NULL;
        CHECK(mc_world_runtime_render_entity_list_get(app->active_world, ordinary, i, &actual,
                                                      NULL) == MC_WORLD_STORAGE_OK &&
              actual == app->i);
    }
    CHECK(mc_game_settings_read_state(app->settings, &settings));
    mc_game_settings_values first_person = settings.values;
    first_person.third_person_view = false;
    CHECK(mc_game_settings_set_values(app->settings, &first_person, NULL) == MC_GAME_SETTINGS_OK);
    settings_changed = true;
    CHECK(mc_entity_living_position(app->i, 0.5f, &c->eye, &c->view) == MC_ENTITY_VIEW_OK &&
          c->eye != NULL &&
          mc_client_source_frustum_begin(app->render_classes, c->owner->classes, app->display, math,
                                         &c->frustum, &c->culling) == MC_CLIENT_FRUSTUM_OK &&
          mc_client_source_frustum_recenter(c->frustum, c->eye->x, c->eye->y, c->eye->z,
                                            &c->culling) == MC_CLIENT_FRUSTUM_OK);
    bool visible = false, near = false, sleeping = true;
    CHECK(mc_client_source_frustum_bounds(c->frustum, app->i->bounds, &visible, &c->culling) ==
              MC_CLIENT_FRUSTUM_OK &&
          mc_entity_render_distance(app->i, c->eye, &near, NULL) == MC_ENTITY_RENDER_DISTANCE_OK &&
          mc_entity_living_is_sleeping(app->i, &sleeping, NULL) == MC_ENTITY_SLEEPING_OK &&
          !sleeping);
    const mc_entity_base_fields *actual_fields = mc_entity_runtime_fields(app->i);
    CHECK(actual_fields != NULL);
    const size_t expected_frustum_checks = near && !actual_fields->bM ? (size_t)entity_count : 0;
    const size_t expected_camera_skips =
        near && (actual_fields->bM || visible) ? (size_t)entity_count : 0;
    CHECK(mc_client_render_global_entities_begin(c->owner, c->eye, c->frustum, 0.5f, &services,
                                                 &c->call,
                                                 &c->error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_entities_inspect(c->call, &c->state) && c->state.returned &&
          !c->state.delayed && c->state.source_pc == 569 && c->state.ll_returned &&
          c->state.th_returned && c->state.origins_stored && c->state.origins_written == 6 &&
          c->state.entities_list == ordinary && c->state.entities_visited == (size_t)entity_count &&
          c->state.weather_visited == 0 && c->state.tiles_visited == 0 &&
          c->state.distance_checks == (size_t)entity_count &&
          c->state.frustum_checks == expected_frustum_checks &&
          c->state.camera_skips == expected_camera_skips && c->state.presence_checks == 0 &&
          c->owner->fields.J == entity_count && c->owner->fields.K == 0 && c->owner->fields.L == 0);
    observed_frustum_checks = c->state.frustum_checks;
    observed_camera_skips = c->state.camera_skips;
    CHECK(mc_client_tile_renderer_consumer_attach(c->tiles, &c->foreign_tiles, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          mc_client_render_manager_consumer_attach(services.manager_classes, &c->foreign_manager,
                                                   NULL) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_global_terminal_detach(c->owner, &c->error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          c->owner->tile_consumer.attached && c->owner->entity_manager_consumer.attached);
    CHECK(mc_client_render_global_entities_retire(&c->call, &c->error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          c->call == NULL);
    CHECK(mc_client_source_frustum_retire(&c->frustum, &c->culling) == MC_CLIENT_FRUSTUM_OK &&
          c->frustum == NULL && mc_entity_vector_release(app->entities, c->eye) == MC_ENTITY_OK);
    c->eye = NULL;
    CHECK(mc_game_settings_set_values(app->settings, &settings.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    settings_changed = false;
    CHECK(mc_client_tile_renderer_inspect(c->tiles, &c->tiles_state) &&
          mc_client_render_manager_inspect(services.manager_classes, &c->manager_state));
    const size_t tile_consumers = c->tiles_state.native_consumers;
    const size_t manager_consumers = c->manager_state.native_consumers;
    CHECK(tile_consumers >= 2 && manager_consumers >= 3 &&
          app->startup->manager_consumer.attached &&
          mc_client_render_global_terminal_detach(c->owner, &c->error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          !c->owner->tile_consumer.attached && !c->owner->entity_manager_consumer.attached &&
          c->foreign_tiles.attached && c->foreign_manager.attached &&
          app->startup->manager_consumer.attached &&
          mc_client_tile_renderer_inspect(c->tiles, &c->tiles_state) &&
          mc_client_render_manager_inspect(services.manager_classes, &c->manager_state) &&
          c->tiles_state.native_consumers == tile_consumers - 1 &&
          c->manager_state.native_consumers == manager_consumers - 1 &&
          c->tiles_state.fields.f == app->active_world && c->tiles_state.fields.g == app->i &&
          c->manager_state.fields.g == app->active_world && c->manager_state.fields.h == app->i);
    CHECK(mc_client_tile_renderer_consumer_detach(&c->foreign_tiles, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          mc_client_render_manager_consumer_detach(&c->foreign_manager, NULL) ==
              MC_CLIENT_MANAGER_OK &&
          mc_client_render_global_retire(&c->owner, &c->error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          c->owner == NULL);
    result = 0;
cleanup:
    if (c != NULL) {
        if (mc_client_render_global_entities_retire(&c->call, &c->error) !=
                MC_CLIENT_RENDER_GLOBAL_OK ||
            mc_client_render_global_world_retire(&c->attachment) != MC_CLIENT_RENDER_GLOBAL_OK)
            result = 1;
        if (mc_client_source_frustum_retire(&c->frustum, &c->culling) != MC_CLIENT_FRUSTUM_OK)
            result = 1;
        if (c->eye != NULL && mc_entity_vector_release(app->entities, c->eye) != MC_ENTITY_OK)
            result = 1;
        if (settings_changed && mc_game_settings_set_values(app->settings, &settings.values,
                                                            NULL) != MC_GAME_SETTINGS_OK)
            result = 1;
        if (c->owner != NULL && mc_client_render_global_terminal_detach(c->owner, &c->error) !=
                                    MC_CLIENT_RENDER_GLOBAL_OK)
            result = 1;
        if (mc_client_tile_renderer_consumer_detach(&c->foreign_tiles, NULL) !=
                MC_CLIENT_TILE_RENDERER_OK ||
            mc_client_render_manager_consumer_detach(&c->foreign_manager, NULL) !=
                MC_CLIENT_MANAGER_OK ||
            mc_client_render_global_retire(&c->owner, &c->error) != MC_CLIENT_RENDER_GLOBAL_OK)
            result = 1;
        free(c);
    }
    if (result == 0)
        printf("N_ENTITIES native-only=1 source-ctor-I2=1 delay-ignores-fp=1 null-World249=1 "
               "rebuild-I2=1 "
               "nonempty-Player-list=1 frustum-checks=%zu camera-skips=%zu returned=569 "
               "exact-consumers=1 checks=%u\n",
               observed_frustum_checks, observed_camera_skips, checks);
    return result;
}
