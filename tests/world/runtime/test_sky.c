#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/world/dimension/sky.h"
#include "mc/world/runtime/sky.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "sky check%u line%d: %s\n", checks, __LINE__, #c);                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
static const uint32_t angles[] = {
    0xc0000000u, 0xbf800000u, 0xbf50c49cu, 0xbf400000u, 0xbf000000u, 0xbe800000u, 0x80000000u,
    0x00000000u, 0x3dcccccdu, 0x3e19999au, 0x3e3851ecu, 0x3e3ced91u, 0x3e3cf030u, 0x3e428f5cu,
    0x3e4ccccdu, 0x3e600000u, 0x3e75c28fu, 0x3e800000u, 0x3e851eb8u, 0x3e8f5c29u, 0x3e99999au,
    0x3e9eb852u, 0x3ea187e8u, 0x3ea18937u, 0x3ea3d70au, 0x3eb33333u, 0x3ecccccdu, 0x3f000000u,
    0x3f19999au, 0x3f266666u, 0x3f2e147bu, 0x3f2f3b64u, 0x3f2f3c0cu, 0x3f333333u, 0x3f3851ecu,
    0x3f3d70a4u, 0x3f400000u, 0x3f428f5cu, 0x3f47ae14u, 0x3f4ccccdu, 0x3f4f5c29u, 0x3f50c3f4u,
    0x3f50c49cu, 0x3f51eb85u, 0x3f59999au, 0x3f666666u, 0x3f800000u, 0x3fa00000u, 0x40000000u};
static const float partials[] = {0.0f, 0.625f, 1.0f};
static const int64_t times[] = {-24001, -1, 0, 3000, 6000, 12000, 18000, INT64_MAX};

static uint32_t bits(float value) {
    uint32_t raw;
    memcpy(&raw, &value, 4);
    return raw;
}

static float value(uint32_t raw) {
    float result;
    memcpy(&result, &raw, 4);
    return result;
}

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_math_classes *math;
    mc_world_runtime_classes *classes;
    mc_world_runtime *world;
    mc_world_info *info;
    mc_world_provider *dimensions[3];
} fixture;

static bool setup(fixture *f) {
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK);
    const int32_t ids[] = {0, -1, 1};
    for (size_t d = 0; d < 3; ++d)
        CHECK(mc_world_provider_create(ids[d], NULL, &f->dimensions[d], NULL) ==
              MC_WORLD_PROVIDER_OK);
    mc_world_provider_sky_error provider_error;
    const float *sun = (const float *)(uintptr_t)1;
    CHECK(mc_world_provider_sunrise_color(f->dimensions[2], 0.25f, 0.0f, NULL, &sun,
                                          &provider_error) == MC_WORLD_PROVIDER_OK &&
          sun == NULL);
    for (size_t d = 0; d < 3; ++d) {
        bool tint;
        float angle, height = -1.0f;
        CHECK(mc_world_provider_cloud_height(f->dimensions[d], &height, &provider_error) ==
                  MC_WORLD_PROVIDER_OK &&
              height == (d == 2 ? 8.0f : 108.0f) && provider_error.source_returned &&
              provider_error.bytecode == 2);
        CHECK(mc_world_provider_horizon_tint(f->dimensions[d], &tint, &provider_error) ==
                  MC_WORLD_PROVIDER_OK &&
              tint == (d != 2));
        CHECK(mc_world_provider_celestial_angle(f->dimensions[d], 0, 0, &angle, NULL) ==
              MC_WORLD_PROVIDER_OK);
    }
    mc_math_class_state math;
    CHECK(mc_math_classes_state(f->math, &math, NULL) == MC_MATH_CLASS_OK &&
          math.phase == MC_MATH_CLASS_COLD);
    /* Canonical World classes require the actual initialized table. This
     * native bootstrap follows the separate cold Dimension checks above. */
    CHECK(mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(f->math), &f->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes, NULL,
                                          &f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &f->info) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct(f->info, 23, mc_world_info_name_empty()) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_retain(f->info) == MC_WORLD_INFO_OK);
    f->world->fields.x = f->info;
    return true;
}

static bool select_dimension(fixture *f, size_t d) {
    CHECK(mc_world_provider_retain(f->dimensions[d]) == MC_WORLD_PROVIDER_OK);
    mc_world_provider *old = f->world->fields.t;
    f->world->fields.t = f->dimensions[d];
    mc_world_provider_release(old);
    return true;
}

static bool observations(fixture *f) {
    mc_world_provider_sky_error provider_error;
    mc_world_color_error error;
    mc_math_class_state math;
    const float *sun = (const float *)(uintptr_t)1;
    bool tint;
    CHECK(mc_world_provider_sunrise_color(f->dimensions[2], 0.25f, 0.0f, NULL, &sun,
                                          &provider_error) == MC_WORLD_PROVIDER_OK &&
          sun == NULL);
    for (size_t d = 0; d < 3; ++d)
        CHECK(mc_world_provider_horizon_tint(f->dimensions[d], &tint, &provider_error) ==
                  MC_WORLD_PROVIDER_OK &&
              tint == (d != 2));
    CHECK(mc_math_classes_state(f->math, &math, NULL) == MC_MATH_CLASS_OK &&
          math.phase == MC_MATH_CLASS_READY && math.holds == 1 &&
          math.source_a == mc_math_classes_table(f->math));
    static const float previous[] = {0, 0.2f, 1, -0.25f}, current[] = {0, 0.8f, 1, 0.75f};
    for (size_t d = 0; d < 3; ++d) {
        CHECK(select_dimension(f, d));
        for (size_t t = 0; t < 8; ++t) {
            CHECK(mc_world_info_time_set(f->info, times[t]) == MC_WORLD_INFO_OK);
            for (size_t p = 0; p < 3; ++p) {
                const size_t index = t * 3 + p;
                f->world->fields.i = previous[index % 4];
                f->world->fields.j = current[index % 4];
                float angle, rain, stars;
                CHECK(mc_world_runtime_sky_angle(f->world, partials[p], &angle, &error) ==
                          MC_WORLD_STORAGE_OK &&
                      error.site.bytecode == 15);
                if (d == 0 && index == 0)
                    CHECK(mc_math_classes_state(f->math, &math, NULL) == MC_MATH_CLASS_OK &&
                          math.phase == MC_MATH_CLASS_READY && math.holds == 1 &&
                          math.source_a == mc_math_classes_table(f->math));
                CHECK(mc_world_runtime_sky_rain_strength(f->world, partials[p], &rain, &error) ==
                          MC_WORLD_STORAGE_OK &&
                      error.site.bytecode == 16);
                CHECK(mc_world_runtime_star_brightness(f->world, partials[p], f->math, &stars,
                                                       &error) == MC_WORLD_STORAGE_OK &&
                      error.site.bytecode == 45);
                CHECK(mc_world_provider_horizon_tint(f->dimensions[d], &tint, &provider_error) ==
                      MC_WORLD_PROVIDER_OK);
                printf("SKY %zu %zu %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %d\n", d, index,
                       bits(angle), bits(rain), bits(stars), tint ? 1 : 0);
            }
        }
    }
    for (size_t d = 0; d < 3; ++d) {
        mc_world_provider_fields fields;
        CHECK(mc_world_provider_fields_get(f->dimensions[d], &fields) == MC_WORLD_PROVIDER_OK);
        const float *identity = fields.h;
        for (size_t a = 0; a < sizeof(angles) / sizeof(angles[0]); ++a)
            for (size_t p = 0; p < 2; ++p) {
                CHECK(mc_world_provider_sunrise_color(f->dimensions[d], value(angles[a]),
                                                      partials[p], f->math, &sun,
                                                      &provider_error) == MC_WORLD_PROVIDER_OK);
                CHECK(sun == NULL || sun == identity);
                CHECK(mc_world_provider_fields_get(f->dimensions[d], &fields) ==
                          MC_WORLD_PROVIDER_OK &&
                      fields.h == identity);
                printf("SUN %zu %zu %zu %d %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
                       "\n",
                       d, a, p, sun != NULL ? 1 : 0, bits(identity[0]), bits(identity[1]),
                       bits(identity[2]), bits(identity[3]));
            }
    }
    return true;
}

static bool failures(fixture *f) {
    mc_world_color_error error;
    mc_world_provider_sky_error pe;
    float out = value(0x7fc12345u);
    const float *sun = (const float *)(uintptr_t)1;
    CHECK(select_dimension(f, 0));
    mc_world_info *info = f->world->fields.x;
    mc_world_provider *dim = f->world->fields.t;
    f->world->fields.x = NULL;
    f->world->fields.t = NULL;
    mc_world_storage_status status =
        mc_world_runtime_star_brightness(f->world, 0, f->math, &out, &error);
    f->world->fields.x = info;
    f->world->fields.t = dim;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !error.native_policy &&
          error.site.bytecode == 2 && error.child.bytecode == 8 && bits(out) == 0x7fc12345u);
    f->world->fields.t = NULL;
    status = mc_world_runtime_sky_angle(f->world, 0, &out, &error);
    f->world->fields.t = dim;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !error.native_policy &&
          error.site.bytecode == 12 && bits(out) == 0x7fc12345u);
    mc_world_provider *raw = NULL;
    CHECK(mc_world_provider_allocate(MC_WORLD_PROVIDER_OVERWORLD, NULL, &raw, NULL) ==
          MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_sunrise_color(raw, 0, 0, f->math, &sun, &pe) == MC_WORLD_PROVIDER_OK &&
          sun == NULL && pe.bytecode == 140);
    sun = (const float *)(uintptr_t)1;
    CHECK(mc_world_provider_sunrise_color(raw, 0.25f, 0, f->math, &sun, &pe) ==
              MC_WORLD_PROVIDER_INVALID_ARGUMENT &&
          !pe.native_policy && pe.bytecode == 92 && pe.stores == 0 &&
          sun == (const float *)(uintptr_t)1);
    CHECK(mc_world_provider_hold_count(raw) == 1);
    mc_world_provider_release(raw);
    mc_math_classes *limited = NULL;
    CHECK(mc_math_classes_create(4096, &limited, NULL) == MC_MATH_CLASS_OK);
    mc_world_provider_fields fields;
    CHECK(mc_world_provider_fields_get(f->dimensions[0], &fields) == MC_WORLD_PROVIDER_OK);
    float old[4];
    memcpy(old, fields.h, sizeof(old));
    CHECK(mc_world_provider_sunrise_color(f->dimensions[0], 0.25f, 0, limited, &sun, &pe) ==
              MC_WORLD_PROVIDER_MATH_FAILED &&
          pe.native_policy && pe.bytecode == 9 && pe.math.source_pc == 2 &&
          sun == (const float *)(uintptr_t)1 && memcmp(old, fields.h, sizeof(old)) == 0);
    CHECK(mc_math_classes_release(&limited, NULL) == MC_MATH_CLASS_OK);
    const unsigned csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8000u);
    status = mc_world_runtime_star_brightness(f->world, 0, f->math, &out, &error);
    mc_world_provider_status ps =
        mc_world_provider_sunrise_color(f->dimensions[2], 0.25f, 0, NULL, &sun, &pe);
    bool tint = false;
    mc_world_provider_status hs = mc_world_provider_horizon_tint(f->dimensions[0], &tint, &pe);
    _mm_setcsr(csr);
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.native_policy &&
          bits(out) == 0x7fc12345u);
    CHECK(ps == MC_WORLD_PROVIDER_OK && sun == NULL && hs == MC_WORLD_PROVIDER_OK && tint);
    CHECK(f->world->active_calls == 0);
    return true;
}

static bool close_fixture(fixture *f) {
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t d = 0; d < 3; ++d)
        mc_world_provider_release(f->dimensions[d]);
    mc_world_info_release(f->info);
    CHECK(mc_world_runtime_classes_free(f->classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK);
    mc_java_random_profile_free(f->profile);
    return true;
}

int main(void) {
    fixture f = {0};
    if (!setup(&f))
        return 1;
    bool okay = observations(&f) && failures(&f);
    if (!close_fixture(&f))
        okay = false;
    if (!okay)
        return 1;
    printf("%u World sky checks passed\n", checks);
    return 0;
}
