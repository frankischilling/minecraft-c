#include "mc/core/math/classes.h"
#include "mc/resources/files/files.h"
#include "mc/world/biome/sky_color.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "world/biome/sky_color_internal.h"
#include "world/runtime/runtime_internal.h"

#include <emmintrin.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Sky leaves check%u line%d: %s\n", checks, __LINE__, #x);              \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static const int64_t seeds[] = {0, -1, INT64_MIN, INT64_MAX};
static const int32_t coordinates[][2] = {
    {0, 0}, {-17, 31}, {INT32_MIN, INT32_MAX}, {INT32_MAX, INT32_MIN}, {30000000, -30000000}};
/* Authored input bits only; every expected result is original-captured JSON. */
static const uint32_t temperatures[] = {
    0,          0x80000000, 1,          0x80000001, 0x00800000, 0x80800000, 0x3f000000, 0x3f800000,
    0xbf800000, 0x403fffff, 0x40400000, 0x40400001, 0xc03fffff, 0xc0400000, 0xc0400001, 0x7f7fffff,
    0xff7fffff, 0x7f800000, 0xff800000, 0x7fc12345, 0xffc12345, 0x7fa12345, 0xffa12345};
static const uint32_t hsb[][3] = {{0, 0, 0x3f800000},
                                  {0x7fc12345, 0, 0x3f800000},
                                  {0x7f800000, 0, 0x80000000},
                                  {0, 0x3f800000, 0x3f800000},
                                  {0x3e2aaaab, 0x3f800000, 0x3f800000},
                                  {0x3eaaaaab, 0x3f800000, 0x3f800000},
                                  {0x3f000000, 0x3f800000, 0x3f800000},
                                  {0x3f2aaaab, 0x3f800000, 0x3f800000},
                                  {0x3f555555, 0x3f800000, 0x3f800000},
                                  {0x3f800000, 0x3f800000, 0x3f800000},
                                  {0x3f000000, 0x3f000000, 0x3f000000},
                                  {0xbdcccccd, 0x3f000000, 0x3f800000},
                                  {0x3f8ccccd, 0x3f000000, 0x3f800000},
                                  {0x3e99999a, 0xbf800000, 0x40000000},
                                  {0x3e99999a, 0x40000000, 0xbf800000},
                                  {0x7fc12345, 0x3f000000, 0x3f800000},
                                  {0x3e99999a, 0x7fc12345, 0x3f800000},
                                  {0x3e99999a, 0x3f000000, 0x7fc12345},
                                  {0x7f800000, 0x3f000000, 0x3f800000},
                                  {0xff800000, 0x3f000000, 0x3f800000},
                                  {0x3e99999a, 0x7f800000, 0x3f800000},
                                  {0x3e99999a, 0x3f000000, 0x7f800000},
                                  {0x3e99999a, 0x3f000000, 0xff800000},
                                  {0, 0x3f000000, 1},
                                  {0x7f7fffff, 0x3f000000, 0x7f7fffff}};

static float float_of(uint32_t b) {
    float v;
    memcpy(&v, &b, sizeof(v));
    return v;
}

static uint64_t bits(double v) {
    uint64_t b;
    memcpy(&b, &v, sizeof(b));
    return b;
}

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_materials *materials;
    mc_resource_files *resources;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_world_runtime_classes *classes;
    mc_world_runtime *world;
    mc_world_provider_registry_binding binding;
} fixture;

static int seed_world(fixture *f, int64_t seed) {
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes, NULL,
                                          &f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &f->world->fields.x) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct(f->world->fields.x, seed, mc_world_info_name_empty()) ==
          MC_WORLD_INFO_OK);
    return 0; /* Prepared RAW World seed operand; no World constructor equivalence. */
}

static int manager(fixture *f, int32_t id, const mc_world_provider_limits *limits,
                   mc_world_provider **dimension, mc_world_chunk_manager **out) {
    CHECK(mc_world_provider_create(id, limits, dimension, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_bind_game(*dimension, id == 0 ? f->world : NULL, &f->binding, NULL) ==
          MC_WORLD_PROVIDER_OK);
    mc_world_provider_fields fields;
    CHECK(mc_world_provider_fields_get(*dimension, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.b != NULL);
    *out = fields.b;
    return 0;
}

static int raw_rows(fixture *f) {
    for (size_t s = 0; s < COUNT(seeds); ++s) {
        CHECK(seed_world(f, seeds[s]) == 0);
        for (size_t c = 0; c < COUNT(coordinates); ++c)
            for (unsigned int mode = 0; mode < 3; ++mode) {
                mc_world_provider *dimension = NULL;
                mc_world_chunk_manager *m = NULL;
                CHECK(manager(f, 0, NULL, &dimension, &m) == 0);
                mc_world_chunk_manager_fields fields;
                CHECK(mc_world_chunk_manager_fields_get(m, &fields) == MC_WORLD_PROVIDER_OK);
                mc_biome_manager *noise = fields.noise_backed;
                CHECK(noise != NULL);
                if (mode != 0) {
                    CHECK(mc_biome_double_buffer_resize(noise->memory, &noise->a,
                                                        mode == 1 ? 0 : 5) == MC_BIOME_OK);
                    for (size_t i = 0; i < noise->a.length; ++i)
                        noise->a.data[i] = -7.5;
                }
                const void *old = noise->a.data;
                CHECK(mc_biome_double_buffer_resize(noise->memory, &noise->b, 2) == MC_BIOME_OK);
                CHECK(mc_biome_double_buffer_resize(noise->memory, &noise->c, 1) == MC_BIOME_OK);
                noise->b.data[0] = 9.25;
                noise->b.data[1] = 10.25;
                noise->c.data[0] = 11.5;
                const void *b = noise->b.data, *aux = noise->c.data;
                double answer = -99;
                mc_world_chunk_manager_error error;
                CHECK(mc_world_chunk_manager_temperature_raw(m, coordinates[c][0],
                                                             coordinates[c][1], &answer,
                                                             &error) == MC_WORLD_PROVIDER_OK &&
                      error.bytecode == 36);
                CHECK(noise->b.data == b && noise->c.data == aux && noise->d.data == NULL &&
                      noise->b.data[0] == 9.25 && noise->b.data[1] == 10.25 &&
                      noise->c.data[0] == 11.5);
                printf("RAW %zu %zu %u %016" PRIx64 " %d %zu", s, c, mode, bits(answer),
                       noise->a.data == old, noise->a.length);
                for (size_t i = 0; i < noise->a.length; ++i)
                    printf(" %016" PRIx64, bits(noise->a.data[i]));
                putchar('\n');
                mc_world_provider_unbind(dimension);
                mc_world_provider_release(dimension);
            }
        CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
        f->world = NULL;
    }
    return 0;
}

static int color_rows(fixture *f) {
    for (unsigned int b = 0; b < 13; ++b) {
        const mc_biome *biome = NULL;
        CHECK(mc_biome_static_get(f->registry, (char)('a' + b), &biome, NULL) == MC_REGISTRY_OK &&
              biome != NULL);
        for (size_t t = 0; t < COUNT(temperatures); ++t) {
            int32_t answer = 0;
            mc_biome_sky_color_error error;
            const bool fixed = mc_biome_kind_get(biome) == MC_BIOME_KIND_RY_LE;
            CHECK(mc_world_biome_sky_color(biome, float_of(temperatures[t]), &answer, &error) ==
                      MC_REGISTRY_OK &&
                  error.source_returned && error.bytecode == (fixed ? 2u : 44u) &&
                  error.color_allocated == !fixed && error.color_constructed == !fixed);
            printf("SKY %u %zu %08" PRIx32 " %08" PRIx32 "\n", b, t, temperatures[t],
                   (uint32_t)answer);
        }
    }
    for (size_t i = 0; i < COUNT(hsb); ++i) {
        int32_t answer = 0;
        CHECK(mc_java_color_hsb_to_rgb(float_of(hsb[i][0]), float_of(hsb[i][1]),
                                       float_of(hsb[i][2]), &answer));
        printf("HSB %zu %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 "\n", i, hsb[i][0],
               hsb[i][1], hsb[i][2], (uint32_t)answer);
    }
    for (unsigned int fidx = 0; fidx < 2; ++fidx) {
        mc_world_provider *dimension = NULL;
        mc_world_chunk_manager *m = NULL;
        CHECK(manager(f, fidx == 0 ? -1 : 1, NULL, &dimension, &m) == 0);
        mc_world_chunk_manager_fields fields;
        CHECK(mc_world_chunk_manager_fields_get(m, &fields) == MC_WORLD_PROVIDER_OK &&
              fields.fixed);
        /* Materialize the actual fixed manager a through its genuine rectangle
         * body, then prepare only its permitted elements for the scalar control. */
        mc_biome_reference_array *sampled = NULL;
        CHECK(mc_world_chunk_manager_sample(m, 0, 0, 2, 1, NULL, &sampled, NULL) ==
              MC_WORLD_PROVIDER_OK);
        CHECK(mc_world_chunk_manager_fields_get(m, &fields) == MC_WORLD_PROVIDER_OK &&
              fields.a->data != NULL && fields.a->length == 2);
        fields.a->data[0] = 4;
        fields.a->data[1] = 5;
        const void *old = fields.a->data;
        double answer = -99;
        mc_world_chunk_manager_error error;
        CHECK(mc_world_chunk_manager_temperature_raw(m, 7, -9, &answer, &error) ==
                  MC_WORLD_PROVIDER_OK &&
              error.bytecode == 4 && fields.a->data == old && fields.a->data[0] == 4 &&
              fields.a->data[1] == 5);
        printf("RAWFIX %u %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", fidx, bits(answer),
               bits(fields.a->data[0]), bits(fields.a->data[1]));
        mc_world_provider_release(dimension);
    }
    return 0;
}

static int controls(fixture *f) {
    CHECK(seed_world(f, 0) == 0);
    mc_world_provider *dimension = NULL;
    mc_world_chunk_manager *m = NULL;
    CHECK(manager(f, 0, NULL, &dimension, &m) == 0);
    mc_world_chunk_manager_fields fields;
    CHECK(mc_world_chunk_manager_fields_get(m, &fields) == MC_WORLD_PROVIDER_OK);
    mc_biome_manager *noise = fields.noise_backed;
    double answer = -99;
    mc_world_chunk_manager_error error;
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr((saved & ~0x6000u) | 0x2000u);
    mc_world_provider_status status =
        mc_world_chunk_manager_temperature_raw(m, 0, 0, &answer, &error);
    _mm_setcsr(saved);
    CHECK(status == MC_WORLD_PROVIDER_UNSUPPORTED_FP && answer == -99 && noise->a.data == NULL &&
          noise->memory->live_allocation_bytes == 0);
    /* The actual raw body must not preflight unrelated future b/c/d descriptors. */
    noise->b.length = SIZE_MAX;
    status = mc_world_chunk_manager_temperature_raw(m, 0, 0, &answer, &error);
    noise->b.length = 0;
    CHECK(status == MC_WORLD_PROVIDER_OK && noise->a.data != NULL);
    CHECK(mc_world_chunk_manager_retain(m) == MC_WORLD_PROVIDER_OK);
    mc_world_provider_unbind(dimension);
    mc_world_provider_release(dimension);
    CHECK(mc_world_chunk_manager_temperature_raw(m, -7, 13, &answer, &error) ==
          MC_WORLD_PROVIDER_OK);
    mc_world_chunk_manager_release(m);
    mc_world_provider_limits limits = mc_world_provider_default_limits();
    limits.max_biome_buffer_bytes = 0;
    dimension = NULL;
    m = NULL;
    CHECK(manager(f, 0, &limits, &dimension, &m) == 0);
    answer = -99;
    CHECK(mc_world_chunk_manager_temperature_raw(m, 0, 0, &answer, &error) ==
              MC_WORLD_CHUNK_MANAGER_FAILED &&
          error.sampling.climate_status == MC_BIOME_LIMIT && answer == -99);
    mc_world_provider_unbind(dimension);
    mc_world_provider_release(dimension);
    const mc_biome *biome = NULL;
    CHECK(mc_biome_static_get(f->registry, 'a', &biome, NULL) == MC_REGISTRY_OK);
    int32_t rgb = 123;
    mc_biome_sky_color_error color;
    CHECK(mc_world_biome_sky_color(NULL, 0, &rgb, &color) == MC_REGISTRY_NULL_REFERENCE &&
          !color.native_policy && rgb == 123 && !color.color_allocated);
    _mm_setcsr((saved & ~0x6000u) | 0x2000u);
    mc_registry_status color_status = mc_world_biome_sky_color(biome, 0, &rgb, &color);
    _mm_setcsr(saved);
    CHECK(color_status == MC_REGISTRY_INVALID_ARGUMENT && color.native_policy &&
          color.bytecode == 3 && !color.color_allocated && rgb == 123);
    CHECK(mc_biome_static_get(f->registry, 'm', &biome, NULL) == MC_REGISTRY_OK);
    _mm_setcsr((saved & ~0x6000u) | 0x2000u);
    color_status = mc_world_biome_sky_color(biome, float_of(UINT32_C(0x7fa12345)), &rgb, &color);
    _mm_setcsr(saved);
    CHECK(color_status == MC_REGISTRY_OK && rgb == INT32_C(12632319) && color.bytecode == 2 &&
          !color.color_allocated && color.source_returned);
    dimension = NULL;
    m = NULL;
    CHECK(manager(f, 1, NULL, &dimension, &m) == 0);
    _mm_setcsr((saved & ~0x6000u) | 0x2000u);
    status = mc_world_chunk_manager_temperature_raw(m, 0, 0, &answer, &error);
    _mm_setcsr(saved);
    CHECK(status == MC_WORLD_PROVIDER_OK && bits(answer) == UINT64_C(0x3fe0000000000000));
    mc_world_provider_release(dimension);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    f->world = NULL;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    fixture f = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&f.materials));
    CHECK(mc_resource_files_create(argv[1], &f.resources) == MC_REGISTRY_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f.profile,
                                         .materials = f.materials,
                                         .resources = mc_resource_files_source(f.resources)};
    CHECK(mc_registry_classes_allocate(&options, &f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &f.math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f.math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f.profile,
                                          mc_math_classes_table(f.math), &f.classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f.classes, f.registry, NULL) == MC_REGISTRY_OK);
    f.binding = (mc_world_provider_registry_binding){f.registry, MC_REGISTRY_CLIENT, f.profile};
    CHECK(raw_rows(&f) == 0);
    CHECK(fflush(stdout) == 0); /* Finish rows before the real bootstrap's native banners. */
    CHECK(color_rows(&f) == 0 && controls(&f) == 0);
    CHECK(mc_world_runtime_classes_free(f.classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&f.math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_registry_classes_free(f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_resource_files_free(f.resources) == MC_REGISTRY_OK);
    mc_materials_destroy(f.materials);
    mc_java_random_profile_free(f.profile);
    printf("%u sky leaf checks passed\n", checks);
    return 0;
}
