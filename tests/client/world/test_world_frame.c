/* Reuse genuine World/Player/picker setup; its earlier main is not executed. */
int mc_world_frame_picker_fixture_main(int argc, char **argv);
#define MC_PICKING_BODY_MAIN_NAME mc_world_frame_picker_fixture_main
#include "test_picking.c"
#undef main
#undef MC_PICKING_BODY_MAIN_NAME

#include "client/display/world_frame_surface.h"
#include "mc/client/world/world_frame.h"

static uint32_t frame_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
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
    f->app->display = display;
    f->app->d = 64;
    f->app->e = 64;
    f->px->services.display = display;
    CHECK(mc_client_entity_renderer_class_active_use(f->models, &f->owner_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    /* Prepared native admission only: no PX constructor/source return claim. */
    f->px->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    mc_entity *camera = f->actors[0];
    camera->base.bl = 7.5;
    camera->base.bm = 63.5;
    camera->base.bn = 6.5;
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
                  MC_CLIENT_WORLD_FRAME_DEPENDENCY &&
              call != NULL && mc_client_entity_renderer_world_frame_inspect(call, &state));
        CHECK(!state.returned && !state.active && state.cull_enabled && state.depth_enabled &&
              state.picking_returned && state.provider_read && state.anaglyph_read &&
              state.anaglyph == before.values.anaglyph && state.viewport_set && state.width == 64 &&
              state.height == 64 && state.render_x == 8.0 && state.render_y == 64.0 &&
              state.render_z == 7.5 && state.deadline == 123456789 && state.camera == camera &&
              state.fog_blend_computed && state.fog_render_distance == distances[index]);
        CHECK(error.site.pc == 56 && strcmp(error.site.method, "h") == 0 &&
              strcmp(error.child.class_name, "fd") == 0 && error.child.pc == 0 &&
              error.native_policy && f->px->holds == holds + 1 && !f->px->active);
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
    CHECK(mc_client_display_source_color_mask(display, true, true, true, true, &display_error));
    CHECK(mc_client_display_check_gl(display, "World-frame source prefix", &display_error));
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
    if (f == NULL || !picking_open(f, argv[1]) || !frame_controls(f, argv[2]) || !picking_close(f))
        return 1;
    free(f);
    printf("Source World frame: %u checks; fog sky-color dependency retained\n", checks);
    return 0;
}
