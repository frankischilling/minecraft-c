/* Authored saved Player, actual SDK Food/Soup clicks and ordinary save/reselection. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool food_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame food_observed_frame
#define main mc_food_included_selection_main
#include "../startup/test_world_selection_loop.c"
#undef main
#undef mc_client_menu_loop_frame
#include "mc/entity/nbt/errors.h"
#include "mc/inventory/player_current.h"

typedef struct food_work {
    fixture *fixture;
    const char *directory;
    reload_epoch first, reloaded;
    mc_item_stack *soup_hold;
    bool right_down, saving, selected;
    size_t save_samples, selected_sample;
    int32_t entry_health, entry_ground, entry_above;
} food_work;

static food_work *food;

static int food_path(const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", food->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

/* Passive owned-tree serialization, outside any source save or extra frame. */
static int food_player_nbt(mc_entity *player, const char *name) {
    int result = 1;
    mc_entity_nbt_error *error = calloc(1, sizeof(*error));
    mc_nbt_tree *tree = NULL;
    uint8_t *wire = NULL;
    char path[8192];
    if (error == NULL || mc_entity_build_nbt(player, NULL, &tree, error) != MC_ENTITY_NBT_OK)
        goto cleanup;
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    size_t bytes = 0, written = 0;
    mc_nbt_error wire_error;
    if (mc_nbt_measure(mc_nbt_tree_root(tree), &limits, &bytes, &wire_error) != MC_NBT_OK ||
        bytes > 1048576 || (wire = malloc(bytes)) == NULL ||
        mc_nbt_write(mc_nbt_tree_root(tree), &limits, wire, bytes, &written, &wire_error) !=
            MC_NBT_OK ||
        written != bytes || food_path(name, path) != 0 || !SDL_SaveFile(path, wire, bytes))
        goto cleanup;
    result = 0;
cleanup:
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return result;
}

static int food_inventory(fixture *f, int32_t selected, int32_t health, int32_t apples,
                          int32_t second_id, int32_t second_count, const mc_item_stack **current) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          player.living.Y == health &&
          mc_inventory_player_read_state(player.player.c, &inventory) &&
          inventory.selected == selected);
    for (int32_t i = 0; i < 36; ++i) {
        const mc_item_stack *stack = NULL;
        CHECK(mc_inventory_player_borrow(player.player.c, i, &stack, NULL) == MC_INVENTORY_OK);
        if (i < 2) {
            mc_item_stack_fields fields;
            CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                  fields.id == (i == 0 ? 260 : second_id) &&
                  fields.count == (i == 0 ? apples : second_count) && fields.damage == 0);
            if (i == selected && current != NULL)
                *current = stack;
        } else
            CHECK(stack == NULL);
    }
    return 0;
}

static int food_statistic(fixture *f, int32_t id) {
    mc_registry_statistic_view view;
    int32_t used = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &view, NULL) == MC_REGISTRY_OK &&
          view.E != NULL && view.E[id] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, view.E[id], &used, NULL) ==
              MC_CLIENT_STATS_OK &&
          used == 0);
    return 0;
}

/* Sample the exact paused entry of the frame that actually selects Save/Quit.
 * Reads and owned NBT serialization balance their native holds. */
static int food_entry(fixture *f) {
    mc_chunk_state *chunk = NULL;
    const size_t count = mc_world_store_chunk_count(food->first.handler);
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *candidate = NULL;
        CHECK(mc_world_store_chunk_at(food->first.handler, i, &candidate, NULL) ==
              MC_WORLD_STORAGE_OK);
        if (candidate != NULL && candidate->x == 0 && candidate->z == 0) {
            CHECK(chunk == NULL);
            chunk = candidate;
        }
    }
    mc_local_player_snapshot player;
    const size_t index = (3u << 11u) | (3u << 7u) | 64u;
    CHECK(chunk != NULL && !chunk->blank && chunk->data != NULL && chunk->data->blocks != NULL &&
          mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK);
    food->entry_health = player.living.Y;
    food->entry_ground = chunk->data->blocks[index];
    food->entry_above = chunk->data->blocks[index + 1u];
    char name[96];
    const int n = snprintf(name, sizeof(name), "food-player-entry-%04zu.nbt", food->save_samples);
    CHECK(n > 0 && (size_t)n < sizeof(name) && food_player_nbt(f->app->h, name) == 0);
    ++food->save_samples;
    return 0;
}

static bool food_observed_frame(mc_client_menu_loop *loop) {
    fixture *f = food == NULL ? NULL : food->fixture;
    const bool sample = food != NULL && food->saving && !food->selected && f != NULL &&
                        f->loop == loop && f->app->active_world == food->first.world &&
                        f->app->h == food->first.player && f->app->r != NULL &&
                        f->app->r->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
                        atomic_load(&f->app->o);
    if (sample && food_entry(f) != 0)
        return false;
    const bool returned = mc_client_menu_loop_frame(loop);
    if (sample && returned && f->app->active_world == NULL && f->app->h == NULL &&
        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        food->selected = true;
        food->selected_sample = food->save_samples - 1u;
    }
    return returned;
}

static int food_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0 && !f->state.failed);
    return 0;
}

static int food_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < deadline; ++i) {
        CHECK(food_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL || !f->app->N) {
            mc_chunk_provider_fields provider;
            size_t eligible = 0;
            int64_t world_time = 0;
            CHECK(mc_chunk_provider_bind_fields(f->app->active_world->fields.v, &provider, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  save_title_resume(f, f->app->active_world, &provider, recovered, &pauses,
                                    &eligible, &world_time) == 0);
            continue;
        }
        const mc_client_world_frame_state *draw = f->state.world_frame;
        if (f->app->h != NULL && f->app->i == f->app->h && draw != NULL && draw->returned &&
            draw->terrain_returned && draw->updates_returned && draw->source_pc == 1013 &&
            f->state.hud != NULL && f->state.hud->returned)
            return 0;
    }
    return 1;
}

static int food_choose_first(fixture *f) {
    mc_client_source_rq *rq = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &rq) == 0 && release_mouse(f, rq) == 0);
    int32_t selected = -1;
    for (size_t i = 0; i < mc_client_save_list_size(rq->fields.n); ++i)
        if (filename_is(mc_client_save_list_at(rq->fields.n, i), "alpha")) {
            CHECK(selected == -1);
            selected = (int32_t)i;
        }
    CHECK(selected >= 0);
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + selected * 36 + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 5000;
    while (rq->fields.m != selected && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.m == selected && release_mouse(f, rq) == 0 &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0 && food_ready(f) == 0 && reload_capture_epoch(f, &food->first) == 0);
    return 0;
}

static int food_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    food->right_down = down;
    return 0;
}

static int food_draw(fixture *f, const mc_item_stack *selected, int32_t slot, int32_t health) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        const mc_client_world_frame_state *draw = f->state.world_frame;
        const mc_client_first_person_hand_state *hand = draw == NULL ? NULL : draw->hand;
        const mc_client_held_item_draw_state *held = hand == NULL ? NULL : hand->held_item;
        mc_client_entity_renderer_state renderer;
        mc_client_held_item_state live;
        const bool live_selected =
            mc_client_entity_renderer_inspect(f->app->t, &renderer) &&
            renderer.ra_occurrence != NULL &&
            mc_client_held_item_identity(renderer.ra_occurrence) == renderer.fields.c &&
            mc_client_held_item_inspect(renderer.ra_occurrence, &live) &&
            live.pending_ra == renderer.fields.c && live.ra_app == f->app &&
            live.ra_b == (const void *)selected && live.ra_g == slot && live.ra_c == 1.0f;
        /* The returned hand pass has retired RA.a before its overlays and
         * cleared borrowed child pointers. Scalar draw receipts survive;
         * identity belongs to the same still-live canonical RA owner. */
        if (draw != NULL && draw->returned && draw->source_pc == 1013 && hand != NULL &&
            hand->returned && hand->held_returned && held != NULL && held->returned &&
            !held->empty_hand && held->stack_captured && held->stack == NULL &&
            held->owner == NULL && held->player == NULL && held->failure == NULL && live_selected &&
            held->flat_item && held->item_icon_returned && held->item_geometry_returned &&
            f->state.hud != NULL && f->state.hud->returned && f->state.hud->slots_returned &&
            f->state.hud->health == health && f->state.hud->selected == slot &&
            f->state.hud->item_slots == 2 && f->state.hud->empty_slots == 7)
            return 0;
        CHECK(food_frame(f) == 0);
    }
    return 1;
}

static int food_tap(fixture *f, const char *action) {
    /* Wait for an ordinary short source frame before offering the sole tap.
     * Observe its existing P; never rewrite clocks or filter queued input. */
    const uint64_t tick_before = f->state.ticks, deadline = SDL_GetTicks() + 60000;
    int64_t age = INT64_MAX;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        int64_t now;
        CHECK(food_frame(f) == 0 && mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        age = mc_java_i64_sub(now, f->app->P);
        if (f->state.ticks > tick_before && age <= 100)
            break;
    }
    CHECK(f->state.ticks > tick_before && age <= 100 && f->app->r == NULL && f->app->N);
    const uint64_t clicks = f->state.gameplay_right_clicks, air = f->state.gameplay_air_uses,
                   used = f->state.gameplay_used_statistics,
                   counts = f->state.gameplay_stack_count_stores,
                   placed = f->state.gameplay_placed_blocks,
                   sounds = f->state.gameplay_placement_sounds,
                   clears = f->state.gameplay_selected_slot_clears,
                   equip = f->state.gameplay_equip_resets;
    const size_t events = f->state.mouse_events;
    CHECK(food_button(f, true) == 0 && food_button(f, false) == 0);
    bool drained = false, observed = false;
    for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(food_frame(f) == 0);
        if (!observed && f->state.gameplay_air_uses > air) {
            /* Later ticks in this frame may replace the last-input record.
             * Compare persistent source totals below; inspect only completed
             * native child retirement in this borrowed final-tick view. */
            const mc_client_gameplay_input *input = f->state.gameplay_input;
            CHECK(input != NULL && input->returned && !input->failed && input->use_call == NULL &&
                  input->click_air_stack == NULL && input->click_block_stack == NULL &&
                  input->use_state.item == NULL);
            observed = true;
        }
        mc_client_sdk_input_state input;
        mc_client_sdk_bytes_state read, queue, buttons;
        CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
              mc_client_sdk_bytes_inspect(input.mouse_read, &read) &&
              mc_client_sdk_queue_inspect(input.mouse_events, &queue) &&
              mc_client_sdk_bytes_inspect(input.buttons, &buttons) && buttons.limit > 1 &&
              buttons.data[1] == 0);
        drained = f->state.mouse_events >= events + 2 && read.position == read.limit &&
                  queue.position == 0 && input.pending_native_event == NULL;
        if (drained && observed)
            break;
    }
    CHECK(drained && observed && f->state.gameplay_right_clicks == clicks + 1 &&
          f->state.gameplay_air_uses == air + 1 && f->state.gameplay_used_statistics == used &&
          f->state.gameplay_stack_count_stores == counts + 1 &&
          f->state.gameplay_placed_blocks == placed &&
          f->state.gameplay_placement_sounds == sounds &&
          f->state.gameplay_selected_slot_clears == clears &&
          f->state.gameplay_equip_resets == equip + 1);
    printf("FOOD_INPUT action=%s taps=1 right-clicks=1 air-uses=1 count-stores=1 used=0 "
           "cleared=0 equip-reset=1 child-retired=1 drained=1\n",
           action);
    return 0;
}

static int food_run(fixture *f) {
    CHECK(repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          food_choose_first(f) == 0 && food_inventory(f, 0, 5, 2, 282, 3, NULL) == 0 &&
          food_statistic(f, 260) == 0 && food_statistic(f, 282) == 0);
    mc_entity_snapshot pose;
    mc_game_settings_state settings;
    CHECK(mc_entity_get_snapshot(f->app->h, &pose) == MC_ENTITY_OK && pose.bounds != NULL &&
          pose.bounds->min_y == 65 && pose.base.x == 3.5 && pose.base.z == .5 &&
          pose.base.y == 65.0 + (double)1.62f && pose.base.yaw == 0 && pose.base.aT == 26 &&
          mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.difficulty == 2 && f->app->active_world->fields.q == 2);
    const mc_item_stack *apple = NULL, *current = NULL;
    CHECK(food_inventory(f, 0, 5, 2, 282, 3, &apple) == 0);
    CHECK(food_draw(f, apple, 0, 5) == 0);
    CHECK(food_player_nbt(f->app->h, "food-player-before.nbt") == 0);
    CHECK(food_tap(f, "apple") == 0);
    CHECK(food_inventory(f, 0, 9, 1, 282, 3, &current) == 0 && current == apple);
    CHECK(food_draw(f, current, 0, 9) == 0);
    CHECK(food_statistic(f, 260) == 0);
    CHECK(food_player_nbt(f->app->h, "food-player-apple.nbt") == 0);
    const size_t keys = f->state.keyboard_events;
    CHECK(gameplay_key(f, SDL_SCANCODE_2, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_2, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; f->state.keyboard_events < keys + 2 && i < 64 && SDL_GetTicks() < deadline;
         ++i)
        CHECK(food_frame(f) == 0);
    CHECK(f->state.keyboard_events >= keys + 2 &&
          food_inventory(f, 1, 9, 1, 282, 3, &current) == 0 && current != apple &&
          food_draw(f, current, 1, 9) == 0 &&
          mc_item_stack_retain((mc_item_stack *)current) == MC_ITEM_STACK_OK);
    food->soup_hold = (mc_item_stack *)current;
    CHECK(food_tap(f, "soup") == 0 && food_inventory(f, 1, 19, 1, 281, 1, &current) == 0 &&
          current != food->soup_hold && current != apple && mc_item_stack_source(current) == NULL);
    mc_item_stack_fields displaced;
    mc_item_stack_phase phase;
    CHECK(mc_item_stack_fields_get(food->soup_hold, &displaced) == MC_ITEM_STACK_OK &&
          displaced.id == 282 && displaced.count == 2 && displaced.damage == 0 &&
          mc_item_stack_phase_get(current, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_READY && food_draw(f, current, 1, 19) == 0 &&
          food_statistic(f, 260) == 0 && food_statistic(f, 282) == 0 &&
          food_player_nbt(f->app->h, "food-player-soup.nbt") == 0);
    mc_item_stack_release(food->soup_hold);
    food->soup_hold = NULL;
    char path[8192];
    CHECK(food_path("food-returned.png", path) == 0 && capture(f, path) == 0);
    puts("FOOD_RETURN health-before=5 health-apple=9 health-soup=19 apple-count=1 "
         "displaced-soup-count=2 bowl-id=281 bowl-count=1 same-apple=1 fresh-bowl=1 "
         "selected=1 hud=1 held=1 used=0 displaced-hold-released=1");
    food->saving = true;
    CHECK(food_path("food-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 1, 0);
    food->saving = false;
    CHECK(saved == 0 && food->selected && food->save_samples > 0 && food->entry_health == 19 &&
          food->entry_ground == 3 && food->entry_above == 0 && reload_snapshot("food-saved") == 0);
    printf("FOOD_SAVE sample=%zu candidates=%zu health=19 ground=3 above=0 "
           "source-save-title=1 pre-frame-owned-nbt=1\n",
           food->selected_sample, food->save_samples);
    CHECK(reload_choose(f, f->app->r, "alpha", &food->first, &food->reloaded, 2) == 0 &&
          food_ready(f) == 0 && food_inventory(f, 0, 19, 1, 281, 1, &current) == 0 &&
          food_draw(f, current, 0, 19) == 0 && food_statistic(f, 260) == 0 &&
          food_statistic(f, 282) == 0 &&
          food_player_nbt(f->app->h, "food-player-reloaded.nbt") == 0);
    puts(
        "FOOD_RELOAD distinct-world=1 distinct-player=1 health=19 apple-count=1 "
        "bowl-id=281 bowl-count=1 selected=0 source-selected-slot-not-saved=1 hud=1 held=1 used=0");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    food_work *work = calloc(1, sizeof(*work));
    if (f == NULL || work == NULL) {
        free(work);
        free(f);
        return 2;
    }
    food = work;
    work->fixture = f;
    work->directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        mc_client_frame_state host;
        if (f->app != NULL &&
            mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
            host.frame_window != NULL && host.canvas_window != NULL) {
            frame_id = SDL_GetWindowID(host.frame_window);
            canvas_id = SDL_GetWindowID(host.canvas_window);
            result = food_run(f);
        }
    }
    work->saving = false;
    if (f->app != NULL && work->right_down)
        (void)food_button(f, false);
    mc_item_stack_release(work->soup_hold);
    work->soup_hold = NULL;
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
        frame_id == 0 || canvas_id == 0 || SDL_GetWindowFromID(frame_id) != NULL ||
        SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("FOOD_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    food = NULL;
    free(work);
    free(f);
    return result;
}
