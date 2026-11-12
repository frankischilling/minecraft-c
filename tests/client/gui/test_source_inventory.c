/* CPU Inventory class/constructor prefixes; no App, Display or original assets. */
#include "client/gui/source_inventory_screen_internal.h"
#include "client/gui/source_screen_events.h"
#include "entity/player/player_internal.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Inventory lifecycle check%u line%d: %s\n", checks, __LINE__, #value); \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *models = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_screen_error error = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK &&
          mc_client_render_classes_create(entities, profile, &models) == MC_CLIENT_MODEL_OK &&
          mc_client_screen_classes_create(profile, NULL, NULL, &classes, &error) ==
              MC_CLIENT_SCREEN_OK);
    mc_client_inventory_class_state id;
    CHECK(mc_client_screen_inventory_class_inspect(classes, &id, &error) == MC_CLIENT_SCREEN_OK &&
          id.phase == MC_CLIENT_SCREEN_CLASS_COLD && id.renderer == NULL &&
          id.pending_renderer == NULL);
    mc_client_screen_options budget = {0};
    mc_client_source_ue *screen = NULL;
    CHECK(mc_client_screen_inventory_allocate(classes, models, &budget, &screen, &error) ==
              MC_CLIENT_SCREEN_LIMIT &&
          screen == NULL);
    CHECK(mc_client_screen_inventory_class_inspect(classes, &id, &error) == MC_CLIENT_SCREEN_OK &&
          id.phase == MC_CLIENT_SCREEN_CLASS_READY && id.renderer != NULL &&
          id.pending_renderer == NULL && id.last_store_pc == 7 && id.last_pc == 10 &&
          mc_client_item_renderer_phase(id.renderer) == MC_CLIENT_MODEL_READY);
    mc_client_item_renderer *actual = id.renderer, *other = NULL;
    mc_client_model_error model_error;
    CHECK(mc_client_item_renderer_allocate(models, 0, &other) == MC_CLIENT_MODEL_OK &&
          mc_client_item_renderer_construct(other, &model_error) == MC_CLIENT_MODEL_OK &&
          other != actual && mc_client_item_renderer_free(&other) == MC_CLIENT_MODEL_OK);
    mc_client_screen_class_state fu;
    CHECK(mc_client_screen_classes_inspect(classes, &fu, &error) == MC_CLIENT_SCREEN_OK &&
          fu.phase == MC_CLIENT_SCREEN_CLASS_COLD && fu.a == NULL && fu.pending_random == NULL);
    CHECK(mc_client_screen_inventory_allocate(classes, models, NULL, &screen, &error) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_inventory_item_renderer(classes) == actual);
    bool pauses = true;
    CHECK(mc_client_screen_pauses_game(mc_client_screen_inventory_base(screen), &pauses, &error) ==
              MC_CLIENT_SCREEN_OK &&
          !pauses && error.source_returned && strcmp(error.site.class_name, "id") == 0 &&
          error.site.pc == 1);
    CHECK(mc_client_screen_inventory_construct(screen, NULL, &error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !error.native_policy && strcmp(error.site.class_name, "ue") == 0 && error.site.pc == 2);
    mc_client_screen_state base;
    CHECK(mc_client_screen_inspect(mc_client_screen_inventory_base(screen), &base, &error) ==
              MC_CLIENT_SCREEN_OK &&
          base.phase == MC_CLIENT_SCREEN_FAILED && base.kind == MC_CLIENT_SCREEN_INVENTORY &&
          base.da.e == NULL && !base.da.f && base.id.a == 0 && base.id.i == 0 && base.id.j == NULL);
    CHECK(mc_client_screen_inventory_retire(&screen, &error) == MC_CLIENT_SCREEN_OK &&
          screen == NULL);

    mc_entity *player = NULL;
    CHECK(mc_local_player_allocate(entities, &player, NULL) == MC_ENTITY_OK &&
          player->phase == MC_ENTITY_RAW && player->local_player->fields.player.d == NULL &&
          player->local_player->fields.player.e == NULL);
    const size_t holds = player->holds;
    CHECK(mc_client_screen_inventory_allocate(classes, models, NULL, &screen, &error) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_inventory_construct(screen, player, &error) ==
              MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY &&
          error.native_policy && strcmp(error.site.class_name, "ue") == 0 && error.site.pc == 14);
    mc_client_inventory_screen_state state;
    CHECK(mc_client_screen_inventory_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_FAILED && state.id.a == 176 && state.id.i == 166 &&
          state.id.j == NULL && state.constructor_player == player &&
          state.constructor_container == NULL && state.player_held && !state.container_held &&
          state.ue.l == 0 && state.ue.m == 0 && player->holds == holds + 1);
    CHECK(mc_client_screen_inspect(mc_client_screen_inventory_base(screen), &base, &error) ==
              MC_CLIENT_SCREEN_OK &&
          base.da.e != NULL && base.da.f && base.id.a == 176 && base.id.i == 166 &&
          base.id.j == NULL && player->phase == MC_ENTITY_RAW &&
          player->local_player->fields.player.e == NULL);
    /* Enter UE.b's own clear body on the retained constructor prefix. There is
     * no fabricated READY transition and no implicit ID.b Container store. */
    const mc_client_screen_list *list = base.da.e;
    const int32_t mod_count = list->mod_count;
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool cleared = mc_screen_container_initialize_locked(&screen->container, &error);
    mtx_unlock(&classes->mutex);
    CHECK(cleared && list->size == 0 && list->mod_count == mod_count + 1 &&
          screen->container.base.phase == MC_CLIENT_SCREEN_FAILED &&
          screen->container.base.fields.f && player->local_player->fields.player.d == NULL &&
          player->local_player->fields.player.e == NULL);
    mc_entity *close_player = NULL;
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool escaped =
        mc_screen_container_key_locked(&screen->container, 1, &close_player, &error);
    mtx_unlock(&classes->mutex);
    CHECK(!escaped && error.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !error.native_policy && error.site.pc == 26 && close_player == NULL);
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool keyed =
        mc_screen_container_key_locked(&screen->container, 18, &close_player, &error);
    mtx_unlock(&classes->mutex);
    CHECK(!keyed && error.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE && error.site.pc == 10);
    CHECK(mc_client_screen_closed(mc_client_screen_inventory_base(screen), &error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !error.native_policy && strcmp(error.site.class_name, "id") == 0 && error.site.pc == 4);
    const mc_client_screen_error source_failure = screen->container.base.failure;
    /* Attach an actual failed OB invocation as native retained work on this
     * constructor prefix. No READY/source continuation is fabricated. */
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(entities, profile, 0, &controller, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_retain(controller) == MC_SINGLEPLAYER_CONTROLLER_OK);
    screen->container.calls =
        mc_screen_allocate(&screen->container.base.memory, sizeof(*screen->container.calls));
    CHECK(screen->container.calls != NULL);
    screen->container.calls->controller = controller;
    CHECK(mc_player_container_close_begin(NULL, NULL, player, 0, &screen->container.calls->call,
                                          &screen->container.calls->failure) ==
              MC_ENTITY_STATE_ERROR &&
          screen->container.calls->failure.source_exception &&
          screen->container.calls->failure.site.pc == 5 && player->holds == holds + 2);
    mc_player_container_call *failed_call = screen->container.calls->call;
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    ++screen->container.base.active_calls;
    const bool active_prepared =
        mc_screen_container_terminal_prepare_locked(&screen->container, &error);
    --screen->container.base.active_calls;
    mtx_unlock(&classes->mutex);
    CHECK(!active_prepared && error.status == MC_CLIENT_SCREEN_BUSY &&
          screen->container.base.phase == MC_CLIENT_SCREEN_FAILED &&
          screen->container.calls->call == failed_call && player->holds == holds + 2);
    mc_client_source_da *external = mc_client_screen_inventory_base(screen);
    CHECK(mc_client_screen_retain(external, &error) == MC_CLIENT_SCREEN_OK);
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool external_prepared =
        mc_screen_container_terminal_prepare_locked(&screen->container, &error);
    mtx_unlock(&classes->mutex);
    CHECK(!external_prepared && error.status == MC_CLIENT_SCREEN_BUSY &&
          screen->container.calls->call == failed_call && player->holds == holds + 2);
    CHECK(mc_client_screen_release(&external, &error) == MC_CLIENT_SCREEN_OK && external == NULL);
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool prepared = mc_screen_container_terminal_prepare_locked(&screen->container, &error);
    mtx_unlock(&classes->mutex);
    mc_singleplayer_controller_snapshot controller_state;
    CHECK(prepared && error.status == MC_CLIENT_SCREEN_OK &&
          screen->container.base.phase == MC_CLIENT_SCREEN_RETIRING &&
          screen->container.calls->call == NULL && screen->container.calls->controller == NULL &&
          !screen->container.player_held && !screen->container.container_held &&
          player->holds == holds && screen->container.calls->failure.source_exception &&
          screen->container.calls->failure.site.pc == 5 &&
          mc_client_singleplayer_controller_read(controller, &controller_state) &&
          controller_state.native_holds == 1);
    CHECK(screen->container.base.failure.status == source_failure.status &&
          screen->container.base.failure.operation == source_failure.operation &&
          screen->container.base.failure.site.class_name == source_failure.site.class_name &&
          screen->container.base.failure.site.pc == source_failure.site.pc &&
          screen->container.base.fields.e == list && screen->container.base.fields.f &&
          screen->container.id.a == 176 && screen->container.id.i == 166 &&
          screen->container.id.j == NULL && player->phase == MC_ENTITY_RAW &&
          player->local_player->fields.player.d == NULL &&
          player->local_player->fields.player.e == NULL);
    CHECK(mc_client_screen_inventory_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_RETIRING && state.id.j == NULL &&
          state.constructor_player == NULL && state.constructor_container == NULL);
    mc_client_sdk_key_event key_event = {0};
    mc_client_screen_event_report report;
    mc_client_screen_event_error event_error;
    CHECK(mc_client_screen_key_event(mc_client_screen_inventory_base(screen), &key_event, &report,
                                     &event_error) == MC_CLIENT_SCREEN_BUSY);
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool repeated = mc_screen_container_terminal_prepare_locked(&screen->container, &error);
    mtx_unlock(&classes->mutex);
    CHECK(repeated && player->holds == holds &&
          screen->container.base.failure.site.pc == source_failure.site.pc);
    CHECK(mc_client_screen_inventory_retire(&screen, &error) == MC_CLIENT_SCREEN_OK &&
          screen == NULL && player->holds == holds && player->phase == MC_ENTITY_RAW);
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          controller == NULL);
    CHECK(mc_client_screen_inventory_class_inspect(classes, &id, &error) == MC_CLIENT_SCREEN_OK &&
          id.renderer == actual);
    mc_entity_release(player);
    CHECK(mc_client_screen_classes_release(&classes, &error) == MC_CLIENT_SCREEN_OK &&
          classes == NULL);
    CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    printf("Inventory lifecycle CPU controls: %u checks; distinct ID BB, NULL/RAW prefixes, UE "
           "clear, terminal admission/failed-child cleanup\n",
           checks);
    return 0;
}
