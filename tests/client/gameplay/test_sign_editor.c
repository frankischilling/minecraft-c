/* Two authored saved one-Sign slots through actual SDK/VC/DC/YC, Done and save.
 * SDL records are test transport, not physical keyboard or original pixels. */
int mc_sign_editor_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_sign_editor_included_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

#include "client/gui/source_sign_drawing.h"
#include "client/gui/source_sign_screen.h"
#include "mc/inventory/player_current.h"

static struct {
    mc_client_source_da *held;
    mc_tile_entity_name *line;
    size_t frames, gui_frames[2];
    uintptr_t screen_identity[2];
} sign_gameplay;

static const uint16_t first0[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                  'I', 'J', 'K', 'L', 'M', 'N', 'Z'};
static const uint16_t first1[] = {0x00e9, ' ', 'B'};
static const uint16_t first2[] = {'r', 'o', 'w', '2'};
static const uint16_t first3[] = {'l', 'a', 's', 't'};
static const uint16_t second0[] = {'t', 'w', 'o'};
static const uint16_t second1[] = {'2'};
static const uint16_t second2[] = {'d', 'o', 'n', 'e'};

static int sign_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0);
    const bool returned = mc_client_menu_loop_frame(f->loop);
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
    if (!returned) {
        fprintf(stderr, "Sign gameplay frontier %s.%s:%" PRId64 ": %s\n",
                f->state.source_class == NULL ? "native" : f->state.source_class,
                f->state.source_method == NULL ? "frame" : f->state.source_method,
                f->state.source_pc, f->state.operation == NULL ? "(none)" : f->state.operation);
        if (f->state.world_frame != NULL && f->state.world_frame->entities != NULL) {
            const mc_client_render_global_entities_state *entities = f->state.world_frame->entities;
            fprintf(stderr,
                    "SIGN_GAMEPLAY_ENTITY_PREFIX delayed=%d initial=%" PRId32 " remaining=%" PRId32
                    " ll-returned=%d th-returned=%d returned=%d pc=%u\n",
                    entities->delayed ? 1 : 0, entities->initial_delay, entities->remaining_delay,
                    entities->ll_returned ? 1 : 0, entities->th_returned ? 1 : 0,
                    entities->returned ? 1 : 0, entities->source_pc);
        }
        return 1;
    }
    CHECK(!f->state.failed && f->state.render_returned && f->state.clocks.frame_returned &&
          f->state.clocks.timer_ticks >= 0 && f->state.clocks.timer_ticks <= 10 &&
          f->state.clocks.tick_returns == f->state.clocks.timer_ticks);
    ++sign_gameplay.frames;
    return 0;
}

static bool sign_hit(fixture *f, int32_t x) {
    mc_entity_hit_result_view hit;
    return f->app->r == NULL && f->app->N && f->app->y != NULL &&
           mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK && hit.fields.b == x &&
           hit.fields.c == 63 && hit.fields.d == 2 && hit.fields.e == 1;
}

static int sign_inventory_state(fixture *f, mc_inventory_player_state *out) {
    mc_local_player_snapshot player;
    CHECK(f->app != NULL && f->app->h != NULL &&
          mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          player.player.c != NULL && mc_inventory_player_read_state(player.player.c, out));
    return 0;
}

static int sign_slot(fixture *f, int32_t slot, bool present) {
    mc_inventory_player_state inventory;
    mc_item_stack_fields fields;
    CHECK(slot >= 0 && slot < 36 && sign_inventory_state(f, &inventory) == 0 &&
          inventory.main_slots != NULL);
    const mc_item_stack *stack = inventory.main_slots[slot];
    CHECK(present ? stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                        fields.id == 323 && fields.count == 1 && fields.damage == 0
                  : stack == NULL);
    return 0;
}

static int sign_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view statistics;
    int32_t value = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.E != NULL && statistics.E[323] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.E[323], &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          value == expected);
    return 0;
}

static int sign_ready(fixture *f, bool picked) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 1024 && SDL_GetTicks() < deadline; ++i) {
        CHECK(sign_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL) {
            size_t eligible;
            int64_t time;
            CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &pauses, &eligible,
                                    &time) == 0);
            continue;
        }
        const mc_client_render_global_entities_state *entities =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->entities;
        if (f->app->N && !atomic_load(&f->app->o) && f->state.world_frame != NULL &&
            f->state.world_frame->returned && f->state.hud != NULL && f->state.hud->returned &&
            entities != NULL && entities->returned && !entities->delayed &&
            entities->initial_delay == 0 && entities->remaining_delay == 0 &&
            entities->ll_returned && entities->th_returned && entities->source_pc == 569 &&
            (!picked || sign_hit(f, 0)))
            return 0;
    }
    fputs("Sign gameplay did not reach real focused World/HUD/picking\n", stderr);
    return 1;
}

static int sign_choose(fixture *f, const reload_epoch *before, reload_epoch *out) {
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
        CHECK(sign_frame(f) == 0);
    CHECK(screen->fields.m == index && release_mouse(f, screen) == 0 &&
          push_button(f, &screen->base, 1, true) == 0 &&
          push_button(f, &screen->base, 1, false) == 0 && transfer(f) == 0 &&
          sign_ready(f, before == NULL) == 0 && reload_capture_epoch(f, out) == 0);
    if (before != NULL)
        CHECK(out->world != before->world && out->player != before->player &&
              out->handler != before->handler && out->controller != before->controller &&
              out->call != before->call);
    printf("SIGN_GAMEPLAY_READY epoch=%d distinct-world-player=%d\n", before == NULL ? 0 : 1,
           before != NULL);
    return 0;
}

static int sign_line(mc_tile_entity *tile, int32_t row, const uint16_t *units, size_t length) {
    mc_tile_entity_name *name = NULL;
    mc_tile_entity_name_view view = {0};
    const mc_tile_entity_status got = mc_tile_entity_sign_get_line(tile, row, &name);
    const bool equal =
        got == MC_TILE_ENTITY_OK &&
        mc_tile_entity_name_get_view(name, &view) == MC_TILE_ENTITY_OK && !view.is_null &&
        view.text.length == length &&
        (length == 0 || memcmp(view.text.units, units, length * sizeof(*units)) == 0);
    mc_tile_entity_name_release(name);
    CHECK(equal);
    return 0;
}

static int sign_rows(mc_tile_entity *tile, size_t which, const char *phase) {
    const uint16_t *const first[] = {first0, first1, first2, first3};
    const size_t first_lengths[] = {15, 3, 4, 4};
    const uint16_t *const second[] = {second0, second1, second2, NULL};
    const size_t second_lengths[] = {3, 1, 4, 0};
    for (int32_t row = 0; row < 4; ++row) {
        const uint16_t *units = which == 0 ? first[row] : second[row];
        const size_t length = which == 0 ? first_lengths[row] : second_lengths[row];
        mc_tile_entity_name *line = NULL;
        mc_tile_entity_name_view actual = {0};
        const bool equal =
            mc_tile_entity_sign_get_line(tile, row, &line) == MC_TILE_ENTITY_OK &&
            mc_tile_entity_name_get_view(line, &actual) == MC_TILE_ENTITY_OK && !actual.is_null &&
            actual.text.length == length &&
            (length == 0 || memcmp(actual.text.units, units, length * sizeof(*units)) == 0);
        if (equal) {
            printf("SIGN_GAMEPLAY_ROW phase=%s sign=%zu row=%" PRId32 " units=", phase, which, row);
            if (actual.text.length == 0)
                putchar('-');
            for (size_t i = 0; i < actual.text.length; ++i)
                printf("%04x", (unsigned int)actual.text.units[i]);
            putchar('\n');
        }
        mc_tile_entity_name_release(line);
        CHECK(equal);
    }
    return 0;
}

static int sign_ui(fixture *f, mc_client_source_yc *screen, size_t which) {
    mc_client_sign_screen_state state;
    mc_tile_entity_snapshot tile;
    mc_client_sdk_input_state input;
    const mc_client_sign_draw_state *draw = f->state.sign;
    CHECK(f->app->r == mc_client_screen_sign_base(screen) && !f->app->N &&
          mc_client_screen_sign_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_READY && state.initialization_count == 1 &&
          state.tile != NULL && state.failure != NULL &&
          state.failure->status == MC_CLIENT_SCREEN_OK &&
          mc_tile_entity_get_snapshot(state.tile, &tile) == MC_TILE_ENTITY_OK &&
          tile.kind == MC_TILE_ENTITY_SIGN && tile.world == f->app->active_world && !tile.invalid &&
          tile.x == (int32_t)which && tile.y == 64 && tile.z == 2 && tile.sign_read_flag &&
          tile.sign_line_count == 4 && tile.sign_edit_row == -1 &&
          mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
          input.repeat_enabled && draw != NULL && draw->receiver == screen && draw->returned &&
          draw->source_pc == 236 && draw->background_returned && draw->title_returned &&
          draw->matrix_pushed && draw->block_returned && draw->metadata_returned &&
          draw->standing && draw->metadata == 8 && draw->preview_returned && draw->marker_cleared &&
          draw->matrix_popped && draw->buttons_returned && draw->buttons_seen == 1 &&
          draw->buttons_drawn == 1 && draw->preview != NULL && draw->preview->returned &&
          draw->preview->sign_rows_measured == 4 && draw->preview->sign_rows_drawn == 4);
    ++sign_gameplay.gui_frames[which];
    return 0;
}

static int sign_open(fixture *f, size_t which, mc_client_source_yc **out) {
    const uint64_t clicks = f->state.gameplay_right_clicks;
    const uint64_t uses = f->state.gameplay_block_uses;
    const uint64_t placed = f->state.gameplay_placed_blocks;
    const uint64_t counts = f->state.gameplay_stack_count_stores;
    const uint64_t statistics = f->state.gameplay_used_statistics;
    const uint64_t sounds = f->state.gameplay_placement_sounds;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    CHECK(sign_hit(f, (int32_t)which) && sign_slot(f, (int32_t)which, true) == 0);
    for (size_t attempt = 0; attempt < 12 && frames < 96 && SDL_GetTicks() < deadline; ++attempt) {
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
            CHECK(sign_frame(f) == 0);
            ++frames;
            mc_client_sdk_input_state input;
            mc_client_sdk_bytes_state read, queue, buttons;
            CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
                  mc_client_sdk_bytes_inspect(input.mouse_read, &read) &&
                  mc_client_sdk_queue_inspect(input.mouse_events, &queue) &&
                  mc_client_sdk_bytes_inspect(input.buttons, &buttons) && buttons.limit > 1 &&
                  buttons.data[1] == 0);
            drained = f->state.mouse_events > events && read.position == read.limit &&
                      queue.position == 0 && input.pending_native_event == NULL;
            if (drained && f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_SIGN) {
                *out = (mc_client_source_yc *)f->app->r;
                CHECK(mc_client_screen_retain(f->app->r, NULL) == MC_CLIENT_SCREEN_OK);
                sign_gameplay.held = f->app->r;
                sign_gameplay.screen_identity[which] = (uintptr_t)*out;
                CHECK((which == 0 || sign_gameplay.screen_identity[0] != (uintptr_t)*out) &&
                      sign_ui(f, *out, which) == 0 &&
                      f->state.gameplay_right_clicks == clicks + 1 &&
                      f->state.gameplay_block_uses == uses + 1 &&
                      f->state.gameplay_placed_blocks == placed + 1 &&
                      f->state.gameplay_stack_count_stores == counts + 1 &&
                      f->state.gameplay_used_statistics == statistics + 1 &&
                      f->state.gameplay_placement_sounds == sounds &&
                      sign_slot(f, (int32_t)which, false) == 0 &&
                      sign_statistic(f, (int32_t)which + 1) == 0);
                printf("SIGN_GAMEPLAY_PLACE sign=%zu attempts=%zu vc-dc-yc=1 count-store=1 "
                       "iz-statistic=1 slot-cleared=1 placement-sound=0 rows=4\n",
                       which, attempt + 1);
                return 0;
            }
        } while (!drained && frames < 96 && SDL_GetTicks() < deadline);
        /* Retry only a source-stale/focus-only tap, never a partial action. */
        CHECK(drained && f->app->r == NULL && f->state.gameplay_right_clicks == clicks &&
              f->state.gameplay_block_uses == uses && f->state.gameplay_placed_blocks == placed &&
              f->state.gameplay_stack_count_stores == counts &&
              f->state.gameplay_used_statistics == statistics);
    }
    fputs("Sign actual VC/DC/editor admission did not return within bounded taps\n", stderr);
    return 1;
}

static int sign_drain_keys(fixture *f, mc_client_source_yc *screen, size_t which, size_t minimum) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    for (size_t i = 0; i < 512 && SDL_GetTicks() < deadline; ++i) {
        CHECK(sign_frame(f) == 0 && sign_ui(f, screen, which) == 0);
        mc_client_sdk_input_state input;
        mc_client_sdk_bytes_state read, queue;
        CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
              mc_client_sdk_bytes_inspect(input.keyboard_read, &read) &&
              mc_client_sdk_queue_inspect(input.keyboard_events, &queue));
        if (f->state.keyboard_events >= minimum && read.position == read.limit &&
            queue.position == 0 && input.pending_native_event == NULL)
            return 0;
    }
    fputs("Sign SDK keyboard drain exceeded bound\n", stderr);
    return 1;
}

static int sign_text(fixture *f, mc_client_source_yc *screen, size_t which, const char *utf8,
                     size_t units) {
    const size_t before = f->state.keyboard_events;
    SDL_Event event = {0};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.text.text = utf8;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0 &&
          sign_drain_keys(f, screen, which, before + units) == 0);
    return 0;
}

static int sign_key(fixture *f, mc_client_source_yc *screen, size_t which, SDL_Scancode key) {
    const size_t before = f->state.keyboard_events;
    CHECK(gameplay_key(f, key, true) == 0 && gameplay_key(f, key, false) == 0 && transfer(f) == 0 &&
          sign_drain_keys(f, screen, which, before + 2) == 0);
    return 0;
}

static int sign_allowed(fixture *f) {
    mc_client_sign_class_state state;
    mc_player_string_view allowed;
    CHECK(mc_client_screen_sign_class_inspect(
              mc_client_screen_transitions_classes(f->app->screen_transitions), &state, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_CLASS_READY &&
          mc_player_string_read(state.accepted, &allowed));
    static const uint16_t selected[] = {'A', 'B', 'Z', ' ', 0x00e9, 0xd83d, 0xde00};
    for (size_t i = 0; i < sizeof(selected) / sizeof(*selected); ++i) {
        int32_t first = -1;
        for (size_t j = 0; j < allowed.length; ++j)
            if (allowed.units[j] == selected[i]) {
                first = (int32_t)j;
                break;
            }
        CHECK((i < 5 && first >= 0) || (i >= 5 && first == -1));
        printf("SIGN_GAMEPLAY_ALLOWED unit=%04x first=%" PRId32 "\n", (unsigned int)selected[i],
               first);
    }
    return 0;
}

static int sign_empty_alias(mc_tile_entity *tile, int32_t a, int32_t b) {
    mc_tile_entity_name *left = NULL, *right = NULL;
    const bool equal = mc_tile_entity_sign_get_line(tile, a, &left) == MC_TILE_ENTITY_OK &&
                       mc_tile_entity_sign_get_line(tile, b, &right) == MC_TILE_ENTITY_OK &&
                       left != NULL && left == right;
    mc_tile_entity_name_release(left);
    mc_tile_entity_name_release(right);
    CHECK(equal && sign_line(tile, a, NULL, 0) == 0);
    return 0;
}

static int sign_direct_raw(fixture *f, mc_client_source_yc *screen) {
    CHECK(mc_client_menu_loop_retire_completed_draws(f->loop));
    mc_client_sign_screen_state before, after;
    mc_tile_entity_snapshot tile_before, tile_after;
    CHECK(mc_client_screen_sign_inspect(screen, &before, NULL) == MC_CLIENT_SCREEN_OK &&
          before.row == 3 &&
          mc_tile_entity_get_snapshot(before.tile, &tile_before) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_sign_get_line(before.tile, 3, &sign_gameplay.line) == MC_TILE_ENTITY_OK);
    const size_t events = f->state.keyboard_events;
    mc_client_screen_error error;
    CHECK(mc_client_screen_sign_key(screen, 0xd83d, 0, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_key(screen, 0xde00, 0, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_inspect(screen, &after, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_tile_entity_get_snapshot(after.tile, &tile_after) == MC_TILE_ENTITY_OK &&
          after.row == before.row && after.tick == before.tick &&
          after.source_calls == before.source_calls + 2 &&
          after.line_stores == before.line_stores && after.row_stores == before.row_stores &&
          tile_before.sign_edit_row == tile_after.sign_edit_row &&
          tile_before.sign_read_flag == tile_after.sign_read_flag &&
          !after.detail->dirty_returned && f->state.keyboard_events == events);
    mc_tile_entity_name *line = NULL;
    const bool same = mc_tile_entity_sign_get_line(after.tile, 3, &line) == MC_TILE_ENTITY_OK &&
                      line == sign_gameplay.line;
    mc_tile_entity_name_release(line);
    CHECK(same);
    mc_tile_entity_name_release(sign_gameplay.line);
    sign_gameplay.line = NULL;
    puts("SIGN_GAMEPLAY_DIRECT raw-high=d83d raw-low=de00 rejected=2 same-line=1 "
         "sdk-delivery=0 tick-marker-dirty-unchanged=1");
    return 0;
}

static int sign_edit(fixture *f, mc_client_source_yc *screen, size_t which) {
    mc_client_sign_screen_state state;
    CHECK(mc_client_screen_sign_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.row == 0 && !state.detail->dirty_returned);
    mc_tile_entity *tile = state.tile; /* Actual Screen/World keep this occurrence live. */
    if (which == 0) {
        CHECK(sign_allowed(f) == 0 && sign_text(f, screen, which, "ABCDEFGHIJKLMNO", 15) == 0 &&
              mc_tile_entity_sign_get_line(tile, 0, &sign_gameplay.line) == MC_TILE_ENTITY_OK &&
              sign_text(f, screen, which, "P", 1) == 0);
        mc_tile_entity_name *line = NULL;
        const bool same = mc_tile_entity_sign_get_line(tile, 0, &line) == MC_TILE_ENTITY_OK &&
                          line == sign_gameplay.line;
        mc_tile_entity_name_release(line);
        CHECK(same);
        mc_tile_entity_name_release(sign_gameplay.line);
        sign_gameplay.line = NULL;
        CHECK(sign_key(f, screen, which, SDL_SCANCODE_BACKSPACE) == 0 &&
              sign_text(f, screen, which, "Z", 1) == 0 &&
              sign_key(f, screen, which, SDL_SCANCODE_DOWN) == 0 &&
              sign_text(f, screen, which, "\xc3\xa9 B", 3) == 0 &&
              sign_key(f, screen, which, SDL_SCANCODE_DOWN) == 0 &&
              sign_text(f, screen, which, "X", 1) == 0 &&
              sign_key(f, screen, which, SDL_SCANCODE_BACKSPACE) == 0 &&
              sign_empty_alias(tile, 2, 3) == 0 && sign_text(f, screen, which, "row2", 4) == 0 &&
              sign_key(f, screen, which, SDL_SCANCODE_DOWN) == 0 &&
              mc_tile_entity_sign_get_line(tile, 3, &sign_gameplay.line) == MC_TILE_ENTITY_OK);
        mc_client_sign_screen_state supplementary_before, supplementary_after;
        CHECK(mc_client_screen_sign_inspect(screen, &supplementary_before, NULL) ==
              MC_CLIENT_SCREEN_OK);
        const size_t supplementary_events = f->state.keyboard_events;
        CHECK(sign_text(f, screen, which, "\xf0\x9f\x98\x80", 2) == 0 &&
              mc_client_screen_sign_inspect(screen, &supplementary_after, NULL) ==
                  MC_CLIENT_SCREEN_OK &&
              f->state.keyboard_events == supplementary_events + 2 &&
              supplementary_after.line_stores == supplementary_before.line_stores &&
              supplementary_after.row_stores == supplementary_before.row_stores &&
              supplementary_after.source_calls - supplementary_before.source_calls ==
                  supplementary_after.tick_stores - supplementary_before.tick_stores + 2);
        line = NULL;
        const bool supplementary_rejected =
            mc_tile_entity_sign_get_line(tile, 3, &line) == MC_TILE_ENTITY_OK &&
            line == sign_gameplay.line;
        mc_tile_entity_name_release(line);
        CHECK(supplementary_rejected);
        mc_tile_entity_name_release(sign_gameplay.line);
        sign_gameplay.line = NULL;
        puts("SIGN_GAMEPLAY_UTF16 cap=15 excess-same-line=1 substring-empty-alias=1 "
             "sdk-supplementary-units=d83dde00 rejected=2");
        CHECK(sign_direct_raw(f, screen) == 0 && sign_text(f, screen, which, "last", 4) == 0);
    } else {
        CHECK(sign_text(f, screen, which, "two", 3) == 0 &&
              sign_key(f, screen, which, SDL_SCANCODE_DOWN) == 0 &&
              sign_text(f, screen, which, "2", 1) == 0 &&
              sign_key(f, screen, which, SDL_SCANCODE_DOWN) == 0 &&
              sign_text(f, screen, which, "done", 4) == 0 &&
              sign_key(f, screen, which, SDL_SCANCODE_DOWN) == 0);
    }
    CHECK(mc_client_screen_sign_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.row == 3 && !state.detail->dirty_returned && sign_rows(tile, which, "edited") == 0);
    printf("SIGN_GAMEPLAY_EDIT sign=%zu rows=4 sdk=1 row=3 dirty-before-done=0\n", which);
    return 0;
}

static int sign_done(fixture *f, mc_client_source_yc *screen, size_t which) {
    CHECK(push_button(f, mc_client_screen_sign_base(screen), 0, true) == 0 &&
          push_button(f, mc_client_screen_sign_base(screen), 0, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    for (size_t i = 0; i < 512 && SDL_GetTicks() < deadline; ++i) {
        CHECK(sign_frame(f) == 0);
        if (f->app->r == NULL && f->app->N && !atomic_load(&f->app->o)) {
            mc_client_sign_screen_state state;
            mc_client_sdk_input_state input;
            CHECK(mc_client_screen_sign_inspect(screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
                  state.failure != NULL && state.failure->status == MC_CLIENT_SCREEN_OK &&
                  state.detail->dirty_returned && state.detail->display_entered &&
                  state.detail->display_returned &&
                  mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
                  !input.repeat_enabled && sign_rows(state.tile, which, "done") == 0 &&
                  sign_gameplay.gui_frames[which] >= 2);
            printf("SIGN_GAMEPLAY_DONE sign=%zu dirty=1 display-returned=1 sp-close-returned=1 "
                   "repeat=0 focused=1 gui-frames=%zu\n",
                   which, sign_gameplay.gui_frames[which]);
            CHECK(mc_client_screen_release(&sign_gameplay.held, NULL) == MC_CLIENT_SCREEN_OK);
            return 0;
        }
    }
    fputs("Sign real Done/SP-close did not return\n", stderr);
    return 1;
}

static int sign_second_target(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 120000;
    for (size_t attempt = 0; attempt < 20 && SDL_GetTicks() < deadline; ++attempt) {
        const double position = f->app->h->base.x;
        if (position > 1.1 && position < 1.9 && sign_hit(f, 1))
            break;
        const SDL_Scancode key = position < 1.5 ? SDL_SCANCODE_A : SDL_SCANCODE_D;
        const size_t events = f->state.keyboard_events;
        CHECK(f->app->r == NULL && gameplay_key(f, key, true) == 0 && transfer(f) == 0 &&
              sign_frame(f) == 0);
        SDL_Delay(60);
        CHECK(gameplay_key(f, key, false) == 0 && transfer(f) == 0);
        while (f->state.keyboard_events < events + 2 && SDL_GetTicks() < deadline)
            CHECK(sign_frame(f) == 0);
        CHECK(sign_frame(f) == 0 && sign_frame(f) == 0 && f->app->h->base.yaw == 0 &&
              f->app->h->base.aT == 30);
        printf("SIGN_GAMEPLAY_STRAFE attempt=%zu key=%s x=%.17g\n", attempt + 1,
               key == SDL_SCANCODE_A ? "A" : "D", f->app->h->base.x);
    }
    CHECK(sign_hit(f, 1));
    const size_t events = f->state.keyboard_events;
    CHECK(gameplay_key(f, SDL_SCANCODE_2, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_2, false) == 0 && transfer(f) == 0);
    mc_inventory_player_state slots;
    do {
        CHECK(sign_frame(f) == 0 && sign_inventory_state(f, &slots) == 0);
    } while ((f->state.keyboard_events < events + 2 || slots.selected != 1) &&
             SDL_GetTicks() < deadline);
    CHECK(slots.selected == 1 && sign_slot(f, 0, false) == 0 && sign_slot(f, 1, true) == 0 &&
          sign_hit(f, 1));
    puts("SIGN_GAMEPLAY_SLOT selected=1 sdk-key=2 stack=323 count=1");
    return 0;
}

static int sign_loaded(fixture *f) {
    mc_world_runtime *world = f->app->active_world;
    CHECK(world != NULL && world->fields.c != NULL && world->fields.c->count == 2);
    bool seen[2] = {false, false};
    for (size_t i = 0; i < world->fields.c->count; ++i) {
        mc_tile_entity *tile = world->fields.c->items[i];
        mc_tile_entity_snapshot state;
        CHECK(mc_tile_entity_get_snapshot(tile, &state) == MC_TILE_ENTITY_OK &&
              state.kind == MC_TILE_ENTITY_SIGN && state.world == world && !state.invalid &&
              state.x >= 0 && state.x <= 1 && state.y == 64 && state.z == 2 &&
              state.sign_line_count == 4 && state.sign_edit_row == -1 && !state.sign_read_flag &&
              !seen[state.x]);
        seen[state.x] = true;
        int32_t id, metadata;
        CHECK(mc_world_runtime_block_get(world, state.x, 64, 2, &id, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_metadata_get(world, state.x, 64, 2, &metadata, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              id == 63 && metadata == 8 && sign_rows(tile, (size_t)state.x, "reloaded") == 0);
    }
    CHECK(seen[0] && seen[1] && sign_slot(f, 0, false) == 0 && sign_slot(f, 1, false) == 0 &&
          sign_statistic(f, 2) == 0 && f->state.world_frame != NULL &&
          f->state.world_frame->entities != NULL &&
          f->state.world_frame->entities->sign_draws_returned == 2);
    puts("SIGN_GAMEPLAY_RELOAD signs=2 rows=8 exact-utf16=1 consumed-slots=2 statistic=2 "
         "distinct-world-player=1 actual-world-draw=1");
    return 0;
}

static int sign_run(fixture *f, mining_work *work, uint32_t *frame_id, uint32_t *canvas_id) {
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && sign_choose(f, NULL, &work->epochs[0]) == 0 &&
          sign_slot(f, 0, true) == 0 && sign_slot(f, 1, true) == 0 && sign_statistic(f, 0) == 0);
    mc_client_source_yc *screen = NULL;
    CHECK(sign_open(f, 0, &screen) == 0 && sign_edit(f, screen, 0) == 0 &&
          mining_capture(f, work, "sign-editor-first.png") == 0 && sign_done(f, screen, 0) == 0 &&
          sign_second_target(f) == 0 && sign_open(f, 1, &screen) == 0 &&
          sign_edit(f, screen, 1) == 0 && mining_capture(f, work, "sign-editor-second.png") == 0 &&
          sign_done(f, screen, 1) == 0 &&
          mining_capture(f, work, "sign-gameplay-before-save.png") == 0);
    printf("SIGN_GAMEPLAY_BOUNDED placements=2 editors=2 done=2 frames=%" PRIu64 " "
           "field-repair=0 physical-input=0 fps-claim=0\n",
           f->state.frames);
    char title[8192];
    CHECK(mining_path(work, "sign-gameplay-title.png", title) == 0 &&
          save_title_run(f, NULL, title, false, 1, 0) == 0 && reload_snapshot("sign-saved") == 0 &&
          sign_choose(f, &work->epochs[0], &work->epochs[1]) == 0 && sign_loaded(f) == 0 &&
          mining_capture(f, work, "sign-gameplay-reloaded.png") == 0 &&
          save_title_host(f, "after-sign-gameplay", false, frame_id, canvas_id) == 0);
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
        result = sign_run(f, &work, &frame_id, &canvas_id);
    }
    mc_tile_entity_name_release(sign_gameplay.line);
    sign_gameplay.line = NULL;
    if (sign_gameplay.held != NULL &&
        mc_client_screen_release(&sign_gameplay.held, NULL) != MC_CLIENT_SCREEN_OK)
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
            puts("SIGN_GAMEPLAY_STOP actual-frame-close=1 source-frame-retry=0");
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
        sign_gameplay.held != NULL || frame_id == 0 || canvas_id == 0 ||
        SDL_GetWindowFromID(frame_id) != NULL || SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("SIGN_GAMEPLAY_FINISH cleanup=1 windows-absent=1 checks=%u source-field-repair=0\n",
               checks);
    free(f);
    return result;
}
