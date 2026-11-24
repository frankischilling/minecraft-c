/* A real saved Leaf and Shears, source Mouse input, harvest and native cleanup. */
int mc_leaf_unused_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_leaf_unused_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

#include "client/display/sdk_update.h"
#include "entity/player/player_internal.h"
#include "mc/inventory/player_current.h"
#include "mc/world/chunk/object.h"

static int leaf_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0);
    CHECK(!f->state.failed && f->state.running && f->state.clocks.frame_returned);
    return 0;
}

static int leaf_resume(fixture *f) {
    mc_client_source_da *recovered[4] = {0};
    size_t focus_pauses = 0, eligible = 0;
    int64_t time = 0;
    CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &focus_pauses, &eligible,
                            &time) == 0);
    return 0;
}

static int leaf_choose(fixture *f) {
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
        CHECK(leaf_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL || !f->app->N) {
            CHECK(leaf_resume(f) == 0);
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
    fprintf(stderr, "Leaf SDK setup did not reach its genuine picked target\n");
    return 1;
}

typedef struct leaf_mouse {
    mc_client_sdk_input_state input;
    mc_client_sdk_bytes_state read, queue;
    bool release_unread;
} leaf_mouse;

static bool leaf_release_record(const uint8_t *bytes, uint64_t timestamp) {
    if (timestamp == 0 || bytes[0] != 0 || bytes[1] != 0)
        return false;
    uint64_t nanos = 0;
    for (size_t i = 14; i < 22; ++i)
        nanos = (nanos << 8) | bytes[i];
    return nanos == timestamp;
}

static int leaf_mouse_read(fixture *f, uint64_t release, leaf_mouse *out) {
    *out = (leaf_mouse){0};
    CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &out->input) &&
          !out->input.active && mc_client_sdk_bytes_inspect(out->input.mouse_read, &out->read) &&
          mc_client_sdk_queue_inspect(out->input.mouse_events, &out->queue) &&
          out->read.position <= out->read.limit && out->read.limit <= out->read.capacity &&
          out->read.position % 22 == 0 && out->read.limit % 22 == 0 &&
          out->queue.position <= out->queue.limit && out->queue.limit <= out->queue.capacity &&
          out->queue.position % 22 == 0);
    for (size_t i = out->read.position; i < out->read.limit; i += 22)
        out->release_unread |= leaf_release_record(out->read.data + i, release);
    return 0;
}

static int leaf_target(fixture *f, int32_t *out) {
    CHECK(mc_world_runtime_block_get(f->app->active_world, 0, 64, 2, out, NULL) ==
          MC_WORLD_STORAGE_OK);
    return 0;
}

static int leaf_focus(fixture *f) {
    SDL_Window *window = mc_client_display_window(f->app->display);
    CHECK(window != NULL && SDL_GetKeyboardFocus() == window &&
          (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0 && f->app->N &&
          f->app->r == NULL);
#ifdef _WIN32
    DWORD process = 0;
    HWND foreground = GetForegroundWindow();
    CHECK(foreground != NULL && GetWindowThreadProcessId(foreground, &process) != 0 &&
          process == GetCurrentProcessId());
#endif
    return 0;
}

static int leaf_click(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (unsigned int attempt = 0; attempt < 12 && SDL_GetTicks() < deadline; ++attempt) {
        if (f->app->r != NULL || !f->app->N)
            CHECK(leaf_resume(f) == 0);
        leaf_mouse queue;
        CHECK(leaf_mouse_read(f, 0, &queue) == 0);
        while ((queue.read.position != queue.read.limit || queue.queue.position != 0) &&
               SDL_GetTicks() < deadline) {
            CHECK(leaf_frame(f) == 0);
            if (f->app->r != NULL || !f->app->N)
                CHECK(leaf_resume(f) == 0);
            CHECK(leaf_mouse_read(f, 0, &queue) == 0);
        }
        CHECK(queue.read.position == queue.read.limit && queue.queue.position == 0);
        int32_t before;
        CHECK(leaf_target(f, &before) == 0 && before == 18);
        uint64_t timestamps[2];
        for (unsigned int i = 0; i < 2; ++i) {
            SDL_Event event = {0};
            timestamps[i] = SDL_GetTicksNS();
            event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            event.common.timestamp = timestamps[i];
            event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
            event.button.button = SDL_BUTTON_LEFT;
            event.button.down = i == 0;
            CHECK(SDL_PushEvent(&event));
        }
        CHECK(transfer(f) == 0);
        const uint64_t clicks = f->state.gameplay_left_clicks;
        size_t frames = 0;
        bool released = false;
        while (SDL_GetTicks() < deadline) {
            leaf_mouse previous, after;
            CHECK(leaf_mouse_read(f, timestamps[1], &previous) == 0);
            const size_t events = f->state.mouse_events;
            const size_t unread = (previous.read.limit - previous.read.position) / 22;
            CHECK(leaf_frame(f) == 0 && leaf_mouse_read(f, timestamps[1], &after) == 0);
            ++frames;
            released |= previous.release_unread && f->state.mouse_events - events >= unread;
            released |= after.input.current_mouse.button == 0 &&
                        !after.input.current_mouse.pressed &&
                        (uint64_t)after.input.current_mouse.nanos == timestamps[1];
            int32_t target;
            CHECK(leaf_target(f, &target) == 0);
            printf("LEAF_SDK_INPUT attempt=%u frames=%zu target=%" PRId32 " clicks=%" PRIu64
                   " mined=%" PRIu64 " release-consumed=%d read=%zu/%zu"
                   " queued=%zu focus=%d screen=%d\n",
                   attempt + 1, frames, target, f->state.gameplay_left_clicks,
                   f->state.gameplay_mined_blocks, (int)released, after.read.position,
                   after.read.limit, after.queue.position, (int)f->app->N,
                   f->app->r == NULL ? -1 : (int)f->app->r->kind);
            CHECK(fflush(stdout) == 0);
            if (target == 0) {
                CHECK(f->state.gameplay_left_clicks > clicks &&
                      f->state.gameplay_mined_blocks == 1 && leaf_focus(f) == 0);
                return 0;
            }
            CHECK(target == 18);
            if (f->app->r != NULL || !f->app->N) {
                CHECK(leaf_resume(f) == 0);
                break;
            }
            if (released)
                break;
        }
        /* Retry only after this exact timestamped release was consumed by
         * the ordinary Source Mouse.next loop. Motion cannot authorize it. */
        CHECK(released);
    }
    fprintf(stderr, "Leaf SDK input did not harvest within 12 consumed taps/60s\n");
    return 1;
}

static int leaf_stack(fixture *f, int32_t expected_damage) {
    mc_local_player_snapshot player;
    const mc_item_stack *held = NULL;
    mc_item_stack_fields fields;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_inventory_player_current(player.player.c, &held, NULL) == MC_INVENTORY_OK &&
          held != NULL && mc_item_stack_fields_get(held, &fields) == MC_ITEM_STACK_OK &&
          fields.id == 359 && fields.count == 1 && fields.damage == expected_damage);
    return 0;
}

static int leaf_item(fixture *f, mc_entity **out) {
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
        if (stack.id != 18)
            continue;
        CHECK(stack.count == 1 && stack.damage == 2 && entity->base.world == world &&
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

static bool leaf_drawn(const mc_client_render_global_draw_receipt *draw, const mc_entity *item) {
    return draw->valid && draw->returned && draw->retired &&
           draw->actor_identity == (uintptr_t)item &&
           draw->actor_kind == MC_ENTITY_CLASS_KIND_ITEM && draw->primary_returned &&
           draw->post_draw_returned && draw->living_source_pc == 796 &&
           draw->renderer_class_name != NULL && strcmp(draw->renderer_class_name, "bb") == 0;
}

static int leaf_draw(fixture *f, mc_entity *item) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        const mc_client_render_global_entities_state *entities =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->entities;
        if (entities != NULL && entities->returned && entities->item_draws_returned > 0 &&
            (leaf_drawn(&entities->first_item_draw, item) ||
             leaf_drawn(&entities->first_entity_draw, item) ||
             leaf_drawn(&entities->last_draw, item)))
            return leaf_focus(f);
        CHECK(leaf_frame(f) == 0 && f->app->r == NULL && f->app->N);
        mc_entity *current = NULL;
        CHECK(leaf_item(f, &current) == 0 && current == item);
    } while (SDL_GetTicks() < deadline);
    fprintf(stderr, "Leaf Item did not reach a returned and retired actual BB draw\n");
    return 1;
}

static int leaf_run(fixture *f, mining_work *work) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && leaf_choose(f) == 0);
    int32_t target, metadata;
    CHECK(leaf_target(f, &target) == 0 && target == 18 &&
          mc_world_runtime_metadata_get(f->app->active_world, 0, 64, 2, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          (metadata & 3) == 2 && leaf_stack(f, 0) == 0 &&
          mining_capture(f, work, "leaf-before.png") == 0);
    mc_registry_statistic_view stats;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &stats, NULL) ==
              MC_REGISTRY_OK &&
          stats.C != NULL && stats.E != NULL && stats.F != NULL && stats.C[18] == NULL &&
          stats.E[359] != NULL && stats.F[359] != NULL);
    int32_t used_before, broken_before;
    CHECK(mc_client_stats_writer_value(f->app->stats_writer, stats.E[359], &used_before, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app->stats_writer, stats.F[359], &broken_before, NULL) ==
              MC_CLIENT_STATS_OK);
    const uint64_t clicks = f->state.gameplay_left_clicks, starts = f->state.gameplay_mining_starts;
    CHECK(leaf_click(f) == 0 && leaf_stack(f, 1) == 0 && f->state.gameplay_left_clicks > clicks &&
          f->state.gameplay_mining_starts > starts);
    int32_t used_after, broken_after;
    mc_entity *item = NULL;
    CHECK(mc_client_stats_writer_value(f->app->stats_writer, stats.E[359], &used_after, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app->stats_writer, stats.F[359], &broken_after, NULL) ==
              MC_CLIENT_STATS_OK &&
          used_after == used_before && broken_after == broken_before && leaf_item(f, &item) == 0 &&
          leaf_draw(f, item) == 0 && mining_semantic(item, work, "leaf-item.nbt") == 0 &&
          mining_semantic(f->app->h, work, "leaf-player.nbt") == 0);
    mc_render_global_auxiliary_observation auxiliary;
    CHECK(mc_render_global_access_auxiliary_observe(f->app->g, &auxiliary) &&
          !auxiliary.overflowed && auxiliary.event2001_entered == 1 &&
          auxiliary.event2001_returned == 1 && auxiliary.destruction_constructed == 64 &&
          auxiliary.destruction_appended == 64 && auxiliary.last.block_id == 18 &&
          (auxiliary.last.metadata & 3) == 2 && auxiliary.last.sound_before_destruction &&
          auxiliary.last.source_returned && auxiliary.last.native_cleanup_returned);
    CHECK(mining_capture(f, work, "leaf-after.png") == 0);
    printf("LEAF_SDK_HARVEST target-air=1 actual-clicks=%" PRIu64 " mining-starts=%" PRIu64
           " mined=1 shears-id=359 count=1 damage=1 used-stat=0 broken-stat=0 leaf-stat-null=1 "
           "drop-id=18 drop-count=1 drop-damage=2 world-occurrences=1 chunk-occurrences=1 focus=1 "
           "item-draw=1 event2001=1 destruction-particles=64\n",
           f->state.gameplay_left_clicks - clicks, f->state.gameplay_mining_starts - starts);
    CHECK(fflush(stdout) == 0);
    return 0;
}

static int leaf_stop(fixture *f, uint32_t frame_id, const char *snapshot) {
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

static int leaf_quit(fixture *f, uint32_t frame_id) {
    return leaf_stop(f, frame_id, "leaf-stopped");
}

#ifndef MC_LEAF_FIXTURE_MAIN
#define MC_LEAF_FIXTURE_MAIN main
#endif
int MC_LEAF_FIXTURE_MAIN(int argc, char **argv) {
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
    int result = leaf_run(f, work);
    if (leaf_quit(f, frame_id) != 0)
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
    if (cleaned && reload_snapshot("leaf-after-native-close") != 0)
        result = 1;
    printf("LEAF_SDK_FINISH cleanup=%d windows-absent=%d checks=%u\n", (int)cleaned, (int)cleaned,
           checks);
    if (cleaned) {
        free(work);
        free(f);
    }
    return cleaned ? result : 1;
}
