/* Genuine World/Player constructors and authored chunk storage. */
int mc_precipitation_picker_main(int argc, char **argv);
#define MC_PICKING_BODY_MAIN_NAME mc_precipitation_picker_main
#include "test_picking.c"
#undef main
#undef MC_PICKING_BODY_MAIN_NAME

#include "client/display/fog_surface.h"
#include "client/display/source_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render_engine/nw.h"
#include "client/render_engine/texture_lookup_internal.h"
#include "client/world/entity_renderer/precipitation_internal.h"
#include "world/biome/registry_internal.h"

#include <xmmintrin.h>

static bool alpha_pixels(picking_fixture *f, mc_client_display *display) {
    mc_client_graphics_error graphics;
    mc_client_tessellator *nw = NULL;
    CHECK(mc_client_nw_active_use(f->graphics, (mc_client_graphics_site){"px", "c", "(F)V", 63},
                                  &nw, &graphics));
    CHECK(mc_client_display_source_texture_enable(display, false, NULL));
    CHECK(mc_client_display_source_alpha_test(display, true, NULL));
    CHECK(mc_client_display_source_cull_test(display, false, NULL));
    CHECK(mc_client_display_source_blend_enable(display, false, NULL));
    CHECK(mc_client_display_source_fog_enable(display, false, NULL));
    CHECK(mc_client_display_source_color(display, 1, 1, 1, 0.05f, NULL));
    for (unsigned int pass = 0; pass < 2; ++pass) {
        CHECK(
            mc_client_display_source_alpha_function(display, 516, pass == 0 ? 0.01f : 0.1f, NULL));
        CHECK(mc_client_display_source_clear_color(display, 0, 0, 0, 0, NULL));
        CHECK(mc_client_display_source_clear(display, 16384, NULL));
        CHECK(mc_client_nw_begin_quads(nw, &graphics));
        CHECK(mc_client_nw_vertex(nw, -1, -1, 0, &graphics));
        CHECK(mc_client_nw_vertex(nw, 1, -1, 0, &graphics));
        CHECK(mc_client_nw_vertex(nw, 1, 1, 0, &graphics));
        CHECK(mc_client_nw_vertex(nw, -1, 1, 0, &graphics));
        CHECK(mc_client_nw_draw(nw, display, &graphics));
        mc_client_capture capture = {0};
        CHECK(mc_client_display_capture_rgba(display, &capture, NULL));
        const size_t center =
            capture.stride * (size_t)(capture.height / 2) + 4u * (size_t)(capture.width / 2);
        CHECK(capture.pixels[center] == (pass == 0 ? 255 : 0) &&
              capture.pixels[center + 1] == capture.pixels[center] &&
              capture.pixels[center + 2] == capture.pixels[center]);
        mc_client_capture_free(&capture);
    }
    return true;
}

static bool precipitation_controls(picking_fixture *f, const char *directory) {
    CHECK(picking_case_open(f, directory));
    mc_client_precipitation_call *call = NULL;
    mc_client_precipitation_state state;
    mc_client_precipitation_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    mc_world_runtime *world = f->base.world;
    const size_t holds = f->px->holds;
    const size_t bindings = world->persistent_bindings;
    mc_entity *camera = f->app->i;
    mc_java_random random;
    mc_java_random_seed(&random, -7);
    f->px->fields.J = &random;
    static const uint32_t dry[] = {0, 0x80000000u, 0xbf800000u, 0x80000001u};
    for (size_t i = 0; i < sizeof dry / sizeof(*dry); ++i) {
        float value;
        memcpy(&value, &dry[i], sizeof(value));
        world->fields.i = world->fields.j = value;
        const uint64_t seed = random.state;
        f->app->i = NULL;
        CHECK(mc_client_entity_renderer_precipitation_begin(f->px, NULL, 0.0f, &call, error) ==
              MC_CLIENT_PRECIPITATION_OK);
        CHECK(mc_client_entity_renderer_precipitation_inspect(call, &state) && state.returned &&
              state.source_pc == 18 && state.strength_read && !state.camera_captured &&
              !state.world_captured && !state.snow_texture_bound && random.state == seed);
        CHECK(f->px->holds == holds + 1 && world->persistent_bindings == bindings);
        CHECK(mc_client_entity_renderer_precipitation_retire(&call, error) ==
                  MC_CLIENT_PRECIPITATION_OK &&
              call == NULL && f->px->holds == holds);
    }
    world->fields.i = world->fields.j = 1.0f;
    CHECK(mc_client_entity_renderer_precipitation_begin(f->px, NULL, 0.5f, &call, error) ==
              MC_CLIENT_PRECIPITATION_SOURCE_NULL &&
          error->source_pc == 37);
    CHECK(mc_client_entity_renderer_precipitation_inspect(call, &state) && state.camera_captured &&
          state.world_captured && !state.returned && world->persistent_bindings == bindings + 1);
    CHECK(mc_client_entity_renderer_precipitation_retire(&call, error) ==
          MC_CLIENT_PRECIPITATION_OK);
    f->app->i = camera;

    static const uint32_t values[] = {0,           0x80000000u, 1,           0x3c23d70au,
                                      0x3dcccccdu, 0xbdcccccdu, 0x7fc12345u, 0x7f800000u,
                                      0xff800000u, 0x3f800000u};
    for (size_t partial = 0; partial < 10; ++partial)
        for (size_t strength = 0; strength < 10; ++strength) {
            float fraction;
            memcpy(&fraction, &values[partial], sizeof(fraction));
            world->fields.i = 0;
            memcpy(&world->fields.j, &values[strength], sizeof(float));
            f->app->i = NULL;
            mc_java_random_seed(&random, -7);
            const mc_client_precipitation_status status =
                mc_client_entity_renderer_precipitation_begin(f->px, NULL, fraction, &call, error);
            CHECK(status == MC_CLIENT_PRECIPITATION_OK ||
                  (status == MC_CLIENT_PRECIPITATION_SOURCE_NULL && error->source_pc == 37));
            CHECK(mc_client_entity_renderer_precipitation_inspect(call, &state));
            CHECK(state.returned == (status == MC_CLIENT_PRECIPITATION_OK));
            printf("PRECIP_PREFIX %zu %zu %s %016" PRIx64 "\n", partial, strength,
                   state.returned ? "OK" : "NULL", (uint64_t)mc_java_random_next_i64(&random));
            CHECK(mc_client_entity_renderer_precipitation_retire(&call, error) ==
                  MC_CLIENT_PRECIPITATION_OK);
        }
    f->app->i = camera;
    world->fields.i = world->fields.j = 1.0f;

    CHECK(mc_client_application_lifetime_register(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = settings;
    mc_client_texture_pack_list *packs = NULL;
    CHECK(mc_client_texture_pack_list_allocate(mc_resource_files_source(f->base.files), NULL,
                                               &packs) == MC_TEXTURE_PACK_OK);
    CHECK(mc_client_texture_pack_list_construct(packs, f->app, directory, NULL) ==
          MC_TEXTURE_PACK_OK);
    mc_client_default_font *font = NULL;
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &font) == MC_CLIENT_FONT_OK);
    mc_client_render_engine *engine = NULL;
    CHECK(mc_client_render_engine_allocate(f->graphics, font, NULL, &engine) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(engine, packs, settings, NULL) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    f->app->render_engine = engine;
    mc_client_display *d = NULL;
    CHECK(mc_client_display_create("Precipitation body controls", 64, 64, true, &d, NULL));
    f->px->services.display = d;
    CHECK(mc_entity_set_position(camera, 16.5, 64.5, 16.5) == MC_ENTITY_OK);
    camera->base.bl = 16.0;
    camera->base.bm = 63.0;
    camera->base.bn = 16.0;
    f->px->fields.l = -31;
    mc_client_texture id;
    mc_client_texture_lookup_error *lookup = calloc(1, sizeof(*lookup));
    CHECK(lookup != NULL);
    CHECK(mc_client_render_engine_texture(engine, d, NULL, (mc_texture_pack_text){NULL, 0, true},
                                          &id, lookup) == MC_CLIENT_LOOKUP_NULL_REFERENCE &&
          mc_client_texture_lookup_pending(engine));
    CHECK(mc_client_entity_renderer_precipitation_begin(f->px, f->math, 0.5f, &call, error) ==
              MC_CLIENT_PRECIPITATION_CHILD_FAILED &&
          error->source_pc == 115);
    CHECK(mc_client_entity_renderer_precipitation_retire(&call, error) ==
              MC_CLIENT_PRECIPITATION_OK &&
          call == NULL && mc_client_texture_lookup_pending(engine));
    CHECK(mc_client_render_engine_retire_lookup(engine, lookup) == MC_CLIENT_LOOKUP_OK);
    CHECK(!mc_client_texture_lookup_pending(engine));
    free(lookup);
    /* Prepare actual mutable Biome fields, preserving each original value.
     * No sampler, query, texture, tessellator or Display result is supplied. */
    const mc_biome *biomes[13];
    bool snow[13], rain[13];
    for (size_t i = 0; i < 13; ++i) {
        CHECK(mc_biome_static_get(f->base.registry, (char)('a' + (int)i), &biomes[i], NULL) ==
                  MC_REGISTRY_OK &&
              biomes[i] != NULL);
        snow[i] = biomes[i]->fields.v;
        rain[i] = biomes[i]->fields.w;
    }
    for (unsigned int mode = 0; mode < 3; ++mode) {
        for (size_t i = 0; i < 13; ++i) {
            mc_biome *biome = (mc_biome *)biomes[i];
            biome->fields.v = mode == 0;
            biome->fields.w = mode == 1;
        }
        for (unsigned int fancy = 0; fancy < 2; ++fancy) {
            mc_game_settings_state options;
            CHECK(mc_game_settings_read_state(settings, &options));
            options.values.fancy_graphics = fancy != 0;
            CHECK(mc_game_settings_set_values(settings, &options.values, NULL) ==
                  MC_GAME_SETTINGS_OK);
            mc_java_random_seed(&random, -7);
            const uint64_t initial = random.state;
            const mc_client_precipitation_status status =
                mc_client_entity_renderer_precipitation_begin(f->px, f->math, 0.5f, &call, error);
            if (status != MC_CLIENT_PRECIPITATION_OK)
                fprintf(stderr,
                        "Precipitation mode%u fancy%u PC%u: %s; graphics=%s; world=%s; query=%s\n",
                        mode, fancy, error->source_pc, error->operation, error->graphics.operation,
                        error->world.operation, error->query.operation);
            CHECK(status == MC_CLIENT_PRECIPITATION_OK);
            CHECK(mc_client_entity_renderer_precipitation_inspect(call, &state));
            const size_t cells = fancy ? 441u : 121u;
            CHECK(state.returned && state.source_pc == 1735 && state.state_restored &&
                  state.snow_texture_bound && state.rain_texture_bound && state.biomes_returned &&
                  state.snow_cells == cells && state.rain_cells == cells &&
                  state.radius == (fancy ? 10 : 5) && state.render_x == 16.25 &&
                  state.render_y == 63.75 && state.render_z == 16.25);
            CHECK(state.snow_draws == (mode == 0 ? cells : 0) &&
                  state.rain_draws == (mode == 1 ? cells : 0));
            CHECK((random.state == initial) == (mode == 2));
            mc_client_source_fog_state fog;
            CHECK(mc_client_display_source_fog_inspect(d, &fog));
            mc_client_source_draw_state draw;
            CHECK(mc_client_display_source_draw_inspect(d, &draw) && draw.alpha_function_set &&
                  draw.alpha_reference == 0.1f);
            CHECK(fog.normal[0] == 0.0f && fog.normal[1] == 1.0f && fog.normal[2] == 0.0f);
            mc_client_nw_state nw;
            CHECK(mc_client_nw_inspect(f->graphics, &nw));
            CHECK(!nw.fields.w && nw.fields.s == 0 && nw.fields.t == 0 && nw.fields.u == 0);
            mc_client_precipitation_call *same = call;
            CHECK(mc_client_entity_renderer_precipitation_begin(f->px, f->math, 0, &call, error) ==
                      MC_CLIENT_PRECIPITATION_ARGUMENT &&
                  call == same);
            CHECK(mc_client_entity_renderer_precipitation_retire(&call, error) ==
                      MC_CLIENT_PRECIPITATION_OK &&
                  call == NULL && f->px->holds == holds && world->persistent_bindings == bindings);
            CHECK(mc_client_display_check_gl(d, "precipitation draw", NULL));
            printf("PRECIPITATION %u %u %zu %zu %016" PRIx64 "\n", mode, fancy, state.snow_draws,
                   state.rain_draws, random.state);
            if (mode < 2) {
                f->px->fields.J = NULL;
                CHECK(mc_client_entity_renderer_precipitation_begin(f->px, f->math, 0.5f, &call,
                                                                    error) ==
                          MC_CLIENT_PRECIPITATION_SOURCE_NULL &&
                      error->source_pc == (mode == 0 ? 411u : 1184u));
                CHECK(mc_client_entity_renderer_precipitation_inspect(call, &state) &&
                      state.biomes_returned && state.snow_draws == 0 && state.rain_draws == 0);
                CHECK(mc_client_entity_renderer_precipitation_retire(&call, error) ==
                          MC_CLIENT_PRECIPITATION_OK &&
                      world->persistent_bindings == bindings && f->px->holds == holds);
                f->px->fields.J = &random;
            }
        }
    }
    for (size_t i = 0; i < 13; ++i) {
        ((mc_biome *)biomes[i])->fields.v = snow[i];
        ((mc_biome *)biomes[i])->fields.w = rain[i];
    }
    CHECK(alpha_pixels(f, d));
    f->px->active = true;
    CHECK(mc_client_entity_renderer_precipitation_begin(f->px, f->math, 0, &call, error) ==
              MC_CLIENT_PRECIPITATION_BUSY &&
          call == NULL);
    world->fields.i = world->fields.j = 0;
    CHECK(entity_renderer_precipitation_entered(f->px, f->math, 0.5f, &call, error) ==
              MC_CLIENT_PRECIPITATION_OK &&
          f->px->active && f->px->holds == holds);
    CHECK(mc_client_entity_renderer_precipitation_retire(&call, error) ==
              MC_CLIENT_PRECIPITATION_OK &&
          f->px->active);
    f->px->active = false;
    f->app->render_engine = NULL;
    CHECK(mc_client_texture_lookup_free_metadata(engine));
    CHECK(mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_retire_resources(f->graphics, NULL) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    CHECK(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_texture_pack_list_release(packs, NULL) == MC_TEXTURE_PACK_OK);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = NULL;
    CHECK(mc_client_application_lifetime_retire(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    f->px->services.display = NULL;
    CHECK(mc_client_display_destroy(d, NULL));
    f->px->fields.J = NULL;
    free(error);
    CHECK(picking_case_close(f));
    CHECK(mc_client_entity_renderer_retire(&f->px, &f->owner_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    return true;
}

int main(int argc, char **argv) {
    picking_fixture *f = calloc(1, sizeof(*f));
    if (argc != 3 || f == NULL || !picking_open(f, argv[1]) ||
        !precipitation_controls(f, argv[2]) || !picking_close(f))
        return 1;
    free(f);
    printf("Precipitation: %u checks passed\n", checks);
    return 0;
}
