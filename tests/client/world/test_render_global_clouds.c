/* Actual App/startup/N construction. World/camera fields are prepared source
 * operands; these controls do not qualify World loading or a complete frame. */
int mc_clouds_startup_fixture_main(int argc, char **argv);
#define main mc_clouds_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/internal.h"
#include "client/world/render_global_internal.h"
#include "client/world/render_global_sky_internal.h"
#include "entity/player/player_internal.h"
#include "mc/client/world/render_global_clouds.h"
#include "world/runtime/runtime_internal.h"

#include <inttypes.h>

static uint32_t cloud_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

typedef GLboolean(APIENTRY *cloud_enabled_fn)(GLenum);
typedef void(APIENTRY *cloud_boolean_fn)(GLenum, GLboolean *);

static int cloud_reset(mc_client_display *display, mc_client_render_engine_classes *graphics) {
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, NULL));
    CHECK(mc_client_display_source_load_identity(display, NULL));
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, NULL));
    CHECK(mc_client_display_source_load_identity(display, NULL));
    CHECK(mc_client_display_source_color(display, .125f, .25f, .5f, .75f, NULL));
    CHECK(mc_client_display_source_cull_test(display, false, NULL));
    CHECK(mc_client_display_source_texture_enable(display, true, NULL));
    CHECK(mc_client_display_source_alpha_test(display, true, NULL));
    CHECK(mc_client_display_source_alpha_function(display, GL_GREATER, .1f, NULL));
    CHECK(mc_client_display_source_blend_enable(display, false, NULL));
    CHECK(mc_client_display_source_fog_enable(display, false, NULL));
    CHECK(mc_client_display_source_depth_mask(display, true, NULL));
    CHECK(mc_client_display_source_shade_model(display, GL_SMOOTH, NULL));
    CHECK(mc_client_display_source_color_mask(display, true, true, true, true, NULL));
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(graphics, &nw) && nw.fields.g != NULL && nw.fields.B > 0 &&
          nw.fields.e != NULL && !nw.fields.w);
    /* Deliberately prepared native test operands, matching the original fixture.
     * Actual NW writes and untouched duplicate-normal words remain observable. */
    memset((void *)nw.fields.g, 0, (size_t)nw.fields.B * sizeof(*nw.fields.g));
    ((mc_client_direct_int_buffer *)(void *)nw.fields.e)->position = 0;
    return 0;
}

static int cloud_row(unsigned int index, unsigned int dim, unsigned int fancy, unsigned int height,
                     unsigned int anaglyph, unsigned int eye, unsigned int sample, fixture *runtime,
                     client_application *app, cloud_enabled_fn enabled, cloud_boolean_fn boolean) {
    /* Match the original CURRENT_COLOR query before matrix/state readbacks.
     * This query commits measured Mesa attributes; passive inspection does not. */
    float current_color[4], projection[16], modelview[16];
    CHECK(mc_client_display_source_current_attribute(app->display, GL_CURRENT_COLOR, current_color,
                                                     4, NULL));
    CHECK(mc_client_display_camera_matrices_inspect(app->display, projection, modelview));
    mc_client_source_draw_state draw;
    mc_client_source_fog_state fog;
    CHECK(mc_client_display_source_draw_inspect(app->display, &draw));
    CHECK(mc_client_display_source_fog_inspect(app->display, &fog));
    GLboolean depth, mask[4];
    boolean(GL_DEPTH_WRITEMASK, &depth);
    boolean(GL_COLOR_WRITEMASK, mask);
    mc_entity_vector_snapshot vectors;
    CHECK(mc_entity_vector_get_snapshot(runtime->services.entities, &vectors) == MC_ENTITY_OK);
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(runtime->graphics, &nw) && nw.fields.e != NULL &&
          nw.fields.e->position % 8 == 0 && nw.fields.e->position <= (size_t)nw.fields.B);
    const size_t words = nw.fields.e->position;
    printf("CLOUD_STATE %u %u %u %u %u %u %u", index, dim, fancy, height, anaglyph, eye, sample);
    for (size_t i = 0; i < 4; ++i)
        printf(" %08" PRIx32, cloud_bits(current_color[i]));
    printf(" %u %u %u %u %u %u %u", enabled(GL_CULL_FACE) ? 1u : 0u, enabled(GL_BLEND) ? 1u : 0u,
           draw.texture_enabled ? 1u : 0u, draw.alpha_test ? 1u : 0u, fog.enabled ? 1u : 0u,
           depth ? 1u : 0u, draw.shade_flat ? 7424u : 7425u);
    for (size_t i = 0; i < 4; ++i)
        printf(" %u", mask[i] ? 1u : 0u);
    printf(" %d", vectors.cursor);
    for (size_t i = 0; i < 16; ++i)
        printf(" %08" PRIx32, cloud_bits(modelview[i]));
    printf(" %zu\n", words);
    const size_t width = fancy ? 7u : 6u;
    printf("CLOUD_WORDS %u %zu %zu ", index, width, words / 8u);
    for (size_t vertex = 0; vertex < words / 8u; ++vertex)
        for (size_t component = 0; component < width; ++component)
            printf("%08" PRIx32, (uint32_t)nw.fields.g[vertex * 8u + component]);
    putchar('\n');
    return 0;
}

static int cloud_pixels(mc_client_render_global *owner, mc_math_classes *math,
                        client_application *app, mc_entity *camera) {
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(app->settings, &settings));
    settings.values.fancy_graphics = settings.values.anaglyph = false;
    CHECK(mc_game_settings_set_values(app->settings, &settings.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    camera->base.y = camera->base.bm = 108.5;
    CHECK(cloud_reset(app->display, owner->classes) == 0);
    CHECK(mc_client_display_source_rotate(app->display, 90, 1, 0, 0, NULL));
    CHECK(mc_client_display_source_clear_color(app->display, 0, 0, 0, 0, NULL));
    CHECK(mc_client_display_source_clear(app->display, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                                         NULL));
    mc_client_capture before = {0}, after = {0};
    CHECK(mc_client_display_capture_rgba(app->display, &before, NULL));
    mc_client_render_global_clouds_call *call = NULL;
    mc_client_render_global_clouds_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_client_render_global_clouds_begin(owner, math, .375f, &call, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_display_capture_rgba(app->display, &after, NULL));
    CHECK(before.width == after.width && before.height == after.height &&
          before.stride == after.stride);
    const size_t center =
        after.stride * (size_t)(after.height / 2) + 4u * (size_t)(after.width / 2);
    CHECK(before.pixels[center] == 0 && before.pixels[center + 1] == 0 &&
          before.pixels[center + 2] == 0);
    CHECK(after.pixels[center] > 0 && after.pixels[center + 1] > 0 && after.pixels[center + 2] > 0);
    CHECK(mc_client_render_global_clouds_retire(&call, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    mc_client_capture_free(&before);
    mc_client_capture_free(&after);
    free(error);
    return 0;
}

static int clouds_device(const char *resources, const char *assets) {
    fixture runtime;
    startup_fixture startup;
    CHECK(fixture_open(&runtime, resources) == 0 && startup_services(&startup) == 0 &&
          startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&runtime, false, &app) == 0 && statistics_session(app) == 0);
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options startup_options = mc_client_application_startup_default_options();
    startup_options.hidden = true;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &startup_options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    const mc_client_startup_status start = mc_client_application_run_prefix(attempt, startup.error);
    if (start != MC_CLIENT_STARTUP_UNIMPLEMENTED)
        fprintf(stderr, "Cloud startup: %d %s %s:%d\n", (int)start, startup.error->operation,
                startup.error->caller.class_name, startup.error->caller.pc);
    CHECK(start == MC_CLIENT_STARTUP_UNIMPLEMENTED && startup.error->caller.pc == 32 &&
          app->g != NULL);
    CHECK(statistics_retire_resources(attempt) == 0);
    mc_client_render_global *owner = app->g;
    mc_math_classes *math = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    mc_world_runtime_classes *world_classes = NULL;
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, runtime.services.profile,
                                          mc_math_classes_table(math), &world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(world_classes, runtime.services.registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(world_classes, runtime.services.entities) ==
          MC_ENTITY_OK);
    mc_world_runtime *world = NULL;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, world_classes,
                                          NULL, &world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &world->fields.x) == MC_WORLD_INFO_OK &&
          mc_world_info_construct(world->fields.x, 23, mc_world_info_name_empty()) ==
              MC_WORLD_INFO_OK);
    CHECK(mc_world_info_time_set(world->fields.x, 6000) == MC_WORLD_INFO_OK);
    world->fields.i = world->fields.j = world->fields.k = world->fields.l = 0;
    world->fields.H = INT64_C(0xffffff);
    mc_entity *camera = NULL;
    CHECK(mc_local_player_allocate(runtime.services.entities, &camera, NULL) == MC_ENTITY_OK);
    app->i = camera;
    app->active_world = owner->fields.k = world;
    owner->fields.x = 17;
    mc_world_provider *dimensions[3] = {0};
    const mc_world_provider_registry_binding binding = {
        runtime.services.registry, MC_REGISTRY_CLIENT, runtime.services.profile};
    const int32_t ids[] = {0, -1, 1};
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_world_provider_create(ids[i], NULL, &dimensions[i], NULL) == MC_WORLD_PROVIDER_OK);
        CHECK(mc_world_provider_bind_game(dimensions[i], world, &binding, NULL) ==
              MC_WORLD_PROVIDER_OK);
    }
    CHECK(mc_world_provider_retain(dimensions[0]) == MC_WORLD_PROVIDER_OK);
    world->fields.t = dimensions[0];
    cloud_enabled_fn enabled = NULL;
    cloud_boolean_fn boolean = NULL;
    SDL_FunctionPointer symbol = SDL_GL_GetProcAddress("glIsEnabled");
    memcpy(&enabled, &symbol, sizeof(symbol));
    symbol = SDL_GL_GetProcAddress("glGetBooleanv");
    memcpy(&boolean, &symbol, sizeof(symbol));
    CHECK(enabled != NULL && boolean != NULL);
    mc_client_render_global_clouds_call *call = NULL;
    mc_client_render_global_clouds_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    mc_game_settings_state options;
    CHECK(mc_game_settings_read_state(app->settings, &options));
    options.values.fancy_graphics = false;
    CHECK(mc_game_settings_set_values(app->settings, &options.values, NULL) == MC_GAME_SETTINGS_OK);
    mc_client_texture_lookup_error *lookup = calloc(1, sizeof(*lookup));
    CHECK(lookup != NULL);
    mc_client_texture texture;
    CHECK(mc_client_render_engine_texture(app->render_engine, app->display, NULL,
                                          (mc_texture_pack_text){NULL, 0, true}, &texture,
                                          lookup) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(mc_client_texture_lookup_pending(app->render_engine));
    CHECK(mc_client_render_global_clouds_begin(owner, math, .375f, &call, error) !=
          MC_CLIENT_RENDER_GLOBAL_OK);
    if (error->site.pc != 103 || error->texture.status != MC_CLIENT_LOOKUP_BUSY)
        fprintf(stderr, "Foreign cloud lookup %s.%s:%d status%d texture%d: %s\n",
                error->site.class_name, error->site.method, error->site.pc, (int)error->status,
                (int)error->texture.status, error->operation);
    CHECK(error->site.pc == 103 && error->texture.status == MC_CLIENT_LOOKUP_BUSY);
    CHECK(mc_client_render_global_clouds_retire(&call, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          call == NULL && mc_client_texture_lookup_pending(app->render_engine));
    CHECK(mc_client_render_engine_retire_lookup(app->render_engine, lookup) ==
              MC_CLIENT_LOOKUP_OK &&
          !mc_client_texture_lookup_pending(app->render_engine));
    free(lookup);
    const double heights[] = {64.5, 108.5, 120.5};
    const unsigned int anaglyphs[] = {0, 1, 1}, eyes[] = {0, 0, 1};
    unsigned int index = 0;
    for (unsigned int sample = 0; sample < 2; ++sample)
        for (unsigned int dim = 0; dim < 3; ++dim) {
            CHECK(mc_world_provider_retain(dimensions[dim]) == MC_WORLD_PROVIDER_OK);
            mc_world_provider *old = world->fields.t;
            world->fields.t = dimensions[dim];
            mc_world_provider_release(old);
            for (unsigned int fancy = 0; fancy < 2; ++fancy)
                for (unsigned int height = 0; height < 3; ++height) {
                    if (sample != 0 && height != 1)
                        continue;
                    for (unsigned int tuple = 0; tuple < 3; ++tuple) {
                        camera->base.x = sample == 0 ? 8.5 : -2049.25;
                        camera->base.z = sample == 0 ? 8.5 : 4097.75;
                        camera->base.aJ = camera->base.bl = camera->base.x - 1.0;
                        camera->base.aL = camera->base.bn = camera->base.z - 2.0;
                        camera->base.y = heights[height];
                        camera->base.bm = heights[height] - 1.0;
                        mc_game_settings_state settings;
                        CHECK(mc_game_settings_read_state(app->settings, &settings));
                        settings.values.fancy_graphics = fancy != 0;
                        settings.values.anaglyph = anaglyphs[tuple] != 0;
                        CHECK(mc_game_settings_set_values(app->settings, &settings.values, NULL) ==
                              MC_GAME_SETTINGS_OK);
                        runtime.services.render_classes->px_b = (int32_t)eyes[tuple];
                        CHECK(cloud_reset(app->display, runtime.graphics) == 0);
                        CHECK(mc_entity_vector_pool_reset(runtime.services.entities, NULL) ==
                              MC_ENTITY_OK);
                        const size_t held = owner->holds;
                        const mc_client_render_global_status status =
                            mc_client_render_global_clouds_begin(owner, math, .375f, &call, error);
                        if (status != MC_CLIENT_RENDER_GLOBAL_OK)
                            fprintf(stderr, "Cloud profile%u %s.%s:%d %s\n", index,
                                    error->site.class_name, error->site.method, error->site.pc,
                                    error->operation);
                        mc_client_render_global_clouds_state state;
                        CHECK(status == MC_CLIENT_RENDER_GLOBAL_OK && call != NULL &&
                              mc_client_render_global_clouds_inspect(call, &state) &&
                              state.returned);
                        CHECK(state.nether_skipped == (dim == 1) && owner->holds == held + 1);
                        CHECK(dim == 1 || (state.fancy == (fancy != 0) && state.vertices > 0 &&
                                           state.draws > 0));
                        CHECK(cloud_row(index, dim, fancy, height, anaglyphs[tuple], eyes[tuple],
                                        sample, &runtime, app, enabled, boolean) == 0);
                        mc_client_render_global_clouds_call *other = NULL;
                        CHECK(mc_client_render_global_clouds_begin(owner, math, .375f, &other,
                                                                   error) ==
                                  MC_CLIENT_RENDER_GLOBAL_BUSY &&
                              other == NULL);
                        mc_client_render_global *alias = owner;
                        CHECK(mc_client_render_global_retire(&alias, NULL) ==
                                  MC_CLIENT_RENDER_GLOBAL_BUSY &&
                              alias == owner);
                        CHECK(mc_client_render_global_clouds_retire(&call, error) ==
                                  MC_CLIENT_RENDER_GLOBAL_OK &&
                              call == NULL && owner->holds == held);
                        CHECK(mc_client_display_check_gl(app->display, "cloud native draw", NULL));
                        ++index;
                    }
                }
        }
    CHECK(index == 72);
    CHECK(mc_world_provider_retain(dimensions[0]) == MC_WORLD_PROVIDER_OK);
    mc_world_provider *old = world->fields.t;
    world->fields.t = dimensions[0];
    mc_world_provider_release(old);
    CHECK(cloud_pixels(owner, math, app, camera) == 0);
    /* Null App wins before every World/Settings/GL read on the actual outer body. */
    owner->fields.t = NULL;
    CHECK(mc_client_render_global_clouds_begin(owner, math, .375f, &call, error) ==
              MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          error->site.pc == 4);
    CHECK(mc_client_render_global_clouds_retire(&call, error) == MC_CLIENT_RENDER_GLOBAL_OK);
    owner->fields.t = app;
    free(error);
    owner->fields.k = NULL;
    app->i = NULL;
    app->active_world = NULL;
    mc_entity_release(camera);
    for (size_t i = 0; i < 3; ++i) {
        mc_world_provider_unbind(dimensions[i]);
        mc_world_provider_release(dimensions[i]);
    }
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_free(world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    mc_client_render_texture_error *terminal = calloc(1, sizeof(*terminal));
    CHECK(terminal != NULL);
    mc_client_render_manager_classes *manager = attempt->manager_classes;
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_engine_retire_resources(runtime.graphics, terminal) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    free(terminal);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL);
    CHECK(statistics_end_app(&runtime, &startup, &app) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3 && clouds_device(argv[1], argv[2]) == 0);
    printf("Source clouds device: %u checks; 72 states and final source batches\n", checks);
    return 0;
}
