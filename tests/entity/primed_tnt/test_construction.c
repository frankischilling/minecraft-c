#include "mc/entity/primed_tnt/construction.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/classes/classes_internal.h"
#include "../../../src/entity/primed_tnt/internal.h"

#include <emmintrin.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint64_t math_state(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->math->state, memory_order_relaxed);
}

/* Prepared native invariants only. Original Fire/TNT replay independently
 * checks numerical constructor fields, publication and sound. These controls
 * check distinct namespace, lifetime and failed-prefix behavior without
 * resetting a reached Entity, Math holder or class owner. */
static bool constructors(void) {
    bool result = false;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *world_only = NULL, *full = NULL, *missing = NULL, *wrong = NULL;
    mc_entity_error error = {0};
    mc_entity_snapshot base = {0};
    mc_primed_tnt_entity_snapshot own = {0};
    double discarded;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_primed_tnt_entity_allocate(classes, &world_only, &error) == MC_ENTITY_OK);
    CHECK(mc_entity_native_bytes(world_only) == sizeof(mc_primed_tnt_entity_object));
    CHECK(mc_primed_tnt_entity_get_snapshot(world_only, &own) == MC_ENTITY_OK && own.fuse == 0 &&
          own.constructor_bytecode == 0 && own.world_constructor_bytecode == 0);
    CHECK(mc_primed_tnt_entity_construct_world(world_only, NULL, &error) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(world_only, &base) == MC_ENTITY_OK &&
          base.phase == MC_ENTITY_READY && base.base.world == NULL && base.base.aF &&
          base.base.width == 0.98f && base.base.height == 0.98f && base.base.offset == 0.49f &&
          base.bounds != NULL && base.bounds->min_x == -(double)0.3f &&
          base.bounds->max_x == (double)0.3f && base.base.velocity_x == 0.0 &&
          base.base.velocity_y == 0.0 && base.base.velocity_z == 0.0);
    CHECK(mc_primed_tnt_entity_get_snapshot(world_only, &own) == MC_ENTITY_OK && own.fuse == 0 &&
          own.constructor_bytecode == 33 && own.world_constructor_bytecode == 33 &&
          !own.full_constructor &&
          atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
              MC_JAVA_RANDOM_HOLDER_COLD);
    CHECK(mc_java_math_random(profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_java_random expected = {.state = math_state(profile)};
    (void)mc_java_random_next_double(&expected);
    CHECK(mc_primed_tnt_entity_allocate(classes, &full, &error) == MC_ENTITY_OK);
    CHECK(mc_primed_tnt_entity_construct(full, NULL, 8.5, 64.5, -2.5, &trig, &error) ==
          MC_ENTITY_OK);
    CHECK(math_state(profile) == expected.state);
    CHECK(mc_entity_get_snapshot(full, &base) == MC_ENTITY_OK && base.phase == MC_ENTITY_READY &&
          base.base.x == 8.5 && base.base.y == 64.5 && base.base.z == -2.5 && base.base.aJ == 8.5 &&
          base.base.aK == 64.5 && base.base.aL == -2.5 &&
          base.base.velocity_y == 0.20000000298023224);
    CHECK(mc_primed_tnt_entity_get_snapshot(full, &own) == MC_ENTITY_OK && own.fuse == 80 &&
          own.constructor_bytecode == 98 && own.world_constructor_bytecode == 33 &&
          own.full_constructor);

    expected.state = math_state(profile);
    (void)mc_java_random_next_double(&expected);
    CHECK(mc_primed_tnt_entity_allocate(classes, &missing, &error) == MC_ENTITY_OK);
    CHECK(mc_primed_tnt_entity_construct(missing, NULL, -12.5, 7.5, 3.5, NULL, &error) ==
          MC_ENTITY_STATE_ERROR);
    CHECK(error.bytecode == 37 && math_state(profile) == expected.state);
    CHECK(mc_entity_get_snapshot(missing, &base) == MC_ENTITY_OK &&
          base.phase == MC_ENTITY_FAILED_CONSTRUCTION && base.base_bytecode == 231 &&
          base.base.x == -12.5 && base.base.y == 7.5 && base.base.z == 3.5 && base.base.aF &&
          base.base.width == 0.98f && base.base.offset == 0.49f && base.base.velocity_x == 0.0 &&
          base.base.velocity_y == 0.0 && base.base.velocity_z == 0.0 && base.base.aJ == 0.0 &&
          base.base.aK == 0.0 && base.base.aL == 0.0);
    CHECK(mc_primed_tnt_entity_get_snapshot(missing, &own) == MC_ENTITY_OK && own.fuse == 0 &&
          own.constructor_bytecode == 37 && own.world_constructor_bytecode == 33 &&
          own.full_constructor);
    CHECK(mc_primed_tnt_entity_construct(missing, NULL, 0.0, 0.0, 0.0, &trig, &error) ==
              MC_ENTITY_INVALID_PHASE &&
          math_state(profile) == expected.state);
    CHECK(mc_item_entity_allocate(classes, &wrong, &error) == MC_ENTITY_OK);
    const mc_primed_tnt_entity_snapshot sentinel = {.fuse = 71,
                                                    .constructor_bytecode = 72,
                                                    .world_constructor_bytecode = 73,
                                                    .full_constructor = true};
    own = sentinel;
    CHECK(mc_primed_tnt_entity_get_snapshot(wrong, &own) == MC_ENTITY_INVALID_ARGUMENT &&
          own.fuse == sentinel.fuse && own.constructor_bytecode == sentinel.constructor_bytecode &&
          own.world_constructor_bytecode == sentinel.world_constructor_bytecode &&
          own.full_constructor == sentinel.full_constructor);
    CHECK(mc_primed_tnt_entity_construct_world(wrong, NULL, &error) == MC_ENTITY_INVALID_ARGUMENT &&
          mc_entity_get_snapshot(wrong, &base) == MC_ENTITY_OK && base.phase == MC_ENTITY_RAW);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_BUSY);
    result = true;
cleanup:
    mc_entity_release(wrong);
    mc_entity_release(missing);
    mc_entity_release(full);
    mc_entity_release(world_only);
    if (classes != NULL && mc_entity_classes_free(classes) != MC_ENTITY_OK)
        result = false;
    mc_java_random_profile_free(profile);
    return result;
}

static bool math_limit(void) {
    bool result = false;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entities[2] = {NULL, NULL};
    mc_entity_error error = {0};
    mc_entity_snapshot base = {0};
    mc_primed_tnt_entity_snapshot own = {0};
    CHECK(mc_java_random_profile_create(sizeof(*profile), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_primed_tnt_entity_allocate(classes, entities + i, &error) == MC_ENTITY_OK);
        CHECK(mc_primed_tnt_entity_construct(entities[i], NULL, 9.5, 65.5, 2.5, &trig, &error) ==
                  MC_ENTITY_PROFILE_FAILED &&
              error.bytecode == 14 &&
              error.profile ==
                  (i == 0 ? MC_JAVA_RANDOM_PROFILE_LIMIT : MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED));
        CHECK(mc_entity_get_snapshot(entities[i], &base) == MC_ENTITY_OK &&
              base.phase == MC_ENTITY_FAILED_CONSTRUCTION && base.base.id == (int32_t)i &&
              base.base.x == 9.5 && base.base.y == 65.5 && base.base.z == 2.5 &&
              base.base.width == 0.98f && base.base.offset == 0.49f &&
              base.base.velocity_y == 0.0 && base.base.aJ == 0.0 && base.base.aK == 0.0 &&
              base.base.aL == 0.0);
        CHECK(mc_primed_tnt_entity_get_snapshot(entities[i], &own) == MC_ENTITY_OK &&
              own.fuse == 0 && own.constructor_bytecode == 14 &&
              own.world_constructor_bytecode == 33 && own.full_constructor);
        CHECK(profile->math == NULL &&
              atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
                  MC_JAVA_RANDOM_HOLDER_FAILED);
    }
    result = true;
cleanup:
    mc_entity_release(entities[1]);
    mc_entity_release(entities[0]);
    if (classes != NULL && mc_entity_classes_free(classes) != MC_ENTITY_OK)
        result = false;
    mc_java_random_profile_free(profile);
    return result;
}

static bool namespace_limit(void) {
    bool result = false;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *server = NULL;
    mc_entity *raw = NULL, *candidate = NULL;
    mc_entity_limits limits = {SIZE_MAX, 1};
    mc_entity_error error = {0};
    mc_entity_class_snapshot state = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_SERVER, profile, NULL, &server) == MC_ENTITY_OK);
    CHECK(mc_primed_tnt_entity_allocate(server, &candidate, &error) == MC_ENTITY_INVALID_ARGUMENT &&
          candidate == NULL);
    CHECK(mc_entity_classes_get_snapshot(server, &state) == MC_ENTITY_OK &&
          state.entity_class == MC_ENTITY_CLASS_COLD && state.next_id == 0 &&
          state.live_entities == 0);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, &limits, &classes) == MC_ENTITY_OK);
    CHECK(mc_primed_tnt_entity_allocate(classes, &raw, &error) == MC_ENTITY_OK);
    candidate = raw;
    CHECK(mc_primed_tnt_entity_allocate(classes, &candidate, &error) == MC_ENTITY_LIMIT &&
          candidate == raw);
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 0 &&
          state.live_entities == 1 && state.entity_class == MC_ENTITY_CLASS_READY);
    result = true;
cleanup:
    mc_entity_release(raw);
    if (classes != NULL && mc_entity_classes_free(classes) != MC_ENTITY_OK)
        result = false;
    if (server != NULL && mc_entity_classes_free(server) != MC_ENTITY_OK)
        result = false;
    mc_java_random_profile_free(profile);
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fprintf(stderr, "Primed TNT contracts require strict SSE admission\n");
        return 1;
    }
    mc_math_trig_init(&trig);
    if (!constructors() || !math_limit() || !namespace_limit())
        return 1;
    printf("Primed TNT construction: %u native invariant checks passed\n", checks);
    return 0;
}
