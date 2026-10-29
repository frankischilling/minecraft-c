/* Reuse genuine World/Player/picker setup; its earlier main is not executed. */
int mc_world_frame_picker_fixture_main(int argc, char **argv);
#define MC_PICKING_BODY_MAIN_NAME mc_world_frame_picker_fixture_main
#include "test_picking.c"
#undef main
#undef MC_PICKING_BODY_MAIN_NAME

#include "client/display/gl_internal.h"
#include "client/display/world_frame_surface.h"
#include "client/world/entity_renderer/renderer_fog_internal.h"
#include "mc/client/world/render_global.h"
#include "mc/client/world/world_frame.h"

#include <SDL3/SDL_video.h>
#include <xmmintrin.h>

static uint32_t frame_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool fog_gl_color(const mc_client_entity_renderer_fields *fields) {
    SDL_FunctionPointer address = SDL_GL_GetProcAddress("glGetFloatv");
    void(GLAPIENTRY * get_float)(GLenum, GLfloat *);
    _Static_assert(sizeof(get_float) == sizeof(address), "desktop GL function representation");
    memcpy(&get_float, &address, sizeof(get_float));
    CHECK(get_float != NULL);
    GLfloat actual[4];
    get_float(GL_COLOR_CLEAR_VALUE, actual);
    CHECK(frame_float_bits(actual[0]) == frame_float_bits(fields->g) &&
          frame_float_bits(actual[1]) == frame_float_bits(fields->h) &&
          frame_float_bits(actual[2]) == frame_float_bits(fields->i) && actual[3] == 0.0f);
    return true;
}

static bool fog_controls(picking_fixture *f, mc_game_settings *settings) {
    static const int64_t times[] = {0, 6000, 18000};
    mc_world_runtime *world = f->base.world;
    const mc_world_info_live *info = mc_world_info_live_get(world->fields.x);
    CHECK(info != NULL);
    world->fields.i = 0.2f;
    world->fields.j = 0.8f;
    world->fields.k = 0.1f;
    world->fields.l = 0.7f;
    world->fields.H = INT64_C(0xffffff);
    f->px->fields.L = 0.25f;
    f->px->fields.M = 0.875f;
    for (size_t time = 0; time < sizeof(times) / sizeof(times[0]); ++time)
        for (int32_t distance = 0; distance < 4; ++distance)
            for (int32_t anaglyph = 0; anaglyph < 2; ++anaglyph)
                for (int32_t branch = 0; branch < 4; ++branch) {
                    *info->time = times[time];
                    mc_game_settings_state before;
                    CHECK(mc_game_settings_read_state(settings, &before));
                    before.values.render_distance = distance;
                    before.values.anaglyph = anaglyph != 0;
                    CHECK(mc_game_settings_set_values(settings, &before.values, NULL) ==
                          MC_GAME_SETTINGS_OK);
                    f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = branch == 1   ? 9
                                                                                 : branch == 2 ? 11
                                                                                               : 0;
                    f->px->fields.D = branch == 3;
                    CHECK(mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
                    const size_t bindings = world->persistent_bindings;
                    const size_t camera_holds = f->app->i->holds;
                    mc_client_renderer_fog_call *call = NULL;
                    mc_client_renderer_fog_error error;
                    mc_client_renderer_fog_state state;
                    /* Entered child borrows this prepared enclosing native guard. */
                    f->px->active = true;
                    CHECK(entity_renderer_fog_entered(f->px, f->math, 0.375f, &call, &error) ==
                              MC_CLIENT_RENDERER_FOG_OK &&
                          call != NULL && f->px->active &&
                          mc_client_entity_renderer_fog_inspect(call, &state));
                    CHECK(state.returned && state.source_pc == 571 && state.clear_color_set &&
                          state.sky_returned && state.fog_returned && state.rain_returned &&
                          state.thunder_returned && state.brightness_applied &&
                          state.anaglyph == (anaglyph != 0) &&
                          state.last_store_pc == (anaglyph ? 552u : 453u) &&
                          state.cloud_returned == (branch == 3) &&
                          state.water_tested == (branch != 3) && state.in_water == (branch == 1) &&
                          state.lava_tested == (branch == 0 || branch == 2) &&
                          state.in_lava == (branch == 2) &&
                          world->persistent_bindings == bindings + 1);
                    CHECK(fog_gl_color(&f->px->fields));
                    printf("FOG %" PRId64 " %" PRId32 " %" PRId32 " %" PRId32 " %08" PRIx32
                           " %08" PRIx32 " %08" PRIx32 "\n",
                           times[time], distance, anaglyph, branch,
                           frame_float_bits(f->px->fields.g), frame_float_bits(f->px->fields.h),
                           frame_float_bits(f->px->fields.i));
                    CHECK(mc_client_entity_renderer_fog_retire(&call, &error) ==
                              MC_CLIENT_RENDERER_FOG_OK &&
                          call == NULL && f->px->active && world->persistent_bindings == bindings);
                    f->px->active = false;
                    CHECK(f->app->i->holds == camera_holds);
                }
    f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = 0;
    f->px->fields.D = false;
    mc_client_renderer_fog_call *unsupported = NULL;
    mc_client_renderer_fog_error unsupported_error;
    mc_client_renderer_fog_state unsupported_state;
    const uint32_t old_red = frame_float_bits(f->px->fields.g);
    const unsigned int controls = _mm_getcsr();
    _mm_setcsr(controls | 0x8000u);
    f->px->active = true;
    const mc_client_renderer_fog_status unsupported_status =
        entity_renderer_fog_entered(f->px, f->math, 0.375f, &unsupported, &unsupported_error);
    _mm_setcsr(controls); /* Test restores only its own authored native control. */
    CHECK(unsupported_status == MC_CLIENT_RENDERER_FOG_UNSUPPORTED_FP && unsupported != NULL &&
          unsupported_error.site.pc == 29 && unsupported_error.native_policy && f->px->active &&
          mc_client_entity_renderer_fog_inspect(unsupported, &unsupported_state) &&
          unsupported_state.world_captured && unsupported_state.camera_captured &&
          unsupported_state.distance_read && !unsupported_state.blend_computed &&
          frame_float_bits(f->px->fields.g) == old_red);
    CHECK(mc_client_entity_renderer_fog_retire(&unsupported, &unsupported_error) ==
              MC_CLIENT_RENDERER_FOG_OK &&
          unsupported == NULL && f->px->active);
    f->px->active = false;
    /* A reached native clear-color failure preserves all preceding RGB and
     * World/Vec effects, then checked child retirement releases occurrences. */
    mc_client_display *display = f->px->services.display;
    f->px->services.display = NULL;
    mc_client_renderer_fog_call *failed = NULL;
    mc_client_renderer_fog_state failed_state;
    mc_client_renderer_fog_error failure;
    const size_t bindings = world->persistent_bindings;
    f->px->active = true;
    CHECK(entity_renderer_fog_entered(f->px, f->math, 0.375f, &failed, &failure) ==
              MC_CLIENT_RENDERER_FOG_NATIVE_FAILED &&
          failed != NULL && f->px->active && failure.site.pc == 568 && failure.native_policy &&
          mc_client_entity_renderer_fog_inspect(failed, &failed_state));
    CHECK(!failed_state.returned && failed_state.brightness_applied &&
          failed_state.last_store_pc == 552 && !failed_state.clear_color_set &&
          failed_state.sky != NULL && failed_state.fog != NULL &&
          world->persistent_bindings == bindings + 1);
    const uint32_t retained_rgb[] = {frame_float_bits(f->px->fields.g),
                                     frame_float_bits(f->px->fields.h),
                                     frame_float_bits(f->px->fields.i)};
    CHECK(mc_client_entity_renderer_fog_retire(&failed, &failure) == MC_CLIENT_RENDERER_FOG_OK &&
          failed == NULL && f->px->active && world->persistent_bindings == bindings &&
          retained_rgb[0] == frame_float_bits(f->px->fields.g) &&
          retained_rgb[1] == frame_float_bits(f->px->fields.h) &&
          retained_rgb[2] == frame_float_bits(f->px->fields.i));
    f->px->active = false;
    f->px->services.display = display;
    return true;
}

static bool frame_controls(picking_fixture *f, const char *directory) {
    CHECK(picking_case_open(f, directory));
    mc_client_display *display = NULL;
    mc_client_display_error display_error;
    CHECK(mc_client_display_create("World-frame source controls", 64, 64, true, &display,
                                   &display_error));
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = settings;
    f->app->entities = f->base.entities;
    f->app->registry = f->base.registry;
    f->app->display = display;
    f->app->d = 64;
    f->app->e = 64;
    f->px->services.display = display;
    CHECK(mc_client_entity_renderer_class_active_use(f->models, &f->owner_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    /* Prepared native admission only: no PX constructor/source return claim. */
    f->px->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    f->px->fields.E = 1.0;
    mc_entity *camera = f->actors[0];
    camera->base.bl = 7.5;
    camera->base.bm = 63.5;
    camera->base.bn = 6.5;
    CHECK(fog_controls(f, settings));
    static const int32_t distances[] = {INT32_MIN, 0, 1, 2, 3, 4, 5, INT32_MAX};
    mc_client_world_frame_call *call = NULL;
    mc_client_world_frame_state state;
    mc_client_world_frame_error error;
    for (size_t index = 0; index < sizeof(distances) / sizeof(distances[0]); ++index) {
        mc_game_settings_state before;
        CHECK(mc_game_settings_read_state(settings, &before));
        before.values.render_distance = distances[index];
        before.values.anaglyph = index % 2 != 0;
        CHECK(mc_game_settings_set_values(settings, &before.values, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
        const size_t holds = f->px->holds;
        CHECK(mc_client_entity_renderer_world_frame_begin(f->px, f->math, 0.5f, 123456789, &call,
                                                          &error) ==
                  MC_CLIENT_WORLD_FRAME_SOURCE_NULL &&
              call != NULL && mc_client_entity_renderer_world_frame_inspect(call, &state));
        CHECK(!state.returned && !state.active && state.cull_enabled && state.depth_enabled &&
              state.picking_returned && state.provider_read && state.anaglyph_read &&
              state.anaglyph == before.values.anaglyph && state.viewport_set && state.width == 64 &&
              state.height == 64 && state.render_x == 8.0 && state.render_y == 64.0 &&
              state.render_z == 7.5 && state.deadline == 123456789 && state.camera == camera &&
              state.fog_blend_computed && state.fog_render_distance == distances[index]);
        CHECK(error.site.pc == 865 && strcmp(error.site.method, "g") == 0 && error.camera != NULL &&
              !error.native_policy && f->px->holds == holds + 1 && !f->px->active);
        CHECK(state.fog_returned && state.buffers_cleared && state.post_fog_cull_enabled &&
              state.fog != NULL && state.fog->clear_color_set && state.camera_setup != NULL &&
              state.camera_setup->projection_reset && state.camera_setup->modelview_reset &&
              state.camera_setup->fov_returned && !state.camera_returned);
        CHECK(state.color_masked == state.anaglyph && state.eye_stored == state.anaglyph);
        if (state.anaglyph)
            CHECK(f->models->px_b == 0);
        const uint32_t blend = frame_float_bits(state.fog_distance_blend);
        if ((blend & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000))
            printf("BLEND %" PRId32 " NAN\n", distances[index]);
        else
            printf("BLEND %" PRId32 " %08" PRIx32 "\n", distances[index], blend);
        mc_client_world_frame_call *same = call;
        CHECK(mc_client_entity_renderer_world_frame_begin(f->px, f->math, 1, 0, &call, &error) ==
                  MC_CLIENT_WORLD_FRAME_ARGUMENT &&
              call == same && f->px->holds == holds + 1);
        mc_client_world_frame_call *another = NULL;
        CHECK(mc_client_entity_renderer_world_frame_begin(f->px, f->math, 1, 0, &another, &error) ==
                  MC_CLIENT_WORLD_FRAME_BUSY &&
              another == NULL && f->px->world_frame_call == call && f->px->holds == holds + 1);
        CHECK(mc_client_entity_renderer_world_frame_retire(&call, &error) ==
                  MC_CLIENT_WORLD_FRAME_OK &&
              call == NULL && f->px->holds == holds);
    }
#ifdef MC_TEST_HAS_RENDER_GLOBAL
    mc_client_render_global *global = NULL;
    CHECK(mc_client_render_global_allocate(f->graphics, NULL, &global, NULL) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    f->app->g = global; /* Actual RAW N is enough for its literal false cloud query. */
    const mc_client_source_dr *frustum_identity = NULL;
    for (int32_t distance = 0; distance <= 2; distance += 2) {
        mc_game_settings_state settings_state;
        CHECK(mc_game_settings_read_state(settings, &settings_state));
        settings_state.values.render_distance = distance;
        settings_state.values.anaglyph = false;
        CHECK(mc_game_settings_set_values(settings, &settings_state.values, NULL) ==
              MC_GAME_SETTINGS_OK);
        CHECK(mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
        CHECK(mc_client_entity_renderer_world_frame_begin(f->px, f->math, 0.5f, 0, &call, &error) ==
                  MC_CLIENT_WORLD_FRAME_DEPENDENCY &&
              call != NULL && mc_client_entity_renderer_world_frame_inspect(call, &state));
        CHECK(state.camera_returned && state.frustum_returned && state.frustum.initialized &&
              state.frustum.normalized_planes == 6 && state.fog_distance_branch_read &&
              state.post_camera_render_distance == distance && error.native_policy &&
              error.site.pc == (distance == 0 ? 306 : 318));
        if (frustum_identity == NULL)
            frustum_identity = state.frustum.singleton;
        CHECK(state.frustum.singleton == frustum_identity);
        CHECK(mc_client_entity_renderer_world_frame_retire(&call, &error) ==
                  MC_CLIENT_WORLD_FRAME_OK &&
              call == NULL);
    }
    f->app->g = NULL;
    CHECK(mc_client_render_global_retire(&global, NULL) == MC_CLIENT_RENDER_GLOBAL_OK);
#endif
    CHECK(mc_client_display_source_color_mask(display, true, true, true, true, &display_error));
    CHECK(mc_client_display_check_gl(display, "World-frame source prefix", &display_error));
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = NULL;
    f->app->display = NULL;
    f->px->services.display = NULL;
    CHECK(mc_client_display_destroy(display, &display_error));
    f->px->phase = MC_CLIENT_ENTITY_RENDERER_RAW;
    CHECK(picking_case_close(f));
    CHECK(mc_client_entity_renderer_retire(&f->px, &f->owner_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    CHECK(mc_client_frustum_namespace_retire(f->models, NULL) == MC_CLIENT_FRUSTUM_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !frame_controls(f, argv[2]) || !picking_close(f))
        return 1;
    free(f);
    printf("Source World frame: %u checks; full fog and camera source prefix retained\n", checks);
    return 0;
}
