/* CPU OO/ID lifecycle prefixes over genuine namespace/model/Container owners.
 * The separate entered ID component below supplies declared App/Player fields;
 * it does not credit OO constructor completion, Display or a graphical frame. */
#include "client/application_state.h"
#include "client/gui/source_screen_events.h"
#include "client/gui/source_screen_transition_receipts_internal.h"
#include "client/gui/source_workbench_screen_internal.h"
#include "entity/player/player_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Workbench lifecycle check%u line%d: %s\n", checks, __LINE__, #value); \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_registry_classes *registry = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *models = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_screen_error error;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_materials_create(&materials));
    const mc_registry_options options = {
        .side = MC_REGISTRY_CLIENT, .random_profile = profile, .materials = materials};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK &&
          mc_client_render_classes_create(entities, profile, &models) == MC_CLIENT_MODEL_OK &&
          mc_client_screen_classes_create(profile, NULL, registry, &classes, &error) ==
              MC_CLIENT_SCREEN_OK);
    mc_client_source_oo *screen = NULL;
    CHECK(mc_client_screen_workbench_allocate(classes, models, NULL, &screen, &error) ==
              MC_CLIENT_SCREEN_OK &&
          screen != NULL && mc_client_screen_inventory_item_renderer(classes) != NULL);
    bool pauses = true;
    CHECK(mc_client_screen_pauses_game(mc_client_screen_workbench_base(screen), &pauses, &error) ==
              MC_CLIENT_SCREEN_OK &&
          !pauses && strcmp(error.site.class_name, "id") == 0 && error.site.pc == 1);
    CHECK(mc_client_screen_workbench_construct(screen, NULL, NULL, -7, 11, 29, &error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !error.native_policy && error.source_exception && !error.source_error &&
          strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
          strcmp(error.site.class_name, "oo") == 0 && error.site.pc == 12 &&
          error.inventory != NULL && error.inventory->ownership.frame_count != 0);
    mc_client_workbench_screen_state state;
    mc_container_state container;
    CHECK(mc_client_screen_workbench_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_FAILED && state.container_owned &&
          state.constructor_container != NULL && state.id.j == NULL && state.id.a == 0 &&
          state.id.i == 0 && screen->container.base.fields.e == NULL &&
          mc_container_read_state(state.constructor_container, &container) &&
          container.constructor_entered && container.constructor_failed &&
          container.constructor_pc == 57 && !container.constructed && container.crafting != NULL &&
          container.result != NULL && container.world == NULL && container.x == -7 &&
          container.y == 11 && container.z == 29);
    mc_container *actual = state.constructor_container;
    const mc_client_screen_error failed = *state.failure;
    CHECK(mc_client_screen_workbench_construct(screen, NULL, NULL, 1, 2, 3, &error) ==
              MC_CLIENT_SCREEN_INVALID_PHASE &&
          screen->container.captured_container == actual &&
          mc_container_read_state(actual, &container) && container.x == -7 && container.z == 29 &&
          screen->container.base.failure.status == failed.status &&
          screen->container.base.failure.source_exception &&
          screen->container.base.failure.site.pc == 12);
    CHECK(mc_client_screen_workbench_retire(&screen, &error) == MC_CLIENT_SCREEN_OK &&
          screen == NULL && failed.source_exception && failed.site.pc == 12);

    CHECK(mc_client_screen_workbench_allocate(classes, models, NULL, &screen, &error) ==
          MC_CLIENT_SCREEN_OK);
    mc_client_source_id *id = &screen->container;
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool base_returned = mc_screen_container_construct_locked(id, NULL, &error);
    mtx_unlock(&classes->mutex);
    CHECK(base_returned && id->base.phase == MC_CLIENT_SCREEN_RAW && id->id.a == 176 &&
          id->id.i == 166 && id->id.j == NULL && id->base.fields.e != NULL && !id->base.fields.f);
    mc_entity *player = NULL;
    mc_container *raw = NULL;
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL && mc_local_player_allocate(entities, &player, NULL) == MC_ENTITY_OK &&
          player->phase == MC_ENTITY_RAW &&
          mc_container_workbench_allocate(MC_REGISTRY_CLIENT, registry, NULL, &raw, NULL) ==
              MC_INVENTORY_OK);
    const size_t holds = player->holds;
    player->local_player->fields.player.e = raw; /* Declared ID.b store receiver input. */
    app->h = player;
    id->base.fields.b = app; /* Stable RAW App operand contract; no registered App claim. */
    const int32_t modifications = id->base.fields.e->mod_count;
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool initialized = mc_screen_container_initialize_locked(id, &error);
    mtx_unlock(&classes->mutex);
    CHECK(initialized && error.source_returned && error.site.pc == 18 &&
          player->local_player->fields.player.e == NULL &&
          player->local_player->fields.player.d == NULL && player->holds == holds + 1 &&
          player->phase == MC_ENTITY_RAW && id->base.fields.e->mod_count == modifications &&
          !id->base.fields.f);
    app->h = NULL; /* Fresh declared source operand for ID.h and then OO.h. */
    CHECK(mc_client_screen_closed(mc_client_screen_workbench_base(screen), &error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !error.native_policy && error.source_exception &&
          strcmp(error.site.class_name, "oo") == 0 && error.site.pc == 15 &&
          id->inherited_closed_returned && !id->direct_closed_returned &&
          id->closed_parent == NULL && id->closed_direct != NULL &&
          id->closed_direct->call != NULL && id->source_calls == 1);
    mc_player_container_call *direct = id->closed_direct->call;
    CHECK(mc_client_screen_closed(mc_client_screen_workbench_base(screen), &error) ==
              MC_CLIENT_SCREEN_BUSY &&
          error.native_policy && !error.source_exception && id->closed_direct->call == direct &&
          id->source_calls == 1);
    /* Public DA initialization correctly rejects this deliberately RAW OO
     * before any source instruction/App borrow. Preserve that assertion. */
    mc_client_screen_init_error *initialization = calloc(1, sizeof(*initialization));
    CHECK(initialization != NULL);
    const mc_client_screen_status admitted = mc_client_screen_initialize(
        mc_client_screen_workbench_base(screen), app, 320, 240, initialization);
    if (admitted != MC_CLIENT_SCREEN_INVALID_PHASE ||
        initialization->screen.site.class_name != NULL)
        fprintf(
            stderr, "Workbench RAW initialization status=%d native=%d site=%s.%s:%u op=%s\n",
            (int)admitted, initialization->screen.native_policy,
            initialization->screen.site.class_name == NULL ? "<none>"
                                                           : initialization->screen.site.class_name,
            initialization->screen.site.method == NULL ? "<none>"
                                                       : initialization->screen.site.method,
            initialization->screen.site.pc,
            initialization->screen.operation == NULL ? "<none>" : initialization->screen.operation);
    CHECK(admitted == MC_CLIENT_SCREEN_INVALID_PHASE && initialization->screen.native_policy &&
          initialization->screen.site.class_name == NULL &&
          !initialization->screen.source_exception && initialization->screen.inventory == NULL &&
          id->base.phase == MC_CLIENT_SCREEN_RAW && id->base.initialization_count == 0);
    /* Separately execute the real entered ID.b child on the declared fields.
     * Its actual failure value feeds the native receipt copy/expiry work scope;
     * no DA invocation, native initialization record, READY phase, complete OO
     * or App startup is credited by this component control. */
    *initialization = (mc_client_screen_init_error){0};
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool source_initialized =
        mc_screen_container_initialize_locked(id, &initialization->screen);
    mtx_unlock(&classes->mutex);
    CHECK(!source_initialized &&
          initialization->screen.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          initialization->screen.inventory == &id->detail &&
          strcmp(initialization->screen.site.class_name, "id") == 0 &&
          initialization->screen.site.pc == 15 && initialization->screen.source_exception &&
          id->base.phase == MC_CLIENT_SCREEN_RAW && id->base.initialization_count == 0);
    mc_client_screen_event_error event = {.screen = initialization->screen};
    event.screen.initialization = initialization;
    const size_t source_calls = id->source_calls, active_calls = id->base.active_calls,
                 player_holds = player->holds;
    /* Refusal-only native operands: the actual Screen/failed receipts stay
     * live, but no monitor ownership or source invocation is fabricated by
     * this control. An unreturned pending receiver must not consume views. */
    mc_client_screen_event_error unreturned = event;
    unreturned.pending_receiver = mc_client_screen_workbench_base(screen);
    mc_client_screen_error scalar_before;
    memcpy(&scalar_before, &unreturned.screen, sizeof(scalar_before));
    CHECK(mc_client_screen_event_retire(&unreturned) == MC_CLIENT_SCREEN_BUSY &&
          unreturned.pending_receiver == mc_client_screen_workbench_base(screen) &&
          !unreturned.monitor_lost &&
          memcmp(&scalar_before, &unreturned.screen, sizeof(scalar_before)) == 0 &&
          id->source_calls == source_calls && id->base.active_calls == active_calls &&
          player->holds == player_holds);
    /* The real completed-error/no-monitor boundary ends only its borrows;
     * it preserves every scalar byte and leaves the actual producer intact. */
    mc_client_screen_error expected;
    memcpy(&expected, &event.screen, sizeof(expected));
    expected.inventory = NULL;
    expected.initialization = NULL;
    CHECK(mc_client_screen_event_retire(&event) == MC_CLIENT_SCREEN_OK &&
          memcmp(&expected, &event.screen, sizeof(expected)) == 0 &&
          initialization->screen.inventory == &id->detail && initialization->screen.site.pc == 15 &&
          initialization->screen.source_exception && id->closed_direct->call == direct &&
          id->source_calls == source_calls && id->base.active_calls == active_calls &&
          player->holds == player_holds);
    CHECK(mc_client_screen_event_retire(&event) == MC_CLIENT_SCREEN_OK &&
          memcmp(&expected, &event.screen, sizeof(expected)) == 0);
    mc_client_screen_error retained = initialization->screen;
    retained.initialization = initialization; /* Exact enclosing native output receipt. */
    mc_screen_transition_receipts receipts = {0};
    CHECK(mc_screen_transition_initialization_capture(&receipts, &retained, initialization) &&
          mc_screen_transition_inventory_capture(&receipts, &retained, &id->detail) &&
          retained.initialization == &receipts.initialization &&
          retained.inventory == &receipts.inventory &&
          retained.initialization->screen.inventory == &receipts.inventory);
    mc_client_source_da *external = mc_client_screen_workbench_base(screen);
    CHECK(mc_client_screen_retain(external, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_workbench_retire(&screen, &error) == MC_CLIENT_SCREEN_BUSY &&
          screen != NULL && screen->container.base.phase == MC_CLIENT_SCREEN_RAW &&
          screen->container.closed_direct->call == direct);
    mc_screen_transition_initialization_after(&receipts, initialization);
    mc_screen_transition_inventory_after(&receipts, &id->detail);
    CHECK(retained.source_exception && retained.site.pc == 15 &&
          retained.initialization->screen.source_exception &&
          retained.initialization->screen.inventory == &receipts.inventory &&
          retained.inventory->direct_close.source_exception &&
          retained.inventory->direct_close.site.pc == 0);
    free(initialization); /* Consume only this actual native output's lifetime. */
    initialization = NULL;
    mc_screen_transition_initialization_after(&receipts, NULL);
    CHECK(retained.initialization->screen.source_exception &&
          retained.initialization->screen.site.pc == 15 &&
          mc_client_screen_release(&external, &error) == MC_CLIENT_SCREEN_OK && external == NULL);
    error = (mc_client_screen_error){0};
    CHECK(mc_screen_lock(classes, &error));
    const bool prepared = mc_screen_container_terminal_prepare_locked(id, &error);
    mtx_unlock(&classes->mutex);
    CHECK(prepared && id->base.phase == MC_CLIENT_SCREEN_RETIRING &&
          id->closed_direct->call == NULL && player->holds == holds &&
          mc_client_screen_closed(mc_client_screen_workbench_base(screen), &error) ==
              MC_CLIENT_SCREEN_BUSY);
    /* The actual child retirement may already have consumed nested borrows.
     * Refresh from the remaining owner before its final checked consumption. */
    mc_screen_transition_inventory_after(&receipts, &id->detail);
    CHECK(mc_client_screen_workbench_retire(&screen, &error) == MC_CLIENT_SCREEN_OK &&
          screen == NULL && mc_container_retire(&raw, NULL) == MC_INVENTORY_OK);
    mc_screen_transition_inventory_after(&receipts, NULL);
    CHECK(retained.inventory == &receipts.inventory &&
          retained.initialization->screen.inventory == &receipts.inventory &&
          retained.inventory->direct_close.source_exception &&
          retained.inventory->direct_close.site.pc == 0 &&
          retained.inventory->container.drop.publication == NULL &&
          retained.inventory->container.drop.statistic == NULL &&
          retained.initialization->screen.site.pc == 15 && retained.source_exception);
    CHECK(event.screen.inventory == NULL && event.screen.initialization == NULL &&
          event.screen.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          event.screen.source_exception && strcmp(event.screen.site.class_name, "id") == 0 &&
          event.screen.site.pc == 15 && !event.screen.native_policy);
    mc_entity_release(player);
    free(app);
    CHECK(mc_client_screen_classes_release(&classes, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK &&
          mc_entity_classes_free(entities) == MC_ENTITY_OK &&
          mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    printf("Workbench lifecycle CPU controls: %u checks; retained IT failure, inherited ID and "
           "distinct OO close; no Display or full OO return claim\n",
           checks);
    return 0;
}
