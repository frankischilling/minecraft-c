/* Continue the genuine collected-Dirt mining fixture through placement. */
int mc_placement_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_placement_included_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

typedef struct placement_work {
    mining_work mining;
    reload_epoch epoch4;
    bool backward_down, right_down, saving, sampled, save_selected;
    size_t empty_frames, clean_frames, completed_compiles, save_samples, saved_items;
    uint64_t placed_before;
} placement_work;

static placement_work *placement;

static int placement_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event));
    return transfer(f);
}

static bool placement_outside(const mc_entity *player) {
    const mc_entity_bounds *box = player->bounds;
    return box != NULL && (box->max_x <= 0. || box->min_x >= 1. || box->max_y <= 64. ||
                           box->min_y >= 65. || box->max_z <= 2. || box->min_z >= 3.);
}

static bool placement_floor(const fixture *f) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    return f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
           mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
           hit.fields.a == kinds.a && hit.fields.b == 0 && hit.fields.c == 63 &&
           hit.fields.d == 2 && hit.fields.e == 1 && placement_outside(f->app->h);
}

static int placement_used_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view statistics;
    int32_t value;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.E != NULL && statistics.E[3] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.E[3], &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          value == expected);
    return 0;
}

static int placement_clean_chunk(fixture *f, bool *clean) {
    const mc_client_render_global_fields *n = &f->app->g->fields;
    const int64_t count = (int64_t)n->p * n->q * n->r;
    CHECK(n->n != NULL && n->p > 0 && n->q > 0 && n->r > 0 && count > 0 && count <= 65536);
    size_t matches = 0;
    *clean = false;
    for (int64_t i = 0; i < count; ++i) {
        mc_client_source_dk_state state;
        CHECK(n->n[i] != NULL && mc_client_source_dk_inspect(n->n[i], &state));
        const mc_client_source_dk_fields *dk = &state.fields;
        if (dk->a == f->app->active_world && dk->c <= 0 && (int64_t)dk->c + dk->f > 0 &&
            dk->d <= 64 && (int64_t)dk->d + dk->g > 64 && dk->e <= 2 &&
            (int64_t)dk->e + dk->h > 2) {
            ++matches;
            *clean = !dk->u;
        }
    }
    CHECK(matches == 1);
    return 0;
}

static int placement_frame(fixture *f, placement_work *work) {
    CHECK(mining_frame(f, &work->mining) == 0);
    int32_t id, carried;
    CHECK(mining_target(f->app->active_world, &id) == 0 &&
          mining_current(f->app->h, &carried) == 0);
    if (f->state.gameplay_placed_blocks > work->placed_before) {
        CHECK(id == 3 && carried == 0 && f->state.world_frame->terrain_returned &&
              f->state.world_frame->updates_returned);
        const mc_client_render_global_update_state *update = f->state.world_frame->update;
        CHECK(update != NULL && update->returned);
        work->completed_compiles += update->completed_compiles;
        bool clean;
        CHECK(placement_clean_chunk(f, &clean) == 0);
        if (clean)
            ++work->clean_frames;
        const mc_client_first_person_hand_state *hand = f->state.world_frame->hand;
        if (f->state.hud->slots_returned && f->state.hud->empty_slots == 9 && hand != NULL &&
            hand->returned && hand->held_returned && hand->held_item != NULL &&
            hand->held_item->returned && hand->held_item->empty_hand) {
            ++work->empty_frames;
            if (work->empty_frames == 1)
                CHECK(mining_capture(f, &work->mining, "placed.png") == 0);
            if (work->empty_frames == 3)
                CHECK(mining_capture(f, &work->mining, "placed-settled.png") == 0);
        }
    }
    return 0;
}

static int placement_release_movement(fixture *f, placement_work *work, SDL_Scancode key,
                                      size_t events_before_press) {
    CHECK(gameplay_key(f, key, false) == 0 && transfer(f) == 0);
    if (key == SDL_SCANCODE_S)
        work->backward_down = false;
    else
        work->mining.forward_down = false;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    while (f->state.keyboard_events < events_before_press + 2 && frames < 64 &&
           SDL_GetTicks() < deadline) {
        CHECK(placement_frame(f, work) == 0);
        ++frames;
    }
    CHECK(f->state.keyboard_events >= events_before_press + 2);
    mc_client_sdk_input_state input;
    mc_client_sdk_bytes_state keys;
    CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
          mc_client_sdk_bytes_inspect(input.key_down, &keys));
    /* Original SDK KEY_W17/KEY_S31, matching the genuine transport mapping. */
    CHECK(keys.limit > 31 && keys.data[17] == 0 && keys.data[31] == 0);
    /* Observe ordinary friction after the source release before qualifying
     * the picked floor; no key, pose or velocity field is supplied here. */
    CHECK(placement_frame(f, work) == 0 && placement_frame(f, work) == 0);
    return 0;
}

static int placement_backaway(fixture *f, placement_work *work) {
    const double initial_z = f->app->h->base.z;
    const uint64_t deadline = SDL_GetTicks() + 120000;
    size_t attempts = 0;
    do {
        /* Choose only real movement from the observed current ray/position.
         * A later W correction can undo ordinary tick batching overshoot. */
        const SDL_Scancode key = f->app->h->base.z > -.5 ? SDL_SCANCODE_S : SDL_SCANCODE_W;
        const size_t events = f->state.keyboard_events;
        CHECK(gameplay_key(f, key, true) == 0 && transfer(f) == 0);
        if (key == SDL_SCANCODE_S)
            work->backward_down = true;
        else
            work->mining.forward_down = true;
        CHECK(placement_frame(f, work) == 0);
        /* A real short key hold after ordinary Display.update collected DOWN.
         * The next unmodified Timer chooses its ticks; UP joins the following
         * poll. Wait for BOTH source key records before choosing another tap. */
        SDL_Delay(60);
        CHECK(placement_release_movement(f, work, key, events) == 0);
        ++attempts;
        printf("PLACEMENT_APPROACH attempt=%zu key=%s floor=%d outside=%d z=%.17g\n", attempts,
               key == SDL_SCANCODE_S ? "S" : "W", (int)placement_floor(f),
               (int)placement_outside(f->app->h), f->app->h->base.z);
        CHECK(fflush(stdout) == 0);
    } while (!placement_floor(f) && attempts < 16 && SDL_GetTicks() < deadline);
    CHECK(placement_floor(f) && f->app->h->base.z < initial_z && !work->backward_down &&
          !work->mining.forward_down && mining_capture(f, &work->mining, "placement-aim.png") == 0);
    return 0;
}

static int placement_tap(fixture *f, placement_work *work) {
    const uint64_t clicks = f->state.gameplay_right_clicks;
    const uint64_t swings = f->state.gameplay_swing_calls;
    const uint64_t blocks = f->state.gameplay_block_uses;
    const uint64_t admitted = f->state.gameplay_placements_admitted;
    const uint64_t sounds = f->state.gameplay_placement_sounds;
    const uint64_t statistics = f->state.gameplay_used_statistics;
    const uint64_t counts = f->state.gameplay_stack_count_stores;
    const uint64_t clears = f->state.gameplay_selected_slot_clears;
    const uint64_t equips = f->state.gameplay_equip_resets;
    const uint64_t air = f->state.gameplay_air_uses;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    for (size_t attempt = 0; attempt < 12 && frames < 64 && SDL_GetTicks() < deadline; ++attempt) {
        CHECK(placement_floor(f) && save_title_focus(f) == 0);
        int64_t now;
        CHECK(mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        const int64_t p = f->app->P;
        const size_t events = f->state.mouse_events;
        CHECK(placement_button(f, true) == 0);
        work->right_down = true;
        CHECK(placement_button(f, false) == 0);
        work->right_down = false;
        bool drained = false;
        do {
            CHECK(placement_frame(f, work) == 0);
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
        } while (!drained && frames < 64 && SDL_GetTicks() < deadline);
        printf("PLACEMENT_INPUT attempt=%zu frames=%zu drained=%d age-before=%" PRId64
               " clicks=%" PRIu64 " block-uses=%" PRIu64 " admitted=%" PRIu64 " placed=%" PRIu64
               " sounds=%" PRIu64 " statistics=%" PRIu64 " count-stores=%" PRIu64 " clears=%" PRIu64
               " equips=%" PRIu64 " swings=%" PRIu64 " air-uses=%" PRIu64 "\n",
               attempt + 1, frames, (int)drained, mc_java_i64_sub(now, p),
               f->state.gameplay_right_clicks - clicks, f->state.gameplay_block_uses - blocks,
               f->state.gameplay_placements_admitted - admitted,
               f->state.gameplay_placed_blocks - work->placed_before,
               f->state.gameplay_placement_sounds - sounds,
               f->state.gameplay_used_statistics - statistics,
               f->state.gameplay_stack_count_stores - counts,
               f->state.gameplay_selected_slot_clears - clears,
               f->state.gameplay_equip_resets - equips, f->state.gameplay_swing_calls - swings,
               f->state.gameplay_air_uses - air);
        CHECK(fflush(stdout) == 0 && drained);
        if (f->state.gameplay_right_clicks > clicks) {
            CHECK(f->state.gameplay_right_clicks >= clicks + 1 &&
                  f->state.gameplay_block_uses >= blocks + 1 &&
                  f->state.gameplay_placements_admitted == admitted + 1 &&
                  f->state.gameplay_placed_blocks == work->placed_before + 1 &&
                  f->state.gameplay_placement_sounds == sounds + 1 &&
                  f->state.gameplay_used_statistics == statistics + 1 &&
                  f->state.gameplay_stack_count_stores == counts + 1 &&
                  f->state.gameplay_selected_slot_clears == clears + 1 &&
                  f->state.gameplay_equip_resets == equips &&
                  f->state.gameplay_swing_calls == swings + 1 && f->state.gameplay_air_uses == air);
            return 0;
        }
        CHECK(f->state.gameplay_block_uses == blocks &&
              f->state.gameplay_placements_admitted == admitted &&
              f->state.gameplay_placed_blocks == work->placed_before &&
              f->state.gameplay_swing_calls == swings);
    }
    fprintf(stderr, "Placement did not admit a real right tap within 12 taps/64 frames/60s\n");
    return 1;
}

static int placement_save_before(fixture *f) {
    placement_work *work = placement;
    work->sampled = false;
    if (!work->saving || f->app->r == NULL || f->app->r->kind != MC_CLIENT_SCREEN_PAUSE ||
        f->app->N || !atomic_load(&f->app->o))
        return 0;
    CHECK(f->app->active_world == work->mining.epochs[2].world &&
          f->app->h == work->mining.epochs[2].player &&
          mining_semantic(f->app->h, &work->mining, "placement-player-entry.nbt") == 0);
    size_t dirt_items;
    mc_entity *dirt = NULL;
    CHECK(mining_items(f->app->active_world, &dirt_items, &dirt) == 0 && dirt_items == 0);
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    size_t count = 0;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *entity = list->items[i];
        if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) ||
            entity->base.be)
            continue;
        char name[64];
        const int n = snprintf(name, sizeof(name), "placement-items-%04zu.nbt", count);
        CHECK(count < 128 && n > 0 && (size_t)n < sizeof(name) &&
              mining_semantic(entity, &work->mining, name) == 0);
        ++count;
    }
    char path[8192], text[32];
    const int n = snprintf(text, sizeof(text), "%zu\n", count);
    CHECK(n > 0 && (size_t)n < sizeof(text) &&
          mining_path(&work->mining, "placement-items.count", path) == 0 &&
          SDL_SaveFile(path, text, (size_t)n));
    work->saved_items = count;
    work->sampled = true;
    ++work->save_samples;
    return 0;
}

static void placement_save_after(fixture *f, bool returned) {
    if (placement->saving && placement->sampled && returned && f->app->active_world == NULL &&
        f->app->h == NULL && f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU)
        placement->save_selected = true;
}

static int placement_run(fixture *f, placement_work *work) {
    CHECK(mining_run(f, &work->mining) == 0 && placement_used_statistic(f, 0) == 0);
    work->placed_before = f->state.gameplay_placed_blocks;
    CHECK(placement_backaway(f, work) == 0 && placement_tap(f, work) == 0 &&
          placement_used_statistic(f, 1) == 0 && mining_statistic(f, 1) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    while ((work->empty_frames < 3 || work->clean_frames < 3 || work->completed_compiles == 0) &&
           frames < 128 && SDL_GetTicks() < deadline) {
        CHECK(placement_frame(f, work) == 0);
        ++frames;
    }
    CHECK(work->empty_frames >= 3 && work->clean_frames >= 3 && work->completed_compiles > 0);
    printf("PLACEMENT_RETURN block=3 metadata=0 empty-frames=%zu clean-frames=%zu "
           "completed-compiles=%zu source-used-statistic=1\n",
           work->empty_frames, work->clean_frames, work->completed_compiles);
    mining_extra_before_frame = placement_save_before;
    mining_extra_after_frame = placement_save_after;
    work->saving = true;
    char path[8192];
    CHECK(mining_path(&work->mining, "placement-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 3, 0);
    work->saving = false;
    CHECK(saved == 0 && work->save_selected && work->save_samples > 0 &&
          reload_snapshot("saved-3") == 0 &&
          mining_choose(f, &work->mining, &work->mining.epochs[2], &work->epoch4) == 0);
    int32_t id, count;
    size_t items;
    mc_entity *item;
    CHECK(mining_target(f->app->active_world, &id) == 0 && id == 3 &&
          mining_current(f->app->h, &count) == 0 && count == 0 &&
          mining_items(f->app->active_world, &items, &item) == 0 && items == 0 &&
          mining_semantic(f->app->h, &work->mining, "placement-player-reloaded.nbt") == 0 &&
          mining_capture(f, &work->mining, "placement-reloaded.png") == 0 &&
          placement_used_statistic(f, 1) == 0 && mining_statistic(f, 1) == 0);
    puts("PLACEMENT_RELOAD epochs=4 block=3 metadata=0 inventory-empty=1 dirt-items=0 "
         "ordinary-save-title=1");
    printf("PLACEMENT_SAVE_ENTRY samples=%zu all-items=%zu paused=1 owned-nbt=1 "
           "selected-before-leave-frame=1\n",
           work->save_samples, work->saved_items);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    placement_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    mining_fixture = f;
    placement = work;
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
            result = placement_run(f, work);
        }
    }
    mining_extra_before_frame = NULL;
    mining_extra_after_frame = NULL;
    if (f->app != NULL && work->mining.left_down)
        (void)mining_button(f, false);
    if (f->app != NULL && work->right_down)
        (void)placement_button(f, false);
    if (f->app != NULL && work->backward_down) {
        (void)gameplay_key(f, SDL_SCANCODE_S, false);
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
        printf("PLACEMENT_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    }
    free(work);
    free(f);
    return result;
}
