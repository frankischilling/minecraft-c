#include "../../../src/core/java/random_profile_internal.h"
#include "client/texture_effects/effect_internal.h"
#include "fixtures.h"
#include "mc/statistics/registry/registry.h"
static unsigned checks;
#define NEED(c)                                                                                    \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "effects:%d: %s\n", __LINE__, #c);                                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fx_effects {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_math_trig *trig;
    const char *root;
} fx_effects;

static bool prepare(fx_effects *f, const char *root) {
    f->root = root;
    NEED(fx_make(root));
    NEED(mc_resource_files_create(root, &f->files) == MC_REGISTRY_OK);
    NEED(mc_materials_create(&f->materials));
    NEED(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f->profile,
                                         .materials = f->materials,
                                         .resources = mc_resource_files_source(f->files)};
    NEED(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    NEED(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    f->trig = malloc(sizeof(*f->trig));
    NEED(f->trig != NULL && mc_math_trig_init(f->trig));
    double discarded;
    NEED(mc_java_math_random(f->profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK);
    return true;
}

static mc_texture_effect_services services(fx_effects *f) {
    return (mc_texture_effect_services){f->registry, f->profile, f->trig, f->files,
                                        mc_resource_files_source(f->files)};
}

static bool one(fx_effects *f, mc_texture_effect_kind kind, int argument) {
    mc_texture_effect_error *e = calloc(1, sizeof(*e));
    NEED(e != NULL);
    mc_client_texture_effect *owner = NULL;
    const mc_texture_effect_services service = services(f);
    const uint64_t before = atomic_load(&f->profile->math->state);
    NEED(mc_client_texture_effect_allocate(&service, kind, NULL, &owner, e) ==
         MC_TEXTURE_EFFECT_OK);
    mc_texture_effect_state raw;
    NEED(mc_client_texture_effect_inspect(owner, &raw));
    NEED(raw.phase == MC_TEXTURE_EFFECT_RAW && raw.fields.a == NULL && raw.array_bytes == 0);
    NEED(mc_client_texture_effect_construct(owner, NULL, argument, e) == MC_TEXTURE_EFFECT_OK);
    mc_texture_effect_state state;
    NEED(mc_client_texture_effect_inspect(owner, &state));
    NEED(state.phase == MC_TEXTURE_EFFECT_READY && state.fields.a_length == 1024 &&
         !state.fields.c && state.fields.d == 0 && state.fields.f == 0);
    NEED(atomic_load(&f->profile->math->state) == before);
    if (kind == MC_TEXTURE_EFFECT_PORTAL) {
        NEED(state.frame_count == 32 && state.frames != NULL);
        const mc_texture_effect_owner *actual = (const mc_texture_effect_owner *)owner;
        mc_java_random oracle;
        mc_java_random_seed(&oracle, 100);
        for (size_t n = 0; n < 8192; ++n)
            (void)mc_java_random_next_float(&oracle);
        NEED(actual->random_constructed && actual->random->state == oracle.state);
        for (size_t n = 0; n < 32; ++n) {
            NEED(state.frames[n] != NULL);
            for (size_t j = 0; j < n; ++j)
                NEED(state.frames[n] != state.frames[j]);
            for (size_t pixel = 0; pixel < 256; ++pixel)
                NEED(state.frames[n][pixel * 4 + 2] == state.frames[n][pixel * 4 + 3]);
        }
        NEED(memcmp(state.frames[0], state.frames[1], 1024) != 0);
        NEED(mc_client_texture_effect_tick(owner, e) == MC_TEXTURE_EFFECT_OK);
        NEED(mc_client_texture_effect_inspect(owner, &state) && state.counter == 1 &&
             memcmp(state.fields.a, state.frames[1], 1024) == 0);
        NEED(mc_client_texture_effect_set_anaglyph(owner, true, e) == MC_TEXTURE_EFFECT_OK);
        NEED(mc_client_texture_effect_tick(owner, e) == MC_TEXTURE_EFFECT_OK);
        NEED(mc_client_texture_effect_inspect(owner, &state) && state.counter == 2);
        const int r = state.frames[2][0], g = state.frames[2][1], b = state.frames[2][2];
        NEED(state.fields.a[0] == (uint8_t)((r * 30 + g * 59 + b * 11) / 100));
    } else {
        mc_java_random oracle = {.state = before};
        const size_t draws = kind == MC_TEXTURE_EFFECT_FIRE ? 64 : 256;
        for (size_t n = 0; n < draws; ++n)
            (void)mc_java_random_next_double(&oracle);
        const float *old_g = state.g, *old_h = state.h;
        NEED(mc_client_texture_effect_tick(owner, e) == MC_TEXTURE_EFFECT_OK);
        NEED(mc_client_texture_effect_inspect(owner, &state));
        NEED(state.g == old_h && state.h == old_g && state.g != state.h);
        NEED(atomic_load(&f->profile->math->state) == oracle.state);
        if (kind == MC_TEXTURE_EFFECT_FLOWING_WATER)
            NEED(state.fields.a[0] == 32 && state.fields.a[1] == 50 && state.fields.a[2] == 255 &&
                 state.fields.a[3] == 146);
        if (kind == MC_TEXTURE_EFFECT_FLOWING_LAVA)
            NEED(state.fields.a[0] == 155 && state.fields.a[1] == 0 && state.fields.a[2] == 0 &&
                 state.fields.a[3] == 255);
        if (kind == MC_TEXTURE_EFFECT_FIRE)
            NEED(state.g_length == 320 && state.fields.a[0] == 100 && state.fields.a[3] == 0);
    }
    NEED(mc_client_texture_effect_retain(owner, e) == MC_TEXTURE_EFFECT_OK);
    mc_client_texture_effect *copy = owner;
    NEED(mc_client_texture_effect_release(&copy, e) == MC_TEXTURE_EFFECT_OK && copy == NULL);
    NEED(mc_client_texture_effect_inspect(owner, &state) && state.native_holds == 1);
    NEED(mc_client_texture_effect_release(&owner, e) == MC_TEXTURE_EFFECT_OK && owner == NULL);
    free(e);
    return true;
}

static bool failures(fx_effects *f) {
    mc_texture_effect_error *e = calloc(1, sizeof(*e));
    NEED(e != NULL);
    const mc_texture_effect_services service = services(f);
    mc_client_texture_effect *p = NULL;
    mc_texture_effect_options options = {.max_array_bytes = 1024 + 32 * sizeof(void *) + 2 * 1024};
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_PORTAL, &options, &p, e) ==
         MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(p, NULL, 0, e) == MC_TEXTURE_EFFECT_LIMIT &&
         e->source_pc == 21);
    mc_texture_effect_state s;
    NEED(mc_client_texture_effect_inspect(p, &s) && s.phase == MC_TEXTURE_EFFECT_FAILED &&
         s.frames == NULL && s.last_store_pc == 12);
    NEED(((mc_texture_effect_owner *)p)->pending_frame_count == 2);
    NEED(mc_client_texture_effect_construct(p, NULL, 0, e) == MC_TEXTURE_EFFECT_INVALID_PHASE);
    NEED(mc_client_texture_effect_release(&p, e) == MC_TEXTURE_EFFECT_OK && p == NULL);
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_FIRE, NULL, &p, e) ==
         MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(p, NULL, 0, e) == MC_TEXTURE_EFFECT_OK);
    mc_texture_effect_owner *actual = (mc_texture_effect_owner *)p;
    float *saved = actual->g;
    actual->g = NULL;
    const unsigned csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8040u);
    const mc_texture_effect_status status = mc_client_texture_effect_tick(p, e);
    _mm_setcsr(csr);
    actual->g = saved;
    NEED(status == MC_TEXTURE_EFFECT_NULL_REFERENCE && e->source_pc == 34);
    NEED(mc_client_texture_effect_release(&p, e) == MC_TEXTURE_EFFECT_OK);
    free(e);
    return true;
}

static bool resource_cases(fx_effects *f) {
    mc_client_class_url *url = NULL;
    mc_client_class_url_error error = {0};
    const mc_registry_resources *source = mc_resource_files_source(f->files);
    NEED(mc_client_class_url_create(f->files, source, "/missing.png", &url, &error) ==
             MC_CLIENT_CLASS_URL_OK &&
         url == NULL);
    const mc_client_image_io_limits limits = {1048576, 1048576, 262144};
    mc_client_argb_image *image = NULL;
    NEED(mc_client_class_url_read_image(url, &limits, &image, &error) ==
             MC_CLIENT_CLASS_URL_SOURCE_ILLEGAL_ARGUMENT &&
         image == NULL);
    NEED(mc_client_class_url_create(f->files, source, "/unrecognized.bin", &url, &error) ==
             MC_CLIENT_CLASS_URL_OK &&
         url != NULL);
    NEED(mc_client_class_url_read_image(url, &limits, &image, &error) == MC_CLIENT_CLASS_URL_OK &&
         image == NULL && error.source_close_reached && error.source_returned);
    NEED(mc_client_class_url_release(&url, &error) == MC_CLIENT_CLASS_URL_OK && url == NULL);
    /* Only this freshly authored root is changed. These are real byte/file
     * failures, not decoder callbacks or manufactured Java throwables. */
    char path[4096];
    NEED(fx_path(path, sizeof(path), f->root, "gui/items.png"));
    FILE *file = fopen(path, "wb");
    NEED(file != NULL);
    static const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    NEED(fwrite(signature, 1, sizeof(signature), file) == sizeof(signature));
    NEED(fclose(file) == 0);
    const mc_texture_effect_services service = services(f);
    mc_client_texture_effect *p = NULL;
    mc_texture_effect_error *e = calloc(1, sizeof(*e));
    NEED(e != NULL);
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_COMPASS, NULL, &p, e) ==
         MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(p, NULL, 0, e) ==
             MC_TEXTURE_EFFECT_DIAGNOSTIC_DEPENDENCY &&
         e->source_pc == 88 && e->cause_pc == 37);
    NEED(e->image.source_close_reached && !e->image.source_returned);
    mc_texture_effect_state state;
    NEED(mc_client_texture_effect_inspect(p, &state) && state.phase == MC_TEXTURE_EFFECT_FAILED &&
         state.icon != NULL && state.fields.f == 1);
    NEED(mc_client_texture_effect_release(&p, e) == MC_TEXTURE_EFFECT_OK);
    NEED(fx_text(path, "authored non-image"));
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_COMPASS, NULL, &p, e) ==
         MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(p, NULL, 0, e) == MC_TEXTURE_EFFECT_NULL_REFERENCE &&
         e->source_pc == 79);
    NEED(e->image.source_close_reached && e->image.source_returned);
    NEED(mc_client_texture_effect_release(&p, e) == MC_TEXTURE_EFFECT_OK);
    NEED(remove(path) == 0);
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_COMPASS, NULL, &p, e) ==
         MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(p, NULL, 0, e) == MC_TEXTURE_EFFECT_IMAGE_FAILED &&
         e->source_pc == 37 && e->image.status == MC_CLIENT_CLASS_URL_SOURCE_ILLEGAL_ARGUMENT);
    NEED(mc_client_texture_effect_release(&p, e) == MC_TEXTURE_EFFECT_OK);
    free(e);
    return true;
}
#ifndef MC_TEXTURE_EFFECT_CPU_MAIN
#define MC_TEXTURE_EFFECT_CPU_MAIN main
#endif
int MC_TEXTURE_EFFECT_CPU_MAIN(int argc, char **argv) {
    if (argc != 2)
        return 2;
    fx_effects f = {0};
    if (!prepare(&f, argv[1]) || !one(&f, MC_TEXTURE_EFFECT_PORTAL, 0) ||
        !one(&f, MC_TEXTURE_EFFECT_FLOWING_WATER, 0) ||
        !one(&f, MC_TEXTURE_EFFECT_FLOWING_LAVA, 0) || !one(&f, MC_TEXTURE_EFFECT_FIRE, 0) ||
        !one(&f, MC_TEXTURE_EFFECT_FIRE, 1) || !failures(&f) || !resource_cases(&f))
        return 1;
    if (mc_registry_classes_free(f.registry, NULL) != MC_REGISTRY_OK)
        return 1;
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    if (mc_resource_files_free(f.files) != MC_REGISTRY_OK)
        return 1;
    free(f.trig);
    printf("Actual texture-effect CPU controls: %u\n", checks);
    return 0;
}
