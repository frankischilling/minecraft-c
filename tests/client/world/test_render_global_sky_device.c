/* Actual whole-App/startup and constructor-compiled N lists. World/camera slots
 * below are prepared direct operands, not a World-load/attachment qualification. */
int mc_sky_startup_fixture_main(int argc, char **argv);
#define main mc_sky_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/display/fog_surface.h"
#include "client/render_engine/internal.h"
#include "client/world/render_global_internal.h"
#include "client/world/render_global_sky_internal.h"
#include "entity/player/player_internal.h"
#include "world/runtime/runtime_internal.h"
#include <inttypes.h>

static uint32_t sky_bits(float value) {
    uint32_t b;
    memcpy(&b, &value, sizeof(b));
    return b;
}

static int sky_device(const char *resources, const char *assets) {
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
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
              MC_CLIENT_STARTUP_UNIMPLEMENTED &&
          startup.error->caller.pc == 32 && app->g != NULL && app->i == NULL &&
          app->active_world == NULL);
    CHECK(statistics_retire_resources(attempt) == 0);
    mc_client_render_global *owner = app->g;
    mc_client_source_list_state list;
    CHECK(mc_client_display_list_inspect(app->display, owner->fields.z, &list) && list.defined &&
          list.draws == 169 && list.vertices == 1014);
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
    mc_entity *camera = NULL;
    CHECK(mc_local_player_allocate(runtime.services.entities, &camera, NULL) == MC_ENTITY_OK);
    camera->base.x = camera->base.z = 8.5;
    camera->base.y = 64.5;
    app->i = camera;
    app->active_world = world;
    owner->fields.k = world;
    mc_world_provider *dimensions[3] = {0};
    const mc_world_provider_registry_binding binding = {
        runtime.services.registry, MC_REGISTRY_CLIENT, runtime.services.profile};
    const int32_t ids[] = {0, -1, 1};
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_world_provider_create(ids[i], NULL, &dimensions[i], NULL) == MC_WORLD_PROVIDER_OK);
        CHECK(mc_world_provider_bind_game(dimensions[i], world, &binding, NULL) ==
              MC_WORLD_PROVIDER_OK);
    }
    /* Prime genuine cached ji.b paths before the observed sky calls. */
    mc_client_texture_lookup_error *lookup = calloc(1, sizeof(*lookup));
    CHECK(lookup != NULL);
    static const uint16_t sun[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n',
                                   '/', 's', 'u', 'n', '.', 'p', 'n', 'g'};
    static const uint16_t moon[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '/',
                                    'm', 'o', 'o', 'n', '.', 'p', 'n', 'g'};
    mc_client_texture id = 0;
    CHECK(mc_client_render_engine_texture(
              app->render_engine, app->display, NULL,
              (mc_texture_pack_text){sun, sizeof(sun) / sizeof(*sun), false}, &id,
              lookup) == MC_CLIENT_LOOKUP_OK);
    CHECK(mc_client_render_engine_texture(
              app->render_engine, app->display, NULL,
              (mc_texture_pack_text){moon, sizeof(moon) / sizeof(*moon), false}, &id,
              lookup) == MC_CLIENT_LOOKUP_OK);
    free(lookup);
    typedef GLboolean(APIENTRY * is_enabled_fn)(GLenum);
    typedef void(APIENTRY * get_boolean_fn)(GLenum, GLboolean *);
    typedef void(APIENTRY * get_buffer_fn)(GLenum, ptrdiff_t, ptrdiff_t, void *);
    is_enabled_fn is_enabled = NULL;
    get_boolean_fn get_boolean = NULL;
    get_buffer_fn get_buffer = NULL;
    SDL_FunctionPointer symbol = SDL_GL_GetProcAddress("glIsEnabled");
    memcpy(&is_enabled, &symbol, sizeof(symbol));
    symbol = SDL_GL_GetProcAddress("glGetBooleanv");
    memcpy(&get_boolean, &symbol, sizeof(symbol));
    symbol = SDL_GL_GetProcAddress("glGetBufferSubData");
    memcpy(&get_buffer, &symbol, sizeof(symbol));
    CHECK(is_enabled != NULL && get_boolean != NULL && get_buffer != NULL);
    mc_client_render_global_sky_call *call = NULL;
    mc_client_render_global_sky_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    mc_client_display_error display_error;
    const int64_t times[] = {0, 6000, 12000, 18000};
    for (unsigned int dim = 0; dim < 3; ++dim) {
        CHECK(mc_world_provider_retain(dimensions[dim]) == MC_WORLD_PROVIDER_OK);
        mc_world_provider *old = world->fields.t;
        world->fields.t = dimensions[dim];
        mc_world_provider_release(old);
        for (unsigned int t = 0; t < 4; ++t)
            for (unsigned int ana = 0; ana < 2; ++ana)
                for (unsigned int rain = 0; rain < 2; ++rain) {
                    CHECK(mc_world_info_time_set(world->fields.x, times[t]) == MC_WORLD_INFO_OK);
                    world->fields.i = world->fields.j = (float)rain;
                    world->fields.k = world->fields.l = 0;
                    mc_game_settings_state settings;
                    CHECK(mc_game_settings_read_state(app->settings, &settings));
                    settings.values.anaglyph = ana != 0;
                    CHECK(mc_game_settings_set_values(app->settings, &settings.values, NULL) ==
                          MC_GAME_SETTINGS_OK);
                    CHECK(mc_client_display_source_color(app->display, .125f, .25f, .5f, .75f,
                                                         &display_error));
                    CHECK(mc_client_display_source_texture_enable(app->display, true,
                                                                  &display_error));
                    CHECK(mc_client_display_source_alpha_test(app->display, true, &display_error));
                    CHECK(
                        mc_client_display_source_blend_enable(app->display, false, &display_error));
                    CHECK(mc_client_display_source_fog_enable(app->display, false, &display_error));
                    CHECK(mc_client_display_source_depth_mask(app->display, true, &display_error));
                    CHECK(mc_client_display_source_shade_model(app->display, 7425, &display_error));
                    CHECK(mc_client_display_source_fog_i(app->display, 2917, 9729, &display_error));
                    CHECK(mc_client_display_source_fog_f(app->display, 2915, 0, &display_error));
                    CHECK(mc_client_display_source_fog_f(app->display, 2916, 1000, &display_error));
                    CHECK(mc_entity_vector_pool_reset(runtime.services.entities, NULL) ==
                          MC_ENTITY_OK);
                    const mc_client_render_global_status status =
                        mc_client_render_global_sky_begin(owner, math, .375f, &call, error);
                    if (status != MC_CLIENT_RENDER_GLOBAL_OK)
                        fprintf(stderr, "Sky %u/%u/%u/%u %s.%s:%d %s\n", dim, t, ana, rain,
                                error->site.class_name, error->site.method, error->site.pc,
                                error->operation);
                    mc_client_render_global_sky_state state;
                    CHECK(status == MC_CLIENT_RENDER_GLOBAL_OK && call != NULL &&
                          mc_client_render_global_sky_inspect(call, &state) && state.returned);
                    CHECK(state.nether_skipped == (dim == 1) &&
                          (dim == 1 || (state.top_called && state.sun_drawn && state.moon_drawn &&
                                        state.bottom_called && state.depth_restored &&
                                        state.top_list == owner->fields.z &&
                                        state.bottom_list == owner->fields.A &&
                                        state.matrix_pushes == state.matrix_pops)));
                    mc_client_source_draw_state actual;
                    mc_client_source_fog_state fog;
                    CHECK(mc_client_display_source_draw_inspect(app->display, &actual) &&
                          actual.color_known &&
                          mc_client_display_source_fog_inspect(app->display, &fog));
                    if (dim != 1) {
                        /* Read the actual uploaded GPU record from the last bottom-list
                         * draw. This is independent of the passive state getter. */
                        float gpu[9] = {0};
                        get_buffer(GL_ARRAY_BUFFER, 0, sizeof(gpu), gpu);
                        CHECK(gpu[1] == -16.0f &&
                              memcmp(gpu + 5, actual.color, 4 * sizeof(float)) == 0);
                    }
                    GLboolean depth = GL_FALSE;
                    get_boolean(GL_DEPTH_WRITEMASK, &depth);
                    mc_entity_vector_snapshot vectors;
                    CHECK(mc_entity_vector_get_snapshot(runtime.services.entities, &vectors) ==
                          MC_ENTITY_OK);
                    printf("SKY_DRAW %u %u %u %u", dim, t, ana, rain);
                    for (size_t c = 0; c < 4; ++c)
                        printf(" %08" PRIx32, sky_bits(actual.color[c]));
                    printf(" %u %u %u %u %u %u %d\n", fog.enabled ? 1u : 0u,
                           actual.alpha_test ? 1u : 0u, is_enabled(GL_BLEND) ? 1u : 0u,
                           actual.texture_enabled ? 1u : 0u, depth ? 1u : 0u,
                           actual.shade_flat ? 7424u : 7425u, vectors.cursor);
                    uint32_t pending = UINT32_MAX;
                    CHECK(mc_client_display_source_get_error(app->display, &pending,
                                                             &display_error) &&
                          pending == 0);
                    CHECK(mc_client_render_global_sky_retire(&call, error) ==
                              MC_CLIENT_RENDER_GLOBAL_OK &&
                          call == NULL);
                }
    }
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
    CHECK(argc == 3 && sky_device(argv[1], argv[2]) == 0);
    printf("Source sky device: %u checks; 48 original states and actual GPU bottom-list records\n",
           checks);
    return 0;
}
