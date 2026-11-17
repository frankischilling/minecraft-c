/* Loaded, unpowered AZ through genuine SDK/World/DC/GQ/SH and save/reselection.
 * Authored SDL records are input transport, not physical mouse delivery. */
int mc_dispenser_included_inventory_main(int, char **);
#define MC_INVENTORY_FIXTURE_MAIN mc_dispenser_included_inventory_main
#include "gameplay/test_inventory.c"
#undef MC_INVENTORY_FIXTURE_MAIN

#include "client/gui/source_dispenser_screen.h"
#include "mc/world/runtime/tile_entities/dispenser.h"

static mc_client_source_da *dispenser_held;
static size_t dispenser_frames, dispenser_gui_frames;

/* Failure-only reads of the retained tagged World/Player prefix. No Source
 * call, owner hold, eligibility invocation, retry or field repair is added. */
static void dispenser_tick_diagnostic(const fixture *f) {
    const mc_client_world_tick *tick = f->state.world_tick;
    if (tick == NULL || !tick->entities_entered)
        return;
    const mc_world_entity_tick_error *world = &tick->entities;
    if (world->child == MC_WORLD_ENTITY_TICK_CHILD_MOB) {
        mining_mob_diagnostics(f);
        return;
    }
    creature_tick_diagnostics(f);
    if (world->child != MC_WORLD_ENTITY_TICK_CHILD_PLAYER)
        return;
    const mc_player_tick_error *player = &world->child_error.player;
    fprintf(stderr,
            "DISPENSER_PLAYER_TICK status=%d operation=%s source=%s.%s player-pc=%u living-pc=%u "
            "local-pc=%u container-pc=%u close-pc=%u child=%d\n",
            (int)player->status, creature_diagnostic_text(player->operation),
            creature_diagnostic_text(player->source_class),
            creature_diagnostic_text(player->source_method), player->player_bytecode,
            player->living_bytecode, player->local_bytecode, player->container_bytecode,
            player->close_bytecode, (int)player->child);
    if (player->child == MC_PLAYER_TICK_CHILD_CONTAINER) {
        const mc_inventory_error *inventory = &player->child_error.container;
        fprintf(stderr,
                "DISPENSER_PLAYER_CONTAINER status=%d operation=%s source-exception=%d "
                "exception=%s frames=%zu az-producer=%d\n",
                (int)inventory->status, creature_diagnostic_text(inventory->operation),
                (int)inventory->source_exception,
                creature_diagnostic_text(inventory->exception_class), inventory->frame_count,
                inventory->dispenser != NULL ? 1 : 0);
        for (size_t i = 0; i < inventory->frame_count && i < MC_INVENTORY_ERROR_FRAMES; ++i) {
            const mc_inventory_site *site = &inventory->frames[i];
            fprintf(stderr, "DISPENSER_PLAYER_CONTAINER_FRAME index=%zu site=%s.%s%s:%" PRId32 "\n",
                    i, creature_diagnostic_text(site->class_name),
                    creature_diagnostic_text(site->method),
                    creature_diagnostic_text(site->descriptor), site->pc);
        }
    }
    mc_local_player_snapshot local = {0};
    mc_container_state active = {0};
    if (f->app->h != NULL && mc_local_player_get_snapshot(f->app->h, &local) == MC_ENTITY_OK &&
        local.player.e != NULL && mc_container_read_state(local.player.e, &active)) {
        fprintf(stderr,
                "DISPENSER_PLAYER_ACTIVE kind=%d constructed=%d active=%d same-default=%d "
                "slots=%zu app-screen-kind=%d\n",
                (int)active.kind, active.constructed ? 1 : 0, active.active ? 1 : 0,
                local.player.e == local.player.d ? 1 : 0, active.slots,
                f->app->r == NULL ? -1 : (int)f->app->r->kind);
    }
}

static int dispenser_player(fixture *f, mc_local_player_snapshot *player,
                            mc_inventory_player_state *inventory) {
    CHECK(f->app->h != NULL && mc_local_player_get_snapshot(f->app->h, player) == MC_ENTITY_OK &&
          player->player.c != NULL && mc_inventory_player_read_state(player->player.c, inventory));
    return 0;
}

static int dispenser_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0);
    const bool returned = mc_client_menu_loop_frame(f->loop);
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
    if (!returned) {
        fprintf(stderr, "Dispenser gameplay frontier %s.%s:%" PRId64 ": %s\n",
                f->state.source_class == NULL ? "native" : f->state.source_class,
                f->state.source_method == NULL ? "frame" : f->state.source_method,
                f->state.source_pc, f->state.operation == NULL ? "none" : f->state.operation);
        dispenser_tick_diagnostic(f);
        return 1;
    }
    CHECK(!f->state.failed && f->state.render_returned && f->state.clocks.frame_returned &&
          f->state.clocks.timer_ticks >= 0 && f->state.clocks.timer_ticks <= 10 &&
          f->state.clocks.tick_returns == f->state.clocks.timer_ticks);
    ++dispenser_frames;
    if (f->state.inventory != NULL && f->state.inventory->dispenser && f->state.inventory->returned)
        ++dispenser_gui_frames;
    return 0;
}

static bool dispenser_hit(fixture *f) {
    mc_entity_hit_result_view hit = {0};
    return f->app->r == NULL && f->app->N && f->app->y != NULL &&
           mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK && hit.fields.b == 0 &&
           hit.fields.c == 64 && hit.fields.d == 2;
}

static int dispenser_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 1024 && SDL_GetTicks() < deadline; ++i) {
        CHECK(dispenser_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL) {
            size_t eligible;
            int64_t time;
            CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &pauses, &eligible,
                                    &time) == 0);
            continue;
        }
        const mc_client_world_frame_state *world = f->state.world_frame;
        const mc_client_render_global_entities_state *entities =
            world == NULL ? NULL : world->entities;
        if (world != NULL && world->returned && f->state.hud != NULL && f->state.hud->returned &&
            entities != NULL && entities->returned && !entities->delayed &&
            entities->initial_delay == 0 && entities->remaining_delay == 0 &&
            entities->ll_returned && entities->th_returned && !atomic_load(&f->app->o) &&
            dispenser_hit(f))
            return 0;
    }
    fputs("Dispenser did not reach genuine ready World/HUD/LL/picking\n", stderr);
    return 1;
}

static int dispenser_choose(fixture *f, const reload_epoch *previous, reload_epoch *current) {
    mc_client_source_rq *screen = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &screen) == 0 && release_mouse(f, screen) == 0);
    int32_t index = -1;
    for (size_t i = 0; i < mc_client_save_list_size(screen->fields.n); ++i)
        if (filename_is(mc_client_save_list_at(screen->fields.n, i), "alpha")) {
            CHECK(index == -1);
            index = (int32_t)i;
        }
    CHECK(index >= 0);
    const mc_client_world_list_fields *list = &screen->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + index * 36 + 8;
    CHECK(push_mouse(f, &screen->base, screen->base.fields.c / 2, y, true) == 0 &&
          transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 5000;
    while (screen->fields.m != index && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(screen->fields.m == index && release_mouse(f, screen) == 0 &&
          push_button(f, &screen->base, 1, true) == 0 &&
          push_button(f, &screen->base, 1, false) == 0 && transfer(f) == 0 &&
          dispenser_ready(f) == 0 && reload_capture_epoch(f, current) == 0);
    if (previous != NULL)
        CHECK(current->world != previous->world && current->player != previous->player &&
              current->handler != previous->handler &&
              current->controller != previous->controller && current->call != previous->call);
    printf("DISPENSER_READY epoch=%d distinct-world-player=%d ll-frame-returned=1 picked=0,64,2\n",
           previous == NULL ? 0 : 1, previous == NULL ? 0 : 1);
    return 0;
}

static int dispenser_stack(const mc_item_stack *stack, int32_t id, int32_t count) {
    mc_item_stack_fields fields = {0};
    CHECK(count == 0
              ? stack == NULL
              : stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                    fields.id == id && fields.count == count && fields.damage == 0);
    return 0;
}

static int dispenser_ui(fixture *f, mc_client_source_gq *screen, bool carried, bool moved) {
    mc_client_dispenser_screen_state state = {0};
    mc_container_state container = {0};
    mc_inventory_dispenser_state adapter = {0};
    mc_local_player_snapshot player = {0};
    mc_inventory_player_state inventory = {0};
    mc_tile_entity_snapshot tile = {0};
    const mc_client_inventory_draw_state *draw = f->state.inventory;
    CHECK(mc_client_screen_dispenser_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          dispenser_player(f, &player, &inventory) == 0 &&
          mc_container_read_state(state.id.j, &container) &&
          mc_inventory_dispenser_read_state(state.constructor_dispenser, &adapter) &&
          mc_tile_entity_get_snapshot(adapter.tile, &tile) == MC_TILE_ENTITY_OK);
    CHECK(f->app->r == mc_client_screen_dispenser_base(screen) && !f->app->N && draw != NULL &&
          draw->returned && draw->dispenser && !draw->chest && !draw->furnace && !draw->workbench &&
          draw->id_returned && draw->background_returned && draw->foreground_returned &&
          !draw->preview_returned && !draw->mouse_stored && draw->receiver == NULL &&
          draw->screen_receiver == f->app->r && draw->slots_drawn == 45 &&
          draw->cursor_drawn == carried && state.id.j == player.player.e &&
          state.id.j == state.constructor_container && state.container_owned &&
          state.dispenser_held && state.constructor_inventory == player.player.c &&
          container.kind == MC_CONTAINER_DISPENSER && container.slots == 45 &&
          container.dispenser_inventory == state.constructor_dispenser &&
          tile.kind == MC_TILE_ENTITY_DISPENSER && tile.world == f->app->active_world &&
          tile.x == 0 && tile.y == 64 && tile.z == 2 && !tile.invalid);
    const mc_item_stack *first = NULL, *last = NULL;
    CHECK(mc_inventory_dispenser_borrow(state.constructor_dispenser, 0, &first, NULL) ==
              MC_INVENTORY_OK &&
          mc_inventory_dispenser_borrow(state.constructor_dispenser, 8, &last, NULL) ==
              MC_INVENTORY_OK &&
          dispenser_stack(first, 264, moved ? 0 : 5) == 0 && dispenser_stack(last, 266, 3) == 0 &&
          dispenser_stack(inventory.cursor, 264, carried ? 5 : 0) == 0 &&
          dispenser_stack(inventory.main_slots[9], 264, moved && !carried ? 5 : 0) == 0);
    return 0;
}

static int dispenser_open(fixture *f, size_t epoch, bool moved, mc_client_source_gq **out) {
    CHECK(dispenser_hit(f) && dispenser_held == NULL);
    const size_t mouse = f->state.mouse_events;
    const uint64_t placed = f->state.gameplay_placed_blocks, uses = f->state.gameplay_block_uses;
    for (unsigned int i = 0; i < 2; ++i) {
        SDL_Event event = {0};
        event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
        event.button.button = SDL_BUTTON_RIGHT;
        event.button.down = i == 0;
        CHECK(SDL_PushEvent(&event));
    }
    CHECK(transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(dispenser_frame(f) == 0);
        if (f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_DISPENSER) {
            *out = (mc_client_source_gq *)f->app->r;
            CHECK(f->state.mouse_events > mouse && f->state.gameplay_placed_blocks == placed &&
                  f->state.gameplay_block_uses == uses + 1 &&
                  dispenser_ui(f, *out, false, moved) == 0);
            mc_client_source_da *actual = f->app->r;
            CHECK(mc_client_screen_retain(actual, NULL) == MC_CLIENT_SCREEN_OK);
            dispenser_held = actual;
            printf("DISPENSER_OPEN epoch=%zu sdk-right=1 real-gq-sh=1 slots=45 placed=0 cursor=0\n",
                   epoch);
            return 0;
        }
    } while (SDL_GetTicks() < deadline);
    fputs("Dispenser SDK World activation did not display genuine GQ\n", stderr);
    return 1;
}

static int dispenser_click(fixture *f, mc_client_source_gq *screen, int32_t index, bool carried) {
    mc_client_dispenser_screen_state state = {0};
    mc_container_slot_state slot = {0};
    CHECK(mc_client_screen_dispenser_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_container_slot_read(state.id.j, index, &slot, NULL) == MC_INVENTORY_OK);
    mc_client_source_da *base = mc_client_screen_dispenser_base(screen);
    const int32_t x = mc_java_i32_sub(base->fields.c, state.id.a) / 2 + slot.x + 8;
    const int32_t y = mc_java_i32_sub(base->fields.d, state.id.i) / 2 + slot.y + 8;
    SDL_Window *window = mc_client_display_window(f->app->display);
    int width, height;
    CHECK(SDL_GetWindowSize(window, &width, &height));
    const float native_x = (float)((int64_t)x * width / base->fields.c);
    const float native_y = (float)((int64_t)y * height / base->fields.d);
    CHECK(position_mouse(f, window, native_x, native_y) == 0);
    SDL_Event motion = {0};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(window);
    motion.motion.x = native_x;
    motion.motion.y = native_y;
    CHECK(SDL_PushEvent(&motion));
    for (unsigned int i = 0; i < 2; ++i) {
        SDL_Event event = {0};
        event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(window);
        event.button.button = SDL_BUTTON_LEFT;
        event.button.down = i == 0;
        event.button.x = native_x;
        event.button.y = native_y;
        CHECK(SDL_PushEvent(&event));
    }
    CHECK(transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    mc_client_dispenser_screen_state after = {0};
    do {
        CHECK(dispenser_frame(f) == 0 &&
              mc_client_screen_dispenser_inspect(screen, &after, NULL) == MC_CLIENT_SCREEN_OK);
    } while (after.source_calls == state.source_calls && SDL_GetTicks() < deadline);
    CHECK(after.source_calls == state.source_calls + 1 &&
          dispenser_ui(f, screen, carried, true) == 0);
    printf("DISPENSER_CLICK slot=%" PRId32
           " sdk-left=1 source-call-delta=1 cursor=%d az0=0 az8=3 player9=%d\n",
           index, carried ? 1 : 0, carried ? 0 : 5);
    return 0;
}

static int dispenser_close(fixture *f, mc_client_source_gq *screen, size_t epoch) {
    CHECK(inventory_key(f, SDL_SCANCODE_E) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(dispenser_frame(f) == 0);
    } while ((f->app->r != NULL || !f->app->N) && SDL_GetTicks() < deadline);
    mc_client_dispenser_screen_state state = {0};
    mc_local_player_snapshot player = {0};
    mc_inventory_player_state inventory = {0};
    CHECK(f->app->r == NULL && f->app->N &&
          mc_client_screen_dispenser_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.inherited_closed_returned && dispenser_player(f, &player, &inventory) == 0 &&
          player.player.e == player.player.d && inventory.cursor == NULL &&
          dispenser_stack(inventory.main_slots[9], 264, 5) == 0 &&
          mc_client_screen_release(&dispenser_held, NULL) == MC_CLIENT_SCREEN_OK);
    printf("DISPENSER_CLOSE epoch=%zu sdk-key=E inherited-close=1 default-container=1 cursor=0 "
           "player9=5\n",
           epoch);
    return 0;
}

static int dispenser_run(fixture *f, mining_work *work, uint32_t *frame_id, uint32_t *canvas_id) {
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 &&
          dispenser_choose(f, NULL, &work->epochs[0]) == 0);
    mc_client_source_gq *screen = NULL;
    CHECK(dispenser_open(f, 0, false, &screen) == 0 &&
          mining_capture(f, work, "dispenser-open.png") == 0 &&
          dispenser_click(f, screen, 0, true) == 0 && dispenser_click(f, screen, 9, false) == 0 &&
          mining_capture(f, work, "dispenser-transferred.png") == 0 &&
          dispenser_close(f, screen, 0) == 0);
    char title[8192];
    CHECK(mining_path(work, "dispenser-title.png", title) == 0 &&
          save_title_run(f, NULL, title, false, 1, 0) == 0 &&
          reload_snapshot("dispenser-saved") == 0 &&
          dispenser_choose(f, &work->epochs[0], &work->epochs[1]) == 0 &&
          dispenser_open(f, 1, true, &screen) == 0 &&
          mining_capture(f, work, "dispenser-reloaded.png") == 0 &&
          dispenser_close(f, screen, 1) == 0 &&
          save_title_host(f, "after-dispenser", false, frame_id, canvas_id) == 0);
    printf("DISPENSER_BOUNDED opens=2 clicks=2 closes=2 epochs=2 frames=%zu gui-frames=%zu "
           "loaded-unpowered=1 field-repair=0 physical-input=0 placement-firing=0 fps-claim=0\n",
           dispenser_frames, dispenser_gui_frames);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    mining_fixture = f;
    mining_work work = {.directory = argv[2]};
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        result = dispenser_run(f, &work, &frame_id, &canvas_id);
    }
    if (dispenser_held != NULL &&
        mc_client_screen_release(&dispenser_held, NULL) != MC_CLIENT_SCREEN_OK)
        result = 1;
    if (f->app != NULL && frame_id != 0) {
        SDL_Event event = {0};
        event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        event.window.windowID = frame_id;
        if (!SDL_PushEvent(&event))
            result = 1;
        mc_client_sdk_update_error error = {0};
        for (size_t i = 0; i < 100 && atomic_load(&f->app->J); ++i) {
            if (mc_client_sdk_display_process_messages(
                    mc_client_display_update_owner(f->app->display), &error) !=
                MC_CLIENT_SDK_UPDATE_OK)
                result = 1;
            SDL_Delay(2);
        }
        if (atomic_load(&f->app->J))
            result = 1;
        else
            puts("DISPENSER_STOP actual-frame-close=1 source-frame-retry=0");
    }
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    if (closed != MC_CLIENT_BOOTSTRAP_OK || f->bootstrap != NULL || f->loop != NULL ||
        dispenser_held != NULL || frame_id == 0 || canvas_id == 0 ||
        SDL_GetWindowFromID(frame_id) != NULL || SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("DISPENSER_FINISH cleanup=1 windows-absent=1 checks=%u source-field-repair=0\n",
               checks);
    free(f);
    return result;
}
