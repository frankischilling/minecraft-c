/* Real Arrow parent admission, early failure and first-use controls.
 * Imported fixture mains never execute. */
#define MC_ARROW_NATURAL_FLIGHT_MAIN mc_arrow_tick_failures_unused_flight_main
#include "test_arrow_natural_flight.c"
#undef MC_ARROW_NATURAL_FLIGHT_MAIN

#include "../../../src/world/runtime/runtime_execution_internal.h"
#include "mc/entity/arrow/tick.h"
#include <xmmintrin.h>

#define ATF_REQUIRE(condition)                                                                     \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct atf_fixture {
    nf_fixture natural;
    mc_arrow_tick_call *call, *rejected;
    mc_math_classes *extra_math;
    mc_entity *raw;
    size_t arrow_holds, code_borrows, world_bindings;
    unsigned int begins, published, preallocation_refusals;
} atf_fixture;

static bool atf_math_state(mc_math_classes *math, mc_math_class_phase phase, size_t holds,
                           bool table) {
    mc_math_class_state state;
    return mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK && state.phase == phase &&
           state.holds == holds && (state.source_a != NULL) == table;
}

static bool atf_construct_null_world(atf_fixture *s) {
    mc_arrow_error error;
    nf_fixture *const n = &s->natural;
    ATF_REQUIRE(mc_arrow_allocate(n->world->entity_classes[0], &n->arrow, NULL) == MC_ENTITY_OK);
    ATF_REQUIRE(mc_arrow_construct_position(n->arrow, NULL, 8.5, 66.0, 8.5, &error) ==
                    MC_ENTITY_OK &&
                n->arrow->phase == MC_ENTITY_READY && n->arrow->base.world == NULL);
    ATF_REQUIRE(mc_arrow_heading(n->arrow, n->math, 1.0, 0.0, 0.0, 0.6f, 0.0f, &error) ==
                MC_ENTITY_OK);
    return true;
}

static bool atf_preallocation(atf_fixture *s) {
    nf_fixture *const n = &s->natural;
    mc_world_runtime_classes *const code = n->world->classes[0];
    mc_arrow_tick_error error;
    const mc_entity_base_fields before = n->arrow->base;
    const mc_java_random random = *n->arrow->random;
    ATF_REQUIRE(mc_arrow_allocate(n->world->entity_classes[0], &s->raw, NULL) == MC_ENTITY_OK &&
                s->raw->phase == MC_ENTITY_RAW);
    const size_t raw_holds = s->raw->holds;
    ++s->begins;
    mc_arrow_tick_status status =
        mc_arrow_tick_begin(s->raw, code, n->math, NULL, &s->call, &error);
    ATF_REQUIRE(status == MC_ARROW_TICK_STATE && error.status == status && error.native_policy &&
                error.child == MC_ARROW_TICK_CHILD_OWNER && s->call == NULL &&
                s->raw->arrow->tick_call == NULL && s->raw->holds == raw_holds);
    ++s->preallocation_refusals;
    ++s->begins;
    status = mc_arrow_tick_begin(n->arrow, code, n->math, NULL, NULL, &error);
    ATF_REQUIRE(status == MC_ARROW_TICK_ARGUMENT && error.status == status && error.native_policy &&
                s->call == NULL && n->arrow->arrow->tick_call == NULL);
    ++s->preallocation_refusals;
    mc_arrow_tick_limits limits = mc_arrow_tick_default_limits();
    limits.max_parent_bytes = 0;
    ++s->begins;
    status = mc_arrow_tick_begin(n->arrow, code, n->math, &limits, &s->call, &error);
    ATF_REQUIRE(status == MC_ARROW_TICK_LIMIT && error.status == status && error.native_policy &&
                s->call == NULL && n->arrow->arrow->tick_call == NULL &&
                n->arrow->holds == s->arrow_holds && code->native_borrows == s->code_borrows &&
                n->world->worlds[0]->persistent_bindings == s->world_bindings &&
                n->arrow->base.bt == before.bt && n->arrow->base.x == before.x &&
                n->arrow->base.y == before.y && n->arrow->base.z == before.z &&
                n->arrow->random->state == random.state &&
                n->arrow->random->have_next_next_gaussian == random.have_next_next_gaussian &&
                ad_bits(n->arrow->random->next_next_gaussian) ==
                    ad_bits(random.next_next_gaussian));
    ++s->preallocation_refusals;
    return true;
}

static bool atf_early_state(atf_fixture *s, const mc_arrow_tick_state *state,
                            mc_arrow_tick_status returned) {
    const mc_arrow_tick_error *const error = state->failure;
    ATF_REQUIRE(s->call != NULL && s->natural.arrow->arrow->tick_call == s->call &&
                !state->active && !state->retiring && !state->source_returned &&
                state->phase == MC_ARROW_TICK_FAILED && state->receiver == s->natural.arrow &&
                state->source_pc == 1 && state->base_wrapper_pc == 1 && error != NULL &&
                error->status == returned && returned != MC_ARROW_TICK_OK &&
                error->child == MC_ARROW_TICK_CHILD_BASE && error->base != NULL &&
                state->cleanup != NULL && state->cleanup->status == MC_ARROW_TICK_OK);
    ATF_REQUIRE(state->candidates_started == 0 && state->angle_iterations == 0 &&
                state->random_calls == 0 && state->bubble_calls == 0 && !state->hurt_invoked &&
                !state->hurt_returned && !state->hurt_result && !state->hurt_prior_pending &&
                !state->hurt_cleanup_acquired && state->candidate == NULL &&
                state->selected == NULL && state->hurt_source == NULL &&
                state->hurt_target == NULL && state->start == NULL && state->end == NULL &&
                state->candidate_bounds == NULL && state->hit == NULL &&
                state->pending_hit == NULL && state->intercept_hit == NULL &&
                state->ray_call == NULL && state->intercept_call == NULL &&
                state->base_particle_work == 0 && state->diagnostic_bytes == 0 &&
                state->query_live && !state->query_invoked && state->query != NULL);
    ATF_REQUIRE(error->block == NULL && error->nearby == NULL && error->ray == NULL &&
                error->intercept == NULL && error->hit == NULL && error->bounds == NULL &&
                error->vector == NULL && error->geometry == NULL && error->effect == NULL &&
                error->environment == NULL && error->sound == NULL && error->particle == NULL &&
                error->loader == NULL && error->position == NULL &&
                error->base->watcher_index == -1);
    return true;
}

static bool atf_observe(const char *mode, atf_fixture *s, const mc_arrow_tick_state *state,
                        mc_arrow_tick_status returned) {
    const mc_arrow_tick_error *const error = state->failure;
    ATF_REQUIRE(error != NULL);
    const mc_entity_tick_error *const base = error->base;
    /* These are actual returned/selected diagnostics, not an exception oracle. */
    ATF_REQUIRE(
        printf("ARROW_TICK_FAILURES_OBSERVATION mode=%s returned=%u phase=%u classification=%u "
               "native=%u source_exception=%u source_error=%u child=%u source_pc=%u "
               "base_status=%d base_pc=%d environment_status=%d environment_cause=%d "
               "environment_pc=%d material_status=%d math_status=%d ownership_status=%u "
               "begins=%u published=%u preallocation_refusals=%u\n",
               mode, (unsigned int)returned, (unsigned int)state->phase,
               (unsigned int)error->classification, (unsigned int)error->native_policy,
               (unsigned int)error->source_exception, (unsigned int)error->source_error,
               (unsigned int)error->child, state->source_pc, base == NULL ? -1 : (int)base->status,
               base == NULL ? -1 : (int)base->bytecode,
               base == NULL ? -1 : (int)base->environment.status,
               base == NULL ? -1 : (int)base->environment.cause,
               base == NULL ? -1 : (int)base->environment.bytecode,
               base == NULL ? -1 : (int)base->environment.material.status,
               error->math == NULL ? -1 : (int)error->math->status,
               state->query == NULL ? (unsigned int)MC_WORLD_STORAGE_OK
                                    : (unsigned int)state->query->ownership_status,
               s->begins, s->published, s->preallocation_refusals) > 0);
    return true;
}

static bool atf_run(atf_fixture *s, const char *mode) {
    nf_fixture *const n = &s->natural;
    mc_world_runtime_classes *code = n->world->classes[0];
    mc_math_classes *math = n->math;
    mc_math_class_error math_error;
    const bool fp = strcmp(mode, "admission-fp") == 0;
    const bool cold = strcmp(mode, "math-cold") == 0;
    const bool failed = strcmp(mode, "math-failed") == 0;
    if (strcmp(mode, "code-null") == 0)
        code = NULL;
    else if (strcmp(mode, "math-null") == 0)
        math = NULL;
    else if (cold || failed || fp) {
        ATF_REQUIRE(mc_math_classes_create(failed ? sizeof(mc_math_trig) : SIZE_MAX, &s->extra_math,
                                           &math_error) == MC_MATH_CLASS_OK);
        math = s->extra_math;
        ATF_REQUIRE(atf_math_state(math, MC_MATH_CLASS_COLD, 1, false));
        if (failed)
            ATF_REQUIRE(mc_math_classes_ensure(math, &math_error) == MC_MATH_CLASS_LIMIT &&
                        math_error.source_pc == 2 &&
                        atf_math_state(math, MC_MATH_CLASS_FAILED, 1, false));
    }
    if (fp)
        ATF_REQUIRE(atf_preallocation(s));

    const mc_entity_base_fields before = n->arrow->base;
    const mc_java_random random = *n->arrow->random;
    mc_arrow_tick_error admission;
    ++s->begins;
    const unsigned int saved = _mm_getcsr();
    if (fp)
        _mm_setcsr(saved | 0x8000u);
    const mc_arrow_tick_status returned =
        mc_arrow_tick_begin(n->arrow, code, math, NULL, &s->call, &admission);
    if (fp)
        _mm_setcsr(saved); /* Restore host control before any assertion/inspection/cleanup. */
    if (s->call != NULL)
        ++s->published;
    ATF_REQUIRE(s->call != NULL && admission.status == MC_ARROW_TICK_OK &&
                n->arrow->arrow->tick_call == s->call && n->arrow->holds == s->arrow_holds + 1u &&
                n->world->classes[0]->native_borrows == s->code_borrows + (code != NULL ? 1u : 0u));
    mc_arrow_tick_state state;
    ATF_REQUIRE(mc_arrow_tick_inspect(s->call, &state));
    const mc_arrow_tick_error *const error = state.failure;
    if (cold) {
        ATF_REQUIRE(returned == MC_ARROW_TICK_OK && !state.active && !state.retiring &&
                    state.source_returned && state.phase == MC_ARROW_TICK_RETURNED &&
                    state.receiver == n->arrow && error != NULL &&
                    error->status == MC_ARROW_TICK_OK && !error->native_policy &&
                    !error->source_exception && !error->source_error && !state.hurt_invoked &&
                    !state.hurt_cleanup_acquired && !state.query_live && state.query == NULL &&
                    !state.direct_world_open && state.diagnostic_bytes == 0 &&
                    state.cleanup != NULL && state.cleanup->status == MC_ARROW_TICK_OK &&
                    atf_math_state(math, MC_MATH_CLASS_READY, 2, true));
    } else {
        ATF_REQUIRE(atf_early_state(s, &state, returned));
        if (strcmp(mode, "code-null") == 0)
            ATF_REQUIRE(error->native_policy &&
                        error->classification == MC_ARROW_TICK_FAILURE_NATIVE &&
                        error->math == NULL && error->base->status == MC_ENTITY_OK &&
                        error->base->bytecode == 0 && n->arrow->base.bt == before.bt &&
                        state.query->ownership_status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
                        strcmp(state.query->ownership_error.operation,
                               "Arrow Base required fresh execution/code/frame") == 0);
        else {
            ATF_REQUIRE(n->arrow->base.bt == mc_java_i32_add(before.bt, 1) &&
                        n->arrow->base.bi == before.bj && n->arrow->base.aJ == before.x &&
                        n->arrow->base.aK == before.y && n->arrow->base.aL == before.z &&
                        n->arrow->base.aV == before.aT && n->arrow->base.aU == before.yaw &&
                        error->base->bytecode == 81);
            if (strcmp(mode, "null-world") == 0)
                ATF_REQUIRE(n->arrow->base.world == NULL && error->source_exception &&
                            !error->native_policy && error->exception_class != NULL &&
                            strcmp(error->exception_class, "java/lang/NullPointerException") == 0 &&
                            error->base->environment.cause ==
                                MC_ENTITY_ENVIRONMENT_NULL_REFERENCE &&
                            error->base->environment.bytecode == 32 && error->math == NULL);
            else if (fp)
                ATF_REQUIRE(error->base->status == MC_ENTITY_UNSUPPORTED_FP &&
                            error->base->environment.cause == MC_ENTITY_ENVIRONMENT_FP &&
                            error->base->environment.bytecode == 13 && error->math == NULL &&
                            atf_math_state(math, MC_MATH_CLASS_COLD, 1, false));
            else
                ATF_REQUIRE(error->native_policy && error->math != NULL &&
                            error->math->status == (failed ? MC_MATH_CLASS_INITIALIZATION_FAILED
                                                           : MC_MATH_CLASS_INVALID_ARGUMENT) &&
                            (!failed || (error->math->initial_cause == MC_MATH_CLASS_LIMIT &&
                                         error->math->source_pc == 2 &&
                                         atf_math_state(math, MC_MATH_CLASS_FAILED, 2, false))));
        }
    }
    ATF_REQUIRE(n->arrow->random->state == random.state &&
                n->arrow->random->have_next_next_gaussian == random.have_next_next_gaussian &&
                ad_bits(n->arrow->random->next_next_gaussian) ==
                    ad_bits(random.next_next_gaussian));
    if (fp) {
        mc_arrow_tick_error busy;
        ++s->begins;
        const mc_arrow_tick_status second =
            mc_arrow_tick_begin(n->arrow, code, math, NULL, &s->rejected, &busy);
        ATF_REQUIRE(second == MC_ARROW_TICK_BUSY && busy.status == second && busy.native_policy &&
                    s->rejected == NULL && n->arrow->arrow->tick_call == s->call &&
                    n->arrow->holds == s->arrow_holds + 1u);
        ++s->preallocation_refusals;
    }
    ATF_REQUIRE(atf_observe(mode, s, &state, returned));
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    atf_fixture s = {0};
    char root[8192];
    if (argc != 3 ||
        (strcmp(argv[1], "null-world") != 0 && strcmp(argv[1], "code-null") != 0 &&
         strcmp(argv[1], "math-null") != 0 && strcmp(argv[1], "math-failed") != 0 &&
         strcmp(argv[1], "math-cold") != 0 && strcmp(argv[1], "admission-fp") != 0) ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    printf("ARROW_TICK_FAILURES_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    CHECK(nf_setup(&s.natural, root, NULL));
    if (strcmp(argv[1], "null-world") == 0)
        CHECK(atf_construct_null_world(&s));
    else
        CHECK(nf_construct(&s.natural));
    s.arrow_holds = s.natural.arrow->holds;
    s.code_borrows = s.natural.world->classes[0]->native_borrows;
    s.world_bindings = s.natural.world->worlds[0]->persistent_bindings;
    printf("ARROW_TICK_FAILURES_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    CHECK(atf_run(&s, argv[1]));
    if (mc_arrow_tick_retire(&s.call) != MC_ARROW_TICK_OK) {
        fputs("Arrow tick failure retirement refused; actual owners retained\n", stderr);
        return 1;
    }
    CHECK(s.call == NULL && s.natural.arrow->arrow->tick_call == NULL &&
          s.natural.arrow->holds == s.arrow_holds &&
          s.natural.world->classes[0]->native_borrows == s.code_borrows &&
          s.natural.world->worlds[0]->persistent_bindings == s.world_bindings);
    if (s.extra_math != NULL)
        CHECK(atf_math_state(s.extra_math,
                             strcmp(argv[1], "math-cold") == 0     ? MC_MATH_CLASS_READY
                             : strcmp(argv[1], "math-failed") == 0 ? MC_MATH_CLASS_FAILED
                                                                   : MC_MATH_CLASS_COLD,
                             1, strcmp(argv[1], "math-cold") == 0));
    CHECK(atf_math_state(s.natural.math, MC_MATH_CLASS_READY, 1, true));
    result = 0;
cleanup:
    /* Stop on refusal: never tear down producers or invent a backlink repair. */
    if (mc_arrow_tick_retire(&s.rejected) != MC_ARROW_TICK_OK ||
        mc_arrow_tick_retire(&s.call) != MC_ARROW_TICK_OK) {
        fputs("Arrow tick failure retirement refused; actual owners retained\n", stderr);
        return 1;
    }
    mc_entity_release(s.raw);
    s.raw = NULL;
    if (mc_math_classes_release(&s.extra_math, NULL) != MC_MATH_CLASS_OK ||
        !nf_cleanup(&s.natural)) {
        fputs("Arrow tick failure fixture cleanup refused; actual owners retained\n", stderr);
        return 1;
    }
    if (result == 0) {
        printf("ARROW_TICK_FAILURES_CASE %s END begins=%u published=%u preallocation_refusals=%u\n",
               argv[1], s.begins, s.published, s.preallocation_refusals);
        puts("ARROW_TICK_FAILURES_OPEN "
             "allocation-failure/diagnostic-oom/active-reentry/full-victim");
        printf("PASS: %u Arrow tick failure checks;mode=%s\n", checks, argv[1]);
    }
    return result;
}
