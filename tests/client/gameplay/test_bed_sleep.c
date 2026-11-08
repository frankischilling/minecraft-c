/* Loaded single-player night, actual SDK Bed use, natural100-tick sleep and
 * complete default-hostile CQfalse -> dawn. Only input files author the scene. */
#define MC_BED_PLACEMENT_ENTRY mc_bed_sleep_included_placement_main
#include "test_bed_placement.c"
#undef MC_BED_PLACEMENT_ENTRY

static int sleep_monitor(fixture *f) {
    mc_client_frame_state host;
    CHECK(mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host));
    SDL_Window *windows[] = {host.frame_window, host.canvas_window};
    int x, y, width, height;
    const char *bounds = SDL_getenv("SDL_DISPLAY_USABLE_BOUNDS");
    CHECK(bounds != NULL && sscanf(bounds, "%d,%d,%d,%d", &x, &y, &width, &height) == 4 &&
          width > 0 && height > 0);
    for (size_t i = 0; i < 2; ++i) {
        int wx, wy, ww, wh;
        SDL_Rect physical;
        CHECK(windows[i] != NULL && SDL_GetWindowPosition(windows[i], &wx, &wy) &&
              SDL_GetWindowSize(windows[i], &ww, &wh) &&
              SDL_GetDisplayBounds(SDL_GetDisplayForWindow(windows[i]), &physical) && wx >= x &&
              wy >= y && wx + ww <= x + width && wy + wh <= y + height && wx >= physical.x &&
              wy >= physical.y && wx + ww <= physical.x + physical.w &&
              wy + wh <= physical.y + physical.h);
        printf("BED_SLEEP_MONITOR window=%s x=%d y=%d width=%d height=%d target=%s\n",
               i == 0 ? "frame" : "canvas", wx, wy, ww, wh, bounds);
    }
    return 0;
}

static int sleep_tap(fixture *f) {
    bed_scene->phase = "sleep-entry";
    CHECK(bed_recent(f, f->state.ticks, 5) == 0 && repeat_focus_state(f, "sleep-entry") == 0 &&
          bed_hit(f, true) == 0 && bed_cells(f, true));
    const uint64_t clicks = f->state.gameplay_right_clicks, blocks = f->state.gameplay_block_uses,
                   air = f->state.gameplay_air_uses, used = f->state.gameplay_used_statistics,
                   counts = f->state.gameplay_stack_count_stores;
    CHECK(bed_button(f, true, true) == 0 && bed_button(f, true, false) == 0);
    bool returned = false;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(bed_frame(f) == 0 && repeat_focus_state(f, "sleep-entry") == 0);
        const mc_client_gameplay_input *input = f->state.gameplay_input;
        if (input != NULL && input->block_uses != 0) {
            const mc_singleplayer_use_state *use = &input->use_state;
            CHECK(input->returned && !input->failed && use->returned && use->result &&
                  use->activation_entered && use->activation_returned && use->activated &&
                  !use->item_entered && !use->count_stored && !use->statistic_returned &&
                  use->clicked_id == 26 && input->player_message_calls == 0);
            returned = true;
        }
        /* Later ticks in this same frame may replace the borrowed input view.
         * Its passive total is updated only after the complete call returns. */
        if (f->state.gameplay_block_uses == blocks + 1)
            returned = true;
        if (returned && f->state.gameplay_block_uses > blocks)
            break;
    }
    mc_local_player_snapshot player;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK);
    printf("BED_SLEEP_ENTRY_OBSERVATION returned=%d clicks=%" PRIu64 " blocks=%" PRIu64
           " air=%" PRIu64 " used=%" PRIu64 " counts=%" PRIu64 " sleeping=%d timer=%" PRId32
           " bed=%" PRId32 ",%" PRId32 ",%" PRId32 " spawn=%d J=%d B=%d q=%" PRId32 " N=%d\n",
           returned, f->state.gameplay_right_clicks - clicks, f->state.gameplay_block_uses - blocks,
           f->state.gameplay_air_uses - air, f->state.gameplay_used_statistics - used,
           f->state.gameplay_stack_count_stores - counts, player.player.u, player.player.private_a,
           player.player.v == NULL ? -1 : player.player.v->x,
           player.player.v == NULL ? -1 : player.player.v->y,
           player.player.v == NULL ? -1 : player.player.v->z, player.player.private_b != NULL,
           f->app->active_world->fields.J, f->app->active_world->fields.B,
           f->app->active_world->fields.q, f->app->active_world->fields.N);
    CHECK(returned && f->state.gameplay_right_clicks == clicks + 1 &&
          f->state.gameplay_block_uses == blocks + 1 && f->state.gameplay_air_uses == air &&
          f->state.gameplay_used_statistics == used &&
          f->state.gameplay_stack_count_stores == counts &&
          mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK && player.player.u &&
          player.player.private_a >= 0 && player.player.private_a < 100 &&
          player.player.v != NULL && player.player.v->x == 0 && player.player.v->y == 64 &&
          player.player.v->z == 3 && player.player.private_b == NULL &&
          f->app->active_world->fields.J && !f->app->active_world->fields.B &&
          f->app->active_world->fields.q == 2 && f->app->active_world->fields.N);
    printf("BED_SLEEP_ENTRY SDK-right-tap=1 source-returned=1 sleeping=1 timer=%" PRId32
           " bed=0,64,3 saved-spawn=null difficulty=2 hostiles=1\n",
           player.player.private_a);
    CHECK(bed_player_nbt(f, "bed-player-sleeping.nbt") == 0 && bed_draw(f, "sleeping") == 0);
    return 0;
}

static int sleep_natural_dawn(fixture *f) {
    bed_scene->phase = "natural-sleep";
    const uint64_t ticks_before = f->state.ticks;
    int32_t highest = 0;
    size_t transitions = 0, overlays = 0;
    bool dawn = false;
    for (size_t i = 0; i < 4000 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(bed_frame(f) == 0 && repeat_focus_state(f, "natural-sleep") == 0);
        mc_local_player_snapshot player;
        CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
              player.living.Y == 20 && !f->app->active_world->fields.B &&
              f->app->active_world->fields.q == 2 && f->app->active_world->fields.N);
        const mc_client_gui_ingame_draw_state *hud = f->state.hud;
        if (hud != NULL && hud->returned && hud->sleep_overlay_entered &&
            hud->sleep_overlay_returned && hud->sleep_timer > 0)
            ++overlays;
        if (player.player.u) {
            CHECK(player.player.private_a >= highest && player.player.private_a <= 100 &&
                  player.player.private_b == NULL && f->app->active_world->fields.J);
            if (player.player.private_a > highest) {
                highest = player.player.private_a;
                ++transitions;
            }
        } else {
            CHECK(f->state.nightmare_calls == 1 && f->state.nightmare_returns == 1 &&
                  f->state.nightmare_true_returns == 0 && f->state.nightmare_wake_returns == 0 &&
                  f->state.sleep_wake_calls == 1 && f->state.sleep_wake_returns == 1 &&
                  highest >= 90 && transitions >= 10 && overlays > 0 &&
                  f->state.ticks - ticks_before >= 90 && player.player.private_a >= 100 &&
                  player.player.private_b != NULL && player.player.private_b != player.player.v &&
                  player.player.private_b->x == 0 && player.player.private_b->y == 64 &&
                  player.player.private_b->z == 3 && !f->app->active_world->fields.J &&
                  f->app->active_world->fields.f < 4);
            printf("BED_SLEEP_DAWN natural-player-ticks=%" PRIu64 " highest-observed=%" PRId32
                   " timer-transitions=%zu overlays=%zu CQ-calls=1 CQ-returns=1 CQ-result=0 "
                   "wake-calls=1 wake-returns=1 copied-spawn=0,64,3 health=20\n",
                   f->state.ticks - ticks_before, highest, transitions, overlays);
            dawn = true;
            break;
        }
    }
    CHECK(dawn && bed_player_nbt(f, "bed-player-dawn.nbt") == 0 && bed_draw(f, "dawn") == 0);
    for (size_t i = 0; i < 256 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        mc_local_player_snapshot player;
        CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK && !player.player.u);
        if (player.player.private_a == 0)
            return 0;
        CHECK(bed_frame(f) == 0 && repeat_focus_state(f, "awake-fade") == 0);
    }
    return 1;
}

static int sleep_run(fixture *f) {
    bed_scene->phase = "loading";
    CHECK(sleep_monitor(f) == 0 && repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          bed_choose(f) == 0);
    const mc_item_stack *before = NULL, *placed = NULL;
    CHECK(bed_player(f, 2, 0, &before) == 0 && bed_cells(f, false) &&
          !f->app->active_world->fields.B && f->app->active_world->fields.f >= 4 &&
          f->app->active_world->fields.q == 2);
    puts("BED_SLEEP_INPUTS chunks=289 glowstone-y=63 Bed=absent item=355 count=2 "
         "time=13000 pre-bootstrap-only=1");
    CHECK(bed_draw(f, "before") == 0 && bed_tap(f, true) == 0 &&
          bed_player(f, 1, 1, &placed) == 0 && before == placed && sleep_tap(f) == 0 &&
          sleep_natural_dawn(f) == 0 && bed_player_nbt(f, "bed-player-awake.nbt") == 0 &&
          mc_client_menu_loop_retire_completed_draws(f->loop));
    mc_world_runtime_lifecycle_error *saving = calloc(1, sizeof(*saving));
    CHECK(saving != NULL);
    const mc_chunk_lifecycle_status saved =
        mc_world_runtime_save_detailed(f->app->active_world, true, saving);
    if (saved != MC_CHUNK_LIFECYCLE_OK)
        fprintf(stderr, "BED_SLEEP_SAVE_FAILURE status=%d operation=%s\n", (int)saved,
                saving->cause.operation == NULL ? "none" : saving->cause.operation);
    free(saving);
    CHECK(saved == MC_CHUNK_LIFECYCLE_OK && mc_client_menu_loop_terminal_prepare(f->loop));
    puts("BED_SLEEP_SAVE source-full-save=1 live-player=1 SDK-save-menu=0 timer=0 sleeping=0");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "natural-night") != 0)
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
            result = sleep_run(f);
        }
    }
    const uint64_t cleanup_deadline = SDL_GetTicks() + 20000;
    if (result != 0 && f->app != NULL)
        native_focus_receipt(f, work.phase);
    if (f->app != NULL && work.right_down)
        (void)bed_button(f, true, false);
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
        printf("BED_SLEEP_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    bed_scene = NULL;
    free(f);
    return result;
}
