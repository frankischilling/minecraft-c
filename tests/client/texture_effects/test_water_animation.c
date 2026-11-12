/* Authored finite observer over the actual owner/layout, not a replay owner.
 * Including the production body permits private fixture field setup only.
 * Its genuine animation and shared mc_java_math_random remain unchanged. */
#include "../../../src/client/texture_effects/water.c"
#include "../../../src/core/java/random_profile_internal.h"
#include "mc/resources/files/files.h"
#include <emmintrin.h>
#include <stdio.h>
#include <string.h>

static size_t checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Water animation contract:%d: %s\n", __LINE__, #x);                    \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct test_case {
    const char *name;
    int pattern, frames;
    bool color;
    int32_t counter;
    const char *fault;
    int pc;
} test_case;

static const test_case cases[] = {
    {"flat", 0, 2, false, 0, "", 0},         {"flat-color", 0, 1, true, 0, "", 0},
    {"waves", 1, 5, false, 7, "", 0},        {"waves-color", 1, 3, true, -2, "", 0},
    {"wrap", 1, 1, false, INT32_MAX, "", 0}, {"subnormal", 2, 2, false, 0, "", 0},
    {"nan", 3, 1, false, 0, "", 0},          {"nan-color", 3, 1, true, 0, "", 0},
    {"null-g", 0, 1, false, 0, "g", 67},     {"null-h", 0, 1, false, 0, "h", 105},
    {"null-i", 0, 1, false, 0, "i", 100},    {"null-j", 0, 1, false, 0, "j", 156},
    {"null-a", 0, 1, false, 0, "a", 419},    {"short-a2", 0, 1, false, 0, "a2", 445},
    {"short-g0", 0, 1, false, 0, "g0", 67},  {"short-h1", 0, 1, false, 0, "h1", 105},
    {"short-i0", 0, 1, false, 0, "i0", 100}, {"short-j0", 0, 1, false, 0, "j0", 156},
    {"null-h-i", 0, 1, false, 0, "hi", 100}, {"short-j1", 0, 1, false, 0, "j1", 156},
    {"clamp", 4, 1, false, 0, "", 0},        {"clamp-color", 4, 1, true, 0, "", 0},
    {"signed-zero", 5, 1, false, 0, "", 0}};

typedef struct fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
} fixture;

static bool setup(fixture *f, const char *resources, size_t budget) {
    memset(f, 0, sizeof(*f));
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(budget, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    return true;
}

static bool cleanup(fixture *f) {
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_resource_files_free(f->files);
    return true;
}

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

static uint32_t bits(float value) {
    uint32_t out;
    memcpy(&out, &value, 4);
    return out;
}

static uint64_t double_bits(double value) {
    uint64_t out;
    memcpy(&out, &value, 8);
    return out;
}

static void initialize(mc_client_water_texture_effect *owner, const test_case *c) {
    mc_water_texture_fields *f = &owner->state.fields;
    f->k = c->counter;
    for (size_t n = 0; n < 256; ++n) {
        ((float *)f->g)[n] = c->pattern == 1 ? (float)((int32_t)(n * 17 % 41) - 20) * 0.0625f : 0;
        ((float *)f->h)[n] = c->pattern == 1 ? -9.0f : 0;
        ((float *)f->i)[n] = c->pattern == 1 ? (float)((int32_t)(n % 7) - 3) * 0.03125f : 0;
        ((float *)f->j)[n] = c->pattern == 1 ? (float)((int32_t)(n % 5) - 2) * 0.125f : 0;
        if (c->pattern == 2) {
            ((float *)f->g)[n] =
                from_bits((uint32_t)(n % 7 + 1) | ((n & 1) ? UINT32_C(0x80000000) : 0));
            ((float *)f->i)[n] = from_bits((uint32_t)(n % 3 + 1));
        }
        if (c->pattern == 4) {
            static const uint32_t values[] = {UINT32_C(0x40000000), UINT32_C(0xc0000000),
                                              UINT32_C(0x7f800000), UINT32_C(0xff800000)};
            ((float *)f->i)[n] = from_bits(values[n % 4]);
        }
        if (c->pattern == 5) {
            ((float *)f->g)[n] = from_bits(UINT32_C(0x80000000));
            ((float *)f->h)[n] = from_bits(UINT32_C(0x80000000));
            ((float *)f->i)[n] = from_bits(UINT32_C(0x80000000));
            ((float *)f->j)[n] = from_bits(UINT32_C(0x80000000));
        }
    }
    if (c->pattern == 3) {
        ((float *)f->g)[0] = from_bits(UINT32_C(0x7fc12345));
        ((float *)f->g)[1] = from_bits(UINT32_C(0xffc54321));
        ((float *)f->g)[15] = from_bits(UINT32_C(0x7fc00abc));
        ((float *)f->i)[0] = from_bits(UINT32_C(0xffc12345));
        ((float *)f->j)[0] = from_bits(UINT32_C(0x7fcabcde));
    }
}

static int identity(const void *p, const void *const *ids) {
    if (p == NULL)
        return 0;
    for (int n = 0; n < 6; ++n)
        if (p == ids[n])
            return n + 1;
    return -1;
}

static bool trace(FILE *out, const test_case *c, int frame, const char *outcome,
                  mc_client_water_texture_effect *owner, const void *const *ids) {
    const mc_water_texture_fields *f = &owner->state.fields;
    mc_java_math_random_owner *math = owner->profile->math;
    CHECK(fprintf(out, "CASE\t%s\t%d\t%s\t%08lx\t%012llx\t%016llx\t%d\t%d\n", c->name, frame,
                  outcome, (unsigned long)(uint32_t)f->k,
                  (unsigned long long)atomic_load(&math->state),
                  (unsigned long long)double_bits(math->next_next_gaussian),
                  (int)math->have_next_next_gaussian, (int)f->c) > 0);
    const float *arrays[] = {f->g, f->h, f->i, f->j};
    const size_t lengths[] = {f->g_length, f->h_length, f->i_length, f->j_length};
    for (size_t n = 0; n < 4; ++n) {
        CHECK(fprintf(out, "ARRAY\t%s\t%d\t%c\t%d\t%ld\t", c->name, frame, (char)('g' + (int)n),
                      identity(arrays[n], ids), arrays[n] == NULL ? -1L : (long)lengths[n]) > 0);
        if (arrays[n] != NULL)
            for (size_t i = 0; i < lengths[n]; ++i)
                CHECK(fprintf(out, "%08lx", (unsigned long)bits(arrays[n][i])) == 8);
        CHECK(fputc('\n', out) != EOF);
    }
    CHECK(fprintf(out, "RGBA\t%s\t%d\t%d\t%ld\t", c->name, frame, identity(f->a, ids),
                  f->a == NULL ? -1L : (long)f->a_length) > 0);
    if (f->a != NULL)
        for (size_t n = 0; n < f->a_length; ++n)
            CHECK(fprintf(out, "%02x", (unsigned int)f->a[n]) == 2);
    CHECK(fputc('\n', out) != EOF);
    return true;
}

static bool run_cases(fixture *f, FILE *out) {
    for (size_t which = 0; which < sizeof(cases) / sizeof(*cases); ++which) {
        const test_case *c = &cases[which];
        mc_client_water_texture_effect *owner = NULL;
        CHECK(mc_client_water_texture_allocate(f->registry, f->profile, NULL, &owner, NULL) ==
              MC_WATER_TEXTURE_OK);
        CHECK(mc_client_water_texture_construct(owner, NULL) == MC_WATER_TEXTURE_OK);
        CHECK(mc_client_water_texture_set_anaglyph(owner, c->color, NULL) == MC_WATER_TEXTURE_OK);
        initialize(owner, c);
        /* Declared finite fixture: warm actual shared holder, then actual setSeed
         * state transition, corresponding to Java Random.setSeed on that holder.
         * No animation draw or production constructor uses an injected RNG/output. */
        double warm;
        CHECK(mc_java_math_random(f->profile, &warm) == MC_JAVA_RANDOM_PROFILE_OK);
        const uint64_t seed = UINT64_C(0x123456789ab) + which * UINT64_C(101);
        atomic_store(&f->profile->math->state,
                     (seed ^ UINT64_C(0x5deece66d)) & UINT64_C(0xffffffffffff));
        f->profile->math->have_next_next_gaussian = false;
        const mc_water_texture_fields original = owner->state.fields;
        void *extra = NULL;
        const void *ids[6] = {original.a, original.g, original.h, original.i, original.j, NULL};
        const char *fault = c->fault;
        if (strcmp(fault, "hi") == 0) {
            owner->state.fields.h = NULL;
            owner->state.fields.i = NULL;
        } else if (fault[0] != 0) {
            size_t length = fault[1] == 0 ? 0 : (size_t)(fault[1] - '0');
            if (fault[1] != 0) {
                extra = calloc(length == 0 ? 1 : length, fault[0] == 'a' ? 1 : sizeof(float));
                CHECK(extra != NULL);
                ids[5] = extra;
            }
            if (fault[0] == 'a') {
                owner->state.fields.a = extra;
                owner->state.fields.a_length = length;
            } else {
                const float **slots[] = {&owner->state.fields.g, &owner->state.fields.h,
                                         &owner->state.fields.i, &owner->state.fields.j};
                size_t *lengths[] = {&owner->state.fields.g_length, &owner->state.fields.h_length,
                                     &owner->state.fields.i_length, &owner->state.fields.j_length};
                *slots[(size_t)(fault[0] - 'g')] = extra;
                *lengths[(size_t)(fault[0] - 'g')] = length;
            }
        }
        CHECK(trace(out, c, 0, "SETUP", owner, ids));
        for (int frame = 1; frame <= c->frames; ++frame) {
            mc_water_texture_error error;
            const mc_water_texture_status status = mc_client_water_texture_animate(owner, &error);
            CHECK(status == (c->pc == 0 ? MC_WATER_TEXTURE_OK
                                        : (fault[1] != 0 && strcmp(fault, "hi") != 0
                                               ? MC_WATER_TEXTURE_INDEX_OUT_OF_RANGE
                                               : MC_WATER_TEXTURE_NULL_REFERENCE)));
            CHECK(error.source_pc == (c->pc == 0 ? -1 : c->pc));
            CHECK(owner->state.phase == MC_WATER_TEXTURE_READY && !owner->animating);
            CHECK(owner->state.fields.b == 205 && owner->state.fields.d == 0 &&
                  owner->state.fields.e == 1 && owner->state.fields.f == 0 &&
                  owner->state.array_bytes == 5120);
            CHECK(trace(out, c, frame,
                        status == MC_WATER_TEXTURE_OK               ? "OK"
                        : status == MC_WATER_TEXTURE_NULL_REFERENCE ? "NULL"
                                                                    : "BOUNDS",
                        owner, ids));
        }
        owner->state.fields = original;
        free(extra);
        CHECK(mc_client_water_texture_retire(&owner, NULL) == MC_WATER_TEXTURE_OK);
    }
    CHECK(fprintf(out, "END\t%zu\n", sizeof(cases) / sizeof(*cases)) > 0);
    return true;
}

static uint64_t advance(uint64_t seed, size_t steps) {
    for (size_t n = 0; n < steps; ++n)
        seed = (seed * UINT64_C(0x5deece66d) + 11) & UINT64_C(0xffffffffffff);
    return seed;
}

static bool native_policies(const char *resources) {
    fixture f;
    CHECK(setup(&f, resources, SIZE_MAX));
    mc_client_water_texture_effect *a = NULL, *b = NULL;
    CHECK(mc_client_water_texture_allocate(f.registry, f.profile, NULL, &a, NULL) ==
          MC_WATER_TEXTURE_OK);
    CHECK(mc_client_water_texture_animate(a, NULL) == MC_WATER_TEXTURE_INVALID_PHASE &&
          a->state.fields.k == 0);
    CHECK(mc_client_water_texture_set_anaglyph(a, true, NULL) == MC_WATER_TEXTURE_INVALID_PHASE);
    CHECK(mc_client_water_texture_construct(a, NULL) == MC_WATER_TEXTURE_OK);
    CHECK(mc_client_water_texture_allocate(f.registry, f.profile, NULL, &b, NULL) ==
          MC_WATER_TEXTURE_OK);
    CHECK(mc_client_water_texture_construct(b, NULL) == MC_WATER_TEXTURE_OK);
    CHECK(atomic_load(&f.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD);
    const uint64_t before = atomic_load(&f.profile->uniquifier);
    CHECK(mc_client_water_texture_animate(a, NULL) == MC_WATER_TEXTURE_OK);
    CHECK(atomic_load(&f.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_READY &&
          f.profile->math != NULL);
    CHECK(atomic_load(&f.profile->uniquifier) == before * UINT64_C(1181783497276652981));
    CHECK(!f.profile->math->have_next_next_gaussian &&
          double_bits(f.profile->math->next_next_gaussian) == 0);
    for (size_t p = 0; p < 256; ++p) {
        CHECK(a->state.fields.a[p * 4] == 32 && a->state.fields.a[p * 4 + 1] == 50 &&
              a->state.fields.a[p * 4 + 2] == 255 && a->state.fields.a[p * 4 + 3] == 146);
    }
    const uint64_t seed = atomic_load(&f.profile->math->state),
                   unique = atomic_load(&f.profile->uniquifier);
    CHECK(mc_client_water_texture_animate(b, NULL) == MC_WATER_TEXTURE_OK);
    CHECK(atomic_load(&f.profile->math->state) == advance(seed, 512) &&
          atomic_load(&f.profile->uniquifier) == unique);
    /* Actual unsupported FP mode reaches the real Math leaf only after k and
     * preceding arithmetic/array stores. No entry preflight moves that failure. */
    {
        const uint64_t math_before = atomic_load(&f.profile->math->state);
        const float *before_g = b->state.fields.g, *before_h = b->state.fields.h;
        const unsigned int control = _mm_getcsr();
        mc_water_texture_error fp_error;
        _mm_setcsr((control & ~UINT32_C(0x6000)) | UINT32_C(0x2000));
        const mc_water_texture_status failed = mc_client_water_texture_animate(b, &fp_error);
        _mm_setcsr(control);
        CHECK(failed == MC_WATER_TEXTURE_RANDOM_FAILED &&
              fp_error.random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP &&
              fp_error.source_pc == 206);
        CHECK(b->state.fields.k == 2 && b->state.fields.g == before_g &&
              b->state.fields.h == before_h);
        CHECK(atomic_load(&f.profile->math->state) == math_before);
        CHECK(mc_client_water_texture_animate(b, NULL) == MC_WATER_TEXTURE_OK &&
              b->state.fields.k == 3);
        CHECK(atomic_load(&f.profile->math->state) == advance(math_before, 512));
    }
    a->animating = true;
    CHECK(mc_client_water_texture_animate(a, NULL) == MC_WATER_TEXTURE_BUSY);
    CHECK(mc_client_water_texture_set_anaglyph(a, true, NULL) == MC_WATER_TEXTURE_BUSY);
    CHECK(mc_client_water_texture_retire(&a, NULL) == MC_WATER_TEXTURE_BUSY && a != NULL);
    a->animating = false;
    CHECK(mc_client_water_texture_retire(&a, NULL) == MC_WATER_TEXTURE_OK);
    CHECK(mc_client_water_texture_retire(&b, NULL) == MC_WATER_TEXTURE_OK);
    CHECK(cleanup(&f));
    CHECK(setup(&f, resources, sizeof(mc_java_random_profile)));
    CHECK(mc_client_water_texture_allocate(f.registry, f.profile, NULL, &a, NULL) ==
          MC_WATER_TEXTURE_OK);
    CHECK(mc_client_water_texture_construct(a, NULL) == MC_WATER_TEXTURE_OK);
    ((float *)a->state.fields.i)[0] = 0.25f;
    ((float *)a->state.fields.j)[0] = -0.5f;
    const float *g = a->state.fields.g, *h = a->state.fields.h;
    mc_water_texture_error error;
    CHECK(mc_client_water_texture_animate(a, &error) == MC_WATER_TEXTURE_RANDOM_FAILED);
    CHECK(error.source_pc == 206 && error.random == MC_JAVA_RANDOM_PROFILE_LIMIT);
    CHECK(a->state.phase == MC_WATER_TEXTURE_READY && a->state.fields.k == 1 && !a->animating);
    CHECK(a->state.fields.g == g && a->state.fields.h == h && bits(h[0]) == UINT32_C(0x3e4ccccd));
    CHECK(bits(a->state.fields.i[0]) == UINT32_C(0x3e666666) &&
          bits(a->state.fields.j[0]) == UINT32_C(0xbf19999a));
    for (size_t p = 0; p < 1024; ++p)
        CHECK(a->state.fields.a[p] == 0);
    CHECK(mc_client_water_texture_animate(a, &error) == MC_WATER_TEXTURE_RANDOM_FAILED &&
          error.random == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED && a->state.fields.k == 2);
    CHECK(a->state.fields.g == g && a->state.fields.h == h);
    CHECK(mc_client_water_texture_retire(&a, NULL) == MC_WATER_TEXTURE_OK);
    CHECK(cleanup(&f));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("Water animation input: resources output-file\n", stderr);
        return 2;
    }
    FILE *out = fopen(argv[2], "wb");
    if (out == NULL) {
        fputs("Water animation input: resources output-file\n", stderr);
        return 2;
    }
    fixture f;
    if (!setup(&f, argv[1], SIZE_MAX) || !run_cases(&f, out) || fclose(out) != 0 || !cleanup(&f) ||
        !native_policies(argv[1]))
        return 1;
    printf("Water animation: %zu checks\n", checks);
    return 0;
}
