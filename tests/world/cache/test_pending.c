#include "mc/world/cache/acquire.h"

#include <stdio.h>
#include <string.h>

_Static_assert(MC_CHUNK_ACQUIRE_OK == 0 && MC_CHUNK_ACQUIRE_INVALID_ARGUMENT == 1 &&
                   MC_CHUNK_ACQUIRE_LIMIT == 2 && MC_CHUNK_ACQUIRE_PARTIAL_LIMIT == 3 &&
                   MC_CHUNK_ACQUIRE_CALLBACK_FAILED == 4 && MC_CHUNK_ACQUIRE_STATE_ERROR == 5 &&
                   MC_CHUNK_ACQUIRE_EMPTY == 6,
               "Pending EMPTY must preserve existing acquisition status values");

static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum { KEYS = 18, MEMBERS = 16, ORDER = 32 };

typedef struct pending_snapshot {
    mc_chunk_pending descriptor;
    int32_t keys[KEYS], order[ORDER];
    size_t key_count, order_count;
} pending_snapshot;

static bool same_descriptor(const mc_chunk_pending *left, const mc_chunk_pending *right) {
    return left->keys == right->keys && left->capacity == right->capacity &&
           left->count == right->count && left->history == right->history;
}

static void fill_keys(int32_t *keys) {
    for (size_t i = 0; i < KEYS; ++i)
        keys[i] = 1000 + (int32_t)i;
}

static int take_snapshot(const mc_chunk_pending *pending, const int32_t *keys, size_t key_count,
                         pending_snapshot *snapshot) {
    CHECK(key_count <= KEYS);
    snapshot->descriptor = *pending;
    snapshot->key_count = key_count;
    if (key_count != 0)
        memcpy(snapshot->keys, keys, key_count * sizeof(*keys));
    for (size_t i = 0; i < ORDER; ++i)
        snapshot->order[i] = 76543;
    snapshot->order_count = 0;
    if (pending->history != NULL)
        CHECK(mc_java_int_set_copy_order(pending->history, snapshot->order, ORDER,
                                         &snapshot->order_count) == MC_JAVA_INT_SET_OK);
    return 0;
}

static int check_snapshot(const mc_chunk_pending *pending, const int32_t *keys,
                          const pending_snapshot *snapshot) {
    CHECK(same_descriptor(pending, &snapshot->descriptor));
    if (snapshot->key_count != 0)
        CHECK(memcmp(keys, snapshot->keys, snapshot->key_count * sizeof(*keys)) == 0);
    if (pending->history != NULL) {
        int32_t order[ORDER];
        for (size_t i = 0; i < ORDER; ++i)
            order[i] = 76543;
        size_t count = SIZE_MAX;
        CHECK(mc_java_int_set_copy_order(pending->history, order, ORDER, &count) ==
              MC_JAVA_INT_SET_OK);
        CHECK(count == snapshot->order_count && memcmp(order, snapshot->order, sizeof(order)) == 0);
    }
    return 0;
}

static int check_order(const mc_chunk_pending *pending, const int32_t *expected, size_t count) {
    int32_t actual[ORDER], complete[ORDER];
    CHECK(count <= ORDER && pending->history != NULL);
    for (size_t i = 0; i < ORDER; ++i)
        actual[i] = complete[i] = 87654;
    if (count != 0)
        memcpy(complete, expected, count * sizeof(*expected));
    size_t written = SIZE_MAX;
    CHECK(mc_java_int_set_copy_order(pending->history, actual, ORDER, &written) ==
          MC_JAVA_INT_SET_OK);
    CHECK(written == count && pending->count == count &&
          memcmp(actual, complete, sizeof(actual)) == 0);
    int32_t first = 98765;
    CHECK(mc_chunk_pending_first(pending, &first) ==
          (count == 0 ? MC_CHUNK_ACQUIRE_EMPTY : MC_CHUNK_ACQUIRE_OK));
    CHECK(first == (count == 0 ? 98765 : expected[0]));
    CHECK(mc_chunk_pending_validate(pending));
    return 0;
}

static int check_same(const mc_chunk_pending *left, const mc_chunk_pending *right) {
    CHECK(mc_chunk_pending_validate(left) && mc_chunk_pending_validate(right));
    CHECK(left->count == right->count);
    CHECK(memcmp(left->keys, right->keys, KEYS * sizeof(*left->keys)) == 0);
    int32_t a[ORDER], b[ORDER];
    size_t an = SIZE_MAX, bn = SIZE_MAX;
    CHECK(mc_java_int_set_copy_order(left->history, a, ORDER, &an) == MC_JAVA_INT_SET_OK);
    CHECK(mc_java_int_set_copy_order(right->history, b, ORDER, &bn) == MC_JAVA_INT_SET_OK);
    CHECK(an == bn && memcmp(a, b, an * sizeof(*a)) == 0);
    return 0;
}

static int test_arguments(void) {
    mc_java_int_set *history = NULL;
    CHECK(mc_java_int_set_create(NULL, &history) == MC_JAVA_INT_SET_OK);
    int32_t keys[KEYS];
    fill_keys(keys);
    mc_chunk_pending pending;
    CHECK(mc_chunk_pending_init(&pending, keys, MEMBERS));
    pending_snapshot snapshot;
    CHECK(take_snapshot(&pending, keys, KEYS, &snapshot) == 0);
    CHECK(!mc_chunk_pending_init(NULL, keys, MEMBERS));
    CHECK(!mc_chunk_pending_init(&pending, NULL, 1));
    CHECK(!mc_chunk_pending_init(&pending, keys, SIZE_MAX));
    CHECK(!mc_chunk_pending_init_ordered(NULL, keys, MEMBERS, history));
    CHECK(!mc_chunk_pending_init_ordered(&pending, keys, MEMBERS, NULL));
    CHECK(!mc_chunk_pending_init_ordered(&pending, NULL, 1, history));
    CHECK(!mc_chunk_pending_init_ordered(&pending, keys, SIZE_MAX, history));
    CHECK(check_snapshot(&pending, keys, &snapshot) == 0);
    CHECK(!mc_chunk_pending_validate(NULL) && !mc_chunk_pending_contains(NULL, 1));
    bool changed = true;
    int32_t first = 123;
    CHECK(mc_chunk_pending_add(NULL, 1, &changed) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT && changed);
    CHECK(mc_chunk_pending_remove(NULL, 1, &changed) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT &&
          changed);
    CHECK(mc_chunk_pending_add(&pending, 1, NULL) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    CHECK(mc_chunk_pending_remove(&pending, 1, NULL) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    CHECK(mc_chunk_pending_first(NULL, &first) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT &&
          first == 123);
    CHECK(mc_chunk_pending_first(&pending, NULL) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    CHECK(mc_chunk_pending_clear(NULL) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    CHECK(check_snapshot(&pending, keys, &snapshot) == 0);
    CHECK(mc_java_int_set_add(history, 16, &changed) == MC_JAVA_INT_SET_OK && changed);
    CHECK(!mc_chunk_pending_init_ordered(&pending, keys, MEMBERS, history));
    CHECK(check_snapshot(&pending, keys, &snapshot) == 0 && mc_java_int_set_contains(history, 16));
    mc_java_int_set_clear(history);
    CHECK(mc_chunk_pending_init_ordered(&pending, NULL, 0, history));
    CHECK(mc_chunk_pending_validate(&pending));
    CHECK(mc_chunk_pending_first(&pending, &first) == MC_CHUNK_ACQUIRE_EMPTY && first == 123);
    changed = false;
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_LIMIT && !changed);
    CHECK(pending.count == 0 && mc_java_int_set_size(history) == 0);
    CHECK(mc_chunk_pending_clear(&pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_pending_init(&pending, NULL, 0));
    CHECK(pending.history == NULL && mc_chunk_pending_validate(&pending));
    CHECK(mc_chunk_pending_first(&pending, &first) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT &&
          first == 123);
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_LIMIT && !changed);
    CHECK(mc_java_int_set_size(history) == 0); /* Reinitializing never frees the borrowed owner. */
    CHECK(strcmp(mc_chunk_acquire_status_string(MC_CHUNK_ACQUIRE_EMPTY), "empty pending set") == 0);
    mc_java_int_set_free(history);
    return 0;
}

static int test_legacy(void) {
    int32_t keys[KEYS];
    fill_keys(keys);
    int32_t original[KEYS];
    memcpy(original, keys, sizeof(keys));
    mc_chunk_pending pending;
    CHECK(mc_chunk_pending_init(&pending, keys, MEMBERS));
    CHECK(memcmp(original, keys, sizeof(keys)) == 0);
    CHECK(pending.history == NULL && pending.count == 0 && pending.capacity == MEMBERS);
    bool changed = false;
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(mc_chunk_pending_add(&pending, 32, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(mc_chunk_pending_add(&pending, -1, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    pending_snapshot snapshot;
    CHECK(take_snapshot(&pending, keys, KEYS, &snapshot) == 0);
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && !changed);
    CHECK(mc_chunk_pending_remove(&pending, 99, &changed) == MC_CHUNK_ACQUIRE_OK && !changed);
    CHECK(check_snapshot(&pending, keys, &snapshot) == 0);
    int32_t first = 123;
    CHECK(mc_chunk_pending_first(&pending, &first) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT &&
          first == 123);
    CHECK(mc_chunk_pending_remove(&pending, 32, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(pending.count == 2 && keys[0] == 16 && keys[1] == -1 && keys[2] == -1);
    CHECK(mc_chunk_pending_contains(&pending, -1) && !mc_chunk_pending_contains(&pending, 32));
    CHECK(mc_chunk_pending_validate(&pending));
    memcpy(snapshot.keys, keys, sizeof(keys));
    CHECK(mc_chunk_pending_clear(&pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(pending.keys == keys && pending.capacity == MEMBERS && pending.count == 0 &&
          pending.history == NULL && memcmp(snapshot.keys, keys, sizeof(keys)) == 0);
    CHECK(mc_chunk_pending_first(&pending, &first) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT &&
          first == 123);
    return 0;
}

static int test_ordered(void) {
    mc_java_int_set *history = NULL;
    CHECK(mc_java_int_set_create(NULL, &history) == MC_JAVA_INT_SET_OK);
    int32_t keys[KEYS];
    fill_keys(keys);
    int32_t original[KEYS];
    memcpy(original, keys, sizeof(keys));
    mc_chunk_pending pending;
    CHECK(mc_chunk_pending_init_ordered(&pending, keys, MEMBERS, history));
    CHECK(memcmp(original, keys, sizeof(keys)) == 0);
    CHECK(check_order(&pending, NULL, 0) == 0);
    bool changed = false;
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(mc_chunk_pending_add(&pending, 32, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    pending_snapshot snapshot;
    CHECK(take_snapshot(&pending, keys, KEYS, &snapshot) == 0);
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && !changed);
    CHECK(mc_chunk_pending_remove(&pending, 99, &changed) == MC_CHUNK_ACQUIRE_OK && !changed);
    CHECK(check_snapshot(&pending, keys, &snapshot) == 0);
    CHECK(mc_chunk_pending_remove(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    const int32_t expected[] = {32, 16};
    CHECK(pending.count == 2 && keys[0] == 32 && keys[1] == 16);
    CHECK(check_order(&pending, expected, 2) == 0);
    CHECK(mc_chunk_pending_remove(&pending, 32, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(mc_chunk_pending_remove(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(check_order(&pending, NULL, 0) == 0);
    CHECK(pending.history == history);
    mc_java_int_set_free(history);
    return 0;
}

static int test_survivors_and_clear(void) {
    mc_java_int_set *fresh = NULL, *grown = NULL;
    CHECK(mc_java_int_set_create(NULL, &fresh) == MC_JAVA_INT_SET_OK);
    CHECK(mc_java_int_set_create(NULL, &grown) == MC_JAVA_INT_SET_OK);
    int32_t a[KEYS], b[KEYS];
    fill_keys(a);
    fill_keys(b);
    mc_chunk_pending left, right;
    CHECK(mc_chunk_pending_init_ordered(&left, a, MEMBERS, fresh));
    CHECK(mc_chunk_pending_init_ordered(&right, b, MEMBERS, grown));
    bool changed = false;
    for (int32_t key = 16; key <= 32; key += 16) {
        CHECK(mc_chunk_pending_add(&left, key, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
        CHECK(mc_chunk_pending_add(&right, key, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    }
    for (int32_t key = 65; key <= 75; ++key)
        CHECK(mc_chunk_pending_add(&right, key, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    for (int32_t key = 65; key <= 75; ++key)
        CHECK(mc_chunk_pending_remove(&right, key, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(left.count == right.count && memcmp(a, b, left.count * sizeof(*a)) == 0);
    /* Independent constants from the captured JDK survivor-history cases in
     * tests/reference/java/int_set_fixtures.json, not sorted C membership. */
    const int32_t fresh_order[] = {16, 32}, grown_order[] = {32, 16};
    CHECK(check_order(&left, fresh_order, 2) == 0);
    CHECK(check_order(&right, grown_order, 2) == 0);

    /* Clear/reinitialization below is a constructed native capacity contract.
     * It is not an additional independently observed JDK clear oracle. */
    pending_snapshot snapshot;
    CHECK(take_snapshot(&right, b, KEYS, &snapshot) == 0);
    CHECK(mc_chunk_pending_clear(&right) == MC_CHUNK_ACQUIRE_OK);
    CHECK(right.history == grown && right.keys == b && right.capacity == MEMBERS &&
          right.count == 0);
    CHECK(memcmp(b, snapshot.keys, sizeof(b)) == 0 && mc_java_int_set_size(grown) == 0);
    CHECK(check_order(&right, NULL, 0) == 0);
    CHECK(mc_chunk_pending_clear(&right) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_pending_init_ordered(&right, b, MEMBERS, grown));
    CHECK(mc_chunk_pending_add(&right, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(mc_chunk_pending_add(&right, 32, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(check_order(&right, grown_order, 2) == 0);
    CHECK(mc_java_int_set_size(fresh) == 2); /* Clear did not affect another owner. */
    mc_java_int_set_free(fresh);
    mc_java_int_set_free(grown);
    return 0;
}

static int test_capacity_and_history_entry_limit(void) {
    for (unsigned mode = 0; mode < 2; ++mode) {
        mc_java_int_set_limits limits = mc_java_int_set_default_limits();
        if (mode == 1)
            limits.max_entries = 2;
        mc_java_int_set *history = NULL;
        CHECK(mc_java_int_set_create(&limits, &history) == MC_JAVA_INT_SET_OK);
        int32_t keys[KEYS];
        fill_keys(keys);
        mc_chunk_pending pending;
        CHECK(mc_chunk_pending_init_ordered(&pending, keys, mode == 0 ? 2u : MEMBERS, history));
        bool changed = false;
        CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
        CHECK(mc_chunk_pending_add(&pending, 32, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
        pending_snapshot snapshot;
        CHECK(take_snapshot(&pending, keys, KEYS, &snapshot) == 0);
        changed = false;
        CHECK(mc_chunk_pending_add(&pending, 65, &changed) == MC_CHUNK_ACQUIRE_LIMIT && !changed);
        CHECK(check_snapshot(&pending, keys, &snapshot) == 0);
        CHECK(mc_chunk_pending_add(&pending, 32, &changed) == MC_CHUNK_ACQUIRE_OK && !changed);
        CHECK(check_snapshot(&pending, keys, &snapshot) == 0);
        CHECK(mc_chunk_pending_remove(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
        CHECK(mc_chunk_pending_add(&pending, 65, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
        const int32_t expected[] = {32, 65};
        CHECK(check_order(&pending, expected, 2) == 0 && keys[0] == 32 && keys[1] == 65);
        mc_java_int_set_free(history);
    }
    return 0;
}

static int test_history_byte_limits(void) {
    size_t create_limits = 0, add_limits = 0, successes = 0, recovered = 0;
    /* Constructed paired controls omit failed mutations. Later removal/readd
     * compares hidden-history preservation, not new reference expectations. */
    for (unsigned mode = 0; mode < 2; ++mode) {
        for (size_t budget = 0; budget <= 2048; budget += 31) {
            mc_java_int_set_limits limits = mc_java_int_set_default_limits();
            if (mode == 0)
                limits.max_retained_bytes = budget;
            else
                limits.max_allocation_bytes = budget;
            mc_java_int_set *history = NULL, *control = NULL;
            mc_java_int_set_status status = mc_java_int_set_create(&limits, &history);
            if (status == MC_JAVA_INT_SET_LIMIT) {
                ++create_limits;
                CHECK(history == NULL);
                continue;
            }
            CHECK(status == MC_JAVA_INT_SET_OK);
            CHECK(mc_java_int_set_create(NULL, &control) == MC_JAVA_INT_SET_OK);
            int32_t a[KEYS], b[KEYS];
            fill_keys(a);
            fill_keys(b);
            mc_chunk_pending pending, expected;
            CHECK(mc_chunk_pending_init_ordered(&pending, a, MEMBERS, history));
            CHECK(mc_chunk_pending_init_ordered(&expected, b, MEMBERS, control));
            for (int32_t step = 0; step < MEMBERS; ++step) {
                int32_t key = step == 0 ? 16 : step == 1 ? 32 : step + 63;
                pending_snapshot snapshot;
                CHECK(take_snapshot(&pending, a, KEYS, &snapshot) == 0);
                bool changed = false, control_changed = false;
                mc_chunk_acquire_status added = mc_chunk_pending_add(&pending, key, &changed);
                if (added == MC_CHUNK_ACQUIRE_LIMIT) {
                    ++add_limits;
                    CHECK(!changed && check_snapshot(&pending, a, &snapshot) == 0);
                } else {
                    ++successes;
                    CHECK(added == MC_CHUNK_ACQUIRE_OK && changed);
                    CHECK(mc_chunk_pending_add(&expected, key, &control_changed) ==
                              MC_CHUNK_ACQUIRE_OK &&
                          control_changed);
                }
                CHECK(check_same(&pending, &expected) == 0);
                if (step % 3 == 2 && pending.count != 0) {
                    int32_t first = 0, other_first = -1;
                    CHECK(mc_chunk_pending_first(&pending, &first) == MC_CHUNK_ACQUIRE_OK);
                    CHECK(mc_chunk_pending_first(&expected, &other_first) == MC_CHUNK_ACQUIRE_OK &&
                          first == other_first);
                    CHECK(mc_chunk_pending_remove(&pending, first, &changed) ==
                              MC_CHUNK_ACQUIRE_OK &&
                          changed);
                    CHECK(mc_chunk_pending_remove(&expected, first, &control_changed) ==
                              MC_CHUNK_ACQUIRE_OK &&
                          control_changed);
                    CHECK(mc_chunk_pending_add(&pending, first, &changed) == MC_CHUNK_ACQUIRE_OK &&
                          changed);
                    CHECK(mc_chunk_pending_add(&expected, first, &control_changed) ==
                              MC_CHUNK_ACQUIRE_OK &&
                          control_changed);
                    ++recovered;
                    CHECK(check_same(&pending, &expected) == 0);
                }
            }
            mc_java_int_set_free(history);
            mc_java_int_set_free(control);
        }
    }
    CHECK(create_limits != 0 && add_limits != 0 && successes != 0 && recovered != 0);
    return 0;
}

static int test_invalid_pairing(void) {
    mc_java_int_set *history = NULL;
    CHECK(mc_java_int_set_create(NULL, &history) == MC_JAVA_INT_SET_OK);
    int32_t keys[KEYS];
    fill_keys(keys);
    mc_chunk_pending pending;
    CHECK(mc_chunk_pending_init_ordered(&pending, keys, MEMBERS, history));
    bool changed = false;
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(mc_chunk_pending_add(&pending, 32, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    for (unsigned invalid = 0; invalid < 4; ++invalid) {
        /* Deliberate contract violations exercise rejection without repair. */
        if (invalid == 0)
            pending.count = 1;
        else if (invalid == 1)
            keys[1] = keys[0];
        else if (invalid == 2) {
            CHECK(mc_java_int_set_remove(history, 16, &changed) == MC_JAVA_INT_SET_OK && changed);
            CHECK(mc_java_int_set_add(history, 64, &changed) == MC_JAVA_INT_SET_OK && changed);
        } else
            pending.count = pending.capacity + 1u;
        pending_snapshot snapshot;
        CHECK(take_snapshot(&pending, keys, KEYS, &snapshot) == 0);
        CHECK(!mc_chunk_pending_validate(&pending));
        changed = true;
        int32_t first = 123;
        CHECK(mc_chunk_pending_add(&pending, 99, &changed) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT &&
              changed);
        CHECK(mc_chunk_pending_remove(&pending, 32, &changed) ==
                  MC_CHUNK_ACQUIRE_INVALID_ARGUMENT &&
              changed);
        CHECK(mc_chunk_pending_first(&pending, &first) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT &&
              first == 123);
        CHECK(mc_chunk_pending_clear(&pending) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
        CHECK(check_snapshot(&pending, keys, &snapshot) == 0);
        if (invalid == 0 || invalid == 3)
            pending.count = 2;
        else if (invalid == 1)
            keys[1] = 32;
        else {
            CHECK(mc_java_int_set_remove(history, 64, &changed) == MC_JAVA_INT_SET_OK && changed);
            CHECK(mc_java_int_set_add(history, 16, &changed) == MC_JAVA_INT_SET_OK && changed);
        }
        CHECK(mc_chunk_pending_validate(&pending));
    }
    mc_java_int_set_free(history);
    return 0;
}

static bool register_tile_entities(void *world, const mc_chunk_acquire_context *context,
                                   mc_chunk_state *chunk) {
    (void)context;
    (void)chunk;
    ++*(unsigned *)world;
    return true;
}

static bool register_entities(void *world, const mc_chunk_acquire_context *context,
                              mc_chunk_state *chunk, size_t slice) {
    (void)context;
    (void)chunk;
    (void)slice;
    ++*(unsigned *)world;
    return true;
}

static int test_acquire_mutations(void) {
    mc_java_int_set *history = NULL;
    CHECK(mc_java_int_set_create(NULL, &history) == MC_JAVA_INT_SET_OK);
    int32_t keys[KEYS];
    fill_keys(keys);
    mc_chunk_pending pending;
    CHECK(mc_chunk_pending_init_ordered(&pending, keys, MEMBERS, history));
    bool changed = false;
    CHECK(mc_chunk_pending_add(&pending, 16, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    CHECK(mc_chunk_pending_add(&pending, 32, &changed) == MC_CHUNK_ACQUIRE_OK && changed);
    /* A normal cache hit needs only height storage, not voxel allocation. */
    static mc_chunk_data height_data;
    mc_chunk_state cached = {.data = &height_data, .x = -7, .z = 9, .last_save_time = 999};
    mc_chunk_state fallback = {.blank = true};
    mc_chunk_cache_entry entries[1];
    mc_chunk_state *insertions[1];
    mc_chunk_cache cache;
    CHECK(mc_chunk_cache_init(&cache, entries, 1, insertions, 1));
    CHECK(mc_chunk_cache_publish(&cache, 0, 16, &cached));
    int64_t time = 77;
    int32_t spawn_x = 0, spawn_z = 0;
    bool world_force = false, provider_force = false;
    unsigned registrations = 0;
    mc_chunk_acquire_context context = {.cache = &cache,
                                        .pending = &pending,
                                        .fallback = &fallback,
                                        .world_time = &time,
                                        .world_force_load = &world_force,
                                        .provider_force_load = &provider_force,
                                        .spawn_x = &spawn_x,
                                        .spawn_z = &spawn_z,
                                        .policy = MC_CHUNK_ACQUIRE_DEDICATED,
                                        .world = &registrations,
                                        .register_tile_entities = register_tile_entities,
                                        .register_entities = register_entities};
    CHECK(mc_chunk_acquire_validate(&context));
    pending_snapshot snapshot;
    CHECK(take_snapshot(&pending, keys, KEYS, &snapshot) == 0);
    mc_chunk_state *out = &fallback;
    CHECK(mc_chunk_acquire_get(&context, 32768, 16, &out) == MC_CHUNK_ACQUIRE_OK && out == &cached);
    CHECK(check_snapshot(&pending, keys, &snapshot) == 0); /* b-hit retains pending. */
    out = &fallback;
    CHECK(mc_chunk_acquire(&context, 0, 32, &out) == MC_CHUNK_ACQUIRE_LIMIT && out == &fallback);
    CHECK(check_snapshot(&pending, keys, &snapshot) == 0); /* Preflight precedes removal. */
    CHECK(mc_chunk_acquire(&context, 32768, 16, &out) == MC_CHUNK_ACQUIRE_OK && out == &cached);
    const int32_t remaining[] = {32}, dropped[] = {32, 16};
    CHECK(check_order(&pending, remaining, 1) == 0 && keys[0] == 32);
    CHECK(!cached.active && cached.last_save_time == 999 && registrations == 0);
    CHECK(mc_chunk_acquire_mark_drop(&context, 0, 16) == MC_CHUNK_ACQUIRE_OK);
    CHECK(check_order(&pending, dropped, 2) == 0 && keys[0] == 32 && keys[1] == 16);
    CHECK(take_snapshot(&pending, keys, KEYS, &snapshot) == 0);
    CHECK(mc_chunk_acquire_mark_drop(&context, 32768, 16) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_acquire_mark_drop(&context, 0, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(check_snapshot(&pending, keys, &snapshot) == 0);
    CHECK(cache.count == 1 && cache.insertion_count == 1 && entries[0].chunk == &cached &&
          insertions[0] == &cached && registrations == 0);
    mc_java_int_set_free(history);
    return 0;
}

int main(void) {
    if (test_arguments() || test_legacy() || test_ordered() || test_survivors_and_clear() ||
        test_capacity_and_history_entry_limit() || test_history_byte_limits() ||
        test_invalid_pairing() || test_acquire_mutations())
        return 1;
    printf("pending collection contracts: %u checks\n", checks);
    return 0;
}
