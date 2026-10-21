#include "mc/core/java/int_set.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int arguments(void) {
    mc_java_int_set *set = NULL;
    bool changed = true;
    int32_t first = 123;
    size_t count = 45;
    CHECK(mc_java_int_set_create(NULL, NULL) == MC_JAVA_INT_SET_INVALID_ARGUMENT);
    CHECK(mc_java_int_set_create(NULL, &set) == MC_JAVA_INT_SET_OK);
    CHECK(mc_java_int_set_size(NULL) == 0);
    CHECK(!mc_java_int_set_contains(NULL, 0));
    CHECK(mc_java_int_set_add(NULL, 1, &changed) == MC_JAVA_INT_SET_INVALID_ARGUMENT && changed);
    CHECK(mc_java_int_set_add(set, 1, NULL) == MC_JAVA_INT_SET_INVALID_ARGUMENT);
    CHECK(mc_java_int_set_remove(NULL, 1, &changed) == MC_JAVA_INT_SET_INVALID_ARGUMENT && changed);
    CHECK(mc_java_int_set_remove(set, 1, NULL) == MC_JAVA_INT_SET_INVALID_ARGUMENT);
    CHECK(mc_java_int_set_first(NULL, &first) == MC_JAVA_INT_SET_INVALID_ARGUMENT && first == 123);
    CHECK(mc_java_int_set_first(set, NULL) == MC_JAVA_INT_SET_INVALID_ARGUMENT);
    CHECK(mc_java_int_set_first(set, &first) == MC_JAVA_INT_SET_EMPTY && first == 123);
    CHECK(mc_java_int_set_copy_order(NULL, NULL, 0, &count) == MC_JAVA_INT_SET_INVALID_ARGUMENT &&
          count == 45);
    CHECK(mc_java_int_set_copy_order(set, NULL, 1, &count) == MC_JAVA_INT_SET_INVALID_ARGUMENT &&
          count == 45);
    CHECK(mc_java_int_set_copy_order(set, NULL, 0, NULL) == MC_JAVA_INT_SET_INVALID_ARGUMENT);
    CHECK(mc_java_int_set_copy_order(set, NULL, 0, &count) == MC_JAVA_INT_SET_OK && count == 0);
    CHECK(mc_java_int_set_remove(set, 1, &changed) == MC_JAVA_INT_SET_OK && !changed);
    CHECK(mc_java_int_set_add(set, INT32_MIN, &changed) == MC_JAVA_INT_SET_OK && changed);
    CHECK(mc_java_int_set_add(set, INT32_MAX, &changed) == MC_JAVA_INT_SET_OK && changed);
    CHECK(mc_java_int_set_add(set, INT32_MIN, &changed) == MC_JAVA_INT_SET_OK && !changed);
    int32_t buffer[] = {13, 17, 19};
    count = 45;
    CHECK(mc_java_int_set_copy_order(set, buffer, 1, &count) == MC_JAVA_INT_SET_LIMIT);
    CHECK(buffer[0] == 13 && buffer[1] == 17 && buffer[2] == 19 && count == 45);
    CHECK(mc_java_int_set_copy_order(set, buffer, 3, &count) == MC_JAVA_INT_SET_OK && count == 2);
    CHECK(buffer[2] == 19 && buffer[0] != buffer[1]);
    CHECK(mc_java_int_set_contains(set, buffer[0]) && mc_java_int_set_contains(set, buffer[1]));
    mc_java_int_set_clear(set);
    CHECK(mc_java_int_set_size(set) == 0 && !mc_java_int_set_contains(set, INT32_MIN));
    mc_java_int_set_free(set);
    mc_java_int_set_clear(NULL);
    mc_java_int_set_free(NULL);
    return 0;
}

static int entry_bound(void) {
    mc_java_int_set_limits limits = mc_java_int_set_default_limits();
    mc_java_int_set *set = NULL;
    limits.max_entries = 0;
    CHECK(mc_java_int_set_create(&limits, &set) == MC_JAVA_INT_SET_OK);
    bool changed = false;
    CHECK(mc_java_int_set_add(set, 0, &changed) == MC_JAVA_INT_SET_LIMIT && !changed);
    CHECK(mc_java_int_set_size(set) == 0);
    mc_java_int_set_free(set);
    limits.max_entries = 2;
    CHECK(mc_java_int_set_create(&limits, &set) == MC_JAVA_INT_SET_OK);
    CHECK(mc_java_int_set_add(set, 0, &changed) == MC_JAVA_INT_SET_OK && changed);
    CHECK(mc_java_int_set_add(set, 64, &changed) == MC_JAVA_INT_SET_OK && changed);
    CHECK(mc_java_int_set_add(set, 0, &changed) == MC_JAVA_INT_SET_OK && !changed);
    CHECK(mc_java_int_set_add(set, 128, &changed) == MC_JAVA_INT_SET_LIMIT && !changed);
    CHECK(mc_java_int_set_size(set) == 2 && !mc_java_int_set_contains(set, 128));
    CHECK(mc_java_int_set_remove(set, 64, &changed) == MC_JAVA_INT_SET_OK && changed);
    CHECK(mc_java_int_set_add(set, 128, &changed) == MC_JAVA_INT_SET_OK && changed);
    mc_java_int_set_free(set);
    limits.max_entries = (size_t)INT32_MAX + 1u;
    set = NULL;
    CHECK(mc_java_int_set_create(&limits, &set) == MC_JAVA_INT_SET_INVALID_ARGUMENT && set == NULL);
    limits = mc_java_int_set_default_limits();
    limits.max_retained_bytes = 0;
    CHECK(mc_java_int_set_create(&limits, &set) == MC_JAVA_INT_SET_LIMIT && set == NULL);
    limits = mc_java_int_set_default_limits();
    limits.max_allocation_bytes = 0;
    CHECK(mc_java_int_set_create(&limits, &set) == MC_JAVA_INT_SET_LIMIT && set == NULL);
    return 0;
}

static int same_order(const mc_java_int_set *left, const mc_java_int_set *right) {
    int32_t a[160], b[160];
    size_t an = 0, bn = 0;
    CHECK(mc_java_int_set_copy_order(left, a, 160, &an) == MC_JAVA_INT_SET_OK);
    CHECK(mc_java_int_set_copy_order(right, b, 160, &bn) == MC_JAVA_INT_SET_OK);
    CHECK(an == bn && memcmp(a, b, an * sizeof(*a)) == 0);
    for (size_t i = 0; i < an; ++i) {
        CHECK(mc_java_int_set_contains(left, a[i]) && mc_java_int_set_contains(right, a[i]));
    }
    return 0;
}

static int byte_bounds(void) {
    /* Sweep both byte budgets around node/table growth. The control omits every
     * rejected add, then follows the same removals/reinsertions. Equal subsequent
     * traversal checks that failed growth did not alter hidden capacity/history. */
    size_t limited_creates = 0, limited_adds = 0, successful_adds = 0;
    for (unsigned mode = 0; mode < 2; ++mode) {
        for (size_t budget = 0; budget <= 12288; budget += 31) {
            mc_java_int_set_limits limits = mc_java_int_set_default_limits();
            limits.max_entries = 160;
            if (mode == 0) {
                limits.max_retained_bytes = budget;
            } else {
                limits.max_allocation_bytes = budget;
            }
            mc_java_int_set *set = NULL, *control = NULL;
            mc_java_int_set_status created = mc_java_int_set_create(&limits, &set);
            if (created == MC_JAVA_INT_SET_LIMIT) {
                ++limited_creates;
                CHECK(set == NULL);
                continue;
            }
            CHECK(created == MC_JAVA_INT_SET_OK);
            CHECK(mc_java_int_set_create(&limits, &control) == MC_JAVA_INT_SET_OK);
            for (int32_t step = 0; step < 144; ++step) {
                int32_t key = step < 24 ? step * 64 : step - 23;
                bool changed = false, control_changed = false;
                mc_java_int_set_status status = mc_java_int_set_add(set, key, &changed);
                if (status == MC_JAVA_INT_SET_LIMIT) {
                    ++limited_adds;
                    CHECK(!changed && !mc_java_int_set_contains(set, key));
                } else {
                    ++successful_adds;
                    CHECK(status == MC_JAVA_INT_SET_OK);
                    CHECK(mc_java_int_set_add(control, key, &control_changed) ==
                          MC_JAVA_INT_SET_OK);
                    CHECK(changed == control_changed);
                }
                CHECK(same_order(set, control) == 0);
                if (step % 7 == 6 && mc_java_int_set_size(set) != 0) {
                    int32_t first = 0;
                    CHECK(mc_java_int_set_first(set, &first) == MC_JAVA_INT_SET_OK);
                    CHECK(mc_java_int_set_remove(set, first, &changed) == MC_JAVA_INT_SET_OK &&
                          changed);
                    CHECK(mc_java_int_set_remove(control, first, &control_changed) ==
                              MC_JAVA_INT_SET_OK &&
                          control_changed);
                    CHECK(same_order(set, control) == 0);
                }
            }
            mc_java_int_set_clear(set);
            mc_java_int_set_clear(control);
            for (int32_t key = 16; key <= 32; key += 16) {
                bool a = false, b = false;
                CHECK(mc_java_int_set_add(set, key, &a) == mc_java_int_set_add(control, key, &b));
                CHECK(a == b && same_order(set, control) == 0);
            }
            mc_java_int_set_free(set);
            mc_java_int_set_free(control);
        }
    }
    CHECK(limited_creates != 0 && limited_adds != 0 && successful_adds != 0);
    return 0;
}

int main(void) {
    if (arguments() != 0 || entry_bound() != 0 || byte_bounds() != 0) {
        return 1;
    }
    printf("PASS: %u integer-set contract checks\n", checks);
    return 0;
}
