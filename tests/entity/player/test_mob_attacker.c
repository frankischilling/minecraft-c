/* Actual incoming Skeleton damage over genuine World/App/statistics owners.
 * The imported fixture entry point is never executed. */
#define main mc_mob_attacker_unused_statistics_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main
#include "../../../src/entity/tick/tick_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/core/math/classes.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/movement/effects.h"
#include "mc/entity/player/ally_alert.h"

static bool incoming_owner(const char *resources) {
    fixture *f = calloc(1, sizeof(*f));
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    mc_entity_tick_error *tick = calloc(1, sizeof(*tick));
    CHECK(f != NULL && error != NULL && tick != NULL && setup(f, resources, "mob-owner", true));
    mc_entity *cow = NULL;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error construction;
    CHECK(mc_entity_class_literal_get(f->app.entities, MC_ENTITY_CLASS_KIND_COW, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->app.entities, f->app.registry, literal, &cow, &construction) ==
              MC_MOB_OK &&
          mc_mob_construct(cow, f->world, &construction) == MC_MOB_OK);
    const size_t holds = f->player->holds;
    f->player->base.world = NULL; /* Declared source input; Cow skips difficulty. */
    bool result = true;
    CHECK(mc_entity_effect_damage_with_math(f->player, f->app.registry, NULL, cow, 4, &result,
                                            NULL) != MC_ENTITY_OK &&
          result && mc_entity_effect_has_pending(f->player));
    mc_player_ally_alert_state state;
    CHECK(mc_player_ally_alert_inspect(f->player->local_player->pending_damage_alert, &state) &&
          !state.returned && state.error->source_exception && !state.error->native_policy &&
          state.error->site.pc == 115 &&
          strcmp(state.error->exception_class, "java/lang/NullPointerException") == 0);
    const mc_entity_status source_status = state.error->status;
    const char *source_operation = state.error->operation;
    const char *source_exception = state.error->exception_class;
    const size_t examined = state.wolves_examined, alerted = state.wolves_alerted;
    const mc_entity_bounds *first_bounds = state.bounds, *first_expanded = state.expanded;
    const mc_player_ally_alert_call *pending = f->player->local_player->pending_damage_alert;
    CHECK(mc_entity_effect_damage_with_math(f->player, f->app.registry, NULL, NULL, 0, &result,
                                            error) == MC_ENTITY_BUSY &&
          f->player->local_player->pending_damage_alert == pending &&
          error->cause == MC_ENTITY_EFFECT_CAUSE_OWNER);
    mc_entity_tick_error_init(tick);
    tick->effect = *error;
    CHECK(!mc_tick_effect(tick, MC_ENTITY_BUSY, "pending damage control", 0) &&
          tick->native_policy && !tick->source_exception && tick->exception_class == NULL);
    /* Native cleanup fault, separate from the already observed source NPE. */
    const size_t bounds = f->app.entities->bounds_holds;
    f->app.entities->bounds_holds = 0;
    const mc_entity_status refused = mc_entity_effect_cleanup(f->player, error);
    f->app.entities->bounds_holds = bounds;
    CHECK(refused != MC_ENTITY_OK && mc_entity_effect_has_pending(f->player) &&
          mc_player_ally_alert_inspect(pending, &state) && state.error->site.pc == 115 &&
          state.error->source_exception && !state.error->native_policy &&
          state.error->status == source_status && state.error->operation == source_operation &&
          state.error->exception_class == source_exception && state.wolves_examined == examined &&
          state.wolves_alerted == alerted && state.bounds == first_bounds &&
          state.expanded == first_expanded && error->cause == MC_ENTITY_EFFECT_CAUSE_OWNER);
    mc_entity_tick_error_init(tick);
    tick->effect = *error;
    CHECK(!mc_tick_effect(tick, refused, "refused cleanup control", 0) && tick->native_policy &&
          !tick->source_exception && tick->exception_class == NULL);
    CHECK(mc_entity_effect_cleanup(f->player, error) == MC_ENTITY_OK &&
          !mc_entity_effect_has_pending(f->player) && f->player->holds == holds &&
          !error->player_alert.pending && error->player_alert.error == NULL &&
          error->player_alert.cleanup == NULL && error->player_alert.receiver == NULL &&
          f->player->local_player->fields.living.Y == 20);
    /* A copied failure view can outlive cleanup through a different sink. Its
     * retirement must clear consumed borrows without reopening source work. */
    CHECK(mc_entity_effect_damage_with_math(f->player, f->app.registry, NULL, cow, 4, &result,
                                            error) != MC_ENTITY_OK &&
          error->player_alert.pending && error->player_alert.source_exception &&
          error->player_alert.site.pc == 115);
    const mc_entity_status first_status = error->status;
    CHECK(mc_entity_effect_cleanup(f->player, NULL) == MC_ENTITY_OK &&
          !mc_entity_effect_has_pending(f->player) && f->player->holds == holds &&
          mc_entity_effect_error_retire(error) && error->status == first_status &&
          error->player_alert.site.pc == 115 && error->player_alert.source_exception &&
          !error->player_alert.pending && error->player_alert.error == NULL &&
          error->player_alert.cleanup == NULL && error->player_alert.receiver == NULL);
    f->player->base.world = f->world; /* End declared input before fixture retirement. */
    mc_entity_release(cow);
    CHECK(teardown(f));
    free(tick);
    free(error);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    mc_entity *attacker = NULL;
    mc_math_classes *math = NULL;
    if (f == NULL || error == NULL || !setup(f, argv[1], "mob-attacker", true))
        return 1;
    const mc_entity_class_literal *skeleton = NULL;
    mc_mob_error construction;
    if (mc_entity_class_literal_get(f->app.entities, MC_ENTITY_CLASS_KIND_SKELETON, &skeleton) !=
            MC_ENTITY_OK ||
        mc_mob_allocate(f->app.entities, f->app.registry, skeleton, &attacker, &construction) !=
            MC_MOB_OK ||
        mc_mob_construct(attacker, f->world, &construction) != MC_MOB_OK ||
        mc_math_classes_create(SIZE_MAX, &math, NULL) != MC_MATH_CLASS_OK ||
        mc_math_classes_ensure(math, NULL) != MC_MATH_CLASS_OK)
        return 1;
    f->world->fields.B = false;
    f->world->fields.q = 2;
    if (mc_entity_set_position(f->player, 8.5, 200.5, 8.5) != MC_ENTITY_OK ||
        mc_entity_set_position(attacker, 9.5, 200.5, 8.5) != MC_ENTITY_OK)
        return 1;
    bool result = false;
    const mc_entity_status status = mc_entity_effect_damage_with_math(
        f->player, f->app.registry, math, attacker, 4, &result, error);
    mc_registry_statistic_view statistics;
    int32_t taken = -1;
    const bool passed =
        status == MC_ENTITY_OK && result && f->player->local_player->fields.living.Y == 16 &&
        mc_registry_classes_inspect_statistics(f->app.registry, &statistics, NULL) ==
            MC_REGISTRY_OK &&
        mc_client_stats_writer_value(f->app.stats_writer, statistics.named[18], &taken, NULL) ==
            MC_CLIENT_STATS_OK &&
        taken == 4;
    if (!passed)
        fprintf(stderr,
                "Incoming Skeleton: status=%d source-pc=%u player-pc=%u health=%d result=%d "
                "operation=%s\n",
                (int)status, (unsigned)error->source_bytecode, (unsigned)error->player_bytecode,
                f->player->local_player->fields.living.Y, result,
                error->operation == NULL ? "none" : error->operation);
    if (!mc_entity_effect_error_retire(error) ||
        mc_entity_effect_cleanup(f->player, NULL) != MC_ENTITY_OK)
        return 1;
    mc_entity_release(attacker);
    if (mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK || !teardown(f))
        return 1;
    free(error);
    free(f);
    if (!passed)
        return 1;
    if (!incoming_owner(argv[1]))
        return 1;
    puts("Incoming Skeleton: actual World/App/statistics; health20-to16; checked cleanup");
    return 0;
}
