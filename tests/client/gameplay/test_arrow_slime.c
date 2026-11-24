/* Saved size2 Slime/Arrow, ordinary Timer/World ticks, delayed removal and
 * SDK save. Live observations never call damage, K, tick or repair fields. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool arrow_slime_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame arrow_slime_observed_frame
#define MC_BED_PLACEMENT_ENTRY mc_arrow_slime_included_bed_main
#include "test_bed_placement.c"
#undef MC_BED_PLACEMENT_ENTRY
#undef mc_client_menu_loop_frame
#include "mc/entity/arrow/construction.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/nbt/factory.h"
#include "mc/entity/nbt/storage.h"

typedef struct arrow_slime_work {
    fixture *fixture;
    mc_world_runtime *world;
    mc_entity *parent, *children[4]; /* Borrow only current live World members. */
    uintptr_t parent_identity, arrow_identity, child_identities[4];
    uint64_t initial_tick, zero_tick, split_tick;
    int32_t first_death_age, greatest_death_age;
    bool valid, parent_seen, initial_pair_seen, zero_seen, split_seen, paused;
} arrow_slime_work;

static arrow_slime_work *arrow_slime_scene;

/* Bounded passive failure evidence. Every Entity is read from the current
 * World list while that World still owns it; no expired parent is followed. */
static void arrow_slime_observation_failure(const arrow_slime_work *work,
                                            const mc_client_menu_loop_state *state,
                                            const mc_world_runtime *world) {
    fprintf(stderr,
            "ARROW_SLIME_OBSERVATION_FAILURE ticks=%" PRIu64
            " initial=%u zero=%u split=%u first-age=%d greatest-age=%d members=%zu\n",
            state->world_tick_returns, (unsigned int)work->initial_pair_seen,
            (unsigned int)work->zero_seen, (unsigned int)work->split_seen, work->first_death_age,
            work->greatest_death_age, world->fields.b->count);
    for (size_t i = 0; i < world->fields.b->count && i < 8; ++i) {
        const mc_entity *entity = world->fields.b->items[i];
        if (entity == NULL) {
            fprintf(stderr, "ARROW_SLIME_CURRENT index=%zu null=1\n", i);
            continue;
        }
        mc_mob_state mob = {0};
        mc_arrow_state arrow = {0};
        const bool has_mob = mc_mob_inspect(entity, &mob);
        const bool has_arrow = mc_arrow_inspect(entity, &arrow);
        fprintf(stderr,
                "ARROW_SLIME_CURRENT index=%zu identity=%" PRIuPTR " kind=%u phase=%u "
                "position=%.17g,%.17g,%.17g be=%u source=%u mob=%u health=%d size=%d "
                "dying=%u age=%d arrow=%u ground=%u flight=%d shooter=%" PRIuPTR "\n",
                i, (uintptr_t)entity, (unsigned int)entity->class_literal->kind,
                (unsigned int)entity->phase, entity->base.x, entity->base.y, entity->base.z,
                (unsigned int)entity->base.be,
                (unsigned int)(mc_entity_storage_source(entity) != NULL), (unsigned int)has_mob,
                mob.living.Y, (int)mob.watcher_16, (unsigned int)mob.living.ah, mob.living.ad,
                (unsigned int)has_arrow, (unsigned int)arrow.fields.i, arrow.fields.k,
                (uintptr_t)arrow.fields.c);
    }
}

static void arrow_slime_observe(arrow_slime_work *work, const mc_client_menu_loop_state *state) {
    fixture *f = work->fixture;
    mc_world_runtime *world = f->app->active_world;
    if (work->paused || world == NULL || f->app->h == NULL || world->fields.b == NULL)
        return;
    size_t slimes = 0, children = 0, arrows = 0, players = 0, others = 0;
    mc_entity *parent = NULL;
    mc_entity *current_children[4] = {0};
    bool arrow_valid = true;
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
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SLIME)) {
            ++slimes;
            mc_mob_state mob = {0};
            if (!mc_mob_inspect(entity, &mob) || mob.phase != MC_ENTITY_READY ||
                !mob.watcher_16_present) {
                work->valid = false;
                continue;
            }
            if ((uintptr_t)entity == work->parent_identity ||
                (work->parent_identity == 0 && mc_entity_storage_source(entity) != NULL)) {
                work->valid &= parent == NULL && mob.watcher_16 == 2 &&
                               (mob.living.Y == 4 || mob.living.Y == 0) && !entity->base.be;
                parent = entity;
            } else {
                work->valid &= children < 4 && mob.watcher_16 == 1 && mob.living.Y == 1 &&
                               !mob.living.ah && !entity->base.be &&
                               mc_entity_storage_source(entity) == NULL;
                if (children < 4)
                    current_children[children] = entity;
                ++children;
            }
        } else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ARROW)) {
            ++arrows;
            mc_arrow_state arrow = {0};
            arrow_valid &= mc_arrow_inspect(entity, &arrow) && arrow.phase == MC_ENTITY_READY &&
                           arrow.fields.c == NULL && arrow.fields.i == 0 &&
                           mc_entity_storage_source(entity) != NULL;
            work->arrow_identity =
                (uintptr_t)entity; /* VALUE only; never dereferenced after removal. */
        } else
            ++others;
    }
    if (slimes == 0 && !work->parent_seen)
        return; /* Loading has not yet activated the saved subject's Chunk. */
    work->valid &= players == 1 && others == 0 && arrows <= 1 && arrow_valid &&
                   (work->world == NULL || work->world == world);
    if (!work->valid) {
        arrow_slime_observation_failure(work, state, world);
        return;
    }
    work->world = world;
    /* The previous borrowed parent can already be retired. Search the actual
     * live list by VALUE identity; never dereference that expired pointer. */
    work->parent = parent;
    if (parent != NULL) {
        mc_mob_state mob = {0};
        work->valid &= slimes == 1 && children == 0 && mc_mob_inspect(parent, &mob);
        if (!work->valid) {
            arrow_slime_observation_failure(work, state, world);
            return;
        }
        work->parent_seen = true;
        work->parent_identity = (uintptr_t)parent;
        if (mob.living.Y == 4) {
            work->valid &= !work->zero_seen && !mob.living.ah && mob.living.ad == 0;
            if (arrows == 1 && !work->initial_pair_seen) {
                work->initial_pair_seen = true;
                work->initial_tick = state->world_tick_returns;
                puts("ARROW_SLIME_INITIAL live-parent-health=4 size=2 live-Arrow=1 receipt-only=0");
            }
        } else {
            work->valid &= work->initial_pair_seen && mob.living.ah && mob.living.ad >= 0 &&
                           mob.living.ad <= 20 && arrows == 0;
            if (!work->zero_seen) {
                work->zero_seen = true;
                work->zero_tick = state->world_tick_returns;
                work->first_death_age = mob.living.ad;
                printf("ARROW_SLIME_ZERO live-parent-health=0 dying=1 death-age=%d "
                       "arrow-members=0 world-ticks=%" PRIu64 "\n",
                       mob.living.ad, work->zero_tick);
            }
            work->valid &= mob.living.ad >= work->greatest_death_age;
            work->greatest_death_age = mob.living.ad;
        }
    } else {
        work->valid &=
            work->parent_seen && work->zero_seen && children == 4 && slimes == 4 && arrows == 0 &&
            state->world_tick_returns >= work->zero_tick &&
            state->world_tick_returns - work->zero_tick >= (uint64_t)(21 - work->first_death_age);
        for (size_t i = 0; i < 4; ++i) {
            work->valid &= current_children[i] != NULL &&
                           (work->child_identities[i] == 0 ||
                            work->child_identities[i] == (uintptr_t)current_children[i]);
            work->children[i] = current_children[i];
            work->child_identities[i] = (uintptr_t)current_children[i];
            for (size_t j = 0; j < i; ++j)
                work->valid &= work->child_identities[i] != work->child_identities[j];
        }
        if (work->valid && !work->split_seen) {
            work->split_seen = true;
            work->split_tick = state->world_tick_returns;
            printf("ARROW_SLIME_SPLIT parent-absent=1 children=4 child-size=1 child-health=1 "
                   "arrow-members=0 world-ticks=%" PRIu64 "\n",
                   work->split_tick);
        }
    }
    if (!work->valid)
        arrow_slime_observation_failure(work, state, world);
}

static bool arrow_slime_observed_frame(mc_client_menu_loop *loop) {
    arrow_slime_work *work = arrow_slime_scene;
    if (work != NULL && work->fixture != NULL && work->fixture->loop == loop) {
        mc_client_menu_loop_state before;
        if (mc_client_menu_loop_inspect(loop, &before))
            arrow_slime_observe(work, &before);
    }
    const bool returned = mc_client_menu_loop_frame(loop);
    if (work != NULL && work->fixture != NULL && work->fixture->loop == loop) {
        mc_client_menu_loop_state state;
        if (mc_client_menu_loop_inspect(loop, &state)) {
            arrow_slime_observe(work, &state);
            if (!returned) {
                work->fixture->state = state;
                creature_tick_diagnostics(work->fixture);
                fprintf(stderr,
                        "ARROW_SLIME_FAILURE phase=%s source=%s.%s:%" PRId64 " operation=%s\n",
                        bed_scene->phase, state.source_class == NULL ? "none" : state.source_class,
                        state.source_method == NULL ? "none" : state.source_method, state.source_pc,
                        state.operation == NULL ? "none" : state.operation);
            }
        }
    }
    return returned;
}

static int arrow_slime_monitor(fixture *f) {
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
        printf("ARROW_SLIME_MONITOR window=%s x=%d y=%d width=%d height=%d target=%s\n",
               i == 0 ? "frame" : "canvas", wx, wy, ww, wh, bounds);
    }
    return 0;
}

static int arrow_slime_player(fixture *f) {
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

static int arrow_slime_members(fixture *f) {
    arrow_slime_work *work = arrow_slime_scene;
    CHECK(work->valid && work->initial_pair_seen && work->zero_seen && work->split_seen &&
          work->parent_seen && work->parent == NULL && work->parent_identity != 0 &&
          work->world == f->app->active_world && f->app->active_world->fields.b->count == 5 &&
          !f->app->active_world->fields.B && f->app->active_world->fields.q == 2);
    size_t slimes = 0, players = 0;
    for (size_t i = 0; i < work->world->fields.b->count; ++i) {
        mc_entity *entity = work->world->fields.b->items[i];
        if (entity == f->app->h)
            ++players;
        else {
            mc_mob_state slime;
            CHECK(slimes < 4 && entity != NULL && entity == work->children[slimes] &&
                  (uintptr_t)entity == work->child_identities[slimes] &&
                  (uintptr_t)entity != work->parent_identity && mc_mob_inspect(entity, &slime) &&
                  slime.kind == MC_ENTITY_CLASS_KIND_SLIME && slime.watcher_16_present &&
                  slime.watcher_16 == 1 && slime.living.Y == 1 && !slime.living.ah &&
                  !entity->base.be && entity->storage_source == NULL);
            ++slimes;
        }
    }
    CHECK(slimes == 4 && players == 1 && arrow_slime_player(f) == 0);
    return 0;
}

static int arrow_slime_picture(fixture *f, const char *name) {
    char path[8192];
    CHECK(bed_path(name, path) == 0 && capture(f, path) == 0);
    return 0;
}

static int arrow_slime_hit_frame(fixture *f) {
    bed_scene->phase = "natural-arrow-slime-split";
    for (size_t i = 0; i < 4000 && SDL_GetTicks() < bed_scene->deadline; ++i) {
        CHECK(arrow_slime_scene->valid && repeat_focus_state(f, bed_scene->phase) == 0);
        if (arrow_slime_scene->split_seen) {
            const mc_client_world_frame_state *draw = f->state.world_frame;
            const mc_client_gui_ingame_draw_state *hud = f->state.hud;
            CHECK(arrow_slime_members(f) == 0);
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
                      arrow_slime_picture(f, "arrow-slime-split.png") == 0);
                puts("ARROW_SLIME_CAUSAL saved-parent-health=4 observed-health=0 parent-absent=1 "
                     "children=4 arrow-members=0 ordinary-world-ticks=1 direct741-receipt=0 "
                     "direct-K-receipt=0 owned-Mob-environment=0");
                printf("ARROW_SLIME_OBSERVE initial-tick=%" PRIu64 " zero-tick=%" PRIu64
                       " split-tick=%" PRIu64 " first-death-age=%d greatest-death-age=%d\n",
                       arrow_slime_scene->initial_tick, arrow_slime_scene->zero_tick,
                       arrow_slime_scene->split_tick, arrow_slime_scene->first_death_age,
                       arrow_slime_scene->greatest_death_age);
                puts("ARROW_SLIME_FRAME phase=split terrain=1 HUD=1 player-health=20 children=4 "
                     "child-health=1 child-size=1 "
                     "empty-slots=9 source-px=1013 source-uq=2101");
                return 0;
            }
        }
        CHECK(bed_frame(f) == 0);
    }
    return 1;
}

static int arrow_slime_nbt(mc_entity *entity, const char *name) {
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

static int arrow_slime_pause_save(fixture *f) {
    bed_scene->phase = "SDK-Escape";
    mc_client_screen_transition_state transitions;
    mc_client_world_load_state load;
    CHECK(repeat_focus_state(f, bed_scene->phase) == 0 &&
          mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
          transitions.world_load != NULL &&
          mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
          load.world == arrow_slime_scene->world && load.player == f->app->h);
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
          transitions.screens == screens_before + 1 && arrow_slime_members(f) == 0 &&
          arrow_slime_picture(f, "arrow-slime-paused.png") == 0 &&
          bed_player_nbt(f, "arrow-slime-player-paused.nbt") == 0);
    for (size_t i = 0; i < 4; ++i) {
        char name[96];
        CHECK(snprintf(name, sizeof(name), "arrow-slime-child-%zu-paused.nbt", i) > 0 &&
              arrow_slime_nbt(arrow_slime_scene->children[i], name) == 0);
    }
    arrow_slime_scene->paused = true;
    puts("ARROW_SLIME_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 simulation-paused=1 "
         "player-health=20 children=4 child-health=1 child-size=1 arrow-members=0 parent-absent=1");
    bed_scene->phase = "SDK-Save-and-Quit";
    /* All subject borrows expire before the real leave can retire World lists. */
    arrow_slime_scene->parent = NULL;
    for (size_t i = 0; i < 4; ++i)
        arrow_slime_scene->children[i] = NULL;
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
          bed_frame(f) == 0 && arrow_slime_picture(f, "arrow-slime-title.png") == 0);
    arrow_slime_scene->world = NULL;
    puts("ARROW_SLIME_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Slime-children=4 "
         "saved-parent=0 saved-Arrow=0 new-title=1");
    return 0;
}

static int arrow_slime_run(fixture *f) {
    bed_scene->phase = "loading";
    CHECK(arrow_slime_monitor(f) == 0 && repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0);
    puts("ARROW_SLIME_INPUTS chunks=625 glowstone-y=63 saved-Slime=1 saved-Arrow=1 time=13000 "
         "player-health=20 parent-health=4 parent-size=2 parent-NBT-Size=1 pre-bootstrap-only=1");
    CHECK(bed_choose(f) == 0 && arrow_slime_hit_frame(f) == 0 && arrow_slime_pause_save(f) == 0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "loaded-arrow-slime") != 0)
        return 2;
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 2;
    bed_placement_work scene = {
        .directory = argv[3], .phase = "bootstrap", .deadline = SDL_GetTicks() + 210000};
    arrow_slime_work work = {.fixture = f, .valid = true};
    bed_scene = &scene;
    arrow_slime_scene = &work;
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
            result = arrow_slime_run(f);
        }
    }
    if (result != 0 && f->app != NULL)
        native_focus_receipt(f, scene.phase);
    work.paused = true;
    work.parent = NULL;
    for (size_t i = 0; i < 4; ++i)
        work.children[i] = NULL;
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
        printf("ARROW_SLIME_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    arrow_slime_scene = NULL;
    bed_scene = NULL;
    free(f);
    return result;
}
