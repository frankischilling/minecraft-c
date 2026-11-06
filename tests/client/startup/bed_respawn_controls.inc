#include "client/gui/gui_ingame_chat_internal.h"
#include "client/gui/source_death_screen.h"
#include "client/world/respawn.h"
#include "entity/player/player_internal.h"
#include "mc/entity/player/coordinates.h"
#include "mc/world/runtime/saving/saving.h"

static void bed_respawn_failure(fixture *f, const char *stage, size_t cycle) {
    native_focus_receipt(f, stage);
    mc_client_screen_transition_state t = {0};
    (void)mc_client_screen_transitions_inspect(f->app->screen_transitions, &t);
    const mc_client_respawn_state *r = t.completed_respawn;
    mc_client_respawn_state live = {0};
    if (r == NULL && t.respawn != NULL && mc_client_respawn_inspect(t.respawn, &live))
        r = &live;
    mc_entity *p = f->app->h;
    fprintf(stderr,
            "BED_RESPAWN_FAILURE stage=%s cycle=%zu screen=%d player=%" PRIuPTR " view=%" PRIuPTR
            " world=%" PRIuPTR " health=%d focused=%d source-pc=%" PRId64
            " live=%d completed=%d returned=%d wrapper=%d saved-read=%d checked=%d valid=%d "
            "notified=%d stored=%d positioned=%d failure=%d pc=%d\n",
            stage, cycle, f->app->r == NULL ? -1 : (int)f->app->r->kind, (uintptr_t)p,
            (uintptr_t)f->app->i, (uintptr_t)f->app->active_world,
            p == NULL || p->local_player == NULL ? -1 : (int)p->local_player->fields.living.Y,
            f->app->N, f->state.source_pc, t.respawn != NULL, t.completed_respawn != NULL,
            r != NULL && r->source_returned, r != NULL && r->wrapper_returned,
            r != NULL && r->saved_spawn_read, r != NULL && r->bed_checked,
            r != NULL && r->bed_valid, r != NULL && r->bed_notified, r != NULL && r->bed_stored,
            r != NULL && r->bed_positioned,
            r == NULL || r->failure == NULL ? -1 : (int)r->failure->status,
            r == NULL || r->failure == NULL ? -1 : r->failure->site.pc);
}

static bool bed_respawn_observed(fixture *f, const char *stage, size_t cycle, bool condition) {
    if (!condition)
        bed_respawn_failure(f, stage, cycle);
    return condition;
}

static int bed_respawn_frame(fixture *f, const char *stage, size_t cycle) {
    const int result = frame(f);
    if (result != 0)
        bed_respawn_failure(f, stage, cycle);
    return result;
}

static int bed_respawn_capture(fixture *f, const char *screenshot, const char *stage,
                               size_t cycle) {
    char path[8192];
    const int count = snprintf(path, sizeof(path), "%s-%s-%zu.png", screenshot, stage, cycle);
    CHECK(count > 0 && (size_t)count < sizeof(path) && capture(f, path) == 0);
    return 0;
}

static int bed_respawn_notice(fixture *f) {
    mc_client_source_uq *hud = f->app->v;
    CHECK(bed_respawn_observed(f, "invalid-Bed-message", 0,
                               hud != NULL && hud->fields.e != NULL && hud->fields.e->size == 1 &&
                                   hud->fields.e->entries != NULL));
    mc_gui_chat_message *message = mc_gui_chat_message_find(hud, hud->fields.e->entries[0]);
    mc_player_string_view text = {0};
    CHECK(bed_respawn_observed(f, "invalid-Bed-text", 0,
                               message != NULL && mc_gui_chat_text_read(message->a, &text) &&
                                   text.length != 0 && text.length <= 320 && message->b >= 0 &&
                                   message->b < 200));
    printf("BED_RESPAWN_NOTICE age=%" PRId32 " utf16=", message->b);
    for (size_t i = 0; i < text.length; ++i)
        printf("%04x", (unsigned int)text.units[i]);
    putchar('\n');
    return 0;
}

static int death_respawn_controls(fixture *f, const char *screenshot) {
    const char *profile = SDL_getenv("MC_TEST_BED_RESPAWN_PROFILE");
    CHECK(profile != NULL && (strcmp(profile, "valid") == 0 || strcmp(profile, "missing") == 0));
    const bool valid = strcmp(profile, "valid") == 0;
    const size_t cycles = valid ? 2u : 1u;
    mc_world_runtime *world = f->app->active_world;
    mc_client_screen_transition_state transition;
    CHECK(bed_respawn_observed(f, "initial-loaded-graph", 0,
                               world != NULL && f->app->h != NULL && f->app->r == NULL &&
                                   mc_client_menu_loop_retire_completed_draws(f->loop)));
    CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &transition) &&
          transition.world_load != NULL && f->app->v != NULL && f->app->v->fields.e != NULL &&
          f->app->v->fields.e->size == 0);
    mc_client_world_load_call *load = (mc_client_world_load_call *)transition.world_load;
    mc_entity_effect_error *effect = calloc(1, sizeof(*effect));
    CHECK(effect != NULL && bed_respawn_capture(f, screenshot, "before", 0) == 0);
    printf("BED_RESPAWN_INPUT profile=%s player=pre-bootstrap-nbt saved-spawn=3,64,4 "
           "Bed=pre-bootstrap-region direct-fatal-body=component-input cycles=%zu\n",
           profile, cycles);
    for (size_t cycle = 0; cycle < cycles; ++cycle) {
        mc_entity *previous = f->app->h;
        const int32_t id = previous->base.id;
        const uintptr_t old_inventory = (uintptr_t)previous->local_player->fields.player.c;
        const uintptr_t old_input = (uintptr_t)previous->local_player->fields.input;
        CHECK(mc_entity_retain(previous) == MC_ENTITY_OK);
        mc_player_coordinate_pin *pin = NULL;
        mc_player_error coordinate = {0};
        CHECK(mc_player_saved_spawn_get(previous, &pin, &coordinate) == MC_PLAYER_OK);
        const mc_player_coordinates *saved = mc_player_coordinate_pin_value(pin);
        CHECK(bed_respawn_observed(f, "old-Q", cycle,
                                   saved != NULL && saved->x == 3 && saved->y == 64 &&
                                       saved->z == 4 && old_inventory != 0 && old_input != 0));
        const uintptr_t old_coordinate = (uintptr_t)saved;
        /* Only the old Player is retained for identity comparisons. End this
         * observing Q pin before respawn, so the coordinator's own saved-Bed
         * coordinate lifetime guard remains necessary. */
        mc_player_coordinate_pin_release(&pin);
        CHECK(pin == NULL);
        bool damaged = false;
        CHECK(mc_entity_effect_damage_with_math(previous, f->app->registry,
                                                mc_client_bootstrap_math(f->bootstrap), NULL, 100,
                                                &damaged, effect) == MC_ENTITY_OK &&
              damaged && effect->player_death.returned &&
              previous->local_player->fields.living.Y <= 0);
        CHECK(mc_entity_effect_error_retire(effect));
        const uint64_t death_deadline = SDL_GetTicks() + 10000;
        do {
            CHECK(bed_respawn_frame(f, "death-frame", cycle) == 0);
        } while (f->app->r == NULL && SDL_GetTicks() < death_deadline);
        const bool death_paused = atomic_load(&f->app->o);
        CHECK(bed_respawn_observed(f, "death-screen", cycle,
                                   f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_DEATH &&
                                       f->state.render_returned && !f->state.failed &&
                                       !death_paused));
        mc_client_source_da *death = f->app->r;
        CHECK(bed_respawn_capture(f, screenshot, "death", cycle) == 0);
        CHECK(push_button(f, death, 1, true) == 0 && transfer(f) == 0);
        const uint64_t deadline = SDL_GetTicks() + 60000;
        do {
            CHECK(bed_respawn_frame(f, "respawn-frame", cycle) == 0);
        } while (f->app->r == death && SDL_GetTicks() < deadline);
        mc_entity *current = f->app->h;
        CHECK(bed_respawn_observed(
            f, "replacement", cycle,
            f->app->r == NULL && current != NULL && current != previous && f->app->i == current &&
                f->app->active_world == world && current->base.id == id &&
                current->local_player != NULL && current->local_player->fields.living.Y == 20 &&
                current->local_player->fields.input != NULL &&
                (uintptr_t)current->local_player->fields.input != old_input &&
                (uintptr_t)current->local_player->fields.player.c != old_inventory));
        CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &transition));
        const mc_client_respawn_state *r = transition.completed_respawn;
        CHECK(bed_respawn_observed(
            f, "completed-prefix", cycle,
            transition.world_load == load && transition.respawn == NULL && r != NULL &&
                r->source_returned && r->wrapper_returned && r->old_player_removed &&
                r->player_created && r->player_stored && r->view_stored && r->spawn_returned &&
                r->prepared && r->player_inserted && r->input_stored && r->id_stored &&
                r->player_finished && r->terrain_returned && r->death_screen_closed &&
                r->saved_spawn_read && r->bed_checked && r->bed_valid == valid &&
                r->bed_notified == !valid && r->bed_stored == valid && r->bed_positioned == valid &&
                r->saved_bed_identity == old_coordinate && (r->bed_exit_identity != 0) == valid));
        CHECK(mc_player_saved_spawn_get(current, &pin, &coordinate) == MC_PLAYER_OK);
        const mc_player_coordinates *copy = mc_player_coordinate_pin_value(pin);
        CHECK(bed_respawn_observed(
            f, "new-Q", cycle,
            valid ? copy != NULL && (uintptr_t)copy != old_coordinate && copy->x == 3 &&
                        copy->y == 64 && copy->z == 4 && current->base.x >= 1.5 &&
                        current->base.x <= 4.5 && current->base.z >= 1.5 &&
                        current->base.z <= 6.5 && current->bounds != NULL &&
                        current->bounds->min_y >= 64.0 && current->bounds->min_y < 65.0
                  : copy == NULL));
        mc_player_coordinate_pin_release(&pin);
        printf("BED_RESPAWN cycle=%zu profile=%s saved-read=1 checked=1 valid=%d notified=%d "
               "stored=%d positioned=%d Q=%s new-player=1 new-inventory=1 new-input=1 "
               "same-world=1 same-id=1 health=20 source-return=372 wrapper-return=1 "
               "terrain=289 old-coordinate-test-pin=0\n",
               cycle, profile, valid, !valid, valid, valid, valid ? "distinct-3,64,4" : "null");
        CHECK(push_button(f, death, 1, false) == 0 && transfer(f) == 0 &&
              bed_respawn_frame(f, "returned-frame", cycle) == 0);
        CHECK(bed_respawn_observed(f, "returned-live-graph", cycle,
                                   f->state.render_returned && !f->state.failed &&
                                       f->app->r == NULL && f->app->N && f->app->h == current &&
                                       current->local_player->fields.living.Y == 20));
        CHECK(bed_respawn_capture(f, screenshot, "returned", cycle) == 0);
        if (!valid)
            CHECK(bed_respawn_notice(f) == 0);
        else
            CHECK(f->app->v->fields.e->size == 0);
        mc_entity_release(previous);
        CHECK(mc_client_menu_loop_retire_completed_draws(f->loop));
    }
    CHECK(capture(f, screenshot) == 0);
    mc_world_runtime_lifecycle_error *saving = calloc(1, sizeof(*saving));
    CHECK(saving != NULL);
    const mc_chunk_lifecycle_status save = mc_world_runtime_save_detailed(world, true, saving);
    if (save != MC_CHUNK_LIFECYCLE_OK) {
        bed_respawn_failure(f, "direct-source-full-save", cycles);
        fprintf(stderr, "BED_RESPAWN_SAVE_FAILURE status=%d cause=%d operation=%s\n", (int)save,
                (int)saving->cause.status,
                saving->cause.operation == NULL ? "none" : saving->cause.operation);
    }
    CHECK(save == MC_CHUNK_LIFECYCLE_OK);
    free(saving);
    printf("BED_RESPAWN_SAVE profile=%s path=direct-world-full-save SDK-save-menu=0 "
           "live-player=1 source-returned=1\n",
           profile);
    free(effect);
    /* End the health transition's borrowed display receipt before the common
     * checked root/Loop/App terminal tail consumes that receipt's owner. */
    mc_client_menu_loop_state terminal;
    CHECK(mc_client_menu_loop_inspect(f->loop, &terminal) && terminal.display_call != NULL);
    CHECK(mc_client_menu_loop_terminal_prepare(f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &terminal) && terminal.terminal_prepared &&
          terminal.display_call == NULL);
    puts("BED_RESPAWN_TERMINAL borrowed-display-before=1 terminal-prefix=1 "
         "borrowed-display-cleared=1 common-cleanup-next=1");
    return 0;
}
