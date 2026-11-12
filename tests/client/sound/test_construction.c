/* Authored source-only controls. Private clock-boundary inputs are declared
 * native fixtures, not observations or entropy supplied to a real JVM ctor. */
#include "mc/client/sound/construction.h"
#include "random_profile_internal.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Sound construction check%u line%d: %s\n", checks, __LINE__, #x);      \
            ok = false;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* These forbidden-boundary probes never stand in for a completed backend.
 * The constructor must not call any one of them. A separate authored control
 * uses the existing actual cold backend context, not this table. */
typedef struct boundaries {
    unsigned reached;
} boundaries;

static mc_sound_status forbidden(void *context, mc_sound_error *error) {
    ++((boundaries *)context)->reached;
    if (error != NULL)
        *error = (mc_sound_error){.status = MC_SOUND_INVALID_ARGUMENT,
                                  .operation = "forbidden constructor backend boundary",
                                  .source_pc = -1};
    return MC_SOUND_INVALID_ARGUMENT;
}

static mc_sound_status configure(void *c, mc_sound_manager_configuration value, mc_sound_error *e) {
    (void)value;
    return forbidden(c, e);
}

static mc_sound_status construct(void *c, void **out, mc_sound_error *e) {
    (void)out;
    return forbidden(c, e);
}

static mc_sound_status command(void *c, void *b, const mc_sound_manager_command *v,
                               mc_sound_error *e) {
    (void)b;
    (void)v;
    return forbidden(c, e);
}

static mc_sound_status playing(void *c, void *b, const char *name, bool *out, mc_sound_error *e) {
    (void)b;
    (void)name;
    (void)out;
    return forbidden(c, e);
}

static mc_sound_status retire_backend(void *c, void **b, mc_sound_error *e) {
    (void)b;
    return forbidden(c, e);
}

static bool shared_create(boundaries *calls, mc_sound_manager_shared **out) {
    const mc_sound_manager_backend api = {calls,   configure, construct,
                                          command, playing,   retire_backend};
    return mc_sound_manager_shared_create(&api, out, NULL) == MC_SOUND_OK;
}

static uint64_t progression(size_t count) {
    uint64_t value = UINT64_C(8682522807148012);
    for (size_t i = 0; i < count; ++i)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static void inputs(mc_java_random_constructor_input *data, size_t count) {
    for (size_t i = 0; i < count; ++i)
        data[i] = (mc_java_random_constructor_input){
            progression(i + 1), -INT64_C(70000001) + (int64_t)i * INT64_C(1234567)};
}

static bool profile_create(const mc_java_random_constructor_input *data, size_t count,
                           mc_java_random_profile **out) {
    return mc_java_random_profile_create(SIZE_MAX, out) == MC_JAVA_RANDOM_PROFILE_OK &&
           mc_java_random_profile_bind_constructor_inputs(*out, data, count) ==
               MC_JAVA_RANDOM_PROFILE_OK;
}

static uint64_t initial(const mc_java_random_constructor_input *input) {
    return (input->committed_uniquifier ^ (uint64_t)input->nanotime ^ UINT64_C(25214903917)) &
           UINT64_C(281474976710655);
}

static int32_t next12000(uint64_t *state) {
    for (;;) {
        *state = (*state * UINT64_C(25214903917) + UINT64_C(11)) & UINT64_C(281474976710655);
        const uint32_t bits = (uint32_t)(*state >> 17);
        const uint32_t value = bits % UINT32_C(12000);
        if ((uint64_t)bits - value + UINT64_C(11999) <= INT32_MAX)
            return (int32_t)value;
    }
}

static bool fresh_random(const mc_java_random *value, uint64_t state) {
    uint64_t gaussian;
    memcpy(&gaussian, &value->next_next_gaussian, sizeof gaussian);
    return value->state == state && !value->have_next_next_gaussian && gaussian == 0;
}

static bool site(const mc_sound_constructor_site *value, const char *name, int32_t pc) {
    return value->pc == pc && strcmp(value->class_name, name) == 0 &&
           strcmp(value->method, "<init>") == 0 && strcmp(value->descriptor, "()V") == 0;
}

static bool cause_equal(const mc_sound_error *a, const mc_sound_error *b) {
    return a->status == b->status && a->source_pc == b->source_pc && a->exception == b->exception &&
           a->system_error == b->system_error && a->random_status == b->random_status &&
           (a->operation == b->operation || (a->operation != NULL && b->operation != NULL &&
                                             strcmp(a->operation, b->operation) == 0));
}

static bool cold(const mc_java_random_profile *p, size_t consumed, size_t committed) {
    return p->constructor_input_consumed == consumed &&
           atomic_load(&p->uniquifier) == progression(committed) &&
           atomic_load(&p->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD && p->math == NULL;
}

static bool random_failure(size_t completed) {
    bool ok = true;
    boundaries calls = {0}, eager_calls = {0};
    mc_sound_manager_shared *shared = NULL, *eager_shared = NULL;
    mc_sound_manager *owner = NULL;
    mc_java_random_profile *profile = NULL, *eager_profile = NULL;
    mc_java_random_constructor_input tape[4];
    inputs(tape, 4);
    mc_sound_manager_constructor_state state;
    mc_sound_manager_constructor_error error;
    mc_sound_manager_report report = {.source_pc = 12345};
    CHECK(profile_create(tape, completed, &profile) &&
          profile_create(tape, completed, &eager_profile));
    CHECK(shared_create(&calls, &shared) && shared_create(&eager_calls, &eager_shared));
    CHECK(mc_sound_manager_allocate(shared, &owner, NULL) == MC_SOUND_OK && owner != NULL);
    CHECK(cold(profile, 0, 0) && mc_sound_manager_read_construction(owner, &state));
    CHECK(state.phase == MC_SOUND_MANAGER_RAW && state.last_store_pc == -1 &&
          state.published_pool_mask == 0 && state.pending_pool == NULL && !state.random_published);
    mc_sound_manager_state ready_view = {.sound_counter = 12345};
    CHECK(!mc_sound_manager_read_state(owner, &ready_view) && ready_view.sound_counter == 12345);
    CHECK(mc_sound_manager_ui(owner, (mc_sound_text){.is_null = true}, 1, 1, &report, NULL) ==
              MC_SOUND_INVALID_ARGUMENT &&
          report.source_pc == 12345);
    CHECK(mc_sound_manager_shared_retire(&shared, NULL) == MC_SOUND_BUSY && shared != NULL);
    CHECK(mc_sound_manager_construct(owner, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &error) ==
          MC_SOUND_RANDOM_FAILED);
    CHECK(mc_sound_manager_read_construction(owner, &state) &&
          state.phase == MC_SOUND_MANAGER_FAILED && !state.active && !state.values.active &&
          !state.values.shared_initialized && !state.values.backend_present);
    CHECK(!mc_sound_manager_read_state(owner, &ready_view) && ready_view.sound_counter == 12345);
    CHECK(state.published_pool_mask == (completed < 3 ? (1u << completed) - 1u : 7u));
    CHECK(state.last_store_pc == (completed == 0   ? -1
                                  : completed == 1 ? 12
                                  : completed == 2 ? 23
                                                   : 39));
    CHECK(state.counter_published == (completed == 3) && !state.random_published &&
          !state.delay_published);
    CHECK(error.native_policy && error.cause.status == MC_SOUND_RANDOM_FAILED &&
          error.cause.random_status == MC_JAVA_RANDOM_PROFILE_RANGE &&
          error.frame_count == (completed < 3 ? 2u : 1u));
    const int32_t outer[] = {9, 20, 31, 47};
    CHECK(site(&error.frames[0], "yo", outer[completed]));
    if (completed < 3) {
        CHECK(error.pool_index == (int32_t)completed && site(&error.frames[1], "hr", 9) &&
              error.cause.source_pc == 9);
        CHECK(state.pending_pool != NULL && state.pending_pool_index == (int32_t)completed &&
              state.pending_prefix.last_store_pc == -1 && !state.pending_prefix.random_published &&
              !state.pending_prefix.map_published && !state.pending_prefix.list_published &&
              !state.pending_prefix.count_published &&
              !state.pending_prefix.strip_digits_published);
        mc_sound_pool_state pending;
        CHECK(mc_sound_pool_read_state(state.pending_pool, &pending, NULL) == MC_SOUND_OK &&
              pending.allocation_bytes > 0 && !pending.strip_digits && pending.count == 0);
    } else
        CHECK(error.pool_index == -1 && error.cause.source_pc == 47 && state.pending_pool == NULL);
    for (size_t i = 0; i < completed && i < 3; ++i) {
        mc_sound_pool_state pool;
        const mc_sound_pool_constructor_prefix *p = &state.pool_prefixes[i];
        CHECK(state.pools[i] != NULL && p->last_store_pc == 44 && p->random_published &&
              p->map_published && p->list_published && p->count_published &&
              p->strip_digits_published);
        CHECK(mc_sound_pool_read_state(state.pools[i], &pool, NULL) == MC_SOUND_OK &&
              pool.strip_digits && pool.count == 0 && pool.group_count == 0 &&
              pool.full_entry_count == 0 && fresh_random(&pool.random, initial(&tape[i])));
    }
    CHECK(cold(profile, completed, completed + 1) &&
          profile->constructor_input_failure == MC_JAVA_RANDOM_INPUT_EXHAUSTED);
    mc_sound_manager *preserved =
        owner; /* Actual live failed owner, never a fabricated sentinel. */
    mc_sound_error eager_error;
    CHECK(mc_sound_manager_create(eager_shared, eager_profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES,
                                  &preserved, &eager_error) == MC_SOUND_RANDOM_FAILED &&
          preserved == owner);
    CHECK(cause_equal(&error.cause, &eager_error) && cold(eager_profile, completed, completed + 1));
    CHECK(mc_sound_manager_shared_retire(&eager_shared, NULL) == MC_SOUND_OK &&
          eager_shared == NULL);
    CHECK(mc_sound_manager_construct(owner, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, NULL) ==
              MC_SOUND_INVALID_ARGUMENT &&
          cold(profile, completed, completed + 1));
    CHECK(mc_sound_manager_bind(owner, NULL, NULL, &report, NULL) == MC_SOUND_INVALID_ARGUMENT);
    CHECK(mc_sound_manager_retire(&owner, NULL) == MC_SOUND_OK && owner == NULL);
    CHECK(mc_sound_manager_ui(owner, (mc_sound_text){.is_null = true}, 1, 1, NULL, NULL) ==
          MC_SOUND_INVALID_ARGUMENT);
    CHECK(calls.reached == 0 && eager_calls.reached == 0);
cleanup:
    if (mc_sound_manager_retire(&owner, NULL) != MC_SOUND_OK)
        ok = false;
    if (mc_sound_manager_shared_retire(&shared, NULL) != MC_SOUND_OK)
        ok = false;
    if (mc_sound_manager_shared_retire(&eager_shared, NULL) != MC_SOUND_OK)
        ok = false;
    mc_java_random_profile_free(profile);
    mc_java_random_profile_free(eager_profile);
    return ok;
}

static bool success_and_busy(void) {
    bool ok = true;
    boundaries calls = {0}, eager_calls = {0};
    mc_sound_manager_shared *shared = NULL, *eager_shared = NULL;
    mc_sound_manager *owner = NULL, *eager = NULL;
    mc_sound_pool_registration *registration = NULL;
    mc_java_random_profile *profile = NULL, *eager_profile = NULL;
    mc_java_random_constructor_input tape[5];
    inputs(tape, 5);
    mc_sound_manager_constructor_state state;
    mc_sound_manager_state eager_state;
    CHECK(profile_create(tape, 5, &profile) && profile_create(tape, 5, &eager_profile));
    CHECK(shared_create(&calls, &shared) && shared_create(&eager_calls, &eager_shared));
    CHECK(mc_sound_manager_allocate(shared, &owner, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_construct(owner, NULL, 0, NULL) == MC_SOUND_INVALID_ARGUMENT &&
          cold(profile, 0, 0));
    CHECK(mc_sound_manager_construct(owner, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, NULL) ==
          MC_SOUND_OK);
    mc_sound_error eager_error;
    CHECK(mc_sound_manager_create(eager_shared, eager_profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES,
                                  &eager, &eager_error) == MC_SOUND_OK &&
          eager_error.source_pc == 67);
    CHECK(mc_sound_manager_read_construction(owner, &state) &&
          state.phase == MC_SOUND_MANAGER_READY && state.last_store_pc == 64 &&
          state.published_pool_mask == 7 && state.pending_pool == NULL && state.counter_published &&
          state.random_published && state.delay_published);
    CHECK(mc_sound_manager_read_state(eager, &eager_state));
    uint64_t expected = initial(&tape[3]);
    const int32_t delay = next12000(&expected);
    CHECK(state.values.music_delay == delay && fresh_random(&state.values.random, expected) &&
          eager_state.music_delay == delay && fresh_random(&eager_state.random, expected) &&
          state.values.sound_counter == 0 && state.values.settings == NULL);
    for (size_t i = 0; i < 3; ++i) {
        mc_sound_pool_state a, b;
        CHECK(mc_sound_pool_read_state(state.pools[i], &a, NULL) == MC_SOUND_OK &&
              mc_sound_pool_read_state(mc_sound_manager_pool(eager, (mc_sound_manager_pool_kind)i),
                                       &b, NULL) == MC_SOUND_OK);
        CHECK(fresh_random(&a.random, initial(&tape[i])) &&
              fresh_random(&b.random, initial(&tape[i])) &&
              a.allocation_bytes == b.allocation_bytes &&
              a.allocation_limit == b.allocation_limit && a.strip_digits && b.strip_digits &&
              a.count == 0 && b.count == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(state.pools[i] != state.pools[j]);
    }
    mc_java_random next, eager_next;
    CHECK(mc_java_random_construct_default(profile, &next) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_java_random_construct_default(eager_profile, &eager_next) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(fresh_random(&next, initial(&tape[4])) && fresh_random(&eager_next, initial(&tape[4])) &&
          cold(profile, 5, 5) && cold(eager_profile, 5, 5));
    CHECK(mc_java_random_profile_constructor_inputs_complete(profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_java_random_profile_constructor_inputs_complete(eager_profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    /* Controlled native fixture: mutate the actual pool through its real
     * registration API under exclusive access; never assign an owner field. */
    const uint16_t name[] = {'b', 'u', 's', 'y', '.', 'o', 'g', 'g'};
    CHECK(mc_sound_pool_begin((mc_sound_pool *)state.pools[1], (mc_sound_text){name, 8, false},
                              &registration, NULL, NULL) == MC_SOUND_OK);
    mc_sound_manager *saved = owner;
    CHECK(mc_sound_manager_retire(&owner, NULL) == MC_SOUND_BUSY && owner == saved);
    CHECK(mc_sound_manager_read_construction(owner, &state) &&
          state.phase == MC_SOUND_MANAGER_READY && state.published_pool_mask == 7 &&
          state.pools[0] != NULL && state.pools[2] != NULL);
    CHECK(mc_sound_manager_shared_retire(&shared, NULL) == MC_SOUND_BUSY);
    mc_sound_pool_registration_free(registration);
    registration = NULL;
    CHECK(mc_sound_manager_retire(&owner, NULL) == MC_SOUND_OK && owner == NULL);
    CHECK(mc_sound_manager_retire(&owner, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_construct(eager, eager_profile, 0, NULL) == MC_SOUND_INVALID_ARGUMENT);
    /* The source-supported null-settings setup catches its dereference and
     * latches the real shared flag without reaching a backend callback. A raw
     * replacement and its retained hr failure must preserve that namespace. */
    mc_sound_manager_report report;
    CHECK(mc_sound_manager_bind(eager, NULL, NULL, &report, NULL) == MC_SOUND_OK &&
          report.setup.throwable_caught && mc_sound_manager_read_state(eager, &eager_state) &&
          eager_state.shared_initialized && !eager_state.backend_present);
    CHECK(mc_sound_manager_allocate(eager_shared, &owner, NULL) == MC_SOUND_OK &&
          mc_sound_manager_read_construction(owner, &state) && state.values.shared_initialized &&
          !state.values.backend_present);
    CHECK(mc_sound_manager_construct(owner, eager_profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES,
                                     NULL) == MC_SOUND_RANDOM_FAILED &&
          mc_sound_manager_read_construction(owner, &state) && state.values.shared_initialized &&
          !state.values.backend_present);
    CHECK(mc_sound_manager_retire(&owner, NULL) == MC_SOUND_OK && owner == NULL &&
          mc_sound_manager_read_state(eager, &eager_state) && eager_state.shared_initialized);
    CHECK(calls.reached == 0 && eager_calls.reached == 0);
cleanup:
    mc_sound_pool_registration_free(registration);
    if (mc_sound_manager_retire(&owner, NULL) != MC_SOUND_OK)
        ok = false;
    if (mc_sound_manager_retire(&eager, NULL) != MC_SOUND_OK)
        ok = false;
    if (mc_sound_manager_shared_retire(&shared, NULL) != MC_SOUND_OK)
        ok = false;
    if (mc_sound_manager_shared_retire(&eager_shared, NULL) != MC_SOUND_OK)
        ok = false;
    mc_java_random_profile_free(profile);
    mc_java_random_profile_free(eager_profile);
    return ok;
}

int main(void) {
    for (size_t i = 0; i < 4; ++i)
        if (!random_failure(i))
            return 1;
    if (!success_and_busy())
        return 1;
    printf("Sound construction: %u checks;4 Random failure prefixes;canonical wrapper/retirement\n",
           checks);
    return 0;
}
