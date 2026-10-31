/* Actual camera methods on constructed GAME World/Player owners and prepared
 * real PX/App/N receivers. No complete PX/N construction or frame is claimed. */
int mc_camera_picker_fixture_main(int argc, char **argv);
#define MC_PICKING_BODY_MAIN_NAME mc_camera_picker_fixture_main
#include "test_picking.c"
#undef main
#undef MC_PICKING_BODY_MAIN_NAME
#include "client/display/camera_surface.h"
#include "client/world/entity_renderer/renderer_camera_internal.h"
#include "mc/client/world/render_global.h"
#include <SDL3/SDL_video.h>

static uint32_t camera_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void camera_operands(picking_fixture *f, int mode) {
    mc_entity *entity = f->actors[0];
    entity->base.x = 8.5;
    entity->base.y = 64.5;
    entity->base.z = 8.5;
    entity->base.aJ = 7.5;
    entity->base.aK = 63.5;
    entity->base.aL = 9.5;
    entity->base.yaw = 17;
    entity->base.aT = -8;
    entity->base.aU = 10;
    entity->base.aV = -4;
    entity->base.offset = 1.62f;
    mc_living_entity_fields *living = &entity->local_player->fields.living;
    living->Y = mode == 2 ? 0 : 20;
    living->aa = mode == 3 ? 6 : 0;
    living->ab = 10;
    living->ac = 27;
    living->ad = 17;
    living->af = 5;
    living->ag = 10;
    entity->base.bi = .75f;
    entity->base.bj = 1.25f;
    mc_player_fields *player = &entity->local_player->fields.player;
    player->h = .2f;
    player->i = .4f;
    player->C = mode == 8 ? .2f : 0;
    player->B = mode == 8 ? .35f : 0;
    player->u = mode == 11;
    mc_client_entity_renderer_fields *px = &f->px->fields;
    px->E = mode == 7 ? 2.0 : 1.0;
    px->F = mode == 7 ? .125 : 0;
    px->G = mode == 7 ? -.25 : 0;
    px->l = 37;
    px->u = 4.25f;
    px->t = 3.5f;
    px->v_to_C[0] = 30;
    px->v_to_C[1] = 15;
    px->v_to_C[2] = -5;
    px->v_to_C[3] = -15;
    px->v_to_C[4] = 2;
    px->v_to_C[5] = 1;
    px->v_to_C[6] = -2;
    px->v_to_C[7] = 1;
    px->D = true;
    f->app->i = f->app->h = entity;
    f->chunks[0]->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = (uint8_t)(mode == 1 ? 9 : 0);
}

static bool camera_controls(picking_fixture *f, const char *root) {
    CHECK(picking_case_open(f, root));
    mc_client_display *display = NULL;
    mc_client_display_error display_error;
    CHECK(mc_client_display_create("Source camera", 64, 64, true, &display, &display_error));
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = settings;
    f->app->display = display;
    f->app->entities = f->base.entities;
    f->app->registry = f->base.registry;
    f->app->d = 854;
    f->app->e = 480;
    f->px->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    CHECK(mc_client_entity_renderer_class_active_use(f->models, &f->owner_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    f->px->services.display = display;
    mc_client_render_global *global = NULL;
    CHECK(mc_client_render_global_allocate(f->graphics, NULL, &global, NULL) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    f->app->g = global; /* Genuine RAW n; reached n.a(DDDF) is literal false. */
    mc_client_renderer_camera_call *call = NULL;
    mc_client_renderer_camera_error error;
    mc_client_renderer_camera_state state;
    static const float partials[] = {0, .5f, 1};
    for (int mode = 0; mode < 12; ++mode) {
        mc_game_settings_state current;
        CHECK(mc_game_settings_read_state(settings, &current));
        current.values.render_distance = 2;
        current.values.view_bobbing = mode == 4;
        current.values.anaglyph = mode == 5 || mode == 6;
        current.values.debug_camera = mode == 9;
        current.values.third_person_view = mode == 9 || mode == 10;
        CHECK(mc_game_settings_set_values(settings, &current.values, NULL) == MC_GAME_SETTINGS_OK);
        for (int step = 0; step < 3; ++step) {
            camera_operands(f, mode);
            f->px->active = true;
            const mc_client_renderer_camera_status status = entity_renderer_camera_entered(
                f->px, f->math, partials[step], mode == 6 ? 1 : 0, &call, &error);
            f->px->active = false;
            if (status != MC_CLIENT_RENDERER_CAMERA_OK)
                fprintf(stderr, "Camera %d/%d stopped %s.%s:%d %s\n", mode, step,
                        error.site.class_name, error.site.method, error.site.pc, error.operation);
            CHECK(status == MC_CLIENT_RENDERER_CAMERA_OK && call != NULL &&
                  mc_client_entity_renderer_camera_inspect(call, &state) && state.returned &&
                  state.source_pc == 338 && state.k_stored && state.fov_returned &&
                  state.hurt_returned && state.orient_returned && state.D_stored &&
                  state.ray_calls == (mode == 10 ? 8u : 0u) && !f->px->fields.D);
            float projection[16], modelview[16];
            CHECK(mc_client_display_camera_matrices_inspect(display, projection, modelview));
            printf("CAMERA %d %d %08" PRIx32 " %d", mode, step, camera_bits(f->px->fields.k),
                   f->px->fields.D);
            for (size_t i = 0; i < 16; ++i)
                printf(" %08" PRIx32, camera_bits(projection[i]));
            for (size_t i = 0; i < 16; ++i)
                printf(" %08" PRIx32, camera_bits(modelview[i]));
            putchar('\n');
            printf("CAMERA_FIELDS %d %d %08" PRIx32 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
                   "\n",
                   mode, step, camera_bits(state.fov), picking_bits(f->px->fields.E),
                   picking_bits(f->px->fields.F), picking_bits(f->px->fields.G));
            CHECK(mc_client_entity_renderer_camera_retire(&call, NULL) ==
                      MC_CLIENT_RENDERER_CAMERA_OK &&
                  call == NULL);
        }
    }
    camera_operands(f, 0);
    f->app->g = NULL;
    f->px->active = true;
    const mc_client_renderer_camera_status null_n =
        entity_renderer_camera_entered(f->px, f->math, .5f, 0, &call, &error);
    f->px->active = false;
    CHECK(null_n == MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL && error.site.pc == 865 &&
          !error.native_policy && f->px->fields.D &&
          mc_client_entity_renderer_camera_inspect(call, &state) && state.modelview_reset &&
          !state.D_stored);
    CHECK(mc_client_entity_renderer_camera_retire(&call, NULL) == MC_CLIENT_RENDERER_CAMERA_OK);
    f->app->g = global;
    mc_game_settings_state current;
    CHECK(mc_game_settings_read_state(settings, &current));
    current.values.third_person_view = true;
    current.values.debug_camera = false;
    CHECK(mc_game_settings_set_values(settings, &current.values, NULL) == MC_GAME_SETTINGS_OK);
    camera_operands(f, 10);
    const size_t wall = (9u << 11u) | (6u << 7u) | 64u;
    f->chunks[0]->data->blocks[wall] = 1;
    f->px->active = true;
    const mc_client_renderer_camera_status collision =
        entity_renderer_camera_entered(f->px, f->math, 1, 0, &call, &error);
    f->px->active = false;
    CHECK(collision == MC_CLIENT_RENDERER_CAMERA_OK &&
          mc_client_entity_renderer_camera_inspect(call, &state) && state.ray_calls == 8 &&
          state.distance < 3.5 && state.distance > 0);
    CHECK(mc_client_entity_renderer_camera_retire(&call, NULL) == MC_CLIENT_RENDERER_CAMERA_OK);
    f->chunks[0]->data->blocks[wall] = 0;
    f->app->g = NULL;
    CHECK(mc_client_render_global_retire(&global, NULL) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = NULL;
    f->app->display = NULL;
    f->px->services.display = NULL;
    f->px->phase = MC_CLIENT_ENTITY_RENDERER_RAW;
    CHECK(mc_client_display_destroy(display, &display_error));
    CHECK(picking_case_close(f));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !camera_controls(f, argv[2]) || !picking_close(f))
        return 1;
    free(f);
    printf("Source camera: %u checks; 36 camera matrices and real collision rays\n", checks);
    return 0;
}
