/* Future separately compiled canonical manager/pool allocation controls.
 * MC_SOUND_MANAGER_CALLOC=mc_test_manager_calloc and
 * MC_SOUND_POOL_CALLOC=mc_test_pool_calloc. No production allocator callback. */
#include "mc/client/sound/construction.h"
#include "random_profile_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t manager_calls, pool_calls, manager_fail_at, pool_fail_at;

void *mc_test_manager_calloc(size_t count, size_t bytes) {
    ++manager_calls;
    return manager_fail_at == manager_calls ? NULL : calloc(count, bytes);
}

void *mc_test_pool_calloc(size_t count, size_t bytes) {
    ++pool_calls;
    return pool_fail_at == pool_calls ? NULL : calloc(count, bytes);
}

static unsigned checks, forbidden_calls;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Sound constructor allocation check%u line%d: %s\n", checks, __LINE__, \
                    #x);                                                                           \
            ok = false;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static mc_sound_status config(void *c, mc_sound_manager_configuration v, mc_sound_error *e) {
    (void)c;
    (void)v;
    (void)e;
    ++forbidden_calls;
    return MC_SOUND_INVALID_ARGUMENT;
}

static mc_sound_status ctor(void *c, void **out, mc_sound_error *e) {
    (void)c;
    (void)out;
    (void)e;
    ++forbidden_calls;
    return MC_SOUND_INVALID_ARGUMENT;
}

static mc_sound_status issue(void *c, void *b, const mc_sound_manager_command *v,
                             mc_sound_error *e) {
    (void)c;
    (void)b;
    (void)v;
    (void)e;
    ++forbidden_calls;
    return MC_SOUND_INVALID_ARGUMENT;
}

static mc_sound_status query(void *c, void *b, const char *n, bool *out, mc_sound_error *e) {
    (void)c;
    (void)b;
    (void)n;
    (void)out;
    (void)e;
    ++forbidden_calls;
    return MC_SOUND_INVALID_ARGUMENT;
}

static uint64_t u(size_t count) {
    uint64_t value = UINT64_C(8682522807148012);
    for (size_t i = 0; i < count; ++i)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static bool priorities(void) {
    bool ok = true;
    mc_java_random_profile *profile = NULL;
    mc_sound_manager_shared *shared = NULL;
    mc_sound_manager *sentinel = NULL;
    const mc_sound_manager_backend api = {NULL, config, ctor, issue, query, ctor};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, NULL, 0) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_sound_manager_shared_create(&api, &shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_allocate(shared, &sentinel, NULL) == MC_SOUND_OK);
    mc_sound_manager *out = sentinel;
    mc_sound_error error;
    manager_calls = pool_calls = 0;
    manager_fail_at = pool_fail_at = 1;
    CHECK(mc_sound_manager_create(shared, NULL, 0, &out, &error) == MC_SOUND_INVALID_ARGUMENT &&
          out == sentinel && manager_calls == 0 && pool_calls == 0);
    CHECK(mc_sound_manager_create(shared, profile, 0, NULL, &error) == MC_SOUND_INVALID_ARGUMENT &&
          manager_calls == 0 && pool_calls == 0);
    CHECK(mc_sound_manager_create(shared, profile, 0, &out, &error) == MC_SOUND_ALLOCATION_FAILED &&
          out == sentinel && manager_calls == 1 && pool_calls == 0 &&
          strcmp(error.operation, "SoundManager owner") == 0 && error.source_pc == -1);
    manager_calls = pool_calls = 0;
    manager_fail_at = 0;
    CHECK(mc_sound_manager_create(shared, profile, 0, &out, &error) == MC_SOUND_LIMIT &&
          out == sentinel && manager_calls == 1 && pool_calls == 0 &&
          strcmp(error.operation, "SoundPool allocation budget") == 0 && error.source_pc == -1);
    manager_calls = pool_calls = 0;
    CHECK(mc_sound_manager_create(shared, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &out,
                                  &error) == MC_SOUND_ALLOCATION_FAILED &&
          out == sentinel && manager_calls == 1 && pool_calls == 1 &&
          strcmp(error.operation, "SoundPool owner") == 0 && error.source_pc == -1);
    CHECK(atomic_load(&profile->uniquifier) == u(0) && profile->constructor_input_consumed == 0 &&
          !atomic_load(&profile->constructor_admitted));
cleanup:
    manager_fail_at = pool_fail_at = 0;
    if (mc_sound_manager_retire(&sentinel, NULL) != MC_SOUND_OK)
        ok = false;
    if (mc_sound_manager_shared_retire(&shared, NULL) != MC_SOUND_OK)
        ok = false;
    mc_java_random_profile_free(profile);
    return ok;
}

static bool retained_allocation(size_t failed_pool) {
    bool ok = true;
    mc_java_random_profile *profile = NULL;
    mc_sound_manager_shared *shared = NULL;
    mc_sound_manager *owner = NULL;
    mc_java_random_constructor_input tape[3];
    for (size_t i = 0; i < 3; ++i)
        tape[i] = (mc_java_random_constructor_input){u(i + 1), (int64_t)i + 111};
    const mc_sound_manager_backend api = {NULL, config, ctor, issue, query, ctor};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_java_random_profile_bind_constructor_inputs(profile, tape, failed_pool) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_sound_manager_shared_create(&api, &shared, NULL) == MC_SOUND_OK &&
          mc_sound_manager_allocate(shared, &owner, NULL) == MC_SOUND_OK);
    manager_calls = pool_calls = 0;
    manager_fail_at = 0;
    pool_fail_at = failed_pool + 1;
    mc_sound_manager_constructor_error error;
    CHECK(mc_sound_manager_construct(owner, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &error) ==
              MC_SOUND_ALLOCATION_FAILED &&
          manager_calls == 0 && pool_calls == failed_pool + 1);
    const int32_t pcs[] = {5, 16, 27};
    CHECK(error.pool_index == (int32_t)failed_pool && error.frame_count == 1 &&
          error.frames[0].pc == pcs[failed_pool] && error.cause.source_pc == -1 &&
          error.cause.status == MC_SOUND_ALLOCATION_FAILED && error.native_policy);
    mc_sound_manager_constructor_state state;
    CHECK(mc_sound_manager_read_construction(owner, &state) &&
          state.phase == MC_SOUND_MANAGER_FAILED &&
          state.published_pool_mask == (1u << failed_pool) - 1u && state.pending_pool == NULL &&
          state.pending_pool_index == (int32_t)failed_pool && !state.counter_published);
    CHECK(profile->constructor_input_consumed == failed_pool &&
          atomic_load(&profile->uniquifier) == u(failed_pool));
    CHECK(mc_sound_manager_shared_retire(&shared, NULL) == MC_SOUND_BUSY && shared != NULL);
    CHECK(mc_sound_manager_retire(&owner, NULL) == MC_SOUND_OK && owner == NULL);
    CHECK(mc_sound_manager_shared_retire(&shared, NULL) == MC_SOUND_OK && shared == NULL);
cleanup:
    manager_fail_at = pool_fail_at = 0;
    if (mc_sound_manager_retire(&owner, NULL) != MC_SOUND_OK)
        ok = false;
    if (mc_sound_manager_shared_retire(&shared, NULL) != MC_SOUND_OK)
        ok = false;
    mc_java_random_profile_free(profile);
    return ok;
}

int main(void) {
    if (!priorities())
        return 1;
    for (size_t i = 0; i < 3; ++i)
        if (!retained_allocation(i))
            return 1;
    if (forbidden_calls != 0)
        return 1;
    printf(
        "Sound construction allocation: %u checks;legacy priorities/3 retained pool allocations\n",
        checks);
    return 0;
}
