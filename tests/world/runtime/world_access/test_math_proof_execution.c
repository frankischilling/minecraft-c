/* Compile the unchanged actual Math class once, as in the existing core Math
 * controls. Delegates below count reached helper calls only; they never supply
 * a result or replace initialization. The sole native active-flag projection
 * creates a real refused cleanup and is restored before any assertion. */
#include "../../../../src/core/math/classes.c"

static size_t mp_retains, mp_ensures;

static mc_math_class_status mp_counted_retain(mc_math_classes *classes,
                                              mc_math_class_error *error) {
    ++mp_retains;
    return mc_math_classes_retain(classes, error);
}

static mc_math_class_status mp_counted_ensure(mc_math_classes *classes,
                                              mc_math_class_error *error) {
    ++mp_ensures;
    return mc_math_classes_ensure(classes, error);
}

#define mc_math_classes_retain mp_counted_retain
#define mc_math_classes_ensure mp_counted_ensure
#include "core/math/execution_internal.h"
#undef mc_math_classes_ensure
#undef mc_math_classes_retain

#include "world/runtime/runtime_execution_internal.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define MP_CHECK(value)                                                                            \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Math proof line%d check%u: %s\n", __LINE__, checks, #value);          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct mp_fixture {
    mc_math_classes *limited, *ready;
    mc_math_execution first, second, missing_a, missing_b, successful;
    mc_math_execution_failure fresh, sticky, prior_class, missing_one, missing_two;
    mc_math_execution_failure retiring, retired;
    mc_math_class_status fresh_status, sticky_status, prior_status, missing_status;
    mc_math_class_status retiring_status, retired_status;
    mc_world_runtime_execution_result parent, child, other, empty, marked;
} mp_fixture;

static const mc_math_execution_site mp_floor = {"fd", "b", "(Lsn;)Z", 8};
static const mc_math_execution_site mp_scatter = {"gs", "a", "(Liz;Z)V", 83};
static const mc_math_execution_site mp_normal = {"gs", "a", "(Liz;Z)V", 132};
static const mc_math_execution_site mp_retiring = {"native.math.proof", "retiring", "()V", 4};
static const mc_math_execution_site mp_retired = {"native.math.proof", "retired", "()V", 5};
static const mc_entity_tick_execution_site mp_native = {"native.math.proof", "forward", "()V", 6};
/* Shared actual arguments make the two missing-owner proofs pointer-identical
 * without depending on the compiler merging separate string literals. */
static const char mp_missing_method[] = "b", mp_missing_descriptor[] = "(D)I";

static bool mp_site_same(mc_math_execution_site a, mc_math_execution_site b) {
    return a.class_name == b.class_name && a.method == b.method && a.descriptor == b.descriptor &&
           a.pc == b.pc;
}

/* require and require_recorded materialize their own child labels. Their
 * returned origins agree by text/PC; copied proof VALUES below still require
 * exact pointer identity through mp_site_same and mp_failure_same. */
static bool mp_child_text_same(mc_math_execution_site a, mc_math_execution_site b) {
    return a.class_name != NULL && b.class_name != NULL && a.method != NULL && b.method != NULL &&
           a.descriptor != NULL && b.descriptor != NULL && !strcmp(a.class_name, b.class_name) &&
           !strcmp(a.method, b.method) && !strcmp(a.descriptor, b.descriptor) && a.pc == b.pc;
}

static bool mp_cause_same(mc_math_class_error a, mc_math_class_error b) {
    return a.status == b.status && a.operation == b.operation && a.source_pc == b.source_pc &&
           a.initial_cause == b.initial_cause;
}

static bool mp_failure_same(mc_math_execution_failure a, mc_math_execution_failure b) {
    return mp_site_same(a.caller, b.caller) && mp_site_same(a.child, b.child) &&
           mp_cause_same(a.cause, b.cause);
}

static bool mp_quiet(const mc_world_runtime_execution_result *result) {
    if (result->selection_ready || result->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (result->closes[i].used)
            return false;
    return true;
}

static bool mp_result_same(const mc_world_runtime_execution_result *a,
                           const mc_world_runtime_execution_result *b) {
    return mp_quiet(a) && mp_quiet(b) && a->ownership_status == b->ownership_status &&
           a->ownership_error.status == b->ownership_error.status &&
           a->ownership_error.operation == b->ownership_error.operation &&
           a->ownership_error.site.class_name == b->ownership_error.site.class_name &&
           a->ownership_error.site.method == b->ownership_error.site.method &&
           a->ownership_error.site.descriptor == b->ownership_error.site.descriptor &&
           a->ownership_error.site.pc == b->ownership_error.site.pc &&
           a->math_failed == b->math_failed && a->math_owner == b->math_owner &&
           a->math_status == b->math_status && mp_failure_same(a->math_failure, b->math_failure);
}

static bool mp_proofs(mp_fixture *s) {
    mc_math_execution_init(&s->first, NULL);
    mc_math_execution_init(&s->second, NULL);
    mc_math_execution_init(&s->missing_a, NULL);
    mc_math_execution_init(&s->missing_b, NULL);
    mc_math_execution_init(&s->successful, NULL);
    MP_CHECK(mc_math_classes_create(1024, &s->limited, NULL) == MC_MATH_CLASS_OK &&
             mc_math_classes_create(SIZE_MAX, &s->ready, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->first, s->limited);
    mc_math_execution_init(&s->second, s->limited);
    mc_math_execution_init(&s->successful, s->ready);
    size_t retains = mp_retains, ensures = mp_ensures;
    s->fresh_status =
        mc_math_execution_require_recorded(&s->first, mp_floor, "b", "(D)I", &s->fresh);
    const mc_math_execution_failure first = s->first.first;
    MP_CHECK(
        s->fresh_status == MC_MATH_CLASS_LIMIT && mp_retains == retains + 1 &&
        mp_ensures == ensures + 1 && s->first.failed && s->first.owned == s->limited &&
        mp_site_same(s->fresh.caller, mp_floor) && s->fresh.child.pc == 2 &&
        !strcmp(s->fresh.child.class_name, "in") && !strcmp(s->fresh.child.method, "<clinit>") &&
        !strcmp(s->fresh.child.descriptor, "()V") && s->fresh.cause.status == MC_MATH_CLASS_LIMIT &&
        s->fresh.cause.source_pc == 2 && s->fresh.cause.initial_cause == MC_MATH_CLASS_OK &&
        mp_site_same(first.caller, s->fresh.caller) &&
        mp_child_text_same(first.child, s->fresh.child) &&
        mp_cause_same(first.cause, s->fresh.cause));
    mc_math_class_state state;
    MP_CHECK(mc_math_classes_state(s->limited, &state, NULL) == MC_MATH_CLASS_OK &&
             state.phase == MC_MATH_CLASS_FAILED && state.holds == 2 && state.source_a == NULL &&
             state.last_store_pc == 0);
    retains = mp_retains;
    ensures = mp_ensures;
    s->sticky_status =
        mc_math_execution_require_recorded(&s->first, mp_scatter, "a", "(F)F", &s->sticky);
    MP_CHECK(s->sticky_status == MC_MATH_CLASS_LIMIT && mp_retains == retains &&
             mp_ensures == ensures && mp_site_same(s->sticky.caller, mp_scatter) &&
             mp_cause_same(s->sticky.cause, s->fresh.cause) &&
             mp_child_text_same(s->sticky.child, s->fresh.child) &&
             mp_failure_same(s->first.first, first) && s->limited->state.holds == 2);
    retains = mp_retains;
    ensures = mp_ensures;
    s->prior_status =
        mc_math_execution_require_recorded(&s->second, mp_floor, "b", "(D)I", &s->prior_class);
    MP_CHECK(s->prior_status == MC_MATH_CLASS_INITIALIZATION_FAILED &&
             s->prior_class.cause.status == MC_MATH_CLASS_INITIALIZATION_FAILED &&
             s->prior_class.cause.initial_cause == MC_MATH_CLASS_LIMIT &&
             s->fresh.cause.status == MC_MATH_CLASS_LIMIT &&
             mp_site_same(s->prior_class.caller, mp_floor) && s->prior_class.child.pc == 2 &&
             mp_retains == retains + 1 && mp_ensures == ensures + 1 &&
             s->second.owned == s->limited && s->limited->state.holds == 3 &&
             s->limited->state.source_a == NULL);
    retains = mp_retains;
    ensures = mp_ensures;
    s->missing_status = mc_math_execution_require_recorded(
        &s->missing_a, mp_floor, mp_missing_method, mp_missing_descriptor, &s->missing_one);
    const mc_math_class_status other_missing = mc_math_execution_require_recorded(
        &s->missing_b, mp_floor, mp_missing_method, mp_missing_descriptor, &s->missing_two);
    MP_CHECK(s->missing_status == MC_MATH_CLASS_INVALID_ARGUMENT &&
             other_missing == s->missing_status && mp_retains == retains + 2 &&
             mp_ensures == ensures && s->missing_a.failed && s->missing_b.failed &&
             s->missing_a.owned == NULL && s->missing_b.owned == NULL &&
             mp_failure_same(s->missing_one, s->missing_two) && s->missing_one.child.pc == -1 &&
             s->missing_one.child.method == mp_missing_method &&
             s->missing_one.child.descriptor == mp_missing_descriptor &&
             !strcmp(s->missing_one.child.method, "b") && s->missing_one.cause.source_pc == -1 &&
             s->missing_one.cause.operation != NULL);
    mc_math_execution_failure preserved = s->fresh;
    retains = mp_retains;
    ensures = mp_ensures;
    MP_CHECK(mc_math_execution_require_recorded(&s->successful, mp_normal, "a", "(F)F",
                                                &preserved) == MC_MATH_CLASS_OK &&
             mp_failure_same(preserved, s->fresh) && mp_retains == retains + 1 &&
             mp_ensures == ensures + 1 &&
             mc_math_classes_state(s->ready, &state, NULL) == MC_MATH_CLASS_OK &&
             state.phase == MC_MATH_CLASS_READY && state.holds == 2 && state.last_store_pc == 37 &&
             state.source_a != NULL && state.source_a == mc_math_classes_table(s->ready));
    const mc_math_trig *const table = state.source_a;
    retains = mp_retains;
    ensures = mp_ensures;
    MP_CHECK(mc_math_execution_require_recorded(&s->successful, mp_scatter, "b", "(F)F",
                                                &preserved) == MC_MATH_CLASS_OK &&
             mp_failure_same(preserved, s->fresh) && mp_retains == retains &&
             mp_ensures == ensures + 1 && s->successful.owned == s->ready &&
             !s->successful.failed && !s->successful.selected && s->ready->state.holds == 2 &&
             s->ready->state.source_a == table);
    retains = mp_retains;
    ensures = mp_ensures;
    MP_CHECK(mc_math_execution_require_recorded(NULL, mp_floor, "b", "(D)I", &preserved) ==
                 MC_MATH_CLASS_OK &&
             mp_failure_same(preserved, s->fresh) && mp_retains == retains &&
             mp_ensures == ensures);
    s->ready->active = true;
    const mc_math_class_status refused = mc_math_execution_retire(&s->successful);
    s->ready->active = false; /* Restore exact native flag before checked observation. */
    MP_CHECK(refused == MC_MATH_CLASS_BUSY && s->successful.phase == MC_MATH_EXECUTION_RETIRING &&
             s->successful.owned == s->ready && s->ready->state.holds == 2);
    retains = mp_retains;
    ensures = mp_ensures;
    s->retiring_status =
        mc_math_execution_require_recorded(&s->successful, mp_retiring, "a", "(F)F", &s->retiring);
    MP_CHECK(s->retiring_status == MC_MATH_CLASS_STATE_ERROR && mp_retains == retains &&
             mp_ensures == ensures && mp_site_same(s->retiring.caller, mp_retiring) &&
             s->retiring.child.pc == -1 && !strcmp(s->retiring.child.method, "a") &&
             s->retiring.cause.source_pc == -1 &&
             s->retiring.cause.status == MC_MATH_CLASS_STATE_ERROR && !s->successful.failed &&
             s->ready->state.source_a == table &&
             mc_math_execution_retire(&s->successful) == MC_MATH_CLASS_OK &&
             s->successful.phase == MC_MATH_EXECUTION_RETIRED && s->successful.owned == NULL &&
             s->ready->state.holds == 1);
    retains = mp_retains;
    ensures = mp_ensures;
    s->retired_status =
        mc_math_execution_require_recorded(&s->successful, mp_retired, "b", "(D)I", &s->retired);
    MP_CHECK(s->retired_status == MC_MATH_CLASS_STATE_ERROR && mp_retains == retains &&
             mp_ensures == ensures && mp_site_same(s->retired.caller, mp_retired) &&
             s->retired.child.pc == -1 && !strcmp(s->retired.child.method, "b") &&
             s->retired.cause.source_pc == -1 && s->ready->state.source_a == table &&
             s->ready->state.holds == 1);
    return true;
}

static bool mp_note(mp_fixture *s) {
    mc_world_runtime_execution_result *r = &s->parent;
    mc_world_runtime_execution_result_init(r);
    MP_CHECK(!r->math_failed && r->math_owner == NULL && r->math_status == MC_MATH_CLASS_OK &&
             r->ownership_status == MC_WORLD_STORAGE_OK && mp_quiet(r));
    mc_world_runtime_execution_result_note(r, MC_WORLD_STORAGE_BUSY, "Preserved native diagnostic",
                                           mp_native);
    MP_CHECK(mc_world_runtime_execution_result_note_math(r, &s->first, s->fresh_status,
                                                         &s->fresh) == MC_WORLD_STORAGE_OK &&
             r->math_failed && r->math_owner == &s->first && r->math_status == s->fresh_status &&
             mp_failure_same(r->math_failure, s->fresh) &&
             r->ownership_status == MC_WORLD_STORAGE_BUSY &&
             r->ownership_error.site.pc == mp_native.pc && mp_quiet(r));
    const mc_world_runtime_execution_result before = *r; /* Ordinary VALUE, no owning occurrence. */
    MP_CHECK(
        mc_world_runtime_execution_result_note_math(r, &s->first, s->fresh_status, &s->fresh) ==
            MC_WORLD_STORAGE_OK &&
        mp_result_same(r, &before) &&
        mc_world_runtime_execution_result_note_math(r, &s->first, s->sticky_status, &s->sticky) ==
            MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
        mp_result_same(r, &before) &&
        mc_world_runtime_execution_result_note_math(r, &s->second, s->prior_status,
                                                    &s->prior_class) ==
            MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
        mp_result_same(r, &before));
    MP_CHECK(mc_world_runtime_execution_result_note_math(r, NULL, s->fresh_status, &s->fresh) ==
                 MC_WORLD_STORAGE_INVALID_ARGUMENT &&
             mp_result_same(r, &before) &&
             mc_world_runtime_execution_result_note_math(
                 r, &s->first, MC_MATH_CLASS_OK, &s->fresh) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
             mp_result_same(r, &before) &&
             mc_world_runtime_execution_result_note_math(r, &s->first, s->fresh_status, NULL) ==
                 MC_WORLD_STORAGE_INVALID_ARGUMENT &&
             mp_result_same(r, &before) &&
             mc_world_runtime_execution_result_note_math(
                 NULL, &s->first, s->fresh_status, &s->fresh) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    mc_world_runtime_execution_result_init(r);
    MP_CHECK(!r->math_failed && r->math_owner == NULL && r->math_status == MC_MATH_CLASS_OK &&
             r->ownership_status == MC_WORLD_STORAGE_OK && r->ownership_error.operation == NULL &&
             mp_quiet(r) &&
             mc_world_runtime_execution_result_note_math(r, &s->second, s->prior_status,
                                                         &s->prior_class) == MC_WORLD_STORAGE_OK &&
             r->math_status == MC_MATH_CLASS_INITIALIZATION_FAILED &&
             r->math_failure.cause.status == MC_MATH_CLASS_INITIALIZATION_FAILED &&
             r->math_failure.cause.initial_cause == MC_MATH_CLASS_LIMIT);
    return true;
}

static bool mp_forward(mp_fixture *s) {
    mc_world_runtime_execution_result *parent = &s->parent, *child = &s->child;
    mc_world_runtime_execution_result_init(parent);
    mc_world_runtime_execution_result_init(child);
    mc_world_runtime_execution_result_init(&s->other);
    mc_world_runtime_execution_result_init(&s->empty);
    MP_CHECK(mc_world_runtime_execution_result_note_math(child, &s->missing_a, s->missing_status,
                                                         &s->missing_one) == MC_WORLD_STORAGE_OK &&
             mc_world_runtime_execution_result_forward(parent, child) == MC_WORLD_STORAGE_OK &&
             mp_result_same(parent, child));
    const mc_world_runtime_execution_result before = *parent;
    MP_CHECK(mc_world_runtime_execution_result_forward(parent, child) == MC_WORLD_STORAGE_OK &&
             mp_result_same(parent, &before) &&
             mc_world_runtime_execution_result_forward(parent, parent) == MC_WORLD_STORAGE_OK &&
             mp_result_same(parent, &before) &&
             mc_world_runtime_execution_result_forward(parent, &s->empty) == MC_WORLD_STORAGE_OK &&
             mp_result_same(parent, &before));
    /* Two actual missing-namespace requires produced identical failure VALUES.
     * Only their real, still-live execution-owner identities differ. */
    MP_CHECK(mc_world_runtime_execution_result_note_math(&s->other, &s->missing_b,
                                                         s->missing_status,
                                                         &s->missing_two) == MC_WORLD_STORAGE_OK &&
             mp_failure_same(child->math_failure, s->other.math_failure));
    mc_world_runtime_execution_result_note(&s->other, MC_WORLD_STORAGE_IO_ERROR,
                                           "Child native diagnostic must not leak on conflict",
                                           mp_native);
    MP_CHECK(mc_world_runtime_execution_result_forward(parent, &s->other) ==
                 MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
             mp_result_same(parent, &before) && parent->ownership_status == MC_WORLD_STORAGE_OK &&
             mc_world_runtime_execution_result_note_math(parent, &s->missing_b, s->missing_status,
                                                         &s->missing_two) ==
                 MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
             mp_result_same(parent, &before));
    mc_world_runtime_execution_result_init(parent);
    mc_world_runtime_execution_result_init(child);
    MP_CHECK(mc_world_runtime_execution_result_note_math(parent, &s->first, s->fresh_status,
                                                         &s->fresh) == MC_WORLD_STORAGE_OK &&
             mc_world_runtime_execution_result_note_math(child, &s->first, s->sticky_status,
                                                         &s->sticky) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_execution_result caller_before = *parent;
    mc_world_runtime_execution_result_note(child, MC_WORLD_STORAGE_LIMIT,
                                           "Actual child native diagnostic remains separate",
                                           mp_native);
    MP_CHECK(mc_world_runtime_execution_result_forward(parent, child) ==
                 MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
             mp_result_same(parent, &caller_before));
    mc_world_runtime_execution_result_init(parent);
    mc_world_runtime_execution_result_init(child);
    MP_CHECK(mc_world_runtime_execution_result_note_math(parent, &s->successful, s->retiring_status,
                                                         &s->retiring) == MC_WORLD_STORAGE_OK &&
             mc_world_runtime_execution_result_note_math(child, &s->successful, s->retired_status,
                                                         &s->retired) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_execution_result phase_before = *parent;
    MP_CHECK(mc_world_runtime_execution_result_forward(parent, child) ==
                 MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
             mp_result_same(parent, &phase_before) &&
             mc_world_runtime_execution_result_forward(NULL, child) ==
                 MC_WORLD_STORAGE_INVALID_ARGUMENT &&
             mc_world_runtime_execution_result_forward(parent, NULL) ==
                 MC_WORLD_STORAGE_INVALID_ARGUMENT &&
             mp_result_same(parent, &phase_before));
    return true;
}

static bool mp_cleanup(mp_fixture *s) {
    /* Result owner pointers are identity-only borrows, expired before contexts. */
    s->parent = s->child = s->other = s->empty = s->marked = (mc_world_runtime_execution_result){0};
    MP_CHECK(mc_math_execution_retire(&s->first) == MC_MATH_CLASS_OK &&
             mc_math_execution_retire(&s->second) == MC_MATH_CLASS_OK &&
             mc_math_execution_retire(&s->missing_a) == MC_MATH_CLASS_OK &&
             mc_math_execution_retire(&s->missing_b) == MC_MATH_CLASS_OK &&
             mc_math_execution_retire(&s->successful) == MC_MATH_CLASS_OK &&
             mc_math_classes_release(&s->limited, NULL) == MC_MATH_CLASS_OK &&
             mc_math_classes_release(&s->ready, NULL) == MC_MATH_CLASS_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2 ||
        (strcmp(argv[1], "require") && strcmp(argv[1], "note") && strcmp(argv[1], "forward")))
        return 2;
    printf("MATH_PROOF_BEGIN schema=1 mode=%s\n", argv[1]);
    mp_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    const bool passed = mp_proofs(s) && (strcmp(argv[1], "note") != 0 || mp_note(s)) &&
                        (strcmp(argv[1], "forward") != 0 || mp_forward(s));
    const bool cleaned = mp_cleanup(s);
    free(s);
    if (!passed || !cleaned)
        return 1;
    printf("PASS: %u actual Math proof checks;mode=%s\n", checks, argv[1]);
    return 0;
}
