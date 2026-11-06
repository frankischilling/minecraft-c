/* The real World call writes its owned failure directly into the client
 * record. This tests native ownership/admission/retirement, not a completed
 * Minecraft.k frame or graphical gameplay. Imported fixture mains do not run. */
int mc_player_sleep_cleanup_fixture_main(int, char **);
#define MC_WORLD_ENTITY_TICK_MAIN mc_player_sleep_cleanup_fixture_main
#include "../../world/runtime/test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN
#include "../../../src/client/runtime/world_tick.h"
#include "mc/core/math/classes.h"

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    client_application *app = NULL;
    mc_client_world_tick *call = NULL;
    mc_entity *player = NULL;
    mc_movement_input *input = NULL;
    mc_math_classes *math = NULL;
    struct mc_player_nbt_coordinate *coordinate = NULL;
    bool pins_changed = false;
    size_t old_pins = 0;
    unsigned int entered = 0;
    char root[8192], resource_root[8192];
    if (argc != 4 || strcmp(argv[1], "player-sleep-cleanup") != 0 ||
        !decode_path(argv[2], root, sizeof root) ||
        !decode_path(argv[3], resource_root, sizeof resource_root) || !mc_math_trig_init(&trig))
        return 2;
    f = calloc(1, sizeof *f);
    app = calloc(1, sizeof *app);
    call = calloc(1, sizeof *call);
    CHECK(f != NULL && app != NULL && call != NULL);
    CHECK(mc_resource_files_create(resource_root, &app->files) == MC_REGISTRY_OK);
    tick_resources = mc_resource_files_source(app->files);
    CHECK(tick_setup(f, root) && tick_presence(f) && app_setup(f, app, root));
    entered = app->registry->entered;
    CHECK(mc_math_classes_create(1024u * 1024u, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_local_player_allocate(app->entities, &player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(player, f->worlds[0], app, app->session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          mc_movement_input_create_from_options(app->settings, &input, NULL) ==
              MC_MOVEMENT_INPUT_OK &&
          reset_tick(f, player, input));
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], player, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    app->active_world = f->worlds[0];
    app->h = player;
    CHECK(wake_tick_prepare(f, app, player, input, 0, true, false));
    mc_player_fields *fields = &player->local_player->fields.player;
    coordinate = player->local_player->nbt_coordinates;
    while (coordinate != NULL && &coordinate->value != fields->v)
        coordinate = coordinate->next;
    CHECK(coordinate != NULL && coordinate->pins == 0);
    old_pins = coordinate->pins;
    coordinate->pins = SIZE_MAX; /* Declared native admission fault, not Source state. */
    pins_changed = true;
    const size_t holds = player->holds, borrows = app->registry->runtime_borrows,
                 world_holds = app->active_world->persistent_bindings;
    CHECK(mc_world_runtime_tick_entity(app->active_world, player, &tick_context, NULL,
                                       &call->entities) == MC_WORLD_STORAGE_LIMIT &&
          call->entities.child == MC_WORLD_ENTITY_TICK_CHILD_PLAYER &&
          call->entities.child_error.player.child == MC_PLAYER_TICK_CHILD_WAKE &&
          call->entities.child_error.player.child_error.wake.call != NULL &&
          call->entities.child_error.player.retained_registry == app->registry &&
          call->entities.child_error.player.child_error.wake.error.site.pc == 13);
    coordinate->pins = old_pins;
    pins_changed = false;
    const uintptr_t pending = (uintptr_t)call->entities.child_error.player.child_error.wake.call;
    const unsigned int caller_pc = call->entities.bytecode;
    const char *const operation = call->entities.operation;
    /* All top-level flags are still zero: the actual tagged child alone must
     * prevent run() from clearing its owner before any Source work begins. */
    CHECK(!call->failed && !call->active && !mc_client_world_tick_run(app, math, call) &&
          (uintptr_t)call->entities.child_error.player.child_error.wake.call == pending &&
          call->entities.bytecode == caller_pc && call->entities.operation == operation);
    app->registry->entered = 1;
    CHECK(!mc_client_world_tick_cleanup(call) &&
          call->entities.child == MC_WORLD_ENTITY_TICK_CHILD_PLAYER &&
          call->entities.status == MC_WORLD_STORAGE_LIMIT && call->entities.bytecode == caller_pc &&
          call->entities.operation == operation &&
          call->entities.child_error.player.player_bytecode == 53 &&
          call->entities.child_error.player.child_error.wake.call == NULL &&
          call->entities.child_error.player.child_error.wake.error.site.pc == 13 &&
          call->entities.child_error.player.retained_registry == app->registry &&
          call->entities.child_error.player.cleanup.child == MC_PLAYER_TICK_CLEANUP_REGISTRY &&
          call->entities.child_error.player.cleanup.status == MC_ENTITY_BUSY &&
          player->holds == holds && app->registry->runtime_borrows == borrows + 1 &&
          app->active_world->persistent_bindings == world_holds && fields->u &&
          fields->private_a == 100 && player->base.bt == 17);
    CHECK(!mc_client_world_tick_run(app, math, call) &&
          call->entities.child_error.player.retained_registry == app->registry);
    app->registry->entered = entered;
    CHECK(mc_client_world_tick_cleanup(call) &&
          call->entities.child == MC_WORLD_ENTITY_TICK_CHILD_NONE &&
          app->registry->runtime_borrows == borrows && player->holds == holds &&
          app->active_world->persistent_bindings == world_holds && fields->u &&
          fields->private_a == 100 && player->base.bt == 17 && player->base.width == .6f &&
          player->base.height == 1.8f && player->base.offset == 1.62f);

    /* Real periodic fire enters GS35 from the inherited SN environment body,
     * rather than the earlier GS missing-Bed/daytime parent. */
    CHECK(wake_tick_prepare(f, app, player, input, 0, false, false));
    coordinate = player->local_player->nbt_coordinates;
    while (coordinate != NULL && &coordinate->value != fields->v)
        coordinate = coordinate->next;
    CHECK(coordinate != NULL && coordinate->pins == 0);
    coordinate->pins = SIZE_MAX;
    pins_changed = true;
    player->base.bv = 20;
    player->base.bC = false;
    CHECK(mc_world_runtime_tick_entity(app->active_world, player, &tick_context, NULL,
                                       &call->entities) == MC_WORLD_STORAGE_LIMIT &&
          call->entities.child == MC_WORLD_ENTITY_TICK_CHILD_PLAYER &&
          call->entities.child_error.player.child == MC_PLAYER_TICK_CHILD_ENVIRONMENT &&
          call->entities.child_error.player.child_error.environment.effect.player_wake.pending &&
          call->entities.child_error.player.child_error.environment.effect.player_wake.site.pc ==
              13);
    coordinate->pins = old_pins;
    pins_changed = false;
    const mc_entity_status damage_status =
        call->entities.child_error.player.child_error.environment.effect.status;
    const unsigned int damage_pc = call->entities.bytecode;
    const char *const damage_operation = call->entities.operation;
    const size_t pending_holds = player->holds;
    player->holds = SIZE_MAX;
    const bool cleanup_refused = !mc_client_world_tick_cleanup(call);
    player->holds = pending_holds;
    CHECK(cleanup_refused && call->entities.child == MC_WORLD_ENTITY_TICK_CHILD_PLAYER &&
          call->entities.child_error.player.child == MC_PLAYER_TICK_CHILD_ENVIRONMENT &&
          call->entities.child_error.player.cleanup.child == MC_PLAYER_TICK_CLEANUP_EFFECT &&
          call->entities.child_error.player.cleanup.status == MC_ENTITY_LIMIT &&
          call->entities.child_error.player.child_error.environment.effect.player_wake.pending &&
          call->entities.child_error.player.child_error.environment.effect.status ==
              damage_status &&
          call->entities.bytecode == damage_pc && call->entities.operation == damage_operation &&
          !mc_client_world_tick_run(app, math, call) && mc_entity_effect_has_pending(player));
    CHECK(mc_client_world_tick_cleanup(call) &&
          call->entities.child == MC_WORLD_ENTITY_TICK_CHILD_NONE &&
          !mc_entity_effect_has_pending(player) && player->holds == holds &&
          app->registry->runtime_borrows == borrows && fields->u && fields->private_a == 100 &&
          player->base.bt == 18 && player->base.bv == 20);
    result = 0;
cleanup:
    if (pins_changed && coordinate != NULL)
        coordinate->pins = old_pins;
    if (app != NULL && app->registry != NULL)
        app->registry->entered = entered;
    if (call != NULL && !mc_client_world_tick_cleanup(call))
        return 1;
    if (math != NULL && mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        return 1;
    free(call);
    if (mc_movement_input_retire(&input, NULL) != MC_MOVEMENT_INPUT_OK)
        return 1;
    mc_entity_release(player);
    player = NULL;
    if (app != NULL) {
        app->h = NULL;
        app->active_world = NULL;
        if (mc_client_application_retire_statistics(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_application_retire_water_effect(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_achievement_notice_free(&app->achievement_notice, NULL) !=
                MC_CLIENT_NOTICE_OK ||
            (app->render_classes != NULL &&
             mc_client_render_classes_free(app->render_classes) != MC_CLIENT_MODEL_OK))
            return 1;
    }
    if (!tick_cleanup(f))
        return 1;
    if (app != NULL) {
        if (mc_game_settings_retire(&app->settings, NULL) != MC_GAME_SETTINGS_OK)
            return 1;
        mc_client_game_directory_free(app->diagnostic_game_directory);
        mc_resource_files_free(app->files);
        free(app);
    }
    if (result == 0 && printf("Client Player wake cleanup: %u checks\n", checks) < 0)
        return 1;
    return result;
}
