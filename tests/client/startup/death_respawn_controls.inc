#if defined(MC_TEST_BED_RESPAWN)
#include "bed_respawn_controls.inc"
#else
#include "client/gui/source_death_screen.h"
#include "client/world/respawn.h"
#include "entity/player/player_internal.h"
#include "mc/world/runtime/world_access.h"

typedef struct respawn_publication_failure {
    client_application *app;
    uintptr_t prior;
    size_t selected;
} respawn_publication_failure;

static mc_world_storage_status respawn_scene_added(void *context, mc_world_runtime *world,
                                                   mc_entity *entity,
                                                   mc_world_storage_error *error) {
    respawn_publication_failure *failure = context;
    (void)world;
    if (entity == failure->app->h && (uintptr_t)entity != failure->prior &&
        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        ++failure->selected;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Authored Respawn publication refusal", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status respawn_scene_removed(void *context, mc_world_runtime *world,
                                                     mc_entity *entity,
                                                     mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)entity;
    (void)error;
    return MC_WORLD_STORAGE_OK;
}

static int death_respawn_controls(fixture *f, const char *screenshot) {
    mc_world_runtime *world = f->app->active_world;
    mc_client_screen_transition_state transition;
    CHECK(world != NULL && f->app->h != NULL && f->app->r == NULL &&
          mc_client_menu_loop_retire_completed_draws(f->loop));
    CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &transition) &&
          transition.world_load != NULL);
    mc_client_world_load_call *load = (mc_client_world_load_call *)transition.world_load;
    mc_entity_effect_error *effect = calloc(1, sizeof(*effect));
    CHECK(effect != NULL);
    for (size_t cycle = 0; cycle < 2; ++cycle) {
        mc_entity *previous = f->app->h;
        const int32_t id = previous->base.id;
        CHECK(mc_entity_retain(previous) == MC_ENTITY_OK);
        bool damaged = false;
        CHECK(mc_entity_effect_damage_with_math(previous, f->app->registry,
                                                mc_client_bootstrap_math(f->bootstrap), NULL, 100,
                                                &damaged, effect) == MC_ENTITY_OK &&
              damaged && effect->player_death.returned &&
              previous->local_player->fields.living.Y <= 0);
        const uint64_t death_deadline = SDL_GetTicks() + 10000;
        do {
            CHECK(frame(f) == 0);
        } while (f->app->r == NULL && SDL_GetTicks() < death_deadline);
        CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_DEATH &&
              f->state.render_returned && !f->state.failed && !atomic_load(&f->app->o));
        mc_client_source_da *death = f->app->r;
        char death_capture[8192];
        const int written =
            snprintf(death_capture, sizeof(death_capture), "%s-death-%zu.png", screenshot, cycle);
        CHECK(written > 0 && (size_t)written < sizeof(death_capture) &&
              capture(f, death_capture) == 0);
        mc_client_screen_state screen;
        CHECK(mc_client_screen_inspect(death, &screen, NULL) == MC_CLIENT_SCREEN_OK &&
              screen.kind == MC_CLIENT_SCREEN_DEATH && screen.da.c == 427 && screen.da.d == 240);
        CHECK(push_button(f, death, 1, true) == 0 && transfer(f) == 0);
        const uint64_t respawn_deadline = SDL_GetTicks() + 60000;
        do {
            CHECK(frame(f) == 0);
        } while (f->app->r == death && SDL_GetTicks() < respawn_deadline);
        const bool replaced =
            f->app->r == NULL && f->app->h != previous && f->app->i == f->app->h &&
            f->app->active_world == world && f->app->h->base.id == id &&
            f->app->h->local_player->fields.living.Y == 20 &&
            f->app->h->local_player->fields.input != NULL &&
            f->app->h->local_player->fields.player.c != previous->local_player->fields.player.c;
        if (!replaced) {
            native_focus_receipt(f, "death-respawn-replacement");
            mc_client_screen_transition_state failed_transition = {0};
            (void)mc_client_screen_transitions_inspect(f->app->screen_transitions,
                                                       &failed_transition);
            fprintf(
                stderr,
                "DEATH_RESPAWN_FAILURE cycle=%zu screen=%d same-death=%d "
                "same-player=%d view-player=%d same-world=%d id=%d expected-id=%d "
                "health=%d input=%d same-inventory=%d focused=%d source-pc=%" PRId64 " "
                "respawn-live=%d completed=%d transition-status=%d\n",
                cycle, f->app->r == NULL ? -1 : (int)f->app->r->kind, f->app->r == death,
                f->app->h == previous, f->app->i == f->app->h, f->app->active_world == world,
                (int)f->app->h->base.id, (int)id, (int)f->app->h->local_player->fields.living.Y,
                f->app->h->local_player->fields.input != NULL,
                f->app->h->local_player->fields.player.c == previous->local_player->fields.player.c,
                f->app->N, f->state.source_pc, failed_transition.respawn != NULL,
                failed_transition.completed_respawn != NULL,
                failed_transition.failure == NULL ? -1 : (int)failed_transition.failure->status);
        }
        CHECK(replaced);
        CHECK(
            mc_client_screen_transitions_inspect(f->app->screen_transitions, &transition) &&
            transition.world_load == load && transition.respawn == NULL &&
            transition.completed_respawn != NULL && transition.completed_respawn->source_returned &&
            transition.completed_respawn->wrapper_returned &&
            transition.completed_respawn->player_stored &&
            transition.completed_respawn->input_stored && transition.completed_respawn->id_stored &&
            transition.completed_respawn->terrain_returned &&
            transition.completed_respawn->death_screen_closed);
        CHECK(push_button(f, death, 1, false) == 0 && transfer(f) == 0 && frame(f) == 0);
        CHECK(f->state.render_returned && !f->state.failed && f->app->r == NULL && f->app->N &&
              f->app->h->local_player->fields.living.Y == 20);
        printf("DEATH_RESPAWN cycle=%zu direct-fatal-body=1 actual-health-tick=1 screen=ch "
               "respawn-button=1 new-player=1 new-inventory=1 same-world=1 same-id=1 health=20 "
               "input=1 terrain=289 source-return=372 wrapper-return=1 gui-return=30 "
               "child-retired=1\n",
               cycle);
        mc_entity_release(previous);
        CHECK(mc_client_menu_loop_retire_completed_draws(f->loop));
    }
    CHECK(capture(f, screenshot) == 0);
    /* A real WorldAccess refusal occurs after h206/i222 and before input321.
     * This native failure is not declared equivalent to a Java Throwable. */
    respawn_publication_failure failure = {.app = f->app, .prior = (uintptr_t)f->app->h};
    const mc_world_access_callbacks callbacks = {.entity_added = respawn_scene_added,
                                                 .entity_removed = respawn_scene_removed};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &failure, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    mc_client_respawn_call *call = NULL;
    mc_client_respawn_error error;
    CHECK(mc_client_respawn_begin(f->app, load, false, 0, &call, &error) ==
              MC_CLIENT_RESPAWN_CHILD_FAILED &&
          call != NULL && failure.selected == 1 && error.site.pc == 303 &&
          error.player_load != NULL);
    mc_client_respawn_state before, after;
    CHECK(mc_client_respawn_inspect(call, &before) && before.player_stored && before.view_stored &&
          !before.input_stored && !before.source_returned &&
          f->app->h->local_player->fields.input == NULL);
    mc_client_respawn_call *same = call;
    CHECK(mc_client_respawn_player_begin(f->app->h, load, &call, &error) ==
              MC_CLIENT_RESPAWN_ARGUMENT &&
          call == same && mc_client_respawn_inspect(call, &after) &&
          before.wrapper_entered == after.wrapper_entered &&
          before.wrapper_returned == after.wrapper_returned &&
          before.player_identity == after.player_identity && before.failure == after.failure &&
          after.failure->site.pc == 303 && failure.selected == 1);
    CHECK(mc_client_respawn_retire(&call, &error) == MC_CLIENT_RESPAWN_OK && call == NULL);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    puts("DEATH_RESPAWN partial-publication=1 source-pc=303 new-player-input-null=1 "
         "rejected-output-preserved=1 callback-cause=native-io pending-call-retired=1 "
         "native-cleanup-next=1");
    free(effect);
    /* The actual health transition left a borrowed display receipt in the
     * loop. End that borrow before terminal dispatchers can consume its owner;
     * the common checked cleanup below still owns the live Loop/App graph. */
    mc_client_menu_loop_state terminal;
    CHECK(mc_client_menu_loop_inspect(f->loop, &terminal) && terminal.display_call != NULL);
    CHECK(mc_client_menu_loop_terminal_prepare(f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &terminal) && terminal.terminal_prepared &&
          terminal.display_call == NULL);
    puts("DEATH_RESPAWN borrowed-display-before=1 terminal-prefix=1 "
         "borrowed-display-cleared=1 common-cleanup-next=1");
    return 0;
}
#endif
