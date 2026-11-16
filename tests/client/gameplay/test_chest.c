/* Authored saved Chest/Player inputs through real picking, SDK and HP/CD. */
int mc_chest_included_inventory_main(int, char **);
#define MC_INVENTORY_FIXTURE_MAIN mc_chest_included_inventory_main
#include "test_inventory.c"
#undef MC_INVENTORY_FIXTURE_MAIN
#include "client/gui/source_chest_screen_internal.h"
#include "inventory/chest_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"

static const char *chest_profile;

typedef struct chest_terminal_snapshot {
    mc_client_source_da *screen;
    mc_entity *player;
    mc_world_runtime *world;
    mc_inventory_player *inventory;
    mc_container *default_container, *active_container, *screen_container;
    mc_client_screen_hp_fields hp;
    mc_client_screen_id_fields id;
    int32_t selected;
    int64_t world_time;
    uint64_t ticks, world_ticks;
    size_t source_calls, stack_count;
    bool changed, inherited_closed, direct_closed;
    const mc_item_stack *stacks[256];
    mc_item_stack_fields fields[256];
} chest_terminal_snapshot;

static int chest_snapshot_stack(chest_terminal_snapshot *out, const mc_item_stack *stack) {
    CHECK(out->stack_count < sizeof(out->stacks) / sizeof(*out->stacks));
    out->stacks[out->stack_count] = stack;
    if (stack != NULL)
        CHECK(mc_item_stack_fields_get(stack, &out->fields[out->stack_count]) == MC_ITEM_STACK_OK);
    ++out->stack_count;
    return 0;
}

/* Native passive source-field inspection only. No Inventory source getter,
 * owning hold, World query, event pump or field write enters this snapshot. */
static int chest_snapshot(fixture *f, mc_client_source_hp *screen, chest_terminal_snapshot *out) {
    memset(out, 0, sizeof(*out));
    mc_entity *player = f->app->h;
    mc_inventory_player_state inventory;
    CHECK(player != NULL && player->local_player != NULL && f->app->active_world != NULL &&
          mc_inventory_player_read_state(player->local_player->fields.player.c, &inventory) &&
          mc_client_menu_loop_inspect(f->loop, &f->state));
    out->screen = f->app->r;
    out->player = player;
    out->world = f->app->active_world;
    out->inventory = player->local_player->fields.player.c;
    out->default_container = player->local_player->fields.player.d;
    out->active_container = player->local_player->fields.player.e;
    out->screen_container = screen->container.id.j;
    out->hp = screen->fields;
    out->id = screen->container.id;
    out->selected = inventory.selected;
    out->changed = inventory.changed;
    out->inherited_closed = screen->container.inherited_closed_returned;
    out->direct_closed = screen->container.direct_closed_returned;
    out->source_calls = screen->container.source_calls;
    out->ticks = f->state.ticks;
    out->world_ticks = f->state.world_tick_returns;
    CHECK(mc_world_info_time_get(out->world->fields.x, &out->world_time) == MC_WORLD_INFO_OK);
    for (size_t i = 0; i < 40; ++i)
        CHECK(chest_snapshot_stack(out, i < 36 ? inventory.main_slots[i]
                                               : inventory.armor_slots[i - 36]) == 0);
    CHECK(chest_snapshot_stack(out, inventory.cursor) == 0);
    size_t nodes;
    CHECK(inv_chest_node_count(screen->fields.m, &nodes, NULL) == MC_INVENTORY_OK && nodes <= 3);
    for (size_t i = 0; i < nodes; ++i) {
        inv_chest_node_view node;
        CHECK(inv_chest_node_at(screen->fields.m, i, &node, NULL) == MC_INVENTORY_OK);
        if (node.large)
            continue;
        CHECK(node.tile != NULL && node.tile->kind == MC_TILE_ENTITY_CHEST &&
              node.tile->state.chest.capacity <= 36);
        for (size_t slot = 0; slot < node.tile->state.chest.capacity; ++slot)
            CHECK(chest_snapshot_stack(out, node.tile->state.chest.slots[slot]) == 0);
    }
    mc_container *container = screen->container.id.j;
    CHECK(container != NULL && container->snapshots != NULL && container->snapshots->size <= 90);
    for (size_t i = 0; i < container->snapshots->size; ++i)
        CHECK(chest_snapshot_stack(out, container->snapshots->values[i]) == 0);
    return 0;
}

static int chest_stop_open(fixture *f, mc_client_source_hp *screen, uint32_t frame_id) {
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
    CHECK(!atomic_load(&f->app->J) && mc_client_menu_loop_frame(f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && !f->state.running &&
          !f->state.failed &&
          mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
          host.close_dispatched && !screen->container.inherited_closed_returned &&
          !screen->container.direct_closed_returned && reload_snapshot("chest-open-stopped") == 0);
    chest_terminal_snapshot *before = calloc(1, sizeof(*before)),
                            *after = calloc(1, sizeof(*after));
    CHECK(before != NULL && after != NULL && chest_snapshot(f, screen, before) == 0);
    const bool prepared = mc_client_menu_loop_terminal_prepare(f->loop);
    CHECK(chest_snapshot(f, screen, after) == 0 && memcmp(before, after, sizeof(*before)) == 0);
    free(after);
    free(before);
    if (!prepared) {
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
        fprintf(stderr, "Chest open native prepare refused: %s\n", f->state.retirement_operation);
        return 1;
    }
    CHECK(reload_snapshot("chest-open-prepared") == 0);
    puts("CHEST_OPEN_PREPARE source-unchanged=1 world-time-unchanged=1 ticks-unchanged=1 "
         "stacks-unchanged=1 native-prefix-only=1");
    return 0;
}

static int chest_stack(const mc_item_stack *stack, int32_t id, int32_t count) {
    if (count == 0)
        CHECK(stack == NULL);
    else {
        mc_item_stack_fields fields;
        CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == id && fields.count == count && fields.damage == 0);
    }
    return 0;
}

static int chest_slot(mc_client_source_hp *screen, int32_t index, int32_t id, int32_t count) {
    mc_inventory_slot *slot = NULL;
    const mc_item_stack *stack = NULL;
    CHECK(mc_container_source_slot(screen->container.id.j, index, &slot, NULL) == MC_INVENTORY_OK &&
          mc_inventory_slot_source_get(slot, &stack, NULL) == MC_INVENTORY_OK &&
          chest_stack(stack, id, count) == 0);
    return 0;
}

static int chest_ui(fixture *f, mc_client_source_hp *screen, bool cursor) {
    const mc_client_inventory_draw_state *draw = f->state.inventory;
    const int32_t rows = strcmp(chest_profile, "single") == 0 ? 3 : 6;
    CHECK(f->app->r == &screen->container.base && !f->app->N && draw != NULL && draw->returned &&
          draw->chest && !draw->workbench && draw->id_returned && draw->background_returned &&
          draw->foreground_returned && !draw->preview_returned && !draw->mouse_stored &&
          draw->receiver == NULL && draw->screen_receiver == f->app->r &&
          draw->slots_drawn == (size_t)(rows * 9 + 36) && draw->cursor_drawn == cursor &&
          screen->fields.n == rows && screen->container.id.i == 114 + rows * 18 &&
          screen->container.id.j == f->app->h->local_player->fields.player.e);
    return 0;
}

static int chest_wait(fixture *f, mc_client_source_hp **out) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(inventory_frame(f) == 0);
        if (f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_CHEST) {
            *out = (mc_client_source_hp *)f->app->r;
            CHECK(chest_ui(f, *out, false) == 0);
            return 0;
        }
    } while (SDL_GetTicks() < deadline);
    fputs("Chest real SDK activation did not display HP\n", stderr);
    return 1;
}

static int chest_right(fixture *f, bool blocked) {
    mc_entity_hit_result_view hit;
    CHECK(f->app->r == NULL && f->app->y != NULL &&
          mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK && hit.fields.b == 0 &&
          hit.fields.c == 64 && hit.fields.d == 2);
    const size_t before = f->state.mouse_events;
    const uint64_t placed = f->state.gameplay_placed_blocks;
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
        CHECK(inventory_frame(f) == 0);
        if (f->state.mouse_events > before)
            break;
    } while (SDL_GetTicks() < deadline);
    CHECK(f->state.mouse_events > before && f->state.gameplay_placed_blocks == placed);
    if (blocked)
        CHECK(f->app->r == NULL &&
              f->app->h->local_player->fields.player.e == f->app->h->local_player->fields.player.d);
    return 0;
}

static int chest_shift(fixture *f, bool down) {
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

static int chest_click(fixture *f, mc_client_source_hp *screen, int32_t index, bool right,
                       bool shift, bool cursor) {
    if (shift)
        CHECK(chest_shift(f, true) == 0);
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
    CHECK(o->source_calls == before + 1 && chest_ui(f, screen, cursor) == 0);
    if (shift)
        CHECK(chest_shift(f, false) == 0);
    return 0;
}

static int chest_close(fixture *f, mc_client_source_hp *screen) {
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

static int chest_ready(fixture *f) {
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
            hit.fields.b == 0 && hit.fields.c == 64 && hit.fields.d == 2)
            return 0;
    } while (SDL_GetTicks() < deadline);
    fputs("Chest saved pose did not reach genuine focused picking\n", stderr);
    return 1;
}

/* Reuse selection transport, with this scene's own ready predicate. The Dirt
 * mining helper deliberately has a different carried-Stack assertion. */
static int chest_choose(fixture *f, const reload_epoch *previous, reload_epoch *current) {
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
          push_button(f, &screen->base, 1, false) == 0 && transfer(f) == 0 && chest_ready(f) == 0 &&
          reload_capture_epoch(f, current) == 0);
    if (previous != NULL)
        CHECK(current->world != previous->world && current->player != previous->player &&
              current->handler != previous->handler &&
              current->controller != previous->controller && current->call != previous->call);
    return 0;
}

static int chest_run(fixture *f, mining_work *work, uint32_t *frame_id, uint32_t *canvas_id) {
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && ticks(f, 2) == 0 &&
          chest_choose(f, NULL, &work->epochs[0]) == 0);
    const bool blocked = strcmp(chest_profile, "blocked") == 0;
    const bool dense = strcmp(chest_profile, "dense") == 0;
    const bool shutdown = strcmp(chest_profile, "shutdown") == 0;
    CHECK(chest_right(f, blocked) == 0);
    mc_client_source_hp *screen = NULL;
    if (!blocked) {
        CHECK(chest_wait(f, &screen) == 0 && chest_slot(screen, 0, 264, dense ? 2 : 5) == 0 &&
              mining_capture(f, work, "chest-open.png") == 0);
        if (shutdown) {
            CHECK(save_title_host(f, "open-chest-terminal", false, frame_id, canvas_id) == 0 &&
                  chest_stop_open(f, screen, *frame_id) == 0);
            puts("CHEST_OPEN_SHUTDOWN actual-open-hp=1 source-close=0");
            return 0;
        }
        if (dense) {
            CHECK(chest_click(f, screen, 0, true, false, true) == 0 &&
                  chest_slot(screen, 0, 264, 1) == 0 && inventory_frame(f) == 0 &&
                  f->state.inventory->slots_drawn == 90 && !f->state.inventory->tooltip_drawn &&
                  mining_capture(f, work, "chest-dense-cursor.png") == 0 &&
                  chest_click(f, screen, 0, false, false, false) == 0 &&
                  chest_slot(screen, 0, 264, 2) == 0 && inventory_frame(f) == 0 &&
                  f->state.inventory->slots_drawn == 90 && f->state.inventory->tooltip_drawn);
        } else {
            const int32_t cells = screen->fields.n * 9;
            CHECK(chest_click(f, screen, 0, false, false, true) == 0 &&
                  chest_click(f, screen, cells, false, false, false) == 0 &&
                  inventory_field(f, 9, 264, 5) == 0 &&
                  chest_click(f, screen, 1, false, true, false) == 0 &&
                  inventory_field(f, 8, 266, 3) == 0 &&
                  chest_click(f, screen, cells, false, true, false) == 0 &&
                  inventory_field(f, 9, 0, 0) == 0 && chest_slot(screen, 0, 264, 5) == 0 &&
                  chest_slot(screen, 1, 0, 0) == 0);
        }
        CHECK(chest_close(f, screen) == 0);
    }
    char path[8192];
    CHECK(mining_path(work, "chest-title.png", path) == 0 &&
          save_title_run(f, NULL, path, false, 1, 0) == 0 && reload_snapshot("chest-saved") == 0 &&
          chest_choose(f, &work->epochs[0], &work->epochs[1]) == 0 && chest_right(f, blocked) == 0);
    if (!blocked) {
        CHECK(chest_wait(f, &screen) == 0 && chest_slot(screen, 0, 264, dense ? 2 : 5) == 0);
        if (!dense)
            CHECK(chest_slot(screen, 1, 0, 0) == 0 && inventory_field(f, 8, 266, 3) == 0);
        CHECK(mining_capture(f, work, "chest-reloaded.png") == 0 && chest_close(f, screen) == 0);
    }
    CHECK(save_title_host(f, "after-chest-reload", false, frame_id, canvas_id) == 0);
    printf("CHEST_SDK profile=%s ordinary-save-reload=1 epochs=2 source-field-repair=0\n",
           chest_profile);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 && (strcmp(argv[3], "single") == 0 || strcmp(argv[3], "double") == 0 ||
                        strcmp(argv[3], "dense") == 0 || strcmp(argv[3], "blocked") == 0 ||
                        strcmp(argv[3], "shutdown") == 0));
    chest_profile = argv[3];
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
        result = chest_run(f, &work, &frame_id, &canvas_id);
    }
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop)) {
        if (mc_client_menu_loop_inspect(f->loop, &f->state))
            fprintf(stderr, "Chest terminal cleanup refused: %s\n", f->state.retirement_operation);
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
        printf("CHEST_FINISH profile=%s cleanup=1 windows-absent=1 checks=%u\n", chest_profile,
               checks);
    free(f);
    return result;
}
