#include "mc/entity/render_distance.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/entity_internal.h"

#include <emmintrin.h>
#include <stdio.h>
#include <string.h>

/* Native contracts only. Every Entity/BT is genuinely allocated/constructed.
 * aE below is an explicitly authored source scalar input on that same Item;
 * no native phase/hold/class/pool/Random field is assigned. */
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static double from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool distance_case(mc_entity *entity, const mc_entity_vector *eye,
                          mc_entity_render_distance_status expected, bool value, int32_t pc) {
    bool ok = false;
    mc_entity before = {0};
    mc_entity_classes classes_before = {0};
    mc_entity_bounds bounds_before = {0};
    mc_entity_vector eye_before = {0};
    mc_java_random random_before = {0};
    uint64_t unique_before = 0, math_before = 0;
    if (entity != NULL) {
        memcpy(&before, entity, sizeof(before));
        memcpy(&classes_before, entity->classes, sizeof(classes_before));
        if (entity->bounds != NULL)
            bounds_before = *entity->bounds;
        if (entity->random != NULL)
            random_before = *entity->random;
        unique_before =
            atomic_load_explicit(&entity->classes->profile->uniquifier, memory_order_relaxed);
        if (entity->classes->profile->math != NULL)
            math_before =
                atomic_load_explicit(&entity->classes->profile->math->state, memory_order_relaxed);
    }
    if (eye != NULL)
        eye_before = *eye;
    bool out = true;
    mc_entity_render_distance_error error;
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_entity_render_distance(entity, eye, &out, &error) == expected);
    CHECK(error.status == expected && error.pc == pc);
    CHECK(out == (expected == MC_ENTITY_RENDER_DISTANCE_OK ? value : true));
    if (expected == MC_ENTITY_RENDER_DISTANCE_OK) {
        CHECK(error.operation == NULL && error.source_class == NULL && error.vector_pc == -1 &&
              error.range_pc == -1 && error.bounds_pc == -1);
    } else {
        CHECK(error.operation != NULL);
    }
    if (entity != NULL) {
        CHECK(memcmp(entity, &before, sizeof(before)) == 0 &&
              memcmp(entity->classes, &classes_before, sizeof(classes_before)) == 0);
        CHECK(entity->bounds == NULL ||
              memcmp(entity->bounds, &bounds_before, sizeof(bounds_before)) == 0);
        CHECK(entity->random == NULL ||
              memcmp(entity->random, &random_before, sizeof(random_before)) == 0);
        CHECK(atomic_load_explicit(&entity->classes->profile->uniquifier, memory_order_relaxed) ==
              unique_before);
        CHECK(entity->classes->profile->math == NULL ||
              atomic_load_explicit(&entity->classes->profile->math->state, memory_order_relaxed) ==
                  math_before);
    }
    CHECK(eye == NULL || memcmp(eye, &eye_before, sizeof(eye_before)) == 0);
    ok = true;
cleanup:
    return ok;
}

int main(void) {
    int result = 1;
    const unsigned int original_csr = _mm_getcsr();
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *item = NULL, *raw = NULL;
    mc_entity_vector *eyes[9] = {0};
    const double nan = from_bits(UINT64_C(0x7ff8000000000042));
    const double infinity = from_bits(UINT64_C(0x7ff0000000000000));
    const double inputs[9][3] = {
        {0.0, 0.0, 0.0},
        {0x1.fffffffffffffp3, 0.0, 0.0}, /* Adjacent binary64 below exact range16. */
        {16.0, 0.0, 0.0},
        {0x1.0000000000001p4, 0.0, 0.0},
        {-16.0, 0.0, 0.0},
        {nan, 0.0, 0.0},
        {0.0, nan, 0.0},
        {0.0, 0.0, nan},
        {infinity, 0.0, 0.0}};
    CHECK((original_csr & 0xffc0u) == 0x1f80u);
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(classes, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, NULL, 0.0, 0.0, 0.0, NULL, NULL) == MC_ENTITY_OK);
    CHECK(item->phase == MC_ENTITY_READY && item->bounds != NULL &&
          item->bounds->max_x - item->bounds->min_x == 0.25 &&
          item->bounds->max_y - item->bounds->min_y == 0.25 &&
          item->bounds->max_z - item->bounds->min_z == 0.25 && item->base.aE == 1.0);
    CHECK(mc_item_entity_allocate(classes, &raw, NULL) == MC_ENTITY_OK &&
          raw->phase == MC_ENTITY_RAW);
    for (size_t i = 0; i < sizeof(eyes) / sizeof(eyes[0]); ++i)
        CHECK(mc_entity_vector_create(classes, inputs[i][0], inputs[i][1], inputs[i][2], &eyes[i],
                                      NULL) == MC_ENTITY_OK);
    CHECK(distance_case(item, eyes[0], MC_ENTITY_RENDER_DISTANCE_OK, true, -1));
    CHECK(distance_case(item, eyes[1], MC_ENTITY_RENDER_DISTANCE_OK, true, -1));
    for (size_t i = 2; i < sizeof(eyes) / sizeof(eyes[0]); ++i)
        CHECK(distance_case(item, eyes[i], MC_ENTITY_RENDER_DISTANCE_OK, false, -1));
    CHECK(distance_case(item, NULL, MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL, true, 5));
    CHECK(distance_case(NULL, eyes[0], MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL, true, 1));
    CHECK(distance_case(raw, eyes[0], MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL, true, 4));
    mc_entity_render_distance_error error;
    bool out = true;
    CHECK(mc_entity_render_distance(raw, eyes[0], &out, &error) ==
              MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL &&
          out && error.vector_pc == 52 && error.range_pc == 4 && !error.native_policy &&
          strcmp(error.source_class, "sn") == 0 && strcmp(error.source_descriptor, "(D)Z") == 0);
    CHECK(mc_entity_render_distance(item, eyes[2], &out, NULL) == MC_ENTITY_RENDER_DISTANCE_OK &&
          !out);
    CHECK(mc_entity_render_distance(item, NULL, NULL, &error) ==
              MC_ENTITY_RENDER_DISTANCE_INVALID_ARGUMENT &&
          error.native_policy);
    out = true;
    CHECK(mc_entity_render_distance(item, NULL, &out, NULL) ==
              MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL &&
          out);

    /* Declared source scalar controls; compare passivity after each input. */
    item->base.aE = 0.0;
    CHECK(distance_case(item, eyes[0], MC_ENTITY_RENDER_DISTANCE_OK, false, -1));
    item->base.aE = -1.0;
    CHECK(distance_case(item, eyes[1], MC_ENTITY_RENDER_DISTANCE_OK, true, -1));
    item->base.aE = nan;
    CHECK(distance_case(item, eyes[0], MC_ENTITY_RENDER_DISTANCE_OK, false, -1));
    item->base.aE = infinity;
    CHECK(distance_case(item, eyes[2], MC_ENTITY_RENDER_DISTANCE_OK, true, -1));
    item->base.aE = 1.0;

    /* Restore controls BEFORE any assertion can take a cleanup branch. */
    _mm_setcsr(original_csr | 0x8000u);
    out = true;
    const mc_entity_render_distance_status fp_status =
        mc_entity_render_distance(item, eyes[0], &out, &error);
    const bool fp_out = out;
    const mc_entity_render_distance_error fp_error = error;
    const unsigned int retained_controls = _mm_getcsr() & 0xffc0u;
    const mc_entity_render_distance_status null_status =
        mc_entity_render_distance(item, NULL, &out, &error);
    const mc_entity_render_distance_error null_error = error;
    const mc_entity_render_distance_status raw_status =
        mc_entity_render_distance(raw, eyes[0], &out, &error);
    const mc_entity_render_distance_error raw_error = error;
    _mm_setcsr(original_csr);
    CHECK(fp_status == MC_ENTITY_RENDER_DISTANCE_UNSUPPORTED_FP && fp_out && fp_error.pc == 8 &&
          fp_error.vector_pc == 8 && fp_error.native_policy &&
          retained_controls == ((original_csr | 0x8000u) & 0xffc0u));
    CHECK(null_status == MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL && null_error.pc == 5 &&
          !null_error.native_policy && out);
    CHECK(raw_status == MC_ENTITY_RENDER_DISTANCE_UNSUPPORTED_FP && raw_error.pc == 8 &&
          raw_error.native_policy);
    result = 0;
cleanup:
    _mm_setcsr(original_csr);
    for (size_t i = 0; i < sizeof(eyes) / sizeof(eyes[0]); ++i)
        if (eyes[i] != NULL && mc_entity_vector_release(classes, eyes[i]) != MC_ENTITY_OK)
            result = 1;
    mc_entity_release(raw);
    mc_entity_release(item);
    if (mc_entity_classes_free(classes) != MC_ENTITY_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    if (result == 0)
        printf("Entity render distance: %u native checks passed\n", checks);
    return result;
}
