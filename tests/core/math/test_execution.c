/* The actual class body is compiled once in this test translation unit so
 * native ownership fault projections can name its opaque fields. Ordinary
 * integration tests link the normal mc_math_classes archive. No source body
 * is replaced; every temporary native fault is restored before an assertion. */
#include "../../../src/core/math/classes.c"
#include "../../../src/core/math/execution_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(test)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #test);                     \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const mc_math_execution_site floor_site = {"fd", "a", "(Leq;Lln;Lsn;)Z", 4};
static const mc_math_execution_site sqrt_site = {"sn", "U", "()V", 138};

static bool same_site(const mc_math_execution_site *a, const mc_math_execution_site *b) {
    return a->class_name == b->class_name && a->method == b->method &&
           a->descriptor == b->descriptor && a->pc == b->pc;
}

static bool same_failure(const mc_math_execution_failure *a, const mc_math_execution_failure *b) {
    return same_site(&a->caller, &b->caller) && same_site(&a->child, &b->child) &&
           a->cause.status == b->cause.status && a->cause.operation == b->cause.operation &&
           a->cause.source_pc == b->cause.source_pc &&
           a->cause.initial_cause == b->cause.initial_cause;
}

static bool unused_and_missing(void) {
    bool ok = false;
    mc_math_classes *math = NULL;
    mc_math_execution unused, missing;
    mc_math_execution_init(&unused, NULL);
    mc_math_execution_init(&missing, NULL);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&unused, math);
    mc_math_class_state state;
    CHECK(mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_COLD && state.holds == 1 && state.source_a == NULL &&
          unused.owned == NULL && !unused.selected && !unused.failed);
    /* The caller's actual last occurrence can disappear on an unused path.
     * Retire must not query/dereference the unused borrowed declaration. */
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK && math == NULL &&
          mc_math_execution_retire(&unused) == MC_MATH_CLASS_OK &&
          unused.phase == MC_MATH_EXECUTION_RETIRED && unused.declared == NULL &&
          unused.owned == NULL && mc_math_execution_retire(&unused) == MC_MATH_CLASS_OK);
    CHECK(mc_math_execution_require(&missing, floor_site, "b", "(D)I") ==
              MC_MATH_CLASS_INVALID_ARGUMENT &&
          missing.failed && missing.selected && missing.owned == NULL &&
          missing.first.caller.pc == 4 && missing.first.cause.source_pc == -1 &&
          strcmp(missing.first.child.class_name, "in") == 0 &&
          strcmp(missing.first.child.method, "b") == 0);
    const mc_math_execution_failure first = missing.first;
    CHECK(mc_math_execution_require(&missing, sqrt_site, "a", "(D)F") ==
              MC_MATH_CLASS_INVALID_ARGUMENT &&
          same_failure(&missing.first, &first) && missing.owned == NULL);
    CHECK(mc_math_execution_retire(&missing) == MC_MATH_CLASS_OK &&
          missing.phase == MC_MATH_EXECUTION_RETIRED && !missing.selected &&
          same_failure(&missing.first, &first));
    CHECK(mc_math_execution_require(&unused, floor_site, "b", "(D)I") ==
              MC_MATH_CLASS_STATE_ERROR &&
          unused.admission.source_pc == -1 && unused.owned == NULL && !unused.failed);
    ok = true;
cleanup:
    if (mc_math_execution_retire(&unused) != MC_MATH_CLASS_OK ||
        mc_math_execution_retire(&missing) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    return ok;
}

static bool success_and_native_ownership(void) {
    bool ok = false;
    mc_math_classes *math = NULL, *other = NULL;
    mc_math_execution execution;
    mc_math_execution_init(&execution, NULL);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_create(SIZE_MAX, &other, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    CHECK(mc_math_execution_require(&execution, floor_site, "b", "(D)I") == MC_MATH_CLASS_OK);
    mc_math_class_state state;
    CHECK(mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_READY && state.holds == 2 && state.last_store_pc == 37 &&
          state.source_a != NULL && state.source_a == mc_math_classes_table(math) &&
          execution.owned == math && !execution.selected && !execution.failed);
    const mc_math_trig *const table = state.source_a;
    const int later_floors[] = {15, 24, 35, 44, 55};
    for (size_t i = 0; i < sizeof(later_floors) / sizeof(later_floors[0]); ++i) {
        mc_math_execution_site site = floor_site;
        site.pc = later_floors[i];
        CHECK(mc_math_execution_require(&execution, site, "b", "(D)I") == MC_MATH_CLASS_OK &&
              mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK && state.holds == 2 &&
              state.source_a == table && !execution.selected);
    }
    CHECK(mc_math_execution_require(&execution, sqrt_site, "a", "(D)F") == MC_MATH_CLASS_OK &&
          mc_math_execution_cause(&execution)->status == MC_MATH_CLASS_OK);
    /* This is a caller-storage protocol fault, not a Java namespace mutation. */
    execution.declared = other;
    const mc_math_class_status changed =
        mc_math_execution_require(&execution, floor_site, "b", "(D)I");
    execution.declared = math;
    CHECK(changed == MC_MATH_CLASS_STATE_ERROR && execution.failed && execution.owned == math &&
          execution.first.cause.source_pc == -1 &&
          mc_math_classes_state(other, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_COLD && state.holds == 1 && state.source_a == NULL);
    const mc_math_execution_failure first = execution.first;
    math->active = true;
    const mc_math_class_status refused = mc_math_execution_retire(&execution);
    math->active = false;
    CHECK(refused == MC_MATH_CLASS_BUSY && execution.phase == MC_MATH_EXECUTION_RETIRING &&
          execution.owned == math && execution.cleanup.status == MC_MATH_CLASS_BUSY &&
          same_failure(&first, &execution.first));
    CHECK(mc_math_execution_require(&execution, sqrt_site, "a", "(D)F") ==
              MC_MATH_CLASS_STATE_ERROR &&
          execution.admission.status == MC_MATH_CLASS_STATE_ERROR &&
          same_failure(&first, &execution.first));
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK &&
          execution.phase == MC_MATH_EXECUTION_RETIRED && execution.owned == NULL &&
          execution.declared == NULL && same_failure(&first, &execution.first) &&
          mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK && state.holds == 1 &&
          state.phase == MC_MATH_CLASS_READY && state.source_a == table);
    ok = true;
cleanup:
    if (mc_math_execution_retire(&execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&other, NULL) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    return ok;
}

static bool retain_refusal_and_sticky_failure(void) {
    bool ok = false;
    mc_math_classes *math = NULL, *limited = NULL;
    mc_math_execution overflow, first_call, second_call;
    mc_math_execution_init(&overflow, NULL);
    mc_math_execution_init(&first_call, NULL);
    mc_math_execution_init(&second_call, NULL);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_create(sizeof(mc_math_trig), &limited, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&overflow, math);
    const size_t holds = math->state.holds;
    math->state.holds = SIZE_MAX;
    const mc_math_class_status refused =
        mc_math_execution_require(&overflow, floor_site, "b", "(D)I");
    math->state.holds = holds;
    CHECK(refused == MC_MATH_CLASS_LIMIT && overflow.owned == NULL && overflow.failed &&
          overflow.first.cause.source_pc == -1 && math->state.phase == MC_MATH_CLASS_COLD &&
          math->state.source_a == NULL && math->state.holds == 1);
    CHECK(mc_math_execution_retire(&overflow) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&first_call, limited);
    CHECK(mc_math_execution_require(&first_call, floor_site, "b", "(D)I") == MC_MATH_CLASS_LIMIT &&
          first_call.owned == limited && first_call.failed && first_call.first.caller.pc == 4 &&
          first_call.first.child.pc == 2 && first_call.first.cause.source_pc == 2 &&
          strcmp(first_call.first.child.method, "<clinit>") == 0 &&
          limited->state.phase == MC_MATH_CLASS_FAILED && limited->state.holds == 2 &&
          limited->state.source_a == NULL && limited->state.last_store_pc == 0);
    CHECK(mc_math_execution_retire(&first_call) == MC_MATH_CLASS_OK && limited->state.holds == 1);
    mc_math_execution_init(&second_call, limited);
    CHECK(mc_math_execution_require(&second_call, sqrt_site, "a", "(D)F") ==
              MC_MATH_CLASS_INITIALIZATION_FAILED &&
          second_call.owned == limited && second_call.first.caller.pc == 138 &&
          second_call.first.child.pc == 2 &&
          second_call.first.cause.initial_cause == MC_MATH_CLASS_LIMIT &&
          limited->state.source_a == NULL && limited->state.holds == 2);
    CHECK(mc_math_execution_retire(&second_call) == MC_MATH_CLASS_OK && limited->state.holds == 1);
    ok = true;
cleanup:
    if (mc_math_execution_retire(&overflow) != MC_MATH_CLASS_OK ||
        mc_math_execution_retire(&first_call) != MC_MATH_CLASS_OK ||
        mc_math_execution_retire(&second_call) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&limited, NULL) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    return ok;
}

static bool published_zero_prefix(void) {
    bool ok = false;
    mc_math_classes *math = NULL;
    mc_math_execution execution;
    mc_math_execution_init(&execution, NULL);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&execution, math);
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const mc_math_class_status status =
        mc_math_execution_require(&execution, floor_site, "b", "(D)I");
    _mm_setcsr(saved);
    CHECK(status == MC_MATH_CLASS_UNSUPPORTED_FP && execution.failed && execution.owned == math &&
          execution.first.caller.pc == 4 && execution.first.child.pc == 20 &&
          math->state.phase == MC_MATH_CLASS_FAILED && math->state.last_store_pc == 4 &&
          math->state.source_a != NULL && mc_math_classes_table(math) == NULL &&
          math->state.holds == 2);
    uint32_t nonzero = 0;
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i) {
        uint32_t bits;
        memcpy(&bits, &math->state.source_a->values[i], sizeof(bits));
        nonzero |= bits;
    }
    CHECK(nonzero == 0);
    const mc_math_execution_failure first = execution.first;
    CHECK(mc_math_execution_require(&execution, sqrt_site, "a", "(D)F") ==
              MC_MATH_CLASS_UNSUPPORTED_FP &&
          same_failure(&first, &execution.first) && math->state.last_store_pc == 4 &&
          math->state.holds == 2);
    CHECK(mc_math_execution_retire(&execution) == MC_MATH_CLASS_OK && math->state.holds == 1);
    ok = true;
cleanup:
    if (mc_math_execution_retire(&execution) != MC_MATH_CLASS_OK ||
        mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        ok = false;
    return ok;
}

int main(void) {
    if (!unused_and_missing() || !success_and_native_ownership() ||
        !retain_refusal_and_sticky_failure() || !published_zero_prefix())
        return 1;
    return printf("PASS: %u Math execution ownership checks\n", checks) < 0 ? 1 : 0;
}
