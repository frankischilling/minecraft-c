#include "../../../src/core/java/random_profile_internal.h"
#include "../../support/java_random.h"

#include <emmintrin.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define WORKERS 6u
#define CALLS 1024u
#define TOTAL_CALLS (WORKERS * CALLS)
#define INITIAL_UNIQUIFIER UINT64_C(8682522807148012)
#define UNIQUIFIER_MULTIPLIER UINT64_C(1181783497276652981)
#define LCG_MASK UINT64_C(0xffffffffffff)

_Static_assert(TOTAL_CALLS <= MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS,
               "all actual concurrent constructors fit the recorder bound");

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Adapted from the actual C17 worker/gate/join pattern in test_random_profile.c.
 * This synchronization is outside constructors. It never holds or changes the
 * recorder gate, clock, uniquifier, Math phase or an actual Random's seed. */
typedef struct start_gate {
    mtx_t mutex;
    cnd_t condition;
    size_t ready;
    bool open;
} start_gate;

typedef struct worker {
    start_gate *gate;
    mc_java_random_profile *profile;
    mc_java_random_profile_status status[CALLS];
    mc_java_random ordinary[CALLS];
    size_t returned;
    bool fp_ready;
} worker;

typedef struct expected_u {
    uint64_t value;
    size_t ordinal;
} expected_u;

typedef struct output_identity {
    uintptr_t value;
    size_t index;
} output_identity;

typedef struct scenario_result {
    bool qualified;
    size_t source_calls, source_ok, recorded;
    unsigned int failures, first_failure;
    uint64_t final_uniquifier;
} scenario_result;

static uint64_t double_bits(const double *value) {
    uint64_t bits;
    memcpy(&bits, value, sizeof(bits));
    return bits;
}

static void poison(mc_java_random *random) {
    memset(random, 0xa5, sizeof(*random));
    random->have_next_next_gaussian = true;
}

static bool fresh(const mc_java_random *random) {
    return random->state <= LCG_MASK && double_bits(&random->next_next_gaussian) == 0 &&
           !random->have_next_next_gaussian;
}

static uint64_t advanced(uint64_t value, size_t count) {
    for (size_t i = 0; i < count; ++i)
        value *= UNIQUIFIER_MULTIPLIER;
    return value;
}

static void require_thread_success(int status) {
    /* A failed lock/wait/join cannot authorize freeing a potentially live user.
     * Abort instead; the external bounded controller must reap that process. */
    if (status != thrd_success) {
        fprintf(stderr, "C17 thread synchronization/join failed: %d\n", status);
        abort();
    }
}

static int worker_main(void *argument) {
    worker *work = argument;
    require_thread_success(mtx_lock(&work->gate->mutex));
    ++work->gate->ready;
    require_thread_success(cnd_broadcast(&work->gate->condition));
    while (!work->gate->open)
        require_thread_success(cnd_wait(&work->gate->condition, &work->gate->mutex));
    require_thread_success(mtx_unlock(&work->gate->mutex));
    work->fp_ready = (_mm_getcsr() & 0xffc0u) == 0x1f80u;
    if (!work->fp_ready)
        return 1;
    int result = 0;
    for (size_t i = 0; i < CALLS; ++i) {
        work->status[i] = mc_java_random_construct_default(work->profile, &work->ordinary[i]);
        ++work->returned;
        if (work->status[i] != MC_JAVA_RANDOM_PROFILE_OK)
            result = 1;
    }
    return result;
}

static bool run_workers(mc_java_random_profile *profile, worker work[WORKERS]) {
    start_gate gate;
    thrd_t threads[WORKERS];
    size_t created = 0;
    bool complete = true;
    if (mtx_init(&gate.mutex, mtx_plain) != thrd_success)
        return false;
    if (cnd_init(&gate.condition) != thrd_success) {
        mtx_destroy(&gate.mutex);
        return false;
    }
    gate.ready = 0;
    gate.open = false;
    for (size_t i = 0; i < WORKERS; ++i) {
        work[i].gate = &gate;
        work[i].profile = profile;
        work[i].returned = 0;
        work[i].fp_ready = false;
        for (size_t j = 0; j < CALLS; ++j) {
            work[i].status[j] = MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT;
            poison(&work[i].ordinary[j]);
        }
        const int create_status = thrd_create(&threads[i], worker_main, &work[i]);
        if (create_status != thrd_success) {
            fprintf(stderr, "C17 thread create failed after %zu/%u workers: %d\n", created, WORKERS,
                    create_status);
            complete = false;
            break;
        }
        ++created;
    }
    /* Wait for every ACTUALLY created borrower, then release and join them all,
     * including after a partial create failure. No borrower can stay at gate. */
    require_thread_success(mtx_lock(&gate.mutex));
    while (gate.ready != created)
        require_thread_success(cnd_wait(&gate.condition, &gate.mutex));
    gate.open = true;
    require_thread_success(cnd_broadcast(&gate.condition));
    require_thread_success(mtx_unlock(&gate.mutex));
    for (size_t i = 0; i < created; ++i) {
        int status = 1;
        require_thread_success(thrd_join(threads[i], &status));
        if (status != 0)
            complete = false;
    }
    cnd_destroy(&gate.condition);
    mtx_destroy(&gate.mutex);
    return complete && created == WORKERS;
}

static bool source_outputs_valid(const worker work[WORKERS]) {
    for (size_t i = 0; i < WORKERS; ++i) {
        if (!work[i].fp_ready || work[i].returned != CALLS) {
            fprintf(stderr, "worker %zu: fp_ready=%u returned=%zu\n", i, work[i].fp_ready ? 1u : 0u,
                    work[i].returned);
            return false;
        }
        for (size_t j = 0; j < CALLS; ++j) {
            if (work[i].status[j] != MC_JAVA_RANDOM_PROFILE_OK || !fresh(&work[i].ordinary[j])) {
                fprintf(stderr, "worker %zu call %zu: source_status=%d fresh=%u\n", i, j,
                        (int)work[i].status[j], fresh(&work[i].ordinary[j]) ? 1u : 0u);
                return false;
            }
        }
    }
    return true;
}

static int compare_u(const void *left, const void *right) {
    const expected_u *a = left, *b = right;
    return (a->value > b->value) - (a->value < b->value);
}

static int compare_identity(const void *left, const void *right) {
    const output_identity *a = left, *b = right;
    return (a->value > b->value) - (a->value < b->value);
}

static const expected_u *find_u(const expected_u *values, uint64_t value) {
    size_t low = 0, high = TOTAL_CALLS;
    while (low < high) {
        const size_t middle = low + (high - low) / 2u;
        if (values[middle].value == value)
            return &values[middle];
        if (values[middle].value < value)
            low = middle + 1u;
        else
            high = middle;
    }
    return NULL;
}

static const output_identity *find_identity(const output_identity *values, uintptr_t value) {
    size_t low = 0, high = TOTAL_CALLS;
    while (low < high) {
        const size_t middle = low + (high - low) / 2u;
        if (values[middle].value == value)
            return &values[middle];
        if (values[middle].value < value)
            low = middle + 1u;
        else
            high = middle;
    }
    return NULL;
}

static int run_scenario(bool overflow_first, scenario_result *summary) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    worker *work = NULL;
    mc_java_random_constructor_record *records = NULL;
    expected_u *expected = NULL;
    output_identity *identities = NULL;
    bool *seen = NULL;
    const char *const name = overflow_first ? "overflow-first" : "ample";
    const size_t capacity = overflow_first ? 0 : TOTAL_CALLS;
    const size_t prefix = overflow_first ? 1 : 0;
    size_t prefix_returned = 0, prefix_ok = 0;
    unsigned char spare_before[sizeof(mc_java_random_constructor_record)];
    mc_java_random prefix_random;
    mc_java_random_constructor_records_view view = {0};
    *summary = (scenario_result){0};
    printf("RANDOM_RECORD_OVERLAP_CASE name=%s BEGIN\n", name);
    fflush(stdout);
    work = calloc(WORKERS, sizeof(*work));
    CHECK(work != NULL);
    if (!overflow_first) {
        records = malloc((TOTAL_CALLS + 1u) * sizeof(*records));
        expected = malloc(TOTAL_CALLS * sizeof(*expected));
        identities = malloc(TOTAL_CALLS * sizeof(*identities));
        seen = calloc(TOTAL_CALLS, sizeof(*seen));
        CHECK(records != NULL && expected != NULL && identities != NULL && seen != NULL);
        memset(records, 0xa5, (TOTAL_CALLS + 1u) * sizeof(*records));
        memcpy(spare_before, &records[TOTAL_CALLS], sizeof(spare_before));
        uint64_t next = INITIAL_UNIQUIFIER;
        for (size_t i = 0; i < TOTAL_CALLS; ++i) {
            next *= UNIQUIFIER_MULTIPLIER;
            expected[i] = (expected_u){next, i + 1u};
            identities[i] = (output_identity){(uintptr_t)&work[i / CALLS].ordinary[i % CALLS], i};
        }
        qsort(expected, TOTAL_CALLS, sizeof(*expected), compare_u);
        qsort(identities, TOTAL_CALLS, sizeof(*identities), compare_identity);
        for (size_t i = 1; i < TOTAL_CALLS; ++i) {
            CHECK(expected[i - 1u].value != expected[i].value);
            CHECK(identities[i - 1u].value != identities[i].value);
        }
    }
    CHECK(mc_java_random_profile_create(sizeof(*profile), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_records(profile, records, capacity) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    if (overflow_first) {
        poison(&prefix_random);
        const mc_java_random_profile_status prefix_status =
            mc_java_random_construct_default(profile, &prefix_random);
        ++prefix_returned;
        prefix_ok += prefix_status == MC_JAVA_RANDOM_PROFILE_OK ? 1u : 0u;
        if (prefix_status != MC_JAVA_RANDOM_PROFILE_OK)
            fprintf(stderr, "serialized prefix source_status=%d\n", (int)prefix_status);
        CHECK(prefix_status == MC_JAVA_RANDOM_PROFILE_OK && fresh(&prefix_random));
        CHECK(mc_java_random_profile_constructor_records_get(profile, &view) ==
                  MC_JAVA_RANDOM_PROFILE_OK &&
              view.bound && !view.active && view.capacity == 0 && view.recorded == 0 &&
              view.failures == MC_JAVA_RANDOM_RECORD_OVERFLOW &&
              view.first_failure == MC_JAVA_RANDOM_RECORD_OVERFLOW);
        CHECK(atomic_load_explicit(&profile->uniquifier, memory_order_seq_cst) ==
              advanced(INITIAL_UNIQUIFIER, 1));
        CHECK(mc_java_random_profile_constructor_records_complete(profile) ==
              MC_JAVA_RANDOM_PROFILE_RANGE);
    }
    /* This returns only after every actual created borrower has been joined. */
    const bool workers_complete = run_workers(profile, work);
    /* Every created worker is already joined, even when creation or an actual
     * Source call failed. Retain the first actual worker/status diagnostic. */
    const bool outputs_valid = source_outputs_valid(work);
    CHECK(workers_complete);
    CHECK(outputs_valid);
    size_t actual_returned = prefix_returned, actual_ok = prefix_ok;
    for (size_t i = 0; i < WORKERS; ++i) {
        actual_returned += work[i].returned;
        for (size_t j = 0; j < CALLS; ++j)
            actual_ok += work[i].status[j] == MC_JAVA_RANDOM_PROFILE_OK ? 1u : 0u;
    }
    CHECK(actual_returned == TOTAL_CALLS + prefix && actual_ok == actual_returned);
    CHECK(mc_java_random_profile_constructor_records_get(profile, &view) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(view.bound && !view.active && view.capacity == capacity && view.recorded <= capacity);
    CHECK(atomic_load_explicit(&profile->uniquifier, memory_order_seq_cst) ==
          advanced(INITIAL_UNIQUIFIER, actual_returned));
    CHECK(atomic_load_explicit(&profile->constructor_admitted, memory_order_relaxed));
    CHECK(atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
              MC_JAVA_RANDOM_HOLDER_COLD &&
          profile->math == NULL &&
          mc_java_random_profile_allocation_bytes(profile) == sizeof(*profile));
    const bool concurrent = (view.failures & MC_JAVA_RANDOM_RECORD_CONCURRENT) != 0;
    if (overflow_first) {
        CHECK(view.recorded == 0 && view.first_failure == MC_JAVA_RANDOM_RECORD_OVERFLOW &&
              view.failures == (unsigned int)(MC_JAVA_RANDOM_RECORD_OVERFLOW |
                                              (concurrent ? MC_JAVA_RANDOM_RECORD_CONCURRENT : 0)));
        CHECK(mc_java_random_profile_constructor_records_complete(profile) ==
              MC_JAVA_RANDOM_PROFILE_RANGE);
    } else {
        CHECK(view.failures == (unsigned int)(concurrent ? MC_JAVA_RANDOM_RECORD_CONCURRENT
                                                         : MC_JAVA_RANDOM_RECORD_NONE) &&
              view.first_failure == view.failures && view.recorded != 0);
        CHECK(concurrent ? view.recorded < TOTAL_CALLS : view.recorded == TOTAL_CALLS);
        CHECK(mc_java_random_profile_constructor_records_complete(profile) ==
              (concurrent ? MC_JAVA_RANDOM_PROFILE_RANGE : MC_JAVA_RANDOM_PROFILE_OK));
        CHECK(memcmp(spare_before, &records[TOTAL_CALLS], sizeof(spare_before)) == 0);
        size_t previous_ordinal = 0;
        for (size_t i = 0; i < view.recorded; ++i) {
            const mc_java_random_constructor_record *record = &records[i];
            CHECK(record->kind == MC_JAVA_RANDOM_CONSTRUCTOR_ORDINARY &&
                  record->math_destination == 0 && record->sample_returned &&
                  record->tail_returned && record->sample_status == MC_JAVA_RANDOM_PROFILE_OK);
            const expected_u *u = find_u(expected, record->committed_uniquifier);
            const output_identity *identity = find_identity(identities, record->destination);
            CHECK(u != NULL && u->ordinal > previous_ordinal);
            CHECK(identity != NULL && identity->index < TOTAL_CALLS && !seen[identity->index]);
            previous_ordinal = u->ordinal;
            seen[identity->index] = true;
            const mc_java_random *actual =
                &work[identity->index / CALLS].ordinary[identity->index % CALLS];
            mc_java_random forward;
            poison(&forward);
            /* A qualified real-tail plumbing check, not an independent clock or
             * arithmetic oracle. Nothing is written back into the actual output. */
            mc_java_random_finish_default(record->committed_uniquifier, record->nanotime, &forward);
            CHECK(mc_test_java_random_equal(actual, &forward));
        }
    }
    summary->qualified = concurrent;
    summary->source_calls = actual_returned;
    summary->source_ok = actual_ok;
    summary->recorded = view.recorded;
    summary->failures = view.failures;
    summary->first_failure = view.first_failure;
    summary->final_uniquifier = atomic_load_explicit(&profile->uniquifier, memory_order_seq_cst);
    printf("RANDOM_RECORD_OVERLAP_CASE name=%s END source_calls=%zu source_ok=%zu recorded=%zu "
           "capacity=%zu failures=%u first_failure=%u concurrent_observed=%u qualified=%u "
           "final_uniquifier=%016" PRIx64 "\n",
           name, summary->source_calls, summary->source_ok, view.recorded, capacity, view.failures,
           view.first_failure, concurrent ? 1u : 0u, summary->qualified ? 1u : 0u,
           summary->final_uniquifier);
    fflush(stdout);
    result = 0;
cleanup:
    /* run_workers never returns with a live created borrower, even on failure.
     * The real profile is retired before any borrowed row/output storage. */
    mc_java_random_profile_free(profile);
    free(seen);
    free(identities);
    free(expected);
    free(records);
    free(work);
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fprintf(stderr, "recorder overlap tests require the qualified strict SSE environment\n");
        return 1;
    }
    printf("RANDOM_RECORD_OVERLAP_BEGIN schema=1 workers=%u calls_per_worker=%u\n", WORKERS, CALLS);
    fflush(stdout);
    scenario_result ample, overflow;
    if (run_scenario(false, &ample) || run_scenario(true, &overflow))
        return 1;
    printf("Random profile recorder overlap: %u checks; source_valid=1; "
           "ample_concurrent_qualified=%u; overflow_first_cause_qualified=%u; qualification=%s\n",
           checks, ample.qualified ? 1u : 0u, overflow.qualified ? 1u : 0u,
           ample.qualified && overflow.qualified ? "QUALIFIED" : "UNQUALIFIED");
    return 0;
}
