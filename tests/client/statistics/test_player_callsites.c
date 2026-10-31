/* Reuse the genuine App/Session/World fixture; its observation main is not run. */
#define main mc_player_statistics_observation_main
#include "test_player_dispatch.c"
#undef main
#include "mc/entity/living/travel.h"
#include "mc/entity/movement/effects.h"

static bool value(fixture *f, size_t index, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t actual = INT32_MIN;
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &view, NULL) == MC_REGISTRY_OK);
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, view.named[index], &actual, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(actual == expected);
    return true;
}

static bool callsites(fixture *f) {
    mc_entity *p = f->player;
    const mc_player_travel_context context = {.code_registry = f->app.registry, .trig = &trig};
    mc_player_travel_error *travel = malloc(sizeof(*travel));
    mc_entity_effect_error *effect = malloc(sizeof(*effect));
    CHECK(travel != NULL && effect != NULL);
    const size_t holds = p->holds;
    CHECK(mc_player_jump(p, &context, travel) == MC_ENTITY_OK);
    CHECK(p->base.velocity_y == 0.41999998688697815 && travel->cause == MC_PLAYER_TRAVEL_NONE);
    CHECK(value(f, 15, 1));
    CHECK(mc_player_jump(p, &context, travel) == MC_ENTITY_OK && value(f, 15, 2));

    /* Above the real World height, block/material queries naturally return air.
     * This uses the constructed World, not a replacement block callback. */
    CHECK(mc_entity_set_position(p, 8, 200, 8) == MC_ENTITY_OK);
    p->base.aX = true;
    p->base.bx = false;
    CHECK(mc_player_movement_statistics(p, &context, 3, 0, 4, travel) == MC_ENTITY_OK);
    CHECK(value(f, 6, 500));
    p->base.aX = false;
    CHECK(mc_player_movement_statistics(p, &context, .26, 0, 0, travel) == MC_ENTITY_OK);
    CHECK(value(f, 10, 26));
    p->base.bx = true;
    CHECK(mc_player_movement_statistics(p, &context, 3, 7, 4, travel) == MC_ENTITY_OK);
    CHECK(value(f, 7, 500));

    /* Complete actual gs travel, including its post-move statistic dispatch. */
    p->base.bq = true;
    p->base.velocity_x = 1;
    p->base.velocity_y = p->base.velocity_z = 0;
    CHECK(mc_player_travel(p, &context, 0, 0, travel) == MC_ENTITY_OK);
    CHECK(p->base.x == 9 && p->base.y == 200 && p->base.z == 8);
    CHECK(value(f, 7, 600));

    CHECK(mc_entity_effect_fall_in_context(p, f->app.registry, 2.5f, effect) == MC_ENTITY_OK);
    CHECK(value(f, 8, 250));
    /* A completed statistic must continue into the actual passenger callback. */
    p->base.aG = p;
    const mc_entity_status passenger =
        mc_entity_effect_fall_in_context(p, f->app.registry, 2.5f, effect);
    p->base.aG = NULL;
    CHECK(passenger == MC_ENTITY_STATE_ERROR &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_RELATIONSHIP &&
          effect->fall_callback_bytecode == 12 && effect->statistic_amount == 250);
    CHECK(value(f, 8, 500));

    /* Damage now increments its real statistic before the unfinished hurt body.
     * Preserve that exact prefix rather than claiming a successful health edit. */
    bool damaged = true;
    const int32_t health = p->local_player->fields.living.Y;
    CHECK(mc_entity_effect_damage_in_context(p, f->app.registry, NULL, 3, &damaged, effect) ==
          MC_ENTITY_STATE_ERROR);
    CHECK(damaged && p->local_player->fields.living.Y == health &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL &&
          effect->living_bytecode == 158 && effect->source_bytecode == 158);
    CHECK(value(f, 18, 3));

    p->local_player->fields.minecraft = NULL;
    const mc_entity_status no_app = mc_player_jump(p, &context, travel);
    p->local_player->fields.minecraft = &f->app;
    CHECK(no_app == MC_ENTITY_STATE_ERROR && travel->cause == MC_PLAYER_TRAVEL_STATISTIC &&
          travel->player_bytecode == 9 && travel->statistic_bytecode == 85 &&
          travel->statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE &&
          travel->statistic_error.site.pc == 85 && travel->statistic_amount == 1);
    CHECK(value(f, 15, 2));
    mc_client_stats_writer *writer = f->app.stats_writer;
    f->app.stats_writer = NULL;
    const mc_entity_status no_writer =
        mc_entity_effect_fall_in_context(p, f->app.registry, 2.5f, effect);
    f->app.stats_writer = writer;
    CHECK(no_writer == MC_ENTITY_STATE_ERROR &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE && effect->player_bytecode == 20 &&
          effect->statistic_bytecode == 90 && effect->statistic_error.site.pc == 90 &&
          effect->statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE);
    CHECK(value(f, 8, 500));
    CHECK(p->holds == holds);
    free(effect);
    free(travel);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig)) {
        fputs("Player callsites input: resources work-directory\n", stderr);
        return 2;
    }
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "callsites", true) || !callsites(f) || !teardown(f))
        return 1;
    free(f);
    printf("Player statistic callsites: %zu checks\n", checks);
    return 0;
}
