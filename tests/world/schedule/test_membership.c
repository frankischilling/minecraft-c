#include "mc/world/schedule/membership.h"

#include <stdio.h>

/* Native membership, identity and budget contracts use only public opaque APIs.
 * They do not establish Java/E/world parity or inject
 * malloc/hold failures. Every CHECK exits through the owning function cleanup. */
static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool same_fields(const mc_scheduled_tick_fields *left, const mc_scheduled_tick_fields *right)
{
    return left->x == right->x && left->y == right->y && left->z == right->z &&
           left->block == right->block && left->due_time == right->due_time &&
           left->sequence == right->sequence;
}

static bool record_matches(const mc_scheduled_tick *tick, const mc_scheduled_tick_fields *expected)
{
    mc_scheduled_tick_fields actual = {0};
    return mc_scheduled_tick_fields_get(tick, &actual) == MC_SCHEDULED_TICK_OK &&
           same_fields(&actual, expected);
}

static bool contains_is(const mc_scheduled_membership *membership, const mc_scheduled_tick *tick,
                        bool expected)
{
    bool actual = !expected;
    return mc_scheduled_membership_contains(membership, tick, &actual) ==
               MC_SCHEDULED_MEMBERSHIP_OK &&
           actual == expected;
}

static bool size_is(const mc_scheduled_membership *membership, int32_t expected)
{
    int32_t actual = -1;
    return mc_scheduled_membership_size(membership, &actual) == MC_SCHEDULED_MEMBERSHIP_OK &&
           actual == expected;
}

static bool bytes_is(const mc_scheduled_membership *membership, size_t expected)
{
    size_t actual = SIZE_MAX;
    return mc_scheduled_membership_native_bytes(membership, &actual) ==
               MC_SCHEDULED_MEMBERSHIP_OK &&
           actual == expected;
}

static int test_collisions_identity_shift_and_removal(void)
{
    int result = 1;
    mc_scheduled_membership *membership = NULL;
    mc_scheduled_tick *original = NULL;
    mc_scheduled_tick *x_collision = NULL;
    mc_scheduled_tick *y_block_collision = NULL;
    mc_scheduled_tick *z_collision = NULL;
    mc_scheduled_tick *duplicate = NULL;
    mc_scheduled_tick *equal_query = NULL;
    mc_scheduled_tick *ordered_hold = NULL; /* Separate retained alias, not E implementation. */
    mc_scheduled_tick_sequence sequence = {0};
    mc_scheduled_tick_fields original_fields = {0}, duplicate_fields = {0}, query_fields = {0};
    mc_scheduled_tick_fields x_fields = {0}, y_block_fields = {0}, z_fields = {0};
    int32_t hash = 0;
    size_t full_bytes = 0, removed_bytes = 0;
    bool changed = false;

    CHECK(mc_scheduled_tick_create(&sequence, 0, 0, 0, 17, &original) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_create(&sequence, 128, 0, 0, 17, &x_collision) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_create(&sequence, 0, 1, 0, -239, &y_block_collision) ==
          MC_SCHEDULED_TICK_OK);
    /* z*32768 wraps by 2^32 when z increases by 131072, without changing y. */
    CHECK(mc_scheduled_tick_create(&sequence, 0, 0, 131072, 17, &z_collision) ==
          MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_hash(original, &hash) == MC_SCHEDULED_TICK_OK && hash == 17);
    CHECK(mc_scheduled_tick_hash(x_collision, &hash) == MC_SCHEDULED_TICK_OK && hash == 17);
    CHECK(mc_scheduled_tick_hash(y_block_collision, &hash) == MC_SCHEDULED_TICK_OK && hash == 17);
    CHECK(mc_scheduled_tick_hash(z_collision, &hash) == MC_SCHEDULED_TICK_OK && hash == 17);
    CHECK(mc_scheduled_tick_time_set(original, INT64_MAX) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_time_set(x_collision, -8) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_time_set(y_block_collision, 0) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_time_set(z_collision, -1) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(original, &original_fields) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(x_collision, &x_fields) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(y_block_collision, &y_block_fields) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(z_collision, &z_fields) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_retain(original) == MC_SCHEDULED_TICK_OK);
    ordered_hold = original;
    CHECK(mc_scheduled_membership_create(4, SIZE_MAX, &membership) == MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(mc_scheduled_membership_add(membership, original, &changed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          changed);
    changed = false;
    CHECK(mc_scheduled_membership_add(membership, x_collision, &changed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          changed);
    changed = false;
    CHECK(mc_scheduled_membership_add(membership, y_block_collision, &changed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          changed);
    changed = false;
    CHECK(mc_scheduled_membership_add(membership, z_collision, &changed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          changed);
    CHECK(size_is(membership, 4));
    CHECK(contains_is(membership, original, true));
    CHECK(contains_is(membership, x_collision, true));
    CHECK(contains_is(membership, y_block_collision, true));
    CHECK(contains_is(membership, z_collision, true));
    CHECK(mc_scheduled_membership_native_bytes(membership, &full_bytes) ==
          MC_SCHEDULED_MEMBERSHIP_OK);

    CHECK(mc_scheduled_tick_create(&sequence, 0, 0, 0, 17, &duplicate) == MC_SCHEDULED_TICK_OK);
    CHECK(duplicate != original);
    CHECK(mc_scheduled_tick_time_set(duplicate, 123) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(duplicate, &duplicate_fields) == MC_SCHEDULED_TICK_OK);
    CHECK(duplicate_fields.sequence != original_fields.sequence);
    changed = true;
    CHECK(mc_scheduled_membership_add(membership, duplicate, &changed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          !changed);
    CHECK(size_is(membership, 4) && bytes_is(membership, full_bytes));
    CHECK(record_matches(original, &original_fields));
    CHECK(record_matches(duplicate, &duplicate_fields));

    CHECK(mc_scheduled_membership_shift_time(membership, 1) == MC_SCHEDULED_MEMBERSHIP_OK);
    original_fields.due_time = INT64_MIN;
    x_fields.due_time = -7;
    y_block_fields.due_time = 1;
    z_fields.due_time = 0;
    CHECK(ordered_hold == original && record_matches(ordered_hold, &original_fields));
    CHECK(record_matches(x_collision, &x_fields));
    CHECK(record_matches(y_block_collision, &y_block_fields));
    CHECK(record_matches(z_collision, &z_fields));
    CHECK(record_matches(duplicate, &duplicate_fields));
    CHECK(size_is(membership, 4) && bytes_is(membership, full_bytes));
    CHECK(contains_is(membership, original, true));
    CHECK(contains_is(membership, x_collision, true));
    CHECK(contains_is(membership, y_block_collision, true));
    CHECK(contains_is(membership, z_collision, true));
    CHECK(contains_is(membership, duplicate, true));

    CHECK(mc_scheduled_tick_create(&sequence, 0, 0, 0, 17, &equal_query) == MC_SCHEDULED_TICK_OK);
    CHECK(equal_query != original && equal_query != duplicate);
    CHECK(mc_scheduled_tick_time_set(equal_query, 999) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(equal_query, &query_fields) == MC_SCHEDULED_TICK_OK);
    changed = false;
    CHECK(mc_scheduled_membership_remove(membership, equal_query, &changed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          changed);
    CHECK(size_is(membership, 3));
    CHECK(contains_is(membership, original, false));
    CHECK(contains_is(membership, equal_query, false));
    CHECK(contains_is(membership, x_collision, true));
    CHECK(contains_is(membership, y_block_collision, true));
    CHECK(contains_is(membership, z_collision, true));
    CHECK(record_matches(equal_query, &query_fields));
    CHECK(record_matches(original, &original_fields));
    CHECK(mc_scheduled_membership_native_bytes(membership, &removed_bytes) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          removed_bytes < full_bytes);
    changed = true;
    CHECK(mc_scheduled_membership_remove(membership, equal_query, &changed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          !changed);
    CHECK(bytes_is(membership, removed_bytes));

    /* F removal did not consume either independent original hold. Drop the
     * first explicitly; the separate retained alias must still read unchanged. */
    mc_scheduled_tick_release(original);
    original = NULL;
    CHECK(record_matches(ordered_hold, &original_fields));
    CHECK(record_matches(equal_query, &query_fields));
    mc_scheduled_membership_destroy(membership);
    membership = NULL;
    CHECK(record_matches(ordered_hold, &original_fields));
    CHECK(record_matches(x_collision, &x_fields));
    CHECK(record_matches(y_block_collision, &y_block_fields));
    CHECK(record_matches(z_collision, &z_fields));
    CHECK(record_matches(duplicate, &duplicate_fields));
    CHECK(record_matches(equal_query, &query_fields));
    result = 0;

cleanup:
    mc_scheduled_membership_destroy(membership);
    mc_scheduled_tick_release(original);
    mc_scheduled_tick_release(x_collision);
    mc_scheduled_tick_release(y_block_collision);
    mc_scheduled_tick_release(z_collision);
    mc_scheduled_tick_release(duplicate);
    mc_scheduled_tick_release(equal_query);
    mc_scheduled_tick_release(ordered_hold);
    return result;
}

static int test_growth_and_public_budget_boundaries(void)
{
    int result = 1;
    mc_scheduled_membership *reference = NULL;
    mc_scheduled_membership *exact = NULL;
    mc_scheduled_membership *tight = NULL;
    mc_scheduled_membership *roomy = NULL;
    mc_scheduled_tick *ticks[13] = {NULL};
    mc_scheduled_tick *duplicate = NULL;
    mc_scheduled_tick_sequence sequence = {0};
    mc_scheduled_tick_fields expected[13] = {0}, duplicate_fields = {0};
    size_t twelve_bytes = 0, thirteen_bytes = 0, larger_budget = 0;
    bool changed = false;

    for (size_t i = 0; i < 13; ++i) {
        /* Fixture i is in 0..12: both conversions/additions are bounded. */
        CHECK(mc_scheduled_tick_create(&sequence, (int32_t)i, -64, 17, 17, &ticks[i]) ==
              MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_time_set(ticks[i], INT64_C(1000) + (int64_t)i) ==
              MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_fields_get(ticks[i], &expected[i]) == MC_SCHEDULED_TICK_OK);
    }
    CHECK(mc_scheduled_membership_create(13, SIZE_MAX, &reference) == MC_SCHEDULED_MEMBERSHIP_OK);
    for (size_t i = 0; i < 13; ++i) {
        changed = false;
        CHECK(mc_scheduled_membership_add(reference, ticks[i], &changed) ==
                  MC_SCHEDULED_MEMBERSHIP_OK &&
              changed);
        if (i == 11) {
            CHECK(mc_scheduled_membership_native_bytes(reference, &twelve_bytes) ==
                  MC_SCHEDULED_MEMBERSHIP_OK);
        }
    }
    CHECK(mc_scheduled_membership_native_bytes(reference, &thirteen_bytes) ==
          MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(twelve_bytes > 0 && thirteen_bytes > twelve_bytes && size_is(reference, 13));

    /* Learn budget values exclusively from public live-charge observations.
     * No opaque sizeof, bucket count/layout or allocator internals are read. */
    CHECK(mc_scheduled_membership_create(12, twelve_bytes, &exact) == MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(mc_scheduled_membership_create(13, thirteen_bytes, &tight) == MC_SCHEDULED_MEMBERSHIP_OK);
    for (size_t i = 0; i < 12; ++i) {
        changed = false;
        CHECK(mc_scheduled_membership_add(exact, ticks[i], &changed) ==
                  MC_SCHEDULED_MEMBERSHIP_OK &&
              changed);
        changed = false;
        CHECK(mc_scheduled_membership_add(tight, ticks[i], &changed) ==
                  MC_SCHEDULED_MEMBERSHIP_OK &&
              changed);
    }
    CHECK(size_is(exact, 12) && bytes_is(exact, twelve_bytes));
    CHECK(size_is(tight, 12) && bytes_is(tight, twelve_bytes));
    CHECK(mc_scheduled_tick_create(&sequence, 0, -64, 17, 17, &duplicate) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_time_set(duplicate, -77) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(duplicate, &duplicate_fields) == MC_SCHEDULED_TICK_OK);
    CHECK(duplicate_fields.sequence != expected[0].sequence);
    changed = true;
    CHECK(mc_scheduled_membership_add(exact, duplicate, &changed) == MC_SCHEDULED_MEMBERSHIP_OK &&
          !changed);
    CHECK(size_is(exact, 12) && bytes_is(exact, twelve_bytes));
    CHECK(record_matches(duplicate, &duplicate_fields) && record_matches(ticks[0], &expected[0]));

    changed = true;
    CHECK(mc_scheduled_membership_add(exact, ticks[12], &changed) == MC_SCHEDULED_MEMBERSHIP_LIMIT);
    CHECK(changed && size_is(exact, 12) && bytes_is(exact, twelve_bytes));
    /* Final 13-entry bytes are insufficient for old+new bucket overlap at 12->13.
     * A failed staged growth must preserve current capacity/charge/membership. */
    changed = true;
    CHECK(mc_scheduled_membership_add(tight, ticks[12], &changed) == MC_SCHEDULED_MEMBERSHIP_LIMIT);
    CHECK(changed && size_is(tight, 12) && bytes_is(tight, twelve_bytes));
    CHECK(contains_is(tight, ticks[12], false) && contains_is(reference, ticks[12], true));
    for (size_t i = 0; i < 12; ++i) {
        CHECK(contains_is(exact, ticks[i], true));
        CHECK(contains_is(tight, ticks[i], true));
        CHECK(record_matches(ticks[i], &expected[i]));
    }
    CHECK(record_matches(ticks[12], &expected[12]));

    CHECK(twelve_bytes <= SIZE_MAX - thirteen_bytes);
    larger_budget = twelve_bytes + thirteen_bytes;
    CHECK(mc_scheduled_membership_create(13, larger_budget, &roomy) == MC_SCHEDULED_MEMBERSHIP_OK);
    for (size_t i = 0; i < 13; ++i) {
        changed = false;
        CHECK(mc_scheduled_membership_add(roomy, ticks[i], &changed) ==
                  MC_SCHEDULED_MEMBERSHIP_OK &&
              changed);
        CHECK(contains_is(roomy, ticks[i], true));
    }
    CHECK(size_is(roomy, 13) && bytes_is(roomy, thirteen_bytes));
    for (size_t i = 0; i < 13; ++i) {
        CHECK(contains_is(reference, ticks[i], true));
        CHECK(contains_is(roomy, ticks[i], true));
        CHECK(record_matches(ticks[i], &expected[i]));
    }
    mc_scheduled_membership_destroy(reference);
    reference = NULL;
    mc_scheduled_membership_destroy(exact);
    exact = NULL;
    mc_scheduled_membership_destroy(tight);
    tight = NULL;
    mc_scheduled_membership_destroy(roomy);
    roomy = NULL;
    for (size_t i = 0; i < 13; ++i) {
        CHECK(record_matches(ticks[i], &expected[i]));
    }
    CHECK(record_matches(duplicate, &duplicate_fields));
    result = 0;

cleanup:
    mc_scheduled_membership_destroy(reference);
    mc_scheduled_membership_destroy(exact);
    mc_scheduled_membership_destroy(tight);
    mc_scheduled_membership_destroy(roomy);
    for (size_t i = 0; i < 13; ++i) {
        mc_scheduled_tick_release(ticks[i]);
    }
    mc_scheduled_tick_release(duplicate);
    return result;
}

static int test_zero_entry_and_invalid_outputs(void)
{
    int result = 1;
    mc_scheduled_membership *zero = NULL;
    mc_scheduled_membership *failed = NULL;
    mc_scheduled_tick *tick = NULL;
    mc_scheduled_tick_sequence sequence = {0};
    mc_scheduled_tick_fields expected = {0};
    size_t owner_bytes = 0, byte_output = SIZE_MAX;
    int32_t size_output = -73;
    bool flag = true;

    CHECK(mc_scheduled_membership_create(0, SIZE_MAX, &zero) == MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(size_is(zero, 0));
    CHECK(mc_scheduled_membership_native_bytes(zero, &owner_bytes) == MC_SCHEDULED_MEMBERSHIP_OK &&
          owner_bytes > 0);
    CHECK(mc_scheduled_membership_create(1, owner_bytes - 1, &failed) ==
          MC_SCHEDULED_MEMBERSHIP_LIMIT);
    CHECK(failed == NULL);
    CHECK(mc_scheduled_membership_create((size_t)INT32_MAX + 1u, SIZE_MAX, &failed) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(failed == NULL);
    CHECK(mc_scheduled_membership_create(0, SIZE_MAX, NULL) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(mc_scheduled_tick_create(&sequence, INT32_MIN, INT32_MAX, -1, INT32_MIN, &tick) ==
          MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_time_set(tick, 71) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_tick_fields_get(tick, &expected) == MC_SCHEDULED_TICK_OK);
    CHECK(contains_is(zero, tick, false));
    CHECK(mc_scheduled_membership_add(zero, tick, &flag) == MC_SCHEDULED_MEMBERSHIP_LIMIT);
    CHECK(flag && size_is(zero, 0) && bytes_is(zero, owner_bytes));

    CHECK(mc_scheduled_membership_contains(NULL, tick, &flag) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(flag);
    CHECK(mc_scheduled_membership_contains(zero, NULL, &flag) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(flag);
    CHECK(mc_scheduled_membership_contains(zero, tick, NULL) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(mc_scheduled_membership_add(NULL, tick, &flag) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(flag);
    CHECK(mc_scheduled_membership_add(zero, NULL, &flag) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(flag);
    CHECK(mc_scheduled_membership_add(zero, tick, NULL) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(mc_scheduled_membership_remove(NULL, tick, &flag) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(flag);
    CHECK(mc_scheduled_membership_remove(zero, NULL, &flag) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(flag);
    CHECK(mc_scheduled_membership_remove(zero, tick, NULL) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(mc_scheduled_membership_remove(zero, tick, &flag) == MC_SCHEDULED_MEMBERSHIP_OK && !flag);
    CHECK(mc_scheduled_membership_size(NULL, &size_output) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(size_output == -73);
    CHECK(mc_scheduled_membership_size(zero, NULL) == MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(mc_scheduled_membership_native_bytes(NULL, &byte_output) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(byte_output == SIZE_MAX);
    CHECK(mc_scheduled_membership_native_bytes(zero, NULL) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(mc_scheduled_membership_shift_time(NULL, 1) == MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    CHECK(mc_scheduled_membership_shift_time(zero, INT64_MIN) == MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(size_is(zero, 0) && bytes_is(zero, owner_bytes));
    CHECK(record_matches(tick, &expected));
    mc_scheduled_membership_destroy(zero);
    zero = NULL;
    CHECK(record_matches(tick, &expected));
    result = 0;

cleanup:
    mc_scheduled_membership_destroy(zero);
    mc_scheduled_membership_destroy(failed);
    mc_scheduled_tick_release(tick);
    return result;
}

static int test_readonly_snapshot(void)
{
    int result = 1;
    mc_scheduled_membership *membership = NULL;
    mc_scheduled_tick *ticks[3] = {NULL};
    mc_scheduled_tick_fields fields[3] = {0};
    mc_scheduled_tick_sequence sequence = {0};
    const mc_scheduled_tick *entries[4] = {NULL};
    size_t written = 71, allocated = 0;
    bool changed = false;
    CHECK(mc_scheduled_membership_create(3, SIZE_MAX, &membership) == MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(mc_scheduled_membership_snapshot(membership, NULL, 0, &written) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          written == 0);
    written = 71;
    CHECK(mc_scheduled_membership_snapshot(NULL, entries, 4, &written) ==
              MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT &&
          written == 71);
    CHECK(mc_scheduled_membership_snapshot(membership, NULL, 1, &written) ==
              MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT &&
          written == 71);
    CHECK(mc_scheduled_membership_snapshot(membership, entries, 4, NULL) ==
          MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_scheduled_tick_create(&sequence, (int32_t)i, 64, -3, 8, &ticks[i]) ==
              MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_time_set(ticks[i], 17 - (int64_t)i) == MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_tick_fields_get(ticks[i], &fields[i]) == MC_SCHEDULED_TICK_OK);
        CHECK(mc_scheduled_membership_add(membership, ticks[i], &changed) ==
                  MC_SCHEDULED_MEMBERSHIP_OK &&
              changed);
    }
    for (size_t i = 0; i < 4; ++i)
        entries[i] = ticks[0];
    CHECK(mc_scheduled_membership_native_bytes(membership, &allocated) ==
          MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(mc_scheduled_membership_snapshot(membership, entries, 2, &written) ==
              MC_SCHEDULED_MEMBERSHIP_LIMIT &&
          written == 71);
    for (size_t i = 0; i < 4; ++i)
        CHECK(entries[i] == ticks[0]);
    CHECK(mc_scheduled_membership_snapshot(membership, NULL, 0, &written) ==
              MC_SCHEDULED_MEMBERSHIP_LIMIT &&
          written == 71);
    CHECK(mc_scheduled_membership_snapshot(membership, entries, 4, &written) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          written == 3 && entries[3] == ticks[0]);
    bool seen[3] = {false};
    for (size_t i = 0; i < written; ++i) {
        size_t match = 3;
        for (size_t j = 0; j < 3; ++j)
            if (entries[i] == ticks[j])
                match = j;
        CHECK(match < 3 && !seen[match]);
        seen[match] = true;
        CHECK(record_matches(entries[i], &fields[match]));
    }
    CHECK(seen[0] && seen[1] && seen[2] && size_is(membership, 3) &&
          bytes_is(membership, allocated));
    written = 71;
    CHECK(mc_scheduled_membership_snapshot(membership, entries, 3, &written) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          written == 3 && entries[3] == ticks[0]);
    for (size_t i = 0; i < 3; ++i)
        CHECK(mc_scheduled_membership_remove(membership, ticks[i], &changed) ==
                  MC_SCHEDULED_MEMBERSHIP_OK &&
              changed);
    CHECK(size_is(membership, 0));
    written = 71;
    CHECK(mc_scheduled_membership_snapshot(membership, NULL, 0, &written) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          written == 0);
    mc_scheduled_membership_destroy(membership);
    membership = NULL;
    for (size_t i = 0; i < 3; ++i)
        CHECK(record_matches(ticks[i], &fields[i]));
    result = 0;
cleanup:
    mc_scheduled_membership_destroy(membership);
    for (size_t i = 0; i < 3; ++i)
        mc_scheduled_tick_release(ticks[i]);
    return result;
}

int main(void)
{
    if (test_collisions_identity_shift_and_removal() != 0 ||
        test_growth_and_public_budget_boundaries() != 0 ||
        test_zero_entry_and_invalid_outputs() != 0 || test_readonly_snapshot() != 0) {
        return 1;
    }
    printf("scheduled membership contract checks: %u\n", checks);
    return 0;
}
