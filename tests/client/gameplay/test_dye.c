/* Authored saved Crop/Player, one SDK Dye click and ordinary save/reselection. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool dye_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame dye_observed_frame
#define main mc_dye_included_selection_main
#include "../startup/test_world_selection_loop.c"
#undef main
#undef mc_client_menu_loop_frame
#include "mc/entity/nbt/errors.h"
#include "mc/inventory/player_current.h"

typedef struct dye_work {
    fixture *fixture;
    const char *directory;
    reload_epoch first, reloaded;
    bool right_down, saving, selected;
    size_t save_samples, selected_sample;
    int32_t entry_health, entry_ground, entry_above, entry_stage, entry_moisture;
} dye_work;

static dye_work *dye;

static int dye_path(const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", dye->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

/* Passive owned-tree serialization, outside any source save or extra frame. */
static int dye_player_nbt(mc_entity *player, const char *name) {
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
        written != bytes || dye_path(name, path) != 0 || !SDL_SaveFile(path, wire, bytes))
        goto cleanup;
    result = 0;
cleanup:
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return result;
}

static int dye_inventory(fixture *f, int32_t count, const mc_item_stack **current) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          player.living.Y == 5 && mc_inventory_player_read_state(player.player.c, &inventory) &&
          inventory.selected == 0);
    for (int32_t i = 0; i < 36; ++i) {
        const mc_item_stack *stack = NULL;
        CHECK(mc_inventory_player_borrow(player.player.c, i, &stack, NULL) == MC_INVENTORY_OK);
        if (i == 0) {
            mc_item_stack_fields fields;
            CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                  fields.id == 351 && fields.count == count && fields.damage == 15);
            if (current != NULL)
                *current = stack;
        } else
            CHECK(stack == NULL);
    }
    return 0;
}

static int dye_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t used = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &view, NULL) == MC_REGISTRY_OK &&
          view.E != NULL && view.E[351] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, view.E[351], &used, NULL) ==
              MC_CLIENT_STATS_OK &&
          used == expected);
    return 0;
}

/* Sample the exact paused entry of the frame that actually selects Save/Quit.
 * Reads and owned NBT serialization balance their native holds. */
static int dye_entry(fixture *f) {
    mc_chunk_state *chunk = NULL;
    const size_t count = mc_world_store_chunk_count(dye->first.handler);
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *candidate = NULL;
        CHECK(mc_world_store_chunk_at(dye->first.handler, i, &candidate, NULL) ==
              MC_WORLD_STORAGE_OK);
        if (candidate != NULL && candidate->x == 0 && candidate->z == 0) {
            CHECK(chunk == NULL);
            chunk = candidate;
        }
    }
    mc_local_player_snapshot player;
    const size_t index = (3u << 11u) | (3u << 7u) | 64u;
    CHECK(chunk != NULL && !chunk->blank && chunk->data != NULL && chunk->data->blocks != NULL &&
          chunk->data->metadata != NULL &&
          mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK);
    dye->entry_health = player.living.Y;
    dye->entry_ground = chunk->data->blocks[index];
    dye->entry_above = chunk->data->blocks[index + 1u];
    dye->entry_moisture = chunk->data->metadata[index / 2u] & 15;
    dye->entry_stage = chunk->data->metadata[index / 2u] >> 4u;
    char name[96];
    const int n = snprintf(name, sizeof(name), "dye-player-entry-%04zu.nbt", dye->save_samples);
    CHECK(n > 0 && (size_t)n < sizeof(name) && dye_player_nbt(f->app->h, name) == 0);
    ++dye->save_samples;
    return 0;
}

static bool dye_observed_frame(mc_client_menu_loop *loop) {
    fixture *f = dye == NULL ? NULL : dye->fixture;
    const bool sample = dye != NULL && dye->saving && !dye->selected && f != NULL &&
                        f->loop == loop && f->app->active_world == dye->first.world &&
                        f->app->h == dye->first.player && f->app->r != NULL &&
                        f->app->r->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
                        atomic_load(&f->app->o);
    if (sample && dye_entry(f) != 0)
        return false;
    const bool returned = mc_client_menu_loop_frame(loop);
    if (sample && returned && f->app->active_world == NULL && f->app->h == NULL &&
        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        dye->selected = true;
        dye->selected_sample = dye->save_samples - 1u;
    }
    return returned;
}

static int dye_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0 && !f->state.failed);
    return 0;
}

static int dye_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < deadline; ++i) {
        CHECK(dye_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL || !f->app->N) {
            size_t eligible = 0;
            int64_t world_time = 0;
            CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &pauses, &eligible,
                                    &world_time) == 0);
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

static int dye_choose_first(fixture *f) {
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
          transfer(f) == 0 && dye_ready(f) == 0 && reload_capture_epoch(f, &dye->first) == 0);
    return 0;
}

static int dye_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    dye->right_down = down;
    return 0;
}

static int dye_draw(fixture *f, const mc_item_stack *selected, int32_t slot, int32_t health) {
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
            held->flat_item && held->item_color_returned && held->color_set &&
            /* Original scalar normal_15: registered bh78, KM.a(15)I returns191. */
            held->item_icon_returned && held->item_icon == 191 && held->item_geometry_returned &&
            f->state.hud != NULL && f->state.hud->returned && f->state.hud->slots_returned &&
            f->state.hud->health == health && f->state.hud->selected == slot &&
            f->state.hud->item_slots == 1 && f->state.hud->empty_slots == 8)
            return 0;
        CHECK(dye_frame(f) == 0);
    }
    /* Failure-only passive terminal views. Do not advance another frame or
     * repeat a Source child to diagnose the exhausted ordinary draw wait. */
    const mc_client_world_frame_state *draw = f->state.world_frame;
    const mc_client_first_person_hand_state *hand = draw == NULL ? NULL : draw->hand;
    const mc_client_held_item_draw_state *held = hand == NULL ? NULL : hand->held_item;
    mc_client_entity_renderer_state renderer = {0};
    mc_client_held_item_state live = {0};
    const bool renderer_read = mc_client_entity_renderer_inspect(f->app->t, &renderer);
    const bool live_read = renderer_read && renderer.ra_occurrence != NULL &&
                           mc_client_held_item_inspect(renderer.ra_occurrence, &live);
    fprintf(stderr,
            "DYE_DRAW_FAILURE frame=%p frame-returned=%d pc=%u hand=%p hand-returned=%d "
            "held-returned=%d held=%p draw-returned=%d empty=%d captured=%d stack=%p owner=%p "
            "player=%p failure=%p "
            "flat=%d color-returned=%d color-set=%d icon-returned=%d icon=%" PRId32
            " geometry-returned=%d\n",
            (const void *)draw, draw != NULL && draw->returned, draw == NULL ? 0 : draw->source_pc,
            (const void *)hand, hand != NULL && hand->returned, hand != NULL && hand->held_returned,
            (const void *)held, held != NULL && held->returned, held != NULL && held->empty_hand,
            held != NULL && held->stack_captured, held == NULL ? NULL : (const void *)held->stack,
            held == NULL ? NULL : (const void *)held->owner,
            held == NULL ? NULL : (const void *)held->player,
            held == NULL ? NULL : (const void *)held->failure, held != NULL && held->flat_item,
            held != NULL && held->item_color_returned, held != NULL && held->color_set,
            held != NULL && held->item_icon_returned, held == NULL ? -1 : held->item_icon,
            held != NULL && held->item_geometry_returned);
    fprintf(stderr,
            "DYE_DRAW_FAILURE renderer-read=%d ra=%p identity=%p field-c=%p live-read=%d "
            "pending=%p app=%p expected-app=%p stack=%p expected-stack=%p slot=%" PRId32
            " expected-slot=%" PRId32 " equipped=%.9g hud=%p returned=%d slots-returned=%d "
            "health=%" PRId32 " expected-health=%" PRId32 " selected=%" PRId32
            " item-slots=%u empty-slots=%u\n",
            renderer_read, (const void *)renderer.ra_occurrence,
            renderer.ra_occurrence == NULL
                ? NULL
                : (const void *)mc_client_held_item_identity(renderer.ra_occurrence),
            (const void *)renderer.fields.c, live_read, (const void *)live.pending_ra,
            (const void *)live.ra_app, (const void *)f->app, (const void *)live.ra_b,
            (const void *)selected, live.ra_g, slot, (double)live.ra_c, (const void *)f->state.hud,
            f->state.hud != NULL && f->state.hud->returned,
            f->state.hud != NULL && f->state.hud->slots_returned,
            f->state.hud == NULL ? -1 : f->state.hud->health, health,
            f->state.hud == NULL ? -1 : f->state.hud->selected,
            f->state.hud == NULL ? 0 : f->state.hud->item_slots,
            f->state.hud == NULL ? 0 : f->state.hud->empty_slots);
    return 1;
}

static int dye_plot(fixture *f, int32_t ground, int32_t crop, int32_t *stage) {
    int32_t id = -1, above = -1, metadata = -1;
    CHECK(mc_world_runtime_block_get(f->app->active_world, 3, 64, 3, &id, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_block_get(f->app->active_world, 3, 65, 3, &above, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(f->app->active_world, 3, 65, 3, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          id == ground && above == crop && metadata >= 0 && metadata <= 7);
    if (stage != NULL)
        *stage = metadata;
    return 0;
}

static int dye_hit(fixture *f, int32_t y, int32_t id) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    int32_t actual = -1;
    CHECK(f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
          hit.fields.a == kinds.a && hit.fields.b == 3 && hit.fields.c == y && hit.fields.d == 3 &&
          mc_world_runtime_block_get(f->app->active_world, 3, y, 3, &actual, NULL) ==
              MC_WORLD_STORAGE_OK &&
          actual == id);
    return 0;
}

static mc_client_source_dk *dye_section(fixture *f) {
    mc_client_source_dk *found = NULL;
    for (mc_client_source_dk *p = f->app->g->chunk_owners; p != NULL; p = p->next)
        if (p->fields.a == f->app->active_world && p->fields.c == 0 && p->fields.d == 64 &&
            p->fields.e == 0) {
            if (found != NULL)
                return NULL;
            found = p;
        }
    return found;
}

static int dye_settle(fixture *f, mc_client_source_dk *section) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        if (section->fields.E && !section->fields.u)
            return 0;
        CHECK(dye_frame(f) == 0);
    }
    fprintf(stderr,
            "DYE_SETTLE_FAILURE section=%p compiled=%d dirty=%d world=%p "
            "expected-world=%p frame=%p frame-returned=%d\n",
            (const void *)section, section != NULL && section->fields.E,
            section != NULL && section->fields.u,
            section == NULL ? NULL : (const void *)section->fields.a,
            (const void *)f->app->active_world, (const void *)f->state.world_frame,
            f->state.world_frame != NULL && f->state.world_frame->returned);
    return 1;
}

static int dye_tap(fixture *f) {
    /* Wait for an ordinary short source frame before offering the sole tap.
     * Observe its existing P; never rewrite clocks or filter queued input. */
    const uint64_t tick_before = f->state.ticks, deadline = SDL_GetTicks() + 60000;
    int64_t age = INT64_MAX;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        int64_t now;
        CHECK(dye_frame(f) == 0 && mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        age = mc_java_i64_sub(now, f->app->P);
        if (f->state.ticks > tick_before && age <= 100)
            break;
    }
    CHECK(f->state.ticks > tick_before && age <= 100 && f->app->r == NULL && f->app->N);
    int32_t stage = -1;
    CHECK(dye_hit(f, 65, 59) == 0 && dye_plot(f, 60, 59, &stage) == 0 && stage == 0);
    const uint64_t clicks = f->state.gameplay_right_clicks, air = f->state.gameplay_air_uses,
                   blocks = f->state.gameplay_block_uses,
                   admitted = f->state.gameplay_placements_admitted,
                   used = f->state.gameplay_used_statistics,
                   counts = f->state.gameplay_stack_count_stores,
                   placed = f->state.gameplay_placed_blocks,
                   sounds = f->state.gameplay_placement_sounds,
                   clears = f->state.gameplay_selected_slot_clears,
                   equip = f->state.gameplay_equip_resets;
    const size_t events = f->state.mouse_events;
    CHECK(dye_button(f, true) == 0 && dye_button(f, false) == 0);
    bool drained = false, observed = false;
    for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(dye_frame(f) == 0);
        if (!observed && f->state.gameplay_block_uses > blocks) {
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
          f->state.gameplay_air_uses == air && f->state.gameplay_block_uses == blocks + 1 &&
          f->state.gameplay_placements_admitted == admitted &&
          f->state.gameplay_used_statistics == used + 1 &&
          f->state.gameplay_stack_count_stores == counts + 1 &&
          f->state.gameplay_placed_blocks == placed &&
          f->state.gameplay_placement_sounds == sounds &&
          f->state.gameplay_selected_slot_clears == clears &&
          f->state.gameplay_equip_resets == equip + 1);
    puts("DYE_INPUT action=bone-meal taps=1 right-clicks=1 block-uses=1 air-uses=0 "
         "count-stores=1 used=1 cleared=0 equip-reset=1 placed=0 sounds=0 "
         "child-retired=1 drained=1");
    return 0;
}

static int dye_geometry(fixture *f, mc_client_source_dk *section, int32_t stage,
                        mc_client_source_batch_view *batch, size_t *colors) {
    loaded_block_reference reference;
    CHECK(block_loaded_reference(&reference, 59, stage,
                                 stage == 0 ? "MC_DYE_CROPS_STAGE0_REFERENCE"
                                            : "MC_DYE_CROPS_STAGE7_REFERENCE",
                                 "DYE_CROPS_V1") == 0 &&
          reference.pass == 0 && reference.vertices == 48);
    const standing_block_profile profile = {"dye", "DYE", "crops", "", "", 59,  stage,
                                            6,     48,    64,      3,  60, 108, 26};
    CHECK(standing_loaded_list(f, section, &reference, &profile, batch, colors) == 0);
    return 0;
}

static int dye_run(fixture *f) {
    CHECK(repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          dye_choose_first(f) == 0 && dye_inventory(f, 2, NULL) == 0 && dye_statistic(f, 0) == 0);
    mc_entity_snapshot pose;
    mc_game_settings_state settings;
    CHECK(mc_entity_get_snapshot(f->app->h, &pose) == MC_ENTITY_OK && pose.bounds != NULL &&
          pose.bounds->min_y == 65 && pose.base.x == 3.5 && pose.base.z == .5 &&
          pose.base.y == 65.0 + (double)1.62f && pose.base.yaw == 0 && pose.base.aT == 26 &&
          mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.difficulty == 2 && f->app->active_world->fields.q == 2);
    const mc_item_stack *before_stack = NULL, *current = NULL;
    mc_client_source_dk *section = dye_section(f);
    mc_client_source_batch_view before, grown;
    size_t before_colors = 0, grown_colors = 0;
    int32_t stage = -1;
    CHECK(dye_inventory(f, 2, &before_stack) == 0 && dye_draw(f, before_stack, 0, 5) == 0 &&
          section != NULL && dye_settle(f, section) == 0 && dye_plot(f, 60, 59, &stage) == 0 &&
          stage == 0 && dye_hit(f, 65, 59) == 0 &&
          dye_geometry(f, section, 0, &before, &before_colors) == 0 &&
          dye_player_nbt(f->app->h, "dye-player-before.nbt") == 0);
    const uintptr_t before_bytes = (uintptr_t)before.bytes;
    CHECK(dye_tap(f) == 0);
    CHECK(dye_inventory(f, 1, &current) == 0);
    CHECK(current == before_stack);
    CHECK(dye_statistic(f, 1) == 0);
    CHECK(dye_plot(f, 60, 59, &stage) == 0);
    CHECK(stage == 7);
    CHECK(dye_draw(f, current, 0, 5) == 0);
    CHECK(dye_settle(f, section) == 0);
    CHECK(dye_geometry(f, section, 7, &grown, &grown_colors) == 0);
    CHECK((uintptr_t)grown.bytes != before_bytes);
    CHECK(dye_player_nbt(f->app->h, "dye-player-grown.nbt") == 0);
    char path[8192];
    CHECK(dye_path("dye-grown.png", path) == 0 && capture(f, path) == 0);
    printf("DYE_RETURN ground=60 crop=59 stage-before=0 stage-after=7 count-before=2 count-after=1 "
           "damage=15 health=5 same-stack=1 selected=0 hud=1 held=1 icon=191 used=1 "
           "target-lists-replaced=1 exact-original-position-uv-before=48 "
           "exact-original-position-uv-after=48 source-color-matches-before=%zu "
           "source-color-matches-after=%zu source-colors=48\n",
           before_colors, grown_colors);
    dye->saving = true;
    CHECK(dye_path("dye-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 1, 0);
    dye->saving = false;
    CHECK(saved == 0 && dye->selected && dye->save_samples > 0 && dye->entry_health == 5 &&
          dye->entry_ground == 60 && dye->entry_above == 59 && dye->entry_stage == 7 &&
          reload_snapshot("dye-saved") == 0);
    printf("DYE_SAVE sample=%zu candidates=%zu health=5 ground=60 crop=59 stage=7 "
           "moisture=%" PRId32 " source-save-title=1 pre-frame-owned-nbt=1\n",
           dye->selected_sample, dye->save_samples, dye->entry_moisture);
    CHECK(reload_choose(f, f->app->r, "alpha", &dye->first, &dye->reloaded, 2) == 0 &&
          dye_ready(f) == 0 && dye_inventory(f, 1, &current) == 0 &&
          dye_draw(f, current, 0, 5) == 0 && dye_statistic(f, 1) == 0 &&
          dye_plot(f, 60, 59, &stage) == 0 && stage == 7 &&
          dye_player_nbt(f->app->h, "dye-player-reloaded.nbt") == 0);
    puts("DYE_RELOAD distinct-world=1 distinct-player=1 health=5 ground=60 crop=59 stage=7 "
         "count=1 damage=15 selected=0 source-selected-slot-not-saved=1 hud=1 held=1 used=1");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    dye_work *work = calloc(1, sizeof(*work));
    if (f == NULL || work == NULL) {
        free(work);
        free(f);
        return 2;
    }
    dye = work;
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
            result = dye_run(f);
        }
    }
    work->saving = false;
    if (f->app != NULL && work->right_down)
        (void)dye_button(f, false);
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
        printf("DYE_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    dye = NULL;
    free(work);
    free(f);
    return result;
}
