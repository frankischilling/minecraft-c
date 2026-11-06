/* A saved Sand block, source mining, actual Item drawing, pickup and Inventory. */
int mc_sand_unused_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_sand_unused_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

#include "client/display/sdk_update.h"
#include "client/gui/source_inventory_drawing.h"
#include "client/gui/source_inventory_screen_internal.h"
#include "entity/player/player_internal.h"
#include "mc/inventory/player_current.h"
#include "mc/world/chunk/object.h"

static int sand_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0);
    CHECK(!f->state.failed && f->state.running && f->state.clocks.frame_returned);
    return 0;
}

static int sand_resume(fixture *f) {
    mc_client_source_da *recovered[4] = {0};
    size_t focus_pauses = 0, eligible = 0;
    int64_t time = 0;
    CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &focus_pauses, &eligible,
                            &time) == 0);
    return 0;
}

static int sand_choose(fixture *f) {
    mc_client_source_rq *rq = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &rq) == 0 && release_mouse(f, rq) == 0);
    int32_t index = -1;
    for (size_t i = 0; i < mc_client_save_list_size(rq->fields.n); ++i)
        if (filename_is(mc_client_save_list_at(rq->fields.n, i), "alpha")) {
            CHECK(index == -1);
            index = (int32_t)i;
        }
    CHECK(index >= 0);
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + index * 36 + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    uint64_t deadline = SDL_GetTicks() + 5000;
    while (rq->fields.m != index && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.m == index && release_mouse(f, rq) == 0 &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0);
    deadline = SDL_GetTicks() + 180000;
    while (SDL_GetTicks() < deadline) {
        CHECK(sand_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL || !f->app->N) {
            CHECK(sand_resume(f) == 0);
            continue;
        }
        mc_entity_hit_result_view hit;
        mc_entity_hit_kind_class_view kinds;
        if (f->app->y != NULL &&
            mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
            mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
            hit.fields.a == kinds.a && hit.fields.b == 0 && hit.fields.c == 64 &&
            hit.fields.d == 2 && f->state.world_frame != NULL && f->state.world_frame->returned &&
            f->state.world_frame->selection_calls != 0) {
            reload_epoch epoch;
            CHECK(reload_capture_epoch(f, &epoch) == 0);
            return 0;
        }
    }
    fprintf(stderr, "Sand SDK setup did not reach its genuine picked target\n");
    return 1;
}

static int sand_target(fixture *f, int32_t *out) {
    CHECK(mc_world_runtime_block_get(f->app->active_world, 0, 64, 2, out, NULL) ==
          MC_WORLD_STORAGE_OK);
    return 0;
}

static void sand_focus_receipt(fixture *f, const char *stage, SDL_Window *window,
                               SDL_Window *keyboard, SDL_WindowFlags flags) {
    /* Failure-only observations. Never pump events, restore focus or retry
     * source work while identifying an environmental or scene-state failure. */
    native_focus_receipt(f, stage);
    fprintf(stderr,
            "SAND_FOCUS_FAILURE stage=%s window-present=%d keyboard-same=%d sdl-input-focus=%d "
            "window-flags=%" PRIx64 " window-id=%" PRIu32 " keyboard-id=%" PRIu32
            " source-focus=%d source-screen-null=%d screen=%d paused=%d frames=%" PRIu64
            " ticks=%" PRIu64 " source=%s.%s:%" PRId64 " operation=%s\n",
            stage, window != NULL, keyboard == window, (flags & SDL_WINDOW_INPUT_FOCUS) != 0,
            (uint64_t)flags, window == NULL ? 0u : SDL_GetWindowID(window),
            keyboard == NULL ? 0u : SDL_GetWindowID(keyboard), f->app->N, f->app->r == NULL,
            f->app->r == NULL ? -1 : (int)f->app->r->kind, atomic_load(&f->app->o), f->state.frames,
            f->state.ticks, f->state.source_class == NULL ? "none" : f->state.source_class,
            f->state.source_method == NULL ? "none" : f->state.source_method, f->state.source_pc,
            f->state.operation == NULL ? "none" : f->state.operation);
#ifdef _WIN32
    const HWND foreground = GetForegroundWindow();
    DWORD process = 0;
    const DWORD thread = foreground == NULL ? 0 : GetWindowThreadProcessId(foreground, &process);
    fprintf(stderr,
            "SAND_FOREGROUND_STATE stage=%s foreground-present=%d thread-read=%d same-pid=%d "
            "foreground-pid=%lu current-pid=%lu\n",
            stage, foreground != NULL, thread != 0, process == GetCurrentProcessId(),
            (unsigned long)process, (unsigned long)GetCurrentProcessId());
#endif
}

static int sand_focus(fixture *f) {
    SDL_Window *window = mc_client_display_window(f->app->display);
    SDL_Window *keyboard = SDL_GetKeyboardFocus();
    const SDL_WindowFlags flags = window == NULL ? 0 : SDL_GetWindowFlags(window);
    const bool focused = window != NULL && keyboard == window &&
                         (flags & SDL_WINDOW_INPUT_FOCUS) != 0 && f->app->N && f->app->r == NULL;
    if (!focused)
        sand_focus_receipt(f, "sand-window-source", window, keyboard, flags);
    CHECK(focused);
#ifdef _WIN32
    DWORD process = 0;
    HWND foreground = GetForegroundWindow();
    const DWORD thread = foreground == NULL ? 0 : GetWindowThreadProcessId(foreground, &process);
    const bool foreground_owned =
        foreground != NULL && thread != 0 && process == GetCurrentProcessId();
    if (!foreground_owned) {
        fprintf(stderr,
                "SAND_FOREGROUND_GATE foreground-present=%d thread-read=%d same-pid=%d "
                "foreground-pid=%lu current-pid=%lu\n",
                foreground != NULL, thread != 0, process == GetCurrentProcessId(),
                (unsigned long)process, (unsigned long)GetCurrentProcessId());
        sand_focus_receipt(f, "sand-foreground-pid", window, keyboard, flags);
    }
    CHECK(foreground_owned);
#endif
    return 0;
}

static int sand_click(fixture *f) {
    int32_t target;
    CHECK(sand_focus(f) == 0 && sand_target(f, &target) == 0 && target == 12 &&
          mining_button(f, true) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 240 && SDL_GetTicks() < deadline; ++i) {
        CHECK(sand_frame(f) == 0 && sand_focus(f) == 0 && sand_target(f, &target) == 0);
        if (target == 0) {
            CHECK(mining_button(f, false) == 0);
            return 0;
        }
        CHECK(target == 12);
    }
    (void)mining_button(f, false);
    fprintf(stderr, "Sand held source input did not finish within 240 frames/60s\n");
    return 1;
}

static int sand_stack(fixture *f, int32_t expected_count) {
    const mc_item_stack *stack = NULL;
    CHECK(mc_inventory_player_current(f->app->h->local_player->fields.player.c, &stack, NULL) ==
          MC_INVENTORY_OK);
    if (expected_count == 0) {
        CHECK(stack == NULL);
    } else {
        mc_item_stack_fields fields;
        CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == 12 && fields.count == expected_count && fields.damage == 0);
    }
    return 0;
}

static int sand_item(fixture *f, mc_entity **out) {
    mc_world_runtime *world = f->app->active_world;
    const mc_world_reference_list *list = world->fields.b;
    CHECK(list != NULL && list->count <= list->capacity && list->count <= 4096);
    size_t count = 0;
    *out = NULL;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *entity = list->items[i];
        if (entity == NULL || entity->base.be ||
            !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
            continue;
        mc_item_stack_fields stack;
        CHECK(entity->stack != NULL &&
              mc_item_stack_fields_get(entity->stack, &stack) == MC_ITEM_STACK_OK);
        if (stack.id != 12)
            continue;
        CHECK(stack.count == 1 && stack.damage == 0 && entity->base.world == world &&
              entity->phase == MC_ENTITY_READY && entity->base.bF && entity->item.c >= 0 &&
              entity->item.c <= 10);
        ++count;
        *out = entity;
    }
    CHECK(count == 1);
    mc_entity *drop = *out;
    size_t world_occurrences = 0;
    for (size_t i = 0; i < list->count; ++i)
        world_occurrences += list->items[i] == drop ? 1u : 0u;
    mc_chunk_state *state = NULL;
    CHECK(world_occurrences == 1 &&
          mc_chunk_cache_lookup(world->acquisition.cache, drop->base.bG, drop->base.bI, &state));
    mc_world_chunk *chunk = mc_world_store_chunk_object(world->fields.w, state);
    mc_world_chunk_fields fields;
    const mc_world_chunk_entity_list *slice = NULL;
    size_t slice_count = 0, slice_occurrences = 0;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
          drop->base.bH >= 0 && drop->base.bH < 8 &&
          mc_world_chunk_slice_at(fields.m, (size_t)drop->base.bH, &slice) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_entity_count(slice, &slice_count) == MC_WORLD_STORAGE_OK &&
          slice_count <= 4096);
    for (size_t i = 0; i < slice_count; ++i) {
        mc_entity *entry = NULL;
        CHECK(mc_world_chunk_entity_at(slice, i, &entry) == MC_WORLD_STORAGE_OK);
        slice_occurrences += entry == drop ? 1u : 0u;
    }
    CHECK(slice_occurrences == 1);
    return 0;
}

static bool sand_drawn(const mc_client_render_global_draw_receipt *draw, const mc_entity *item) {
    return draw->valid && draw->returned && draw->retired &&
           draw->actor_identity == (uintptr_t)item &&
           draw->actor_kind == MC_ENTITY_CLASS_KIND_ITEM && draw->primary_returned &&
           draw->post_draw_returned && draw->living_source_pc == 796 &&
           draw->renderer_class_name != NULL && strcmp(draw->renderer_class_name, "bb") == 0;
}

static int sand_draw(fixture *f, mc_entity *item) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        const mc_client_render_global_entities_state *entities =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->entities;
        if (entities != NULL && entities->returned && entities->item_draws_returned > 0 &&
            (sand_drawn(&entities->first_item_draw, item) ||
             sand_drawn(&entities->first_entity_draw, item) ||
             sand_drawn(&entities->last_draw, item)))
            return sand_focus(f);
        CHECK(sand_frame(f) == 0 && f->app->r == NULL && f->app->N);
        mc_entity *current = NULL;
        CHECK(sand_item(f, &current) == 0 && current == item);
    } while (SDL_GetTicks() < deadline);
    fprintf(stderr, "Sand Item did not reach a returned and retired actual BB draw\n");
    return 1;
}

static int sand_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view stats;
    int32_t actual;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &stats, NULL) ==
              MC_REGISTRY_OK &&
          stats.C != NULL && stats.C[12] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, stats.C[12], &actual, NULL) ==
              MC_CLIENT_STATS_OK &&
          actual == expected);
    return 0;
}

static int sand_pickup(fixture *f) {
    CHECK(sand_stack(f, 0) == 0 && gameplay_key(f, SDL_SCANCODE_W, true) == 0 && transfer(f) == 0);
    const double before_z = f->app->h->base.z;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    bool picked = false;
    for (size_t i = 0; i < 240 && SDL_GetTicks() < deadline; ++i) {
        CHECK(sand_frame(f) == 0 && sand_focus(f) == 0);
        const mc_item_stack *stack = NULL;
        CHECK(mc_inventory_player_current(f->app->h->local_player->fields.player.c, &stack, NULL) ==
              MC_INVENTORY_OK);
        if (stack != NULL) {
            picked = true;
            break;
        }
    }
    CHECK(gameplay_key(f, SDL_SCANCODE_W, false) == 0 && transfer(f) == 0 && picked &&
          f->app->h->base.z > before_z && sand_stack(f, 1) == 0);
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    CHECK(list != NULL && list->count <= list->capacity);
    for (size_t i = 0; i < list->count; ++i) {
        const mc_entity *entry = list->items[i];
        if (entry == NULL || entry->base.be || !mc_entity_is_kind(entry, MC_ENTITY_CLASS_KIND_ITEM))
            continue;
        mc_item_stack_fields fields;
        CHECK(entry->stack != NULL &&
              mc_item_stack_fields_get(entry->stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id != 12);
    }
    return 0;
}

static int sand_held(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    size_t frames = 0;
    do {
        CHECK(sand_frame(f) == 0 && sand_focus(f) == 0 && sand_stack(f, 1) == 0);
        const mc_client_first_person_hand_state *hand =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->hand;
        if (hand != NULL && hand->returned && hand->held_returned && hand->held_item != NULL &&
            hand->held_item->returned && hand->held_item->stack_captured &&
            !hand->held_item->empty_hand && hand->held_item->faces == 6 &&
            hand->held_item->vertices == 24 && f->state.hud != NULL &&
            f->state.hud->slots_returned && f->state.hud->empty_slots == 8)
            ++frames;
        if (frames >= 3)
            return 0;
    } while (SDL_GetTicks() < deadline);
    return 1;
}

static int sand_inventory(fixture *f, mining_work *work) {
    CHECK(gameplay_key(f, SDL_SCANCODE_E, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_E, false) == 0 && transfer(f) == 0);
    uint64_t deadline = SDL_GetTicks() + 15000;
    mc_client_source_ue *screen = NULL;
    do {
        CHECK(sand_frame(f) == 0);
        if (f->app->r == NULL || f->app->r->kind != MC_CLIENT_SCREEN_INVENTORY)
            continue;
        const mc_client_inventory_draw_state *draw = f->state.inventory;
        if (draw != NULL && draw->receiver == (mc_client_source_ue *)f->app->r && draw->returned &&
            draw->id_returned && draw->background_returned && draw->preview_returned &&
            draw->pose_restored && draw->foreground_returned && draw->slots_drawn == 45 &&
            draw->item_draws == 45 && !draw->cursor_drawn) {
            screen = (mc_client_source_ue *)f->app->r;
            break;
        }
    } while (SDL_GetTicks() < deadline);
    if (screen == NULL) {
        const mc_client_inventory_draw_state *draw = f->state.inventory;
        fprintf(stderr,
                "SAND_SDK_INVENTORY_WAIT screen=%d focus=%d state=%d returned=%d id=%d "
                "background=%d preview=%d pose=%d foreground=%d slots=%zu bb-calls=%zu cursor=%d\n",
                f->app->r == NULL ? -1 : (int)f->app->r->kind, (int)f->app->N, (int)(draw != NULL),
                (int)(draw != NULL && draw->returned), (int)(draw != NULL && draw->id_returned),
                (int)(draw != NULL && draw->background_returned),
                (int)(draw != NULL && draw->preview_returned),
                (int)(draw != NULL && draw->pose_restored),
                (int)(draw != NULL && draw->foreground_returned),
                draw == NULL ? 0 : draw->slots_drawn, draw == NULL ? 0 : draw->item_draws,
                (int)(draw != NULL && draw->cursor_drawn));
        CHECK(mining_capture(f, work, "sand-inventory-observed.png") == 0);
    }
    CHECK(screen != NULL && sand_stack(f, 1) == 0 &&
          screen->container.id.j == f->app->h->local_player->fields.player.d &&
          f->app->h->local_player->fields.player.e == screen->container.id.j &&
          mining_capture(f, work, "sand-inventory.png") == 0);
    /* All actual GP.e()I receivers, including PV/YV, inherit return -1.
     * ID therefore invokes BB once for every slot, including NULL Stacks. */
    puts("SAND_SDK_INVENTORY slots=45 bb-calls=45 cursor=0 same-container=1");
    CHECK(gameplay_key(f, SDL_SCANCODE_E, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_E, false) == 0 && transfer(f) == 0);
    deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(sand_frame(f) == 0);
        if (f->app->r == NULL && f->app->N)
            return sand_focus(f);
    } while (SDL_GetTicks() < deadline);
    return 1;
}

static int sand_run(fixture *f, mining_work *work) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && sand_choose(f) == 0);
    int32_t target;
    CHECK(sand_target(f, &target) == 0 && target == 12 && sand_stack(f, 0) == 0 &&
          sand_statistic(f, 0) == 0 && mining_capture(f, work, "sand-before.png") == 0);
    const uint64_t clicks = f->state.gameplay_left_clicks, starts = f->state.gameplay_mining_starts;
    CHECK(sand_click(f) == 0 && sand_statistic(f, 1) == 0 &&
          f->state.gameplay_left_clicks > clicks && f->state.gameplay_mining_starts > starts &&
          f->state.gameplay_mining_progress_calls > 0 && f->state.gameplay_mined_blocks == 1);
    mc_entity *item = NULL;
    CHECK(sand_item(f, &item) == 0 && sand_draw(f, item) == 0 &&
          mining_semantic(item, work, "sand-item.nbt") == 0);
    mc_render_global_auxiliary_observation auxiliary;
    CHECK(mc_render_global_access_auxiliary_observe(f->app->g, &auxiliary) &&
          !auxiliary.overflowed && auxiliary.event2001_entered == 1 &&
          auxiliary.event2001_returned == 1 && auxiliary.destruction_constructed == 64 &&
          auxiliary.destruction_appended == 64 && auxiliary.last.block_id == 12 &&
          auxiliary.last.sound_before_destruction && auxiliary.last.source_returned &&
          auxiliary.last.native_cleanup_returned);
    CHECK(mining_capture(f, work, "sand-drop.png") == 0 && sand_pickup(f) == 0 &&
          sand_held(f) == 0 && mining_capture(f, work, "sand-held.png") == 0 &&
          sand_inventory(f, work) == 0 &&
          mining_semantic(f->app->h, work, "sand-player.nbt") == 0 && sand_statistic(f, 1) == 0);
    puts("SAND_SDK_HARVEST target-air=1 source-input=1 mined=1 block=12 drop-count=1 damage=0 "
         "same-world-chunk=1 item-draw=1 particles=64 statistic=1 actual-movement=1 pickup=1 "
         "live-items=0 held-frames=3 inventory-draw=1 inventory-count=1");
    CHECK(fflush(stdout) == 0);
    return 0;
}

static int sand_stop(fixture *f, uint32_t frame_id, const char *snapshot) {
    SDL_Event event = {0};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = frame_id;
    CHECK(SDL_PushEvent(&event));
    mc_client_sdk_update_error update;
    for (unsigned int i = 0; i < 100 && atomic_load(&f->app->J); ++i) {
        CHECK(
            mc_client_sdk_display_process_messages(mc_client_display_update_owner(f->app->display),
                                                   &update) == MC_CLIENT_SDK_UPDATE_OK);
        SDL_Delay(2);
    }
    CHECK(!atomic_load(&f->app->J));
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
    if (!f->state.failed)
        CHECK(mc_client_menu_loop_frame(f->loop));
    CHECK(reload_snapshot(snapshot) == 0);
    return 0;
}

static int sand_quit(fixture *f, uint32_t frame_id) {
    return sand_stop(f, frame_id, "sand-stopped");
}

#ifndef MC_SAND_FIXTURE_MAIN
#define MC_SAND_FIXTURE_MAIN main
#endif
int MC_SAND_FIXTURE_MAIN(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    mining_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    mining_fixture = f;
    work->directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    CHECK(mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f->app = mc_client_bootstrap_app(f->bootstrap);
    mc_client_frame_state host;
    CHECK(f->app != NULL &&
          mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
          host.frame_window != NULL && host.canvas_window != NULL);
    const uint32_t frame_id = SDL_GetWindowID(host.frame_window);
    const uint32_t canvas_id = SDL_GetWindowID(host.canvas_window);
    int result = sand_run(f, work);
    if (sand_quit(f, frame_id) != 0)
        result = 1;
    if (!mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    const bool cleaned = closed == MC_CLIENT_BOOTSTRAP_OK && f->bootstrap == NULL &&
                         f->loop == NULL && SDL_GetWindowFromID(frame_id) == NULL &&
                         SDL_GetWindowFromID(canvas_id) == NULL;
    if (cleaned && reload_snapshot("sand-after-native-close") != 0)
        result = 1;
    printf("SAND_SDK_FINISH cleanup=%d windows-absent=%d checks=%u\n", (int)cleaned, (int)cleaned,
           checks);
    if (cleaned) {
        free(work);
        free(f);
    }
    return cleaned ? result : 1;
}
