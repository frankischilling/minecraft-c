#include "mc/world/schedule/membership.h"
#include "mc/world/schedule/ordered.h"

#include <stdio.h>
#include <string.h>

/* Public-contract fixtures, derived without reading ordered.c. No world or
 * allocation/hold failure stubs; all owning CHECK paths release their holds. */
static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool size_is(const mc_scheduled_ordered *ordered, int32_t expected)
{
    int32_t actual = -1;
    return mc_scheduled_ordered_size(ordered, &actual) == MC_SCHEDULED_ORDERED_OK &&
           actual == expected;
}

static bool bytes_is(const mc_scheduled_ordered *ordered, size_t expected)
{
    size_t actual = SIZE_MAX;
    return mc_scheduled_ordered_native_bytes(ordered, &actual) == MC_SCHEDULED_ORDERED_OK &&
           actual == expected;
}

static bool record_matches(const mc_scheduled_tick *tick, const mc_scheduled_tick_fields *expected)
{
    mc_scheduled_tick_fields actual = {0};
    return mc_scheduled_tick_fields_get(tick, &actual) == MC_SCHEDULED_TICK_OK &&
           actual.x == expected->x && actual.y == expected->y && actual.z == expected->z &&
           actual.block == expected->block && actual.due_time == expected->due_time &&
           actual.sequence == expected->sequence;
}

static bool first_is(const mc_scheduled_ordered *ordered, const mc_scheduled_tick *expected)
{
    mc_scheduled_tick *held = NULL;
    const mc_scheduled_ordered_status status = mc_scheduled_ordered_first(ordered, &held);
    const bool result = status == MC_SCHEDULED_ORDERED_OK && held == expected;
    mc_scheduled_tick_release(held);
    return result;
}

static int test_invalid_empty_identity_and_limits(void)
{
    int result = 1;
    mc_scheduled_ordered *zero = NULL, *failed = NULL, *reference = NULL;
    mc_scheduled_ordered *capacity = NULL, *tight = NULL;
    mc_scheduled_tick *original = NULL, *duplicate = NULL, *later = NULL, *held = NULL;
    mc_scheduled_tick_sequence sequence = {0}, reused_sequence_value = {0};
    mc_scheduled_tick_fields expected = {0}, duplicate_fields = {0}, later_fields = {0};
    size_t owner_bytes = 0, one_bytes = 0, byte_output = SIZE_MAX;
    int32_t size_output = -73, comparison = 7;
    bool flag = true, equal = true;

    for (int status = MC_SCHEDULED_ORDERED_OK; status <= MC_SCHEDULED_ORDERED_EMPTY; ++status) {
        const char *text = mc_scheduled_ordered_status_string((mc_scheduled_ordered_status)status);
        CHECK(text != NULL && strlen(text) != 0);
    }
    mc_scheduled_ordered_destroy(NULL);
    CHECK(mc_scheduled_ordered_create(0, SIZE_MAX, &zero) == MC_SCHEDULED_ORDERED_OK);
    CHECK(size_is(zero, 0));
    CHECK(mc_scheduled_ordered_native_bytes(zero, &owner_bytes) == MC_SCHEDULED_ORDERED_OK &&
          owner_bytes > 0);
    CHECK(mc_scheduled_ordered_first(zero, &held) == MC_SCHEDULED_ORDERED_EMPTY && held == NULL);
    CHECK(mc_scheduled_ordered_create(1, owner_bytes - 1, &failed) == MC_SCHEDULED_ORDERED_LIMIT);
    CHECK(failed == NULL);
    CHECK(mc_scheduled_ordered_create(1, 0, &failed) == MC_SCHEDULED_ORDERED_LIMIT &&
          failed == NULL);
    CHECK(mc_scheduled_ordered_create((size_t)INT32_MAX + 1u, SIZE_MAX, &failed) ==
              MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          failed == NULL);
    CHECK(mc_scheduled_ordered_create(0, SIZE_MAX, NULL) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT);
    CHECK(mc_scheduled_ordered_size(NULL, &size_output) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT);
    CHECK(size_output == -73);
    CHECK(mc_scheduled_ordered_size(zero, NULL) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT);
    CHECK(mc_scheduled_ordered_native_bytes(NULL, &byte_output) ==
              MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          byte_output == SIZE_MAX);
    CHECK(mc_scheduled_ordered_native_bytes(zero, NULL) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT);
    CHECK(mc_scheduled_ordered_first(NULL, &held) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          held == NULL);
    CHECK(mc_scheduled_ordered_first(zero, NULL) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT);

    CHECK(mc_scheduled_tick_create(&sequence, -1, 0, 0, 17, &original) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_time_set(original, 10) == MC_SCHEDULED_TICK_OK);
    /* Explicit represented-key fixture for sequence reuse, not a game counter reset. */
    CHECK(mc_scheduled_tick_create(&reused_sequence_value, 91, -3, 5, 42, &duplicate) ==
          MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_time_set(duplicate, 10) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_create(&sequence, -1, 0, 0, 17, &later) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_time_set(later, 11) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(original, &expected) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(duplicate, &duplicate_fields) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(later, &later_fields) == MC_SCHEDULED_TICK_OK);
    CHECK(original != duplicate);
    CHECK(mc_scheduled_tick_compare(original, duplicate, &comparison) == MC_SCHEDULED_TICK_OK &&
          comparison == 0);
    CHECK(mc_scheduled_tick_equals(original, duplicate, &equal) == MC_SCHEDULED_TICK_OK && !equal);
    CHECK(mc_scheduled_tick_equals(original, later, &equal) == MC_SCHEDULED_TICK_OK && equal);
    CHECK(mc_scheduled_ordered_add(zero, original, &flag) == MC_SCHEDULED_ORDERED_LIMIT && flag);
    CHECK(size_is(zero, 0) && bytes_is(zero, owner_bytes));
    CHECK(mc_scheduled_ordered_add(NULL, original, &flag) ==
              MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          flag);
    CHECK(mc_scheduled_ordered_add(zero, NULL, &flag) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          flag);
    CHECK(mc_scheduled_ordered_add(zero, original, NULL) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT);
    flag = false;
    CHECK(mc_scheduled_ordered_remove(NULL, original, &flag) ==
              MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          !flag);
    CHECK(mc_scheduled_ordered_remove(zero, NULL, &flag) == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          !flag);
    CHECK(mc_scheduled_ordered_remove(zero, original, NULL) ==
          MC_SCHEDULED_ORDERED_INVALID_ARGUMENT);
    flag = true;
    CHECK(mc_scheduled_ordered_remove(zero, original, &flag) == MC_SCHEDULED_ORDERED_OK && !flag);

    CHECK(mc_scheduled_ordered_create(2, SIZE_MAX, &reference) == MC_SCHEDULED_ORDERED_OK);
    CHECK(bytes_is(reference, owner_bytes));
    CHECK(mc_scheduled_ordered_add(reference, original, &flag) == MC_SCHEDULED_ORDERED_OK && flag);
    CHECK(mc_scheduled_ordered_native_bytes(reference, &one_bytes) == MC_SCHEDULED_ORDERED_OK &&
          one_bytes > owner_bytes);
    CHECK(mc_scheduled_ordered_add(reference, later, &flag) == MC_SCHEDULED_ORDERED_OK && flag);
    CHECK(size_is(reference, 2)); /* Full membership equality is not E's comparator equality. */
    CHECK(mc_scheduled_ordered_first(reference, &held) == MC_SCHEDULED_ORDERED_OK &&
          held == original);
    CHECK(mc_scheduled_ordered_create(1, one_bytes, &capacity) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_ordered_create(2, one_bytes, &tight) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_ordered_add(capacity, original, &flag) == MC_SCHEDULED_ORDERED_OK && flag);
    CHECK(mc_scheduled_ordered_add(tight, original, &flag) == MC_SCHEDULED_ORDERED_OK && flag);
    flag = true;
    CHECK(mc_scheduled_ordered_add(capacity, duplicate, &flag) == MC_SCHEDULED_ORDERED_OK && !flag);
    CHECK(first_is(capacity, original) && size_is(capacity, 1) && bytes_is(capacity, one_bytes));
    flag = true;
    CHECK(mc_scheduled_ordered_add(tight, duplicate, &flag) == MC_SCHEDULED_ORDERED_OK && !flag);
    CHECK(first_is(tight, original) && size_is(tight, 1) && bytes_is(tight, one_bytes));
    flag = true;
    CHECK(mc_scheduled_ordered_add(capacity, later, &flag) == MC_SCHEDULED_ORDERED_LIMIT && flag);
    flag = false;
    CHECK(mc_scheduled_ordered_add(tight, later, &flag) == MC_SCHEDULED_ORDERED_LIMIT && !flag);
    CHECK(size_is(capacity, 1) && bytes_is(capacity, one_bytes));
    CHECK(size_is(tight, 1) && bytes_is(tight, one_bytes));
    CHECK(record_matches(original, &expected) && record_matches(duplicate, &duplicate_fields));
    CHECK(record_matches(later, &later_fields));
    CHECK(mc_scheduled_ordered_remove(tight, original, &flag) == MC_SCHEDULED_ORDERED_OK && flag);
    CHECK(size_is(tight, 0) && bytes_is(tight, owner_bytes));
    CHECK(mc_scheduled_ordered_add(tight, later, &flag) == MC_SCHEDULED_ORDERED_OK && flag);
    CHECK(first_is(tight, later) && bytes_is(tight, one_bytes));
    CHECK(mc_scheduled_ordered_remove(reference, duplicate, &flag) == MC_SCHEDULED_ORDERED_OK &&
          flag);
    CHECK(size_is(reference, 1) && first_is(reference, later));
    CHECK(record_matches(duplicate, &duplicate_fields) && record_matches(held, &expected));
    mc_scheduled_ordered_destroy(reference);
    reference = NULL;
    mc_scheduled_ordered_destroy(capacity);
    capacity = NULL;
    mc_scheduled_ordered_destroy(tight);
    tight = NULL;
    mc_scheduled_ordered_destroy(zero);
    zero = NULL;
    mc_scheduled_tick_release(original);
    original = NULL;
    CHECK(record_matches(held, &expected)); /* Only first's independent hold remains. */
    CHECK(record_matches(duplicate, &duplicate_fields) && record_matches(later, &later_fields));
    result = 0;
cleanup:
    mc_scheduled_ordered_destroy(zero);
    mc_scheduled_ordered_destroy(failed);
    mc_scheduled_ordered_destroy(reference);
    mc_scheduled_ordered_destroy(capacity);
    mc_scheduled_ordered_destroy(tight);
    mc_scheduled_tick_release(held);
    mc_scheduled_tick_release(original);
    mc_scheduled_tick_release(duplicate);
    mc_scheduled_tick_release(later);
    return result;
}

static int test_successor_hold_transfer(void)
{
    int result = 1;
    mc_scheduled_ordered *ordered = NULL;
    mc_scheduled_tick *ticks[3] = {NULL}, *first_hold = NULL, *successor_hold = NULL;
    const mc_scheduled_tick *successor_identity = NULL; /* Borrowed through its E node. */
    mc_scheduled_tick_sequence sequence = {0};
    mc_scheduled_tick_fields expected[3] = {0};
    bool changed = false;
    size_t before_bytes = 0, after_bytes = 0;

    CHECK(mc_scheduled_ordered_create(3, SIZE_MAX, &ordered) == MC_SCHEDULED_ORDERED_OK);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_scheduled_tick_create(&sequence, (int32_t)i, 64, 0, 17, &ticks[i]) ==
              MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_time_set(ticks[i], (int64_t)i) == MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_fields_get(ticks[i], &expected[i]) == MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_ordered_add(ordered, ticks[i], &changed) == MC_SCHEDULED_ORDERED_OK &&
              changed);
    }
    /* Captured three-key insertion: B root, A left, C right. C has only its
     * E hold while B's two-child deletion transfers C into the surviving node. */
    successor_identity = ticks[2];
    mc_scheduled_tick_release(ticks[2]);
    ticks[2] = NULL;
    CHECK(mc_scheduled_ordered_native_bytes(ordered, &before_bytes) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_ordered_remove(ordered, ticks[1], &changed) == MC_SCHEDULED_ORDERED_OK &&
          changed);
    CHECK(size_is(ordered, 2) && first_is(ordered, ticks[0]));
    CHECK(mc_scheduled_ordered_native_bytes(ordered, &after_bytes) == MC_SCHEDULED_ORDERED_OK &&
          after_bytes < before_bytes);
    CHECK(record_matches(ticks[1], &expected[1]));
    mc_scheduled_tick_release(ticks[1]);
    ticks[1] = NULL;
    CHECK(mc_scheduled_ordered_first(ordered, &first_hold) == MC_SCHEDULED_ORDERED_OK &&
          first_hold == ticks[0]);
    CHECK(mc_scheduled_ordered_remove(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          changed);
    mc_scheduled_tick_release(ticks[0]);
    ticks[0] = NULL;
    CHECK(mc_scheduled_ordered_first(ordered, &successor_hold) == MC_SCHEDULED_ORDERED_OK &&
          successor_hold == successor_identity);
    CHECK(size_is(ordered, 1) && record_matches(successor_hold, &expected[2]));
    mc_scheduled_ordered_destroy(ordered);
    ordered = NULL;
    CHECK(record_matches(first_hold, &expected[0]) && record_matches(successor_hold, &expected[2]));
    result = 0;
cleanup:
    mc_scheduled_ordered_destroy(ordered);
    mc_scheduled_tick_release(first_hold);
    mc_scheduled_tick_release(successor_hold);
    for (size_t i = 0; i < 3; ++i)
        mc_scheduled_tick_release(ticks[i]);
    return result;
}

static int test_deletion_permutations(void)
{
    static const unsigned insertion[3][15] = {{7, 3, 11, 1, 5, 9, 13, 0, 2, 4, 6, 8, 10, 12, 14},
                                              {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                                              {14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
    static const unsigned deletion[3][15] = {{7, 3, 11, 1, 5, 9, 13, 0, 2, 4, 6, 8, 10, 12, 14},
                                             {0, 14, 1, 13, 2, 12, 3, 11, 4, 10, 5, 9, 6, 8, 7},
                                             {7, 6, 8, 5, 9, 4, 10, 3, 11, 2, 12, 1, 13, 0, 14}};
    int result = 1;
    mc_scheduled_ordered *ordered = NULL;
    mc_scheduled_tick *ticks[15] = {NULL}, *empty_output = NULL;
    mc_scheduled_tick_fields expected[15] = {0};
    mc_scheduled_tick_sequence sequence = {0};
    bool present[15] = {false}, changed = false;
    size_t owner_bytes = 0, before_bytes = 0, after_bytes = 0;

    for (size_t i = 0; i < 15; ++i) {
        CHECK(mc_scheduled_tick_create(&sequence, (int32_t)i, 0, 0, 17, &ticks[i]) ==
              MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_time_set(ticks[i], (int64_t)i) == MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_fields_get(ticks[i], &expected[i]) == MC_SCHEDULED_TICK_OK);
    }
    for (size_t profile = 0; profile < 3; ++profile) {
        memset(present, 0, sizeof(present));
        CHECK(mc_scheduled_ordered_create(15, SIZE_MAX, &ordered) == MC_SCHEDULED_ORDERED_OK);
        CHECK(mc_scheduled_ordered_native_bytes(ordered, &owner_bytes) == MC_SCHEDULED_ORDERED_OK);
        for (size_t i = 0; i < 15; ++i) {
            const unsigned key = insertion[profile][i];
            CHECK(!present[key]);
            CHECK(mc_scheduled_ordered_add(ordered, ticks[key], &changed) ==
                      MC_SCHEDULED_ORDERED_OK &&
                  changed);
            present[key] = true;
            unsigned minimum = 0;
            while (minimum < 15 && !present[minimum])
                ++minimum;
            CHECK(minimum < 15 && first_is(ordered, ticks[minimum]));
            CHECK(size_is(ordered, (int32_t)i + 1));
        }
        /* Expected answers come only from the active numeric keys, not a
         * second red-black implementation or observed private node layout. */
        for (size_t i = 0; i < 15; ++i) {
            const unsigned key = deletion[profile][i];
            CHECK(present[key]);
            CHECK(mc_scheduled_ordered_native_bytes(ordered, &before_bytes) ==
                  MC_SCHEDULED_ORDERED_OK);
            CHECK(mc_scheduled_ordered_remove(ordered, ticks[key], &changed) ==
                      MC_SCHEDULED_ORDERED_OK &&
                  changed);
            present[key] = false;
            CHECK(size_is(ordered, 14 - (int32_t)i));
            CHECK(mc_scheduled_ordered_native_bytes(ordered, &after_bytes) ==
                      MC_SCHEDULED_ORDERED_OK &&
                  after_bytes < before_bytes);
            changed = true;
            CHECK(mc_scheduled_ordered_remove(ordered, ticks[key], &changed) ==
                      MC_SCHEDULED_ORDERED_OK &&
                  !changed);
            CHECK(bytes_is(ordered, after_bytes));
            unsigned minimum = 0;
            while (minimum < 15 && !present[minimum])
                ++minimum;
            if (minimum < 15)
                CHECK(first_is(ordered, ticks[minimum]));
            else
                CHECK(mc_scheduled_ordered_first(ordered, &empty_output) ==
                          MC_SCHEDULED_ORDERED_EMPTY &&
                      empty_output == NULL);
            CHECK(record_matches(ticks[key], &expected[key]));
        }
        CHECK(bytes_is(ordered, owner_bytes));
        mc_scheduled_ordered_destroy(ordered);
        ordered = NULL;
        for (size_t i = 0; i < 15; ++i)
            CHECK(record_matches(ticks[i], &expected[i]));
    }
    result = 0;
cleanup:
    mc_scheduled_ordered_destroy(ordered);
    mc_scheduled_tick_release(empty_output);
    for (size_t i = 0; i < 15; ++i)
        mc_scheduled_tick_release(ticks[i]);
    return result;
}

static int test_mirrored_deletion_history(void)
{
    static const unsigned insertion[] = {4, 2, 6, 1, 3, 5, 7};
    static const unsigned pruning[] = {1, 3, 2};
    int result = 1;
    mc_scheduled_ordered *ordered = NULL;
    mc_scheduled_tick *ticks[8] = {NULL}, *empty_output = NULL;
    mc_scheduled_tick_fields expected[8] = {0};
    mc_scheduled_tick_sequence sequence = {0};
    bool changed = false;

    for (unsigned mirror = 0; mirror < 2; ++mirror) {
        const int64_t sign = mirror == 0 ? 1 : -1;
        CHECK(mc_scheduled_ordered_create(7, SIZE_MAX, &ordered) == MC_SCHEDULED_ORDERED_OK);
        for (unsigned i = 1; i <= 7; ++i) {
            CHECK(mc_scheduled_tick_create(&sequence, (int32_t)i, 0, 0, 17, &ticks[i]) ==
                  MC_SCHEDULED_TICK_OK);
            CHECK(mc_scheduled_tick_time_set(ticks[i], sign * (int64_t)i) == MC_SCHEDULED_TICK_OK);
            CHECK(mc_scheduled_tick_fields_get(ticks[i], &expected[i]) == MC_SCHEDULED_TICK_OK);
        }
        for (size_t i = 0; i < 7; ++i) {
            CHECK(mc_scheduled_ordered_add(ordered, ticks[insertion[i]], &changed) ==
                      MC_SCHEDULED_ORDERED_OK &&
                  changed);
        }
        /* Delete two red leaves, then their black parent. The captured far-red
         * sibling case rotates root4 to root6; negating due times mirrors it. */
        for (size_t i = 0; i < 3; ++i) {
            CHECK(mc_scheduled_ordered_remove(ordered, ticks[pruning[i]], &changed) ==
                      MC_SCHEDULED_ORDERED_OK &&
                  changed);
            CHECK(size_is(ordered, 6 - (int32_t)i));
        }
        CHECK(mc_scheduled_tick_time_set(ticks[4], sign * 10) == MC_SCHEDULED_TICK_OK);
        expected[4].due_time = sign * 10;
        CHECK(first_is(ordered, ticks[mirror == 0 ? 4 : 7]));
        CHECK(mc_scheduled_ordered_remove(ordered, ticks[4], &changed) == MC_SCHEDULED_ORDERED_OK &&
              !changed);
        CHECK(size_is(ordered, 4) && record_matches(ticks[4], &expected[4]));
        CHECK(mc_scheduled_tick_time_set(ticks[4], sign * 4) == MC_SCHEDULED_TICK_OK);
        expected[4].due_time = sign * 4;
        /* Black one-child replacement, black-leaf ascent, root replacement,
         * and sole-root removal, without any private topology observation. */
        CHECK(mc_scheduled_ordered_remove(ordered, ticks[4], &changed) == MC_SCHEDULED_ORDERED_OK &&
              changed);
        CHECK(size_is(ordered, 3) && first_is(ordered, ticks[mirror == 0 ? 5 : 7]));
        CHECK(mc_scheduled_ordered_remove(ordered, ticks[7], &changed) == MC_SCHEDULED_ORDERED_OK &&
              changed);
        CHECK(size_is(ordered, 2) && first_is(ordered, ticks[mirror == 0 ? 5 : 6]));
        CHECK(mc_scheduled_ordered_remove(ordered, ticks[6], &changed) == MC_SCHEDULED_ORDERED_OK &&
              changed);
        CHECK(size_is(ordered, 1) && first_is(ordered, ticks[5]));
        CHECK(mc_scheduled_ordered_remove(ordered, ticks[5], &changed) == MC_SCHEDULED_ORDERED_OK &&
              changed);
        CHECK(size_is(ordered, 0));
        CHECK(mc_scheduled_ordered_first(ordered, &empty_output) == MC_SCHEDULED_ORDERED_EMPTY &&
              empty_output == NULL);
        mc_scheduled_ordered_destroy(ordered);
        ordered = NULL;
        for (unsigned i = 1; i <= 7; ++i) {
            CHECK(record_matches(ticks[i], &expected[i]));
            mc_scheduled_tick_release(ticks[i]);
            ticks[i] = NULL;
        }
    }
    result = 0;
cleanup:
    mc_scheduled_ordered_destroy(ordered);
    mc_scheduled_tick_release(empty_output);
    for (size_t i = 0; i < 8; ++i)
        mc_scheduled_tick_release(ticks[i]);
    return result;
}

static int test_wrapped_and_individually_mutated_paths(void)
{
    int result = 1;
    mc_scheduled_ordered *ordered = NULL;
    mc_scheduled_membership *membership = NULL;
    mc_scheduled_tick *ticks[3] = {NULL}, *first_hold = NULL;
    mc_scheduled_tick_fields expected[3] = {0};
    mc_scheduled_tick_sequence sequence = {0};
    bool changed = false, contains = false;
    int32_t membership_size = -1, comparison = 7;
    size_t three_bytes = 0, four_bytes = 0;

    CHECK(mc_scheduled_ordered_create(4, SIZE_MAX, &ordered) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_membership_create(3, SIZE_MAX, &membership) == MC_SCHEDULED_MEMBERSHIP_OK);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_scheduled_tick_create(&sequence, (int32_t)i, 64, -1, 17, &ticks[i]) ==
              MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_time_set(ticks[i], (int64_t)i) == MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_membership_add(membership, ticks[i], &changed) ==
                  MC_SCHEDULED_MEMBERSHIP_OK &&
              changed);
        CHECK(mc_scheduled_ordered_add(ordered, ticks[i], &changed) == MC_SCHEDULED_ORDERED_OK &&
              changed);
        CHECK(mc_scheduled_tick_fields_get(ticks[i], &expected[i]) == MC_SCHEDULED_TICK_OK);
    }
    CHECK(mc_scheduled_ordered_native_bytes(ordered, &three_bytes) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_membership_shift_time(membership, INT64_MAX) == MC_SCHEDULED_MEMBERSHIP_OK);
    expected[0].due_time = INT64_MAX;
    expected[1].due_time = INT64_MIN;
    expected[2].due_time = INT64_MIN + 1;
    for (size_t i = 0; i < 3; ++i)
        CHECK(record_matches(ticks[i], &expected[i]));
    CHECK(mc_scheduled_ordered_first(ordered, &first_hold) == MC_SCHEDULED_ORDERED_OK &&
          first_hold == ticks[0]);
    CHECK(mc_scheduled_ordered_remove(ordered, first_hold, &changed) == MC_SCHEDULED_ORDERED_OK &&
          !changed);
    CHECK(size_is(ordered, 3) && bytes_is(ordered, three_bytes) && first_is(ordered, ticks[0]));
    CHECK(mc_scheduled_membership_contains(membership, ticks[0], &contains) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          contains);
    CHECK(mc_scheduled_ordered_add(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          changed);
    CHECK(size_is(ordered, 4)); /* A now has two structural nodes and two E holds. */
    CHECK(mc_scheduled_ordered_native_bytes(ordered, &four_bytes) == MC_SCHEDULED_ORDERED_OK &&
          four_bytes > three_bytes);
    CHECK(mc_scheduled_ordered_add(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          !changed);

    /* Change C alone: A(MAX,0) now compares left of C(MAX,2), so search
     * misses both historical A nodes. A global pointer scan would be wrong. */
    CHECK(mc_scheduled_tick_time_set(ticks[2], INT64_MAX) == MC_SCHEDULED_TICK_OK);
    expected[2].due_time = INT64_MAX;
    CHECK(mc_scheduled_tick_compare(ticks[0], ticks[2], &comparison) == MC_SCHEDULED_TICK_OK &&
          comparison == -1);
    CHECK(mc_scheduled_ordered_remove(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          !changed);
    changed = true;
    CHECK(mc_scheduled_ordered_add(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_LIMIT &&
          changed);
    CHECK(size_is(ordered, 4) && bytes_is(ordered, four_bytes) && first_is(ordered, ticks[0]));
    CHECK(record_matches(ticks[2], &expected[2]));
    CHECK(mc_scheduled_tick_time_set(ticks[2], INT64_MIN + 1) == MC_SCHEDULED_TICK_OK);
    expected[2].due_time = INT64_MIN + 1;
    CHECK(mc_scheduled_ordered_remove(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          changed);
    CHECK(size_is(ordered, 3) && bytes_is(ordered, three_bytes) && first_is(ordered, ticks[0]));
    CHECK(mc_scheduled_ordered_remove(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          !changed);
    CHECK(mc_scheduled_ordered_add(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          changed);
    CHECK(size_is(ordered, 4) && bytes_is(ordered, four_bytes));
    CHECK(mc_scheduled_membership_remove(membership, first_hold, &changed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          changed);
    CHECK(mc_scheduled_membership_size(membership, &membership_size) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          membership_size == 2);
    CHECK(size_is(ordered, 4)); /* Independent F removal does not repair/roll back E. */
    mc_scheduled_membership_destroy(membership);
    membership = NULL;
    mc_scheduled_ordered_destroy(ordered);
    ordered = NULL; /* Drops both A node holds. */
    mc_scheduled_tick_release(ticks[0]);
    ticks[0] = NULL;
    CHECK(record_matches(first_hold, &expected[0]));
    CHECK(record_matches(ticks[1], &expected[1]) && record_matches(ticks[2], &expected[2]));
    result = 0;
cleanup:
    mc_scheduled_membership_destroy(membership);
    mc_scheduled_ordered_destroy(ordered);
    mc_scheduled_tick_release(first_hold);
    for (size_t i = 0; i < 3; ++i)
        mc_scheduled_tick_release(ticks[i]);
    return result;
}

static int test_readonly_snapshot(void)
{
    int result = 1;
    mc_scheduled_ordered *ordered = NULL;
    mc_scheduled_tick *ticks[3] = {NULL};
    mc_scheduled_tick_fields fields[3] = {0};
    mc_scheduled_tick_sequence sequence = {0};
    const mc_scheduled_tick *entries[5] = {NULL};
    size_t written = 71, allocated = 0;
    bool changed = false;
    CHECK(mc_scheduled_ordered_create(4, SIZE_MAX, &ordered) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_ordered_snapshot(ordered, NULL, 0, &written) == MC_SCHEDULED_ORDERED_OK &&
          written == 0);
    written = 71;
    CHECK(mc_scheduled_ordered_snapshot(NULL, entries, 5, &written) ==
              MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          written == 71);
    CHECK(mc_scheduled_ordered_snapshot(ordered, NULL, 1, &written) ==
              MC_SCHEDULED_ORDERED_INVALID_ARGUMENT &&
          written == 71);
    CHECK(mc_scheduled_ordered_snapshot(ordered, entries, 5, NULL) ==
          MC_SCHEDULED_ORDERED_INVALID_ARGUMENT);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_scheduled_tick_create(&sequence, (int32_t)i, 64, -3, 8, &ticks[i]) ==
              MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_time_set(ticks[i], (int64_t)i * 10) == MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_ordered_add(ordered, ticks[i], &changed) == MC_SCHEDULED_ORDERED_OK &&
              changed);
    }
    /* Current keys now disagree with the historical tree. Snapshot must not
     * sort or repair it, and a new path may hold the SAME identity twice. */
    CHECK(mc_scheduled_tick_time_set(ticks[0], 30) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_ordered_snapshot(ordered, entries, 5, &written) == MC_SCHEDULED_ORDERED_OK &&
          written == 3 && entries[0] == ticks[0] && entries[1] == ticks[1] &&
          entries[2] == ticks[2]);
    CHECK(mc_scheduled_ordered_add(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          changed);
    for (size_t i = 0; i < 3; ++i)
        CHECK(mc_scheduled_tick_fields_get(ticks[i], &fields[i]) == MC_SCHEDULED_TICK_OK);
    for (size_t i = 0; i < 5; ++i)
        entries[i] = ticks[1];
    written = 71;
    CHECK(mc_scheduled_ordered_native_bytes(ordered, &allocated) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_ordered_snapshot(ordered, entries, 3, &written) ==
              MC_SCHEDULED_ORDERED_LIMIT &&
          written == 71);
    for (size_t i = 0; i < 5; ++i)
        CHECK(entries[i] == ticks[1]);
    CHECK(mc_scheduled_ordered_snapshot(ordered, NULL, 0, &written) == MC_SCHEDULED_ORDERED_LIMIT &&
          written == 71);
    CHECK(mc_scheduled_ordered_snapshot(ordered, entries, 5, &written) == MC_SCHEDULED_ORDERED_OK &&
          written == 4 && entries[0] == ticks[0] && entries[1] == ticks[1] &&
          entries[2] == ticks[2] && entries[3] == ticks[0] && entries[4] == ticks[1]);
    for (size_t i = 0; i < 3; ++i)
        CHECK(record_matches(ticks[i], &fields[i]));
    CHECK(size_is(ordered, 4) && bytes_is(ordered, allocated) && first_is(ordered, ticks[0]));
    written = 71;
    CHECK(mc_scheduled_ordered_snapshot(ordered, entries, 4, &written) == MC_SCHEDULED_ORDERED_OK &&
          written == 4 && entries[0] == ticks[0] && entries[1] == ticks[1] &&
          entries[2] == ticks[2] && entries[3] == ticks[0] && entries[4] == ticks[1]);
    CHECK(mc_scheduled_ordered_remove(ordered, ticks[0], &changed) == MC_SCHEDULED_ORDERED_OK &&
          changed);
    /* Restore the first node's original key before removing it through the
     * public search. The snapshot itself never repairs the mutated tree. */
    CHECK(mc_scheduled_tick_time_set(ticks[0], 0) == MC_SCHEDULED_TICK_OK);
    fields[0].due_time = 0;
    for (size_t i = 0; i < 3; ++i)
        CHECK(mc_scheduled_ordered_remove(ordered, ticks[i], &changed) == MC_SCHEDULED_ORDERED_OK &&
              changed);
    CHECK(size_is(ordered, 0));
    written = 71;
    CHECK(mc_scheduled_ordered_snapshot(ordered, NULL, 0, &written) == MC_SCHEDULED_ORDERED_OK &&
          written == 0);
    mc_scheduled_ordered_destroy(ordered);
    ordered = NULL;
    for (size_t i = 0; i < 3; ++i)
        CHECK(record_matches(ticks[i], &fields[i]));
    result = 0;
cleanup:
    mc_scheduled_ordered_destroy(ordered);
    for (size_t i = 0; i < 3; ++i)
        mc_scheduled_tick_release(ticks[i]);
    return result;
}

int main(void)
{
    if (test_invalid_empty_identity_and_limits() != 0 || test_successor_hold_transfer() != 0 ||
        test_deletion_permutations() != 0 || test_mirrored_deletion_history() != 0 ||
        test_wrapped_and_individually_mutated_paths() != 0 || test_readonly_snapshot() != 0) {
        return 1;
    }
    printf("scheduled ordered contract checks: %u\n", checks);
    return 0;
}
