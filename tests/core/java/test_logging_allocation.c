#include "mc/core/java/logging.h"
#include <stdio.h>
#include <stdlib.h>

/* Test-only included-TU allocation control. Every request reaches real calloc;
 * selected native refusal frees only the just-created unpublished allocation.
 * No production callbacks or supplied completed children are introduced. */
static mc_java_logging *observed_service;
static size_t allocation_calls, fail_at;
static unsigned probes;
static bool reentry_ok = true;

static void *actual_calloc_control(size_t count, size_t bytes) {
    void *result = calloc(count, bytes);
    if (observed_service != NULL) {
        ++allocation_calls;
        mc_java_logging_error error;
        mc_java_logging *same = observed_service;
        mc_java_logger *none = NULL;
        if (mc_java_logging_retain(same, &error) != MC_JAVA_LOGGING_BUSY ||
            mc_java_logging_release(&same, &error) != MC_JAVA_LOGGING_BUSY ||
            same != observed_service ||
            mc_java_logging_retire(&same, &error) != MC_JAVA_LOGGING_BUSY ||
            same != observed_service ||
            mc_java_logging_get_minecraft(same, &none, &error) != MC_JAVA_LOGGING_BUSY ||
            none != NULL)
            reentry_ok = false;
        ++probes;
        if (allocation_calls == fail_at) {
            free(result);
            result = NULL;
        }
    }
    return result;
}

#define calloc actual_calloc_control
#include "../../../src/core/java/logging.c"
#undef calloc

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "logging allocation check%u line%d: %s\n", checks, __LINE__, #x);      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int exercise(void) {
    for (size_t cut = 1; cut <= 17; ++cut) {
        mc_java_logging *s = NULL;
        mc_java_logger *n = NULL;
        mc_java_logging_error e;
        CHECK(mc_java_logging_create(NULL, &s, &e) == MC_JAVA_LOGGING_OK);
        observed_service = s;
        allocation_calls = 0;
        fail_at = cut;
        reentry_ok = true;
        probes = 0;
        const mc_java_logging_status result = mc_java_logging_get_minecraft(s, &n, &e);
        observed_service = NULL;
        CHECK(reentry_ok && probes == allocation_calls &&
              allocation_calls == (cut == 17 ? 16 : cut));
        mc_java_logging_state st;
        CHECK(mc_java_logging_inspect(s, &st, &e) == MC_JAVA_LOGGING_OK && st.active_calls == 0 &&
              st.service_holds == 1 && !st.entered);
        if (cut <= 16) {
            CHECK(result == MC_JAVA_LOGGING_ALLOCATION_FAILED && n == NULL &&
                  st.phase == MC_JAVA_LOGGING_FAILED && st.native_allocations == cut - 1 &&
                  st.failure->status == MC_JAVA_LOGGING_ALLOCATION_FAILED);
            CHECK(mc_java_logging_get_minecraft(s, &n, &e) == MC_JAVA_LOGGING_LATCHED_FAILURE &&
                  e.cause_status == MC_JAVA_LOGGING_ALLOCATION_FAILED && n == NULL);
        } else {
            CHECK(result == MC_JAVA_LOGGING_OK && n == st.minecraft && st.native_allocations == 16);
            CHECK(mc_java_logger_release(&n, &e) == MC_JAVA_LOGGING_OK && n == NULL);
        }
        CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK && s == NULL);
    }
    /* Declared native-counter limits on a genuine quiescent READY owner;
     * no source field, seed, object or Java allocation exception is supplied. */
    mc_java_logging *s = NULL;
    mc_java_logger *n = NULL;
    mc_java_logging_error e;
    CHECK(mc_java_logging_create(NULL, &s, &e) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_logging_get_minecraft(s, &n, &e) == MC_JAVA_LOGGING_OK);
    atomic_store_explicit(&n->external_holds, SIZE_MAX, memory_order_release);
    CHECK(mc_java_logger_retain(n, &e) == MC_JAVA_LOGGING_LIMIT);
    const size_t total = atomic_load_explicit(&s->external_logger_holds, memory_order_acquire);
    CHECK(total == 1);
    atomic_store_explicit(&n->external_holds, 1u, memory_order_release);
    atomic_store_explicit(&s->service_holds, SIZE_MAX, memory_order_release);
    CHECK(mc_java_logging_retain(s, &e) == MC_JAVA_LOGGING_LIMIT);
    atomic_store_explicit(&s->service_holds, 1u, memory_order_release);
    CHECK(mc_java_logger_release(&n, &e) == MC_JAVA_LOGGING_OK && n == NULL);
    CHECK(mc_java_logging_retire(&s, &e) == MC_JAVA_LOGGING_OK && s == NULL);
    return 0;
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1) {
        fputs("Logging allocation input: no arguments expected\n", stderr);
        return 2;
    }
    CHECK(exercise() == 0);
    printf("Core logging allocation/reentry contracts: %u checks\n", checks);
    return 0;
}
