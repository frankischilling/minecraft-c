/* Pre-bootstrap saved Pig/Arrow, ordinary Timer/World ticks and SDK save.
 * Health/membership are causal scene observations, never a direct SL741 receipt. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool arrow_pig_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame arrow_pig_observed_frame
#define MC_BED_PLACEMENT_ENTRY mc_arrow_pig_included_bed_main
#include "test_bed_placement.c"
#undef MC_BED_PLACEMENT_ENTRY
#undef mc_client_menu_loop_frame
#include "mc/entity/arrow/construction.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/nbt/factory.h"
#include "mc/entity/nbt/storage.h"

typedef struct arrow_pig_work {
    fixture *fixture;
    mc_world_runtime *world;
    mc_entity *pig; /* Borrowed only while the live World owns this surviving Pig. */
    uintptr_t pig_identity, arrow_identity;
    uint64_t observed_world_ticks;
    bool valid, pig_seen, pig_initial_seen, arrow_seen, hit_seen, paused;
} arrow_pig_work;

static arrow_pig_work *arrow_pig_scene;

static void arrow_pig_observe(arrow_pig_work *work, const mc_client_menu_loop_state *state) {
    fixture *f = work->fixture;
    mc_world_runtime *world = f->app->active_world;
    if (work->paused || world == NULL || f->app->h == NULL || world->fields.b == NULL)
        return;
    size_t pigs = 0, arrows = 0, players = 0, others = 0;
    mc_entity *pig = NULL;
    for (size_t i = 0; i < world->fields.b->count; ++i) {
        mc_entity *entity = world->fields.b->items[i];
        if (entity == f->app->h) {
            ++players;
            continue;
        }
        if (entity == NULL || entity->classes != f->app->entities || entity->base.world != world) {
            work->valid = false;
            continue;
        }
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PIG)) {
            ++pigs;
            pig = entity;
        } else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ARROW)) {
            ++arrows;
            mc_arrow_state arrow = {0};
            work->valid &= mc_arrow_inspect(entity, &arrow) && arrow.phase == MC_ENTITY_READY &&
                           arrow.fields.c == NULL && mc_entity_storage_source(entity) != NULL;
            work->arrow_seen = true;
            work->arrow_identity =
                (uintptr_t)entity; /* VALUE only; never dereferenced after removal. */
        } else
            ++others;
    }
    if (pigs == 0 && !work->pig_seen)
        return; /* Loading has not yet activated the saved subject's Chunk. */
    mc_mob_state mob = {0};
    work->valid &= pigs == 1 && players == 1 && others == 0 && arrows <= 1 && pig != NULL &&
                   mc_mob_inspect(pig, &mob) && mob.kind == MC_ENTITY_CLASS_KIND_PIG &&
                   mob.phase == MC_ENTITY_READY && !pig->base.be && !mob.living.ah &&
                   mc_entity_storage_source(pig) != NULL &&
                   (mob.living.Y == 10 || mob.living.Y == 6);
    if (!work->valid)
        return;
    if (work->pig_seen)
        work->valid &= work->world == world && work->pig == pig;
    work->pig_seen = true;
    work->world = world;
    work->pig = pig;
    work->pig_identity = (uintptr_t)pig;
    work->pig_initial_seen |= mob.living.Y == 10;
    if (mob.living.Y == 6 && arrows == 0 && state->world_tick_returns > 0) {
        work->hit_seen = true;
        work->observed_world_ticks = state->world_tick_returns;
    }
}

static bool arrow_pig_observed_frame(mc_client_menu_loop *loop) {
    const bool returned = mc_client_menu_loop_frame(loop);
    arrow_pig_work *work = arrow_pig_scene;
    if (work != NULL && work->fixture != NULL && work->fixture->loop == loop) {
        mc_client_menu_loop_state state;
        if (mc_client_menu_loop_inspect(loop, &state)) {
            arrow_pig_observe(work, &state);
            if (!returned) {
                work->fixture->state = state;
                creature_tick_diagnostics(work->fixture);
                fprintf(stderr,
                        "ARROW_PIG_FAILURE phase=%s source=%s.%s:%" PRId64 " operation=%s\n",
                        bed_scene->phase, state.source_class == NULL ? "none" : state.source_class,
                        state.source_method == NULL ? "none" : state.source_method, state.source_pc,
                        state.operation == NULL ? "none" : state.operation);
            }
        }
    }
    return returned;
}

static int arrow_pig_monitor(fixture *f) {
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
        printf("ARROW_PIG_MONITOR window=%s x=%d y=%d width=%d height=%d target=%s\n",
               i == 0 ? "frame" : "canvas", wx, wy, ww, wh, bounds);
    }
    return 0;
}

static int arrow_pig_player(fixture *f) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    mc_registry_statistic_view statistics;
    int32_t taken = -1;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          player.living.Y == 20 && !player.player.u && player.player.private_a == 0 &&
          player.player.v == NULL && player.player.private_b == NULL &&
          mc_inventory_player_read_state(player.player.c, &inventory) && inventory.selected == 0 &&
          inventory.cursor == NULL && inventory.main_slots != NULL &&
          inventory.armor_slots != NULL &&
          mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.named != NULL && statistics.named[18] != NULL &&
          mc_statistic_fields_get(statistics.named[18]) != NULL &&
          mc_statistic_fields_get(statistics.named[18])->e == 2021 &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.named[18], &taken, NULL) ==
              MC_CLIENT_STATS_OK &&
          taken == 0);
    for (size_t i = 0; i < 36; ++i)
        CHECK(inventory.main_slots[i] == NULL);
    for (size_t i = 0; i < 4; ++i)
        CHECK(inventory.armor_slots[i] == NULL);
    return 0;
}

static int arrow_pig_members(fixture *f) {
    arrow_pig_work *work = arrow_pig_scene;
    CHECK(work->valid && work->hit_seen && work->pig_seen && work->pig != NULL &&
          work->world == f->app->active_world && f->app->active_world->fields.b->count == 2 &&
          !f->app->active_world->fields.B && f->app->active_world->fields.q == 2);
    size_t pigs = 0, players = 0;
    for (size_t i = 0; i < work->world->fields.b->count; ++i) {
        mc_entity *entity = work->world->fields.b->items[i];
        if (entity == f->app->h)
            ++players;
        else {
            mc_mob_state pig;
            CHECK(entity == work->pig && mc_mob_inspect(entity, &pig) &&
                  pig.kind == MC_ENTITY_CLASS_KIND_PIG && pig.living.Y == 6 && !pig.living.ah &&
                  !entity->base.be && entity->storage_source != NULL);
            ++pigs;
        }
    }
    CHECK(pigs == 1 && players == 1 && arrow_pig_player(f) == 0);
    return 0;
}

static int arrow_pig_picture(fixture *f, const char *name) {
    char path[8192];
    CHECK(bed_path(name, path) == 0 && capture(f, path) == 0);
    return 0;
}

static int arrow_pig_hit_frame(fixture *f) {
    bed_scene->phase = "natural-arrow-pig-impact";
    for (size_t i = 0; i < 4000 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(arrow_pig_scene->valid && repeat_focus_state(f, bed_scene->phase) == 0);
        if (arrow_pig_scene->hit_seen) {
            const mc_client_world_frame_state *draw = f->state.world_frame;
            const mc_client_gui_ingame_draw_state *hud = f->state.hud;
            CHECK(arrow_pig_members(f) == 0);
            if (draw != NULL && draw->returned && draw->terrain_returned &&
                draw->updates_returned && draw->source_pc == 1013 && draw->terrain_count > 0 &&
                hud != NULL && hud->returned && hud->source_pc == 2101 && hud->hotbar_returned &&
                hud->slots_returned && hud->chat_returned && hud->health == 20 &&
                hud->selected == 0 && hud->item_slots == 0 && hud->empty_slots == 9) {
                CHECK(f->state.gameplay_left_clicks == 0 && f->state.gameplay_right_clicks == 0 &&
                      f->state.gameplay_mining_starts == 0 && f->state.gameplay_block_uses == 0 &&
                      f->state.gameplay_air_uses == 0 && f->state.nightmare_calls == 0 &&
                      f->state.sleep_wake_calls == 0 && f->state.world_tick_returns > 0 &&
                      f->state.mob_hurt.reached_calls == 0 && !f->state.mob_hurt.saturated &&
                      arrow_pig_picture(f, "arrow-pig-hit.png") == 0);
                puts("ARROW_PIG_CAUSAL saved-pig-health=10 observed-health=6 arrow-members=0 "
                     "ordinary-world-ticks=1 direct741-receipt=0");
                printf("ARROW_PIG_OBSERVE initial10-seen=%d arrow-seen=%d world-ticks=%" PRIu64
                       "\n",
                       arrow_pig_scene->pig_initial_seen, arrow_pig_scene->arrow_seen,
                       arrow_pig_scene->observed_world_ticks);
                puts("ARROW_PIG_FRAME phase=hit terrain=1 HUD=1 player-health=20 pig-health=6 "
                     "empty-slots=9 source-px=1013 source-uq=2101");
                return 0;
            }
        }
        CHECK(bed_frame(f) == 0);
    }
    return 1;
}

static int arrow_pig_nbt(mc_entity *entity, const char *name) {
    int result = 1;
    bool serialized = false;
    mc_entity_nbt_error *error = calloc(1, sizeof(*error));
    mc_nbt_tree *tree = NULL;
    uint8_t *wire = NULL;
    char path[8192];
    if (error == NULL ||
        mc_entity_save_nbt(entity, NULL, &serialized, &tree, error) != MC_ENTITY_NBT_OK ||
        !serialized)
        goto cleanup;
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    size_t bytes = 0, written = 0;
    mc_nbt_error failure;
    if (mc_nbt_measure(mc_nbt_tree_root(tree), &limits, &bytes, &failure) != MC_NBT_OK ||
        bytes > 1048576 || (wire = malloc(bytes)) == NULL ||
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

static int arrow_pig_pause_save(fixture *f) {
    bed_scene->phase = "SDK-Escape";
    mc_client_screen_transition_state transitions;
    mc_client_world_load_state load;
    CHECK(repeat_focus_state(f, bed_scene->phase) == 0 &&
          mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
          transitions.world_load != NULL &&
          mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
          load.world == arrow_pig_scene->world && load.player == f->app->h);
    const size_t screens_before = transitions.screens, keyboard_before = f->state.keyboard_events;
    CHECK(push_escape(f, true) == 0 && push_escape(f, false) == 0 && transfer(f) == 0);
    for (size_t i = 0; i < 64 && SDL_GetTicks() < bed_scene->deadline && f->app->r == NULL; ++i)
        CHECK(bed_native_focus(f) == 0 && bed_frame(f) == 0);
    mc_client_source_da *pause = f->app->r;
    mc_client_pause_screen_state paused;
    mc_client_screen_error error;
    CHECK(pause != NULL && pause->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
          atomic_load(&f->app->o) && f->state.keyboard_events == keyboard_before + 2 &&
          mc_client_screen_pause_inspect((mc_client_source_oz *)pause, &paused, &error) ==
              MC_CLIENT_SCREEN_OK &&
          paused.last_initialization_returned &&
          mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
          transitions.screens == screens_before + 1 && arrow_pig_members(f) == 0 &&
          arrow_pig_picture(f, "arrow-pig-paused.png") == 0 &&
          bed_player_nbt(f, "arrow-pig-player-paused.nbt") == 0 &&
          arrow_pig_nbt(arrow_pig_scene->pig, "arrow-pig-npc-paused.nbt") == 0);
    arrow_pig_scene->paused = true;
    puts("ARROW_PIG_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 simulation-paused=1 "
         "player-health=20 pig-health=6 arrow-members=0");
    bed_scene->phase = "SDK-Save-and-Quit";
    CHECK(push_button(f, pause, 1, true) == 0 && push_button(f, pause, 1, false) == 0 &&
          transfer(f) == 0);
    for (size_t i = 0; i < 64 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(bed_native_focus(f) == 0 && bed_frame(f) == 0 &&
              mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
              mc_client_world_load_inspect(transitions.world_load, &load));
        if (load.leave.started)
            break;
        CHECK(f->app->r == pause && !f->app->N && atomic_load(&f->app->o));
    }
    CHECK(load.leave.calls == 1 && load.leave.started && load.leave.source_returned &&
          !load.leave.failed && load.leave.pc == 366 && load.leave.i_cleared &&
          load.leave.f_cleared && load.leave.h_cleared && load.leave.failure != NULL &&
          load.leave.failure->status == MC_CLIENT_WORLD_LOAD_OK && load.leave.saving != NULL &&
          load.leave.saving->status == MC_CHUNK_LIFECYCLE_OK && f->app->i == NULL &&
          f->app->h == NULL && f->app->active_world == NULL && f->app->r != NULL &&
          f->app->r != pause && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          transitions.screens == screens_before + 2 && bed_native_focus(f) == 0 &&
          bed_frame(f) == 0 && arrow_pig_picture(f, "arrow-pig-title.png") == 0);
    arrow_pig_scene->pig = NULL;
    arrow_pig_scene->world = NULL;
    puts("ARROW_PIG_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Pig=1 new-title=1");
    return 0;
}

static int arrow_pig_run(fixture *f) {
    bed_scene->phase = "loading";
    CHECK(arrow_pig_monitor(f) == 0 && repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0);
    puts("ARROW_PIG_INPUTS chunks=441 glowstone-y=63 saved-Pig=1 saved-Arrow=1 time=13000 "
         "player-health=20 pig-health=10 pre-bootstrap-only=1");
    CHECK(bed_choose(f) == 0 && arrow_pig_hit_frame(f) == 0 && arrow_pig_pause_save(f) == 0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "loaded-arrow-pig") != 0)
        return 2;
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 2;
    bed_placement_work scene = {
        .directory = argv[3], .phase = "bootstrap", .deadline = SDL_GetTicks() + 210000};
    arrow_pig_work work = {.fixture = f, .valid = true};
    bed_scene = &scene;
    arrow_pig_scene = &work;
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
            result = arrow_pig_run(f);
        }
    }
    if (result != 0 && f->app != NULL)
        native_focus_receipt(f, scene.phase);
    work.paused = true;
    work.pig = NULL;
    work.world = NULL;
    if (f->app != NULL && scene.right_down)
        (void)bed_button(f, true, false);
    if (f->app != NULL && scene.left_down)
        (void)bed_button(f, false, false);
    const uint64_t deadline = SDL_GetTicks() + 20000;
    while (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop) &&
           SDL_GetTicks() < deadline)
        SDL_Delay(1);
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
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
        printf("ARROW_PIG_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    arrow_pig_scene = NULL;
    bed_scene = NULL;
    free(f);
    return result;
}
