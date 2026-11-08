/* Pre-bootstrap saved UZ, ordinary Timer/World/AI hit, returned HUD and SDK
 * Escape/Save and Quit. Observation never invokes damage or repairs source. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool zombie_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame zombie_observed_frame
#define MC_BED_PLACEMENT_ENTRY mc_zombie_included_bed_main
#include "test_bed_placement.c"
#undef MC_BED_PLACEMENT_ENTRY
#undef mc_client_menu_loop_frame
#include "mc/entity/nbt/factory.h"

typedef struct zombie_work {
    fixture *fixture;
    mc_world_runtime *world;
    mc_entity *player, *zombie; /* Borrowed; the actual loaded World owns both. */
    bool hit_seen;
    uint64_t hit_calls, hit_returns, hit_true_returns;
    uintptr_t actor_identity, target_identity;
    bool hurt_returned, hurt_result, health_stored, statistic_returned;
    int32_t damage, health_before, health_after, statistic_amount;
} zombie_work;

static zombie_work *zombie_scene;

/* Copy immediately after the real frame; no borrowed Mob/Effect view survives
 * the next tick. A failed or wrong-caller prefix never becomes a hit. */
static void zombie_copy_hit(zombie_work *work, const mc_client_menu_loop_state *state) {
    const mc_world_mob_hurt_observation *observed = &state->mob_hurt;
    const mc_world_mob_hurt_receipt *hit = &observed->last;
    if (work->hit_seen || observed->saturated || !hit->present || !hit->returned || !hit->result ||
        !hit->parent_returned || hit->prior_pending ||
        hit->actor_kind != MC_ENTITY_CLASS_KIND_ZOMBIE || hit->caller_class == NULL ||
        strcmp(hit->caller_class, "gz") != 0 || hit->caller_method == NULL ||
        strcmp(hit->caller_method, "a") != 0 || hit->caller_descriptor == NULL ||
        strcmp(hit->caller_descriptor, "(Lsn;F)V") != 0 || hit->caller_pc != 61 ||
        hit->return_pc != 65)
        return;
    work->hit_seen = true;
    work->hit_calls = observed->reached_calls;
    work->hit_returns = observed->returned_calls;
    work->hit_true_returns = observed->true_returns;
    work->actor_identity = hit->actor_identity;
    work->target_identity = hit->target_identity;
    work->hurt_returned = hit->returned;
    work->hurt_result = hit->result;
    work->health_stored = hit->health_stored;
    work->statistic_returned = hit->statistic_returned;
    work->damage = hit->damage;
    work->health_before = hit->health_before;
    work->health_after = hit->health_after;
    work->statistic_amount = hit->statistic_amount;
}

static bool zombie_observed_frame(mc_client_menu_loop *loop) {
    const bool returned = mc_client_menu_loop_frame(loop);
    zombie_work *work = zombie_scene;
    if (work != NULL && work->fixture != NULL && work->fixture->loop == loop) {
        mc_client_menu_loop_state state;
        if (mc_client_menu_loop_inspect(loop, &state)) {
            zombie_copy_hit(work, &state);
            if (!returned) {
                work->fixture->state = state;
                creature_tick_diagnostics(work->fixture);
                fprintf(stderr,
                        "ZOMBIE_DAMAGE_FAILURE phase=%s source=%s.%s:%" PRId64 " operation=%s\n",
                        bed_scene->phase, state.source_class == NULL ? "none" : state.source_class,
                        state.source_method == NULL ? "none" : state.source_method, state.source_pc,
                        state.operation == NULL ? "none" : state.operation);
            }
        }
    }
    return returned;
}

static int zombie_monitor(fixture *f) {
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
        printf("ZOMBIE_DAMAGE_MONITOR window=%s x=%d y=%d width=%d height=%d target=%s\n",
               i == 0 ? "frame" : "canvas", wx, wy, ww, wh, bounds);
    }
    return 0;
}

static int zombie_player(fixture *f, int32_t health, int32_t statistic) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    mc_registry_statistic_view statistics;
    int32_t taken = -1;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          player.living.Y == health && !player.player.u && player.player.private_a == 0 &&
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
          taken == statistic);
    for (size_t i = 0; i < 36; ++i)
        CHECK(inventory.main_slots[i] == NULL);
    for (size_t i = 0; i < 4; ++i)
        CHECK(inventory.armor_slots[i] == NULL);
    return 0;
}

static int zombie_loaded(fixture *f) {
    mc_world_runtime *world = f->app->active_world;
    mc_entity *found = NULL;
    CHECK(world != NULL && world->fields.b != NULL && world->fields.b->items != NULL &&
          world->fields.b->count == 2 && f->app->h != NULL && f->app->i == f->app->h &&
          !world->fields.B && world->fields.q == 2 && world->fields.N && world->fields.f >= 4);
    for (size_t i = 0; i < world->fields.b->count; ++i) {
        mc_entity *entity = (mc_entity *)world->fields.b->items[i];
        CHECK(entity != NULL && entity->base.world == world && entity->classes == f->app->entities);
        if (entity == f->app->h)
            continue;
        mc_mob_state mob;
        CHECK(found == NULL && mc_mob_inspect(entity, &mob) &&
              mob.kind == MC_ENTITY_CLASS_KIND_ZOMBIE && mob.phase == MC_ENTITY_READY &&
              mob.monster_damage == 5 && mob.living.Y == 20 && entity->storage_source != NULL &&
              mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ZOMBIE));
        found = entity;
    }
    CHECK(found != NULL);
    zombie_scene->world = world;
    zombie_scene->player = f->app->h;
    zombie_scene->zombie = found;
    puts("ZOMBIE_DAMAGE_LOAD saved-NBT=1 canonical=uz monster-damage=5 same-world-and-classes=1");
    return 0;
}

static int zombie_picture(fixture *f, const char *name) {
    char path[8192];
    CHECK(bed_path(name, path) == 0 && capture(f, path) == 0);
    return 0;
}

static int zombie_hit_frame(fixture *f) {
    bed_scene->phase = "natural-zombie-hit";
    for (size_t i = 0; i < 4000 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(f->app->active_world == zombie_scene->world && f->app->h == zombie_scene->player &&
              repeat_focus_state(f, bed_scene->phase) == 0);
        if (zombie_scene->hit_seen) {
            mc_mob_state mob;
            CHECK(zombie_scene->hit_calls == 1 && zombie_scene->hit_returns == 1 &&
                  zombie_scene->hit_true_returns == 1 && zombie_scene->hurt_returned &&
                  zombie_scene->hurt_result && zombie_scene->health_stored &&
                  zombie_scene->statistic_returned && zombie_scene->damage == 5 &&
                  zombie_scene->health_before == 20 && zombie_scene->health_after == 15 &&
                  zombie_scene->statistic_amount == 5 &&
                  zombie_scene->actor_identity == (uintptr_t)zombie_scene->zombie &&
                  zombie_scene->target_identity == (uintptr_t)zombie_scene->player &&
                  mc_mob_inspect(zombie_scene->zombie, &mob) &&
                  mob.creature_target == zombie_scene->player && mob.monster_damage == 5 &&
                  zombie_player(f, 15, 5) == 0);
            const mc_client_world_frame_state *draw = f->state.world_frame;
            const mc_client_gui_ingame_draw_state *hud = f->state.hud;
            if (draw != NULL && draw->returned && draw->terrain_returned &&
                draw->updates_returned && draw->source_pc == 1013 && draw->terrain_count > 0 &&
                hud != NULL && hud->returned && hud->source_pc == 2101 && hud->hotbar_returned &&
                hud->slots_returned && hud->chat_returned && hud->health == 15 &&
                hud->selected == 0 && hud->item_slots == 0 && hud->empty_slots == 9) {
                CHECK(f->state.gameplay_left_clicks == 0 && f->state.gameplay_right_clicks == 0 &&
                      f->state.gameplay_mining_starts == 0 && f->state.gameplay_block_uses == 0 &&
                      f->state.gameplay_air_uses == 0 && f->state.nightmare_calls == 0 &&
                      f->state.sleep_wake_calls == 0 && f->state.world_tick_returns > 0 &&
                      f->state.random_chunk_calls > 0 && f->state.random_display_calls > 0 &&
                      zombie_picture(f, "zombie-hit.png") == 0);
                puts("ZOMBIE_DAMAGE_HIT caller=gz.a:61 returned=1 result=1 damage=5 health=20,15 "
                     "statistic=2021 amount=5 calls=1 returns=1 true=1");
                puts("ZOMBIE_DAMAGE_FRAME phase=hit terrain=1 HUD=1 health=15 empty-slots=9 "
                     "source-px=1013 source-uq=2101");
                return 0;
            }
        }
        CHECK(bed_frame(f) == 0);
    }
    return 1;
}

/* Passive independent serialization of the same genuine saved NPC. */
static int zombie_nbt(mc_entity *entity, const char *name) {
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

static int zombie_pause_save(fixture *f) {
    bed_scene->phase = "SDK-Escape";
    mc_client_screen_transition_state transitions;
    mc_client_world_load_state load;
    CHECK(repeat_focus_state(f, bed_scene->phase) == 0 &&
          mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
          transitions.world_load != NULL &&
          mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
          load.world == zombie_scene->world && load.player == zombie_scene->player);
    const size_t screens_before = transitions.screens, keyboard_before = f->state.keyboard_events;
    CHECK(push_escape(f, true) == 0 && push_escape(f, false) == 0 && transfer(f) == 0);
    for (size_t i = 0; i < 64 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(bed_native_focus(f) == 0 && bed_frame(f) == 0);
        if (f->app->r != NULL)
            break;
    }
    mc_client_source_da *pause = f->app->r;
    mc_client_pause_screen_state paused;
    mc_client_screen_error screen_error;
    CHECK(pause != NULL && pause->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
          atomic_load(&f->app->o) && f->state.keyboard_events == keyboard_before + 2 &&
          mc_client_screen_pause_inspect((mc_client_source_oz *)pause, &paused, &screen_error) ==
              MC_CLIENT_SCREEN_OK &&
          paused.last_initialization_returned &&
          mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
          transitions.screens == screens_before + 1 && zombie_player(f, 15, 5) == 0 &&
          !f->state.mob_hurt.saturated && f->state.mob_hurt.reached_calls == 1 &&
          f->state.mob_hurt.returned_calls == 1 && f->state.mob_hurt.true_returns == 1 &&
          zombie_picture(f, "zombie-paused.png") == 0 &&
          bed_player_nbt(f, "zombie-player-paused.nbt") == 0 &&
          zombie_nbt(zombie_scene->zombie, "zombie-npc-paused.nbt") == 0);
    puts("ZOMBIE_DAMAGE_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 "
         "simulation-paused=1 health=15 statistic2021=5");
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
    CHECK(load.world == zombie_scene->world && load.player == zombie_scene->player &&
          load.leave.calls == 1 && load.leave.started && load.leave.source_returned &&
          !load.leave.failed && load.leave.pc == 366 && load.leave.i_cleared &&
          load.leave.f_cleared && load.leave.h_cleared && load.leave.failure != NULL &&
          load.leave.failure->status == MC_CLIENT_WORLD_LOAD_OK && load.leave.saving != NULL &&
          load.leave.saving->status == MC_CHUNK_LIFECYCLE_OK && f->app->i == NULL &&
          f->app->h == NULL && f->app->active_world == NULL && f->app->r != NULL &&
          f->app->r != pause && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          transitions.screens == screens_before + 2 && bed_native_focus(f) == 0 &&
          bed_frame(f) == 0 && f->app->active_world == NULL &&
          f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          zombie_picture(f, "zombie-title.png") == 0);
    puts("ZOMBIE_DAMAGE_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Zombie=1 "
         "new-title=1");
    return 0;
}

static int zombie_run(fixture *f) {
    bed_scene->phase = "loading";
    CHECK(zombie_monitor(f) == 0 && repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0);
    puts("ZOMBIE_DAMAGE_INPUTS chunks=289 glowstone-y=63 saved-Zombie=1 time=13000 difficulty=2 "
         "hostiles=1 player-health=20 zombie-health=20 armor=0 pre-bootstrap-only=1");
    CHECK(bed_choose(f) == 0 && zombie_loaded(f) == 0 && zombie_hit_frame(f) == 0 &&
          zombie_pause_save(f) == 0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "loaded-zombie") != 0)
        return 2;
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 2;
    bed_placement_work scene = {
        .directory = argv[3], .phase = "bootstrap", .deadline = SDL_GetTicks() + 210000};
    zombie_work work = {.fixture = f};
    bed_scene = &scene;
    zombie_scene = &work;
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
            result = zombie_run(f);
        }
    }
    if (result != 0 && f->app != NULL)
        native_focus_receipt(f, scene.phase);
    if (f->app != NULL && scene.right_down)
        (void)bed_button(f, true, false);
    if (f->app != NULL && scene.left_down)
        (void)bed_button(f, false, false);
    const uint64_t cleanup_deadline = SDL_GetTicks() + 20000;
    while (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop) &&
           SDL_GetTicks() < cleanup_deadline)
        SDL_Delay(1);
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
        printf("ZOMBIE_DAMAGE_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    zombie_scene = NULL;
    bed_scene = NULL;
    free(f);
    return result;
}
