#include "turn_fixture.h"

#include <emmintrin.h>
#include <math.h>
#include <stdio.h>

static unsigned int checks;
#define CHECK(test)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test);                             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int exit_code = 1;
    const unsigned int original_csr = _mm_getcsr();
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL, *raw = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_turn(NULL, 0.0f, 0.0f) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_item_entity_allocate(classes, &raw, NULL) == MC_ENTITY_OK);
    unsigned char raw_before[sizeof(mc_entity)];
    memcpy(raw_before, raw, sizeof(*raw));
    CHECK(mc_entity_turn(raw, 1.0f, 1.0f) == MC_ENTITY_INVALID_PHASE);
    CHECK(memcmp(raw_before, raw, sizeof(*raw)) == 0);
    mc_entity_release(raw);
    raw = NULL;

    for (unsigned int kind = 0; kind < 2; ++kind) {
        CHECK((kind == 0 ? mc_item_entity_allocate(classes, &entity, NULL)
                         : mc_falling_entity_allocate(classes, &entity, NULL)) == MC_ENTITY_OK);
        CHECK((kind == 0 ? mc_item_entity_construct_world(entity, NULL, NULL)
                         : mc_falling_entity_construct_world(entity, NULL, NULL)) == MC_ENTITY_OK);
        CHECK(entity->phase == MC_ENTITY_READY && entity->base.world == NULL);
        turn_guard guard;
        const uint32_t zero[4] = {0};
        turn_prepare(entity, zero);
        CHECK(turn_capture(entity, &guard));
        CHECK(mc_entity_turn(entity, 3.0f, 3.0f) == MC_ENTITY_OK);
        CHECK(turn_bits(entity->base.yaw) == UINT32_C(0x3ee66666));
        CHECK(turn_bits(entity->base.aT) == UINT32_C(0xbee66666));
        CHECK(turn_bits(entity->base.aU) == UINT32_C(0x3ee66666));
        CHECK(turn_bits(entity->base.aV) == UINT32_C(0xbee66666));
        CHECK(turn_unchanged_other_fields(entity, &guard));

        const uint32_t clamp[4] = {UINT32_C(0x44340000), UINT32_C(0x42b20000), UINT32_C(0x41f00000),
                                   UINT32_C(0xc1a00000)};
        turn_prepare(entity, clamp);
        CHECK(turn_capture(entity, &guard));
        CHECK(mc_entity_turn(entity, 10.0f, -10.0f) == MC_ENTITY_OK);
        CHECK(turn_bits(entity->base.yaw) == turn_bits(721.5f));
        CHECK(turn_bits(entity->base.aT) == turn_bits(90.0f));
        CHECK(turn_bits(entity->base.aU) == turn_bits(31.5f));
        CHECK(turn_bits(entity->base.aV) == turn_bits(-19.0f));
        CHECK(turn_unchanged_other_fields(entity, &guard));

        const uint32_t rounding[4] = {UINT32_C(0x4b800000), 0, UINT32_C(0x40a00000), 0};
        turn_prepare(entity, rounding);
        CHECK(mc_entity_turn(entity, 10.0f, 0.0f) == MC_ENTITY_OK);
        CHECK(turn_bits(entity->base.yaw) == UINT32_C(0x4b800001));
        CHECK(turn_bits(entity->base.aU) == turn_bits(7.0f));

        const uint32_t overflow[4] = {UINT32_C(0x7f7fffff), UINT32_C(0x7f7fffff), 0,
                                      UINT32_C(0xff7fffff)};
        turn_prepare(entity, overflow);
        CHECK(mc_entity_turn(entity, turn_float(UINT32_C(0x7f7fffff)), 0.0f) == MC_ENTITY_OK);
        CHECK(turn_bits(entity->base.yaw) == UINT32_C(0x7f800000));
        CHECK(turn_bits(entity->base.aT) == turn_bits(90.0f));
        CHECK(turn_bits(entity->base.aU) == UINT32_C(0x7f800000));
        CHECK(turn_bits(entity->base.aV) == UINT32_C(0xff800000));

        for (unsigned int sign = 0; sign < 2; ++sign) {
            turn_prepare(entity, zero);
            const float delta = turn_float(UINT32_C(0x7f800000) | (sign << 31));
            CHECK(mc_entity_turn(entity, delta, delta) == MC_ENTITY_OK);
            CHECK(turn_bits(entity->base.yaw) == turn_bits(delta));
            CHECK(turn_bits(entity->base.aU) == turn_bits(delta));
            CHECK(entity->base.aT == (sign == 0 ? -90.0f : 90.0f));
            CHECK(entity->base.aV == entity->base.aT);
        }
        const uint32_t infinity[4] = {UINT32_C(0x7f800000), UINT32_C(0x7f800000), 0, 0};
        turn_prepare(entity, infinity);
        CHECK(mc_entity_turn(entity, 0.0f, 0.0f) == MC_ENTITY_OK);
        CHECK(turn_bits(entity->base.yaw) == UINT32_C(0x7f800000));
        CHECK(entity->base.aT == 90.0f && isnan(entity->base.aU));
        CHECK(turn_bits(entity->base.aV) == UINT32_C(0xff800000));

        const uint32_t nan[4] = {0, UINT32_C(0x7fc12345), 0, 0};
        turn_prepare(entity, nan);
        CHECK(mc_entity_turn(entity, 0.0f, 0.0f) == MC_ENTITY_OK);
        CHECK(isnan(entity->base.aT) && isnan(entity->base.aV));
        CHECK(turn_bits(entity->base.yaw) == 0 && turn_bits(entity->base.aU) == 0);

        const uint32_t tiny[4] = {1, 1, 0, 0};
        turn_prepare(entity, tiny);
        CHECK(mc_entity_turn(entity, 0.0f, 0.0f) == MC_ENTITY_OK);
        CHECK(turn_bits(entity->base.yaw) == 1 && turn_bits(entity->base.aT) == 1);
        CHECK(turn_bits(entity->base.aU) == 0 && turn_bits(entity->base.aV) == 0);

        const uint32_t negative_zero[4] = {UINT32_C(0x80000000), UINT32_C(0x80000000), 0, 0};
        turn_prepare(entity, negative_zero);
        CHECK(mc_entity_turn(entity, turn_float(UINT32_C(0x80000000)), 0.0f) == MC_ENTITY_OK);
        CHECK(turn_bits(entity->base.yaw) == UINT32_C(0x80000000));
        CHECK(turn_bits(entity->base.aT) == UINT32_C(0x80000000));
        CHECK(turn_bits(entity->base.aU) == 0 && turn_bits(entity->base.aV) == 0);

        const unsigned int unsupported[] = {0x2000u, 0x4000u, 0x8000u, 0x0040u};
        for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
            CHECK(turn_capture(entity, &guard));
            const uint32_t angles[] = {turn_bits(entity->base.yaw), turn_bits(entity->base.aT),
                                       turn_bits(entity->base.aU), turn_bits(entity->base.aV)};
            _mm_setcsr(original_csr | unsupported[i]);
            const mc_entity_status status = mc_entity_turn(entity, 1.0f, 1.0f);
            _mm_setcsr(original_csr);
            CHECK(status == MC_ENTITY_UNSUPPORTED_FP);
            CHECK(turn_unchanged_other_fields(entity, &guard));
            CHECK(turn_bits(entity->base.yaw) == angles[0] &&
                  turn_bits(entity->base.aT) == angles[1] &&
                  turn_bits(entity->base.aU) == angles[2] &&
                  turn_bits(entity->base.aV) == angles[3]);
        }
        mc_entity_release(entity);
        entity = NULL;
    }
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    classes = NULL;
    printf("Entity turn: %u checks passed\n", checks);
    exit_code = 0;
cleanup:
    _mm_setcsr(original_csr);
    mc_entity_release(raw);
    mc_entity_release(entity);
    (void)mc_entity_classes_free(classes);
    mc_java_random_profile_free(profile);
    return exit_code;
}
