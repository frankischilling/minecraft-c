/* Authored native LS.V inputs and actual WorldAccess delivery. These controls
 * are not an unchanged-original LS.V execution or server-HL runtime replay. */
#define main spawn_effect_unused_mob_main
#include "../test_mob_construction.c"
#undef main

#include "block/registry/registry_internal.h"
#include "mc/entity/living/spawn_effect.h"
#include "world/runtime/world_access_internal.h"

#include <xmmintrin.h>

typedef struct effect_observer {
    fixture *fixture;
    mc_entity *actor;
    mc_world_access *access;
    mc_world_access_receipt *receipt;
    mc_java_random expected;
    size_t calls, fail_at;
    bool valid, source_failure, capture_limit, mutate, clear_world, change_fp, nan_profile;
    uint64_t expected_nan[3];
} effect_observer;

/* An independent binary32/binary64 evaluation of the declared Source formula.
 * In particular the velocity*10 offsets distinguish this method from death. */
static double effect_position(double coordinate, float draw, float extent, double velocity,
                              bool horizontal) {
    volatile float product = draw * extent;
    if (horizontal)
        product = product * 2.0f;
    volatile double sum = coordinate + (double)product;
    if (horizontal)
        sum = sum - (double)extent;
    volatile double displacement = velocity * 10.0;
    return sum - displacement;
}

static mc_world_storage_status effect_particle(void *context, mc_world_runtime *world,
                                               const mc_registry_string *name, double x, double y,
                                               double z, double vx, double vy, double vz,
                                               mc_world_storage_error *cause,
                                               mc_world_access_failure *failure) {
    effect_observer *observer = context;
    const size_t index = observer->calls++;
    static const uint16_t explode[] = {'e', 'x', 'p', 'l', 'o', 'd', 'e'};
    double velocity[3];
    for (size_t i = 0; i < 3; ++i) {
        volatile double scaled = mc_java_random_next_gaussian(&observer->expected) * .02;
        velocity[i] = scaled;
    }
    const float dx = mc_java_random_next_float(&observer->expected);
    const float dy = mc_java_random_next_float(&observer->expected);
    const float dz = mc_java_random_next_float(&observer->expected);
    const mc_entity_base_fields *base = &observer->actor->base;
    const bool positions =
        observer->nan_profile
            ? dbits(x) == observer->expected_nan[0] && dbits(y) == observer->expected_nan[1] &&
                  dbits(z) == observer->expected_nan[2]
            : dbits(x) == dbits(effect_position(base->x, dx, base->width, vx, true)) &&
                  dbits(y) == dbits(effect_position(base->y, dy, base->height, vy, false)) &&
                  dbits(z) == dbits(effect_position(base->z, dz, base->width, vz, true));
    observer->valid &= world == observer->fixture->world && name != NULL && name->length == 7 &&
                       memcmp(name->units, explode, sizeof(explode)) == 0 &&
                       observer->actor->holds == 2 && world->active_calls == 1 &&
                       dbits(vx) == dbits(velocity[0]) && dbits(vy) == dbits(velocity[1]) &&
                       dbits(vz) == dbits(velocity[2]) && positions &&
                       mc_test_java_random_equal(observer->actor->random, &observer->expected);
    if (observer->mutate) {
        /* Explicit reached external callback edits; the effect must reread all
         * these actual Source fields on its next iteration. */
        observer->actor->base.x += .25;
        observer->actor->base.y -= .125;
        observer->actor->base.z += .5;
        observer->actor->base.width = index % 2 == 0 ? .25f : 1.75f;
        observer->actor->base.height = index % 2 == 0 ? 2.25f : .5f;
    }
    if (observer->clear_world && index == 0)
        observer->valid &= mc_entity_set_world(observer->actor, NULL, NULL) == MC_ENTITY_OK;
    if (observer->change_fp)
        _mm_setcsr(_mm_getcsr() | 0x40u);
    if (index != observer->fail_at)
        return MC_WORLD_STORAGE_OK;
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "declared native spawn-effect observer failure", 27};
    if (observer->source_failure) {
        const mc_world_storage_status created = mc_world_access_receipt_create(
            observer->access, NULL, &observer->receipt, failure, NULL);
        if (created != MC_WORLD_STORAGE_OK)
            return created;
        /* This explicitly declared source producer owns its actual immutable
         * invocation receipt. No RenderGlobal or original effect is invented. */
        observer->receipt->exception_class = "java/lang/IllegalStateException";
        if (observer->capture_limit)
            observer->receipt->holds = SIZE_MAX; /* Declared native capture-quota fault. */
    }
    return cause->status;
}

static bool effect_actor(fixture *f, mc_world_runtime *world, int64_t seed, bool cached,
                         mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_PIG, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->entities, f->registry, literal, out, &error) == MC_MOB_OK &&
          mc_mob_construct(*out, world, &error) == MC_MOB_OK);
    (*out)->base.x = 8.25;
    (*out)->base.y = 65.125;
    (*out)->base.z = -.75;
    (*out)->base.width = .9f;
    (*out)->base.height = .9f;
    mc_java_random_reseed((*out)->random, seed);
    if (cached)
        (void)mc_java_random_next_gaussian((*out)->random);
    return true;
}

static bool effect_observe(fixture *f, mc_entity *actor, effect_observer *observer) {
    observer->fixture = f;
    observer->actor = actor;
    observer->expected = *actor->random;
    observer->valid = true;
    const mc_world_access_source_callbacks callbacks = {.particle = effect_particle};
    CHECK(mc_world_access_source_create(&callbacks, observer, &observer->access, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, observer->access, NULL) ==
              MC_WORLD_STORAGE_OK);
    return true;
}

static bool effect_unobserve(fixture *f, effect_observer *observer) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, observer->access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    CHECK(mc_world_access_receipt_retire(&observer->receipt, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_access_free(observer->access, NULL) == MC_WORLD_STORAGE_OK);
    observer->access = NULL;
    return true;
}

static void effect_advance(mc_java_random *random, size_t iterations, bool floats) {
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        for (size_t i = 0; i < 3; ++i)
            (void)mc_java_random_next_gaussian(random);
        if (floats)
            for (size_t i = 0; i < 3; ++i)
                (void)mc_java_random_next_float(random);
    }
}

static bool effect_same_state(const mc_living_spawn_effect_state *a,
                              const mc_living_spawn_effect_state *b) {
    return a->returned == b->returned && a->source_pc == b->source_pc &&
           a->iteration == b->iteration && a->iterations_started == b->iterations_started &&
           a->gaussian_calls == b->gaussian_calls && a->float_calls == b->float_calls &&
           a->particles_returned == b->particles_returned;
}

static bool effect_success(fixture *f) {
    const int64_t seeds[] = {0, 1, INT64_C(123456789), INT64_MIN};
    for (size_t row = 0; row < sizeof(seeds) / sizeof(seeds[0]); ++row)
        for (size_t cached = 0; cached < 2; ++cached) {
            mc_entity *actor = NULL;
            CHECK(effect_actor(f, f->world, seeds[row], cached != 0, &actor));
            effect_observer observer = {.fail_at = SIZE_MAX};
            CHECK(effect_observe(f, actor, &observer));
            unsigned char source_fields[sizeof(actor->base)];
            memcpy(source_fields, &actor->base, sizeof(source_fields));
            const mc_java_random world_random = *f->world->fields.r;
            const uint64_t unique = atomic_load(&f->profile->uniquifier);
            const uint64_t math = atomic_load(&f->profile->math->state);
            const size_t bindings = f->world->persistent_bindings;
            mc_living_spawn_effect_state state = {0};
            mc_living_spawn_effect_error error = {0};
            CHECK(mc_living_spawn_effect(actor, f->registry, NULL, &state, &error) ==
                      MC_WORLD_STORAGE_OK &&
                  state.returned && state.iteration == 20 && state.source_pc == 158 &&
                  state.iterations_started == 20 && state.gaussian_calls == 60 &&
                  state.float_calls == 60 && state.particles_returned == 20 &&
                  observer.calls == 20 && observer.valid);
            CHECK(mc_test_java_random_equal(actor->random, &observer.expected) &&
                  mc_test_java_random_equal(f->world->fields.r, &world_random) &&
                  atomic_load(&f->profile->uniquifier) == unique &&
                  atomic_load(&f->profile->math->state) == math &&
                  memcmp(source_fields, &actor->base, sizeof(source_fields)) == 0 &&
                  f->world->persistent_bindings == bindings && actor->holds == 1);
            mc_living_spawn_effect_error_retire(&error);
            mc_living_spawn_effect_error_retire(&error);
            CHECK(effect_unobserve(f, &observer));
            mc_entity_release(actor);
        }
    return true;
}

static bool effect_limits(fixture *f) {
    const size_t ceilings[] = {0, 1, 19, 21};
    for (size_t row = 0; row < sizeof(ceilings) / sizeof(ceilings[0]); ++row) {
        mc_entity *actor = NULL;
        CHECK(effect_actor(f, f->world, 17, true, &actor));
        effect_observer observer = {.fail_at = SIZE_MAX};
        CHECK(effect_observe(f, actor, &observer));
        const size_t count = ceilings[row] < 20 ? ceilings[row] : 20;
        const mc_living_spawn_effect_limits limits = {ceilings[row]};
        mc_living_spawn_effect_state state = {0};
        mc_living_spawn_effect_error error = {0};
        const mc_world_storage_status status =
            mc_living_spawn_effect(actor, f->registry, &limits, &state, &error);
        CHECK(status == (count == 20 ? MC_WORLD_STORAGE_OK : MC_WORLD_STORAGE_LIMIT) &&
              state.returned == (count == 20) && state.iterations_started == count &&
              state.gaussian_calls == 3 * count && state.float_calls == 3 * count &&
              state.particles_returned == count && observer.calls == count && observer.valid);
        if (count < 20)
            CHECK(error.native_policy && !error.source_exception && error.site.pc == 12);
        CHECK(mc_test_java_random_equal(actor->random, &observer.expected));
        mc_living_spawn_effect_error_retire(&error);
        CHECK(effect_unobserve(f, &observer));
        mc_entity_release(actor);
    }
    return true;
}

static bool effect_nan_profile(fixture *f) {
    /* Authored READY inputs through actual mutators. Expected raw results use
     * the already qualified two-NaN DADD operation profile, not a new original
     * LS.V/JIT observation. Distinct coordinate payloads must not win. */
    const uint64_t coordinates[3] = {UINT64_C(0x7ff8000000000011), UINT64_C(0xfff8000000000022),
                                     UINT64_C(0x7ff8000000000033)};
    const uint32_t width_bits = UINT32_C(0x7fc00023), height_bits = UINT32_C(0xffc00045);
    double position[3];
    float width, height;
    memcpy(position, coordinates, sizeof(position));
    memcpy(&width, &width_bits, sizeof(width));
    memcpy(&height, &height_bits, sizeof(height));
    for (size_t cached = 0; cached < 2; ++cached) {
        mc_entity *actor = NULL;
        CHECK(effect_actor(f, f->world, 431, cached != 0, &actor));
        CHECK(mc_entity_set_size(actor, width, height) == MC_ENTITY_OK &&
              mc_entity_set_position(actor, position[0], position[1], position[2]) ==
                  MC_ENTITY_OK &&
              actor->phase == MC_ENTITY_READY && fbits(actor->base.width) == width_bits &&
              fbits(actor->base.height) == height_bits && dbits(actor->base.x) == coordinates[0] &&
              dbits(actor->base.y) == coordinates[1] && dbits(actor->base.z) == coordinates[2]);
        effect_observer observer = {.fail_at = SIZE_MAX,
                                    .nan_profile = true,
                                    .expected_nan = {UINT64_C(0x7ff8000460000000),
                                                     UINT64_C(0xfff80008a0000000),
                                                     UINT64_C(0x7ff8000460000000)}};
        CHECK(effect_observe(f, actor, &observer));
        unsigned char source_fields[sizeof(actor->base)];
        memcpy(source_fields, &actor->base, sizeof(source_fields));
        mc_living_spawn_effect_state state = {0};
        mc_living_spawn_effect_error error = {0};
        CHECK(mc_living_spawn_effect(actor, f->registry, NULL, &state, &error) ==
                  MC_WORLD_STORAGE_OK &&
              state.returned && state.source_pc == 158 && state.gaussian_calls == 60 &&
              state.float_calls == 60 && state.particles_returned == 20 && observer.calls == 20 &&
              observer.valid);
        CHECK(mc_test_java_random_equal(actor->random, &observer.expected) &&
              memcmp(source_fields, &actor->base, sizeof(source_fields)) == 0);
        mc_living_spawn_effect_error_retire(&error);
        CHECK(effect_unobserve(f, &observer));
        mc_entity_release(actor);
    }
    return true;
}

static bool effect_prefixes(fixture *f) {
    for (size_t row = 0; row < 2; ++row) {
        mc_entity *actor = NULL;
        CHECK(effect_actor(f, NULL, 38, true, &actor));
        mc_java_random expected = *actor->random;
        effect_advance(&expected, 1, row == 0);
        mc_living_spawn_effect_state state = {0};
        mc_living_spawn_effect_error error = {0};
        CHECK(mc_living_spawn_effect(actor, row == 0 ? f->registry : NULL, NULL, &state, &error) ==
                  (row == 0 ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                            : MC_WORLD_STORAGE_INVALID_ARGUMENT) &&
              !state.returned && state.gaussian_calls == 3 &&
              state.float_calls == (row == 0 ? 3u : 0u) && state.particles_returned == 0 &&
              error.site.pc == (row == 0 ? 149u : 55u) && error.source_exception == (row == 0) &&
              error.native_policy == (row != 0) &&
              mc_test_java_random_equal(actor->random, &expected));
        if (row == 0)
            CHECK(strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
                  error.particle.world_access_bytecode == 0);
        mc_living_spawn_effect_error_retire(&error);
        mc_entity_release(actor);
    }
    const mc_entity_class_literal *literal = NULL;
    mc_entity *raw = NULL;
    mc_mob_error mob;
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_PIG, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->entities, f->registry, literal, &raw, &mob) == MC_MOB_OK &&
          raw->phase == MC_ENTITY_RAW && raw->random == NULL);
    mc_living_spawn_effect_state state = {0};
    mc_living_spawn_effect_error error = {0};
    CHECK(mc_living_spawn_effect(raw, NULL, NULL, &state, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.source_exception && !error.native_policy && error.site.pc == 12 &&
          state.gaussian_calls == 0 && state.float_calls == 0 && raw->phase == MC_ENTITY_RAW);
    mc_living_spawn_effect_error_retire(&error);
    mc_entity_release(raw);
    return true;
}

static bool effect_fresh_reads(fixture *f) {
    for (size_t row = 0; row < 3; ++row) {
        mc_entity *actor = NULL;
        CHECK(effect_actor(f, f->world, 817, false, &actor));
        effect_observer observer = {.fail_at = SIZE_MAX,
                                    .mutate = row == 0,
                                    .clear_world = row == 1,
                                    .change_fp = row == 2};
        CHECK(effect_observe(f, actor, &observer));
        mc_living_spawn_effect_state state = {0};
        mc_living_spawn_effect_error error = {0};
        const unsigned int controls = _mm_getcsr();
        const mc_world_storage_status status =
            mc_living_spawn_effect(actor, f->registry, NULL, &state, &error);
        if (row == 2)
            _mm_setcsr(controls); /* Correct only the declared native FP fault for cleanup. */
        CHECK(observer.valid && observer.calls == (row == 0 ? 20u : 1u));
        if (row == 0)
            CHECK(status == MC_WORLD_STORAGE_OK && state.returned &&
                  state.particles_returned == 20);
        else if (row == 1) {
            effect_advance(&observer.expected, 1, true);
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.source_exception &&
                  error.site.pc == 149 && state.gaussian_calls == 6 && state.float_calls == 6 &&
                  state.particles_returned == 1 && actor->base.world == NULL);
        } else
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.native_policy &&
                  !error.source_exception && error.site.pc == 12 && state.gaussian_calls == 3 &&
                  state.float_calls == 3 && state.particles_returned == 1);
        CHECK(mc_test_java_random_equal(actor->random, &observer.expected));
        mc_living_spawn_effect_error_retire(&error);
        CHECK(effect_unobserve(f, &observer));
        mc_entity_release(actor);
    }
    return true;
}

static bool effect_failures(fixture *f) {
    const size_t failures[] = {0, 7, 19};
    for (size_t row = 0; row < sizeof(failures) / sizeof(failures[0]); ++row)
        for (size_t source = 0; source < 2; ++source) {
            mc_entity *actor = NULL;
            CHECK(effect_actor(f, f->world, 119, true, &actor));
            effect_observer observer = {.fail_at = failures[row], .source_failure = source != 0};
            CHECK(effect_observe(f, actor, &observer));
            const size_t bindings = f->world->persistent_bindings;
            mc_living_spawn_effect_state state = {0};
            mc_living_spawn_effect_error error = {0};
            CHECK(mc_living_spawn_effect(actor, f->registry, NULL, &state, &error) ==
                      MC_WORLD_STORAGE_IO_ERROR &&
                  !state.returned && state.iteration == failures[row] &&
                  state.gaussian_calls == 3 * (failures[row] + 1) &&
                  state.float_calls == 3 * (failures[row] + 1) &&
                  state.particles_returned == failures[row] && observer.valid &&
                  observer.calls == failures[row] + 1 && error.site.pc == 149 &&
                  error.child.pc == 43 && error.particle.world_access_bytecode == 43 &&
                  error.particle.cause.status == MC_WORLD_STORAGE_IO_ERROR &&
                  error.particle.cause.system_error == 27 &&
                  error.source_exception == (source != 0) && error.native_policy == (source == 0));
            CHECK(mc_test_java_random_equal(actor->random, &observer.expected) &&
                  actor->holds == 1 && error.world_occurrence == f->world &&
                  f->world->persistent_bindings == bindings + 1);
            if (source != 0) {
                CHECK(error.world_access_capture_attempted &&
                      error.world_access_capture_status == MC_WORLD_STORAGE_OK &&
                      error.world_access.receipt == observer.receipt &&
                      observer.receipt->holds == 2 &&
                      strcmp(error.exception_class, "java/lang/IllegalStateException") == 0);
                const mc_living_spawn_effect_state previous = state;
                const mc_java_random random = *actor->random;
                CHECK(mc_living_spawn_effect(actor, f->registry, NULL, &state, &error) ==
                          MC_WORLD_STORAGE_BUSY &&
                      effect_same_state(&state, &previous) &&
                      mc_test_java_random_equal(actor->random, &random) &&
                      error.status == MC_WORLD_STORAGE_IO_ERROR && error.site.pc == 149 &&
                      mc_world_access_receipt_retire(&observer.receipt, NULL) ==
                          MC_WORLD_STORAGE_BUSY);
            }
            mc_living_spawn_effect_error_retire(&error);
            CHECK(error.status == MC_WORLD_STORAGE_IO_ERROR && error.site.pc == 149 &&
                  error.world_access.receipt == NULL &&
                  error.particle.world_access_failure.receipt == NULL &&
                  error.world_occurrence == NULL && f->world->persistent_bindings == bindings);
            CHECK(effect_unobserve(f, &observer));
            mc_entity_release(actor);
        }
    return true;
}

static bool effect_capture_refusal(fixture *f) {
    mc_entity *actor = NULL;
    CHECK(effect_actor(f, f->world, 99, false, &actor));
    effect_observer observer = {.fail_at = 0, .source_failure = true, .capture_limit = true};
    CHECK(effect_observe(f, actor, &observer));
    mc_living_spawn_effect_state state = {0};
    mc_living_spawn_effect_error error = {0};
    CHECK(mc_living_spawn_effect(actor, f->registry, NULL, &state, &error) ==
              MC_WORLD_STORAGE_IO_ERROR &&
          error.source_exception && error.site.pc == 149 && error.child.pc == 43 &&
          error.world_access_capture_attempted &&
          error.world_access_capture_status == MC_WORLD_STORAGE_LIMIT &&
          error.world_access.receipt == NULL && observer.receipt->holds == SIZE_MAX &&
          error.particle.world_access_failure.receipt == observer.receipt && observer.calls == 1 &&
          observer.valid);
    const mc_java_random random = *actor->random;
    observer.receipt->holds = 1; /* Correct only the declared native quota fault. */
    mc_living_spawn_effect_error_retire(&error);
    CHECK(error.status == MC_WORLD_STORAGE_IO_ERROR && error.site.pc == 149 &&
          error.world_access_capture_status == MC_WORLD_STORAGE_LIMIT && observer.calls == 1 &&
          mc_test_java_random_equal(actor->random, &random));
    CHECK(effect_unobserve(f, &observer));
    mc_entity_release(actor);
    return true;
}

static bool effect_null_access(fixture *f) {
    mc_entity *actor = NULL;
    CHECK(effect_actor(f, f->world, 73, true, &actor));
    mc_java_random expected = *actor->random;
    effect_advance(&expected, 1, true);
    CHECK(mc_world_runtime_add_world_access(f->world, NULL, NULL) == MC_WORLD_STORAGE_OK);
    mc_living_spawn_effect_state state = {0};
    mc_living_spawn_effect_error error = {0};
    CHECK(mc_living_spawn_effect(actor, f->registry, NULL, &state, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.source_exception && !error.native_policy && error.site.pc == 149 &&
          error.child.pc == 43 && error.world_access_capture_attempted &&
          error.world_access_capture_status == MC_WORLD_STORAGE_OK &&
          error.world_access.source_null_receiver && error.world_access.receipt == NULL &&
          state.gaussian_calls == 3 && state.float_calls == 3 && state.particles_returned == 0 &&
          strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
          mc_test_java_random_equal(actor->random, &expected));
    mc_living_spawn_effect_error_retire(&error);
    CHECK(!error.world_access.source_null_receiver &&
          !error.particle.world_access_failure.source_null_receiver && error.source_exception &&
          strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, NULL, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    mc_entity_release(actor);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || !mc_math_trig_init(&trig))
        return 2;
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]) || !effect_success(&f) || !effect_nan_profile(&f) ||
        !effect_limits(&f) || !effect_prefixes(&f) || !effect_fresh_reads(&f) ||
        !effect_failures(&f) || !effect_capture_refusal(&f) || !effect_null_access(&f) ||
        !teardown(&f))
        return 1;
    printf("PASS: %u Living spawn effect/source-order/ownership checks\n", checks);
    return 0;
}
