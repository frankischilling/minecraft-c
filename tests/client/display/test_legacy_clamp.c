/* Actual owned Display/JI/NW/GE fixture. Authored sample inputs, no supplied
 * texture answers, replacement sampling shader or original asset prerequisite. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
int mc_clamp_lookup_fixture_main(int argc, char **argv);
#define main mc_clamp_lookup_fixture_main
#include "../render_engine/test_texture_lookup.c"
#undef main

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/gl_internal.h"
#include "client/display/source_surface.h"
#include "client/display/world_frame_surface.h"
#include <SDL3/SDL.h>
#include <inttypes.h>

typedef void(GLAPIENTRY *clamp_read_fn)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
typedef void(GLAPIENTRY *clamp_border_fn)(GLenum, GLenum, const GLfloat *);
typedef void(GLAPIENTRY *clamp_disable_fn)(GLenum);
typedef void(GLAPIENTRY *clamp_parameter_fn)(GLenum, GLenum, GLint *);
static clamp_read_fn clamp_read;
static clamp_border_fn clamp_border;
static clamp_disable_fn clamp_disable;
static clamp_parameter_fn clamp_parameter;
static mc_client_display_error clamp_error;
static mc_client_graphics_error clamp_graphics;
static const double clamp_coordinates[] = {-2, -.25, 0, .125, .25, .5, .75, .875, 1, 1.25, 3};
static const uint8_t clamp_texels[] = {240, 32, 16,  255, 16,  224, 48, 192,
                                       32,  48, 240, 128, 208, 176, 64, 64};

static int clamp_functions(void) {
#define CLAMP_LOAD(target, name)                                                                   \
    do {                                                                                           \
        SDL_FunctionPointer address = SDL_GL_GetProcAddress(name);                                 \
        _Static_assert(sizeof(target) == sizeof(address), "GL function representation");           \
        CHECK(address != NULL);                                                                    \
        memcpy(&(target), &address, sizeof(target));                                               \
    } while (0)
    CLAMP_LOAD(clamp_read, "glReadPixels");
    CLAMP_LOAD(clamp_border, "glTexParameterfv");
    CLAMP_LOAD(clamp_disable, "glDisable");
    CLAMP_LOAD(clamp_parameter, "glGetTexParameteriv");
#undef CLAMP_LOAD
    return 0;
}

static bool clamp_equal(mc_client_texture_source_state a, mc_client_texture_source_state b) {
    return a.generation == b.generation && a.min_filter == b.min_filter &&
           a.mag_filter == b.mag_filter && a.wrap_s == b.wrap_s && a.wrap_t == b.wrap_t &&
           a.known == b.known && a.image_uploaded == b.image_uploaded &&
           a.physical_known == b.physical_known && a.physical_wrap_s == b.physical_wrap_s &&
           a.physical_wrap_t == b.physical_wrap_t;
}

static int clamp_setup(mc_client_display *display) {
    CHECK(mc_client_display_source_viewport(display, 1, 1, &clamp_error));
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, &clamp_error));
    CHECK(mc_client_display_source_load_identity(display, &clamp_error));
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &clamp_error));
    CHECK(mc_client_display_source_load_identity(display, &clamp_error));
    CHECK(mc_client_display_source_blend_enable(display, false, &clamp_error));
    CHECK(mc_client_display_source_alpha_test(display, false, &clamp_error));
    CHECK(mc_client_display_source_depth_test(display, false, &clamp_error));
    CHECK(mc_client_display_source_cull_test(display, false, &clamp_error));
    CHECK(mc_client_display_source_lighting_enable(display, false, &clamp_error));
    CHECK(mc_client_display_source_fog_enable(display, false, &clamp_error));
    CHECK(mc_client_display_source_color_material_enable(display, false, &clamp_error));
    CHECK(mc_client_display_source_texture_enable(display, true, &clamp_error));
    CHECK(mc_client_display_source_color(display, 1, 1, 1, 1, &clamp_error));
    return 0;
}

static int clamp_parameters(mc_client_texture_resource *resource, GLint filter, GLint wrap_s,
                            GLint wrap_t, unsigned int border) {
    CHECK(mc_client_display_bind_texture(resource, &clamp_error));
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_MIN_FILTER, filter,
                                              &clamp_error));
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_MAG_FILTER, filter,
                                              &clamp_error));
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_WRAP_S, wrap_s,
                                              &clamp_error));
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_WRAP_T, wrap_t,
                                              &clamp_error));
    /* An explicit actual driver border input; no source border-setter body is
     * claimed. This does not replace any source wrap/filter or query result. */
    const GLfloat zero[] = {0, 0, 0, 0}, nonzero[] = {.25f, .375f, .5f, .625f};
    clamp_border(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border == 0 ? zero : nonzero);
    return 0;
}

static int clamp_pixel(uint8_t pixel[4]) {
    clamp_read(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    return 0;
}

static int clamp_gui(mc_client_display *display, mc_client_texture texture, double s, double t,
                     double gradient) {
    const mc_client_vertex vertices[] = {
        {0, 0, (float)(s - gradient * .5), (float)(t + gradient * .5), UINT32_MAX},
        {1, 0, (float)(s + gradient * .5), (float)(t + gradient * .5), UINT32_MAX},
        {1, 1, (float)(s + gradient * .5), (float)(t - gradient * .5), UINT32_MAX},
        {0, 1, (float)(s - gradient * .5), (float)(t - gradient * .5), UINT32_MAX}};
    CHECK(mc_client_display_draw_quad(display, texture, vertices, false, false, &clamp_error));
    return 0;
}

static int clamp_nw_quad(mc_client_tessellator *nw, double s, double t, double gradient) {
    CHECK(mc_client_nw_begin_quads(nw, &clamp_graphics));
    CHECK(mc_client_nw_vertex_uv(nw, -1, -1, 0, s - gradient * .5, t - gradient * .5,
                                 &clamp_graphics));
    CHECK(mc_client_nw_vertex_uv(nw, 1, -1, 0, s + gradient * .5, t - gradient * .5,
                                 &clamp_graphics));
    CHECK(
        mc_client_nw_vertex_uv(nw, 1, 1, 0, s + gradient * .5, t + gradient * .5, &clamp_graphics));
    CHECK(mc_client_nw_vertex_uv(nw, -1, 1, 0, s - gradient * .5, t + gradient * .5,
                                 &clamp_graphics));
    return 0;
}

static int clamp_samples(fixture *f, mc_client_texture_resource *resource, mc_client_texture id,
                         mc_client_tessellator *nw) {
    CHECK(mc_client_display_begin(f->display, 1, 1, &clamp_error));
    CHECK(clamp_setup(f->display) == 0);
    const GLint filters[] = {GL_NEAREST, GL_LINEAR}, wraps[] = {GL_REPEAT, GL_CLAMP};
    size_t index = 0;
    for (size_t filter = 0; filter < COUNT(filters); ++filter)
        for (unsigned int border = 0; border < 2; ++border)
            for (size_t si = 0; si < COUNT(wraps); ++si)
                for (size_t ti = 0; ti < COUNT(wraps); ++ti) {
                    CHECK(clamp_parameters(resource, filters[filter], wraps[si], wraps[ti],
                                           border) == 0);
                    mc_client_texture_source_state state;
                    CHECK(mc_client_display_texture_state_inspect(f->display, id, &state) &&
                          state.known && state.physical_known && state.image_uploaded &&
                          state.min_filter == filters[filter] &&
                          state.mag_filter == filters[filter] && state.wrap_s == wraps[si] &&
                          state.wrap_t == wraps[ti]);
                    GLint physical_s = 0, physical_t = 0;
                    clamp_parameter(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &physical_s);
                    clamp_parameter(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &physical_t);
                    CHECK(physical_s == state.physical_wrap_s &&
                          physical_t == state.physical_wrap_t);
                    printf("CLAMP_STATE %zu %d %u %d %d %d %d\n", index, filters[filter], border,
                           wraps[si], wraps[ti], physical_s, physical_t);
                    for (unsigned int gradient = 0; gradient < 2; ++gradient)
                        for (size_t y = 0; y < COUNT(clamp_coordinates); ++y)
                            for (size_t x = 0; x < COUNT(clamp_coordinates); ++x) {
                                const double g = gradient == 0 ? 0 : 8;
                                uint8_t gui[4], source[4];
                                CHECK(clamp_gui(f->display, id, clamp_coordinates[x],
                                                clamp_coordinates[y], g) == 0);
                                CHECK(clamp_pixel(gui) == 0);
                                CHECK(clamp_nw_quad(nw, clamp_coordinates[x], clamp_coordinates[y],
                                                    g) == 0);
                                CHECK(mc_client_nw_draw(nw, f->display, &clamp_graphics));
                                CHECK(clamp_pixel(source) == 0);
                                CHECK(memcmp(gui, source, 4) == 0);
                                printf("CLAMP_PIXEL %zu %d %u %d %d %u %zu %zu %02x%02x%02x%02x\n",
                                       index, filters[filter], border, wraps[si], wraps[ti],
                                       gradient, x, y, gui[0], gui[1], gui[2], gui[3]);
                                ++index;
                            }
                }
    CHECK(index == 3872);
    return 0;
}

static int clamp_lists(fixture *f, mc_client_texture_resource *resource, mc_client_texture id,
                       mc_client_tessellator *nw) {
    const mc_client_graphics_site caller = {"clamp_fixture", "lists", "()V", 0};
    int32_t list = 0;
    CHECK(mc_client_ge_generate_lists(f->classes, f->display, 1, caller, &list, &clamp_graphics));
    CHECK(mc_client_display_lists_begin(f->display, list, &clamp_error));
    CHECK(clamp_nw_quad(nw, 1.25, -.25, 0) == 0);
    CHECK(mc_client_nw_draw(nw, f->display, &clamp_graphics));
    CHECK(mc_client_display_lists_end(f->display, &clamp_error));
    mc_client_source_batch_view view;
    CHECK(mc_client_display_list_batch(f->display, list, 0, &view) && view.byte_count == 192 &&
          view.vertices == 6);
    uint8_t saved[192];
    memcpy(saved, view.bytes, sizeof(saved));
    const GLint filters[] = {GL_NEAREST, GL_NEAREST, GL_LINEAR, GL_NEAREST, GL_LINEAR};
    const GLint s[] = {GL_REPEAT, GL_CLAMP, GL_CLAMP, GL_CLAMP, GL_REPEAT};
    const GLint t[] = {GL_REPEAT, GL_REPEAT, GL_CLAMP, GL_CLAMP, GL_REPEAT};
    for (size_t i = 0; i < COUNT(filters); ++i) {
        CHECK(clamp_parameters(resource, filters[i], s[i], t[i], 1) == 0);
        CHECK(mc_client_display_list_call(f->display, list, &clamp_error));
        uint8_t pixel[4];
        CHECK(clamp_pixel(pixel) == 0);
        CHECK(mc_client_display_list_batch(f->display, list, 0, &view) &&
              view.byte_count == sizeof(saved) && !memcmp(saved, view.bytes, sizeof(saved)));
        printf("CLAMP_LIST %zu %02x%02x%02x%02x\n", i, pixel[0], pixel[1], pixel[2], pixel[3]);
    }
    CHECK(clamp_parameters(resource, GL_NEAREST, GL_CLAMP, GL_CLAMP, 0) == 0);
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_MIN_FILTER, GL_LINEAR,
                                              &clamp_error));
    mc_client_texture_source_state mixed;
    CHECK(mc_client_display_texture_state_inspect(f->display, id, &mixed) &&
          mixed.min_filter == GL_LINEAR && mixed.mag_filter == GL_NEAREST &&
          mixed.wrap_s == GL_CLAMP);
    uint8_t before[4], after[4];
    CHECK(clamp_pixel(before) == 0);
    CHECK(!mc_client_display_list_call(f->display, list, &clamp_error) &&
          clamp_error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL &&
          !strcmp(clamp_error.operation, "source legacy clamp sampling"));
    CHECK(clamp_pixel(after) == 0 && !memcmp(before, after, 4));
    mc_client_texture_source_state retained;
    CHECK(mc_client_display_texture_state_inspect(f->display, id, &retained) &&
          clamp_equal(mixed, retained));
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_MAG_FILTER, GL_LINEAR,
                                              &clamp_error));
    CHECK(mc_client_display_list_call(f->display, list, &clamp_error));
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_MIN_FILTER,
                                              GL_LINEAR_MIPMAP_LINEAR, &clamp_error));
    CHECK(!mc_client_display_list_call(f->display, list, &clamp_error) &&
          clamp_error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
    CHECK(mc_client_display_source_texture_enable(f->display, false, &clamp_error));
    CHECK(mc_client_display_list_call(f->display, list, &clamp_error));
    CHECK(mc_client_display_source_texture_enable(f->display, true, &clamp_error));
    CHECK(clamp_parameters(resource, GL_NEAREST, GL_CLAMP, GL_CLAMP, 0) == 0);
    CHECK(mc_client_display_list_call(f->display, list, &clamp_error));
    CHECK(mc_client_display_list_batch(f->display, list, 0, &view) &&
          !memcmp(saved, view.bytes, sizeof(saved)));
    int32_t pending = 0;
    CHECK(
        mc_client_ge_generate_lists(f->classes, f->display, 1, caller, &pending, &clamp_graphics));
    CHECK(mc_client_display_lists_begin(f->display, pending, &clamp_error));
    CHECK(mc_client_display_texture_state_inspect(f->display, id, &retained));
    CHECK(!mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_WRAP_S, GL_REPEAT,
                                               &clamp_error) &&
          clamp_error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL &&
          !strcmp(clamp_error.operation, "owned texture parameter compilation"));
    CHECK(mc_client_display_texture_state_inspect(f->display, id, &mixed) &&
          clamp_equal(retained, mixed));
    mc_client_source_list_state definition;
    CHECK(mc_client_display_list_inspect(f->display, pending, &definition) && definition.building &&
          definition.pending_commands == 0);
    CHECK(mc_client_display_lists_end(f->display, &clamp_error));
    puts("CLAMP_CONTROL dynamic_lists_mixed_mipmap_and_compiled_parameter");
    return 0;
}

static int clamp_staging(mc_client_display *display, mc_client_texture_resource *resource) {
    CHECK(clamp_parameters(resource, GL_NEAREST, GL_CLAMP, GL_CLAMP, 0) == 0);
    CHECK(mc_client_display_source_color(display, .125f, .25f, .5f, .75f, &clamp_error));
    float current[4];
    CHECK(mc_client_display_source_current_attribute(display, GL_CURRENT_COLOR, current, 4,
                                                     &clamp_error));
    CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .125f, &clamp_error));
    mc_client_source_draw_state before, same, changed;
    CHECK(mc_client_display_source_draw_inspect(display, &before));
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_WRAP_S, GL_CLAMP,
                                              &clamp_error));
    CHECK(mc_client_display_source_draw_inspect(display, &same) &&
          !memcmp(before.color, same.color, sizeof(same.color)));
    CHECK(mc_client_display_texture_parameter(resource, MC_CLIENT_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE,
                                              &clamp_error));
    CHECK(mc_client_display_source_draw_inspect(display, &changed) && changed.color_known &&
          changed.color[0] == .75f && changed.color[1] == .5f && changed.color[2] == .25f &&
          changed.color[3] == .125f);
    CHECK(mc_client_display_source_color(display, 1, 1, 1, 1, &clamp_error));
    puts("CLAMP_CONTROL logical_parameter_material_staging");
    return 0;
}

static int clamp_cold_lookup(fixture *f) {
    static const uint16_t shadow[] = {'%', 'c', 'l', 'a', 'm', 'p', '%', '/', 'm', 'i', 's', 'c',
                                      '/', 's', 'h', 'a', 'd', 'o', 'w', '.', 'p', 'n', 'g'};
    bool found = true;
    mc_client_texture value = 0xdeadbeefu;
    CHECK(mc_client_render_engine_cached_texture(f->engine, text(shadow, COUNT(shadow)), &found,
                                                 &value) &&
          !found);
    const size_t ledger = f->classes->b->count;
    CHECK(mc_client_render_engine_texture(f->engine, f->display, NULL, text(shadow, COUNT(shadow)),
                                          &value, f->error) == MC_CLIENT_LOOKUP_OK);
    mc_client_texture_source_state state;
    CHECK(value != 0xdeadbeefu && f->classes->b->count == ledger + 1 && !f->engine->fields.j &&
          mc_client_display_texture_state_inspect(f->display, value, &state) && state.known &&
          state.image_uploaded && state.wrap_s == GL_CLAMP && state.wrap_t == GL_CLAMP &&
          state.min_filter == GL_NEAREST && state.mag_filter == GL_NEAREST);
    const mc_client_texture id = value;
    CHECK(mc_client_render_engine_texture(f->engine, f->display, NULL, text(shadow, COUNT(shadow)),
                                          &value, f->error) == MC_CLIENT_LOOKUP_OK &&
          value == id && f->classes->b->count == ledger + 1);
    CHECK(clamp_gui(f->display, id, 0, 0, 0) == 0);
    uint8_t pixel[4];
    CHECK(clamp_pixel(pixel) == 0 && !memcmp(pixel, clamp_texels, 4));
    value = 0xdeadbeefu;
    CHECK(mc_client_render_engine_texture(f->engine, f->display, NULL,
                                          (mc_texture_pack_text){NULL, 0, true}, &value,
                                          f->error) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    mc_client_texture_lookup_attempt *foreign = f->engine->lookup->pending;
    CHECK(foreign != NULL && value == 0xdeadbeefu);
    CHECK(mc_client_render_engine_texture(f->engine, f->display, NULL, text(shadow, COUNT(shadow)),
                                          &value, f->error) == MC_CLIENT_LOOKUP_BUSY &&
          f->engine->lookup->pending == foreign && value == 0xdeadbeefu);
    CHECK(mc_client_display_texture_state_inspect(f->display, id, &state) &&
          state.wrap_s == GL_CLAMP);
    CHECK(mc_client_render_engine_retire_lookup(f->engine, f->error) == MC_CLIENT_LOOKUP_OK);
    puts("CLAMP_CONTROL genuine_cold_shadow_upload_cache_hit_and_foreign_lookup");
    return 0;
}

int main(int argc, char **argv) {
    SDL_SetMainReady();
    CHECK(argc == 3);
    fixture f;
    CHECK(open_fixture(&f, argv[1], argv[2], MC_TEXTURE_PACK_DEFAULT) == 0);
    CHECK(clamp_functions() == 0);
    clamp_disable(GL_DITHER);
    clamp_disable(GL_MULTISAMPLE);
    clamp_disable(GL_FRAMEBUFFER_SRGB);
    const mc_client_display_info *device = mc_client_display_info_get(f.display);
    CHECK(device != NULL);
    printf("CLAMP_DEVICE %s | %s | %s\n", device->vendor, device->renderer, device->version);
    mc_client_framebuffer_info format;
    CHECK(mc_client_display_framebuffer_info_get(f.display, &format, &clamp_error) &&
          format.red_bits == 8 && format.green_bits == 8 && format.blue_bits == 8 &&
          format.alpha_bits == 8 && !format.srgb_enabled);
    mc_client_texture id = 0;
    mc_client_texture_generation generation = {0}, second = {0};
    CHECK(mc_client_display_generate_texture(f.display, &id, &generation, &clamp_error));
    CHECK(clamp_parameters(generation.resource, GL_NEAREST, GL_REPEAT, GL_REPEAT, 0) == 0);
    const mc_image_view image = {2, 2, 8, clamp_texels};
    CHECK(mc_client_display_upload_existing_texture(generation.resource, &image, &clamp_error));
    mc_client_tessellator *nw = NULL;
    const mc_client_graphics_site caller = {"clamp_fixture", "main", "()V", 0};
    CHECK(mc_client_nw_active_use(f.classes, caller, &nw, &clamp_graphics));
    CHECK(clamp_samples(&f, generation.resource, id, nw) == 0);
    CHECK(clamp_lists(&f, generation.resource, id, nw) == 0);
    CHECK(clamp_staging(f.display, generation.resource) == 0);
    mc_client_texture second_id = 0;
    CHECK(mc_client_display_generate_texture(f.display, &second_id, &second, &clamp_error));
    CHECK(clamp_parameters(second.resource, GL_NEAREST, GL_REPEAT, GL_REPEAT, 0) == 0);
    mc_client_texture_source_state before, after;
    CHECK(mc_client_display_texture_state_inspect(f.display, id, &before));
    CHECK(!mc_client_display_texture_parameter(generation.resource, MC_CLIENT_TEXTURE_WRAP_S,
                                               GL_REPEAT, &clamp_error) &&
          clamp_error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(mc_client_display_texture_state_inspect(f.display, id, &after) &&
          clamp_equal(before, after));
    mc_client_display_texture_lease *lease = NULL;
    CHECK(mc_client_display_texture_lease_acquire(generation.resource, &lease, &clamp_error));
    CHECK(!mc_client_display_retire_texture(&generation.resource, &clamp_error) &&
          clamp_error.status == MC_CLIENT_DISPLAY_BUSY && generation.resource != NULL);
    CHECK(mc_client_display_texture_state_inspect(f.display, id, &after) &&
          clamp_equal(before, after));
    CHECK(!mc_client_display_destroy(f.display, &clamp_error) &&
          clamp_error.status == MC_CLIENT_DISPLAY_BUSY);
    CHECK(mc_client_display_texture_lease_release(&lease, &clamp_error));
    CHECK(mc_client_display_retire_texture(&generation.resource, &clamp_error));
    CHECK(!mc_client_display_texture_state_inspect(f.display, id, &after));
    CHECK(mc_client_display_texture_state_inspect(f.display, second_id, &after) &&
          after.generation != before.generation && after.wrap_s == GL_REPEAT &&
          after.wrap_t == GL_REPEAT);
    CHECK(mc_client_display_retire_texture(&second.resource, &clamp_error));
    puts("CLAMP_CONTROL exact_generation_binding_lease_and_retirement");
    CHECK(clamp_cold_lookup(&f) == 0);
    uint32_t error = UINT32_MAX;
    CHECK(mc_client_display_source_get_error(f.display, &error, &clamp_error) && error == 0);
    CHECK(close_fixture(&f) == 0);
    printf("Legacy clamp production: %u checks; 3872 GUI/NW pairs; 5 GE replays; 4 controls\n",
           checks);
    return 0;
}
