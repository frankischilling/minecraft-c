/* Actual ordinary three-epoch source UI lifecycle. No gameplay-field repairs. */
typedef struct reload_epoch {
    const mc_client_world_load_call *call;
    mc_world_runtime *world;
    mc_entity *player;
    mc_world_store *handler;
    mc_movement_input *input;
    const mc_client_singleplayer_controller *controller;
} reload_epoch;

typedef struct reload_snapshot_budget {
    size_t files, entries, bytes;
} reload_snapshot_budget;

typedef struct reload_snapshot_walk {
    const char *destination;
    unsigned int depth;
    reload_snapshot_budget *budget;
} reload_snapshot_walk;

static bool reload_snapshot_tree(const char *, const char *, unsigned int,
                                 reload_snapshot_budget *);

static SDL_EnumerationResult SDLCALL reload_snapshot_entry(void *context, const char *directory,
                                                           const char *name) {
    reload_snapshot_walk *walk = context;
    char source[8192], target[8192];
    if (++walk->budget->entries > 1024 || strchr(name, '/') != NULL || strchr(name, '\\') != NULL)
        return SDL_ENUM_FAILURE;
    const int a = snprintf(source, sizeof(source), "%s%s", directory, name);
    const int b = snprintf(target, sizeof(target), "%s/%s", walk->destination, name);
    if (a <= 0 || (size_t)a >= sizeof(source) || b <= 0 || (size_t)b >= sizeof(target))
        return SDL_ENUM_FAILURE;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(source, &info))
        return SDL_ENUM_FAILURE;
    if (info.type == SDL_PATHTYPE_DIRECTORY)
        return walk->depth < 3 &&
                       reload_snapshot_tree(source, target, walk->depth + 1, walk->budget)
                   ? SDL_ENUM_CONTINUE
                   : SDL_ENUM_FAILURE;
    if (info.type != SDL_PATHTYPE_FILE || info.size > 1048576 || walk->budget->files >= 1024 ||
        info.size > 16u * 1048576u - walk->budget->bytes || !SDL_CopyFile(source, target))
        return SDL_ENUM_FAILURE;
    ++walk->budget->files;
    walk->budget->bytes += (size_t)info.size;
    return SDL_ENUM_CONTINUE;
}

static bool reload_snapshot_tree(const char *source, const char *target, unsigned int depth,
                                 reload_snapshot_budget *budget) {
    reload_snapshot_walk walk = {target, depth, budget};
    return SDL_CreateDirectory(target) &&
           SDL_EnumerateDirectory(source, reload_snapshot_entry, &walk);
}

static int reload_snapshot(const char *phase) {
    const char *saves = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_WORLD_RELOAD_SAVES");
    const char *root =
        SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_WORLD_SAVE_TITLE_SNAPSHOTS");
    char target[8192];
    CHECK(saves != NULL && root != NULL);
    const int n = snprintf(target, sizeof(target), "%s/%s", root, phase);
    CHECK(n > 0 && (size_t)n < sizeof(target));
    reload_snapshot_budget budget = {0};
    CHECK(reload_snapshot_tree(saves, target, 0, &budget) && budget.files != 0);
    printf("WORLD_RELOAD_SNAPSHOT phase=%s files=%zu bytes=%zu actual-file-copy=1\n", phase,
           budget.files, budget.bytes);
    return 0;
}

static int reload_entry_snapshot(size_t epoch, size_t frame_index, size_t *aggregate,
                                 size_t *bytes) {
    const char *source =
        SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_WORLD_SAVE_TITLE_SAVES");
    const char *root =
        SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_WORLD_SAVE_TITLE_ENTRIES");
    char target[8192];
    CHECK(source != NULL && root != NULL && epoch >= 1 && epoch <= 3 && frame_index < 64 &&
          *aggregate <= 64u * 16u * 1048576u);
    const int n =
        snprintf(target, sizeof(target), "%s/epoch-%zu/frame-%zu", root, epoch, frame_index);
    CHECK(n > 0 && (size_t)n < sizeof(target));
    save_title_entry_budget budget = {0};
    CHECK(save_title_entry_tree(source, target, 0, &budget) && budget.files != 0 &&
          budget.bytes <= 64u * 16u * 1048576u - *aggregate);
    *aggregate += budget.bytes;
    *bytes = budget.bytes;
    return 0;
}

static int reload_player_observation(mc_world_runtime *world, mc_entity *player, size_t epoch) {
    const char *root =
        SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_WORLD_SAVE_TITLE_SNAPSHOTS");
    mc_world_info_fields info;
    CHECK(root != NULL && mc_world_info_fields_get(world->fields.x, &info) == MC_WORLD_INFO_OK);
    CHECK(info.player == NULL); /* Actual fd.a(gs) clears WorldInfo.Player at22. */
    mc_entity_nbt_error *entity = calloc(1, sizeof(*entity));
    mc_nbt_tree *tree = NULL;
    CHECK(entity != NULL && mc_entity_build_nbt(player, NULL, &tree, entity) == MC_ENTITY_NBT_OK);
    const mc_nbt_tag *semantic = mc_nbt_tree_root(tree);
    CHECK(semantic != NULL && semantic->type == MC_NBT_COMPOUND);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    size_t bytes = 0, written = 0;
    mc_nbt_error error;
    CHECK(mc_nbt_measure(semantic, &limits, &bytes, &error) == MC_NBT_OK && bytes <= 1048576);
    uint8_t *wire = malloc(bytes);
    CHECK(wire != NULL &&
          mc_nbt_write(semantic, &limits, wire, bytes, &written, &error) == MC_NBT_OK &&
          written == bytes);
    char path[8192];
    const int n = snprintf(path, sizeof(path), "%s/loaded-player-%zu.nbt", root, epoch);
    CHECK(n > 0 && (size_t)n < sizeof(path) && SDL_SaveFile(path, wire, written));
    free(wire);
    mc_nbt_tree_free(tree);
    free(entity);
    return 0;
}

static int reload_capture_epoch(fixture *f, reload_epoch *epoch) {
    mc_client_screen_transition_state transitions;
    mc_client_world_load_state load;
    CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
          transitions.world_load != NULL &&
          mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
          load.failure->status == MC_CLIENT_WORLD_LOAD_OK && load.last_outer_pc == 130 &&
          load.world == f->app->active_world && load.player == f->app->h &&
          f->app->i == load.player && load.handler != NULL && load.input != NULL &&
          load.controller == f->app->controller && load.renderer != NULL &&
          load.renderer->source_returned && load.renderer->source_pc == 73 && !load.superseded);
    *epoch = (reload_epoch){transitions.world_load, load.world, load.player,
                            load.handler,           load.input, load.controller};
    return 0;
}

static int reload_choose(fixture *f, mc_client_source_da *title, const char *folder,
                         const reload_epoch *previous, reload_epoch *current, size_t epoch) {
    mc_client_source_rq *rq = NULL;
    CHECK(title != NULL && title->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          f->app->active_world == NULL && f->app->h == NULL && f->app->i == NULL &&
          open_selection(f, title, &rq) == 0 && release_mouse(f, rq) == 0);
    const size_t count = mc_client_save_list_size(rq->fields.n);
    int32_t index = -1;
    for (size_t i = 0; i < count; ++i)
        if (filename_is(mc_client_save_list_at(rq->fields.n, i), folder)) {
            CHECK(index == -1);
            index = (int32_t)i;
        }
    CHECK(index >= 0);
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + index * 36 + 8;
    CHECK(y >= list->h && y <= list->i &&
          push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    const uint64_t row_deadline = SDL_GetTicks() + 3000;
    while (rq->fields.m != index && SDL_GetTicks() < row_deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.m == index && release_mouse(f, rq) == 0 && rq->fields.t->fields.g &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 10000;
    mc_client_screen_transition_state transitions;
    do {
        CHECK(frame(f) == 0 &&
              mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions));
        if (f->app->active_world != NULL && transitions.world_load != previous->call)
            break;
    } while (SDL_GetTicks() < deadline);
    CHECK(reload_capture_epoch(f, current) == 0 && current->call != previous->call &&
          current->world != previous->world && current->player != previous->player &&
          current->handler != previous->handler && current->input != previous->input &&
          current->controller != previous->controller && f->state.render_returned &&
          f->state.world_frame != NULL && f->state.world_frame->returned &&
          f->state.world_frame->source_pc == 1013 && f->state.world_frame->world == current->world);
    mc_client_world_load_state load, old;
    mc_client_render_global_state n;
    mc_effect_renderer_snapshot effects;
    CHECK(mc_client_world_load_inspect(current->call, &load) &&
          mc_client_world_load_inspect(previous->call, &old) && old.superseded &&
          old.source_returned && old.leave.source_returned && old.leave.i_cleared &&
          old.leave.f_cleared && old.leave.h_cleared && old.renderer == NULL &&
          load.renderer != NULL && load.renderer->old_world == previous->world &&
          load.renderer->argument == current->world && load.renderer->old_removed &&
          load.renderer->manager_returned && load.renderer->k_stored &&
          load.renderer->cv_constructed && load.renderer->u_stored && load.renderer->registered &&
          load.renderer->rebuild_returned && mc_client_render_global_inspect(f->app->g, &n) &&
          n.phase == MC_CLIENT_RENDER_GLOBAL_READY && !f->app->g->world_retiring &&
          n.fields.k == current->world &&
          mc_client_effect_renderer_read(f->app->effect_renderer, &effects) &&
          effects.world == current->world && previous->world->world_accesses.count == 0);
    mc_world_runtime_construction construction;
    CHECK(mc_world_runtime_construction_get(previous->world, &construction) ==
              MC_WORLD_STORAGE_OK &&
          construction.phase == MC_WORLD_RUNTIME_READY);
    int32_t marker = 0;
    mc_world_storage_error block_error;
    CHECK(mc_world_runtime_block_get(current->world, 0, 127, 0, &marker, &block_error) ==
              MC_WORLD_STORAGE_OK &&
          marker == (epoch == 3 ? 56 : 57));
    if (epoch == 2)
        CHECK(reload_player_observation(current->world, current->player, epoch) == 0);
    printf(
        "WORLD_RELOAD_SELECT epoch=%zu folder=%s distinct-os=1 distinct-load=1 distinct-handler=1 "
        "distinct-world=1 distinct-player=1 distinct-input=1 n-ready=1 n-old-remove=1 n-th=1 n-k=1 "
        "n-cv=1 n-register=1 n-rebuild=1 dn-replaced=1 previous-ready=1 returned=130 frame=1013\n",
        epoch, folder);
    return 0;
}

static int reload_audit(fixture *f, const reload_epoch *epochs, size_t count,
                        mc_world_storage_status expected) {
    struct copy {
        const void *owner;
        size_t bytes;
        void *data;
    } copies[28];

    CHECK(count == 3);
    size_t used = 0, total = 0;
    copies[used++] = (struct copy){f->app, sizeof(*f->app), NULL};
    for (size_t i = 0; i < count; ++i) {
        mc_world_source_state handler;
        mc_world_source_chunk_loader_state loader;
        mc_world_saved_data_source_state saved;
        CHECK(epochs[i].world->source_construction != NULL &&
              mc_world_store_source_inspect(epochs[i].handler, &handler) &&
              mc_world_source_chunk_loader_inspect(epochs[i].world->source_construction->loader,
                                                   &loader) &&
              mc_world_saved_data_source_inspect(epochs[i].world->fields.z, &saved));
        copies[used++] = (struct copy){epochs[i].world, sizeof(*epochs[i].world), NULL};
        copies[used++] = (struct copy){epochs[i].player, sizeof(*epochs[i].player), NULL};
        copies[used++] = (struct copy){epochs[i].world->source_construction,
                                       sizeof(*epochs[i].world->source_construction), NULL};
        copies[used++] = (struct copy){handler.failure, sizeof(*handler.failure), NULL};
        copies[used++] = (struct copy){handler.cleanup, sizeof(*handler.cleanup), NULL};
        copies[used++] = (struct copy){loader.failure, sizeof(*loader.failure), NULL};
        copies[used++] = (struct copy){loader.cleanup, sizeof(*loader.cleanup), NULL};
        copies[used++] = (struct copy){saved.failure, sizeof(*saved.failure), NULL};
        copies[used++] = (struct copy){saved.cleanup, sizeof(*saved.cleanup), NULL};
    }
    CHECK(used == 28);
    for (size_t i = 0; i < used; ++i) {
        CHECK(copies[i].owner != NULL && copies[i].bytes <= 16u * 1048576u - total);
        total += copies[i].bytes;
        copies[i].data = malloc(copies[i].bytes);
        CHECK(copies[i].data != NULL);
        memcpy(copies[i].data, copies[i].owner, copies[i].bytes);
    }
    mc_world_terminal_inventory inventory;
    memset(&inventory, 0xa5, sizeof(inventory));
    unsigned char sentinel[sizeof(inventory)];
    memcpy(sentinel, &inventory, sizeof(inventory));
    mc_world_storage_error error;
    const mc_world_storage_status status = mc_client_screen_transitions_terminal_admit(
        f->app->screen_transitions, NULL, &inventory, &error);
    CHECK(status == expected && error.status == expected);
    for (size_t i = 0; i < used; ++i) {
        CHECK(memcmp(copies[i].owner, copies[i].data, copies[i].bytes) == 0);
        free(copies[i].data);
    }
    if (expected != MC_WORLD_STORAGE_OK)
        CHECK(memcmp(sentinel, &inventory, sizeof(inventory)) == 0);
    else
        CHECK(inventory.entities >= 3 && inventory.entity_occurrences >= 3);
    return 0;
}

static int reload_cycle_controls(fixture *f, mc_client_source_da *initial,
                                 const char *capture_path) {
    reload_epoch epochs[3];
    const char *folders[] = {"alpha", "alpha", "older"};
    CHECK(reload_capture_epoch(f, &epochs[0]) == 0);
    mc_client_render_global *n = f->app->g;
    mc_client_effect_renderer *dn = f->app->effect_renderer;
    for (size_t i = 0; i < 3; ++i) {
        if (i != 0)
            CHECK(reload_choose(f, f->app->r, folders[i], &epochs[i - 1], &epochs[i], i + 1) == 0);
        CHECK(f->app->g == n && f->app->effect_renderer == dn);
        char before[32], after[32];
        CHECK(snprintf(before, sizeof(before), "before-%zu", i + 1) > 0 &&
              snprintf(after, sizeof(after), "after-%zu", i + 1) > 0 &&
              reload_snapshot(before) == 0 &&
              save_title_run(f, initial, capture_path, false, i + 1, i + 1) == 0 &&
              reload_snapshot(after) == 0);
        printf("WORLD_RELOAD_PHASE epoch=%zu folder=%s save-title=1\n", i + 1, folders[i]);
    }
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < i; ++j)
            CHECK(epochs[i].call != epochs[j].call && epochs[i].world != epochs[j].world &&
                  epochs[i].player != epochs[j].player && epochs[i].handler != epochs[j].handler &&
                  epochs[i].input != epochs[j].input &&
                  epochs[i].controller != epochs[j].controller);
    puts("WORLD_RELOAD_IDENTITIES epochs=3 all-pairs-distinct=1 source-owners-live=1");
    /* Retain a genuine predecessor root before true outer preparation. All
     * three Worlds must remain READY when its passive audit refuses. */
    mc_client_achievement_notice *actual_notice = f->app->achievement_notice;
    const mc_client_achievement_notice_fields *notice_fields =
        mc_client_achievement_notice_fields_get(actual_notice);
    unsigned char notice_before[sizeof(*notice_fields)];
    CHECK(notice_fields != NULL);
    memcpy(notice_before, notice_fields, sizeof(notice_before));
    CHECK(mc_entity_retain(epochs[0].player) == MC_ENTITY_OK &&
          !mc_client_menu_loop_terminal_retire(&f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && !f->state.terminal_tail_entered &&
          f->state.notice == NULL && f->app->achievement_notice == actual_notice &&
          memcmp(notice_before, mc_client_achievement_notice_fields_get(actual_notice),
                 sizeof(notice_before)) == 0 &&
          f->state.retirement_world != NULL &&
          f->state.retirement_world->status == MC_WORLD_STORAGE_BUSY);
    puts("WORLD_RELOAD_NOTICE terminal-prefix-retired=1 source-fields-unchanged=1");
    for (size_t i = 0; i < 3; ++i) {
        mc_world_runtime_construction state;
        CHECK(mc_world_runtime_construction_get(epochs[i].world, &state) == MC_WORLD_STORAGE_OK &&
              state.phase == MC_WORLD_RUNTIME_READY);
    }
    CHECK(reload_audit(f, epochs, 3, MC_WORLD_STORAGE_BUSY) == 0);
    mc_entity_release(epochs[0].player);
    CHECK(reload_audit(f, epochs, 3, MC_WORLD_STORAGE_OK) == 0);
    puts("WORLD_RELOAD_TERMINAL epochs=3 old-root-busy=1 all-worlds-ready=1 passive-invariant=1 "
         "released-root-retry=1 all-epoch-audit=1");
    return 0;
}
