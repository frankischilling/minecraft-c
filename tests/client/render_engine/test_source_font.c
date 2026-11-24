#include "../../../src/client/render_engine/texture_lookup.h"
/* Compile the exact production Display TU once. This white-box control forwards
 * every intercepted GL call to its genuine function before testing reentry.
 * Do not also link another display.c object into this executable. */
#include "../../../src/client/application_state.h"
#include "../../../src/client/display/camera_surface.h"
#include "../../../src/client/display/display.c"
#include "../../../src/client/gui/bitmap_font_internal.h"
#include "../../../src/client/render_engine/ge.h"
#include "../../../src/client/render_engine/internal.h"
#include "../../../src/client/startup/application_owner.h"
#include "mc/block/registry/runtime.h"
#include "mc/resources/files/files.h"
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <xmmintrin.h>

static unsigned probe_checks;
#define REQUIRE(c)                                                                                 \
    do {                                                                                           \
        ++probe_checks;                                                                            \
        if (!(c)) {                                                                                \
            fprintf(stderr, "source font check %u failed at line %d: %s\n", probe_checks,          \
                    __LINE__, #c);                                                                 \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

typedef struct probe_source_prefix {
    float projection[16], modelview[16], color[4], pending_color[4];
    int32_t mode;
    size_t modelview_depth, projection_depth;
    mc_client_source_attribute_profile attribute_profile;
    uint32_t pending_enabled, pending_flags;
    bool color_known, pending_color_known;
} probe_source_prefix;

static bool probe_prefix(mc_client_display *display, probe_source_prefix *out) {
    mc_client_source_draw_state draw = {0};
    if (!mc_client_display_source_draw_inspect(display, &draw) ||
        !mc_client_display_camera_matrices_inspect(display, out->projection, out->modelview) ||
        !mc_client_display_camera_stack_inspect(display, &out->mode, &out->modelview_depth,
                                                &out->projection_depth))
        return false;
    memcpy(out->color, draw.color, sizeof(out->color));
    out->color_known = draw.color_known;
    /* Passive reads retain Mesa's pending attributes without committing them. */
    out->attribute_profile = display->source.attribute_profile;
    out->pending_enabled = display->source.attribute_staging.enabled;
    out->pending_flags = display->source.attribute_staging.flags;
    memcpy(out->pending_color, display->source.attribute_staging.color, sizeof(out->pending_color));
    out->pending_color_known = display->source.attribute_staging.color_known;
    return true;
}

static bool probe_prefix_equal(const probe_source_prefix *left, const probe_source_prefix *right) {
    return left->mode == right->mode && left->modelview_depth == right->modelview_depth &&
           left->projection_depth == right->projection_depth &&
           left->color_known == right->color_known &&
           left->attribute_profile == right->attribute_profile &&
           left->pending_enabled == right->pending_enabled &&
           left->pending_flags == right->pending_flags &&
           left->pending_color_known == right->pending_color_known &&
           memcmp(left->projection, right->projection, sizeof(left->projection)) == 0 &&
           memcmp(left->modelview, right->modelview, sizeof(left->modelview)) == 0 &&
           memcmp(left->color, right->color, sizeof(left->color)) == 0 &&
           memcmp(left->pending_color, right->pending_color, sizeof(left->pending_color)) == 0;
}

static mc_client_gl_texture_image_fn probe_real_image;
static mc_client_gl_draw_arrays_fn probe_real_draw;
static mc_client_bitmap_font **probe_slot;
static unsigned probe_callbacks;
static bool probe_callback_ok = true;
static mc_registry_classes *probe_registry;
static const mc_player_string_view *probe_view;

static void probe_reentry(void) {
    ++probe_callbacks;
    mc_client_bitmap_font *before = *probe_slot;
    mc_client_bitmap_font_error error = {0};
    const mc_client_bitmap_font_status released = mc_client_bitmap_font_release(probe_slot, &error);
    if (released != MC_CLIENT_BITMAP_FONT_BUSY || *probe_slot != before) {
        probe_callback_ok = false;
        return;
    }
    if (mc_client_bitmap_font_retain(before) != MC_CLIENT_BITMAP_FONT_BUSY)
        probe_callback_ok = false;
    if (probe_registry != NULL &&
        mc_registry_classes_free(probe_registry, NULL) != MC_REGISTRY_BUSY)
        probe_callback_ok = false;
    if (probe_view != NULL) {
        int32_t untouched = -77;
        if (mc_client_bitmap_font_measure_view(before, probe_view, &untouched, NULL) !=
                MC_CLIENT_BITMAP_FONT_BUSY ||
            untouched != -77)
            probe_callback_ok = false;
    }
}

static void GLAPIENTRY probe_image(GLenum target, GLint level, GLint internal_format, GLsizei width,
                                   GLsizei height, GLint border, GLenum format, GLenum type,
                                   const GLvoid *pixels) {
    probe_real_image(target, level, internal_format, width, height, border, format, type, pixels);
    probe_reentry();
}

static void GLAPIENTRY probe_draw(GLenum mode, GLint first, GLsizei count) {
    probe_real_draw(mode, first, count);
    probe_reentry();
}

static bool probe_string(const char *ascii, mc_player_string **out) {
    uint16_t units[64];
    const size_t length = strlen(ascii);
    if (length > sizeof(units) / sizeof(units[0]))
        return false;
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint8_t)ascii[i];
    return mc_player_string_create(units, length, out) == MC_PLAYER_OK;
}

static void probe_hex(const char *value, size_t capacity) {
    for (size_t i = 0; i < capacity && value[i] != '\0'; ++i)
        printf("%02x", (unsigned)(uint8_t)value[i]);
}

static mc_client_display_list_range *probe_range(mc_client_display *display, int32_t base) {
    for (mc_client_display_list_range *range = display->source.ranges; range != NULL;
         range = range->next)
        if (range->base == base)
            return range;
    return NULL;
}

/* Inspect physical immutable production commands, not only sj's counters. */
static bool probe_commands(mc_client_bitmap_font *font, unsigned glyphs, unsigned palettes) {
    mc_client_display_list_range *range = probe_range(font->display, font->fields.c);
    if (range == NULL || range->count != 288)
        return false;
    mc_client_nw_state nw;
    if (!mc_client_nw_inspect(font->gui->graphics, &nw))
        return false;
    for (unsigned i = 0; i < 288; ++i) {
        const mc_client_source_list *list = &range->lists[i];
        if (i < glyphs) {
            const mc_client_source_command *draw = list->first;
            const mc_client_source_command *translate = draw == NULL ? NULL : draw->next;
            if (!list->defined || list->building || draw == NULL || translate == NULL ||
                draw->kind != SOURCE_COMMAND_DRAW || translate->kind != SOURCE_COMMAND_TRANSLATE ||
                translate->next != NULL || list->last != translate || draw->value.draw.count != 6 ||
                draw->value.draw.size != 6u * 32u || draw->value.draw.bytes == nw.fields.d->data ||
                translate->value.vector[0] != (i == 32 ? 4.0f : 9.0f) ||
                translate->value.vector[1] != 0.0f || translate->value.vector[2] != 0.0f)
                return false;
        } else if (i >= 256 && i < 256 + palettes) {
            if (!list->defined || list->building || list->first == NULL ||
                list->first != list->last || list->first->next != NULL ||
                list->first->kind != SOURCE_COMMAND_COLOR)
                return false;
        } else if (list->defined || list->building || list->first != NULL || list->last != NULL) {
            return false;
        }
    }
    return true;
}

static bool probe_pixel(const mc_client_capture *capture, int x, int y, uint8_t value) {
    const size_t px = (size_t)x * (size_t)capture->width / 64u;
    const size_t py = (size_t)y * (size_t)capture->height / 64u;
    if (px >= (size_t)capture->width || py >= (size_t)capture->height)
        return false;
    const uint8_t *pixel = capture->pixels + py * capture->stride + px * 4u;
    return pixel[0] == value && pixel[1] == value && pixel[2] == value;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("one authored resource directory is required\n", stderr);
        return 2;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("strict FP controls are required; no implicit repair\n", stderr);
        return 2;
    }
    int result = 1;
    mc_resource_files *files = NULL;
    mc_java_random_profile *label_profile = NULL;
    mc_materials *label_materials = NULL;
    mc_registry_classes *label_registry = NULL;
    bool label_registry_held = false;
    mc_client_capture legacy_capture = {0};
    mc_client_display *display = NULL;
    mc_client_render_engine_classes *classes = NULL;
    mc_client_default_font *default_font = NULL;
    mc_client_render_engine *engine = NULL;
    mc_game_settings *settings = NULL;
    mc_client_loading_classes *gui = NULL;
    mc_client_bitmap_font *ready = NULL, *failed = NULL, *other = NULL, *alias = NULL;
    client_application *app = calloc(1, sizeof(*app));
    mc_client_loading_screen *screen = NULL;
    mc_client_loading_error *loading_error = calloc(1, sizeof(*loading_error));
    mc_client_application_owner_error app_error;
    bool registered = false;
    mc_player_string *name = NULL, *bad_name = NULL, *text = NULL, *unsafe_text = NULL;
    mc_client_bitmap_font_error *error = calloc(1, sizeof(*error));
    mc_client_bitmap_font_state *state = calloc(1, sizeof(*state));
    mc_client_display_error device_error = {0};
    mc_client_graphics_error graphics_error = {0};
    mc_client_render_texture_error texture_error = {0};
    mc_client_capture capture = {0};
    bool lease_entered = false;
    REQUIRE(error != NULL && state != NULL && app != NULL && loading_error != NULL);
    REQUIRE(mc_resource_files_create(argv[1], &files) == MC_REGISTRY_OK);
    REQUIRE(
        mc_client_display_create("source font controls", 64, 64, true, &display, &device_error));
#ifdef _WIN32
    /* The actual Windows control owns a hidden context; place that same
     * window on physical monitor2 before any Font/GL work, without showing it. */
    int display_count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
    REQUIRE(displays != NULL && display_count >= 2);
    const SDL_DisplayID selected_display = displays[1];
    SDL_free(displays);
    REQUIRE(SDL_SetWindowPosition(mc_client_display_window(display),
                                  (int)SDL_WINDOWPOS_CENTERED_DISPLAY(selected_display),
                                  (int)SDL_WINDOWPOS_CENTERED_DISPLAY(selected_display)));
    REQUIRE(SDL_GetDisplayForWindow(mc_client_display_window(display)) == selected_display);
#endif
    const mc_client_display_info *info = mc_client_display_info_get(display);
    REQUIRE(info != NULL && (info->gl_major > 3 || (info->gl_major == 3 && info->gl_minor >= 3)) &&
            info->profile == SDL_GL_CONTEXT_PROFILE_CORE);
    printf("DEVICE|%d,%d,%d|", info->gl_major, info->gl_minor, info->profile);
    probe_hex(info->vendor, sizeof(info->vendor));
    putchar('|');
    probe_hex(info->renderer, sizeof(info->renderer));
    putchar('|');
    probe_hex(info->version, sizeof(info->version));
    putchar('\n');
    REQUIRE(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    REQUIRE(mc_client_default_font_create(32u * 1024u * 1024u, &default_font) == MC_CLIENT_FONT_OK);
    REQUIRE(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    REQUIRE(mc_client_render_engine_allocate(classes, default_font, NULL, &engine) ==
            MC_CLIENT_RENDER_ENGINE_OK);
    REQUIRE(mc_client_render_engine_construct(engine, NULL, settings, NULL) ==
            MC_CLIENT_RENDER_ENGINE_OK);
    REQUIRE(mc_client_loading_classes_create(classes, &gui) == MC_CLIENT_LOADING_OK);
    REQUIRE(mc_client_loading_classes_bind_resources(gui, mc_resource_files_source(files), NULL) ==
            MC_CLIENT_LOADING_OK);
    REQUIRE(probe_string("/font/white.png", &name) && probe_string("/not-image.bin", &bad_name) &&
            probe_string("A B", &text));

    /* Real ImageIO null result precedes width use56; no READY fabrication. */
    REQUIRE(mc_client_bitmap_font_allocate(gui, NULL, &other) == MC_CLIENT_BITMAP_FONT_OK);
    REQUIRE(mc_client_bitmap_font_construct(other, settings, bad_name, engine, display, error) ==
                MC_CLIENT_BITMAP_FONT_NULL_REFERENCE &&
            error->pc == 56 && !error->native_policy);
    REQUIRE(mc_client_bitmap_font_inspect(other, state) &&
            state->phase == MC_CLIENT_BITMAP_FONT_FAILED && state->widths_completed == 0 &&
            state->retained_image == NULL && !state->has_native_generation);
    REQUIRE(mc_client_bitmap_font_release(&other, error) == MC_CLIENT_BITMAP_FONT_OK &&
            other == NULL);
    puts("FONT not-image56 PASS");

    /* Real PNG decode, getRGB and all256 occupancy widths precede null ji242. */
    REQUIRE(mc_client_bitmap_font_allocate(gui, NULL, &other) == MC_CLIENT_BITMAP_FONT_OK);
    REQUIRE(mc_client_bitmap_font_construct(other, settings, name, NULL, display, error) ==
                MC_CLIENT_BITMAP_FONT_NULL_REFERENCE &&
            error->pc == 242 && !error->native_policy);
    REQUIRE(mc_client_bitmap_font_inspect(other, state) &&
            state->phase == MC_CLIENT_BITMAP_FONT_FAILED && state->widths_completed == 256 &&
            state->retained_image != NULL && !state->has_native_generation);
    for (unsigned i = 0; i < 256; ++i)
        REQUIRE(state->fields.b[i] == (i == 32 ? 4 : 9));
    REQUIRE(mc_client_bitmap_font_release(&other, error) == MC_CLIENT_BITMAP_FONT_OK &&
            other == NULL);
    puts("FONT png-widths242 PASS");

    REQUIRE(mc_client_bitmap_font_allocate(gui, NULL, &ready) == MC_CLIENT_BITMAP_FONT_OK);
    probe_slot = &ready;
    probe_real_image = display->gl.TexImage2D;
    display->gl.TexImage2D = probe_image;
    const mc_client_bitmap_font_status constructed =
        mc_client_bitmap_font_construct(ready, settings, name, engine, display, error);
    display->gl.TexImage2D = probe_real_image;
    REQUIRE(constructed == MC_CLIENT_BITMAP_FONT_OK && probe_callback_ok && probe_callbacks > 0);
    REQUIRE(mc_client_bitmap_font_inspect(ready, state) &&
            state->phase == MC_CLIENT_BITMAP_FONT_READY && state->holds == 1 && !state->active &&
            state->widths_completed == 256 && state->glyphs_defined == 256 &&
            state->palettes_defined == 32 && state->has_native_generation);
    REQUIRE(probe_commands(ready, 256, 32));
    REQUIRE(ready->retained_name == NULL && ready->retained_engine == NULL &&
            ready->borrowed_settings == NULL);
    puts("FONT ready-288-commands PASS");
    puts("FONT constructor-reentry PASS");

    mc_client_loading_projection_stage projection_stage;
    REQUIRE(mc_client_display_begin_scene(display, 0, 0, 0, &device_error));
    REQUIRE(
        mc_client_display_loading_projection(display, 64, 64, &projection_stage, &device_error));
    mc_client_direct_int_buffer *ids = (mc_client_direct_int_buffer *)ready->fields.d;
    const mc_client_graphics_site call = {"native-test", "font-lists", "()V", -1};
    REQUIRE(mc_client_ge_int_clear(ids, call, &graphics_error));
    for (int32_t i = 0; i < 288; ++i)
        REQUIRE(mc_client_ge_int_put(ids, ready->fields.c + i, call, &graphics_error));
    REQUIRE(mc_client_ge_int_flip(ids, call, &graphics_error));
    REQUIRE(mc_client_ge_call_lists(classes, display, ids, call, &graphics_error));
    REQUIRE(display->source.modelview.column_major[12] == 2299.0f);
    puts("FONT execute-all-288 PASS");

    REQUIRE(mc_client_display_begin_scene(display, 0, 0, 0, &device_error));
    REQUIRE(
        mc_client_display_loading_projection(display, 64, 64, &projection_stage, &device_error));
    REQUIRE(mc_client_display_source_texture_enable(display, true, &device_error));
    REQUIRE(mc_client_display_source_alpha_test(display, true, &device_error));
    const mc_client_modelview matrix = display->source.modelview;
    const size_t depth = display->source.depth;
    int32_t measured = -1;
    REQUIRE(mc_client_bitmap_font_measure(ready, text, &measured, error) ==
                MC_CLIENT_BITMAP_FONT_OK &&
            measured == 22);
    probe_callbacks = 0;
    probe_slot = &ready;
    probe_real_draw = display->gl.DrawArrays;
    display->gl.DrawArrays = probe_draw;
    const mc_client_bitmap_font_status drawn =
        mc_client_bitmap_font_draw(ready, text, 8, 8, UINT32_C(0xffffffff), false, error);
    display->gl.DrawArrays = probe_real_draw;
    REQUIRE(drawn == MC_CLIENT_BITMAP_FONT_OK && probe_callback_ok && probe_callbacks == 3);
    REQUIRE(memcmp(&matrix, &display->source.modelview, sizeof(matrix)) == 0 &&
            display->source.depth == depth);
    mc_client_direct_buffer_state nio;
    REQUIRE(mc_client_direct_int_buffer_inspect(ready->fields.d, &nio) && nio.position == 0 &&
            nio.limit == 3);
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &device_error));
    REQUIRE(probe_pixel(&capture, 10, 10, 255) && probe_pixel(&capture, 31, 31, 0));
    /* The borrowed label is an ACTUAL translation String, not a copied Player
     * wrapper. Its real namespace occurrence keeps the UTF16 owner alive. */
    REQUIRE(mc_java_random_profile_create(SIZE_MAX, &label_profile) == MC_JAVA_RANDOM_PROFILE_OK);
    REQUIRE(mc_materials_create(&label_materials));
    const mc_registry_options label_options = {.side = MC_REGISTRY_CLIENT,
                                               .random_profile = label_profile,
                                               .materials = label_materials,
                                               .resources = mc_resource_files_source(files)};
    REQUIRE(mc_registry_classes_allocate(&label_options, &label_registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_registry_classes_initialize_translation_singleton(label_registry, NULL) ==
            MC_REGISTRY_OK);
    REQUIRE(mc_registry_classes_retain(label_registry, MC_REGISTRY_CLIENT, label_profile, NULL) ==
            MC_REGISTRY_OK);
    label_registry_held = true;
    mc_registry_translation_view translations;
    REQUIRE(mc_registry_classes_inspect_translation(label_registry, &translations, NULL) ==
            MC_REGISTRY_OK);
    const uint16_t key[] = {'g', 'u', 'i', '.', 't', 'e', 's', 't'};
    const mc_registry_string *registry_label = NULL, *same_label = NULL;
    REQUIRE(mc_translation_property(translations.singleton_a, key, sizeof(key) / sizeof(key[0]),
                                    &registry_label, NULL) == MC_REGISTRY_OK &&
            registry_label != NULL);
    REQUIRE(mc_translation_property(translations.singleton_a, key, sizeof(key) / sizeof(key[0]),
                                    &same_label, NULL) == MC_REGISTRY_OK &&
            same_label == registry_label);
    mc_registry_string_view registry_view;
    REQUIRE(mc_registry_string_read(registry_label, &registry_view));
    const mc_player_string_view held_view = {registry_view.units, registry_view.length};
    REQUIRE(held_view.length == 3 && held_view.units[0] == 'A' && held_view.units[1] == ' ' &&
            held_view.units[2] == 'B');
    measured = -1;
    REQUIRE(mc_client_bitmap_font_measure_view(ready, &held_view, &measured, error) ==
                MC_CLIENT_BITMAP_FONT_OK &&
            measured == 22);
    measured = -1;
    REQUIRE(mc_client_bitmap_font_measure_view(ready, NULL, &measured, error) ==
                MC_CLIENT_BITMAP_FONT_OK &&
            measured == 0);
    const mc_player_string_view empty_view = {NULL, 0};
    REQUIRE(mc_client_bitmap_font_measure_view(ready, &empty_view, &measured, error) ==
                MC_CLIENT_BITMAP_FONT_OK &&
            measured == 0);
    const mc_player_string_view malformed_view = {NULL, 1};
    measured = -77;
    REQUIRE(mc_client_bitmap_font_measure_view(ready, &malformed_view, &measured, error) ==
                MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT &&
            measured == -77 && error->native_policy);
    const mc_player_string_view long_view = {held_view.units, 65537u};
    REQUIRE(mc_client_bitmap_font_measure_view(ready, &long_view, &measured, error) ==
                MC_CLIENT_BITMAP_FONT_LIMIT &&
            measured == -77);
    REQUIRE(mc_client_bitmap_font_inspect(ready, state) && state->holds == 1 && !state->active);

    legacy_capture = capture;
    capture = (mc_client_capture){0};
    REQUIRE(mc_client_display_begin_scene(display, 0, 0, 0, &device_error));
    REQUIRE(
        mc_client_display_loading_projection(display, 64, 64, &projection_stage, &device_error));
    REQUIRE(mc_client_display_source_texture_enable(display, true, &device_error));
    REQUIRE(mc_client_display_source_alpha_test(display, true, &device_error));
    probe_callbacks = 0;
    probe_registry = label_registry;
    probe_view = &held_view;
    display->gl.DrawArrays = probe_draw;
    const mc_client_bitmap_font_status view_drawn = mc_client_bitmap_font_draw_view(
        ready, &held_view, 8, 8, UINT32_C(0xffffffff), false, error);
    display->gl.DrawArrays = probe_real_draw;
    probe_view = NULL;
    probe_registry = NULL;
    REQUIRE(view_drawn == MC_CLIENT_BITMAP_FONT_OK && probe_callbacks == 3 && probe_callback_ok);
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &device_error));
    REQUIRE(capture.width == legacy_capture.width && capture.height == legacy_capture.height &&
            capture.stride == legacy_capture.stride &&
            memcmp(capture.pixels, legacy_capture.pixels,
                   capture.stride * (size_t)capture.height) == 0);
    mc_client_capture_free(&capture);
    mc_client_capture_free(&legacy_capture);

    REQUIRE(mc_client_display_begin_scene(display, 0, 0, 0, &device_error));
    REQUIRE(
        mc_client_display_loading_projection(display, 64, 64, &projection_stage, &device_error));
    REQUIRE(mc_client_display_source_texture_enable(display, true, &device_error));
    REQUIRE(mc_client_display_source_alpha_test(display, true, &device_error));
    REQUIRE(mc_client_bitmap_font_draw_shadow(ready, text, 8, 8, UINT32_C(0xffffffff), error) ==
            MC_CLIENT_BITMAP_FONT_OK);
    REQUIRE(mc_client_display_capture_rgba(display, &legacy_capture, &device_error));
    REQUIRE(mc_client_display_begin_scene(display, 0, 0, 0, &device_error));
    REQUIRE(
        mc_client_display_loading_projection(display, 64, 64, &projection_stage, &device_error));
    REQUIRE(mc_client_display_source_texture_enable(display, true, &device_error));
    REQUIRE(mc_client_display_source_alpha_test(display, true, &device_error));
    REQUIRE(mc_client_bitmap_font_draw_shadow_view(ready, &held_view, 8, 8, UINT32_C(0xffffffff),
                                                   error) == MC_CLIENT_BITMAP_FONT_OK);
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &device_error));
    REQUIRE(capture.width == legacy_capture.width && capture.height == legacy_capture.height &&
            capture.stride == legacy_capture.stride &&
            memcmp(capture.pixels, legacy_capture.pixels,
                   capture.stride * (size_t)capture.height) == 0);
    mc_client_capture_free(&capture);
    mc_client_capture_free(&legacy_capture);

    const unsigned int old_csr = _mm_getcsr();
    _mm_setcsr(old_csr | 0x8000u); /* Real unsupported FTZ environment, no repair in source. */
    const mc_client_bitmap_font_status null_draw =
        mc_client_bitmap_font_draw_view(ready, NULL, 8, 8, UINT32_C(0xffffffff), false, error);
    const mc_client_bitmap_font_status empty_draw = mc_client_bitmap_font_draw_view(
        ready, &empty_view, 8, 8, UINT32_C(0xffffffff), false, error);
    const int32_t empty_pc = error->pc;
    _mm_setcsr(old_csr);
    REQUIRE(null_draw == MC_CLIENT_BITMAP_FONT_OK &&
            empty_draw == MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP && empty_pc == 55);
    REQUIRE(mc_client_bitmap_font_inspect(ready, state) && state->holds == 1 && !state->active);
    REQUIRE(mc_registry_classes_release(label_registry, NULL) == MC_REGISTRY_OK);
    label_registry_held = false;
    REQUIRE(mc_registry_classes_free(label_registry, NULL) == MC_REGISTRY_OK);
    label_registry = NULL;
    mc_materials_destroy(label_materials);
    label_materials = NULL;
    mc_java_random_profile_free(label_profile);
    label_profile = NULL;
    puts("FONT measure-draw-capture PASS");
    puts("FONT draw-reentry PASS");

    /* The complete canonical App type is the constructor checkpoint's owner.
     * These are explicit prepared source inputs, not a whole constructor.
     * Deliberately different native dimensions/flags cannot supply d/e/J. */
    REQUIRE(mc_client_application_lifetime_register(app, &app_error) ==
            MC_CLIENT_APPLICATION_OWNER_OK);
    registered = true;
    app->display = display;
    app->settings = settings;
    app->render_engine = engine;
    app->d = 640;
    app->e = 480;
    app->display_width = 13;
    app->display_height = 17;
    app->running = false;
    app->captured = true;
    atomic_store_explicit(&app->J, true, memory_order_seq_cst);
    REQUIRE(mc_client_loading_screen_allocate(gui, &screen) == MC_CLIENT_LOADING_OK);
    REQUIRE(mc_client_loading_screen_construct(screen, app, loading_error) == MC_CLIENT_LOADING_OK);
    mc_client_loading_state loading_state;
    REQUIRE(mc_client_loading_screen_inspect(screen, &loading_state) &&
            loading_state.fields.a == mc_client_loading_classes_empty(gui) &&
            loading_state.fields.c == mc_client_loading_classes_empty(gui));
    REQUIRE(mc_client_loading_screen_title(screen, text, loading_error) == MC_CLIENT_LOADING_OK &&
            loading_error->qq_width == 640 && loading_error->qq_height == 480 &&
            loading_error->qq_factor == 2);
    REQUIRE(app->display_width == 13 && app->display_height == 17 && !app->running &&
            app->captured);
    app->constructor_phase = MC_CLIENT_APP_RETIRING;
    REQUIRE(mc_client_loading_screen_title(screen, name, loading_error) ==
                MC_CLIENT_LOADING_INVALID_PHASE &&
            loading_error->site.pc == 4);
    REQUIRE(mc_client_loading_screen_inspect(screen, &loading_state) &&
            loading_state.fields.c == text);
    app->constructor_phase = MC_CLIENT_APP_RAW;
    puts("FONT actual-App-d-e-J-lifetime PASS");
    REQUIRE(mc_client_loading_screen_message(screen, text, loading_error) ==
                MC_CLIENT_LOADING_NULL_REFERENCE &&
            loading_error->site.pc == 165 && loading_error->child.pc == 4 &&
            strcmp(loading_error->child.class_name, "ji") == 0 &&
            strcmp(loading_error->child.method, "b") == 0 && loading_error->lookup != NULL &&
            loading_error->lookup->status == MC_CLIENT_LOOKUP_NULL_REFERENCE &&
            loading_error->lookup->site.pc == 4 &&
            !loading_error->lookup->texture.driver_generated &&
            loading_error->sampled_clock_committed);
    mc_client_loading_call_state pending_call;
    REQUIRE(mc_client_loading_screen_call_inspect(screen, &pending_call) && pending_call.retained &&
            pending_call.lookup_failed && pending_call.engine == engine);
    REQUIRE(mc_client_loading_screen_retire_call(screen, loading_error) == MC_CLIENT_LOADING_OK);
    REQUIRE(mc_client_loading_screen_call_inspect(screen, &pending_call) && !pending_call.retained);
    REQUIRE(mc_client_loading_screen_release(screen) == MC_CLIENT_LOADING_OK);
    screen = NULL;
    REQUIRE(mc_client_application_lifetime_retire(app, &app_error) ==
            MC_CLIENT_APPLICATION_OWNER_OK);
    registered = false;
    puts("FONT actual-mk165-frontier PASS");

    /* A genuine failed constructor owns256 completed glyphs before Settings580. */
    REQUIRE(mc_client_bitmap_font_allocate(gui, NULL, &failed) == MC_CLIENT_BITMAP_FONT_OK);
    REQUIRE(mc_client_bitmap_font_construct(failed, NULL, name, engine, display, error) ==
                MC_CLIENT_BITMAP_FONT_NULL_REFERENCE &&
            error->pc == 580 && !error->native_policy);
    REQUIRE(mc_client_bitmap_font_inspect(failed, state) &&
            state->phase == MC_CLIENT_BITMAP_FONT_FAILED && state->glyphs_defined == 256 &&
            state->palettes_defined == 0 && state->has_native_generation);
    REQUIRE(probe_commands(failed, 256, 0));
    REQUIRE(mc_client_bitmap_font_retain(failed) == MC_CLIENT_BITMAP_FONT_OK);
    alias = failed;
    REQUIRE(mc_client_bitmap_font_release(&alias, error) == MC_CLIENT_BITMAP_FONT_OK &&
            alias == NULL);
    REQUIRE(failed->holds == 1);
    REQUIRE(mc_client_font_lease_begin(failed->lease, &graphics_error));
    lease_entered = true;
    mc_client_bitmap_font *preserved = failed;
    REQUIRE(mc_client_bitmap_font_release(&failed, error) == MC_CLIENT_BITMAP_FONT_BUSY &&
            failed == preserved && failed->phase == MC_CLIENT_BITMAP_FONT_RETIRING &&
            failed->holds == 1 && !failed->active);
    REQUIRE(failed->failure.pc == 580 &&
            mc_client_bitmap_font_retain(failed) == MC_CLIENT_BITMAP_FONT_INVALID_PHASE);
    REQUIRE(mc_client_font_lease_end(failed->lease, &graphics_error));
    lease_entered = false;
    REQUIRE(mc_client_bitmap_font_release(&failed, error) == MC_CLIENT_BITMAP_FONT_OK &&
            failed == NULL);
    puts("FONT failed580-retire-busy-retry PASS");

    REQUIRE(mc_client_render_engine_retire_resources(classes, &texture_error) ==
            MC_CLIENT_RENDER_TEXTURE_BUSY);
    /* LAST Source invocation. U+0130 is outside the invariant whole-String
     * lowercase domain. The first glyph is already queued when sj reaches178;
     * no palette command, flush, Source pop or subsequent Source case follows. */
    static const uint16_t unsafe_units[] = {'A', UINT16_C(0x00a7), 'a', UINT16_C(0x0130)};
    REQUIRE(mc_player_string_create(unsafe_units, sizeof(unsafe_units) / sizeof(*unsafe_units),
                                    &unsafe_text) == MC_PLAYER_OK);
    probe_source_prefix unsafe_before, unsafe_failed, unsafe_first_release, unsafe_final_release;
    const float unsafe_initial_modelview[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -200, 1};
    REQUIRE(probe_prefix(display, &unsafe_before) && unsafe_before.mode == GL_MODELVIEW &&
            unsafe_before.color_known && unsafe_before.pending_enabled == 0 &&
            unsafe_before.pending_flags == 0 &&
            (unsafe_before.attribute_profile == SOURCE_ATTRIBUTE_PROFILE_UNKNOWN ||
             unsafe_before.attribute_profile == SOURCE_ATTRIBUTE_PROFILE_NVIDIA_581_42 ||
             unsafe_before.attribute_profile == SOURCE_ATTRIBUTE_PROFILE_MESA_LLVMPIPE) &&
            memcmp(unsafe_before.modelview, unsafe_initial_modelview,
                   sizeof(unsafe_initial_modelview)) == 0);
    mc_client_direct_buffer_state unsafe_nio_before, unsafe_nio_failed, unsafe_nio_held;
    REQUIRE(mc_client_direct_int_buffer_inspect(ready->fields.d, &unsafe_nio_before) &&
            unsafe_nio_before.capacity == 1024 && unsafe_nio_before.direct &&
            unsafe_nio_before.native_order);
    REQUIRE(mc_client_bitmap_font_retain(ready) == MC_CLIENT_BITMAP_FONT_OK);
    alias = ready;
    REQUIRE(mc_client_bitmap_font_draw(ready, unsafe_text, 7, 11, UINT32_C(0xff123456), false,
                                       error) == MC_CLIENT_BITMAP_FONT_UNSUPPORTED_LOWERCASE &&
            error->status == MC_CLIENT_BITMAP_FONT_UNSUPPORTED_LOWERCASE && error->pc == 178 &&
            error->native_policy && !error->cleanup_failed && strcmp(error->method, "a") == 0 &&
            strcmp(error->descriptor, "(Ljava/lang/String;IIIZ)V") == 0);
    REQUIRE(mc_client_bitmap_font_inspect(ready, state) &&
            state->phase == MC_CLIENT_BITMAP_FONT_READY && state->holds == 2 && !state->active);
    REQUIRE(probe_prefix(display, &unsafe_failed) && unsafe_failed.mode == unsafe_before.mode &&
            unsafe_failed.modelview_depth == unsafe_before.modelview_depth + 1 &&
            unsafe_failed.projection_depth == unsafe_before.projection_depth &&
            memcmp(unsafe_failed.projection, unsafe_before.projection,
                   sizeof(unsafe_before.projection)) == 0 &&
            unsafe_failed.color_known);
    /* Full loading-projection prestate makes the rounded translation exact in
     * every row, including the signed-zero-sensitive last-column stores. */
    const float unsafe_expected_modelview[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 7, 11, -200, 1};
    const float unsafe_expected_color[] = {18.0f / 255.0f, 52.0f / 255.0f, 86.0f / 255.0f, 1.0f};
    REQUIRE(memcmp(unsafe_failed.modelview, unsafe_expected_modelview,
                   sizeof(unsafe_expected_modelview)) == 0);
    probe_source_prefix unsafe_expected = unsafe_before;
    memcpy(unsafe_expected.modelview, unsafe_expected_modelview, sizeof(unsafe_expected.modelview));
    ++unsafe_expected.modelview_depth;
    if (unsafe_before.attribute_profile == SOURCE_ATTRIBUTE_PROFILE_MESA_LLVMPIPE) {
        /* sj118 stages CURRENT-only color. Push does not commit attributes and
         * Translate flushes only STORED, so sj178 retains the previous committed
         * color alongside this newly pending color. No getter/flush is inserted. */
        unsafe_expected.pending_enabled = UINT32_C(1) << SOURCE_STAGE_COLOR;
        unsafe_expected.pending_flags = SOURCE_STAGE_CURRENT;
        memcpy(unsafe_expected.pending_color, unsafe_expected_color,
               sizeof(unsafe_expected.pending_color));
        unsafe_expected.pending_color_known = true;
    } else {
        /* NVIDIA and the unknown-profile immediate path update committed color;
         * all inactive staging cells retain their captured bytes and knowledge. */
        memcpy(unsafe_expected.color, unsafe_expected_color, sizeof(unsafe_expected.color));
    }
    REQUIRE(probe_prefix_equal(&unsafe_failed, &unsafe_expected));
    REQUIRE(mc_client_direct_int_buffer_inspect(ready->fields.d, &unsafe_nio_failed) &&
            unsafe_nio_failed.identity == unsafe_nio_before.identity &&
            unsafe_nio_failed.storage == unsafe_nio_before.storage &&
            unsafe_nio_failed.position == 1 && unsafe_nio_failed.limit == 1024 &&
            unsafe_nio_failed.capacity == 1024 && unsafe_nio_failed.direct &&
            unsafe_nio_failed.native_order &&
            ((const int32_t *)unsafe_nio_failed.storage)[0] == ready->fields.c + 65);
    REQUIRE(mc_client_bitmap_font_release(&ready, error) == MC_CLIENT_BITMAP_FONT_OK &&
            ready == NULL);
    REQUIRE(mc_client_bitmap_font_inspect(alias, state) && state->holds == 1 && !state->active &&
            probe_prefix(display, &unsafe_first_release) &&
            probe_prefix_equal(&unsafe_failed, &unsafe_first_release) &&
            mc_client_direct_int_buffer_inspect(alias->fields.d, &unsafe_nio_held) &&
            unsafe_nio_held.identity == unsafe_nio_failed.identity &&
            unsafe_nio_held.storage == unsafe_nio_failed.storage &&
            unsafe_nio_held.position == unsafe_nio_failed.position &&
            unsafe_nio_held.limit == unsafe_nio_failed.limit &&
            unsafe_nio_held.capacity == unsafe_nio_failed.capacity &&
            ((const int32_t *)unsafe_nio_held.storage)[0] == alias->fields.c + 65);
    REQUIRE(mc_client_bitmap_font_release(&alias, error) == MC_CLIENT_BITMAP_FONT_OK &&
            alias == NULL);
    /* The NIO borrow ends with final Font release. Passive Source matrix/color
     * state remains inspectable on the live Display; no repair is performed. */
    REQUIRE(probe_prefix(display, &unsafe_final_release) &&
            probe_prefix_equal(&unsafe_failed, &unsafe_final_release));
    puts("FONT unsupported-lowercase178-terminal PASS");
    REQUIRE(mc_client_render_engine_retire_resources(classes, &texture_error) ==
            MC_CLIENT_RENDER_TEXTURE_OK);
    REQUIRE(classes->font_lease_count == 0 && classes->command_range_count == 0 &&
            classes->native_resource_count == 0 && display->source.range_count == 0 &&
            display->source.texture_lease_count == 0);
    REQUIRE(mc_client_loading_classes_release(gui) == MC_CLIENT_LOADING_OK);
    gui = NULL;
    REQUIRE(engine->holds == 1 && engine->lookup != NULL &&
            !mc_client_texture_lookup_pending(engine));
    REQUIRE(mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_BUSY);
    REQUIRE(mc_client_texture_lookup_free_metadata(engine) && engine->lookup == NULL);
    REQUIRE(mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_OK);
    engine = NULL;
    REQUIRE(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    classes = NULL;
    REQUIRE(mc_client_default_font_release(default_font) == MC_CLIENT_FONT_OK);
    default_font = NULL;
    REQUIRE(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    REQUIRE(mc_resource_files_free(files) == MC_REGISTRY_OK);
    files = NULL;
    REQUIRE(mc_client_display_destroy(display, &device_error));
    display = NULL;
    puts("FONT native-retirement PASS");
    printf("source font: %u checks passed\n", probe_checks);
    REQUIRE(fflush(stdout) == 0 && !ferror(stdout));
    result = 0;
done:
    probe_registry = NULL;
    probe_view = NULL;
    mc_client_capture_free(&legacy_capture);
    if (label_registry_held && mc_registry_classes_release(label_registry, NULL) == MC_REGISTRY_OK)
        label_registry_held = false;
    if (label_registry != NULL && !label_registry_held &&
        mc_registry_classes_free(label_registry, NULL) == MC_REGISTRY_OK)
        label_registry = NULL;
    if (label_registry == NULL) {
        mc_materials_destroy(label_materials);
        mc_java_random_profile_free(label_profile);
    }
    if (display != NULL) {
        if (probe_real_image != NULL)
            display->gl.TexImage2D = probe_real_image;
        if (probe_real_draw != NULL)
            display->gl.DrawArrays = probe_real_draw;
    }
    probe_slot = NULL;
    if (screen != NULL && mc_client_loading_screen_release(screen) == MC_CLIENT_LOADING_OK)
        screen = NULL;
    if (registered && screen == NULL &&
        mc_client_application_lifetime_retire(app, &app_error) == MC_CLIENT_APPLICATION_OWNER_OK)
        registered = false;
    if (lease_entered && failed != NULL)
        (void)mc_client_font_lease_end(failed->lease, &graphics_error);
    if (other != NULL)
        (void)mc_client_bitmap_font_release(&other, error);
    if (alias != NULL)
        (void)mc_client_bitmap_font_release(&alias, error);
    if (failed != NULL)
        (void)mc_client_bitmap_font_release(&failed, error);
    if (ready != NULL)
        (void)mc_client_bitmap_font_release(&ready, error);
    bool resources_retired = classes == NULL;
    if (classes != NULL)
        resources_retired = mc_client_render_engine_retire_resources(classes, &texture_error) ==
                            MC_CLIENT_RENDER_TEXTURE_OK;
    if (engine != NULL && ready == NULL && failed == NULL && other == NULL && alias == NULL &&
        resources_retired)
        (void)mc_client_texture_lookup_free_metadata(engine);
    if (gui != NULL)
        (void)mc_client_loading_classes_release(gui);
    if (engine != NULL)
        (void)mc_client_render_engine_release(engine);
    if (classes != NULL)
        (void)mc_client_render_engine_classes_release(classes);
    if (default_font != NULL)
        (void)mc_client_default_font_release(default_font);
    if (settings != NULL)
        (void)mc_game_settings_retire(&settings, NULL);
    if (files != NULL && label_registry == NULL)
        (void)mc_resource_files_free(files);
    if (display != NULL)
        (void)mc_client_display_destroy(display, &device_error);
    mc_player_string_release(name);
    mc_player_string_release(bad_name);
    mc_player_string_release(text);
    mc_player_string_release(unsafe_text);
    mc_client_capture_free(&capture);
    free(state);
    free(error);
    free(loading_error);
    if (!registered && screen == NULL)
        free(app);
    return result;
}
