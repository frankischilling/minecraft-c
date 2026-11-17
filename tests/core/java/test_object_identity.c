#include "object_identity_internal.h"
#include "random_profile_internal.h"

#include <stdio.h>

static unsigned checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int words_equal(mc_java_identity_thread actual, mc_java_identity_thread expected) {
    return actual.x == expected.x && actual.y == expected.y && actual.z == expected.z &&
           actual.w == expected.w;
}

static int authored_draws(void) {
    mc_java_identity_thread thread = {1, 2, 3, 4};
    mc_java_identity_cell cells[4] = {{0}};
    const uint32_t hashes[] = {UINT32_C(0x80d), UINT32_C(0x181f), 4, UINT32_C(0x2020)};
    const mc_java_identity_thread states[] = {
        {2, 3, 4, UINT32_C(0x80d)},
        {3, 4, UINT32_C(0x80d), UINT32_C(0x181f)},
        {4, UINT32_C(0x80d), UINT32_C(0x181f), 4},
        {UINT32_C(0x80d), UINT32_C(0x181f), 4, UINT32_C(0x2020)},
    };
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(cells[i].hash == 0);
        CHECK(mc_java_object_identity_hash(&thread, &cells[i]) == hashes[i]);
        CHECK(cells[i].hash == hashes[i]);
        CHECK(words_equal(thread, states[i]));
        CHECK(mc_java_object_identity_hash(&thread, &cells[i]) == hashes[i]);
        CHECK(words_equal(thread, states[i]));
    }
    return 0;
}

static int full_word_and_fallback(void) {
    mc_java_identity_thread thread = {UINT32_C(0x100000), 0, 0, 0};
    mc_java_identity_cell first = {0}, second = {0};
    CHECK(mc_java_object_identity_hash(&thread, &first) == UINT32_C(0x901000));
    CHECK(thread.w == UINT32_C(0x80901000));
    CHECK(words_equal(thread, (mc_java_identity_thread){0, 0, 0, UINT32_C(0x80901000)}));
    CHECK(mc_java_object_identity_hash(&thread, &second) == UINT32_C(0x900012));
    CHECK(thread.w == UINT32_C(0x80900012) && thread.z == UINT32_C(0x80901000));
    thread = (mc_java_identity_thread){0, 1, 2, 0};
    first = (mc_java_identity_cell){0};
    CHECK(mc_java_object_identity_hash(&thread, &first) == UINT32_C(0xbad));
    CHECK(first.hash == UINT32_C(0xbad));
    CHECK(words_equal(thread, (mc_java_identity_thread){1, 2, 0, 0}));
    CHECK(mc_java_object_identity_hash(&thread, &first) == UINT32_C(0xbad));
    CHECK(words_equal(thread, (mc_java_identity_thread){1, 2, 0, 0}));
    return 0;
}

static int null_cached_and_cross_thread(void) {
    mc_java_identity_thread first = {1, 2, 3, 4};
    mc_java_identity_thread second = {5, 6, 7, 8};
    mc_java_identity_cell cell = {0};
    CHECK(mc_java_object_identity_hash(&first, NULL) == 0);
    CHECK(mc_java_object_identity_hash(NULL, NULL) == 0);
    CHECK(words_equal(first, (mc_java_identity_thread){1, 2, 3, 4}));
    CHECK(mc_java_object_identity_hash(&first, &cell) == UINT32_C(0x80d));
    CHECK(mc_java_object_identity_hash(&second, &cell) == UINT32_C(0x80d));
    CHECK(words_equal(second, (mc_java_identity_thread){5, 6, 7, 8}));
    CHECK(mc_java_object_identity_hash(NULL, &cell) == UINT32_C(0x80d));
    CHECK(words_equal(first, (mc_java_identity_thread){2, 3, 4, UINT32_C(0x80d)}));
    /* Hash collisions do not make two actual objects the same reference. */
    mc_java_identity_cell distinct = {0};
    second = (mc_java_identity_thread){1, 2, 3, 4};
    CHECK(mc_java_object_identity_hash(&second, &distinct) == cell.hash);
    CHECK(&cell != &distinct);
    CHECK(words_equal(first, second));
    cell = (mc_java_identity_cell){UINT32_C(0x7fffffff)};
    CHECK(mc_java_object_identity_hash(&first, &cell) == UINT32_C(0x7fffffff));
    CHECK(words_equal(first, second));
    return 0;
}

static int native_profile(void) {
    mc_java_identity_profile native;
    mc_java_identity_profile_initialize(&native);
    CHECK(native.startup_random == UINT32_C(1422014746));
    CHECK(words_equal(native.main_thread,
                      (mc_java_identity_thread){UINT32_C(1422014746), UINT32_C(842502087),
                                                UINT32_C(0x8767), UINT32_C(273326509)}));
    mc_java_identity_cell a = {0}, b = {0};
    CHECK(mc_java_object_identity_hash(&native.main_thread, &a) == UINT32_C(0x55054057));
    CHECK(mc_java_object_identity_hash(&native.main_thread, &b) == UINT32_C(0x5bc2b487));
    CHECK(native.main_thread.w == UINT32_C(0xdbc2b487));
    CHECK(native.startup_random == UINT32_C(1422014746));
    CHECK(mc_java_random_profile_main_identity(NULL) == NULL);

    mc_java_random_profile *profile = NULL, *second = NULL;
    CHECK(mc_java_random_profile_create(1024u * 1024u, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_java_identity_thread *thread = mc_java_random_profile_main_identity(profile);
    CHECK(thread == &profile->identity.main_thread);
    CHECK(thread == mc_java_random_profile_main_identity(profile));
    const uint64_t initial_uniquifier = atomic_load(&profile->uniquifier);
    const unsigned initial_phase = atomic_load(&profile->holder_phase);
    const size_t bytes = mc_java_random_profile_allocation_bytes(profile);
    a = (mc_java_identity_cell){0};
    CHECK(mc_java_object_identity_hash(thread, &a) == UINT32_C(0x55054057));
    CHECK(atomic_load(&profile->uniquifier) == initial_uniquifier);
    CHECK(atomic_load(&profile->holder_phase) == initial_phase && profile->math == NULL);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == bytes);
    const mc_java_identity_thread saved = *thread;
    mc_java_random random;
    CHECK(mc_java_random_construct_default(profile, &random) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(words_equal(*thread, saved));
    double value = -1.0;
    CHECK(mc_java_math_random(profile, &value) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(value >= 0.0 && value < 1.0 && words_equal(*thread, saved));
    CHECK(mc_java_object_identity_hash(thread, &a) == UINT32_C(0x55054057));
    CHECK(words_equal(*thread, saved));
    CHECK(mc_java_random_profile_create(1024u * 1024u, &second) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_main_identity(second) != thread);
    b = (mc_java_identity_cell){0};
    CHECK(mc_java_object_identity_hash(mc_java_random_profile_main_identity(second), &b) ==
          UINT32_C(0x55054057));
    CHECK(words_equal(*thread, saved));
    mc_java_random_profile_free(second);
    mc_java_random_profile_free(profile);
    return 0;
}

int main(void) {
    if (authored_draws() != 0 || full_word_and_fallback() != 0 ||
        null_cached_and_cross_thread() != 0 || native_profile() != 0)
        return 1;
    printf("Java object identity: %u checks passed (authored states and native startup)\n", checks);
    return 0;
}
