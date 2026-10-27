#include "application_state.h"
#include "client/render_engine/nw.h"
#include "client/render_engine/texture_lookup_internal.h"
#include "client/texture_packs/internal.h"
#include "mc/block/registry/registry.h"
#include "mc/resources/files/files.h"
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Authored/unexecuted genuine-owner contracts. Args select real imported/
 * authored resource files and a real options.txt/texturepacks directory. Run
 * separately for actual default and selected-ZIP profiles. No ready Pack,
 * image, texture ID or successful game-child callback is supplied by fixture. */
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "lookup check%u line%d: %s\n", checks, __LINE__, #x);                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static mc_texture_pack_text text(const uint16_t *units, size_t count) {
    return (mc_texture_pack_text){units, count, false};
}

static const uint16_t background[] = {'/', 'g', 'u', 'i', '/', 'b', 'a', 'c', 'k', 'g',
                                      'r', 'o', 'u', 'n', 'd', '.', 'p', 'n', 'g'};
static const uint16_t missing[] = {'/', 'l', 'o', 'o', 'k', 'u', 'p', '-', 'a',
                                   'b', 's', 'e', 'n', 't', '.', 'p', 'n', 'g'};
static const uint16_t nonimage[] = {'/', 'f', 'o', 'n', 't', '.', 't', 'x', 't'};
static const uint16_t strips[] = {'#', '#', '/', 'g', 'u', 'i', '/', 'b', 'a', 'c', 'k',
                                  'g', 'r', 'o', 'u', 'n', 'd', '.', 'p', 'n', 'g'};
static const uint16_t blur[] = {'%', 'b', 'l', 'u', 'r', '%', '/', 'g', 'u', 'i', '/', 'b', 'a',
                                'c', 'k', 'g', 'r', 'o', 'u', 'n', 'd', '.', 'p', 'n', 'g'};
static const uint16_t clamp[] = {'%', 'c', 'l', 'a', 'm', 'p', '%', '/', 'g', 'u', 'i', '/', 'b',
                                 'a', 'c', 'k', 'g', 'r', 'o', 'u', 'n', 'd', '.', 'p', 'n', 'g'};
static const uint16_t truncated_blur[] = {'%', 'b', 'l', 'u', 'r', '%', '/', 'l', 'o',
                                          'o', 'k', 'u', 'p', '-', 't', 'r', 'u', 'n',
                                          'c', 'a', 't', 'e', 'd', '.', 'p', 'n', 'g'};

typedef struct fixture {
    client_application app; /* Genuine registered native prepared-state fixture. */
    mc_java_random_profile *profile;
    mc_materials *materials;
    mc_registry_classes *registry;
    mc_resource_files *files;
    mc_client_texture_pack_list *packs;
    mc_client_render_engine_classes *classes;
    mc_client_default_font *font;
    mc_client_render_engine *engine;
    mc_client_display *display;
    mc_client_texture_lookup_error *error;
    mc_texture_pack_kind selected_kind;
} fixture;

static int open_fixture(fixture *f, const char *resources, const char *directory,
                        mc_texture_pack_kind expected_kind) {
    memset(f, 0, sizeof(*f));
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
    mc_texture_pack_error pack_error;
    CHECK(mc_client_texture_pack_list_construct(f->packs, &f->app, directory, &pack_error) ==
          MC_TEXTURE_PACK_OK);
    mc_client_texture_pack *selected = NULL;
    mc_texture_pack_state state;
    CHECK(mc_client_texture_pack_selected(f->packs, &selected) && selected != NULL);
    CHECK(mc_client_texture_pack_inspect(selected, &state));
    f->selected_kind = state.kind;
    printf("SELECTED kind=%d\n", (int)state.kind);
    CHECK(f->selected_kind == expected_kind); /* A ZIP refusal/fallback is not ZIP parity. */
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &f->classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &f->font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_render_engine_allocate(f->classes, f->font, NULL, &f->engine) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(f->engine, f->packs, f->app.settings, NULL) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_create("Actual selected-Pack texture lookup", 64, 64, true, &f->display,
                                   NULL));
    return 0;
}

static int release_engine(fixture *f, mc_client_render_engine **engine) {
    CHECK(mc_client_render_engine_retire_lookup(*engine, f->error) == MC_CLIENT_LOOKUP_OK);
    if ((*engine)->lookup != NULL)
        CHECK(mc_client_render_engine_release(*engine) == MC_CLIENT_RENDER_ENGINE_BUSY);
    CHECK(mc_client_texture_lookup_free_metadata(*engine));
    mc_client_texture sentinel = 0xdeadbeefu;
    CHECK(mc_client_render_engine_texture(*engine, f->display, NULL,
                                          text(background, COUNT(background)), &sentinel,
                                          f->error) == MC_CLIENT_LOOKUP_INVALID_PHASE);
    CHECK(sentinel == 0xdeadbeefu);
    CHECK(mc_client_render_engine_release(*engine) == MC_CLIENT_RENDER_ENGINE_OK);
    *engine = NULL;
    return 0;
}

static int close_fixture(fixture *f) {
    CHECK(release_engine(f, &f->engine) == 0);
    CHECK(mc_client_render_engine_retire_resources(f->classes, NULL) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    CHECK(mc_client_render_engine_classes_release(f->classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_release(f->font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_display_destroy(f->display, NULL));
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

static int contracts(fixture *f) {
    mc_client_render_engine *o = f->engine;
    const size_t first_ledger = f->classes->b->count;
    size_t d_count;
    CHECK(mc_client_render_map_count(o->fields.d, &d_count) && d_count == 0);
    mc_client_texture id = 0xdeadbeefu;
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(background, COUNT(background)),
                                          &id, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(f->classes->b->count == first_ledger + 1 && o->fields.b->count == 1 &&
          o->fields.d->count == d_count);
    CHECK(o->fields.e->position == 0 && o->fields.e->limit == 1);
    mc_client_argb_image *registered = o->fields.m;
    CHECK(mc_client_render_engine_registered_image(o, id, &registered) && registered == NULL);
    uint16_t equal_key[COUNT(background)];
    memcpy(equal_key, background, sizeof(equal_key));
    mc_client_texture again = 0;
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(equal_key, COUNT(equal_key)),
                                          &again, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(again == id && f->classes->b->count == first_ledger + 1);
    equal_key[0] = 'X'; /* Actual b keeps its independent immutable content. */
    bool found;
    CHECK(mc_client_render_engine_cached_texture(o, text(background, COUNT(background)), &found,
                                                 &again) &&
          found && again == id);

    /* Declared source-state fixtures use genuine existing owners. Null l must
     * fail before this real cached entry; a genuine RAW ik with null a may hit. */
    o->fields.l = NULL;
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(background, COUNT(background)),
                                          &again, f->error) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(f->error->site.pc == 4 && f->classes->b->count == first_ledger + 1);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    o->fields.l = f->packs;
    mc_client_texture_pack_list *raw = NULL;
    CHECK(mc_client_texture_pack_list_allocate(mc_resource_files_source(f->files), NULL, &raw) ==
          MC_TEXTURE_PACK_OK);
    o->fields.l = raw;
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(background, COUNT(background)),
                                          &again, f->error) == MC_CLIENT_LOOKUP_OK &&
          again == id);
    const size_t before_null_pack = f->classes->b->count;
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(missing, COUNT(missing)),
                                          &again, f->error) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(f->error->site.pc == 171 && f->classes->b->count == before_null_pack + 1);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    o->fields.l = f->packs;
    CHECK(mc_client_texture_pack_list_release(raw, NULL) == MC_TEXTURE_PACK_OK);

    const size_t before_null_key = f->classes->b->count;
    again = 0xdeadbeefu;
    CHECK(mc_client_render_engine_texture(o, f->display, NULL,
                                          (mc_texture_pack_text){NULL, 0, true}, &again,
                                          f->error) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(f->error->site.pc == 56 && f->classes->b->count == before_null_key + 1 &&
          again == 0xdeadbeefu);
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(background, COUNT(background)),
                                          &again, f->error) == MC_CLIENT_LOOKUP_BUSY);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(missing, COUNT(missing)),
                                          &again, f->error) == MC_CLIENT_LOOKUP_OK);

    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(nonimage, COUNT(nonimage)),
                                          &again, f->error) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(f->error->site.pc == 201 && f->error->texture.source_pc == 154);
    mc_client_texture_lookup_state state;
    mc_texture_stream_state stream;
    CHECK(mc_client_render_engine_lookup_inspect(o, &state) && state.pending &&
          state.image_io_returned && state.stream != NULL);
    CHECK(mc_client_texture_stream_inspect(state.stream, &stream) && stream.source_closed &&
          stream.source_close_calls == 1);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(strips, COUNT(strips)), &again,
                                          f->error) == MC_CLIENT_LOOKUP_UNIMPLEMENTED);
    CHECK(f->error->site.pc == 77);
    CHECK(mc_client_render_engine_lookup_inspect(o, &state) && state.image != NULL &&
          state.source_closed && !state.cache_published);
    if (f->selected_kind == MC_TEXTURE_PACK_ZIP) {
        mc_client_argb_image_state image;
        CHECK(mc_client_argb_image_inspect(state.image, &image));
        CHECK(image.width == 7 && image.height == 3); /* Declared authored ZIP fixture. */
        for (uint32_t y = 0; y < image.height; ++y)
            for (uint32_t x = 0; x < image.width; ++x) {
                const uint32_t expected =
                    UINT32_C(0xff000000) | ((x * 31u) << 16) | ((y * 67u) << 8) | (x + y);
                CHECK(image.pixels[(size_t)y * image.width + x] == expected);
            }
    }
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(blur, COUNT(blur)), &again,
                                          f->error) == MC_CLIENT_LOOKUP_OK &&
          !o->fields.k);
    const size_t before_io = f->classes->b->count;
    CHECK(mc_client_render_engine_texture(o, f->display, NULL,
                                          text(truncated_blur, COUNT(truncated_blur)), &again,
                                          f->error) == MC_CLIENT_LOOKUP_UNIMPLEMENTED);
    CHECK(f->error->site.pc == 221 && f->error->caught_source_io && o->fields.k);
    CHECK(f->classes->b->count == before_io + 1); /* No fabricated diagnostic/second ge. */
    CHECK(mc_client_render_engine_lookup_inspect(o, &state) && state.stream != NULL &&
          !state.source_closed);
    CHECK(mc_client_texture_stream_inspect(state.stream, &stream) && !stream.source_closed &&
          stream.source_close_calls == 0);
    mc_client_texture_stream *live = (mc_client_texture_stream *)(void *)state.stream;
    live->active = true; /* Declared native in-flight guard fixture, not a read callback. */
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_BUSY);
    CHECK(mc_client_render_engine_lookup_inspect(o, &state) && state.stream == live &&
          !state.native_stream_consumed);
    live->active = false;
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(clamp, COUNT(clamp)), &again,
                                          f->error) == MC_CLIENT_LOOKUP_UNIMPLEMENTED);
    CHECK(f->error->site.pc == 117 && f->error->texture.source_pc == 111 && o->fields.j &&
          o->fields.k);
    CHECK(mc_client_render_engine_retire_lookup(o, f->error) == MC_CLIENT_LOOKUP_OK);
    CHECK(mc_client_render_engine_texture(o, f->display, NULL, text(background, COUNT(background)),
                                          &again, f->error) == MC_CLIENT_LOOKUP_OK &&
          again == id && o->fields.j);
    CHECK(o->fields.d->count == d_count);

    mc_client_render_engine *limited = NULL;
    CHECK(mc_client_render_engine_allocate(f->classes, f->font, NULL, &limited) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(limited, f->packs, f->app.settings, NULL) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_texture_lookup_options options = mc_client_texture_lookup_default_options();
    options.max_cache_entries = 0;
    const size_t before_limit = f->classes->b->count;
    again = 0xdeadbeefu;
    CHECK(mc_client_render_engine_texture(limited, f->display, &options,
                                          text(background, COUNT(background)), &again,
                                          f->error) == MC_CLIENT_LOOKUP_LIMIT);
    CHECK(f->error->site.pc == 213 && f->error->native_policy && again == 0xdeadbeefu);
    CHECK(f->classes->b->count == before_limit + 1 && limited->fields.b->count == 0 &&
          limited->fields.d->count == 0);
    CHECK(release_engine(f, &limited) == 0);
    mc_client_render_engine *raw_engine = NULL;
    CHECK(mc_client_render_engine_allocate(f->classes, f->font, NULL, &raw_engine) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_texture_lookup_free_metadata(
        raw_engine)); /* Explicit final native stop on RAW. */
    CHECK(mc_client_render_engine_construct(raw_engine, f->packs, f->app.settings, NULL) ==
          MC_CLIENT_RENDER_ENGINE_INVALID_PHASE);
    CHECK(raw_engine->phase == MC_CLIENT_RENDER_ENGINE_RAW && raw_engine->fields.b == NULL &&
          raw_engine->arena.allocations == 0);
    CHECK(mc_client_render_engine_release(raw_engine) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4);
    CHECK(strcmp(argv[3], "default") == 0 || strcmp(argv[3], "zip") == 0);
    const mc_texture_pack_kind expected_kind =
        strcmp(argv[3], "zip") == 0 ? MC_TEXTURE_PACK_ZIP : MC_TEXTURE_PACK_DEFAULT;
    fixture f;
    CHECK(open_fixture(&f, argv[1], argv[2], expected_kind) == 0);
    CHECK(contracts(&f) == 0);
    CHECK(close_fixture(&f) == 0);
    printf("Actual selected-Pack lookup contracts: %u checks\n", checks);
    return 0;
}
