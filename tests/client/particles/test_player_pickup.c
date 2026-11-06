/* Reuse the complete production App fixture, including real Session, writer,
 * Registry, World and canonical LocalPlayer. Its original main is not run. */
#define main mc_particle_app_fixture_main
#include "../statistics/test_player_dispatch.c"
#undef main
#include "../../../src/client/particles/application.h"
#include "../../../src/client/particles/effect_renderer_internal.h"
#include "../../../src/entity/particles/particle_internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "mc/entity/item/pickup.h"
#include "mc/entity/player/pickup.h"
#include "mc/inventory/runtime/insertion.h"

static bool particle_cases(fixture *f) {
    mc_player_pickup_error error;
    const size_t held = f->player->holds;
    const int32_t next = f->app.entities->next_id;
    client_application *app = f->player->local_player->fields.minecraft;
    f->player->local_player->fields.minecraft = NULL;
    const mc_entity_status no_app =
        mc_local_player_pickup_notification(f->player, f->player, 3, &error);
    f->player->local_player->fields.minecraft = app;
    CHECK(no_app == MC_ENTITY_STATE_ERROR && error.notification_pc == 4 &&
          f->app.entities->next_id == next && f->player->holds == held);
    /* Real null j still evaluates new em and its arguments. App.f is genuinely
     * null here while this Player has a nonnull World; never substitute it. */
    CHECK(f->app.active_world == NULL && f->player->base.world == f->world);
    CHECK(mc_local_player_pickup_notification(f->player, f->player, 3, &error) ==
              MC_ENTITY_STATE_ERROR &&
          error.notification_pc == 25 && f->app.entities->next_id == mc_java_i32_add(next, 1) &&
          f->player->holds == held);
    CHECK(mc_local_player_pickup_notification(f->player, NULL, 3, &error) ==
              MC_ENTITY_STATE_ERROR &&
          error.notification_pc == 22 && error.particle.pickup_pc == 3 &&
          f->app.entities->next_id == mc_java_i32_add(next, 1));
    CHECK(mc_client_application_construct_effect_renderer(app, NULL, NULL) ==
              MC_EFFECT_RENDERER_OK &&
          app->effect_renderer == app->effect_renderer_retained &&
          app->effect_renderer->world == NULL && app->effect_renderer->render_engine == NULL);
    CHECK(mc_client_application_construct_effect_renderer(app, NULL, NULL) ==
          MC_EFFECT_RENDERER_INVALID_PHASE);
    CHECK(mc_local_player_pickup_notification(f->player, f->player, -999, &error) == MC_ENTITY_OK &&
          app->effect_renderer->layers[3]->size == 1 && f->player->holds == held + 2);
    mc_entity *first = mc_client_effect_renderer_particle(app->effect_renderer, 3, 0);
    CHECK(first != NULL && first->base.world == NULL && first->particle->pickup.a == f->player &&
          first->particle->pickup.o == f->player && first->particle->pickup.r == -.5f &&
          first->particle->pickup.q == 3 && first->particle->pickup.p == 0);
    CHECK(mc_client_effect_renderer_tick(app->effect_renderer, NULL) == MC_EFFECT_RENDERER_OK &&
          first->particle->pickup.p == 1);
    CHECK(mc_client_effect_renderer_tick(app->effect_renderer, NULL) == MC_EFFECT_RENDERER_OK &&
          first->particle->pickup.p == 2);
    CHECK(mc_client_effect_renderer_tick(app->effect_renderer, NULL) == MC_EFFECT_RENDERER_OK &&
          app->effect_renderer->layers[3]->size == 0 && f->player->holds == held);
    CHECK(mc_client_application_publish_world(app, f->world) == MC_EFFECT_RENDERER_OK &&
          app->active_world == f->world && app->effect_renderer->world == NULL);
    CHECK(mc_local_player_pickup_notification(f->player, f->player, 0, &error) == MC_ENTITY_OK);
    mc_entity *second = mc_client_effect_renderer_particle(app->effect_renderer, 3, 0);
    CHECK(second != NULL && second->base.world == f->world);
    CHECK(mc_client_effect_renderer_change_world(app->effect_renderer, f->world, NULL) ==
              MC_EFFECT_RENDERER_OK &&
          app->effect_renderer->layers[3]->size == 0 && f->player->holds == held);
    /* A normal Item pickup now reaches its genuine post-notification death.
     * The Item survives solely through the queued particle after caller release. */
    mc_item_stack *stack = NULL;
    mc_entity *item = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 262, 5, 0) == MC_ITEM_STACK_OK);
    CHECK(mc_item_entity_allocate(app->entities, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, f->world, 8, 200, 8, stack, NULL) == MC_ENTITY_OK &&
          mc_entity_item_pickup_delay_set(item, 0) == MC_ENTITY_OK);
    mc_item_pickup_error pickup;
    CHECK(mc_item_entity_pickup(item, f->player, app->registry, NULL, &pickup) == MC_ENTITY_OK &&
          item->base.be && app->effect_renderer->layers[3]->size == 1 && item->holds == 2);
    mc_item_stack_fields remainder;
    CHECK(mc_item_stack_fields_get(stack, &remainder) == MC_ITEM_STACK_OK && remainder.count == 0);
    mc_entity *effect = mc_client_effect_renderer_particle(app->effect_renderer, 3, 0);
    CHECK(effect->particle->pickup.a == item && effect->particle->pickup.o == f->player &&
          effect->base.world == app->active_world);
    const size_t before_retirement = app->entities->live_entities;
    mc_entity_release(item);
    mc_item_stack_release(stack);
    CHECK(app->entities->live_entities == before_retirement);
    CHECK(mc_client_effect_renderer_tick(app->effect_renderer, NULL) == MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_tick(app->effect_renderer, NULL) == MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_tick(app->effect_renderer, NULL) == MC_EFFECT_RENDERER_OK &&
          app->entities->live_entities == before_retirement - 2 && f->player->holds == held &&
          app->effect_renderer->layers[3]->size == 0);
    CHECK(mc_client_application_retire_effect_renderer(app) == MC_EFFECT_RENDERER_OK &&
          app->effect_renderer == NULL && app->effect_renderer_retained == NULL);
    CHECK(mc_client_application_publish_world(app, NULL) == MC_EFFECT_RENDERER_OK);
    /* Failed actual manager construction never publishes j; the private
     * candidate persists and blocks retry until explicit retirement. */
    const mc_effect_renderer_options none = {sizeof(mc_client_effect_renderer)};
    CHECK(mc_client_application_construct_effect_renderer(app, &none, NULL) ==
              MC_EFFECT_RENDERER_LIMIT &&
          app->effect_renderer == NULL && app->effect_renderer_retained != NULL &&
          app->effect_renderer_retained->phase == MC_EFFECT_RENDERER_FAILED_CONSTRUCTION);
    CHECK(mc_client_application_construct_effect_renderer(app, NULL, NULL) ==
          MC_EFFECT_RENDERER_INVALID_PHASE);
    CHECK(mc_client_application_retire_effect_renderer(app) == MC_EFFECT_RENDERER_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "particles", true) || !particle_cases(f) || !teardown(f))
        return 1;
    free(f);
    printf("Player pickup particles: %zu checks\n", checks);
    return 0;
}
