/* Real SDK preparation followed by declared adversarial WorldAccess inputs.
 * Neither mode replaces DC.r or manufactures a keyboard close invocation. */
int mc_auto_close_failure_included_main(int, char **);
#define MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN mc_auto_close_failure_included_main
#include "test_workbench_auto_close.c"
#undef MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN

typedef struct close_failure_work {
    bool fail, swapped, bad;
    mc_container *original_default, *replacement;
    mc_client_source_id *screen;
    const mc_statistic *play_statistic;
    int32_t play_time;
    uint64_t smoothing[6];
} close_failure_work;

static close_failure_work *adversarial;
static const char refusal[] = "Authored second Workbench drop WorldAccess refusal";

static bool player_site(mc_player_site site, const char *cl, const char *method,
                        const char *descriptor, unsigned int pc) {
    return site.class_name != NULL && strcmp(site.class_name, cl) == 0 && site.method != NULL &&
           strcmp(site.method, method) == 0 && site.descriptor != NULL &&
           strcmp(site.descriptor, descriptor) == 0 && site.pc == pc;
}

static bool screen_site(mc_client_screen_site site, const char *cl, const char *method,
                        const char *descriptor, unsigned int pc) {
    return player_site((mc_player_site){site.class_name, site.method, site.descriptor, site.pc}, cl,
                       method, descriptor, pc);
}

static void smoothing_read(const mc_player_fields *p, uint64_t out[6]) {
    const double values[6] = {p->o, p->p, p->q, p->r, p->s, p->t};
    for (size_t i = 0; i < 6; ++i)
        out[i] = wb_double_bits(values[i]);
}

static mc_world_storage_status adversarial_added(void *context, mc_world_runtime *world,
                                                 mc_entity *entity, mc_world_storage_error *error) {
    close_failure_work *w = context;
    *error = (mc_world_storage_error){0};
    if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
        return MC_WORLD_STORAGE_OK;
    for (size_t i = 0; i < 2; ++i) {
        if (entity->stack != automatic->expected[i])
            continue;
        mc_player_fields *p = &automatic->player->local_player->fields.player;
        if (world != automatic->world || automatic->publication_count != i ||
            automatic->published[i] != NULL || p->e != w->original_default ||
            p->d != w->original_default)
            w->bad = true;
        automatic->published[i] = entity;
        ++automatic->publication_count;
        if (i == 0)
            return MC_WORLD_STORAGE_OK;
        mc_player_container_state inherited;
        if (!w->screen->inherited_closed_returned || w->screen->direct_closed_returned ||
            w->screen->closed_parent == NULL ||
            !mc_player_container_inspect(w->screen->closed_parent->call, &inherited) ||
            !inherited.returned || inherited.container != w->original_default || w->swapped ||
            mc_client_stats_writer_value(mining_fixture->app->stats_writer, w->play_statistic,
                                         &w->play_time, NULL) != MC_CLIENT_STATS_OK)
            w->bad = true;
        smoothing_read(p, w->smoothing);
        const double dx = automatic->player->base.x - p->r;
        const double dy = automatic->player->base.y - p->s;
        const double dz = automatic->player->base.z - p->t;
        if (dx > 10 || dx < -10 || dy > 10 || dy < -10 || dz > 10 || dz < -10)
            w->bad = true; /* This suffix control excludes the separate smoothing clamp. */
        /* Declared adversarial callback mutation, after actual OB.e13. This
         * independently distinguishes both GS151 suppression and its fresh d. */
        p->d = w->replacement;
        w->swapped = true;
        if (w->fail) {
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE, refusal, 0x5a17};
            return error->status;
        }
    }
    return MC_WORLD_STORAGE_OK;
}

/* This authored observer ignores other notifications. Actual rendering/audio
 * listeners stay registered and execute normally; no source child is replaced. */
static int failure_receipts(bool cleaned) {
    const mc_client_world_tick *t = &automatic->tick;
    const mc_player_tick_error *p = &t->entities.child_error.player;
    const mc_client_screen_error *screen = &t->close_screen;
    const mc_client_inventory_screen_error *detail = &t->close_receipts.inventory;
    const mc_player_container_error *direct = &detail->direct_close;
    const mc_world_entity_error *publication =
        &t->close_publications[MC_CLIENT_WORLD_TICK_CLOSE_DIRECT];
    CHECK(t->failed && !t->returned && !t->entities_returned && t->close_calls == 1 &&
          t->close_returns == 0 && t->site.pc == 1216 &&
          strcmp(t->site.class_name, "net/minecraft/client/Minecraft") == 0 &&
          t->entities.child == MC_WORLD_ENTITY_TICK_CHILD_PLAYER && p->player_bytecode == 143 &&
          p->close_bytecode == 9 && p->close_error.native_policy &&
          !p->close_error.source_exception && !p->close_error.source_error &&
          !p->close_error.source_returned && p->close_error.exception_class == NULL &&
          player_site(p->close_error.site, "dc", "r", "()V", 9) &&
          player_site(p->close_error.parent_site, "oo", "h", "()V", 15));
    CHECK(screen->inventory == detail && screen->native_policy && !screen->source_exception &&
          !screen->source_error && !screen->source_returned && screen->exception_class == NULL &&
          screen_site(screen->site, "dc", "r", "()V", 9) &&
          screen_site(screen->parent_site, "oo", "h", "()V", 15) &&
          screen_site(detail->child, "it", "a", "(Lgs;)V", 39) &&
          player_site(detail->inherited_close.site, "ob", "a", "(ILgs;)V", 16) &&
          player_site(direct->site, "it", "a", "(Lgs;)V", 39) && direct->native_policy &&
          !direct->source_exception && !direct->source_error &&
          direct->drop.status == MC_PLAYER_DROP_WORLD && direct->drop.native_policy &&
          player_site(direct->drop.site, "gs", "a", "(Liz;Z)V", 315) &&
          player_site(direct->drop.child, "gs", "a", "(Lhl;)V", 5) &&
          direct->drop.publication == publication &&
          publication->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          publication->bytecode == 103 && publication->world_access_bytecode == 29 &&
          publication->cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          publication->cause.system_error == 0x5a17 && publication->cause.operation != NULL &&
          strcmp(publication->cause.operation, refusal) == 0 &&
          !t->close_access[MC_CLIENT_WORLD_TICK_CLOSE_DIRECT].present &&
          publication->world_access_failure.receiver == NULL &&
          publication->world_access_failure.render_global == NULL &&
          publication->world_access_failure.receipt == NULL);
    CHECK((t->close_receipts.inventory_source == NULL) == cleaned &&
          t->close_receipts.initialization_source == NULL && automatic->publication_count == 2 &&
          !adversarial->bad);
    return 0;
}

static int final_value_receipts(void) {
    if (adversarial->fail)
        return failure_receipts(true);
    const mc_client_world_tick *t = &automatic->tick;
    CHECK(t->returned && !t->failed && t->entities_returned && t->close_calls == 1 &&
          t->close_returns == 1 && t->close_screen.status == MC_CLIENT_SCREEN_OK &&
          t->close_screen.source_returned && t->close_screen.inventory == NULL &&
          t->close_screen.initialization == NULL && automatic->publication_count == 2);
    return 0;
}

static int failure_setup(fixture *f, mc_client_source_id **out) {
    mining_work *m = &workbench->mining;
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && auto_choose(f, m) == 0 && auto_world_ready(f) == 0);
    CHECK(auto_open(f, out) == 0 && *out != NULL);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(inventory_frame(f) == 0 && wb_ui(f, *out, 0, 0) == 0);
        ++workbench->ui_frames;
    } while (workbench->ui_frames < 3 && SDL_GetTicks() < deadline);
    CHECK(workbench->ui_frames == 3 && wb_click(f, *out, 12, false, false, 280, 2) == 0 &&
          wb_click(f, *out, 1, false, false, 0, 0) == 0 && wb_slot(*out, 1, 280, 2) == 0 &&
          wb_click(f, *out, 10, false, false, 5, 7) == 0 &&
          mining_capture(f, m, "workbench-adversarial-before.png") == 0);
    return 0;
}

static int adversarial_run(fixture *f) {
    mc_client_source_id *o = NULL;
    CHECK(failure_setup(f, &o) == 0);
    mc_player_fields *p = &f->app->h->local_player->fields.player;
    mc_container_state state;
    mc_registry_statistic_view statistics;
    automatic->world = f->app->active_world;
    automatic->player = f->app->h;
    adversarial->screen = o;
    adversarial->original_default = p->d;
    CHECK(f->app->r == &o->base && p->e == o->id.j && !f->app->N &&
          mc_inventory_player_source_cursor_get(p->c, &automatic->expected[0], NULL) ==
              MC_INVENTORY_OK &&
          mc_container_read_state(o->id.j, &state) && state.kind == MC_CONTAINER_WORKBENCH &&
          state.world == automatic->world && state.x == 0 && state.y == 64 && state.z == 2 &&
          mc_inventory_crafting_borrow_slot(state.crafting, 0, &automatic->expected[1], NULL) ==
              MC_INVENTORY_OK &&
          wb_stack(automatic->expected[0], 5, 7) == 0 &&
          wb_stack(automatic->expected[1], 280, 2) == 0 &&
          mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          mc_container_player_create_default(MC_REGISTRY_CLIENT, f->app->registry, p->c, NULL,
                                             &adversarial->replacement, NULL) == MC_INVENTORY_OK);
    adversarial->play_statistic = statistics.named[5];
    CHECK(adversarial->replacement != p->d &&
          mc_entity_set_position(automatic->player, 9.0, automatic->player->base.y,
                                 automatic->player->base.z) == MC_ENTITY_OK &&
          auto_distance(automatic->player) > 64);
    const mc_world_access_callbacks callbacks = {.cell = ignore_cell,
                                                 .range = ignore_range,
                                                 .sound = ignore_sound,
                                                 .particle = ignore_particle,
                                                 .entity_added = adversarial_added,
                                                 .tile_entity_dirty = ignore_tile,
                                                 .entity_removed = ignore_entity,
                                                 .sky_changed = ignore_sky,
                                                 .auxiliary = ignore_auxiliary};
    CHECK(mc_world_access_create(&callbacks, adversarial, &automatic->observer, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(automatic->world, automatic->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    automatic->observer_registered = true;
    mc_world_info_fields before, after;
    CHECK(mc_world_info_fields_get(automatic->world->fields.x, &before) == MC_WORLD_INFO_OK);
    const bool returned =
        mc_client_world_tick_run(f->app, f->app->source_math_classes, &automatic->tick);
    if (returned == adversarial->fail)
        fprintf(stderr, "Adversarial %s unexpected return%d: %s\n", automatic->mode, (int)returned,
                automatic->tick.operation);
    CHECK(returned != adversarial->fail && adversarial->swapped && !adversarial->bad &&
          automatic->publication_count == 2 && p->d == adversarial->replacement &&
          o->inherited_closed_returned && o->closed_parent != NULL && o->closed_direct != NULL &&
          wb_cursor(f, 0, 0) == 0 && wb_slot(o, 1, 280, 2) == 0);
    mc_player_container_state inherited, direct;
    mc_player_drop_state drops[2];
    CHECK(mc_player_container_inspect(o->closed_parent->call, &inherited) && inherited.returned &&
          inherited.container == adversarial->original_default && inherited.drop_calls == 1 &&
          mc_player_container_inspect(o->closed_direct->call, &direct) && direct.direct_target &&
          direct.container == o->id.j && direct.drop_calls == 1 &&
          direct.returned != adversarial->fail &&
          mc_player_container_drop_inspect(o->closed_parent->call, 0, &drops[0]) &&
          mc_player_container_drop_inspect(o->closed_direct->call, 0, &drops[1]) &&
          drops[0].returned && drops[0].publication_returned && drops[0].statistic_returned &&
          drops[1].publication_entered && drops[1].returned != adversarial->fail &&
          drops[1].publication_returned != adversarial->fail &&
          drops[1].statistic_returned != adversarial->fail);
    for (size_t i = 0; i < 2; ++i) {
        size_t occurrences = 0;
        const mc_world_reference_list *live = automatic->world->fields.b;
        CHECK(drops[i].item == automatic->published[i] &&
              drops[i].stack == automatic->expected[i] && drops[i].item != NULL &&
              drops[i].item->stack == automatic->expected[i] && live != NULL &&
              live->count <= live->capacity);
        for (size_t j = 0; j < live->count; ++j)
            if (live->items[j] == drops[i].item)
                ++occurrences;
        CHECK(occurrences == 1);
    }
    int32_t play_time = -1;
    uint64_t smoothing[6];
    smoothing_read(p, smoothing);
    CHECK(mc_client_stats_writer_value(f->app->stats_writer, adversarial->play_statistic,
                                       &play_time, NULL) == MC_CLIENT_STATS_OK &&
          mc_world_info_fields_get(automatic->world->fields.x, &after) == MC_WORLD_INFO_OK);
    if (adversarial->fail) {
        CHECK(p->e == adversarial->original_default && p->e != p->d && f->app->r == &o->base &&
              !f->app->N && !o->direct_closed_returned &&
              memcmp(smoothing, adversarial->smoothing, sizeof(smoothing)) == 0 &&
              play_time == adversarial->play_time && after.value.time == before.value.time &&
              failure_receipts(false) == 0);
        CHECK(!mc_client_world_tick_run(f->app, f->app->source_math_classes, &automatic->tick) &&
              failure_receipts(false) == 0);
    } else {
        printf("WORKBENCH_AUTO_FOCUS mode=%s after-close-source-focus=%d screen-null=%d\n",
               automatic->mode, (int)f->app->N, f->app->r == NULL);
        printf("WORKBENCH_FRESH_STATE active-new=%d direct-returned=%d close-calls=%zu "
               "close-returns=%zu returned=%d previous=%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64
               " observed=%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64
               " play-before=%d play-after=%d world-delta=%" PRId64 "\n",
               p->e == adversarial->replacement, o->direct_closed_returned,
               automatic->tick.close_calls, automatic->tick.close_returns,
               (int)automatic->tick.returned, adversarial->smoothing[3], adversarial->smoothing[4],
               adversarial->smoothing[5], smoothing[0], smoothing[1], smoothing[2],
               adversarial->play_time, play_time,
               mc_java_i64_sub(after.value.time, before.value.time));
        CHECK(p->e == adversarial->replacement && p->e != adversarial->original_default &&
              f->app->r == NULL && o->direct_closed_returned && automatic->tick.close_calls == 1 &&
              automatic->tick.close_returns == 1 && automatic->tick.returned &&
              smoothing[0] == adversarial->smoothing[3] &&
              smoothing[1] == adversarial->smoothing[4] &&
              smoothing[2] == adversarial->smoothing[5] &&
              play_time == mc_java_i32_add(adversarial->play_time, 1) &&
              mc_java_i64_sub(after.value.time, before.value.time) == 1);
    }
    CHECK(mc_client_world_tick_cleanup(&automatic->tick) &&
          mc_client_world_tick_cleanup(&automatic->tick) && automatic->publication_count == 2 &&
          (adversarial->fail ? failure_receipts(true) == 0 : true));
    printf("WORKBENCH_ADVERSARIAL mode=%s sdk-open=1 actual-world-tick=1 publications=2 "
           "callback-default-swap=1 inherited-AA-returned=1 direct-IT-returned=%d "
           "GS151=%d smoothing-suffix=%d play-time-suffix=%d typed-receipt=%d "
           "cleanup-no-replay=1 ordinary-save=0\n",
           automatic->mode, adversarial->fail ? 0 : 1, adversarial->fail ? 0 : 1,
           adversarial->fail ? 0 : 1, adversarial->fail ? 0 : 1, adversarial->fail ? 1 : 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 && (strcmp(argv[3], "failure") == 0 || strcmp(argv[3], "freshness") == 0));
    fixture *f = calloc(1, sizeof(*f));
    workbench = calloc(1, sizeof(*workbench));
    automatic = calloc(1, sizeof(*automatic));
    adversarial = calloc(1, sizeof(*adversarial));
    if (f == NULL || workbench == NULL || automatic == NULL || adversarial == NULL)
        return 1;
    automatic->mode = argv[3];
    adversarial->fail = strcmp(argv[3], "failure") == 0;
    mining_fixture = f;
    workbench->mining.directory = argv[2];
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
            result = adversarial_run(f);
        }
    }
    const bool tick_clean = mc_client_world_tick_cleanup(&automatic->tick);
    if (!tick_clean || auto_observer_remove() != 0)
        result = 1;
    /* Independent fixture teardown only, after all source-prefix/suffix and
     * copied-value observations. Restore the declared callback input so the
     * actual Player's constructor-owned default can retire normally. */
    if (adversarial->swapped && automatic->player != NULL) {
        mc_player_fields *p = &automatic->player->local_player->fields.player;
        p->d = adversarial->original_default;
        p->e = adversarial->original_default;
    }
    if (mc_container_retire(&adversarial->replacement, NULL) != MC_INVENTORY_OK)
        result = 1;
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    if (result == 0 && final_value_receipts() != 0)
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
        SDL_GetWindowFromID(canvas_id) != NULL || automatic->publication_count != 2)
        result = 1;
    if (result == 0 && final_value_receipts() != 0)
        result = 1;
    if (result == 0)
        printf("WORKBENCH_ADVERSARIAL_FINISH mode=%s cleanup=1 windows-absent=1 "
               "publications=2 diagnostic-after-router=1 diagnostic-after-bootstrap=1 checks=%u\n",
               automatic->mode, checks);
    if (closed == MC_CLIENT_BOOTSTRAP_OK && tick_clean && automatic->observer == NULL &&
        adversarial->replacement == NULL) {
        free(adversarial);
        free(automatic);
        free(workbench);
        free(f);
    }
    return result;
}
