#include "application_state.h"
#include "client/render_engine/color_array_internal.h"
#include "client/render_engine/color_lookup.h"
#include "client/render_engine/resource_lookup_internal.h"
#include "client/texture_packs/internal.h"
#include "mc/block/registry/registry.h"
#include "mc/resources/files/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

/* Authored/unexecuted contracts on actual prepared App/Settings/ik/Engine and
 * FILE/ZIP/ImageIO owners. No Display is created. This is not App construction,
 * startup, a completed refresh or original/native parity. */
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "color-lookup check%u line%d: %s\n", checks, __LINE__, #x);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    client_application app;
    mc_java_random_profile *profile;
    mc_materials *materials;
    mc_registry_classes *registry;
    mc_resource_files *files;
    mc_client_texture_pack_list *packs;
    mc_client_render_engine_classes *classes;
    mc_client_default_font *font;
    mc_client_render_engine *engine;
    mc_client_color_lookup_error *error;
    bool zip;
} fixture;

static mc_texture_pack_text key(const char *ascii, uint16_t units[128]) {
    const size_t length = strlen(ascii);
    if (length >= 128)
        return (mc_texture_pack_text){NULL, 0, true};
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)ascii[i];
    return (mc_texture_pack_text){units, length, false};
}

static mc_client_texture_lookup_status lookup(fixture *f, mc_client_render_engine *engine,
                                              const char *name,
                                              const mc_client_color_lookup_options *options,
                                              mc_client_color_array **out) {
    uint16_t units[128];
    return mc_client_render_engine_colors(engine, options, key(name, units), out, f->error);
}

static int open_fixture(fixture *f, const char *resources, const char *directory, bool zip) {
    memset(f, 0, sizeof(*f));
    f->zip = zip;
    f->error = calloc(1, sizeof(*f->error));
    CHECK(f->error != NULL);
    CHECK(mc_client_application_lifetime_register(&f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f->profile,
                                         .materials = f->materials,
                                         .resources = mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_game_settings_create_file(&f->app, directory, NULL, NULL, &f->app.settings, NULL,
                                       NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_texture_pack_list_allocate(mc_resource_files_source(f->files), NULL,
                                               &f->packs) == MC_TEXTURE_PACK_OK);
    CHECK(mc_client_texture_pack_list_construct(f->packs, &f->app, directory, NULL) ==
          MC_TEXTURE_PACK_OK);
    mc_client_texture_pack *pack = NULL;
    mc_texture_pack_state pack_state;
    CHECK(mc_client_texture_pack_selected(f->packs, &pack) && pack != NULL);
    CHECK(mc_client_texture_pack_inspect(pack, &pack_state));
    CHECK(pack_state.kind == (zip ? MC_TEXTURE_PACK_ZIP : MC_TEXTURE_PACK_DEFAULT));
    CHECK(mc_client_render_engine_classes_create(NULL, &f->classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &f->font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_render_engine_allocate(f->classes, f->font, NULL, &f->engine) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(f->engine, f->packs, NULL, NULL) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(f->engine->fields.i == NULL); /* Actual color method never reads Settings. */
    return 0;
}

static int release_engine(fixture *f, mc_client_render_engine **slot) {
    CHECK(mc_client_render_engine_retire_lookup(*slot, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(mc_client_texture_lookup_free_metadata(*slot));
    CHECK(mc_client_render_engine_release(*slot) == MC_CLIENT_RENDER_ENGINE_OK);
    *slot = NULL;
    return 0;
}

static int contracts(fixture *f) {
    mc_client_render_engine *o = f->engine;
    const size_t ledger = f->classes->b->count;
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(f->profile);
    mc_client_color_array *water = NULL, *other = NULL;
    CHECK(lookup(f, o, "/misc/watercolor.png", NULL, &water) == MC_CLIENT_LOOKUP_OK);
    mc_client_color_array_state state;
    CHECK(mc_client_color_array_inspect(water, &state));
    CHECK(state.phase == MC_CLIENT_COLOR_ARRAY_READY && state.length == (f->zip ? 21u : 65536u));
    if (f->zip)
        for (uint32_t y = 0; y < 3; ++y)
            for (uint32_t x = 0; x < 7; ++x) {
                const uint32_t alpha = (x + y + 1u) % 3u == 0 ? 0u : UINT32_C(0xff000000);
                const uint32_t expected =
                    alpha | ((x * 31u + 1u) << 16) | ((y * 67u + 1u) << 8) | (x + y + 1u);
                CHECK(state.bits[(size_t)y * 7u + x] == expected);
            }
    CHECK(o->fields.b->count == 0 && o->fields.d->count == 0 && o->fields.c->count == 1);
    CHECK(f->classes->b->count == ledger && f->classes->native_resource_count == 0 &&
          f->classes->device == NULL);
    CHECK(f->classes->nw.phase == MC_CLIENT_RENDER_ENGINE_RAW);
    CHECK(lookup(f, o, "/misc/watercolor.png", NULL, &other) == MC_CLIENT_LOOKUP_OK &&
          other == water);
    CHECK(mc_client_color_array_release(&other, NULL) == MC_CLIENT_COLOR_ARRAY_OK && other == NULL);
    const unsigned csr = _mm_getcsr();
    _mm_setcsr(csr | 0x2000u);
    const mc_client_texture_lookup_status integer_only =
        lookup(f, o, "/misc/watercolor.png", NULL, &other);
    _mm_setcsr(csr);
    CHECK(integer_only == MC_CLIENT_LOOKUP_OK && other == water);
    CHECK(mc_client_color_array_release(&other, NULL) == MC_CLIENT_COLOR_ARRAY_OK);

    o->fields.l = NULL;
    other = water; /* Failure sentinel is borrowed; water owns its actual occurrence. */
    CHECK(lookup(f, o, "/misc/watercolor.png", NULL, &other) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(f->error->site.pc == 4 && other == water && o->fields.c->count == 1);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    o->fields.l = f->packs;
    mc_client_texture_pack_list *raw = NULL;
    CHECK(mc_client_texture_pack_list_allocate(mc_resource_files_source(f->files), NULL, &raw) ==
          MC_TEXTURE_PACK_OK);
    o->fields.l = raw;
    other = NULL;
    CHECK(lookup(f, o, "/misc/watercolor.png", NULL, &other) == MC_CLIENT_LOOKUP_OK &&
          other == water);
    CHECK(mc_client_color_array_release(&other, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(lookup(f, o, "/color-absent.png", NULL, &other) == MC_CLIENT_LOOKUP_NULL_REFERENCE &&
          f->error->site.pc == 146);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    o->fields.l = f->packs;
    CHECK(mc_client_texture_pack_list_release(raw, NULL) == MC_TEXTURE_PACK_OK);
    CHECK(mc_client_render_engine_colors(o, NULL, (mc_texture_pack_text){NULL, 0, true}, &other,
                                         f->error) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(f->error->site.pc == 31 && f->classes->b->count == ledger);
    CHECK(lookup(f, o, "/misc/watercolor.png", NULL, &other) == MC_CLIENT_LOOKUP_BUSY);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(lookup(f, o, "/color-absent.png", NULL, &other) == MC_CLIENT_LOOKUP_OK);
    CHECK(mc_client_color_array_inspect(other, &state) && state.length == 4096);
    CHECK(mc_client_color_array_release(&other, NULL) == MC_CLIENT_COLOR_ARRAY_OK);

    CHECK(lookup(f, o, "/font.txt", NULL, &other) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(f->error->site.pc == 175 && f->error->array.site.pc == 1);
    mc_client_texture_lookup_state pending;
    mc_texture_stream_state stream;
    CHECK(mc_client_render_engine_lookup_inspect(o, &pending) && pending.color_lookup &&
          pending.image_io_returned);
    CHECK(mc_client_texture_stream_inspect(pending.stream, &stream) && stream.source_closed &&
          stream.source_close_calls == 1);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(lookup(f, o, "##/misc/watercolor.png", NULL, &other) == MC_CLIENT_LOOKUP_UNIMPLEMENTED &&
          f->error->site.pc == 52);
    CHECK(mc_client_render_engine_lookup_inspect(o, &pending) && pending.image != NULL &&
          pending.array == NULL && pending.source_closed);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(lookup(f, o, "%blur%/lookup-truncated.png", NULL, &other) ==
          MC_CLIENT_LOOKUP_UNIMPLEMENTED);
    CHECK(f->error->site.pc == 193 && f->error->caught_source_io && o->fields.k);
    CHECK(mc_client_render_engine_lookup_inspect(o, &pending) && !pending.source_closed &&
          pending.array == NULL);
    mc_client_texture_stream *active = (mc_client_texture_stream *)(void *)pending.stream;
    CHECK(active != NULL);
    active->active = true; /* Declared native guard fixture on the genuine failed-read stream. */
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_BUSY);
    CHECK(strcmp(f->error->site.method, "a") == 0 &&
          strcmp(f->error->site.descriptor, "(Ljava/lang/String;)[I") == 0);
    CHECK(mc_client_render_engine_lookup_inspect(o, &pending) && pending.stream == active &&
          !pending.native_stream_consumed);
    active->active = false;
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(lookup(f, o, "%clamp%/misc/grasscolor.png", NULL, &other) == MC_CLIENT_LOOKUP_OK);
    CHECK(!o->fields.j && o->fields.k); /* CPU conversion does not reach GL_CLAMP. */
    CHECK(mc_client_color_array_release(&other, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(lookup(f, o, "%blur%/misc/foliagecolor.png", NULL, &other) == MC_CLIENT_LOOKUP_OK &&
          !o->fields.k);
    CHECK(mc_client_color_array_release(&other, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(f->classes->b->count == ledger && f->classes->native_resource_count == 0 &&
          f->classes->device == NULL);
    CHECK(mc_java_random_profile_allocation_bytes(f->profile) == profile_bytes);

    for (size_t limit = 0; limit < 2; ++limit) {
        mc_client_render_engine *limited = NULL;
        CHECK(mc_client_render_engine_allocate(f->classes, f->font, NULL, &limited) ==
              MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(mc_client_render_engine_construct(limited, f->packs, NULL, NULL) ==
              MC_CLIENT_RENDER_ENGINE_OK);
        mc_client_color_lookup_options options = mc_client_color_lookup_default_options();
        options.arrays.max_allocations = limit;
        CHECK(lookup(f, limited, "/misc/watercolor.png", &options, &other) ==
              MC_CLIENT_LOOKUP_LIMIT);
        CHECK(f->error->site.pc == 175 && f->error->array.site.pc == 13 &&
              limited->fields.c->count == 0);
        CHECK(mc_client_render_engine_lookup_inspect(limited, &pending));
        CHECK((pending.array == NULL) == (limit == 0));
        if (limit == 1)
            CHECK(mc_client_color_array_inspect(pending.array, &state) &&
                  state.phase == MC_CLIENT_COLOR_ARRAY_FAILED);
        CHECK(release_engine(f, &limited) == 0);
    }
    mc_client_render_engine *limited = NULL;
    CHECK(mc_client_render_engine_allocate(f->classes, f->font, NULL, &limited) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(limited, f->packs, NULL, NULL) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_color_lookup_options options = mc_client_color_lookup_default_options();
    options.max_cache_entries = 0;
    CHECK(lookup(f, limited, "/misc/watercolor.png", &options, &other) == MC_CLIENT_LOOKUP_LIMIT);
    CHECK(f->error->site.pc == 185 && limited->fields.c->count == 0);
    CHECK(mc_client_render_engine_lookup_inspect(limited, &pending) && pending.array != NULL &&
          !pending.cache_published);
    CHECK(release_engine(f, &limited) == 0);

    /* Same actual c array in all three static fields, then both last parents. */
    for (size_t k = 0; k < MC_CLIENT_COLORIZER_COUNT; ++k)
        CHECK(mc_client_colorizer_set(f->classes, (mc_client_colorizer_kind)k, water, NULL, NULL) ==
              MC_CLIENT_COLORIZER_OK);
    CHECK(water->holds == 5); /* c + caller + three static fields. */
    mc_client_color_array *borrowed = water;
    CHECK(mc_client_color_array_release(&water, NULL) == MC_CLIENT_COLOR_ARRAY_OK && water == NULL);
    CHECK(mc_client_texture_lookup_free_metadata(o));
    borrowed->active_calls = 1;
    CHECK(mc_client_render_engine_retain(o) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_release(o) == MC_CLIENT_RENDER_ENGINE_OK && o->holds == 1);
    CHECK(mc_client_render_engine_classes_release(f->classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(f->classes->holds == 1 && borrowed->holds == 4);
    CHECK(mc_client_render_engine_release(o) == MC_CLIENT_RENDER_ENGINE_BUSY);
    CHECK(o->holds == 1 && f->classes->holds == 1 && borrowed->holds == 4);
    CHECK(mc_client_colorizer_occurrences(f->classes, borrowed) == 3 &&
          o->fields.c->color_arrays[0].array == borrowed);
    CHECK(mc_client_color_array_retain(borrowed) == MC_CLIENT_COLOR_ARRAY_OK);
    other = borrowed; /* Real external occurrence permits both parents to retire. */
    CHECK(mc_client_render_engine_release(o) == MC_CLIENT_RENDER_ENGINE_OK);
    f->engine = NULL;
    f->classes = NULL;
    CHECK(other->holds == 1 && other->active_calls == 1);
    CHECK(mc_client_color_array_release(&other, NULL) == MC_CLIENT_COLOR_ARRAY_BUSY &&
          other == borrowed);
    other->active_calls = 0;
    CHECK(mc_client_color_array_release(&other, NULL) == MC_CLIENT_COLOR_ARRAY_OK && other == NULL);
    return 0;
}

static int close_fixture(fixture *f) {
    CHECK(f->engine == NULL && f->classes == NULL); /* Combined real retirement above. */
    CHECK(mc_client_default_font_release(f->font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_texture_pack_list_release(f->packs, NULL) == MC_TEXTURE_PACK_OK);
    CHECK(mc_game_settings_retire(&f->app.settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_application_lifetime_retire(&f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_resource_files_free(f->files) == MC_REGISTRY_OK);
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->profile);
    free(f->error);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4);
    CHECK(strcmp(argv[3], "default") == 0 || strcmp(argv[3], "zip") == 0);
    fixture f;
    CHECK(open_fixture(&f, argv[1], argv[2], strcmp(argv[3], "zip") == 0) == 0);
    CHECK(contracts(&f) == 0);
    CHECK(close_fixture(&f) == 0);
    printf("Actual selected-Pack color lookup contracts: %u checks\n", checks);
    return 0;
}
