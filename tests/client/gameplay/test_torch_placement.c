/* Authored Dirt/Air and Torch Stack; one SDK placement, queued light and source save/reselection.
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool torch_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame torch_observed_frame
#define main mc_torch_included_selection_main
#include "../startup/test_world_selection_loop.c"
#undef main
#undef mc_client_menu_loop_frame
#include "mc/entity/nbt/errors.h"
#include "mc/inventory/player_current.h"

typedef struct torch_work {
    fixture *fixture;
    const char *directory;
    int32_t id;
    const char *kind;
    reload_epoch first, reloaded;
    bool right_down, left_down, removal, saving, selected;
    size_t save_samples, selected_sample;
    int32_t entry_health, entry_ground, entry_above, entry_stage, entry_moisture;
    int32_t entry_lights[9];
    size_t entry_items;
} torch_work;

static torch_work *torch;

static const int32_t torch_light_cells[9][3] = {{3, 65, 3}, {4, 65, 3}, {2, 65, 3},
                                                {3, 65, 4}, {3, 65, 2}, {3, 66, 3},
                                                {3, 64, 3}, {3, 67, 3}, {4, 65, 4}};
static const int32_t torch_light_expected[9] = {14, 13, 13, 13, 13, 13, 0, 12, 12};

/* Passive Store-owned nibble reads; no acquiring query, queue mutation or drain. */
static int torch_light_read(fixture *f, int32_t values[9]) {
    mc_world_store *store = f->app->active_world->fields.w;
    mc_chunk_state *chunk = NULL;
    CHECK(store != NULL);
    for (size_t i = 0; i < mc_world_store_chunk_count(store); ++i) {
        mc_chunk_state *candidate = NULL;
        CHECK(mc_world_store_chunk_at(store, i, &candidate, NULL) == MC_WORLD_STORAGE_OK);
        if (candidate != NULL && candidate->x == 0 && candidate->z == 0) {
            CHECK(chunk == NULL);
            chunk = candidate;
        }
    }
    CHECK(chunk != NULL && !chunk->blank && chunk->data != NULL);
    for (size_t i = 0; i < 9; ++i) {
        const size_t index = ((size_t)torch_light_cells[i][0] << 11u) |
                             ((size_t)torch_light_cells[i][2] << 7u) |
                             (size_t)torch_light_cells[i][1];
        values[i] = (chunk->data->block_light[index / 2u] >> ((index & 1u) * 4u)) & 15;
    }
    return 0;
}

static int torch_path(const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", torch->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

/* Passive owned-tree serialization, outside any source save or extra frame. */
static int torch_player_nbt(mc_entity *player, const char *name) {
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
        written != bytes || torch_path(name, path) != 0 || !SDL_SaveFile(path, wire, bytes))
        goto cleanup;
    result = 0;
cleanup:
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return result;
}

static int torch_inventory(fixture *f, int32_t count, const mc_item_stack **current) {
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
                  fields.id == torch->id && fields.count == count && fields.damage == 0);
            if (current != NULL)
                *current = stack;
        } else
            CHECK(stack == NULL);
    }
    return 0;
}

static int torch_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t used = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &view, NULL) == MC_REGISTRY_OK &&
          view.E != NULL && view.E[torch->id] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, view.E[torch->id], &used, NULL) ==
              MC_CLIENT_STATS_OK &&
          used == expected);
    return 0;
}

/* Sample the exact paused entry of the frame that actually selects Save/Quit.
 * Reads and owned NBT serialization balance their native holds. */
static int torch_items(fixture *f, bool save, size_t sample, size_t *count) {
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    CHECK(list != NULL && list->count <= list->capacity && list->count <= 4096);
    size_t dirt = 0, torches = 0;
    *count = 0;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *entity = list->items[i];
        if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) ||
            entity->base.be)
            continue;
        mc_item_stack_fields fields;
        CHECK(entity->base.world == f->app->active_world && entity->stack != NULL &&
              mc_item_stack_fields_get(entity->stack, &fields) == MC_ITEM_STACK_OK &&
              fields.count == 1 && fields.damage == 0 && (fields.id == 3 || fields.id == 50));
        dirt += fields.id == 3;
        torches += fields.id == 50;
        if (save) {
            char name[112];
            const int length =
                snprintf(name, sizeof(name), "torch-item-entry-%04zu-%04zu.nbt", sample, *count);
            CHECK(length > 0 && (size_t)length < sizeof(name) &&
                  torch_player_nbt(entity, name) == 0);
        }
        ++*count;
    }
    CHECK(dirt <= 1 && torches <= 1);
    if (*count != 0)
        CHECK(dirt == 1 && torches == 1 && *count == 2);
    return 0;
}

static int torch_entry(fixture *f) {
    mc_chunk_state *chunk = NULL;
    const size_t count = mc_world_store_chunk_count(torch->first.handler);
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *candidate = NULL;
        CHECK(mc_world_store_chunk_at(torch->first.handler, i, &candidate, NULL) ==
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
    torch->entry_health = player.living.Y;
    torch->entry_ground = chunk->data->blocks[index];
    torch->entry_above = chunk->data->blocks[index + 1u];
    torch->entry_moisture = chunk->data->metadata[index / 2u] & 15;
    torch->entry_stage = chunk->data->metadata[index / 2u] >> 4u;
    CHECK(torch_light_read(f, torch->entry_lights) == 0);
    char name[96];
    const int n = snprintf(name, sizeof(name), "torch-player-entry-%04zu.nbt", torch->save_samples);
    CHECK(n > 0 && (size_t)n < sizeof(name) && torch_player_nbt(f->app->h, name) == 0);
    CHECK(torch_items(f, torch->removal, torch->save_samples, &torch->entry_items) == 0 &&
          torch->entry_items == (torch->removal ? 2u : 0u));
    ++torch->save_samples;
    return 0;
}

static bool torch_observed_frame(mc_client_menu_loop *loop) {
    fixture *f = torch == NULL ? NULL : torch->fixture;
    const bool sample = torch != NULL && torch->saving && !torch->selected && f != NULL &&
                        f->loop == loop && f->app->active_world == torch->first.world &&
                        f->app->h == torch->first.player && f->app->r != NULL &&
                        f->app->r->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
                        atomic_load(&f->app->o);
    if (sample && torch_entry(f) != 0)
        return false;
    const bool returned = mc_client_menu_loop_frame(loop);
    if (sample && returned && f->app->active_world == NULL && f->app->h == NULL &&
        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        torch->selected = true;
        torch->selected_sample = torch->save_samples - 1u;
    }
    return returned;
}

static int torch_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0 && !f->state.failed);
    return 0;
}

static int torch_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < deadline; ++i) {
        CHECK(torch_frame(f) == 0);
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

static int torch_choose_first(fixture *f) {
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
          transfer(f) == 0 && torch_ready(f) == 0 && reload_capture_epoch(f, &torch->first) == 0);
    return 0;
}

static int torch_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    torch->right_down = down;
    return 0;
}

static int torch_plot(fixture *f, int32_t ground, int32_t crop, int32_t *stage) {
    int32_t id = -1, above = -1, metadata = -1;
    CHECK(mc_world_runtime_block_get(f->app->active_world, 3, 64, 3, &id, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_block_get(f->app->active_world, 3, 65, 3, &above, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(f->app->active_world, 3, 65, 3, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          id == ground && above == crop && (metadata == (crop == 50 ? 5 : 0)));
    if (stage != NULL)
        *stage = metadata;
    return 0;
}

static int torch_hit(fixture *f, int32_t y, int32_t id) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    int32_t actual = -1;
    CHECK(f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
          hit.fields.a == kinds.a && hit.fields.b == 3 && hit.fields.c == y && hit.fields.d == 3 &&
          hit.fields.e == 1 &&
          mc_world_runtime_block_get(f->app->active_world, 3, y, 3, &actual, NULL) ==
              MC_WORLD_STORAGE_OK &&
          actual == id);
    return 0;
}

static mc_client_source_dk *torch_section(fixture *f) {
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

static int torch_settle(fixture *f, mc_client_source_dk *section) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        if (section->fields.E && !section->fields.u)
            return 0;
        CHECK(torch_frame(f) == 0);
    }
    fprintf(stderr,
            "TORCH_SETTLE_FAILURE section=%p compiled=%d dirty=%d world=%p "
            "expected-world=%p frame=%p frame-returned=%d\n",
            (const void *)section, section != NULL && section->fields.E,
            section != NULL && section->fields.u,
            section == NULL ? NULL : (const void *)section->fields.a,
            (const void *)f->app->active_world, (const void *)f->state.world_frame,
            f->state.world_frame != NULL && f->state.world_frame->returned);
    return 1;
}

static int torch_light_ready(fixture *f, const char *phase, bool present) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    int32_t values[9] = {0};
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        CHECK(torch_light_read(f, values) == 0 && f->app->active_world->fields.C != NULL);
        static const int32_t dark[9] = {0};
        if (f->app->active_world->fields.C->count == 0 && f->app->active_world->fields.M == 0 &&
            memcmp(values, present ? torch_light_expected : dark, sizeof(values)) == 0) {
            printf("TORCH_LIGHT phase=%s queue=0 target=%d east=%d west=%d south=%d north=%d "
                   "up=%d down=%d up-two=%d diagonal=%d normal-frames=1\n",
                   phase, values[0], values[1], values[2], values[3], values[4], values[5],
                   values[6], values[7], values[8]);
            return 0;
        }
        CHECK(torch_frame(f) == 0);
    }
    fprintf(stderr, "TORCH_LIGHT_FAILURE queue=%zu M=%" PRId32 " values=",
            f->app->active_world->fields.C == NULL ? 0 : f->app->active_world->fields.C->count,
            f->app->active_world->fields.M);
    for (size_t i = 0; i < 9; ++i)
        fprintf(stderr, "%s%" PRId32, i == 0 ? "" : ",", values[i]);
    fputc('\n', stderr);
    return 1;
}

static int torch_draw(fixture *f, const mc_item_stack *selected) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        const mc_client_world_frame_state *draw = f->state.world_frame;
        const mc_client_first_person_hand_state *hand = draw == NULL ? NULL : draw->hand;
        const mc_client_held_item_draw_state *held = hand == NULL ? NULL : hand->held_item;
        mc_client_entity_renderer_state renderer;
        mc_client_held_item_state live;
        const bool identity = mc_client_entity_renderer_inspect(f->app->t, &renderer) &&
                              renderer.ra_occurrence != NULL &&
                              mc_client_held_item_inspect(renderer.ra_occurrence, &live) &&
                              live.ra_app == f->app && live.ra_b == selected && live.ra_g == 0 &&
                              live.ra_c == 1.0f;
        if (draw != NULL && draw->returned && draw->source_pc == 1013 && draw->terrain_returned &&
            draw->updates_returned && draw->terrain_count > 0 && hand != NULL && hand->returned &&
            hand->held_returned && held != NULL && held->returned && !held->empty_hand &&
            held->flat_item && held->item_color_returned && held->color_set &&
            held->item_icon_returned && held->item_icon == 80 && held->item_geometry_returned &&
            identity && f->state.hud != NULL && f->state.hud->returned &&
            f->state.hud->slots_returned && f->state.hud->health == 5 &&
            f->state.hud->selected == 0 && f->state.hud->item_slots == 1 &&
            f->state.hud->empty_slots == 8)
            return 0;
        CHECK(torch_frame(f) == 0);
    }
    fprintf(stderr,
            "TORCH_DRAW_FAILURE kind=%s screen=%p focus=%d paused=%d world-draws=%" PRIu64
            " hud-draws=%" PRIu64 "\n",
            torch->kind, (void *)f->app->r, f->app->N, atomic_load(&f->app->o),
            f->state.world_draws, f->state.hud_draws);
    return 1;
}

static int torch_batch(fixture *f, mc_client_source_dk *section, const char *phase, int shape,
                       uintptr_t *identity) {
    mc_client_source_list_state definition;
    mc_client_source_batch_view batch;
    const size_t vertices = shape == 1 ? 90u : shape == 2 ? 30u : 60u;
    CHECK(section != NULL && section->fields.a == f->app->active_world && section->fields.E &&
          section->fields.o && !section->fields.u && section->fields.p != NULL &&
          !section->fields.p[0] && section->fields.p[1] &&
          mc_client_display_list_inspect(f->app->display, section->fields.C, &definition) &&
          definition.defined && !definition.building && definition.draws == 1 &&
          definition.vertices == vertices &&
          mc_client_display_list_batch(f->app->display, section->fields.C, 0, &batch) &&
          batch.mode == 4 && batch.arrays == 7 && batch.vertices == (int32_t)vertices &&
          batch.byte_count == vertices * 32u && f->state.world_frame->terrain_count > 0);
    if (identity != NULL)
        *identity = (uintptr_t)batch.bytes;
    char name[96], path[8192];
    const int n = snprintf(name, sizeof(name), "torch-geometry-%s.bin", phase);
    CHECK(n > 0 && (size_t)n < sizeof(name) && torch_path(name, path) == 0 &&
          SDL_SaveFile(path, batch.bytes, batch.byte_count));
    printf("TORCH_GEOMETRY phase=%s vertices=%zu torch-vertices=%u prefix=%u arrays=7 "
           "mode=4 source-list-returned=1 terrain-returned=1\n",
           phase, vertices, shape == 1 ? 30u : 0u, shape == 2 ? 30u : 60u);
    return 0;
}

static int torch_tap(fixture *f) {
    const uint64_t tick_before = f->state.ticks, deadline = SDL_GetTicks() + 60000;
    int64_t age = INT64_MAX;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        int64_t now;
        CHECK(torch_frame(f) == 0 &&
              mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        age = mc_java_i64_sub(now, f->app->P);
        if (f->state.ticks > tick_before && age <= 100)
            break;
    }
    CHECK(f->state.ticks > tick_before && age <= 100 && f->app->r == NULL && f->app->N &&
          torch_hit(f, 64, 3) == 0 && torch_plot(f, 3, 0, NULL) == 0);
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
    CHECK(torch_button(f, true) == 0 && torch_button(f, false) == 0);
    bool drained = false, direct_suffix = false;
    for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(torch_frame(f) == 0);
        const mc_client_gameplay_input *input = f->state.gameplay_input;
        if (input != NULL && input->block_uses != 0) {
            const mc_singleplayer_use_state *s = &input->use_state;
            CHECK(input->returned && !input->failed && input->use_call == NULL && s->item == NULL &&
                  s->returned && s->result && s->item_returned && s->placement_admitted &&
                  s->setter_returned && s->placed && s->sound_returned && s->count_stored &&
                  s->statistic_returned && s->block_id == torch->id && s->metadata == 0 &&
                  s->count_before == 2 && s->count_after == 1 && s->x == 3 && s->y == 65 &&
                  s->z == 3 && s->face == 1);
            direct_suffix = true;
        }
        mc_client_sdk_input_state input_state;
        mc_client_sdk_bytes_state read, queue, buttons;
        CHECK(
            mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input_state) &&
            mc_client_sdk_bytes_inspect(input_state.mouse_read, &read) &&
            mc_client_sdk_queue_inspect(input_state.mouse_events, &queue) &&
            mc_client_sdk_bytes_inspect(input_state.buttons, &buttons) && buttons.limit > 1 &&
            buttons.data[1] == 0);
        drained = f->state.mouse_events >= events + 2 && read.position == read.limit &&
                  queue.position == 0 && input_state.pending_native_event == NULL;
        if (drained && f->state.gameplay_placed_blocks > placed)
            break;
    }
    CHECK(drained && f->state.gameplay_right_clicks == clicks + 1 &&
          f->state.gameplay_air_uses == air && f->state.gameplay_block_uses == blocks + 1 &&
          f->state.gameplay_placements_admitted == admitted + 1 &&
          f->state.gameplay_used_statistics == used + 1 &&
          f->state.gameplay_stack_count_stores == counts + 1 &&
          f->state.gameplay_placed_blocks == placed + 1 &&
          f->state.gameplay_placement_sounds == sounds + 1 &&
          f->state.gameplay_selected_slot_clears == clears &&
          f->state.gameplay_equip_resets == equip + 1);
    /* Sound/count/statistic completion follows both placed hooks in CK. Only
     * value diagnostics survive native child retirement; no hook is replayed. */
    printf("TORCH_INPUT kind=%s taps=1 right-clicks=1 block-uses=1 air-uses=0 admitted=1 "
           "placed=1 sounds=1 count-stores=1 used=1 cleared=0 equip-reset=1 drained=1 "
           "placed-hooks=source-control-flow onAdded-before-face=source-control-flow "
           "direct-suffix=%d metadata=0 target=3,65,3 face=1\n",
           torch->kind, direct_suffix);
    return 0;
}

static int torch_left(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    torch->left_down = down;
    return 0;
}

static bool torch_ray_support(fixture *f) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    return f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
           mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
           hit.fields.a == kinds.a && hit.fields.b == 3 && hit.fields.c == 64 &&
           hit.fields.d == 3 && hit.fields.e == 1;
}

static int torch_remove_support(fixture *f, mc_client_source_dk *section) {
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.mouse_sensitivity == .5f && !settings.values.invert_mouse && f->app->N &&
          f->app->r == NULL);
    const size_t events = f->state.mouse_events;
    SDL_Event event = {0};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.motion.xrel = 24;
    event.motion.yrel = 0;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    bool aimed = false;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        CHECK(torch_frame(f) == 0);
        if (f->state.mouse_events > events && torch_ray_support(f)) {
            aimed = true;
            break;
        }
    }
    if (!aimed)
        fprintf(stderr,
                "TORCH_AIM_FAILURE one-motion=1 xrel=24 yrel=0 sensitivity=.5 "
                "yaw=%.9g pitch=%.9g events=%zu,%zu\n",
                f->app->h->base.yaw, f->app->h->base.aT, events, f->state.mouse_events);
    CHECK(aimed && f->app->h->base.yaw == 3.6f && f->app->h->base.aT == 26 &&
          torch_hit(f, 64, 3) == 0);
    const uint64_t mined = f->state.gameplay_mined_blocks;
    const uint64_t starts = f->state.gameplay_mining_starts;
    const uint64_t progress = f->state.gameplay_mining_progress_calls;
    CHECK(torch_left(f, true) == 0);
    const uint64_t mining_deadline = SDL_GetTicks() + 180000;
    int32_t ground = 3, above = 50;
    for (size_t i = 0; i < 240 && SDL_GetTicks() < mining_deadline; ++i) {
        CHECK(torch_frame(f) == 0 &&
              mc_world_runtime_block_get(f->app->active_world, 3, 64, 3, &ground, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_runtime_block_get(f->app->active_world, 3, 65, 3, &above, NULL) ==
                  MC_WORLD_STORAGE_OK);
        if (ground == 0) {
            CHECK(torch_left(f, false) == 0);
            break;
        }
        CHECK(ground == 3 && above == 50);
    }
    bool drained = false;
    for (size_t i = 0; i < 64 && !torch->left_down && SDL_GetTicks() < mining_deadline; ++i) {
        CHECK(torch_frame(f) == 0);
        mc_client_sdk_input_state input;
        mc_client_sdk_bytes_state read, queue, buttons;
        CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
              mc_client_sdk_bytes_inspect(input.mouse_read, &read) &&
              mc_client_sdk_queue_inspect(input.mouse_events, &queue) &&
              mc_client_sdk_bytes_inspect(input.buttons, &buttons) && buttons.limit > 0 &&
              buttons.data[0] == 0);
        drained = read.position == read.limit && queue.position == 0 &&
                  input.pending_native_event == NULL;
        if (drained)
            break;
    }
    CHECK(drained && !torch->left_down && ground == 0 && above == 0 &&
          f->state.gameplay_mined_blocks == mined + 1 && f->state.gameplay_mining_starts > starts &&
          f->state.gameplay_mining_progress_calls > progress && torch_statistic(f, 1) == 0 &&
          torch_light_ready(f, "removed", false) == 0 && torch_settle(f, section) == 0 &&
          torch_batch(f, section, "removed", 2, NULL) == 0);
    size_t drops;
    CHECK(torch_items(f, false, 0, &drops) == 0 && drops == 2 && torch_inventory(f, 1, NULL) == 0);
    mc_registry_statistic_view statistics;
    int32_t mined_statistic = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.C != NULL && statistics.C[3] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.C[3], &mined_statistic,
                                       NULL) == MC_CLIENT_STATS_OK &&
          mined_statistic == 1);
    printf("TORCH_REMOVAL motion=1 xrel=24 yrel=0 sensitivity=0.5 ray=3,64,3 face=1 "
           "mined=1 support=0 torch=0 dirt-drops=1 torch-drops=1 count=1 used=1 "
           "light=0 queue=0 mined-statistic=1 aim-repair=0\n");
    return 0;
}

static int torch_run(fixture *f) {
    CHECK(repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          torch_choose_first(f) == 0 && torch_inventory(f, 2, NULL) == 0 &&
          torch_statistic(f, 0) == 0 && torch_plot(f, 3, 0, NULL) == 0);
    const mc_block *block = NULL;
    mc_block_render_description description;
    int32_t initial_light[9];
    CHECK(mc_registry_block_get(f->app->registry, 50, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && mc_block_render_describe(block, &description) == MC_REGISTRY_OK &&
          description.type == 2 && torch_light_read(f, initial_light) == 0);
    for (size_t i = 0; i < 9; ++i)
        CHECK(initial_light[i] == 0);
    mc_entity_snapshot pose;
    mc_game_settings_state settings;
    CHECK(mc_entity_get_snapshot(f->app->h, &pose) == MC_ENTITY_OK && pose.bounds != NULL &&
          pose.bounds->min_y == 65 && pose.base.x == 3.5 && pose.base.z == .5 &&
          pose.base.y == 65.0 + (double)1.62f && pose.base.yaw == 0 && pose.base.aT == 26 &&
          mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.difficulty == 2 && f->app->active_world->fields.q == 2);
    const mc_item_stack *before_stack = NULL, *current = NULL;
    mc_client_source_dk *section = torch_section(f);
    uintptr_t before_bytes, placed_bytes;
    CHECK(torch_inventory(f, 2, &before_stack) == 0 && torch_draw(f, before_stack) == 0 &&
          section != NULL && torch_settle(f, section) == 0 &&
          torch_batch(f, section, "before", false, &before_bytes) == 0 &&
          torch_player_nbt(f->app->h, "torch-player-before.nbt") == 0 && torch_tap(f) == 0 &&
          torch_inventory(f, 1, &current) == 0 && current == before_stack &&
          torch_statistic(f, 1) == 0);
    int32_t stage;
    CHECK(torch_plot(f, 3, torch->id, &stage) == 0 && stage == 5 &&
          torch_light_ready(f, "placed", true) == 0 && torch_draw(f, current) == 0 &&
          torch_settle(f, section) == 0 &&
          torch_batch(f, section, "placed", true, &placed_bytes) == 0 &&
          placed_bytes != before_bytes &&
          torch_player_nbt(f->app->h, "torch-player-placed.nbt") == 0);
    const uint64_t draws = f->state.world_draws, hud = f->state.hud_draws;
    for (unsigned int i = 0; i < 3; ++i)
        CHECK(torch_frame(f) == 0 && torch_draw(f, current) == 0 &&
              torch_plot(f, 3, torch->id, &stage) == 0);
    CHECK(f->state.world_draws >= draws + 3 && f->state.hud_draws >= hud + 3);
    char path[8192];
    CHECK(torch_path("torch-placed.png", path) == 0 && capture(f, path) == 0);
    printf("TORCH_RETURN kind=%s ground=3 id=%" PRId32 " stage=%" PRId32
           " count-before=2 count-after=1 damage=0 health=5 same-stack=1 selected=0 "
           "hud=1 held=1 continuing=3 used=1 list-replaced=1\n",
           torch->kind, torch->id, stage);
    if (torch->removal)
        CHECK(torch_remove_support(f, section) == 0 && torch_draw(f, current) == 0);
    torch->saving = true;
    CHECK(torch_path("torch-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 1, 0);
    torch->saving = false;
    static const int32_t dark[9] = {0};
    CHECK(saved == 0 && torch->selected && torch->save_samples > 0 && torch->entry_health == 5 &&
          torch->entry_ground == (torch->removal ? 0 : 3) &&
          torch->entry_above == (torch->removal ? 0 : 50) &&
          torch->entry_stage == (torch->removal ? 0 : 5) &&
          memcmp(torch->entry_lights, torch->removal ? dark : torch_light_expected,
                 sizeof(torch->entry_lights)) == 0 &&
          reload_snapshot("torch-saved") == 0);
    printf("TORCH_SAVE sample=%zu candidates=%zu health=5 ground=%" PRId32 " id=%" PRId32
           " stage=%" PRId32 " source-save-title=1 pre-frame-owned-nbt=1 blocklight=%d items=%zu\n",
           torch->selected_sample, torch->save_samples, torch->entry_ground, torch->entry_above,
           torch->entry_stage, torch->removal ? 0 : 14, torch->entry_items);
    CHECK(reload_choose(f, f->app->r, "alpha", &torch->first, &torch->reloaded, 2) == 0 &&
          torch_ready(f) == 0 && torch_inventory(f, 1, &current) == 0 &&
          torch_light_ready(f, "reloaded", !torch->removal) == 0 && torch_draw(f, current) == 0 &&
          torch_statistic(f, 1) == 0 &&
          torch_plot(f, torch->removal ? 0 : 3, torch->removal ? 0 : 50, &stage) == 0 &&
          stage == torch->entry_stage &&
          torch_player_nbt(f->app->h, "torch-player-reloaded.nbt") == 0);
    section = torch_section(f);
    CHECK(section != NULL && torch_settle(f, section) == 0 &&
          torch_batch(f, section, "reloaded", torch->removal ? 2 : 1, NULL) == 0);
    size_t reloaded_items;
    CHECK(torch_items(f, false, 0, &reloaded_items) == 0 &&
          reloaded_items == (torch->removal ? 2u : 0u));
    printf("TORCH_RELOAD distinct-world=1 distinct-player=1 health=5 ground=%d count=1 damage=0 "
           "selected=0 hud=1 held=1 used=1 stage=%" PRId32 " items=%zu\n",
           torch->removal ? 0 : 3, stage, reloaded_items);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || (strcmp(argv[2], "placement") != 0 && strcmp(argv[2], "support-removal") != 0))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    torch_work *work = calloc(1, sizeof(*work));
    if (f == NULL || work == NULL) {
        free(work);
        free(f);
        return 2;
    }
    torch = work;
    work->fixture = f;
    work->directory = argv[3];
    work->removal = strcmp(argv[2], "support-removal") == 0;
    work->kind = "torch";
    work->id = 50;
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
            result = torch_run(f);
        }
    }
    work->saving = false;
    if (f->app != NULL && work->right_down)
        (void)torch_button(f, false);
    if (f->app != NULL && work->left_down)
        (void)torch_left(f, false);
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
        printf("TORCH_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    torch = NULL;
    free(work);
    free(f);
    return result;
}
