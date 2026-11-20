/* Native controls over genuine Arrow, Player, World and statistics owners.
 * These cases do not execute Arrow flight or replace any child operation. */
#define MC_ARROW_CONSTRUCTION_MAIN mc_arrow_damage_unused_construction_main
#include "test_arrow_construction.c"
#undef MC_ARROW_CONSTRUCTION_MAIN

#include "mc/entity/movement/effects.h"

static void ad_reset(arrow_fixture *f, int32_t difficulty) {
    mc_entity *p = f->base.player;
    mc_local_player_snapshot *v = &p->local_player->fields;
    v->living.E = 20;
    v->living.Y = 20;
    v->living.Z = 17;
    v->living.aa = 0;
    v->living.ad = 0;
    v->living.av = 17;
    v->living.ah = false;
    v->player.private_bO = 0;
    v->player.u = false;
    p->base.by = 0;
    p->base.bb = p->base.be = false;
    p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = 0;
    f->base.world->fields.q = difficulty;
    f->base.world->fields.B = false;
}

static bool ad_success(arrow_fixture *f, mc_entity *a, int32_t difficulty, bool player_shooter,
                       int32_t adjusted) {
    ad_reset(f, difficulty);
    mc_entity *p = f->base.player;
    CHECK(mc_arrow_shooter_store(a, player_shooter ? p : NULL, NULL) == MC_ENTITY_OK);
    const size_t arrow_holds = a->holds, player_holds = p->holds;
    const mc_java_random random = *a->random;
    mc_entity_effect_error e;
    bool result = false;
    CHECK(mc_entity_effect_damage_with_math(p, f->base.app.registry, f->math, a, 4, &result, &e) ==
              MC_ENTITY_OK &&
          result && p->local_player->fields.living.Y == 20 - adjusted &&
          e.damage_statistic_returned && e.damage_statistic_amount == adjusted &&
          e.living_damage.entered && e.living_damage.returned &&
          e.living_damage.source_identity == (uintptr_t)a && e.living_damage.source_hold_acquired &&
          e.living_damage.source_hold_released && e.player_alert.entered == player_shooter &&
          (!player_shooter || (e.player_alert.returned && !e.player_alert.pending)) &&
          a->arrow->fields.c == (player_shooter ? p : NULL) &&
          a->arrow->shooter_hold == a->arrow->fields.c && ac_random_equal(&random, a->random) &&
          a->holds == arrow_holds && p->holds == player_holds && !mc_entity_effect_has_pending(p));
    return true;
}

static bool ad_refusal(arrow_fixture *f, mc_entity *a) {
    ad_reset(f, 2);
    CHECK(mc_arrow_shooter_store(a, f->base.player, NULL) == MC_ENTITY_OK);
    const size_t holds = f->base.player->holds;
    a->arrow->fields.c = NULL; /* Authored native provenance fault, hold retained. */
    bool result = true;
    mc_entity_effect_error e;
    const mc_entity_status status = mc_entity_effect_damage_with_math(
        f->base.player, f->base.app.registry, f->math, a, 4, &result, &e);
    a->arrow->fields.c = f->base.player;
    CHECK(status == MC_ENTITY_STATE_ERROR && result && e.cause == MC_ENTITY_EFFECT_CAUSE_OWNER &&
          e.source_bytecode == 117 && e.entity.status == MC_ENTITY_STATE_ERROR &&
          f->base.player->local_player->fields.living.av == 0 &&
          f->base.player->local_player->fields.living.Y == 20 && !e.player_alert.entered &&
          !e.damage_statistic_returned && f->base.player->holds == holds &&
          !mc_entity_effect_has_pending(f->base.player));
    return true;
}

static bool ad_skip(arrow_fixture *f, mc_entity *a, bool dead) {
    ad_reset(f, dead ? 2 : 0);
    CHECK(mc_arrow_shooter_store(a, f->base.player, NULL) == MC_ENTITY_OK);
    if (dead)
        f->base.player->local_player->fields.living.Y = 0;
    a->arrow->fields.c = NULL; /* An unreached getter must not inspect this fault. */
    bool result = true;
    mc_entity_effect_error e;
    const mc_entity_status status = mc_entity_effect_damage_with_math(
        f->base.player, f->base.app.registry, f->math, a, 4, &result, &e);
    a->arrow->fields.c = f->base.player;
    CHECK(status == MC_ENTITY_OK && !result &&
          f->base.player->local_player->fields.living.Y == (dead ? 0 : 20) &&
          f->base.player->local_player->fields.living.av == 0 && !e.player_alert.entered &&
          !e.damage_statistic_returned && !mc_entity_effect_has_pending(f->base.player));
    return true;
}

static bool ad_alert_hold(arrow_fixture *f, mc_entity *a) {
    ad_reset(f, 2);
    CHECK(mc_arrow_shooter_store(a, f->shooters[0], NULL) == MC_ENTITY_OK);
    mc_entity *shooter = f->shooters[0];
    const size_t holds = shooter->holds;
    shooter->holds = SIZE_MAX; /* Reached helper's native ownership ceiling. */
    bool result = true;
    mc_entity_effect_error e;
    const mc_entity_status status = mc_entity_effect_damage_with_math(
        f->base.player, f->base.app.registry, f->math, a, 4, &result, &e);
    shooter->holds = holds;
    CHECK(status == MC_ENTITY_LIMIT && result && e.source_bytecode == 144 &&
          e.cause == MC_ENTITY_EFFECT_CAUSE_OWNER && !e.damage_statistic_returned &&
          f->base.player->local_player->fields.living.Y == 20 && a->arrow->fields.c == shooter &&
          a->arrow->shooter_hold == shooter && !mc_entity_effect_has_pending(f->base.player));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    arrow_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !ac_setup(f, argv[1], "arrow-player-damage") || !ac_prepare_shooters(f))
        return 1;
    mc_entity *a = NULL;
    if (!ac_allocate(f, &a) ||
        mc_arrow_construct_position(a, f->base.world, f->base.player->base.x + 2,
                                    f->base.player->base.y, f->base.player->base.z,
                                    NULL) != MC_ENTITY_OK)
        return 1;
    for (unsigned int i = 0; i < 8; ++i) {
        const bool ok = i == 0   ? ad_success(f, a, 2, false, 4)
                        : i == 1 ? ad_success(f, a, 2, true, 4)
                        : i == 2 ? ad_skip(f, a, false)
                        : i == 3 ? ad_refusal(f, a)
                        : i == 4 ? ad_success(f, a, 1, false, 2)
                        : i == 5 ? ad_success(f, a, 3, true, 6)
                        : i == 6 ? ad_alert_hold(f, a)
                                 : ad_skip(f, a, true);
        if (!ok || printf("ARROW_PLAYER_DAMAGE_CASE id=%u\n", i) < 0 || fflush(stdout) != 0)
            return 1;
    }
    mc_entity_release(a);
    if (!ac_finish(f))
        return 1;
    free(f);
    return printf("Arrow Player damage: %zu checks;8 native component controls\n", checks) < 0 ||
                   fflush(stdout) != 0
               ? 1
               : 0;
}
