/* Pre-bootstrap Grass/Item355 inputs; one ordinary SDK placement and daytime
 * activation, real returned terrain/HUD, direct source full save. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#define main mc_bed_placement_included_selection_main
#include "tests/client/startup/test_world_selection_loop.c"
#undef main
#include "client/gui/gui_ingame_chat_internal.h"
#include "entity/player/player_internal.h"
#include "mc/entity/nbt/errors.h"
#include "mc/entity/player/coordinates.h"
#include "mc/inventory/player_current.h"
#include "mc/world/runtime/saving/saving.h"

typedef struct bed_placement_work {
    const char *directory, *phase;
    uint64_t deadline;
    bool right_down, left_down;
} bed_placement_work;

static bed_placement_work *bed_scene;

static int bed_frame(fixture *f) {
    CHECK(SDL_GetTicks() < bed_scene->deadline);
    const int result = frame(f);
    if (result != 0 || f->state.failed) {
        native_focus_receipt(f, bed_scene->phase);
        fprintf(stderr, "BED_PLACEMENT_FAILURE phase=%s source=%s.%s:%" PRId64 " operation=%s\n",
                bed_scene->phase, f->state.source_class == NULL ? "none" : f->state.source_class,
                f->state.source_method == NULL ? "none" : f->state.source_method,
                f->state.source_pc, f->state.operation == NULL ? "none" : f->state.operation);
    }
    CHECK(result == 0 && !f->state.failed);
    return 0;
}

static int bed_button(fixture *f, bool right, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = right ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    if (right)
        bed_scene->right_down = down;
    else
        bed_scene->left_down = down;
    return 0;
}

/* No focus request, window raising, Pause recovery or source field repair. */
static int bed_native_focus(fixture *f) {
    bool active = false;
    mc_client_sdk_update_error error;
    const bool read = mc_client_sdk_display_active(mc_client_display_update_owner(f->app->display),
                                                   &active, &error) == MC_CLIENT_SDK_UPDATE_OK;
    const bool focused =
        read && active && SDL_GetKeyboardFocus() == mc_client_display_window(f->app->display);
    if (!focused)
        native_focus_receipt(f, bed_scene->phase);
    CHECK(focused);
    return 0;
}

static int bed_recent(fixture *f, uint64_t ticks_before, uint64_t tick_gap) {
    for (size_t i = 0; i < 96 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        int64_t now;
        CHECK(bed_frame(f) == 0 && f->app->r == NULL && !atomic_load(&f->app->o) &&
              bed_native_focus(f) == 0 &&
              mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        if (f->state.ticks >= ticks_before + tick_gap && mc_java_i64_sub(now, f->app->P) <= 100)
            return 0;
    }
    return 1;
}

static int bed_ready(fixture *f) {
    for (size_t i = 0; i < 180 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(bed_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        CHECK(f->app->r == NULL && !atomic_load(&f->app->o) && bed_native_focus(f) == 0);
        if (!f->app->N) {
            const uint64_t ticks_before = f->state.ticks;
            const uint64_t left = f->state.gameplay_left_clicks,
                           right = f->state.gameplay_right_clicks;
            CHECK(bed_recent(f, ticks_before, 1) == 0 && bed_button(f, false, true) == 0 &&
                  bed_button(f, false, false) == 0);
            for (size_t j = 0; j < 64 && !f->app->N && SDL_GetTicks() < bed_scene->deadline; ++j)
                CHECK(bed_frame(f) == 0 && f->app->r == NULL && !atomic_load(&f->app->o));
            CHECK(f->app->N && f->state.gameplay_left_clicks == left &&
                  f->state.gameplay_right_clicks == right);
            puts("BED_PLACEMENT_ACQUIRE source-k442=1 SDK-left-tap=1 gameplay-clicks=0");
        }
        const mc_client_world_frame_state *draw = f->state.world_frame;
        if (f->app->h != NULL && f->app->i == f->app->h && draw != NULL && draw->returned &&
            draw->terrain_returned && draw->updates_returned && draw->source_pc == 1013 &&
            f->state.hud != NULL && f->state.hud->returned)
            return 0;
    }
    return 1;
}

static int bed_choose(fixture *f) {
    mc_client_source_rq *rq = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          bed_native_focus(f) == 0 && open_selection(f, f->app->r, &rq) == 0 &&
          release_mouse(f, rq) == 0);
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
    for (size_t i = 0; i < 64 && rq->fields.m != selected && SDL_GetTicks() < bed_scene->deadline;
         ++i)
        CHECK(bed_frame(f) == 0);
    CHECK(rq->fields.m == selected && release_mouse(f, rq) == 0 &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0 && bed_ready(f) == 0);
    return 0;
}

static mc_chunk_state *bed_chunk(fixture *f) {
    mc_world_store *store = f->app->active_world->fields.w;
    mc_chunk_state *found = NULL;
    for (size_t i = 0; i < mc_world_store_chunk_count(store); ++i) {
        mc_chunk_state *chunk = NULL;
        if (mc_world_store_chunk_at(store, i, &chunk, NULL) != MC_WORLD_STORAGE_OK)
            return NULL;
        if (chunk != NULL && chunk->x == 0 && chunk->z == 0) {
            if (found != NULL)
                return NULL;
            found = chunk;
        }
    }
    return found != NULL && !found->blank && found->data != NULL ? found : NULL;
}

static bool bed_cells(fixture *f, bool placed) {
    mc_chunk_state *chunk = bed_chunk(f);
    if (chunk == NULL)
        return false;
    for (int32_t z = 2; z <= 3; ++z) {
        const size_t index = ((size_t)z << 7u) | 64u;
        const int32_t metadata = (chunk->data->metadata[index >> 1u] >> ((index & 1u) * 4u)) & 15;
        if (chunk->data->blocks[index - 1u] != 2 ||
            chunk->data->blocks[index] != (placed ? 26 : 0) ||
            metadata != (placed && z == 3 ? 8 : 0))
            return false;
    }
    return true;
}

static int bed_player(fixture *f, int32_t count, int32_t used, const mc_item_stack **selected) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    mc_registry_statistic_view statistics;
    int32_t value = -1;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          player.living.Y == 20 && !player.player.u && player.player.private_a == 0 &&
          player.player.private_b == NULL && player.player.v == NULL &&
          mc_inventory_player_read_state(player.player.c, &inventory) && inventory.selected == 0 &&
          mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.E != NULL && statistics.E[355] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.E[355], &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          value == used);
    for (int32_t i = 0; i < 36; ++i) {
        const mc_item_stack *stack = NULL;
        CHECK(mc_inventory_player_borrow(player.player.c, i, &stack, NULL) == MC_INVENTORY_OK);
        if (i == 0) {
            mc_item_stack_fields fields;
            CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                  fields.id == 355 && fields.count == count && fields.damage == 0);
            if (selected != NULL)
                *selected = stack;
        } else
            CHECK(stack == NULL);
    }
    return 0;
}

static int bed_hit(fixture *f, bool placed) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    CHECK(f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
          hit.fields.a == kinds.a && hit.fields.b == 0 && hit.fields.c == (placed ? 64 : 63) &&
          hit.fields.d == 2 && hit.fields.e == (placed ? 2 : 1));
    return 0;
}

static bool bed_pose_same(const mc_entity_snapshot *a, const mc_entity_bounds *bounds,
                          const mc_entity_snapshot *b) {
    return a->base.x == b->base.x && a->base.y == b->base.y && a->base.z == b->base.z &&
           a->base.yaw == b->base.yaw && a->base.aT == b->base.aT &&
           a->base.width == b->base.width && a->base.height == b->base.height &&
           a->base.offset == b->base.offset && a->bounds == b->bounds && b->bounds != NULL &&
           memcmp(bounds, b->bounds, sizeof(*bounds)) == 0;
}

static int bed_tap(fixture *f, bool placement) {
    bed_scene->phase = placement ? "place" : "daytime";
    CHECK(bed_recent(f, f->state.ticks, 5) == 0 && repeat_focus_state(f, bed_scene->phase) == 0 &&
          bed_hit(f, !placement) == 0 && bed_cells(f, !placement));
    mc_entity_snapshot before, after;
    CHECK(mc_entity_get_snapshot(f->app->h, &before) == MC_ENTITY_OK && before.bounds != NULL);
    const mc_entity_bounds bounds = *before.bounds;
    const uint64_t clicks = f->state.gameplay_right_clicks, blocks = f->state.gameplay_block_uses,
                   air = f->state.gameplay_air_uses, used = f->state.gameplay_used_statistics,
                   counts = f->state.gameplay_stack_count_stores,
                   placed = f->state.gameplay_placed_blocks;
    const size_t events = f->state.mouse_events;
    CHECK(bed_button(f, true, true) == 0 && bed_button(f, true, false) == 0);
    bool drained = false, source_returned = false;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(bed_frame(f) == 0 && repeat_focus_state(f, bed_scene->phase) == 0);
        const mc_client_gameplay_input *input = f->state.gameplay_input;
        if (input != NULL && input->block_uses != 0) {
            const mc_singleplayer_use_state *use = &input->use_state;
            CHECK(input->returned && !input->failed && input->use_call == NULL && use->returned &&
                  use->result && use->activation_entered && use->activation_returned);
            if (placement) {
                CHECK(use->item_returned && use->setter_returned && use->placed &&
                      use->count_stored && use->statistic_returned && use->block_id == 26 &&
                      use->count_before == 2 && use->count_after == 1 && use->x == 0 &&
                      use->y == 64 && use->z == 2 && use->face == 1);
            } else {
                CHECK(use->activated && !use->item_entered && !use->count_stored &&
                      !use->statistic_returned && use->clicked_id == 26 &&
                      input->player_message_calls == 1 && input->player_message_returns == 1 &&
                      input->message_call == NULL && input->message_state.returned &&
                      !input->message_state.base_noop &&
                      input->message_state.receiver_identity == (uintptr_t)f->app->h &&
                      input->message_state.app_identity == (uintptr_t)f->app &&
                      input->message_state.uq_identity == (uintptr_t)f->app->v);
            }
            source_returned = true;
        }
        /* Totals are published only after the complete input call returns.
         * Another tick in this frame can replace its borrowed child view. */
        if (f->state.gameplay_block_uses == blocks + 1)
            source_returned = true;
        mc_client_sdk_input_state state;
        mc_client_sdk_bytes_state read, queue, buttons;
        CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &state) &&
              mc_client_sdk_bytes_inspect(state.mouse_read, &read) &&
              mc_client_sdk_queue_inspect(state.mouse_events, &queue) &&
              mc_client_sdk_bytes_inspect(state.buttons, &buttons) && buttons.limit > 1 &&
              buttons.data[0] == 0 && buttons.data[1] == 0);
        drained = f->state.mouse_events >= events + 2 && read.position == read.limit &&
                  queue.position == 0 && state.pending_native_event == NULL;
        if (drained && f->state.gameplay_block_uses > blocks)
            break;
    }
    CHECK(drained && source_returned && bed_cells(f, true) &&
          f->state.gameplay_right_clicks == clicks + 1 &&
          f->state.gameplay_block_uses == blocks + 1 && f->state.gameplay_air_uses == air &&
          f->state.gameplay_used_statistics == used + (placement ? 1u : 0u) &&
          f->state.gameplay_stack_count_stores == counts + (placement ? 1u : 0u) &&
          f->state.gameplay_placed_blocks == placed + (placement ? 1u : 0u) &&
          mc_entity_get_snapshot(f->app->h, &after) == MC_ENTITY_OK &&
          (placement || bed_pose_same(&before, &bounds, &after)));
    printf("BED_PLACEMENT_INPUT action=%s SDK-right-tap=1 source-returned=1 no-air-fallback=1 "
           "drained=1 count=%d used=1 pose-bounds-unchanged=%d\n",
           placement ? "place" : "daytime", 1, !placement);
    return 0;
}

static int bed_path(const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", bed_scene->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

static int bed_player_nbt(fixture *f, const char *name) {
    int result = 1;
    mc_entity_nbt_error *error = calloc(1, sizeof(*error));
    mc_nbt_tree *tree = NULL;
    uint8_t *wire = NULL;
    char path[8192];
    if (error == NULL || mc_entity_build_nbt(f->app->h, NULL, &tree, error) != MC_ENTITY_NBT_OK)
        goto cleanup;
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    size_t bytes = 0, written = 0;
    mc_nbt_error failure;
    if (mc_nbt_measure(mc_nbt_tree_root(tree), &limits, &bytes, &failure) != MC_NBT_OK ||
        (wire = malloc(bytes)) == NULL ||
        mc_nbt_write(mc_nbt_tree_root(tree), &limits, wire, bytes, &written, &failure) !=
            MC_NBT_OK ||
        written != bytes || bed_path(name, path) != 0 || !SDL_SaveFile(path, wire, bytes))
        goto cleanup;
    result = 0;
cleanup:
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return result;
}

static int bed_draw(fixture *f, const char *phase) {
    bed_scene->phase = phase;
    for (size_t i = 0; i < 96 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(bed_frame(f) == 0 && repeat_focus_state(f, phase) == 0);
        const mc_client_world_frame_state *draw = f->state.world_frame;
        const mc_client_gui_ingame_draw_state *hud = f->state.hud;
        if (draw != NULL && draw->returned && draw->terrain_returned && draw->updates_returned &&
            draw->source_pc == 1013 && draw->terrain_count > 0 && hud != NULL && hud->returned &&
            hud->hotbar_returned && hud->slots_returned && hud->chat_returned &&
            hud->health == 20 && hud->selected == 0 && hud->item_slots == 1 &&
            hud->empty_slots == 8 &&
            (strcmp(phase, "notice") != 0 || (hud->chat_entries == 1 && hud->chat_draws == 1))) {
            char name[96], path[8192];
            const int n = snprintf(name, sizeof(name), "bed-%s.png", phase);
            CHECK(n > 0 && (size_t)n < sizeof(name) && bed_path(name, path) == 0 &&
                  capture(f, path) == 0);
            printf("BED_PLACEMENT_FRAME phase=%s terrain=1 HUD=1 health=20 selected=0 item-slots=1 "
                   "chat=%u\n",
                   phase, hud->chat_entries);
            return 0;
        }
    }
    return 1;
}

static int bed_notice(fixture *f) {
    mc_client_source_uq *hud = f->app->v;
    CHECK(hud != NULL && hud->fields.e != NULL && hud->fields.e->size == 1 &&
          hud->fields.e->entries != NULL);
    mc_gui_chat_message *message = mc_gui_chat_message_find(hud, hud->fields.e->entries[0]);
    mc_player_string_view text;
    CHECK(message != NULL && mc_gui_chat_text_read(message->a, &text) && text.length > 0 &&
          text.length <= 320 && message->b >= 0 && message->b < 200);
    printf("BED_PLACEMENT_NOTICE key=tile.bed.noSleep age=%" PRId32 " utf16=", message->b);
    for (size_t i = 0; i < text.length; ++i)
        printf("%04x", (unsigned int)text.units[i]);
    putchar('\n');
    return 0;
}

static int bed_run(fixture *f) {
    bed_scene->phase = "loading";
    CHECK(repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          bed_choose(f) == 0);
    const mc_item_stack *before = NULL, *placed = NULL, *notice = NULL;
    mc_entity_snapshot pose;
    CHECK(bed_player(f, 2, 0, &before) == 0 && bed_cells(f, false) &&
          mc_entity_get_snapshot(f->app->h, &pose) == MC_ENTITY_OK && pose.bounds != NULL &&
          pose.base.x == .5 && pose.base.z == .5 && pose.bounds->min_y == 64 &&
          pose.base.yaw == 0 && pose.base.aT == 41 && f->app->active_world->fields.f < 4 &&
          f->app->v->fields.e != NULL && f->app->v->fields.e->size == 0);
    puts("BED_PLACEMENT_INPUTS chunks=289 grass-y=63 Bed=absent item=355 count=2 health=20 "
         "selected=0 saved-spawn=null feet=64 x=.5 z=.5 yaw=0 pitch=41 pre-bootstrap-only=1");
    CHECK(bed_draw(f, "before") == 0 && bed_hit(f, false) == 0 && bed_tap(f, true) == 0 &&
          bed_player(f, 1, 1, &placed) == 0 && placed == before && bed_draw(f, "placed") == 0 &&
          bed_player_nbt(f, "bed-player-placed.nbt") == 0 && bed_tap(f, false) == 0 &&
          bed_player(f, 1, 1, &notice) == 0 && notice == before && bed_notice(f) == 0 &&
          bed_draw(f, "notice") == 0 && bed_player_nbt(f, "bed-player-notice.nbt") == 0);
    puts("BED_PLACEMENT_RETURN foot=0,64,2:26:0 head=0,64,3:26:8 count=2,1,1 used=1 "
         "sleeping=0 timer=0 Q=null same-stack=1 pose-bounds-unchanged=1");
    CHECK(mc_client_menu_loop_retire_completed_draws(f->loop));
    mc_world_runtime_lifecycle_error *saving = calloc(1, sizeof(*saving));
    CHECK(saving != NULL);
    const mc_chunk_lifecycle_status saved =
        mc_world_runtime_save_detailed(f->app->active_world, true, saving);
    if (saved != MC_CHUNK_LIFECYCLE_OK)
        fprintf(stderr, "BED_PLACEMENT_SAVE_FAILURE status=%d operation=%s\n", (int)saved,
                saving->cause.operation == NULL ? "none" : saving->cause.operation);
    free(saving);
    CHECK(saved == MC_CHUNK_LIFECYCLE_OK);
    puts("BED_PLACEMENT_SAVE source-full-save=1 live-player=1 SDK-save-menu=0 reload=0 crafting=0 "
         "night=0");
    CHECK(mc_client_menu_loop_terminal_prepare(f->loop));
    return 0;
}

#ifndef MC_BED_PLACEMENT_ENTRY
#define MC_BED_PLACEMENT_ENTRY main
#endif
int MC_BED_PLACEMENT_ENTRY(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "place-daytime") != 0)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    bed_placement_work work = {
        .directory = argv[3], .phase = "bootstrap", .deadline = SDL_GetTicks() + 210000};
    if (f == NULL)
        return 2;
    bed_scene = &work;
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
            result = bed_run(f);
        }
    }
    /* Cleanup has a separate budget after scene work ends.
     * The process timeout includes additional headroom. */
    const uint64_t cleanup_deadline = SDL_GetTicks() + 20000;
    if (result != 0 && f->app != NULL)
        native_focus_receipt(f, work.phase);
    if (f->app != NULL && work.right_down)
        (void)bed_button(f, true, false);
    if (f->app != NULL && work.left_down)
        (void)bed_button(f, false, false);
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < cleanup_deadline);
    if (closed != MC_CLIENT_BOOTSTRAP_OK || f->bootstrap != NULL || f->loop != NULL ||
        frame_id == 0 || canvas_id == 0 || SDL_GetWindowFromID(frame_id) != NULL ||
        SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("BED_PLACEMENT_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    bed_scene = NULL;
    free(f);
    return result;
}
