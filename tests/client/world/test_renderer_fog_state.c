int mc_fog_state_picker_fixture_main(int argc, char **argv);
#define MC_PICKING_BODY_MAIN_NAME mc_fog_state_picker_fixture_main
#include "test_picking.c"
#undef main
#undef MC_PICKING_BODY_MAIN_NAME

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/gl_internal.h"
#include "client/display/startup_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render_engine/internal.h"
#include "client/world/entity_renderer/renderer_fog_state_internal.h"
#include "mc/world/dimension/world_chunk_manager.h"

#include <SDL3/SDL_video.h>

static uint32_t fog_state_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void fog_state_row(const mc_client_source_fog_state *s, bool nv, int32_t dimension,
                          int32_t anaglyph, int32_t pass, int32_t branch,
                          const mc_client_renderer_fog_state_view *view) {
    printf("FOG_STATE %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32, dimension,
           anaglyph, pass, branch, s->mode);
    printf(" %08" PRIx32 " %08" PRIx32 " %08" PRIx32, fog_state_bits(s->density),
           fog_state_bits(s->start), fog_state_bits(s->end));
    for (size_t i = 0; i < 4; ++i)
        printf(" %08" PRIx32, fog_state_bits(s->color[i]));
    for (size_t i = 0; i < 3; ++i)
        printf(" %08" PRIx32, fog_state_bits(s->normal[i]));
    for (size_t i = 0; i < 4; ++i)
        printf(" %08" PRIx32, fog_state_bits(s->current_color[i]));
    printf(" %d %" PRId32 " %" PRId32 " %d %" PRId32 " %zu %zu %zu\n", s->color_material ? 1 : 0,
           s->material_face, s->material_mode, nv ? 1 : 0, nv ? s->distance_mode : -1,
           view->buffer_state.position, view->buffer_state.limit, view->buffer_state.capacity);
}

static bool fog_pixel_controls(picking_fixture *f, mc_client_display *display) {
    mc_client_display_error error;
    mc_client_startup_projection_stage stage;
    CHECK(mc_client_display_startup_projection(display, 64, 64, &stage, &error));
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, &error));
    CHECK(mc_client_display_source_load_identity(display, &error));
    CHECK(mc_client_display_source_scale(display, 0.125, 0.125, 0.03125, &error));
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &error));
    CHECK(mc_client_display_source_load_identity(display, &error));
    CHECK(mc_client_display_source_viewport(display, 64, 64, &error));
    CHECK(mc_client_display_source_texture_enable(display, false, &error));
    CHECK(mc_client_display_source_alpha_test(display, false, &error));
    CHECK(
        mc_client_display_source_disable_unlit_feature(display, MC_CLIENT_SOURCE_LIGHTING, &error));
    CHECK(mc_client_display_source_color_material_enable(display, false, &error));
    CHECK(mc_client_display_source_blend_enable(display, false, &error));
    CHECK(mc_client_display_source_depth_test(display, false, &error));
    CHECK(mc_client_display_source_cull_test(display, false, &error));
    /* Authored raster controls match the separate original pixel profile. */
    SDL_FunctionPointer address = SDL_GL_GetProcAddress("glDisable");
    mc_client_gl_disable_fn disable;
    _Static_assert(sizeof(disable) == sizeof(address), "desktop GL function representation");
    memcpy(&disable, &address, sizeof(disable));
    CHECK(disable != NULL);
    disable(GL_DITHER);
    disable(GL_SCISSOR_TEST);
    disable(GL_MULTISAMPLE);
    CHECK(mc_client_display_source_fog_enable(display, true, &error));
    const float black[] = {0, 0, 0, 1};
    CHECK(mc_client_display_source_fog_color(display, black, &error));
    CHECK(mc_client_display_source_fog_f(display, GL_FOG_START, 0, &error));
    CHECK(mc_client_display_source_fog_f(display, GL_FOG_END, 24, &error));
    CHECK(mc_client_display_source_color(display, 1, 1, 1, 1, &error));
    CHECK(mc_client_display_source_clear_color(display, 0, 0, 0, 0, &error));
    mc_client_render_arena arena = {.options = mc_client_render_engine_default_options()};
    mc_client_direct_byte_buffer *bytes = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_ge_c(f->graphics, &arena, 96,
                         (mc_client_graphics_site){"test", "fog pixels", "()V", 0}, &bytes,
                         &graphics));
    const float positions[3][3] = {{-6, -6, -2}, {6, -6, -18}, {0, 6, -2}};
    for (size_t i = 0; i < 3; ++i)
        memcpy(bytes->data + i * 32, positions[i], sizeof(positions[i]));
    mc_client_display_context_lease *lease = NULL;
    CHECK(mc_client_display_context_acquire(display, &lease, &error));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, bytes, 0, &error));
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true, &error));
    for (mc_client_source_array array = MC_CLIENT_SOURCE_UV_ARRAY;
         array <= MC_CLIENT_SOURCE_NORMAL_ARRAY; array = (mc_client_source_array)(array + 1))
        CHECK(mc_client_display_source_array(display, array, false, &error));
    static const int32_t modes[] = {GL_LINEAR, GL_EXP, GL_EXP2};
    static const char *const tags[] = {"FOG_PIXEL", "FOG_PIXEL_EXP", "FOG_PIXEL_EXP2"};
    static const int points[5][2] = {{16, 16}, {32, 16}, {32, 32}, {24, 24}, {40, 24}};
    for (size_t mode = 0; mode < 3; ++mode) {
        CHECK(mc_client_display_source_fog_i(display, GL_FOG_MODE, modes[mode], &error));
        if (mode != 0)
            CHECK(mc_client_display_source_fog_f(display, GL_FOG_DENSITY, 0.1f, &error));
        for (int32_t selector = 34139; selector <= 34140; ++selector) {
            CHECK(mc_client_display_source_fog_i(display, 34138, selector, &error));
            CHECK(mc_client_display_source_clear(display, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                                                 &error));
            CHECK(mc_client_display_source_batch(lease, bytes, bytes->data, 96, 3, GL_TRIANGLES,
                                                 &error));
            mc_client_capture capture = {0};
            CHECK(mc_client_display_capture_rgba(display, &capture, &error) &&
                  capture.width == 64 && capture.height == 64);
            for (size_t i = 0; i < 5; ++i) {
                const size_t row = (size_t)(capture.height - 1 - points[i][1]);
                const uint8_t *pixel =
                    capture.pixels + row * capture.stride + (size_t)points[i][0] * 4;
                printf("%s %" PRId32 " %d %d %u %u %u %u\n", tags[mode], selector, points[i][0],
                       points[i][1], pixel[0], pixel[1], pixel[2], pixel[3]);
            }
            mc_client_capture_free(&capture);
        }
    }
    CHECK(mc_client_display_context_release(&lease, &error));
    mc_client_render_arena_free(&arena);
    CHECK(mc_client_display_source_fog_enable(display, false, &error));
    return true;
}

static bool fog_state_failures(picking_fixture *f) {
    mc_client_renderer_fog_state_call *call = NULL;
    mc_client_renderer_fog_state_view state;
    mc_client_renderer_fog_state_error error;
    mc_client_direct_float_buffer *buffer = f->px->fields.f;
    f->px->fields.f = NULL;
    f->px->active = true;
    CHECK(entity_renderer_fog_state_entered(f->px, -1, 0, &call, &error) ==
              MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL &&
          error.site.pc == 4 &&
          strcmp(error.site.descriptor, "(FFFF)Ljava/nio/FloatBuffer;") == 0 &&
          !error.native_policy && mc_client_entity_renderer_fog_state_inspect(call, &state) &&
          !state.color_set && f->px->active);
    CHECK(mc_client_entity_renderer_fog_state_retire(&call, &error) ==
          MC_CLIENT_RENDERER_FOG_STATE_OK);
    f->px->active = false;
    f->px->fields.f = buffer;
    mc_game_settings *settings = f->app->settings;
    f->app->settings = NULL;
    f->px->fields.D = true;
    f->px->active = true;
    CHECK(entity_renderer_fog_state_entered(f->px, -1, 0, &call, &error) ==
              MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL &&
          error.site.pc == 85 && !error.native_policy &&
          mc_client_entity_renderer_fog_state_inspect(call, &state) && state.color_set &&
          state.mode_set && state.density_set && !state.returned &&
          state.buffer_state.position == 0 && state.buffer_state.limit == 4);
    CHECK(mc_client_entity_renderer_fog_state_retire(&call, &error) ==
          MC_CLIENT_RENDERER_FOG_STATE_OK);
    f->px->active = false;
    f->app->settings = settings;
    f->px->fields.D = false;
    mc_entity *camera = f->app->i;
    f->app->i = NULL;
    f->px->active = true;
    CHECK(entity_renderer_fog_state_entered(f->px, 0, 0, &call, &error) ==
              MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL &&
          error.site.pc == 164 && !error.native_policy &&
          mc_client_entity_renderer_fog_state_inspect(call, &state) && state.color_set &&
          state.white_set && !state.mode_set);
    CHECK(mc_client_entity_renderer_fog_state_retire(&call, &error) ==
          MC_CLIENT_RENDERER_FOG_STATE_OK);
    f->px->active = false;
    f->app->i = camera;
    f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = 0;
    f->app->active_world = NULL;
    f->px->active = true;
    CHECK(entity_renderer_fog_state_entered(f->px, 0, 0, &call, &error) ==
              MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL &&
          error.site.pc == 480 && !error.native_policy &&
          mc_client_entity_renderer_fog_state_inspect(call, &state) && state.start_set &&
          state.end_set && state.capability_read && !state.dimension_read);
    CHECK(mc_client_entity_renderer_fog_state_retire(&call, &error) ==
          MC_CLIENT_RENDERER_FOG_STATE_OK);
    f->px->active = false;
    f->app->active_world = f->base.world;
    mc_client_direct_float_buffer *short_buffer = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_ge_e(f->graphics, &f->px->arena, 2,
                         (mc_client_graphics_site){"test", "short buffer", "()V", 0}, &short_buffer,
                         &graphics));
    f->px->fields.f = short_buffer;
    f->px->active = true;
    CHECK(entity_renderer_fog_state_entered(f->px, 0, 0, &call, &error) ==
              MC_CLIENT_RENDERER_FOG_STATE_CHILD_FAILED &&
          error.site.pc == 21 && error.graphics.status == MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW &&
          !error.native_policy && mc_client_entity_renderer_fog_state_inspect(call, &state) &&
          state.buffer_puts == 2 && state.buffer_state.position == 2 && !state.color_set);
    CHECK(mc_client_entity_renderer_fog_state_retire(&call, &error) ==
          MC_CLIENT_RENDERER_FOG_STATE_OK);
    f->px->active = false;
    f->px->fields.f = buffer;
    return true;
}

static bool fog_state_controls(picking_fixture *f, const char *directory) {
    CHECK(picking_case_open(f, directory));
    mc_client_display *display = NULL;
    mc_client_display_error display_error;
    CHECK(mc_client_display_create("Source PX fog state", 64, 64, true, &display, &display_error));
    SDL_FunctionPointer driver_address = SDL_GL_GetProcAddress("glGetString");
    mc_client_gl_get_string_fn driver_string;
    _Static_assert(sizeof(driver_string) == sizeof(driver_address),
                   "desktop GL function representation");
    memcpy(&driver_string, &driver_address, sizeof(driver_string));
    CHECK(driver_string != NULL && driver_string(GL_VENDOR) != NULL &&
          driver_string(GL_RENDERER) != NULL && driver_string(GL_VERSION) != NULL);
    printf("FOG_DRIVER %s | %s | %s\n", (const char *)driver_string(GL_VENDOR),
           (const char *)driver_string(GL_RENDERER), (const char *)driver_string(GL_VERSION));
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = settings;
    f->app->display = display;
    f->app->registry = f->base.registry;
    f->app->entities = f->base.entities;
    f->px->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    f->px->services.display = display;
    mc_client_graphics_error graphics;
    CHECK(mc_client_ge_e(
        f->graphics, &f->px->arena, 16,
        (mc_client_graphics_site){"px", "<init>", "(Lnet/minecraft/client/Minecraft;)V", 193},
        &f->px->fields.f, &graphics));
    f->px->fields.g = .2f;
    f->px->fields.h = .4f;
    f->px->fields.i = .6f;
    f->px->fields.k = 256;
    mc_world_provider *nether = NULL;
    CHECK(mc_world_provider_create(-1, NULL, &nether, NULL) == MC_WORLD_PROVIDER_OK);
    const mc_world_provider_registry_binding binding = {f->base.registry, MC_REGISTRY_CLIENT,
                                                        f->base.profile};
    CHECK(mc_world_provider_bind_game(nether, f->base.world, &binding, NULL) ==
          MC_WORLD_PROVIDER_OK);
    mc_world_provider *dimensions[] = {f->base.world->fields.t, nether};
    CHECK(mc_client_display_source_color(display, 1, 1, 1, 1, &display_error));
    bool nv;
    CHECK(mc_client_display_source_nv_fog_distance(display, &nv, &display_error));
    mc_client_source_fog_state native_state;
    CHECK(mc_client_display_source_fog_inspect(display, &native_state));
    printf("FOG_INITIAL %d %" PRId32 "\n", nv ? 1 : 0, nv ? native_state.distance_mode : -1);
    static const int32_t passes[] = {-1, 0, 1};
    for (int32_t dimension = 0; dimension < 2; ++dimension)
        for (int32_t anaglyph = 0; anaglyph < 2; ++anaglyph)
            for (size_t p = 0; p < 3; ++p)
                for (int32_t branch = 0; branch < 4; ++branch) {
                    const int32_t pass = passes[p];
                    f->base.world->fields.t = dimensions[dimension];
                    mc_game_settings_state current;
                    CHECK(mc_game_settings_read_state(settings, &current));
                    current.values.anaglyph = anaglyph != 0;
                    CHECK(mc_game_settings_set_values(settings, &current.values, NULL) ==
                          MC_GAME_SETTINGS_OK);
                    f->px->fields.D = branch == 0;
                    f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | 64u] =
                        (uint8_t)(branch == 1   ? 9
                                  : branch == 2 ? 11
                                                : 0);
                    uint32_t partial_bits = pass < 0    ? UINT32_C(0x3ec00000)
                                            : pass == 0 ? UINT32_C(0xff800000)
                                                        : UINT32_C(0x7fc12345);
                    float partial;
                    memcpy(&partial, &partial_bits, sizeof(partial));
                    const size_t bindings = f->base.world->persistent_bindings,
                                 camera_holds = f->app->i->holds;
                    mc_client_renderer_fog_state_call *call = NULL;
                    mc_client_renderer_fog_state_error error;
                    mc_client_renderer_fog_state_view state;
                    f->px->active = true;
                    CHECK(entity_renderer_fog_state_entered(f->px, pass, partial, &call, &error) ==
                              MC_CLIENT_RENDERER_FOG_STATE_OK &&
                          call != NULL && f->px->active &&
                          mc_client_entity_renderer_fog_state_inspect(call, &state));
                    CHECK(state.returned && state.source_pc == 511 && state.helper_pc == 42 &&
                          state.buffer_puts == 4 && state.color_set && state.white_set &&
                          state.normal_set && state.color_material_enabled &&
                          state.color_material_selected && state.cloud == (branch == 0) &&
                          state.water_tested == (branch != 0) && state.in_water == (branch == 1) &&
                          state.lava_tested == (branch == 2 || branch == 3) &&
                          state.in_lava == (branch == 2) &&
                          state.capability_read == (branch == 3) &&
                          state.dimension_read == (branch == 3) &&
                          state.dimension_override == (dimension == 1 && branch == 3) &&
                          fog_state_bits(state.partial) == partial_bits &&
                          f->app->i->holds == camera_holds + 1 &&
                          f->base.world->persistent_bindings == bindings + (branch == 3 ? 1u : 0u));
                    CHECK(mc_client_display_source_fog_inspect(display, &native_state) &&
                          native_state.current_color_known);
                    fog_state_row(&native_state, nv, dimension, anaglyph, pass, branch, &state);
                    CHECK(mc_client_entity_renderer_fog_state_retire(&call, &error) ==
                              MC_CLIENT_RENDERER_FOG_STATE_OK &&
                          call == NULL && f->px->active && f->app->i->holds == camera_holds &&
                          f->base.world->persistent_bindings == bindings);
                    f->px->active = false;
                }
    f->base.world->fields.t = dimensions[0];
    CHECK(fog_state_failures(f));
    CHECK(fog_pixel_controls(f, display));
    f->px->fields.D = false;
    f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = 0;
    mc_world_provider_unbind(nether);
    mc_world_provider_release(nether);
    CHECK(mc_client_display_check_gl(display, "source fog state", &display_error));
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = NULL;
    f->app->display = NULL;
    f->px->services.display = NULL;
    CHECK(mc_client_display_destroy(display, &display_error));
    f->px->phase = MC_CLIENT_ENTITY_RENDERER_RAW;
    CHECK(picking_case_close(f));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !fog_state_controls(f, argv[2]) ||
        !picking_close(f))
        return 1;
    free(f);
    printf("Source PX fog state: %u checks; 48 scalar rows and 6 authored GPU profiles\n", checks);
    return 0;
}
