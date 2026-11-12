#include "client/render_engine/internal.h"
#include "mc/client/render_engine/render_engine.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "RenderEngine check:%d: %s\n", __LINE__, #c);                          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool empty_fields(const mc_client_render_engine_state *s) {
    return s->fields.b == NULL && s->fields.c == NULL && s->fields.d == NULL &&
           s->fields.e == NULL && s->fields.f == NULL && s->fields.g == NULL &&
           s->fields.h == NULL && s->fields.m == NULL;
}

static bool complete(bool trace) {
    mc_client_render_engine_classes *classes = NULL;
    mc_client_default_font *font = NULL;
    mc_client_render_engine *owner = NULL, *other = NULL;
    mc_game_settings *settings = NULL;
    mc_client_render_engine_state s, t;
    mc_client_render_engine_error error;
    mc_client_render_engine_class_state c;
    mc_client_font_snapshot f;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_render_engine_allocate(classes, font, NULL, &owner) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_inspect(owner, &s) && s.phase == MC_CLIENT_RENDER_ENGINE_RAW &&
          empty_fields(&s));
    CHECK(mc_client_render_engine_classes_inspect(classes, &c) && !c.ji_ready && c.ge_a == NULL &&
          c.ge_b == NULL);
    CHECK(mc_client_default_font_inspect(font, &f) && !f.ready && f.font_bytes == 0 &&
          f.native_holds == 2);
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_render_engine_construct(owner, NULL, settings, &error) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(error.status == MC_CLIENT_RENDER_ENGINE_OK && error.operation == NULL);
    CHECK(mc_client_render_engine_inspect(owner, &s) && s.phase == MC_CLIENT_RENDER_ENGINE_READY &&
          s.last_store_pc == 109 && s.fields.i == settings && s.fields.l == NULL && !s.fields.j &&
          !s.fields.k && s.graphics_created && s.graphics_disposed &&
          s.graphics_color == UINT32_C(0xff000000));
    CHECK(s.constructor_allocations == 11 && s.failed_image == NULL);
    const mc_client_render_map *maps[4] = {s.fields.b, s.fields.c, s.fields.d, s.fields.h};
    size_t count = 999;
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_client_render_map_count(maps[i], &count) && count == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(maps[i] != maps[j]);
    }
    CHECK(mc_client_render_list_count(s.fields.g, &count) && count == 0);
    CHECK(mc_client_render_engine_classes_inspect(classes, &c) && c.ji_ready && !c.ji_a &&
          c.ge_phase == MC_CLIENT_RENDER_ENGINE_READY && c.ge_a != c.ge_b && c.ge_a != s.fields.g &&
          c.ge_b != s.fields.g);
    CHECK(mc_client_render_list_count(c.ge_a, &count) && count == 0 &&
          mc_client_render_list_count(c.ge_b, &count) && count == 0);
    mc_client_direct_buffer_state e, b;
    CHECK(mc_client_direct_int_buffer_inspect(s.fields.e, &e) && e.position == 0 && e.limit == 1 &&
          e.capacity == 1 && e.storage_bytes == 4 && e.direct && e.native_order &&
          e.underlying_byte_buffer != e.identity);
    CHECK(mc_client_direct_byte_buffer_inspect(s.fields.f, &b) && b.position == 0 &&
          b.limit == 1048576 && b.capacity == 1048576 && b.storage_bytes == 1048576 && b.direct &&
          b.native_order && b.storage != e.storage);
    for (size_t i = 0; i < e.storage_bytes; ++i)
        CHECK(((const uint8_t *)e.storage)[i] == 0);
    for (size_t i = 0; i < b.storage_bytes; ++i)
        if (((const uint8_t *)b.storage)[i] != 0)
            CHECK(false);
    ++checks;
    mc_client_argb_image_state image;
    CHECK(mc_client_argb_image_inspect(s.fields.m, &image) && image.constructed &&
          image.native_holds == 1 && image.type == 2 && image.width == 64 && image.height == 64 &&
          image.pixel_count == 4096);
    size_t black = 0;
    for (size_t i = 0; i < image.pixel_count; ++i) {
        CHECK(image.pixels[i] == UINT32_C(0xffffffff) || image.pixels[i] == UINT32_C(0xff000000));
        black += image.pixels[i] == UINT32_C(0xff000000);
    }
    CHECK(black > 0 && black < 512);
    CHECK(mc_client_default_font_inspect(font, &f) && f.ready && !f.failed && f.native_holds == 2 &&
          f.font_bytes > 0);
    printf("FONT %s %d.%d.%d\n", f.family, f.freetype_major, f.freetype_minor, f.freetype_patch);
    printf("IMAGE 64 64 BLACK %zu\n", black);
    CHECK(f.completed_glyphs == 10);
    for (size_t i = 0; i < 10; ++i) {
        const mc_client_font_glyph_state *g = &f.glyphs[i];
        printf("METRIC %zu %" PRIu32 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRIu32
               " %" PRIu32 "\n",
               i, g->glyph, g->origin_x, g->advance_x, g->bounds_x, g->bounds_y, g->width,
               g->height);
    }

    if (trace) {
        fputs("PIXELS", stdout);
        for (size_t i = 0; i < image.pixel_count; ++i)
            printf(" %08" PRIx32, image.pixels[i]);
        puts("");
    }
    CHECK(mc_client_render_engine_allocate(classes, font, NULL, &other) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(other, NULL, NULL, &error) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_inspect(other, &t) && t.fields.i == NULL && t.fields.l == NULL);
    CHECK(t.fields.b != s.fields.b && t.fields.c != s.fields.c && t.fields.m != s.fields.m &&
          t.fields.e != s.fields.e && t.fields.f != s.fields.f);
    CHECK(mc_client_render_engine_classes_inspect(classes, &c) && c.native_holds == 3);
    CHECK(mc_client_render_engine_construct(owner, NULL, NULL, &error) ==
          MC_CLIENT_RENDER_ENGINE_INVALID_PHASE);
    CHECK(mc_client_argb_image_retain(s.fields.m) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_argb_image *held = s.fields.m;
    CHECK(mc_client_render_engine_release(owner) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_argb_image_inspect(held, &image) && image.native_holds == 1 &&
          image.pixels != NULL);
    mc_client_argb_image_release(held);
    CHECK(mc_client_render_engine_release(other) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_default_font_inspect(font, &f) && f.native_holds == 1);
    CHECK(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
    mc_client_render_engine_classes_release(classes);
    return true;
}

static bool prefixes(void) {
    const unsigned pcs[11] = {5, 16, 27, 1, 6, 1, 55, 66, 87, 96, 116};
    const unsigned caller[11] = {5, 16, 27, 39, 39, 48, 55, 66, 87, 96, 116};
    const unsigned stores[11] = {0, 12, 23, 34, 34, 42, 51, 62, 83, 83, 109};
    for (size_t quota = 0; quota < 11; ++quota) {
        mc_client_render_engine_classes *classes = NULL;
        mc_client_default_font *font = NULL;
        mc_client_render_engine *o = NULL;
        mc_client_render_engine_options limits = mc_client_render_engine_default_options();
        limits.max_constructor_allocations = quota;
        mc_client_render_engine_error error;
        mc_client_render_engine_state s;
        mc_client_font_snapshot f;
        CHECK(mc_client_render_engine_classes_create(NULL, &classes) ==
                  MC_CLIENT_RENDER_ENGINE_OK &&
              mc_client_default_font_create(32u * 1024u * 1024u, &font) == MC_CLIENT_FONT_OK &&
              mc_client_render_engine_allocate(classes, font, &limits, &o) ==
                  MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(mc_client_render_engine_construct(o, NULL, NULL, &error) ==
                  MC_CLIENT_RENDER_ENGINE_LIMIT &&
              error.source_pc == pcs[quota] && error.ji_pc == caller[quota] &&
              ((quota != 3 && quota != 5) ||
               (error.ge_pc == 1 && error.source_class != NULL && error.ge_method != NULL &&
                strcmp(error.source_class, "ge") == 0 && strcmp(error.ge_method, "c") == 0)));
        CHECK(mc_client_render_engine_inspect(o, &s) && s.phase == MC_CLIENT_RENDER_ENGINE_FAILED &&
              s.constructor_allocations == quota && s.last_store_pc == stores[quota]);
        CHECK(mc_client_default_font_inspect(font, &f) && !f.ready && !f.failed &&
              f.font_bytes == 0);
        CHECK((s.failed_image != NULL) == (quota == 9));
        mc_client_render_engine_class_state class_state;
        CHECK(mc_client_render_engine_classes_inspect(classes, &class_state) &&
              class_state.ji_ready &&
              class_state.ge_phase ==
                  (quota >= 3 ? MC_CLIENT_RENDER_ENGINE_READY : MC_CLIENT_RENDER_ENGINE_RAW) &&
              (class_state.ge_a != NULL) == (quota >= 3) &&
              (class_state.ge_b != NULL) == (quota >= 3));
        if (quota == 10) {
            mc_client_argb_image_state image;
            CHECK(mc_client_argb_image_inspect(s.fields.m, &image) && image.pixels[0] == 0 &&
                  !s.graphics_created);
        }
        CHECK(mc_client_render_engine_release(o) == MC_CLIENT_RENDER_ENGINE_OK &&
              mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
        mc_client_render_engine_classes_release(classes);
    }
    for (size_t quota = 0; quota < 2; ++quota) {
        mc_client_render_engine_classes *classes = NULL;
        mc_client_default_font *font = NULL;
        mc_client_render_engine *a = NULL, *b = NULL;
        mc_client_render_engine_options limits = mc_client_render_engine_default_options();
        limits.max_constructor_allocations = quota;
        mc_client_render_engine_error error;
        mc_client_render_engine_class_state c;
        mc_client_render_engine_state s;
        CHECK(mc_client_render_engine_classes_create(&limits, &classes) ==
                  MC_CLIENT_RENDER_ENGINE_OK &&
              mc_client_default_font_create(0, &font) == MC_CLIENT_FONT_OK &&
              mc_client_render_engine_allocate(classes, font, NULL, &a) ==
                  MC_CLIENT_RENDER_ENGINE_OK &&
              mc_client_render_engine_allocate(classes, font, NULL, &b) ==
                  MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(mc_client_render_engine_construct(a, NULL, NULL, &error) ==
                  MC_CLIENT_RENDER_ENGINE_LIMIT &&
              error.ji_pc == 39 && error.ge_pc == (quota == 0 ? 0u : 10u) &&
              strcmp(error.ge_method, "<clinit>") == 0);
        CHECK(mc_client_render_engine_classes_inspect(classes, &c) &&
              c.ge_phase == MC_CLIENT_RENDER_ENGINE_FAILED && (c.ge_a != NULL) == (quota == 1) &&
              c.ge_b == NULL);
        CHECK(mc_client_render_engine_inspect(a, &s) && s.fields.b != NULL && s.fields.c != NULL &&
              s.fields.d != NULL && s.fields.e == NULL && s.last_store_pc == 34);
        CHECK(mc_client_render_engine_construct(b, NULL, NULL, &error) ==
                  MC_CLIENT_RENDER_ENGINE_CLASS_FAILED &&
              error.cause_status == MC_CLIENT_RENDER_ENGINE_LIMIT && error.ji_pc == 39 &&
              error.source_pc == 39 && strcmp(error.source_class, "ji") == 0);
        CHECK(mc_client_render_engine_release(a) == MC_CLIENT_RENDER_ENGINE_OK &&
              mc_client_render_engine_release(b) == MC_CLIENT_RENDER_ENGINE_OK &&
              mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
        mc_client_render_engine_classes_release(classes);
    }
    mc_client_render_engine_classes *classes = NULL;
    mc_client_default_font *font = NULL;
    mc_client_render_engine *owner = NULL;
    mc_client_render_engine_error error;
    mc_client_render_engine_state s;
    mc_client_argb_image_state image;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_default_font_create(0, &font) == MC_CLIENT_FONT_OK &&
          mc_client_render_engine_allocate(classes, font, NULL, &owner) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(owner, NULL, NULL, &error) ==
              MC_CLIENT_RENDER_ENGINE_FONT_FAILED &&
          error.ji_pc == 150 && error.font.status == MC_CLIENT_FONT_LIMIT);
    CHECK(mc_client_render_engine_inspect(owner, &s) && s.last_store_pc == 109 &&
          s.graphics_created && !s.graphics_disposed && s.graphics_color == UINT32_C(0xff000000));
    CHECK(mc_client_argb_image_inspect(s.fields.m, &image) && image.native_holds == 2);
    for (size_t i = 0; i < image.pixel_count; ++i)
        CHECK(image.pixels[i] == UINT32_C(0xffffffff));
    const mc_client_render_engine_error retained = error;
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_OK &&
          owner == NULL);
    CHECK(retained.status == MC_CLIENT_RENDER_ENGINE_FONT_FAILED &&
          retained.font.operation != NULL && retained.font.status == MC_CLIENT_FONT_LIMIT &&
          retained.ji_pc == 150);
    CHECK(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
    mc_client_render_engine_classes_release(classes);
    return true;
}

/* Native admission/occurrence guard fixtures on genuine factory-allocated
 * storage. These guard assignments are not a running source constructor,
 * successful child, real GPU resource or concurrency qualification. */
static bool lifecycle_api(void) {
    mc_client_render_engine_classes *classes = NULL;
    mc_client_render_engine *owner = NULL;
    mc_client_default_font *font = NULL;
    mc_client_render_engine_error error = {0};
    mc_client_render_engine_class_state state;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_inspect(classes, &state) && !state.ji_ready &&
          state.ge_phase == MC_CLIENT_RENDER_ENGINE_RAW && state.native_holds == 1);
    CHECK(mc_client_render_engine_classes_active_use(classes, &error) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_active_use(classes, &error) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_inspect(classes, &state) && state.ji_ready &&
          !state.ji_a && state.ge_phase == MC_CLIENT_RENDER_ENGINE_RAW && state.ge_a == NULL &&
          state.ge_b == NULL);
    CHECK(mc_client_render_engine_allocate(classes, NULL, NULL, &owner) ==
              MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT &&
          owner == NULL);
    CHECK(mc_client_render_engine_classes_inspect(classes, &state) && state.ji_ready &&
          state.native_holds == 1);
    CHECK(mc_client_default_font_create(0, &font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_render_engine_allocate(classes, font, NULL, &owner) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_retain(owner) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          owner != NULL);
    CHECK(mc_client_render_engine_release(owner) == MC_CLIENT_RENDER_ENGINE_OK);
    owner->active_resource_calls = 1;
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          owner != NULL);
    owner->active_resource_calls = 0;
    owner->phase = MC_CLIENT_RENDER_ENGINE_CONSTRUCTING;
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          owner != NULL);
    owner->phase = MC_CLIENT_RENDER_ENGINE_RAW;
    classes->active_source_calls = 1; /* Unrelated shared source-call domain. */
    CHECK(mc_client_render_engine_release(owner) == MC_CLIENT_RENDER_ENGINE_BUSY);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_BUSY);
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_OK &&
          owner == NULL);
    CHECK(atomic_load_explicit(&classes->holds, memory_order_acquire) == 1 &&
          classes->active_source_calls == 1);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_BUSY);
    classes->active_source_calls = 0;
    CHECK(mc_client_render_engine_classes_retain(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_inspect(classes, &state) && state.native_holds == 2);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_inspect(classes, &state) && state.native_holds == 1);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);

    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_create(0, &font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_render_engine_allocate(classes, font, NULL, &owner) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_release(classes) ==
          MC_CLIENT_RENDER_ENGINE_OK); /* Engine owns final class occurrence. */
    classes->active_source_calls = 1;
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          owner != NULL);
    CHECK(atomic_load_explicit(&classes->holds, memory_order_acquire) == 1);
    classes->active_source_calls = 0;
    classes->active_resource_calls = 1;
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          owner != NULL);
    classes->active_resource_calls = 0;
    classes->nw.phase = MC_CLIENT_RENDER_ENGINE_CONSTRUCTING;
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_BUSY &&
          owner != NULL);
    classes->nw.phase = MC_CLIENT_RENDER_ENGINE_RAW;
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_OK &&
          owner == NULL);
    CHECK(mc_client_render_engine_retire(&owner, &error) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_render_engine_retire(NULL, &error) == MC_CLIENT_RENDER_ENGINE_INVALID_ARGUMENT);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--trace-image") != 0)) {
        fputs("RenderEngine input: [--trace-image]\n", stderr);
        return 2;
    }
    if (!complete(argc == 2) || !prefixes() || !lifecycle_api())
        return 1;
    printf("RenderEngine constructor: %u checks\n", checks);
    return 0;
}
