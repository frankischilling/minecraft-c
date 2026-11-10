/* Reuse genuine constructed World/Player/Skeleton fixture and teardown only. */
int de_unused_close_main(int, char **);
#define MC_DAMAGE_CLOSE_MAIN de_unused_close_main
#include "test_damage_close_execution.c"
#undef MC_DAMAGE_CLOSE_MAIN

/* First regression is built against the actual legacy dispatcher. Root clears
 * this definition after observing its missing protected invocation ownership. */
#ifndef MC_DAMAGE_EXECUTION_LEGACY_CONTROL
#define MC_DAMAGE_EXECUTION_LEGACY_CONTROL 0
#endif
#if !MC_DAMAGE_EXECUTION_LEGACY_CONTROL
#include "entity/mob/death_execution_internal.h"
#include "entity/movement/damage_execution_internal.h"
#endif

typedef struct de_fixture {
    dnc_fixture base;
#if !MC_DAMAGE_EXECUTION_LEGACY_CONTROL
    mc_entity_damage_execution damage;
#endif
    mc_world_access *observer;
    mc_world_access_receipt *receipt;
    mc_entity_effect_error unrelated_error;
    mc_world_runtime_execution_result unrelated_result;
    const void *unrelated_work;
    size_t calls;
    bool callback_owned, unrelated, unknown;
} de_fixture;

static mc_world_storage_status de_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    de_fixture *s = context;
    ++s->calls;
    mc_world_access_failure_lease *node = mc_entity_tick_execution_world_lease(&s->base.execution);
    s->callback_owned = node != NULL && node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                        s->observer->active_holds == 1;
    if (s->unrelated) {
        /* A genuine owning nullable World assignment in this authored Source
         * callback. Its old occurrence is balanced; no field repair follows.
         * The outer sound keeps its previously evaluated World independently. */
        mc_entity *victim = s->base.skeleton;
        mc_world_runtime *old = victim->base.world;
        victim->base.world = NULL;
        mc_world_runtime_binding_release(old);
        mc_entity_effect_error_init(&s->unrelated_error);
        mc_entity_status raw = MC_ENTITY_OK;
        const bool completed = mc_mob_death_entered_in_execution(
            &s->base.execution, s->base.game->classes[0], dnc_code, victim,
            s->base.game->registry[0], NULL, &raw, &s->unrelated_error, &s->unrelated_result);
        s->unrelated_work = victim->mob->pending_death;
        s->callback_owned &= !completed && raw == MC_ENTITY_STATE_ERROR &&
                             s->unrelated_work != NULL && s->unrelated_error.death.pending &&
                             s->unrelated_error.death.source_exception;
    }
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->observer, &removed, cause);
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    status = mc_world_access_receipt_create(s->observer, NULL, &s->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    s->receipt->exception_class =
        s->unknown ? "authored/UnqualifiedError" : "java/lang/IllegalStateException";
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Damage actual sound callback source failure", 129};
    return cause->status;
}

static bool de_run(de_fixture *s) {
    fixture *f = s->base.game;
    mc_entity *victim = s->base.skeleton;
    DNC_CHECK(mc_entity_set_position(victim, 8.5, 64, 8.5) == MC_ENTITY_OK &&
              mc_entity_set_position(s->base.player, 11.5, 64, 12.5) == MC_ENTITY_OK);
    victim->mob->living.Y = victim->mob->living.E = 20;
    victim->mob->living.au = victim->base.by = 0;
    const mc_world_access_source_callbacks callbacks = {.sound = de_sound};
    DNC_CHECK(
        mc_world_access_source_create(&callbacks, s, &s->observer, NULL) == MC_WORLD_STORAGE_OK &&
        mc_world_runtime_add_world_access(f->worlds[0], s->observer, NULL) == MC_WORLD_STORAGE_OK);
    bool answer = false;
    mc_entity_status raw;
#if MC_DAMAGE_EXECUTION_LEGACY_CONTROL
    raw = mc_entity_effect_damage_with_math(victim, f->registry[0], s->base.math_owner,
                                            s->base.player, 4, &answer, &s->base.effect);
    const bool completed = raw == MC_ENTITY_OK;
#else
    mc_entity_damage_execution_init(&s->damage, &s->base.execution, f->classes[0], dnc_code,
                                    f->registry[0]);
    const bool completed = mc_entity_effect_damage_in_execution(&s->damage, victim, s->base.player,
                                                                4, &answer, &s->base.effect, &raw);
#endif
    printf("DAMAGE_RETURN completed=%d answer=%d callbacks=%zu callback_owned=%d raw=%d "
           "health=%d captures=%zu active=%zu receipt_holds=%zu protected=%d\n",
           completed, answer, s->calls, s->callback_owned, (int)raw, victim->mob->living.Y,
           s->base.execution.receipt_count, s->observer->active_holds, s->receipt->holds,
           s->receipt->leases != NULL);
    DNC_CHECK(!completed && !answer && s->calls == 1 && s->callback_owned &&
              victim->mob->living.Y == 16 && raw == MC_ENTITY_WORLD_FAILED &&
              !s->base.effect.living_damage.returned &&
              s->base.effect.world.world_access_failure.receipt == s->receipt &&
              s->base.execution.receipt_count == 1 && s->observer->active_holds == 1 &&
              s->receipt->holds == 1 && s->receipt->leases != NULL &&
              mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
              mc_world_access_free(s->observer, NULL) == MC_WORLD_STORAGE_BUSY);
#if !MC_DAMAGE_EXECUTION_LEGACY_CONTROL
    mc_entity_damage_failure failure;
    DNC_CHECK(mc_entity_damage_execution_source_failure(&s->damage, &failure) &&
              failure.kind == (s->unknown ? MC_ENTITY_DAMAGE_FAILURE_UNRESOLVED
                                          : MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION) &&
              failure.source_proven && failure.unresolved == s->unknown && failure.math == NULL &&
              strcmp(failure.exception_class, s->unknown
                                                  ? "authored/UnqualifiedError"
                                                  : "java/lang/IllegalStateException") == 0 &&
              failure.operation == s->base.effect.world.cause.operation &&
              strcmp(failure.site.class_name, "fd") == 0);
    if (s->unrelated)
        DNC_CHECK(s->damage.acquired_work == NULL &&
                  victim->mob->pending_death == s->unrelated_work);
    DNC_CHECK(mc_entity_damage_execution_native_close(&s->damage) == MC_WORLD_STORAGE_OK &&
              mc_entity_damage_execution_cleanup(&s->damage, &s->base.effect) == MC_ENTITY_OK);
    if (s->unrelated) {
        DNC_CHECK(victim->mob->pending_death == s->unrelated_work &&
                  mc_entity_effect_error_retire(&s->unrelated_error) &&
                  victim->mob->pending_death == NULL);
        mc_entity_effect_error_init(&s->unrelated_error);
    }
#endif
    mc_entity_effect_error_init(&s->base.effect);
    DNC_CHECK(mc_entity_tick_execution_release_receipts(&s->base.execution) ==
                  MC_WORLD_STORAGE_OK &&
              mc_math_execution_retire(&s->base.math) == MC_MATH_CLASS_OK &&
              mc_entity_tick_execution_retire(&s->base.execution) == MC_WORLD_STORAGE_OK &&
              mc_math_classes_release(&s->base.math_owner, NULL) == MC_MATH_CLASS_OK &&
              mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_access_free(s->observer, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

#ifndef MC_DAMAGE_EXECUTION_MAIN
#define MC_DAMAGE_EXECUTION_MAIN main
#endif
int MC_DAMAGE_EXECUTION_MAIN(int argc, char **argv) {
    char root[8192];
    if (argc != 3 ||
        (strcmp(argv[1], "sound-receipt") != 0 && strcmp(argv[1], "unrelated-death") != 0 &&
         strcmp(argv[1], "unknown-receipt") != 0) ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    de_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    s->base.game = calloc(1, sizeof(*s->base.game));
    s->unrelated = strcmp(argv[1], "unrelated-death") == 0;
    s->unknown = strcmp(argv[1], "unknown-receipt") == 0;
    printf("DAMAGE_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    if (s->base.game == NULL || !dnc_setup(&s->base, root) || !de_run(s) || !dnc_cleanup(&s->base))
        return 1;
    printf("PASS: %u execution-owned damage checks;mode=%s\n", checks, argv[1]);
    free(s);
    return 0;
}
