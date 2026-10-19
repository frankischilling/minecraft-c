#include "mc/world/runtime/environment.h"

#include "mc/world/dimension/dimension.h"
#include "runtime_internal.h"

#include <math.h>
#include <stdio.h>

#include <emmintrin.h>

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

/* Native state/arithmetic contracts. Actual construction reaches its null
 * storage failure after Info and Dimension binding. No READY World is forged;
 * private field edits exercise these helpers on that retained failed owner.
 * Constants below are dyadic/cardinal cases, not original observation goldens. */
int main(void)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL;
    mc_world_dimension *dimension = NULL;
    mc_world_storage_error error;
    float strength = 99.0f;
    int32_t light = -123;
    unsigned int controls = _mm_getcsr();
    CHECK(mc_math_trig_init(&trig));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &classes, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, classes, NULL,
                                          &world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_celestial_angle(world, 1.0f, &strength, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          strength == 99.0f);
    CHECK(mc_world_dimension_create(1, NULL, &dimension, NULL) == MC_WORLD_DIMENSION_OK);
    mc_world_runtime_world_arguments arguments = {.dimension = dimension, .seed = 17};
    CHECK(mc_world_runtime_construct_world(world, &arguments, NULL) != MC_WORLD_STORAGE_OK);
    CHECK(world->construction.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION);
    CHECK(world->fields.x != NULL && mc_world_dimension_world(dimension) == world);
    CHECK(mc_world_info_time_set(world->fields.x, INT64_MIN) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_celestial_angle(world, NAN, &strength, NULL) == MC_WORLD_STORAGE_OK &&
          strength == 0.0f && !signbit(strength));
    CHECK(mc_world_runtime_sky_subtraction(world, 1.0f, &light, NULL) == MC_WORLD_STORAGE_OK &&
          light == 0);
    world->fields.i = 0.25f;
    world->fields.j = 0.75f;
    world->fields.k = 0.125f;
    world->fields.l = 0.625f;
    CHECK(mc_world_runtime_rain_strength(world, 0.5f, &strength, NULL) == MC_WORLD_STORAGE_OK &&
          strength == 0.5f);
    CHECK(mc_world_runtime_thunder_strength(world, 0.5f, &strength, NULL) == MC_WORLD_STORAGE_OK &&
          strength == 0.1875f);
    world->fields.i = world->fields.k = 0.0f;
    world->fields.j = 1.0f;
    world->fields.l = 0.0f;
    CHECK(mc_world_runtime_sky_subtraction(world, 1.0f, &light, NULL) == MC_WORLD_STORAGE_OK &&
          light == 3);
    world->fields.l = 1.0f;
    CHECK(mc_world_runtime_sky_subtraction(world, 1.0f, &light, NULL) == MC_WORLD_STORAGE_OK &&
          light == 5);
    world->fields.j = 0.0f;
    CHECK(mc_world_runtime_sky_subtraction(world, NAN, &light, NULL) == MC_WORLD_STORAGE_OK &&
          light == 0);
    CHECK(mc_world_runtime_rain_strength(world, NAN, &strength, NULL) == MC_WORLD_STORAGE_OK &&
          isnan(strength));
    CHECK(mc_world_info_raining_set(world->fields.x, true) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_thundering_set(world->fields.x, true) == MC_WORLD_INFO_OK);
    world->fields.i = world->fields.j = world->fields.k = world->fields.l = 0.0f;
    world->fields.f = 99;
    CHECK(mc_world_runtime_initialize_environment(world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(world->fields.f == 0 && world->fields.i == 0 && world->fields.j == 1 &&
          world->fields.k == 0 && world->fields.l == 1);
    CHECK(mc_world_runtime_sky_subtraction(world, 1.0f, &light, NULL) == MC_WORLD_STORAGE_OK &&
          light == 5);
    CHECK(mc_world_info_raining_set(world->fields.x, false) == MC_WORLD_INFO_OK);
    world->fields.j = 0.75f;
    world->fields.l = 0.625f;
    CHECK(mc_world_runtime_initialize_environment(world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(world->fields.j == 0.75f && world->fields.l == 0.625f);
    CHECK(mc_world_info_raining_set(world->fields.x, true) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_thundering_set(world->fields.x, false) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_initialize_environment(world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(world->fields.j == 1.0f && world->fields.l == 0.625f);
    strength = 77.0f;
    _mm_setcsr(controls | 0x8000u);
    mc_world_storage_status status = mc_world_runtime_rain_strength(world, 1.0f, &strength, &error);
    _mm_setcsr(controls);
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && strength == 77.0f);
    CHECK(mc_world_runtime_retire_world(world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_dimension_world(dimension) == NULL);
    CHECK(mc_world_runtime_rain_strength(world, 0.5f, &strength, NULL) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          strength == 77.0f);
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    world = NULL;
    result = 0;
cleanup:
    _mm_setcsr(controls);
    if (world != NULL) {
        (void)mc_world_runtime_retire_world(world, NULL);
        (void)mc_world_runtime_free(world, NULL);
    }
    mc_world_dimension_release(dimension);
    if (mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    if (result == 0)
        printf("World environment native contracts: %u checks passed\n", checks);
    return result;
}
