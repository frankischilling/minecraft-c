/* Authored finite observer over the actual owner/layout, not a replay owner.
 * Including the production body permits private fixture field setup only.
 * Its genuine animation and shared mc_java_math_random remain unchanged. */
#include "../../../src/client/texture_effects/lava.c"
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
            fprintf(stderr, "Lava animation contract:%d: %s\n", __LINE__, #x);                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct test_case {
    const char *name;
    int pattern, frames;
    bool color;
    const char *fault;
    int pc;
} test_case;

static const test_case cases[] = {
    {"flat", 0, 2, false, "", 0},         {"flat-color", 0, 1, true, "", 0},
    {"waves", 1, 5, false, "", 0},        {"waves-color", 1, 3, true, "", 0},
    {"waves-repeat", 1, 1, false, "", 0}, {"subnormal", 2, 2, false, "", 0},
    {"nan", 3, 1, false, "", 0},          {"nan-color", 3, 1, true, "", 0},
    {"null-g", 0, 1, false, "g", 115},    {"null-h", 0, 1, false, "h", 238},
    {"null-i", 0, 1, false, "i", 164},    {"null-j", 0, 1, false, "j", 261},
    {"null-a", 0, 1, false, "a", 525},    {"short-a2", 0, 1, false, "a2", 551},
    {"short-g0", 0, 1, false, "g0", 115}, {"short-h1", 0, 1, false, "h1", 238},
    {"short-i0", 0, 1, false, "i0", 164}, {"short-j0", 0, 1, false, "j0", 261},
    {"null-h-i", 0, 1, false, "hi", 164}, {"short-j1", 0, 1, false, "j1", 261},
    {"clamp", 4, 1, false, "", 0},        {"clamp-color", 4, 1, true, "", 0},
    {"signed-zero", 5, 1, false, "", 0}};

typedef struct fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_math_trig *trig;
} fixture;

static bool setup(fixture *f, const char *resources, size_t budget) {
    memset(f, 0, sizeof(*f));
    f->trig = malloc(sizeof(*f->trig));
    CHECK(f->trig != NULL && mc_math_trig_init(f->trig));
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
    free(f->trig);
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

static void initialize(mc_client_lava_texture_effect *owner, const test_case *c) {
    mc_lava_texture_fields *f = &owner->state.fields;
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
                  mc_client_lava_texture_effect *owner, const void *const *ids) {
    const mc_lava_texture_fields *f = &owner->state.fields;
    mc_java_math_random_owner *math = owner->profile->math;
    CHECK(fprintf(out, "CASE\t%s\t%d\t%s\t%012llx\t%016llx\t%d\t%d\n", c->name, frame, outcome,
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
        mc_client_lava_texture_effect *owner = NULL;
        CHECK(mc_client_lava_texture_allocate(f->registry, f->profile, f->trig, NULL, &owner,
                                              NULL) == MC_LAVA_TEXTURE_OK);
        CHECK(mc_client_lava_texture_construct(owner, NULL) == MC_LAVA_TEXTURE_OK);
        const mc_lava_texture_fields *initial = &owner->state.fields;
        CHECK(initial->b == 237 && !initial->c && initial->d == 0 && initial->e == 1 &&
              initial->f == 0);
        CHECK(initial->a_length == 1024 && initial->g_length == 256 && initial->h_length == 256 &&
              initial->i_length == 256 && initial->j_length == 256);
        for (size_t n = 0; n < 1024; ++n)
            CHECK(initial->a[n] == 0);
        for (size_t n = 0; n < 256; ++n)
            CHECK(bits(initial->g[n]) == 0 && bits(initial->h[n]) == 0 &&
                  bits(initial->i[n]) == 0 && bits(initial->j[n]) == 0);
        CHECK(fprintf(out, "CTOR\t%s\t%ld\t%d\t%ld\t%ld\t%ld\n", c->name, (long)initial->b,
                      (int)initial->c, (long)initial->d, (long)initial->e, (long)initial->f) > 0);
        CHECK(mc_client_lava_texture_set_anaglyph(owner, c->color, NULL) == MC_LAVA_TEXTURE_OK);
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
        const mc_lava_texture_fields original = owner->state.fields;
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
            mc_lava_texture_error error;
            const mc_lava_texture_status status = mc_client_lava_texture_animate(owner, &error);
            CHECK(status == (c->pc == 0 ? MC_LAVA_TEXTURE_OK
                                        : (fault[1] != 0 && strcmp(fault, "hi") != 0
                                               ? MC_LAVA_TEXTURE_INDEX_OUT_OF_RANGE
                                               : MC_LAVA_TEXTURE_NULL_REFERENCE)));
            CHECK(error.source_pc == (c->pc == 0 ? -1 : c->pc));
            CHECK(owner->state.phase == MC_LAVA_TEXTURE_READY && !owner->animating);
            CHECK(owner->state.fields.b == 237 && owner->state.fields.d == 0 &&
                  owner->state.fields.e == 1 && owner->state.fields.f == 0 &&
                  owner->state.array_bytes == 5120);
            CHECK(trace(out, c, frame,
                        status == MC_LAVA_TEXTURE_OK               ? "OK"
                        : status == MC_LAVA_TEXTURE_NULL_REFERENCE ? "NULL"
                                                                   : "BOUNDS",
                        owner, ids));
        }
        owner->state.fields = original;
        free(extra);
        CHECK(mc_client_lava_texture_retire(&owner, NULL) == MC_LAVA_TEXTURE_OK);
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
    mc_client_lava_texture_effect *a = NULL, *b = NULL;
    CHECK(mc_client_lava_texture_allocate(f.registry, f.profile, f.trig, NULL, &a, NULL) ==
          MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_animate(a, NULL) == MC_LAVA_TEXTURE_INVALID_PHASE);
    CHECK(mc_client_lava_texture_set_anaglyph(a, true, NULL) == MC_LAVA_TEXTURE_INVALID_PHASE);
    CHECK(mc_client_lava_texture_construct(a, NULL) == MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_allocate(f.registry, f.profile, f.trig, NULL, &b, NULL) ==
          MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_construct(b, NULL) == MC_LAVA_TEXTURE_OK);
    CHECK(atomic_load(&f.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD);
    const uint64_t before = atomic_load(&f.profile->uniquifier);
    CHECK(mc_client_lava_texture_animate(a, NULL) == MC_LAVA_TEXTURE_OK);
    CHECK(atomic_load(&f.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_READY &&
          f.profile->math != NULL);
    CHECK(atomic_load(&f.profile->uniquifier) == before * UINT64_C(1181783497276652981));
    CHECK(!f.profile->math->have_next_next_gaussian &&
          double_bits(f.profile->math->next_next_gaussian) == 0);
    for (size_t p = 0; p < 256; ++p) {
        CHECK(a->state.fields.a[p * 4] == 155 && a->state.fields.a[p * 4 + 1] == 0 &&
              a->state.fields.a[p * 4 + 2] == 0 && a->state.fields.a[p * 4 + 3] == 255);
    }
    const uint64_t seed = atomic_load(&f.profile->math->state),
                   unique = atomic_load(&f.profile->uniquifier);
    CHECK(mc_client_lava_texture_animate(b, NULL) == MC_LAVA_TEXTURE_OK);
    CHECK(atomic_load(&f.profile->math->state) == advance(seed, 512) &&
          atomic_load(&f.profile->uniquifier) == unique);
    /* Actual unsupported FP mode reaches the real Math leaf only after
     * preceding arithmetic/array stores. No entry preflight moves that failure. */
    {
        const uint64_t math_before = atomic_load(&f.profile->math->state);
        const float *before_g = b->state.fields.g, *before_h = b->state.fields.h;
        const unsigned int control = _mm_getcsr();
        mc_lava_texture_error fp_error;
        _mm_setcsr((control & ~UINT32_C(0x6000)) | UINT32_C(0x2000));
        const mc_lava_texture_status failed = mc_client_lava_texture_animate(b, &fp_error);
        _mm_setcsr(control);
        CHECK(failed == MC_LAVA_TEXTURE_RANDOM_FAILED &&
              fp_error.random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP &&
              fp_error.source_pc == 311);
        CHECK(b->state.fields.g == before_g && b->state.fields.h == before_h);
        CHECK(atomic_load(&f.profile->math->state) == math_before);
        CHECK(mc_client_lava_texture_animate(b, NULL) == MC_LAVA_TEXTURE_OK);
        CHECK(atomic_load(&f.profile->math->state) == advance(math_before, 512));
    }
    a->animating = true;
    CHECK(mc_client_lava_texture_animate(a, NULL) == MC_LAVA_TEXTURE_BUSY);
    CHECK(mc_client_lava_texture_set_anaglyph(a, true, NULL) == MC_LAVA_TEXTURE_BUSY);
    CHECK(mc_client_lava_texture_retire(&a, NULL) == MC_LAVA_TEXTURE_BUSY && a != NULL);
    a->animating = false;
    CHECK(mc_client_lava_texture_retire(&a, NULL) == MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_retire(&b, NULL) == MC_LAVA_TEXTURE_OK);
    CHECK(cleanup(&f));
    CHECK(setup(&f, resources, sizeof(mc_java_random_profile)));
    CHECK(mc_client_lava_texture_allocate(f.registry, f.profile, f.trig, NULL, &a, NULL) ==
          MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_construct(a, NULL) == MC_LAVA_TEXTURE_OK);
    ((float *)a->state.fields.i)[0] = 0.25f;
    ((float *)a->state.fields.j)[0] = -0.5f;
    const float *g = a->state.fields.g, *h = a->state.fields.h;
    mc_lava_texture_error error;
    CHECK(mc_client_lava_texture_animate(a, &error) == MC_LAVA_TEXTURE_RANDOM_FAILED);
    CHECK(error.source_pc == 311 && error.random == MC_JAVA_RANDOM_PROFILE_LIMIT);
    CHECK(a->state.phase == MC_LAVA_TEXTURE_READY && !a->animating);
    CHECK(a->state.fields.g == g && a->state.fields.h == h && bits(h[0]) == UINT32_C(0x3d4ccccd));
    CHECK(bits(a->state.fields.i[0]) == UINT32_C(0x3e7ae148) &&
          bits(a->state.fields.j[0]) == UINT32_C(0xbf0f5c29));
    for (size_t p = 0; p < 1024; ++p)
        CHECK(a->state.fields.a[p] == 0);
    CHECK(mc_client_lava_texture_animate(a, &error) == MC_LAVA_TEXTURE_RANDOM_FAILED &&
          error.random == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED);
    CHECK(a->state.fields.g == g && a->state.fields.h == h);
    CHECK(mc_client_lava_texture_retire(&a, NULL) == MC_LAVA_TEXTURE_OK);
    CHECK(cleanup(&f));
    CHECK(setup(&f, resources, SIZE_MAX));
    const size_t quotas[] = {1, 1024, 2048, 3072, 4096};
    const int32_t allocation_pcs[] = {8, 14, 23, 32, 41};
    for (size_t n = 0; n < sizeof(quotas) / sizeof(*quotas); ++n) {
        const mc_lava_texture_options options = {quotas[n]};
        CHECK(mc_client_lava_texture_allocate(f.registry, f.profile, f.trig, &options, &a, NULL) ==
              MC_LAVA_TEXTURE_OK);
        mc_lava_texture_error quota_error;
        CHECK(mc_client_lava_texture_construct(a, &quota_error) == MC_LAVA_TEXTURE_LIMIT);
        CHECK(quota_error.source_pc == allocation_pcs[n] &&
              quota_error.cg_pc == (n == 0 ? 7 : allocation_pcs[n]) &&
              strcmp(quota_error.source_class, n == 0 ? "aw" : "cg") == 0);
        CHECK(a->state.phase == MC_LAVA_TEXTURE_FAILED && a->state.array_bytes == n * 1024);
        CHECK(a->state.fields.b == (n == 0 ? 0 : 237) && a->state.fields.e == (n == 0 ? 0 : 1));
        CHECK(mc_client_lava_texture_construct(a, NULL) == MC_LAVA_TEXTURE_INVALID_PHASE);
        CHECK(mc_client_lava_texture_animate(a, NULL) == MC_LAVA_TEXTURE_INVALID_PHASE);
        CHECK(mc_client_lava_texture_retire(&a, NULL) == MC_LAVA_TEXTURE_OK && a == NULL);
    }
    CHECK(atomic_load(&f.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD);
    CHECK(cleanup(&f));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("Lava animation input: resources output-file\n", stderr);
        return 2;
    }
    FILE *out = fopen(argv[2], "wb");
    if (out == NULL) {
        fputs("Lava animation input: resources output-file\n", stderr);
        return 2;
    }
    fixture f;
    if (!setup(&f, argv[1], SIZE_MAX) || !run_cases(&f, out) || fclose(out) != 0 || !cleanup(&f) ||
        !native_policies(argv[1]))
        return 1;
    printf("Lava animation: %zu checks\n", checks);
    return 0;
}
