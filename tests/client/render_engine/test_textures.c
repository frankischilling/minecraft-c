#include "../../../src/client/render_engine/internal.h"
#include "../../../src/client/render_engine/textures.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <SDL3/SDL_video.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static bool reference_trace;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "RenderTexture check:%d: %s\n", __LINE__, #c);                         \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
typedef GLboolean(GLAPIENTRY *is_texture_fn)(GLuint);
typedef void(GLAPIENTRY *get_integer_fn)(GLenum, GLint *);
typedef void(GLAPIENTRY *get_parameter_fn)(GLenum, GLenum, GLint *);
typedef void(GLAPIENTRY *get_image_fn)(GLenum, GLint, GLenum, GLenum, GLvoid *);
typedef void(GLAPIENTRY *enable_fn)(GLenum);
typedef void(GLAPIENTRY *pixel_store_fn)(GLenum, GLint);
typedef void(GLAPIENTRY *bind_texture_fn)(GLenum, GLuint);
static is_texture_fn is_texture;
static get_integer_fn get_integer;
static get_parameter_fn get_parameter;
static get_image_fn get_image;

typedef struct fixture {
    mc_client_display *display;
    mc_client_render_engine_classes *classes;
    mc_client_default_font *font;
    mc_client_render_engine *engine;
    mc_game_settings *settings;
} fixture;

static bool scene(fixture *f, mc_client_display *display, bool null_settings) {
    f->display = display;
    CHECK(mc_client_render_engine_classes_create(NULL, &f->classes) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_default_font_create(32u * 1024u * 1024u, &f->font) == MC_CLIENT_FONT_OK);
    if (!null_settings)
        CHECK(mc_game_settings_create_defaults(NULL, &f->settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_render_engine_allocate(f->classes, f->font, NULL, &f->engine) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(f->engine, NULL, f->settings, NULL) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    return true;
}

static bool finish(fixture *f) {
    mc_client_render_texture_error e;
    CHECK(mc_client_render_engine_retire_resources(f->classes, &e) == MC_CLIENT_RENDER_TEXTURE_OK);
    mc_client_render_engine_class_state c;
    CHECK(mc_client_render_engine_classes_inspect(f->classes, &c) && c.native_resource_count == 0 &&
          c.active_resource_calls == 0 && c.resource_retirement_started);
    CHECK(mc_client_render_engine_release(f->engine) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_release(f->classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_release(f->font) == MC_CLIENT_FONT_OK);
    CHECK(mc_game_settings_retire(&f->settings, NULL) == MC_GAME_SETTINGS_OK);
    return true;
}

static bool pattern(mc_client_argb_image **out) {
    CHECK(mc_client_argb_image_create(2, 2, 4096, out) == MC_CLIENT_RENDER_ENGINE_OK);
    const uint32_t colors[4] = {UINT32_C(0xff123456), UINT32_C(0x80ff0000), UINT32_C(0x00010203),
                                UINT32_C(0x4000ff00)};
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            CHECK(mc_client_argb_image_set_pixel(*out, x, y, colors[y * 2 + x]) ==
                  MC_CLIENT_RENDER_ENGINE_OK);
    return true;
}

static bool readback(mc_client_texture id, const uint8_t *expected, size_t bytes) {
    GLint bound = 0;
    get_integer(GL_TEXTURE_BINDING_2D, &bound);
    CHECK((uint32_t)bound == id && is_texture(id) == GL_TRUE);
    uint8_t pixels[65536];
    CHECK(bytes <= sizeof(pixels));
    get_image(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    CHECK(memcmp(pixels, expected, bytes) == 0);
    return true;
}

static bool trace_state(const char *label, fixture *f, mc_client_texture id, const char *outcome) {
    if (!reference_trace)
        return true;
    mc_client_direct_buffer_state e, b;
    size_t ledger = 0, map = 0;
    CHECK(mc_client_direct_int_buffer_inspect(f->engine->fields.e, &e) &&
          mc_client_direct_byte_buffer_inspect(f->engine->fields.f, &b) &&
          mc_client_render_list_count(f->classes->b, &ledger) &&
          mc_client_render_map_count(f->engine->fields.d, &map));
    GLint bound = 0, min = 0, mag = 0;
    get_integer(GL_TEXTURE_BINDING_2D, &bound);
    get_parameter(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min);
    get_parameter(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag);
    printf("REG %s %s %zu %zu %zu %zu %zu %zu %d %d %d %d\n", label, outcome, ledger, map,
           e.position, e.limit, b.position, b.limit, (uint32_t)bound == id,
           is_texture(id) == GL_TRUE, min, mag);
    if (strcmp(outcome, "return") == 0) {
        uint8_t pixels[16];
        get_image(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        printf("GPU %s", label);
        for (size_t i = 0; i < 16; ++i)
            printf(" %02x", pixels[i]);
        puts("");
        printf("TRANSFER %s", label);
        for (size_t i = 0; i < b.limit; ++i)
            printf(" %02x", ((const uint8_t *)b.storage)[i]);
        puts("");
    }
    return true;
}

static bool ordinary(mc_client_display *display) {
    fixture f = {0};
    mc_client_argb_image *image = NULL;
    CHECK(scene(&f, display, false) && pattern(&image));
    mc_client_render_texture_error e;
    mc_client_texture out = UINT32_C(0xdeadbeef);
    CHECK(mc_client_render_engine_register_image(f.engine, display, NULL, NULL, &out, &e) ==
              MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE &&
          e.source_pc == 154 && e.register_pc == 27 && e.driver_generated &&
          e.source_ledger_appended && !e.base_uploaded && !e.image_published &&
          out == UINT32_C(0xdeadbeef));
    GLint bound = 0, min = 0, mag = 0;
    get_integer(GL_TEXTURE_BINDING_2D, &bound);
    get_parameter(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min);
    get_parameter(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag);
    CHECK((uint32_t)bound == e.generated_id && is_texture(e.generated_id) == GL_TRUE &&
          min == GL_NEAREST && mag == GL_NEAREST);
    CHECK(trace_state("null", &f, e.generated_id, "null"));
    const mc_client_texture failed = e.generated_id;
    mc_client_display_error d;
    CHECK(!mc_client_display_destroy(display, &d) && d.status == MC_CLIENT_DISPLAY_BUSY);
    CHECK(mc_client_render_engine_register_image(f.engine, display, NULL, image, &out, &e) ==
              MC_CLIENT_RENDER_TEXTURE_OK &&
          e.image_published && e.base_uploaded && out != failed);
    const uint8_t rgba[16] = {0x12, 0x34, 0x56, 0xff, 0xff, 0,    0, 0x80,
                              1,    2,    3,    0,    0,    0xff, 0, 0x40};
    CHECK(readback(out, rgba, 16) && trace_state("plain", &f, out, "return"));
    CHECK(mc_client_argb_image_set_pixel(image, 0, 0, UINT32_C(0xffabcdef)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_argb_image_state changed;
    CHECK(mc_client_argb_image_inspect(image, &changed) &&
          changed.pixels[0] == UINT32_C(0xffabcdef));
    CHECK(readback(out, rgba, 16)); /* Source image identity is mutable; GPU copy is distinct. */
    CHECK(mc_client_argb_image_set_pixel(image, 0, 0, UINT32_C(0xff123456)) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_argb_image *cached = NULL;
    mc_client_argb_image_state state;
    CHECK(mc_client_render_engine_registered_image(f.engine, out, &cached) && cached == image &&
          mc_client_argb_image_inspect(image, &state) && state.native_holds == 2);
    mc_client_direct_buffer_state buffer;
    CHECK(mc_client_direct_byte_buffer_inspect(f.engine->fields.f, &buffer) &&
          buffer.position == 0 && buffer.limit == 16 && memcmp(buffer.storage, rgba, 16) == 0);
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f.settings, &settings));
    settings.values.anaglyph = true;
    CHECK(mc_game_settings_set_values(f.settings, &settings.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_render_engine_register_image(f.engine, display, NULL, image, &out, &e) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    uint8_t anaglyph[16];
    for (size_t i = 0; i < 16; i += 4) {
        const unsigned r = rgba[i], g = rgba[i + 1], b = rgba[i + 2];
        anaglyph[i] = (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
        anaglyph[i + 1] = (uint8_t)((r * 30 + g * 70) / 100);
        anaglyph[i + 2] = (uint8_t)((r * 30 + b * 70) / 100);
        anaglyph[i + 3] = rgba[i + 3];
    }
    CHECK(readback(out, anaglyph, 16) && trace_state("anaglyph", &f, out, "return"));
    CHECK(mc_client_argb_image_inspect(image, &state) && state.native_holds == 3);
    mc_client_render_engine *other = NULL;
    CHECK(mc_client_render_engine_allocate(f.classes, f.font, NULL, &other) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(other, NULL, NULL, NULL) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_register_image(other, display, NULL, image, &out, &e) ==
              MC_CLIENT_RENDER_TEXTURE_OK &&
          e.image_published);
    fixture alias = f;
    alias.engine = other;
    CHECK(readback(out, rgba, 16) && trace_state("null-settings", &alias, out, "return"));
    CHECK(mc_client_render_engine_release(other) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(finish(&f));
    CHECK(is_texture(failed) == GL_FALSE && is_texture(out) == GL_FALSE);
    CHECK(mc_client_argb_image_inspect(image, &state) && state.native_holds == 1);
    mc_client_argb_image_release(image);
    return true;
}

static bool failed_prefixes(mc_client_display *display) {
    for (int scenario = 0; scenario < 6; ++scenario) {
        fixture f = {0};
        mc_client_argb_image *image = NULL;
        CHECK(scene(&f, display, true) && pattern(&image));
        mc_client_render_texture_options options = mc_client_render_texture_default_options();
        if (scenario == 0)
            options.max_source_ledger_entries = 0;
        if (scenario == 1)
            options.max_image_entries = 0;
        if (scenario == 2)
            options.max_upload_scratch_bytes = 0;
        if (scenario == 3)
            options.max_upload_scratch_bytes = 16;
        if (scenario == 4)
            f.engine->fields.j = true;
        if (scenario == 5)
            f.classes->ji_a = true;
        mc_client_render_texture_error e;
        mc_client_texture id = UINT32_C(0xdeadbeef);
        const mc_client_render_texture_status status =
            mc_client_render_engine_register_image(f.engine, display, &options, image, &id, &e);
        const unsigned pcs[6] = {28, 39, 168, 178, 111, 460};
        const mc_client_render_texture_status statuses[6] = {
            MC_CLIENT_RENDER_TEXTURE_LIMIT,         MC_CLIENT_RENDER_TEXTURE_LIMIT,
            MC_CLIENT_RENDER_TEXTURE_LIMIT,         MC_CLIENT_RENDER_TEXTURE_LIMIT,
            MC_CLIENT_RENDER_TEXTURE_UNIMPLEMENTED, MC_CLIENT_RENDER_TEXTURE_UNIMPLEMENTED};
        CHECK(status == statuses[scenario] && e.source_pc == pcs[scenario] && e.driver_generated &&
              !e.image_published && id == UINT32_C(0xdeadbeef));
        CHECK(e.source_ledger_appended == (scenario != 0) &&
              e.base_uploaded == (scenario == 1 || scenario == 5));
        CHECK(is_texture(e.generated_id) == (scenario == 0 ? GL_FALSE : GL_TRUE));
        mc_client_argb_image_state held;
        CHECK(mc_client_argb_image_inspect(image, &held) && held.native_holds == 1);
        CHECK(!mc_client_display_destroy(display, &e.display) &&
              e.display.status == MC_CLIENT_DISPLAY_BUSY);
        const mc_client_texture generated = e.generated_id;
        CHECK(finish(&f) && is_texture(generated) == GL_FALSE);
        mc_client_argb_image_release(image);
    }
    fixture f = {0};
    mc_client_argb_image *large = NULL;
    CHECK(scene(&f, display, true) &&
          mc_client_argb_image_create(512, 513, 2u * 1024u * 1024u, &large) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_texture_error e;
    mc_client_texture id = UINT32_C(0xdeadbeef);
    CHECK(mc_client_render_engine_register_image(f.engine, display, NULL, large, &id, &e) ==
              MC_CLIENT_RENDER_TEXTURE_BUFFER_OVERFLOW &&
          e.source_pc == 411 && e.source_ledger_appended && !e.base_uploaded &&
          !e.image_published && id == UINT32_C(0xdeadbeef));
    mc_client_direct_buffer_state b;
    CHECK(mc_client_direct_byte_buffer_inspect(f.engine->fields.f, &b) && b.position == 0 &&
          b.limit == 1048576);
    CHECK(finish(&f));
    mc_client_argb_image_release(large);
    return true;
}

static bool retry_final_reference(mc_client_display *display) {
    fixture f = {0};
    mc_client_argb_image *image = NULL;
    mc_client_texture id = 0;
    mc_client_render_texture_error e;
    CHECK(scene(&f, display, true) && pattern(&image));
    CHECK(mc_client_render_engine_register_image(f.engine, display, NULL, image, &id, &e) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    CHECK(mc_client_render_engine_classes_release(f.classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_release(f.engine) == MC_CLIENT_RENDER_ENGINE_BUSY);
    mc_client_render_engine_state state;
    CHECK(mc_client_render_engine_inspect(f.engine, &state) && state.native_holds == 1 &&
          state.phase == MC_CLIENT_RENDER_ENGINE_READY);
    CHECK(is_texture(id) == GL_TRUE);
    CHECK(mc_client_render_engine_retire_resources(f.classes, &e) == MC_CLIENT_RENDER_TEXTURE_OK);
    mc_client_texture preserved = UINT32_C(0xdeadbeef);
    CHECK(mc_client_render_engine_register_image(f.engine, display, NULL, image, &preserved, &e) ==
              MC_CLIENT_RENDER_TEXTURE_INVALID_PHASE &&
          preserved == UINT32_C(0xdeadbeef));
    CHECK(is_texture(id) == GL_FALSE &&
          mc_client_render_engine_release(f.engine) == MC_CLIENT_RENDER_ENGINE_OK);
    /* Engine owned the final class reference; it has now retired that real class. */
    CHECK(mc_client_default_font_release(f.font) == MC_CLIENT_FONT_OK &&
          mc_game_settings_retire(&f.settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_client_argb_image_state image_state;
    CHECK(mc_client_argb_image_inspect(image, &image_state) && image_state.native_holds == 1);
    mc_client_argb_image_release(image);
    return true;
}

typedef struct callback_state {
    mc_client_display *display;
    mc_client_texture_resource **resource;
    unsigned calls;
    bool busy;
} callback_state;

static void GLAPIENTRY debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                      GLsizei length, const GLchar *message, const void *context) {
    (void)source;
    (void)id;
    (void)severity;
    (void)length;
    (void)message;
    if (type != GL_DEBUG_TYPE_ERROR)
        return;
    callback_state *s = (callback_state *)(void *)context;
    ++s->calls;
    mc_client_display_error e;
    mc_client_texture_resource *before = *s->resource;
    const bool retired = mc_client_display_retire_texture(s->resource, &e);
    const bool busy = !retired && e.status == MC_CLIENT_DISPLAY_BUSY && *s->resource == before;
    const bool destroyed = mc_client_display_destroy(s->display, &e);
    s->busy = busy && !destroyed && e.status == MC_CLIENT_DISPLAY_BUSY;
}

static bool direct_resources(mc_client_display *display) {
    mc_client_texture value = 0;
    mc_client_texture_generation generated = {0};
    mc_client_display_error e;
    CHECK(mc_client_display_generate_texture(display, &value, &generated, &e) &&
          generated.resource != NULL && generated.driver_generated &&
          value == generated.buffer_value);
    CHECK(is_texture(value) == GL_FALSE);
    CHECK(mc_client_display_bind_texture(generated.resource, &e) && is_texture(value) == GL_TRUE);
    PFNGLDEBUGMESSAGECALLBACKPROC debug =
        (PFNGLDEBUGMESSAGECALLBACKPROC)SDL_GL_GetProcAddress("glDebugMessageCallback");
    enable_fn enable = (enable_fn)SDL_GL_GetProcAddress("glEnable");
    callback_state callback = {display, &generated.resource, 0, false};
    if (debug != NULL && enable != NULL) {
        enable(GL_DEBUG_OUTPUT);
        enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        debug(debug_callback, &callback);
        CHECK(!mc_client_display_texture_parameter(generated.resource, MC_CLIENT_TEXTURE_MIN_FILTER,
                                                   0, &e) &&
              e.status == MC_CLIENT_DISPLAY_GL_ERROR);
        debug(NULL, NULL);
        CHECK(callback.calls > 0 && callback.busy);
    }
    CHECK(mc_client_display_texture_parameter(generated.resource, MC_CLIENT_TEXTURE_MIN_FILTER,
                                              GL_NEAREST, &e));
    mc_client_texture second_value = 0;
    mc_client_texture_generation second = {0};
    CHECK(mc_client_display_generate_texture(display, &second_value, &second, &e) &&
          mc_client_display_bind_texture(second.resource, &e));
    CHECK(!mc_client_display_texture_parameter(generated.resource, MC_CLIENT_TEXTURE_MAG_FILTER,
                                               GL_NEAREST, &e) &&
          e.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    GLint bound = 0;
    get_integer(GL_TEXTURE_BINDING_2D, &bound);
    CHECK((GLuint)bound == second_value); /* No implicit bind or restoration. */
    CHECK(mc_client_display_retire_texture(&second.resource, &e) && second.resource == NULL &&
          mc_client_display_bind_texture(generated.resource, &e));
    pixel_store_fn pixel_store = (pixel_store_fn)SDL_GL_GetProcAddress("glPixelStorei");
    bind_texture_fn bind_texture = (bind_texture_fn)SDL_GL_GetProcAddress("glBindTexture");
    CHECK(pixel_store != NULL && bind_texture != NULL);
    const uint8_t pixels[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const mc_image_view view = {2, 2, 8, pixels};
    pixel_store(GL_UNPACK_ROW_LENGTH, 1);
    CHECK(!mc_client_display_upload_existing_texture(generated.resource, &view, &e) &&
          e.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
    GLint row_length = 0;
    get_integer(GL_UNPACK_ROW_LENGTH, &row_length);
    CHECK(row_length == 1);               /* Admission does not silently reset source GL state. */
    pixel_store(GL_UNPACK_ROW_LENGTH, 0); /* Explicit test-owner reset. */
    CHECK(mc_client_display_upload_existing_texture(generated.resource, &view, &e) &&
          readback(value, pixels, sizeof(pixels)));
    CHECK(mc_client_display_retire_texture(&generated.resource, &e) && generated.resource == NULL &&
          is_texture(value) == GL_FALSE);
    CHECK(mc_client_display_retire_texture(&generated.resource, &e));
    /* The real driver still writes its Gen output while a prior real GL error
     * is pending. The native error path must retain that uncertain generated
     * name/context owner and must not report successful driver generation. */
    bind_texture(0, 0); /* Real GL_INVALID_ENUM, deliberately not consumed here. */
    mc_client_texture partial_value = 0;
    mc_client_texture_generation partial = {0};
    CHECK(!mc_client_display_generate_texture(display, &partial_value, &partial, &e) &&
          e.status == MC_CLIENT_DISPLAY_GL_ERROR && !partial.driver_generated &&
          partial.resource != NULL && partial_value != 0 && partial_value == partial.buffer_value);
    CHECK(!mc_client_display_destroy(display, &e) && e.status == MC_CLIENT_DISPLAY_BUSY);
    CHECK(mc_client_display_retire_texture(&partial.resource, &e) && partial.resource == NULL);
    printf("REAL_DEBUG_CALLBACK %u\n", callback.calls);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--reference") == 0)
        reference_trace = true;
    if (argc != 1 && !reference_trace) {
        fputs("RenderTexture input: [--reference]\n", stderr);
        return 2;
    }
    mc_client_display *display = NULL;
    mc_client_display_error e;
    if (!mc_client_display_create("Real RenderEngine ownership", 64, 64, true, &display, &e))
        return 1;
    is_texture = (is_texture_fn)SDL_GL_GetProcAddress("glIsTexture");
    get_integer = (get_integer_fn)SDL_GL_GetProcAddress("glGetIntegerv");
    get_parameter = (get_parameter_fn)SDL_GL_GetProcAddress("glGetTexParameteriv");
    get_image = (get_image_fn)SDL_GL_GetProcAddress("glGetTexImage");
    if (is_texture == NULL || get_integer == NULL || get_parameter == NULL || get_image == NULL)
        return 1;
    if ((!reference_trace && !direct_resources(display)) || !ordinary(display) ||
        (!reference_trace && (!failed_prefixes(display) || !retry_final_reference(display))))
        return 1;
    if (!mc_client_display_destroy(display, &e))
        return 1;
    printf("RenderTexture ownership: %u checks\n", checks);
    return 0;
}
