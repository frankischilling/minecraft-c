/* Continue the ordinary mining/pickup path through a real Q drop and save. */
int mc_drop_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_drop_included_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

typedef struct drop_work {
    mining_work mining;
    reload_epoch epoch4;
    size_t empty_frames, item_frames, save_samples, saved_items;
    bool saving, sampled, save_selected, q_down;
} drop_work;

static drop_work *dropping;

static int drop_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view statistics;
    int32_t value;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.named != NULL && statistics.named['v' - 'f'] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.named['v' - 'f'], &value,
                                       NULL) == MC_CLIENT_STATS_OK &&
          value == expected);
    return 0;
}

static int drop_frame(fixture *f, drop_work *work) {
    CHECK(mining_frame(f, &work->mining) == 0);
    int32_t carried;
    size_t items;
    mc_entity *item = NULL;
    CHECK(mining_current(f->app->h, &carried) == 0 &&
          mining_items(f->app->active_world, &items, &item) == 0);
    const mc_client_world_frame_state *world = f->state.world_frame;
    const mc_client_first_person_hand_state *hand = world->hand;
    if (carried == 0 && items == 1 && f->state.hud->slots_returned &&
        f->state.hud->empty_slots == 9 && hand != NULL && hand->returned && hand->held_returned &&
        hand->held_item != NULL && hand->held_item->returned && hand->held_item->empty_hand)
        ++work->empty_frames;
    const mc_client_render_global_entities_state *entities = world->entities;
    if (entities != NULL && entities->returned && entities->item_draws_returned > 0 &&
        (mining_dirt_draw(&entities->first_item_draw, item) ||
         mining_dirt_draw(&entities->first_entity_draw, item) ||
         mining_dirt_draw(&entities->last_draw, item))) {
        ++work->item_frames;
        if (work->item_frames == 3)
            CHECK(mining_capture(f, &work->mining, "q-dropped.png") == 0);
    }
    return 0;
}

static int drop_q(fixture *f, drop_work *work, bool nonempty) {
    const uint64_t calls = f->state.gameplay_drop_calls;
    const uint64_t inserted = f->state.gameplay_dropped_items;
    const size_t events = f->state.keyboard_events;
    CHECK(save_title_focus(f) == 0 && gameplay_key(f, SDL_SCANCODE_Q, true) == 0 &&
          transfer(f) == 0);
    work->q_down = true;
    CHECK(gameplay_key(f, SDL_SCANCODE_Q, false) == 0 && transfer(f) == 0);
    work->q_down = false;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    do {
        CHECK(drop_frame(f, work) == 0);
        ++frames;
    } while (f->state.keyboard_events < events + 2 && frames < 64 && SDL_GetTicks() < deadline);
    CHECK(f->state.keyboard_events >= events + 2 && f->state.gameplay_drop_calls == calls + 1 &&
          f->state.gameplay_dropped_items == inserted + (nonempty ? 1u : 0u));
    int32_t carried;
    size_t items;
    mc_entity *item;
    CHECK(mining_current(f->app->h, &carried) == 0 && carried == 0 &&
          mining_items(f->app->active_world, &items, &item) == 0 && items == 1);
    printf("DROP_INPUT nonempty=%d calls=1 inserted=%d inventory-empty=1 live-dirt=1 frames=%zu\n",
           (int)nonempty, (int)nonempty, frames);
    CHECK(fflush(stdout) == 0);
    return 0;
}

static int drop_save_before(fixture *f) {
    drop_work *work = dropping;
    work->sampled = false;
    if (!work->saving || f->app->r == NULL || f->app->r->kind != MC_CLIENT_SCREEN_PAUSE ||
        f->app->N || !atomic_load(&f->app->o))
        return 0;
    CHECK(f->app->active_world == work->mining.epochs[2].world &&
          f->app->h == work->mining.epochs[2].player &&
          mining_semantic(f->app->h, &work->mining, "drop-player-entry.nbt") == 0);
    size_t items;
    mc_entity *item;
    CHECK(mining_items(f->app->active_world, &items, &item) == 0 && items == 1 &&
          mining_semantic(item, &work->mining, "drop-item-entry.nbt") == 0);
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    size_t count = 0;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *entity = list->items[i];
        if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) ||
            entity->base.be)
            continue;
        char name[64];
        const int n = snprintf(name, sizeof(name), "drop-items-%04zu.nbt", count);
        CHECK(count < 128 && n > 0 && (size_t)n < sizeof(name) &&
              mining_semantic(entity, &work->mining, name) == 0);
        ++count;
    }
    char path[8192], text[32];
    const int n = snprintf(text, sizeof(text), "%zu\n", count);
    CHECK(n > 0 && (size_t)n < sizeof(text) &&
          mining_path(&work->mining, "drop-items.count", path) == 0 &&
          SDL_SaveFile(path, text, (size_t)n));
    /* The save leaves unloaded Chunks on disk. Record the actual loaded set so
     * the decoder compares its Items with live state and all other Items with
     * the exact disk snapshot preceding this selected save frame. */
    mc_chunk_provider_fields provider;
    CHECK(mc_chunk_provider_bind_fields(f->app->active_world->fields.v, &provider, NULL) ==
              MC_WORLD_STORAGE_OK &&
          provider.cache != NULL && provider.cache->insertion_count <= 4096 &&
          mining_path(&work->mining, "drop-loaded-chunks.txt", path) == 0);
    SDL_IOStream *loaded = SDL_IOFromFile(path, "wb");
    CHECK(loaded != NULL);
    for (size_t i = 0; i < provider.cache->insertion_count; ++i) {
        const mc_chunk_state *chunk = provider.cache->insertions[i];
        CHECK(chunk != NULL && !chunk->blank && !chunk->never_save);
        char coordinate[64];
        const int bytes = snprintf(coordinate, sizeof(coordinate), "%" PRId32 " %" PRId32 "\n",
                                   chunk->x, chunk->z);
        CHECK(bytes > 0 && (size_t)bytes < sizeof(coordinate) &&
              SDL_WriteIO(loaded, coordinate, (size_t)bytes) == (size_t)bytes);
    }
    CHECK(SDL_CloseIO(loaded));
    work->saved_items = count;
    work->sampled = true;
    ++work->save_samples;
    return 0;
}

static void drop_save_after(fixture *f, bool returned) {
    if (dropping->saving && dropping->sampled && returned && f->app->active_world == NULL &&
        f->app->h == NULL && f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU)
        dropping->save_selected = true;
}

static int drop_run(fixture *f, drop_work *work) {
    CHECK(mining_run(f, &work->mining) == 0 && drop_statistic(f, 0) == 0 &&
          drop_q(f, work, true) == 0 && drop_statistic(f, 1) == 0 && drop_q(f, work, false) == 0 &&
          drop_statistic(f, 1) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    while ((work->empty_frames < 3 || work->item_frames < 3) && frames < 128 &&
           SDL_GetTicks() < deadline) {
        CHECK(drop_frame(f, work) == 0);
        ++frames;
    }
    CHECK(work->empty_frames >= 3 && work->item_frames >= 3);
    printf("DROP_RETURN empty-frames=%zu item-frames=%zu source-drop-statistic=1\n",
           work->empty_frames, work->item_frames);
    mining_extra_before_frame = drop_save_before;
    mining_extra_after_frame = drop_save_after;
    work->saving = true;
    char path[8192];
    CHECK(mining_path(&work->mining, "drop-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 3, 3);
    work->saving = false;
    CHECK(saved == 0 && work->save_selected && work->save_samples > 0 &&
          reload_snapshot("saved-3") == 0 &&
          mining_choose(f, &work->mining, &work->mining.epochs[2], &work->epoch4) == 0);
    int32_t id, count;
    size_t items;
    mc_entity *item;
    CHECK(mining_target(f->app->active_world, &id) == 0 && id == 0 &&
          mining_current(f->app->h, &count) == 0 && count == 0 &&
          mining_items(f->app->active_world, &items, &item) == 0 && items == 1 &&
          mining_semantic(f->app->h, &work->mining, "drop-player-reloaded.nbt") == 0 &&
          mining_semantic(item, &work->mining, "drop-item-reloaded.nbt") == 0 &&
          mining_capture(f, &work->mining, "drop-reloaded.png") == 0 && drop_statistic(f, 1) == 0);
    puts("DROP_RELOAD epochs=4 inventory-empty=1 dirt-items=1 ordinary-save-title=1");
    printf("DROP_SAVE_ENTRY samples=%zu all-items=%zu paused=1 owned-nbt=1\n", work->save_samples,
           work->saved_items);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    drop_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    mining_fixture = f;
    dropping = work;
    work->mining.directory = argv[2];
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
            result = drop_run(f, work);
        }
    }
    mining_extra_before_frame = NULL;
    mining_extra_after_frame = NULL;
    if (f->app != NULL && work->mining.left_down)
        (void)mining_button(f, false);
    if (f->app != NULL && work->q_down) {
        (void)gameplay_key(f, SDL_SCANCODE_Q, false);
        (void)transfer(f);
    }
    if (f->app != NULL && work->mining.forward_down) {
        (void)gameplay_key(f, SDL_SCANCODE_W, false);
        (void)transfer(f);
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
        frame_id == 0 || canvas_id == 0 || SDL_GetWindowFromID(frame_id) != NULL ||
        SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0) {
        printf("MINING_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
        printf("DROP_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    }
    free(work);
    free(f);
    return result;
}
