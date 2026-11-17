/* Genuine hidden SDK component: two real Players/AZ/GQ owners, entered old
 * ID receiver B with fresh App.h/Player.e selecting A, and actual native router
 * custody. Public App operands are declared fixture setup; no Source phase,
 * list, call, ownership counter or returned field is repaired. No frame/pixel
 * or physical-input claim. Each node order runs in its own fresh process. */
#include "block/registry/registry_internal.h"
#include "client/application_state.h"
#include "client/display/events.h"
#include "client/display/sdk_input.h"
#include "client/display/staged.h"
#include "client/gui/display_screen.h"
#include "client/gui/source_dispenser_screen_internal.h"
#include "client/gui/source_screen_events.h"
#include "client/gui/source_screen_transitions.h"
#include "client/runtime/bootstrap.h"
#include "client/startup/application_startup_internal.h"
#include "entity/player/player_internal.h"
#include "inventory/internal.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/statistics/writer.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/tile_entities/dispenser.h"
#include "mc/world/storage/store.h"
#include "world/runtime/world_access_internal.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Dispenser cross-owner check%u line%d: %s\n", checks, __LINE__, #c);   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_client_bootstrap *bootstrap;
    client_application *app;
    mc_client_screen_transitions *router;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_info_name *name;
    mc_world_runtime *world;
    mc_entity *players[2];
    mc_tile_entity *tiles[2];
    mc_inventory_dispenser *adapters[2];
    mc_client_source_gq *screens[2]; /* Borrowed actual router node owners. */
    mc_client_singleplayer_controller *controller;
    mc_world_access *producer;
    mc_world_access_receipt *receipt;
    char *exception;
    uintptr_t owner_id;
    size_t producer_calls;
    bool producer_observed, owner_newest;
    mc_client_screen_event_error event;
    mc_client_screen_event_report report;
    SDL_WindowID window;
} fixture;

/* Failure-only, passive diagnostics. Report the actual retained owner graph;
 * never replay a Source call or inspect an already-ended Container borrow. */
static void diagnose(fixture *f, const char *stage, mc_client_screen_status status,
                     mc_client_screen_status expected, const mc_client_screen_error *error) {
    fprintf(stderr,
            "XOWNER_DIAG stage=%s actual=%d expected=%d operation=%s site=%s.%s%s@%u "
            "native=%d exception=%d source-error=%d class=%s name-owner-A=%d\n",
            stage, (int)status, (int)expected, error->operation == NULL ? "NULL" : error->operation,
            error->site.class_name == NULL ? "NULL" : error->site.class_name,
            error->site.method == NULL ? "NULL" : error->site.method,
            error->site.descriptor == NULL ? "NULL" : error->site.descriptor, error->site.pc,
            error->native_policy, error->source_exception, error->source_error,
            error->exception_class == NULL ? "NULL" : error->exception_class,
            error->exception_class_dispenser_owner_id == f->owner_id);
    mc_client_screen_transition_state router = {0};
    if (mc_client_screen_transitions_inspect(f->router, &router)) {
        fprintf(stderr,
                "XOWNER_ROUTER order=%s active=%d retiring=%d screens=%zu calls=%zu "
                "app-player-A=%d app-player-B=%d current-screen-A=%d current-screen-B=%d\n",
                f->owner_newest ? "owner-newest" : "owner-older", router.active, router.retiring,
                router.screens, router.calls, f->app->h == f->players[0],
                f->app->h == f->players[1],
                f->app->r == mc_client_screen_dispenser_base(f->screens[0]),
                f->app->r == mc_client_screen_dispenser_base(f->screens[1]));
        const mc_client_display_screen_call *display = router.display_call;
        if (display != NULL)
            fprintf(
                stderr,
                "XOWNER_DISPLAY status=%d operation=%s site=%s.%s%s@%u child-status=%d "
                "child-operation=%s child-site=%s.%s%s@%u child-inventory-B=%d\n",
                (int)display->status, display->operation == NULL ? "NULL" : display->operation,
                display->site.class_name == NULL ? "NULL" : display->site.class_name,
                display->site.method == NULL ? "NULL" : display->site.method,
                display->site.descriptor == NULL ? "NULL" : display->site.descriptor,
                display->site.pc, (int)display->screen.status,
                display->screen.operation == NULL ? "NULL" : display->screen.operation,
                display->screen.site.class_name == NULL ? "NULL" : display->screen.site.class_name,
                display->screen.site.method == NULL ? "NULL" : display->screen.site.method,
                display->screen.site.descriptor == NULL ? "NULL" : display->screen.site.descriptor,
                display->screen.site.pc,
                display->screen.inventory == &f->screens[1]->container.detail);
    }
    for (size_t i = 0; i < 2; ++i) {
        const mc_client_source_id *screen = &f->screens[i]->container;
        const mc_container *container = screen->captured_container;
        fprintf(stderr,
                "XOWNER_SCREEN which=%zu phase=%d holds=%zu container-live=%d owned=%d "
                "ended-owner-A=%d base-name-owner-A=%d detail-name-owner-A=%d "
                "player-e-is-its-default=%d\n",
                i, (int)screen->base.phase, screen->base.holds, container != NULL,
                screen->container_owned, screen->ended_dispenser_diagnostic_owner_id == f->owner_id,
                screen->base.failure.exception_class_dispenser_owner_id == f->owner_id,
                screen->detail.container.exception_class_dispenser_owner_id == f->owner_id,
                f->players[i]->local_player->fields.player.e ==
                    f->players[i]->local_player->fields.player.d);
        if (container != NULL)
            fprintf(stderr,
                    "XOWNER_CONTAINER which=%zu kind=%d interaction-active=%d "
                    "interaction-holds=%zu terminal-sealed=%d\n",
                    i, (int)container->kind, container->interaction_active,
                    container->interaction_holds, container->terminal_sealed);
        const mc_inventory_screen_call *call = screen->calls;
        mc_player_container_state entered = {0};
        const bool inspected = call != NULL && mc_player_container_inspect(call->call, &entered);
        fprintf(stderr,
                "XOWNER_CALL which=%zu record=%d child-live=%d inspected=%d returned=%d "
                "actual-container-A=%d actual-container-B=%d actual-player-A=%d "
                "actual-player-B=%d source-pc=%d name-owner-A=%d\n",
                i, call != NULL, call != NULL && call->call != NULL, inspected,
                call != NULL && call->returned,
                inspected && entered.container == f->screens[0]->container.captured_container,
                inspected && entered.container == f->screens[1]->container.captured_container,
                inspected && entered.player == f->players[0],
                inspected && entered.player == f->players[1],
                inspected && entered.interaction != NULL ? entered.interaction->source_pc : -1,
                call != NULL && call->failure.exception_class_dispenser_owner_id == f->owner_id);
    }
}

static mc_world_storage_status cross_dirty(void *context, mc_world_runtime *world, int32_t x,
                                           int32_t y, int32_t z, mc_tile_entity *tile,
                                           mc_world_storage_error *cause,
                                           mc_world_access_failure *failure) {
    fixture *f = context;
    ++f->producer_calls;
    f->producer_observed = f->producer_calls == 1 && world == f->world && tile == f->tiles[0] &&
                           x == 8 && y == 64 && z == 8;
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, f->producer, &removed, cause);
    f->producer_observed &= status == MC_WORLD_STORAGE_OK && removed &&
                            mc_world_access_free(f->producer, NULL) == MC_WORLD_STORAGE_BUSY;
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_INVALID_DATA : status;
    const char name[] = "test/OwnedCrossScreenDispenserFailure";
    f->exception = malloc(sizeof(name));
    if (f->exception == NULL) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                          "Cross-screen producer name allocation", 0};
        return cause->status;
    }
    memcpy(f->exception, name, sizeof(name));
    status = mc_world_access_receipt_create(f->producer, NULL, &f->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    f->receipt->exception_class = f->exception;
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Cross-screen authored dirty producer", 97};
    return cause->status;
}

static bool cross_world_and_players(fixture *f) {
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t blocks[256] = {0};
    mc_registry_block_view view;
    CHECK(mc_registry_classes_inspect_blocks(f->app->registry, &view, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        blocks[i] = view.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options options = {.session_token = 6391,
                                            .block_map = blocks,
                                            .block_map_length = 256,
                                            .limits = mc_world_store_default_limits()};
    const uint16_t title[] = {'C', 'r', 'o', 's', 's', ' ', 'A', 'Z'};
    CHECK(mc_world_store_open("native-world", f->regions, &options, &f->store, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, sizeof(title) / sizeof(*title)}, SIZE_MAX,
                                  &f->name) == MC_WORLD_INFO_OK &&
          mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->app->world_classes, &limits, &f->world,
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 5143, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_local_player_allocate(f->app->entities, &f->players[i], NULL) == MC_ENTITY_OK &&
              mc_local_player_construct(f->players[i], f->world, f->app, f->app->session, 0, NULL,
                                        NULL) == MC_PLAYER_OK &&
              mc_tile_entity_allocate(f->world, MC_TILE_ENTITY_DISPENSER, &f->tiles[i]) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_construct(f->tiles[i]) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_bind_world(f->tiles[i], f->world) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_set_coordinates(f->tiles[i], 8 + (int32_t)i, 64, 8) ==
                  MC_TILE_ENTITY_OK &&
              mc_inventory_dispenser_from_tile(MC_REGISTRY_CLIENT, f->tiles[i], NULL,
                                               &f->adapters[i], NULL) == MC_INVENTORY_OK);
    }
    mc_inventory_dispenser_state a, b;
    CHECK(f->players[0] != f->players[1] && mc_inventory_dispenser_read_state(f->adapters[0], &a) &&
          mc_inventory_dispenser_read_state(f->adapters[1], &b) && a.diagnostic_owner_id != 0 &&
          b.diagnostic_owner_id != 0 && a.diagnostic_owner_id != b.diagnostic_owner_id);
    f->owner_id = a.diagnostic_owner_id;
    CHECK(mc_client_singleplayer_controller_allocate(f->app->entities, f->app->profile, 0,
                                                     &f->controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(f->controller, f->app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    /* These are explicitly declared public App operands for this component.
     * Real DC/DA/ID calls below create every GQ and publish each Player.e. */
    f->app->active_world = f->world;
    f->app->controller = f->controller;
    return true;
}

static bool cross_open(fixture *f, size_t index) {
    f->app->h = f->players[index];
    mc_client_screen_error error;
    CHECK(mc_client_screen_transitions_dispenser(f->router, f->players[index], f->adapters[index],
                                                 &error) == MC_CLIENT_SCREEN_OK);
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_DISPENSER);
    f->screens[index] = (mc_client_source_gq *)f->app->r;
    CHECK(f->players[index]->local_player->fields.player.e == f->screens[index]->container.id.j);
    return true;
}

static bool cross_poll(fixture *f) {
    mc_client_events_error events;
    mc_client_sdk_input_error input;
    mc_client_sdk_input *sdk = mc_client_display_inputs(f->app->display);
    CHECK(mc_client_events_retain_pending(mc_client_display_event_port(f->app->display), &events) ==
              MC_CLIENT_EVENTS_OK &&
          mc_client_sdk_mouse_poll(sdk, &input) == MC_CLIENT_SDK_INPUT_OK &&
          mc_client_sdk_keyboard_poll(sdk, &input) == MC_CLIENT_SDK_INPUT_OK);
    return true;
}

static bool cross_run(fixture *f, const char *assets) {
    const mc_client_application_options options = {.assets = assets,
                                                   .username = "Dispenser custody",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = true};
    CHECK(mc_client_bootstrap_open(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f->app = mc_client_bootstrap_app(f->bootstrap);
    CHECK(f->app != NULL && f->app->constructor_phase == MC_CLIENT_APP_READY &&
          f->app->active_world == NULL && f->app->h == NULL && f->app->controller == NULL &&
          f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU && !f->app->N);
    SDL_Window *window = mc_client_display_window(f->app->display);
    f->window = SDL_GetWindowID(window);
    CHECK(f->window != 0 && (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) != 0);
#ifdef _WIN32
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    CHECK(displays != NULL && count >= 2);
    const SDL_DisplayID monitor = displays[1];
    SDL_free(displays);
    CHECK(SDL_SetWindowPosition(window, (int)SDL_WINDOWPOS_CENTERED_DISPLAY(monitor),
                                (int)SDL_WINDOWPOS_CENTERED_DISPLAY(monitor)) &&
          SDL_GetDisplayForWindow(window) == monitor);
#endif
    CHECK(mc_client_stats_writer_join(f->app->stats_writer, NULL) == MC_CLIENT_STATS_OK &&
          mc_client_screen_transitions_create(f->app, &f->router, &f->event.screen) ==
              MC_CLIENT_SCREEN_OK &&
          cross_poll(f));
    CHECK(mc_client_screen_handle_input_with_transitions(
              f->app->r, mc_client_display_inputs(f->app->display), f->router, &f->report,
              &f->event) == MC_CLIENT_SCREEN_OK &&
          f->report.click_calls == 0 &&
          mc_client_screen_event_retire(&f->event) == MC_CLIENT_SCREEN_OK);
    CHECK(cross_world_and_players(f));
    CHECK(cross_open(f, f->owner_newest ? 1u : 0u));
    CHECK(cross_open(f, f->owner_newest ? 0u : 1u));
    mc_client_source_id *b = &f->screens[1]->container;
    CHECK(b->constructor_dispenser == f->adapters[1]);
    f->app->h = f->players[0]; /* Fresh ID reads intentionally select the other actual Player.e. */
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = cross_dirty};
    CHECK(mc_world_access_source_create(&callbacks, f, &f->producer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, f->producer, NULL) == MC_WORLD_STORAGE_OK);
    int width = 0, height = 0;
    CHECK(SDL_GetWindowSize(window, &width, &height));
    SDL_Event click = {0};
    click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    click.button.windowID = f->window;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.down = true;
    click.button.x =
        (float)((int64_t)((b->base.fields.c - b->id.a) / 2 + 63) * width / b->base.fields.c);
    click.button.y =
        (float)((int64_t)((b->base.fields.d - b->id.i) / 2 + 18) * height / b->base.fields.d);
    CHECK(SDL_PushEvent(&click) && cross_poll(f));
    CHECK(mc_client_screen_handle_input_with_transitions(
              &b->base, mc_client_display_inputs(f->app->display), f->router, &f->report,
              &f->event) == MC_CLIENT_SCREEN_APP_FAILED);
    CHECK(f->producer_calls == 1 && f->producer_observed && f->receipt != NULL &&
          f->receipt->holds == 2 && b->calls != NULL && b->calls->call != NULL &&
          !b->calls->returned && b->detail.container.exception_class == f->exception &&
          b->detail.container.exception_class_dispenser_owner_id == f->owner_id &&
          b->base.failure.exception_class == f->exception &&
          b->base.failure.exception_class_dispenser_owner_id == f->owner_id &&
          b->calls->failure.exception_class == f->exception &&
          b->calls->failure.interaction.site.pc == 210);
    CHECK(mc_client_screen_event_retire(&f->event) == MC_CLIENT_SCREEN_OK);
    if (!f->owner_newest) {
        /* Real later DC/Display work reaches old B.h. Its retained failed ID
         * child refuses reuse and the router owns the returned B detail. */
        mc_client_screen_error error;
        const mc_client_screen_status displayed = mc_client_screen_transitions_dispenser(
            f->router, f->players[1], f->adapters[1], &error);
        if (displayed != MC_CLIENT_SCREEN_APP_FAILED)
            diagnose(f, "later-display", displayed, MC_CLIENT_SCREEN_APP_FAILED, &error);
        CHECK(displayed == MC_CLIENT_SCREEN_APP_FAILED);
        CHECK(error.native_policy && !error.source_exception && !error.source_error &&
              error.exception_class == NULL && error.site.pc == 16 &&
              error.site.class_name != NULL && strcmp(error.site.class_name, "dc") == 0);
        mc_client_screen_transition_state state;
        CHECK(mc_client_screen_transitions_inspect(f->router, &state) && state.failure != NULL &&
              state.failure->inventory == &b->detail &&
              state.failure->inventory->container.exception_class == f->exception);
        CHECK(state.display_call != NULL &&
              state.display_call->status == MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED &&
              state.display_call->site.pc == 22 &&
              state.display_call->screen.status == MC_CLIENT_SCREEN_BUSY &&
              state.display_call->screen.inventory == &b->detail &&
              state.display_call->screen.operation != NULL &&
              strcmp(state.display_call->screen.operation,
                     "Inventory retained failed source invocation") == 0);
        mc_player_container_state entered = {0};
        CHECK(mc_player_container_inspect(b->calls->call, &entered) &&
              entered.player == f->players[0] &&
              entered.container == f->screens[0]->container.captured_container &&
              entered.interaction != NULL && entered.interaction->source_pc == 210);
    }
    for (size_t i = 0; i < 2; ++i)
        CHECK(mc_player_container_restore_default(f->players[i], NULL) == MC_ENTITY_OK);
    mc_client_screen_error cleanup;
    /* In owner-newest order the native call in B holds A's actual Container.
     * Correct native preparation must drain that call before retiring A. */
    const mc_client_screen_status prepared =
        mc_client_screen_transitions_terminal_prepare(f->router, &cleanup);
    if (prepared != MC_CLIENT_SCREEN_OK)
        diagnose(f, "terminal-prepare", prepared, MC_CLIENT_SCREEN_OK, &cleanup);
    CHECK(prepared == MC_CLIENT_SCREEN_OK);
    CHECK(b->base.failure.source_exception && b->base.failure.site.pc == 179);
    CHECK(b->base.failure.exception_class == NULL);
    CHECK(b->detail.container.source_exception && b->detail.container.exception_class == NULL);
    CHECK(b->detail.container.interaction.exception_class == NULL);
    CHECK(b->detail.container.interaction.inventory.exception_class == NULL &&
          b->detail.container.interaction.inventory.dispenser == NULL);
    CHECK(b->calls->failure.source_exception && b->calls->failure.exception_class == NULL &&
          b->calls->failure.interaction.inventory.dispenser == NULL);
    if (!f->owner_newest) {
        mc_client_screen_transition_state state;
        CHECK(mc_client_screen_transitions_inspect(f->router, &state) &&
              state.failure->inventory != NULL &&
              state.failure->inventory->container.source_exception &&
              state.failure->inventory->container.exception_class == NULL &&
              state.failure->inventory->container.interaction.inventory.dispenser == NULL);
    }
    CHECK(f->receipt->holds == 2); /* External actual adapter still owns its ledger. */
    return true;
}

static bool cross_cleanup(fixture *f) {
    bool ok = mc_client_screen_event_retire(&f->event) == MC_CLIENT_SCREEN_OK;
    for (size_t i = 0; i < 2; ++i)
        if (f->players[i] != NULL &&
            mc_player_container_restore_default(f->players[i], NULL) != MC_ENTITY_OK)
            ok = false;
    /* Failed-control cleanup may discharge B's actual entered child first.
     * This is the real native owner operation, never a pointer/count repair;
     * the failed run remains failed and receives no regression PASS credit. */
    if (f->router != NULL && f->screens[1] != NULL) {
        mc_client_screen_error error = {0};
        mc_client_source_id *b = &f->screens[1]->container;
        if (!mc_screen_lock(b->base.classes, &error))
            ok = false;
        else {
            if (!mc_screen_container_terminal_prepare_locked(b, &error))
                ok = false;
            mtx_unlock(&b->base.classes->mutex);
        }
    }
    if (mc_client_screen_transitions_retire(&f->router, &f->event.screen) != MC_CLIENT_SCREEN_OK)
        return false;
    f->screens[0] = f->screens[1] = NULL;
    if (f->app != NULL) {
        f->app->h = NULL;
        f->app->active_world = NULL;
        f->app->controller = NULL; /* Actual component Source views have ended. */
    }
    if (mc_client_singleplayer_controller_free(&f->controller, NULL) !=
        MC_SINGLEPLAYER_CONTROLLER_OK)
        return false;
    for (size_t i = 0; i < 2; ++i) {
        if (mc_inventory_dispenser_retire(&f->adapters[i], NULL) != MC_INVENTORY_OK)
            return false;
        mc_tile_entity_release(f->tiles[i]);
        f->tiles[i] = NULL;
        mc_entity_release(f->players[i]);
        f->players[i] = NULL;
    }
    if (f->producer != NULL) {
        bool removed;
        if (mc_world_runtime_remove_world_access(f->world, f->producer, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            mc_world_access_receipt_retire(&f->receipt, NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_access_free(f->producer, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->producer = NULL;
        free(f->exception);
        f->exception = NULL;
    }
    if (f->world != NULL) {
        if (mc_world_runtime_retire_entity_containers(f->world, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        mc_world_provider_unbind(f->dimension);
        if (mc_world_runtime_retire_world(f->world, NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_runtime_free(f->world, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->world = NULL;
    }
    mc_world_provider_release(f->dimension);
    f->dimension = NULL;
    if (mc_world_store_free_checked(&f->store, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_regions_free(f->regions);
    f->regions = NULL;
    mc_world_info_name_release(f->name);
    f->name = NULL;
    mc_client_bootstrap_status status;
    const uint64_t deadline = SDL_GetTicks() + 5000;
    do {
        status = mc_client_bootstrap_close(&f->bootstrap);
        if (status == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(5);
    } while (status == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    return ok && status == MC_CLIENT_BOOTSTRAP_OK && f->bootstrap == NULL &&
           (f->window == 0 || SDL_GetWindowFromID(f->window) == NULL);
}

int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[2], "owner-older") != 0 && strcmp(argv[2], "owner-newest") != 0))
        return 2;
#ifndef _WIN32
    (void)cross_run;
    (void)cross_cleanup;
    fputs("Dispenser cross-owner SDK requires Windows monitor2\n", stderr);
    return 2;
#else
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    f->owner_newest = strcmp(argv[2], "owner-newest") == 0;
    const bool passed = cross_run(f, argv[1]);
    const bool retired = cross_cleanup(f);
    free(f);
    if (!passed || !retired)
        return 1;
    printf("Dispenser cross-owner SDK passed: %u checks; order=%s monitor=2 windows-closed=1\n",
           checks, argv[2]);
    return 0;
#endif
}
