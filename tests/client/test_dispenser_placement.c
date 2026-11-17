/* One ordinary BlockItem23 placement, genuine empty AZ/GQ, save/reselection.
 * The included fixture supplies genuine bootstrap/World/input owners only. */
int mc_dispenser_loaded_fixture_main(int, char **);
#define MC_DISPENSER_FIXTURE_MAIN mc_dispenser_loaded_fixture_main
#include "test_dispenser.c"
#undef MC_DISPENSER_FIXTURE_MAIN

static int dp_inventory(fixture *, bool);
static size_t dp_taps;

static uint64_t dp_double_bits(double value) {
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t dp_float_bits(float value) {
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int dp_aim(fixture *f, size_t attempt) {
    mc_entity_hit_result_view hit = {0};
    const mc_client_world_frame_state *frame = f->state.world_frame;
    CHECK(f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          frame != NULL && frame->returned && frame->eye_position_returned &&
          frame->entity_eye != NULL && frame->camera == f->app->h && dp_inventory(f, true) == 0);
    static const int32_t dx[] = {0, 0, 0, 0, -1, 1};
    static const int32_t dy[] = {-1, 1, 0, 0, 0, 0};
    static const int32_t dz[] = {0, 0, -1, 1, 0, 0};
    CHECK(hit.fields.e >= 0 && hit.fields.e < 6);
    const int32_t x = mc_java_i32_add(hit.fields.b, dx[hit.fields.e]);
    const int32_t y = mc_java_i32_add(hit.fields.c, dy[hit.fields.e]);
    const int32_t z = mc_java_i32_add(hit.fields.d, dz[hit.fields.e]);
    const mc_entity_base_fields *player = &f->app->h->base;
    const mc_entity_vector *eye = frame->entity_eye;
    printf("DISPENSER_PLACEMENT_AIM attempt=%zu picked=%" PRId32 ",%" PRId32 ",%" PRId32
           " face=%" PRId32 " target=%" PRId32 ",%" PRId32 ",%" PRId32 " player-bits=%016" PRIx64
           ",%016" PRIx64 ",%016" PRIx64 " entity-eye-bits=%016" PRIx64 ",%016" PRIx64
           ",%016" PRIx64 " yaw-bits=%08" PRIx32 " pitch-bits=%08" PRIx32 " cursor=0\n",
           attempt, hit.fields.b, hit.fields.c, hit.fields.d, hit.fields.e, x, y, z,
           dp_double_bits(player->x), dp_double_bits(player->y), dp_double_bits(player->z),
           dp_double_bits(eye->x), dp_double_bits(eye->y), dp_double_bits(eye->z),
           dp_float_bits(player->yaw), dp_float_bits(player->aT));
    CHECK(hit.fields.b == 0 && hit.fields.c == 64 && hit.fields.d == 2 && hit.fields.e == 2 &&
          x == 0 && y == 64 && z == 1 && player->yaw == 0 && player->aT == 30);
    return 0;
}

static bool dp_hit(fixture *f, int32_t z) {
    mc_entity_hit_result_view hit = {0};
    return f->app->r == NULL && f->app->N && f->app->y != NULL &&
           mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK && hit.fields.b == 0 &&
           hit.fields.c == 64 && hit.fields.d == z;
}

static int dp_inventory(fixture *f, bool before) {
    mc_local_player_snapshot player = {0};
    mc_inventory_player_state inventory = {0};
    CHECK(dispenser_player(f, &player, &inventory) == 0 && inventory.selected == 0 &&
          inventory.cursor == NULL);
    for (int32_t i = 0; i < 36; ++i)
        CHECK(dispenser_stack(inventory.main_slots[i], 23, before && i == 0 ? 1 : 0) == 0);
    return 0;
}

static int dp_world(fixture *f, bool placed) {
    mc_world_runtime *world = f->app->active_world;
    int32_t id = -1, metadata = -1;
    CHECK(
        world != NULL && world->fields.c != NULL && world->fields.c->count == (placed ? 1u : 0u) &&
        mc_world_runtime_block_get(world, 0, 64, 2, &id, NULL) == MC_WORLD_STORAGE_OK && id == 1 &&
        mc_world_runtime_metadata_get(world, 0, 64, 2, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
        metadata == 0 &&
        mc_world_runtime_block_get(world, 0, 64, 1, &id, NULL) == MC_WORLD_STORAGE_OK &&
        id == (placed ? 23 : 0) &&
        mc_world_runtime_metadata_get(world, 0, 64, 1, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
        metadata == (placed ? 2 : 0));
    if (placed) {
        mc_tile_entity *tile = world->fields.c->items[0];
        mc_tile_entity_snapshot state = {0};
        int32_t size = -1;
        CHECK(mc_tile_entity_get_snapshot(tile, &state) == MC_TILE_ENTITY_OK &&
              state.kind == MC_TILE_ENTITY_DISPENSER && state.world == world && !state.invalid &&
              state.x == 0 && state.y == 64 && state.z == 1 && state.dispenser_capacity == 9 &&
              state.dispenser_random_constructed &&
              mc_tile_entity_dispenser_size(tile, &size) == MC_TILE_ENTITY_OK && size == 9);
        for (int32_t i = 0; i < size; ++i) {
            mc_item_stack *stack = NULL;
            const mc_tile_entity_status got = mc_tile_entity_dispenser_get(tile, i, &stack);
            const bool empty = stack == NULL;
            mc_item_stack_release(stack);
            CHECK(got == MC_TILE_ENTITY_OK && empty);
        }
    }
    CHECK(dp_inventory(f, !placed) == 0);
    return 0;
}

static int dp_ready(fixture *f, bool placed) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 1024 && SDL_GetTicks() < deadline; ++i) {
        CHECK(dispenser_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL) {
            size_t eligible = 0;
            int64_t time = 0;
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
            dp_hit(f, placed ? 1 : 2))
            return dp_world(f, placed);
    }
    fputs("Dispenser placement did not reach genuine ready World/HUD/LL/picking\n", stderr);
    return 1;
}

static int dp_choose(fixture *f, const reload_epoch *previous, reload_epoch *current) {
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
          dp_ready(f, previous != NULL) == 0 && reload_capture_epoch(f, current) == 0);
    if (previous != NULL)
        CHECK(current->world != previous->world && current->player != previous->player &&
              current->handler != previous->handler &&
              current->controller != previous->controller && current->call != previous->call);
    printf("DISPENSER_PLACEMENT_READY epoch=%d distinct-world-player=%d ll-frame-returned=1 "
           "picked=0,64,%d\n",
           previous == NULL ? 0 : 1, previous == NULL ? 0 : 1, previous == NULL ? 2 : 1);
    return 0;
}

static int dp_right(fixture *f) {
    for (unsigned int i = 0; i < 2; ++i) {
        SDL_Event event = {0};
        event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
        event.button.button = SDL_BUTTON_RIGHT;
        event.button.down = i == 0;
        CHECK(SDL_PushEvent(&event));
    }
    return transfer(f);
}

static int dp_place(fixture *f) {
    CHECK(dp_hit(f, 2) && dp_world(f, false) == 0 && f->app->h->base.yaw == 0 &&
          f->app->h->base.aT == 30);
    const uint64_t clicks = f->state.gameplay_right_clicks, uses = f->state.gameplay_block_uses;
    const uint64_t admitted = f->state.gameplay_placements_admitted;
    const uint64_t placed = f->state.gameplay_placed_blocks;
    const uint64_t sounds = f->state.gameplay_placement_sounds;
    const uint64_t counts = f->state.gameplay_stack_count_stores;
    const uint64_t stats = f->state.gameplay_used_statistics;
    const uint64_t clears = f->state.gameplay_selected_slot_clears;
    const uint64_t air = f->state.gameplay_air_uses, equip = f->state.gameplay_equip_resets;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    for (size_t attempt = 0; attempt < 12 && frames < 96 && SDL_GetTicks() < deadline; ++attempt) {
        const size_t events = f->state.mouse_events;
        CHECK(dp_aim(f, attempt + 1) == 0);
        dp_taps = attempt + 1;
        printf("DISPENSER_PLACEMENT_TAP attempt=%zu sdk-right-down-up=1\n", attempt + 1);
        CHECK(dp_hit(f, 2) && dp_right(f) == 0);
        bool drained = false;
        do {
            CHECK(dispenser_frame(f) == 0);
            ++frames;
            mc_client_sdk_input_state input = {0};
            mc_client_sdk_bytes_state read = {0}, queue = {0}, buttons = {0};
            CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
                  mc_client_sdk_bytes_inspect(input.mouse_read, &read) &&
                  mc_client_sdk_queue_inspect(input.mouse_events, &queue) &&
                  mc_client_sdk_bytes_inspect(input.buttons, &buttons) && buttons.limit > 1 &&
                  buttons.data[1] == 0);
            drained = f->state.mouse_events > events && read.position == read.limit &&
                      queue.position == 0 && input.pending_native_event == NULL;
            if (drained && f->state.gameplay_placed_blocks == placed + 1) {
                CHECK(f->app->r == NULL && f->state.gameplay_right_clicks == clicks + 1 &&
                      f->state.gameplay_block_uses == uses + 1 &&
                      f->state.gameplay_placements_admitted == admitted + 1 &&
                      f->state.gameplay_placement_sounds == sounds + 1 &&
                      f->state.gameplay_stack_count_stores == counts + 1 &&
                      f->state.gameplay_used_statistics == stats + 1 &&
                      f->state.gameplay_selected_slot_clears == clears + 1 &&
                      f->state.gameplay_air_uses == air && dp_world(f, true) == 0);
                printf("DISPENSER_PLACED attempts=%zu sdk-right=1 block-item=23 count-before=1 "
                       "count-after=0 count-stores=1 statistic=1 sound=1 support=0,64,2 "
                       "target=0,64,1 yaw=0 metadata=2 az-slots=9 empty=1\n",
                       attempt + 1);
                return dp_ready(f, true);
            }
        } while (!drained && frames < 96 && SDL_GetTicks() < deadline);
        /* A fresh tap is legal only if the drained tap had no Source action. */
        CHECK(drained && f->app->r == NULL && f->state.gameplay_right_clicks == clicks &&
              f->state.gameplay_block_uses == uses &&
              f->state.gameplay_placements_admitted == admitted &&
              f->state.gameplay_placed_blocks == placed &&
              f->state.gameplay_placement_sounds == sounds &&
              f->state.gameplay_stack_count_stores == counts &&
              f->state.gameplay_used_statistics == stats &&
              f->state.gameplay_selected_slot_clears == clears &&
              f->state.gameplay_air_uses == air && f->state.gameplay_equip_resets == equip &&
              dp_world(f, false) == 0);
        printf("DISPENSER_PLACEMENT_NO_EFFECT attempt=%zu drained=1 source-action=0\n",
               attempt + 1);
    }
    fputs("Dispenser ordinary BlockItem placement did not return within bounded taps\n", stderr);
    return 1;
}

static int dp_ui(fixture *f, mc_client_source_gq *screen) {
    mc_client_dispenser_screen_state state = {0};
    mc_container_state container = {0};
    mc_inventory_dispenser_state adapter = {0};
    mc_local_player_snapshot player = {0};
    mc_inventory_player_state inventory = {0};
    const mc_client_inventory_draw_state *draw = f->state.inventory;
    CHECK(mc_client_screen_dispenser_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          dispenser_player(f, &player, &inventory) == 0 &&
          mc_container_read_state(state.id.j, &container) &&
          mc_inventory_dispenser_read_state(state.constructor_dispenser, &adapter) &&
          f->app->r == mc_client_screen_dispenser_base(screen) && !f->app->N && draw != NULL &&
          draw->returned && draw->dispenser && draw->id_returned && draw->background_returned &&
          draw->foreground_returned && !draw->preview_returned && !draw->mouse_stored &&
          draw->receiver == NULL && draw->screen_receiver == f->app->r && draw->slots_drawn == 45 &&
          !draw->cursor_drawn && state.id.j == player.player.e &&
          state.id.j == state.constructor_container && state.container_owned &&
          state.dispenser_held && state.constructor_inventory == player.player.c &&
          container.kind == MC_CONTAINER_DISPENSER && container.slots == 45 &&
          container.dispenser_inventory == state.constructor_dispenser &&
          adapter.tile == f->app->active_world->fields.c->items[0] && dp_world(f, true) == 0);
    for (int32_t i = 0; i < 9; ++i) {
        const mc_item_stack *stack = NULL;
        CHECK(mc_inventory_dispenser_borrow(state.constructor_dispenser, i, &stack, NULL) ==
                  MC_INVENTORY_OK &&
              stack == NULL);
    }
    return 0;
}

static int dp_open_close(fixture *f, size_t epoch) {
    CHECK(dp_hit(f, 1) && dispenser_held == NULL);
    const uint64_t placed = f->state.gameplay_placed_blocks, uses = f->state.gameplay_block_uses;
    const size_t mouse = f->state.mouse_events;
    CHECK(dp_right(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    mc_client_source_gq *screen = NULL;
    do {
        CHECK(dispenser_frame(f) == 0);
        if (f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_DISPENSER) {
            screen = (mc_client_source_gq *)f->app->r;
            break;
        }
    } while (SDL_GetTicks() < deadline);
    CHECK(screen != NULL && f->state.mouse_events > mouse &&
          f->state.gameplay_placed_blocks == placed && f->state.gameplay_block_uses == uses + 1 &&
          dp_ui(f, screen) == 0 && mc_client_screen_retain(f->app->r, NULL) == MC_CLIENT_SCREEN_OK);
    dispenser_held = f->app->r;
    printf(
        "DISPENSER_PLACEMENT_OPEN epoch=%zu sdk-right=1 real-gq-sh=1 slots=45 empty=1 placed=0\n",
        epoch);
    CHECK(inventory_key(f, SDL_SCANCODE_E) == 0);
    const uint64_t close_deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(dispenser_frame(f) == 0);
    } while ((f->app->r != NULL || !f->app->N) && SDL_GetTicks() < close_deadline);
    mc_client_dispenser_screen_state state = {0};
    mc_local_player_snapshot player = {0};
    mc_inventory_player_state inventory = {0};
    CHECK(f->app->r == NULL && f->app->N &&
          mc_client_screen_dispenser_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.inherited_closed_returned && dispenser_player(f, &player, &inventory) == 0 &&
          player.player.e == player.player.d && dp_world(f, true) == 0 &&
          mc_client_screen_release(&dispenser_held, NULL) == MC_CLIENT_SCREEN_OK);
    printf("DISPENSER_PLACEMENT_CLOSE epoch=%zu sdk-key=E inherited-close=1 default-container=1 "
           "empty=1\n",
           epoch);
    return 0;
}

static int dp_run(fixture *f, mining_work *work, uint32_t *frame_id, uint32_t *canvas_id) {
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && dp_choose(f, NULL, &work->epochs[0]) == 0 &&
          dp_place(f) == 0 && mining_capture(f, work, "dispenser-placement-placed.png") == 0 &&
          dp_open_close(f, 0) == 0);
    char title[8192];
    CHECK(mining_path(work, "dispenser-placement-title.png", title) == 0 &&
          save_title_run(f, NULL, title, false, 1, 0) == 0 &&
          reload_snapshot("dispenser-placement-saved") == 0 &&
          dp_choose(f, &work->epochs[0], &work->epochs[1]) == 0 && dp_open_close(f, 1) == 0 &&
          mining_capture(f, work, "dispenser-placement-reloaded.png") == 0 &&
          save_title_host(f, "after-dispenser-placement", false, frame_id, canvas_id) == 0);
    printf("DISPENSER_PLACEMENT_BOUNDED placements=1 opens=2 closes=2 epochs=2 taps=%zu frames=%zu "
           "gui-frames=%zu field-repair=0 physical-input=0 firing=0 fps-claim=0\n",
           dp_taps, dispenser_frames, dispenser_gui_frames);
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
        result = dp_run(f, &work, &frame_id, &canvas_id);
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
            puts("DISPENSER_PLACEMENT_STOP actual-frame-close=1 source-frame-retry=0");
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
        printf("DISPENSER_PLACEMENT_FINISH cleanup=1 windows-absent=1 checks=%u "
               "source-field-repair=0\n",
               checks);
    free(f);
    return result;
}
