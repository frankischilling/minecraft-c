/* Actual placed Furnace through CK, SK World ticks, BM/VQ/OV and SDK transport. */
int mc_furnace_included_inventory_main(int, char **);
#define MC_INVENTORY_FIXTURE_MAIN mc_furnace_included_inventory_main
#include "gameplay/test_inventory.c"
#undef MC_INVENTORY_FIXTURE_MAIN
#include "client/application_access.h"
#include "client/gui/source_furnace_screen_internal.h"
#include "mc/world/runtime/tile_entities/furnace.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"

static int32_t furnace_pick_y = 63;

static int furnace_ui(fixture *f, mc_client_source_ov *screen, bool cursor) {
    const mc_client_inventory_draw_state *draw = f->state.inventory;
    CHECK(f->app->r == &screen->container.base && !f->app->N && draw != NULL && draw->returned &&
          draw->furnace && !draw->chest && !draw->workbench && draw->id_returned &&
          draw->background_returned && draw->foreground_returned && !draw->preview_returned &&
          !draw->mouse_stored && draw->slots_drawn == 39 && draw->cursor_drawn == cursor &&
          screen->container.id.a == 176 && screen->container.id.i == 166 &&
          screen->container.id.j == f->app->h->local_player->fields.player.e);
    return 0;
}

static int furnace_wait(fixture *f, mc_client_source_ov **out) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(inventory_frame(f) == 0);
        if (f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_FURNACE) {
            *out = (mc_client_source_ov *)f->app->r;
            return furnace_ui(f, *out, false);
        }
    } while (SDL_GetTicks() < deadline);
    fputs("Furnace SDK activation did not display OV\n", stderr);
    return 1;
}

static int furnace_shift(fixture *f, bool down) {
    CHECK(gameplay_key(f, SDL_SCANCODE_LSHIFT, down) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    bool held = !down;
    mc_client_sdk_input_error error;
    do {
        CHECK(inventory_frame(f) == 0 &&
              mc_client_sdk_keyboard_key_down(mc_client_display_inputs(f->app->display), 42, &held,
                                              &error) == MC_CLIENT_SDK_INPUT_OK);
    } while (held != down && SDL_GetTicks() < deadline);
    CHECK(held == down);
    return 0;
}

static int furnace_click(fixture *f, mc_client_source_ov *screen, int32_t index, bool right,
                         bool shift, bool cursor) {
    if (shift)
        CHECK(furnace_shift(f, true) == 0);
    mc_client_source_id *o = &screen->container;
    mc_container_slot_state slot;
    CHECK(mc_container_slot_read(o->id.j, index, &slot, NULL) == MC_INVENTORY_OK);
    const int32_t x = mc_java_i32_sub(o->base.fields.c, o->id.a) / 2 + slot.x + 8;
    const int32_t y = mc_java_i32_sub(o->base.fields.d, o->id.i) / 2 + slot.y + 8;
    SDL_Window *window = mc_client_display_window(f->app->display);
    int width, height;
    CHECK(SDL_GetWindowSize(window, &width, &height));
    const float native_x = (float)((int64_t)x * width / o->base.fields.c);
    const float native_y = (float)((int64_t)y * height / o->base.fields.d);
    CHECK(position_mouse(f, window, native_x, native_y) == 0);
    SDL_Event motion = {0};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(window);
    motion.motion.x = native_x;
    motion.motion.y = native_y;
    CHECK(SDL_PushEvent(&motion));
    const size_t before = o->source_calls;
    for (unsigned int i = 0; i < 2; ++i) {
        SDL_Event event = {0};
        event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(window);
        event.button.button = right ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
        event.button.down = i == 0;
        event.button.x = native_x;
        event.button.y = native_y;
        CHECK(SDL_PushEvent(&event));
    }
    CHECK(transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (o->source_calls == before && SDL_GetTicks() < deadline)
        CHECK(inventory_frame(f) == 0);
    CHECK(o->source_calls == before + 1 && furnace_ui(f, screen, cursor) == 0);
    if (shift)
        CHECK(furnace_shift(f, false) == 0);
    printf("FURNACE_CLICK index=%d right=%d shift=%d cursor=%d calls-before=%zu calls-after=%zu\n",
           index, (int)right, (int)shift, (int)cursor, before, o->source_calls);
    CHECK(fflush(stdout) == 0);
    return 0;
}

static int furnace_close(fixture *f, mc_client_source_ov *screen) {
    CHECK(inventory_key(f, SDL_SCANCODE_E) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(inventory_frame(f) == 0);
        if (f->app->r == NULL && f->app->N)
            break;
    } while (SDL_GetTicks() < deadline);
    CHECK(f->app->r == NULL && f->app->N && screen->container.inherited_closed_returned &&
          !screen->container.direct_closed_returned &&
          f->app->h->local_player->fields.player.e == f->app->h->local_player->fields.player.d);
    return 0;
}

static int furnace_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t focus_pauses = 0;
    do {
        CHECK(inventory_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL) {
            size_t eligible;
            int64_t time;
            CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &focus_pauses,
                                    &eligible, &time) == 0);
            continue;
        }
        const mc_client_world_frame_state *world = f->state.world_frame;
        if (!f->app->N || world == NULL || !world->returned ||
            world->world != f->app->active_world || f->state.hud == NULL || !f->state.hud->returned)
            continue; /* Publication can precede the first returned World/HUD frame. */
        mc_entity_hit_result_view hit;
        if (f->app->y != NULL &&
            mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
            hit.fields.b == 0 && hit.fields.c == furnace_pick_y && hit.fields.d == 2)
            return 0;
    } while (SDL_GetTicks() < deadline);
    fputs("Furnace saved pose did not reach genuine focused picking\n", stderr);
    return 1;
}

/* Reuse selection transport, with this scene's own ready predicate. The Dirt
 * mining helper deliberately has a different carried-Stack assertion. */
static int furnace_choose(fixture *f, const reload_epoch *previous, reload_epoch *current) {
    mc_client_source_rq *screen = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &screen) == 0 && release_mouse(f, screen) == 0);
    int32_t index = -1;
    const size_t count = mc_client_save_list_size(screen->fields.n);
    for (size_t i = 0; i < count; ++i)
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
          furnace_ready(f) == 0 && reload_capture_epoch(f, current) == 0);
    if (previous != NULL)
        CHECK(current->world != previous->world && current->player != previous->player &&
              current->handler != previous->handler &&
              current->controller != previous->controller && current->call != previous->call);
    return 0;
}

static bool furnace_hit(const fixture *f, int32_t x, int32_t y) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    return f->app->r == NULL && f->app->y != NULL &&
           mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
           mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
           hit.fields.a == kinds.a && hit.fields.b == x && hit.fields.c == y && hit.fields.d == 2 &&
           (y != 63 || hit.fields.e == 1);
}

static int furnace_tap(fixture *f, int32_t x, int32_t y, bool placed) {
    CHECK(furnace_hit(f, x, y));
    const uint64_t clicks = f->state.gameplay_right_clicks;
    const uint64_t blocks = f->state.gameplay_block_uses;
    const uint64_t admitted = f->state.gameplay_placements_admitted;
    const uint64_t setters = f->state.gameplay_placed_blocks;
    const uint64_t sounds = f->state.gameplay_placement_sounds;
    const uint64_t counts = f->state.gameplay_stack_count_stores;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    for (size_t attempt = 0; attempt < 12 && frames < 96 && SDL_GetTicks() < deadline; ++attempt) {
        CHECK(furnace_hit(f, x, y));
        const size_t events = f->state.mouse_events;
        for (unsigned int edge = 0; edge < 2; ++edge) {
            SDL_Event event = {0};
            event.type = edge == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
            event.button.button = SDL_BUTTON_RIGHT;
            event.button.down = edge == 0;
            CHECK(SDL_PushEvent(&event));
        }
        CHECK(transfer(f) == 0);
        bool drained = false;
        do {
            CHECK(inventory_frame(f) == 0);
            ++frames;
            mc_client_sdk_input_state input;
            mc_client_sdk_bytes_state read, buttons, queue;
            CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
                  mc_client_sdk_bytes_inspect(input.mouse_read, &read) &&
                  mc_client_sdk_queue_inspect(input.mouse_events, &queue) &&
                  mc_client_sdk_bytes_inspect(input.buttons, &buttons) && buttons.limit > 1 &&
                  buttons.data[1] == 0);
            drained = f->state.mouse_events > events && read.position == read.limit &&
                      queue.position == 0 && input.pending_native_event == NULL;
        } while (!drained && frames < 96 && SDL_GetTicks() < deadline);
        CHECK(drained);
        if (f->state.gameplay_right_clicks > clicks) {
            const uint64_t delta = placed ? 1u : 0u;
            printf("FURNACE_TAP target=%d,%d,2 placed=%d attempts=%zu clicks=%" PRIu64
                   " block-use=%" PRIu64 " admitted=%" PRIu64 " setters=%" PRIu64 " sounds=%" PRIu64
                   " count-stores=%" PRIu64 "\n",
                   x, y, (int)placed, attempt + 1, f->state.gameplay_right_clicks - clicks,
                   f->state.gameplay_block_uses - blocks,
                   f->state.gameplay_placements_admitted - admitted,
                   f->state.gameplay_placed_blocks - setters,
                   f->state.gameplay_placement_sounds - sounds,
                   f->state.gameplay_stack_count_stores - counts);
            CHECK(fflush(stdout) == 0);
            CHECK(f->state.gameplay_right_clicks == clicks + 1 &&
                  f->state.gameplay_block_uses == blocks + 1 &&
                  f->state.gameplay_placements_admitted == admitted + delta &&
                  f->state.gameplay_placed_blocks == setters + delta &&
                  f->state.gameplay_placement_sounds == sounds + delta &&
                  f->state.gameplay_stack_count_stores == counts + delta);
            return 0;
        }
        CHECK(f->state.gameplay_block_uses == blocks &&
              f->state.gameplay_placements_admitted == admitted &&
              f->state.gameplay_placed_blocks == setters &&
              f->state.gameplay_stack_count_stores == counts);
    }
    fputs("Furnace placement tap not consumed within bounded SDK attempts\n", stderr);
    return 1;
}

static int furnace_slot(mc_client_source_ov *screen, int32_t index, int32_t id, int32_t count) {
    mc_inventory_slot *slot = NULL;
    const mc_item_stack *stack = NULL;
    mc_item_stack_fields fields;
    CHECK(mc_container_source_slot(screen->container.id.j, index, &slot, NULL) == MC_INVENTORY_OK &&
          mc_inventory_slot_source_get(slot, &stack, NULL) == MC_INVENTORY_OK);
    if (count == 0)
        CHECK(stack == NULL);
    else
        CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == id && fields.count == count && fields.damage == 0);
    return 0;
}

static int furnace_identity(fixture *f, mc_client_source_ov *screen, mc_tile_entity *expected,
                            bool burning) {
    mc_tile_entity *from_screen = NULL, *from_world = NULL;
    CHECK(mc_inventory_furnace_tile_borrow(screen->fields.l, &from_screen, NULL) ==
              MC_INVENTORY_OK &&
          from_screen == expected &&
          mc_live_tile_entities_get(f->app->active_world, 0, 64, 2, &from_world, NULL) ==
              MC_WORLD_STORAGE_OK);
    const bool same = from_world == expected;
    mc_tile_entity_release(from_world);
    int32_t block, metadata;
    CHECK(same &&
          mc_world_runtime_block_get(f->app->active_world, 0, 64, 2, &block, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(f->app->active_world, 0, 64, 2, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          block == (burning ? 62 : 61) && metadata == 2 && furnace_ui(f, screen, false) == 0);
    return 0;
}

static int furnace_cook(fixture *f, mining_work *work, mc_client_source_ov *screen,
                        mc_tile_entity *tile, const char *capture) {
    mc_furnace_fields start, current;
    CHECK(mc_tile_entity_furnace_fields_get(tile, &start) == MC_TILE_ENTITY_OK &&
          start.cook_progress >= 0 && start.cook_progress < 200 &&
          furnace_slot(screen, 2, 0, 0) == 0);
    const uint64_t ticks = f->state.world_tick_returns, deadline = SDL_GetTicks() + 120000;
    bool completed = false, progress_drawn = false, flame_drawn = false;
    do {
        CHECK(inventory_frame(f) == 0 && furnace_identity(f, screen, tile, true) == 0 &&
              mc_tile_entity_furnace_fields_get(tile, &current) == MC_TILE_ENTITY_OK);
        const mc_client_inventory_draw_state *draw = f->state.inventory;
        progress_drawn |= draw->furnace_cook_scale > 0;
        flame_drawn |= draw->furnace_burning && draw->furnace_burn_scale > 0;
        const mc_item_stack *output = NULL;
        CHECK(mc_inventory_furnace_borrow(screen->fields.l, 2, &output, NULL) == MC_INVENTORY_OK);
        if (output != NULL) {
            completed = true;
            break;
        }
    } while (SDL_GetTicks() < deadline);
    CHECK(completed && progress_drawn && flame_drawn &&
          f->state.world_tick_returns >= ticks + (uint64_t)(200 - start.cook_progress) &&
          furnace_slot(screen, 2, 265, 1) == 0 && mining_capture(f, work, capture) == 0);
    printf("FURNACE_COOK epoch=%d start-ticks=%" PRIu64 " start-cook=%d end-ticks=%" PRIu64
           " end-cook=%d burn=%d output=1 shared-sk=1 draw-progress=1 draw-flame=1\n",
           work->epochs[1].world == f->app->active_world ? 2 : 1, ticks, start.cook_progress,
           f->state.world_tick_returns, current.cook_progress, current.burn_remaining);
    CHECK(fflush(stdout) == 0);
    return 0;
}

typedef struct furnace_passive_snapshot {
    mc_client_source_da *screen;
    mc_entity *player;
    mc_world_runtime *world;
    mc_container *active, *ordinary;
    mc_client_screen_id_fields id;
    mc_client_screen_ov_fields ov;
    mc_furnace_fields furnace;
    int64_t time;
    uint64_t ticks, world_ticks;
    bool inherited_close, direct_close, changed;
    int32_t selected;
    size_t count, source_calls;
    const mc_item_stack *stacks[96];
    mc_item_stack_fields fields[96];
} furnace_passive_snapshot;

static int furnace_snapshot_stack(furnace_passive_snapshot *out, const mc_item_stack *stack) {
    CHECK(out->count < 96);
    out->stacks[out->count] = stack;
    if (stack != NULL)
        CHECK(mc_item_stack_fields_get(stack, &out->fields[out->count]) == MC_ITEM_STACK_OK);
    ++out->count;
    return 0;
}

/* Native passive reads only; this never invokes a World tick, source Inventory
 * getter, World Tile query, SDL pump, source close or source field write. */
static int furnace_snapshot(fixture *f, mc_client_source_ov *screen, mc_tile_entity *tile,
                            furnace_passive_snapshot *out) {
    memset(out, 0, sizeof(*out));
    mc_inventory_player_state inventory;
    CHECK(mc_inventory_player_read_state(f->app->h->local_player->fields.player.c, &inventory) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          mc_tile_entity_furnace_fields_get(tile, &out->furnace) == MC_TILE_ENTITY_OK);
    out->screen = f->app->r;
    out->player = f->app->h;
    out->world = f->app->active_world;
    out->active = out->player->local_player->fields.player.e;
    out->ordinary = out->player->local_player->fields.player.d;
    out->id = screen->container.id;
    out->ov = screen->fields;
    out->ticks = f->state.ticks;
    out->world_ticks = f->state.world_tick_returns;
    out->inherited_close = screen->container.inherited_closed_returned;
    out->direct_close = screen->container.direct_closed_returned;
    out->source_calls = screen->container.source_calls;
    out->selected = inventory.selected;
    out->changed = inventory.changed;
    CHECK(mc_world_info_time_get(out->world->fields.x, &out->time) == MC_WORLD_INFO_OK);
    for (size_t i = 0; i < 40; ++i)
        CHECK(furnace_snapshot_stack(out, i < 36 ? inventory.main_slots[i]
                                                 : inventory.armor_slots[i - 36]) == 0);
    CHECK(furnace_snapshot_stack(out, inventory.cursor) == 0 && tile->state.furnace.capacity == 3);
    for (size_t i = 0; i < 3; ++i)
        CHECK(furnace_snapshot_stack(out, tile->state.furnace.slots[i]) == 0);
    mc_container *container = screen->container.id.j;
    CHECK(container != NULL && container->snapshots != NULL && container->snapshots->size == 39);
    for (size_t i = 0; i < 39; ++i)
        CHECK(furnace_snapshot_stack(out, container->snapshots->values[i]) == 0);
    return 0;
}

static int furnace_stop_open(fixture *f, mc_client_source_ov *screen, mc_tile_entity *tile,
                             uint32_t frame_id) {
    SDL_Event event = {0};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = frame_id;
    CHECK(SDL_PushEvent(&event));
    mc_client_sdk_update_error update = {0};
    for (unsigned int i = 0; i < 100 && atomic_load(&f->app->J); ++i) {
        CHECK(
            mc_client_sdk_display_process_messages(mc_client_display_update_owner(f->app->display),
                                                   &update) == MC_CLIENT_SDK_UPDATE_OK);
        SDL_Delay(2);
    }
    mc_client_frame_state host;
    CHECK(
        !atomic_load(&f->app->J) && mc_client_menu_loop_frame(f->loop) &&
        mc_client_menu_loop_inspect(f->loop, &f->state) && !f->state.running && !f->state.failed &&
        mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
        host.close_dispatched && !screen->container.inherited_closed_returned &&
        !screen->container.direct_closed_returned && reload_snapshot("furnace-open-stopped") == 0);
    furnace_passive_snapshot *before = calloc(1, sizeof(*before)),
                             *after = calloc(1, sizeof(*after));
    CHECK(before != NULL && after != NULL && furnace_snapshot(f, screen, tile, before) == 0);
    const bool prepared = mc_client_menu_loop_terminal_prepare(f->loop);
    CHECK(furnace_snapshot(f, screen, tile, after) == 0 &&
          memcmp(before, after, sizeof(*before)) == 0);
    free(after);
    free(before);
    if (!prepared) {
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
        fprintf(stderr, "Furnace open native prepare refused: %s\n", f->state.retirement_operation);
        return 1;
    }
    CHECK(reload_snapshot("furnace-open-prepared") == 0);
    puts("FURNACE_OPEN_PREPARE source-unchanged=1 world-time-unchanged=1 ticks-unchanged=1 "
         "stacks-unchanged=1 native-prefix-only=1");
    puts("FURNACE_OPEN_SHUTDOWN actual-open-ov=1 source-close=0");
    return 0;
}

static int furnace_run(fixture *f, mining_work *work, uint32_t *frame_id, uint32_t *canvas_id) {
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && ticks(f, 2) == 0 &&
          furnace_choose(f, NULL, &work->epochs[0]) == 0 && inventory_field(f, 0, 61, 1) == 0 &&
          inventory_field(f, 9, 15, 2) == 0 && inventory_field(f, 10, 263, 2) == 0 &&
          furnace_tap(f, 0, 63, true) == 0 && inventory_field(f, 0, 0, 0) == 0);
    furnace_pick_y = 64;
    CHECK(furnace_ready(f) == 0 && mining_capture(f, work, "furnace-placed.png") == 0 &&
          furnace_tap(f, 0, 64, false) == 0);
    mc_client_source_ov *screen = NULL;
    mc_tile_entity *first_tile = NULL;
    CHECK(furnace_wait(f, &screen) == 0 &&
          mc_inventory_furnace_tile_borrow(screen->fields.l, &first_tile, NULL) ==
              MC_INVENTORY_OK &&
          furnace_identity(f, screen, first_tile, false) == 0 &&
          furnace_click(f, screen, 3, false, false, true) == 0 &&
          furnace_click(f, screen, 0, false, false, false) == 0 &&
          furnace_click(f, screen, 4, false, false, true) == 0 &&
          furnace_click(f, screen, 1, false, false, false) == 0 &&
          furnace_cook(f, work, screen, first_tile, "furnace-smelted.png") == 0 &&
          furnace_click(f, screen, 2, false, true, false) == 0 &&
          inventory_field(f, 8, 265, 1) == 0 && furnace_slot(screen, 0, 15, 1) == 0 &&
          furnace_slot(screen, 1, 263, 1) == 0 && furnace_slot(screen, 2, 0, 0) == 0);
    /* This authored held Furnace has no crafting achievement history. DC35
     * checks EP4 presence and returns without granting EP5 when it is absent. */
    mc_registry_achievement_view achievements;
    mc_client_stats_writer *actual_writer =
        mc_client_application_live_stats_writer(f->app->h->local_player->fields.minecraft);
    int32_t parent_value = -1, iron_value = -1;
    bool parent_present = true;
    CHECK(f->app->h->local_player->fields.minecraft == f->app &&
          actual_writer == f->app->stats_writer &&
          mc_registry_classes_inspect_achievements(f->app->registry, &achievements, NULL) ==
              MC_REGISTRY_OK &&
          achievements.named != NULL);
    const mc_achievement_fields *iron = mc_statistic_achievement_fields_get(achievements.named[5]);
    CHECK(iron != NULL && iron->c == achievements.named[4] &&
          mc_client_stats_writer_contains(actual_writer, achievements.named[4], &parent_present,
                                          NULL) == MC_CLIENT_STATS_OK &&
          !parent_present &&
          mc_client_stats_writer_value(actual_writer, achievements.named[4], &parent_value, NULL) ==
              MC_CLIENT_STATS_OK &&
          parent_value == 0 &&
          mc_client_stats_writer_value(actual_writer, achievements.named[5], &iron_value, NULL) ==
              MC_CLIENT_STATS_OK &&
          iron_value == 0);
    puts("FURNACE_AWARD parent-present=0 parent-value=0 iron-value=0 writer-same=1");
    CHECK(fflush(stdout) == 0 && furnace_close(f, screen) == 0);
    char title[8192];
    CHECK(mining_path(work, "furnace-title.png", title) == 0 &&
          save_title_run(f, NULL, title, false, 1, 0) == 0 &&
          reload_snapshot("furnace-saved") == 0 &&
          furnace_choose(f, &work->epochs[0], &work->epochs[1]) == 0 &&
          inventory_field(f, 8, 265, 1) == 0 && furnace_tap(f, 0, 64, false) == 0 &&
          furnace_wait(f, &screen) == 0);
    mc_tile_entity *reloaded = NULL;
    CHECK(mc_inventory_furnace_tile_borrow(screen->fields.l, &reloaded, NULL) == MC_INVENTORY_OK &&
          reloaded != first_tile && furnace_identity(f, screen, reloaded, true) == 0 &&
          furnace_cook(f, work, screen, reloaded, "furnace-reloaded.png") == 0 &&
          furnace_click(f, screen, 2, false, true, false) == 0 &&
          inventory_field(f, 8, 265, 2) == 0 && furnace_slot(screen, 0, 0, 0) == 0 &&
          furnace_slot(screen, 1, 263, 1) == 0 && furnace_slot(screen, 2, 0, 0) == 0 &&
          save_title_host(f, "after-furnace-reload", false, frame_id, canvas_id) == 0 &&
          furnace_stop_open(f, screen, reloaded, *frame_id) == 0);
    puts("FURNACE_SHARED placed=1 real-ticks=1 shared-sk=1 output-taken=2 save-burn=1 epochs=2 "
         "source-field-repair=0");
    puts("FURNACE_SDK profile=cycle ordinary-save-reload=1 epochs=2 source-field-repair=0");
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
        result = furnace_run(f, &work, &frame_id, &canvas_id);
    }
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop)) {
        if (mc_client_menu_loop_inspect(f->loop, &f->state))
            fprintf(stderr, "Furnace terminal cleanup refused: %s\n",
                    f->state.retirement_operation);
        result = 1;
    }
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    if (closed != MC_CLIENT_BOOTSTRAP_OK || f->bootstrap != NULL || f->loop != NULL ||
        frame_id == 0 || canvas_id == 0 || SDL_GetWindowFromID(frame_id) != NULL ||
        SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("FURNACE_FINISH profile=cycle cleanup=1 windows-absent=1 checks=%u\n", checks);
    free(f);
    return result;
}
