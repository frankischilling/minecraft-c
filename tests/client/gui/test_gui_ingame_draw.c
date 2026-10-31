/* Genuine App/startup/World/Player/controller owners; finite prepared HUD inputs. */
int mc_hud_startup_fixture_main(int argc, char **argv);
#define main mc_hud_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/gui/gui_ingame_internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/texture_lookup.h"
#include "entity/player/player_internal.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/gui/gui_ingame_draw.h"
#include "mc/client/world/overlay_setup.h"
#include "mc/core/math/math.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/runtime_internal.h"
#include <SDL3/SDL.h>
#include <inttypes.h>

typedef struct hud_world {
    mc_world_runtime_classes *classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_chunk_state *chunks[4];
    mc_chunk_provider_fields provider;
    mc_entity *player;
} hud_world;

static int hud_world_open(hud_world *w, fixture *runtime, client_application *app,
                          mc_math_classes *math, const char *directory) {
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, runtime->services.profile,
                                          mc_math_classes_table(math), &w->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(w->classes, runtime->services.registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(w->classes, runtime->services.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &w->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t ids[256];
    for (size_t i = 0; i < sizeof(ids); ++i)
        ids[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 6381,
                                            .block_map = ids,
                                            .block_map_length = sizeof(ids),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, w->regions, &options, &w->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t title[] = {'H', 'U', 'D'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, 3}, SIZE_MAX, &w->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &w->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, w->classes,
                                          &limits, &w->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = w->store, .name = w->name, .seed = 23, .dimension = w->dimension};
    CHECK(mc_world_runtime_construct_world(w->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_provider_bind_fields(w->world->fields.v, &w->provider, NULL) ==
          MC_WORLD_STORAGE_OK);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            const size_t index = (size_t)(x * 2 + z);
            CHECK(mc_world_runtime_get(w->world, x, z, &w->chunks[index]) == MC_CHUNK_ACQUIRE_OK &&
                  w->chunks[index] != NULL && w->chunks[index]->terrain_populated);
        }
    CHECK(mc_local_player_allocate(runtime->services.entities, &w->player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(w->player, w->world, app, app->session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_entity_set_position(w->player, 8.5, 200.5, 8.5) == MC_ENTITY_OK);
    app->i = app->h = w->player;
    app->active_world = w->world;
    return 0;
}

static int hud_world_close(hud_world *w, client_application *app) {
    app->i = app->h = NULL;
    app->active_world = NULL;
    mc_entity_release(w->player);
    CHECK(mc_world_runtime_retire_entity_containers(w->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_pending_clear(w->provider.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(w->provider.cache, w->provider.cache->entries,
                              w->provider.cache->capacity, w->provider.cache->insertions,
                              w->provider.cache->insertion_capacity));
    mc_world_provider_unbind(w->dimension);
    CHECK(mc_world_runtime_retire_world(w->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 4; ++i)
        CHECK(mc_world_store_retire_chunk(w->store, w->chunks[i], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(w->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(w->dimension);
    CHECK(mc_world_store_free_checked(&w->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(w->regions);
    mc_world_info_name_release(w->name);
    CHECK(mc_world_runtime_classes_free(w->classes, NULL) == MC_WORLD_STORAGE_OK);
    return 0;
}

static uint32_t hud_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

typedef GLboolean(APIENTRY *hud_enabled_fn)(GLenum);
typedef void(APIENTRY *hud_boolean_fn)(GLenum, GLboolean *);
typedef void(APIENTRY *hud_integer_fn)(GLenum, GLint *);

static int hud_reset(fixture *runtime, client_application *app) {
    CHECK(mc_client_display_source_viewport(app->display, 320, 240, NULL));
    CHECK(mc_client_display_source_matrix_mode(app->display, GL_PROJECTION, NULL));
    CHECK(mc_client_display_source_load_identity(app->display, NULL));
    CHECK(mc_client_display_source_scale(app->display, 1.25, .75, 1.5, NULL));
    CHECK(mc_client_display_source_matrix_mode(app->display, GL_MODELVIEW, NULL));
    CHECK(mc_client_display_source_load_identity(app->display, NULL));
    CHECK(mc_client_display_source_translate(app->display, 5, 6, 7, NULL));
    CHECK(mc_client_display_source_color(app->display, .125f, .25f, .5f, .75f, NULL));
    CHECK(mc_client_display_source_texture_enable(app->display, true, NULL));
    CHECK(mc_client_display_source_depth_test(app->display, true, NULL));
    CHECK(mc_client_display_source_depth_mask(app->display, true, NULL));
    CHECK(mc_client_display_source_alpha_test(app->display, true, NULL));
    CHECK(mc_client_display_source_alpha_function(app->display, GL_GREATER, .1f, NULL));
    CHECK(mc_client_display_source_blend_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_cull_test(app->display, false, NULL));
    CHECK(mc_client_display_source_fog_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_lighting_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_light_enable(app->display, 0, false, NULL));
    CHECK(mc_client_display_source_light_enable(app->display, 1, false, NULL));
    CHECK(mc_client_display_source_color_material_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_rescale_normal(app->display, false, NULL));
    CHECK(mc_client_display_source_color_mask(app->display, true, true, true, true, NULL));
    CHECK(mc_client_display_source_shade_model(app->display, GL_SMOOTH, NULL));
    CHECK(mc_client_display_source_clear_color(app->display, 0, 0, 0, 1, NULL));
    CHECK(mc_client_display_source_clear_depth(app->display, 1, NULL));
    CHECK(mc_client_display_source_clear(app->display, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                                         NULL));
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(runtime->graphics, &nw) && nw.fields.g != NULL &&
          nw.fields.e != NULL && nw.fields.B > 0 && !nw.fields.w);
    /* Explicit original observer inputs. Zero once before the complete HUD,
     * never between its rectangles and never in runtime code. */
    memset((void *)nw.fields.g, 0, (size_t)nw.fields.B * sizeof(*nw.fields.g));
    ((mc_client_direct_int_buffer *)(void *)nw.fields.e)->position = 0;
    CHECK(mc_entity_vector_pool_reset(runtime->services.entities, NULL) == MC_ENTITY_OK);
    return 0;
}

static int hud_prepare(client_application *app, mc_entity *player, unsigned int size,
                       unsigned int health, bool fancy) {
    mc_game_settings_state options;
    CHECK(mc_game_settings_read_state(app->settings, &options));
    options.values.fancy_graphics = fancy;
    options.values.third_person_view = false;
    options.values.show_debug_info = false;
    options.values.gui_scale = size == 0 ? 0 : 2;
    CHECK(mc_game_settings_set_values(app->settings, &options.values, NULL) == MC_GAME_SETTINGS_OK);
    app->d = size == 0 ? 320 : 641;
    app->e = size == 0 ? 240 : 481;
    const int32_t current[] = {20, 3, 11}, previous[] = {20, 7, 13}, slots[] = {0, 8, 4};
    player->local_player->fields.living.Y = current[health];
    player->local_player->fields.living.Z = previous[health];
    player->base.by = health == 0 ? 0 : 15;
    player->local_player->fields.player.u = false;
    player->local_player->fields.player.B = player->local_player->fields.player.C = 0;
    CHECK(mc_inventory_player_select_hotbar(player->local_player->fields.player.c, slots[health],
                                            NULL) == MC_INVENTORY_OK);
    app->v->fields.h = size == 0 ? 17 : INT32_C(-2147483647);
    app->v->fields.j = 0;
    app->v->fields.c = fancy ? .75f : 1.0f;
    /* Same finite UQ operands as the original after its World.k preparation. */
    CHECK(app->active_world->live.time != NULL);
    *app->active_world->live.time = fancy ? 18000 : 6000;
    app->active_world->fields.f = fancy ? 11 : 0;
    CHECK(app->v->fields.e != NULL && app->v->fields.e->size == 0);
    return 0;
}

static int hud_pending_guards(mc_client_source_uq *owner, mc_client_render_engine_classes *graphics,
                              mc_math_classes *math, mc_client_display *display) {
    mc_client_gui_ingame_error error;
    const size_t holds = owner->holds;
    const int32_t tick = owner->fields.h, notice = owner->fields.j;
    const int32_t modifications = owner->fields.e->mod_count;
    const uint64_t random = owner->fields.f->state;
    const mc_client_gui_ingame_phase phase = owner->phase;
    CHECK(mc_client_gui_ingame_retain(owner, &error) == MC_CLIENT_GUI_INGAME_BUSY &&
          owner->holds == holds);
    mc_client_source_uq *occurrence = owner;
    CHECK(mc_client_gui_ingame_release(&occurrence, &error) == MC_CLIENT_GUI_INGAME_BUSY &&
          occurrence == owner && owner->holds == holds);
    CHECK(mc_client_gui_ingame_retire(&occurrence, &error) == MC_CLIENT_GUI_INGAME_BUSY &&
          occurrence == owner);
    CHECK(mc_client_gui_ingame_tick(owner, &error) == MC_CLIENT_GUI_INGAME_BUSY);
    CHECK(mc_client_gui_ingame_clear_messages(owner, &error) == MC_CLIENT_GUI_INGAME_BUSY);
    mc_client_gui_ingame_draw_call *other = NULL;
    mc_client_gui_ingame_draw_error draw_error;
    CHECK(mc_client_gui_ingame_draw_begin(owner, graphics, math, display, .375f, false, 17, 23,
                                          &other, &draw_error) == MC_CLIENT_GUI_INGAME_DRAW_BUSY &&
          other == NULL);
    CHECK(owner->holds == holds && owner->phase == phase && owner->fields.h == tick &&
          owner->fields.j == notice && owner->fields.e->mod_count == modifications &&
          owner->fields.f->state == random);
    return 0;
}

static int hud_row(fixture *runtime, client_application *app, unsigned int index, unsigned int size,
                   unsigned int health, unsigned int partial, hud_enabled_fn enabled,
                   hud_boolean_fn boolean, hud_integer_fn integer) {
    float color[4], projection[16], modelview[16];
    /* Identical original query order. CURRENT_COLOR commits staged Mesa attributes. */
    CHECK(
        mc_client_display_source_current_attribute(app->display, GL_CURRENT_COLOR, color, 4, NULL));
    CHECK(mc_client_display_camera_matrices_inspect(app->display, projection, modelview));
    mc_entity_vector_snapshot vectors;
    CHECK(mc_entity_vector_get_snapshot(runtime->services.entities, &vectors) == MC_ENTITY_OK);
    int32_t mode;
    size_t model_depth, projection_depth;
    CHECK(mc_client_display_camera_stack_inspect(app->display, &mode, &model_depth,
                                                 &projection_depth));
    mc_client_source_draw_state draw;
    mc_client_source_fog_state fog;
    CHECK(mc_client_display_source_draw_inspect(app->display, &draw));
    CHECK(mc_client_display_source_fog_inspect(app->display, &fog));
    GLboolean depth_mask;
    GLint blend_source, blend_destination;
    boolean(GL_DEPTH_WRITEMASK, &depth_mask);
    integer(GL_BLEND_SRC_RGB, &blend_source);
    integer(GL_BLEND_DST_RGB, &blend_destination);
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(runtime->graphics, &nw) && nw.fields.e->position == 48);
    printf("HUD_STATE %u %u %u %u %08" PRIx32 " %08" PRIx32 " %" PRIu64 " %d %zu %zu %d", index,
           size, health, partial, hud_bits(app->v->fields.k), hud_bits(app->v->fields.c),
           app->v->fields.f->state, vectors.cursor, model_depth, projection_depth, mode);
    for (size_t i = 0; i < 16; ++i)
        printf(" %08" PRIx32, hud_bits(modelview[i]));
    for (size_t i = 0; i < 16; ++i)
        printf(" %08" PRIx32, hud_bits(projection[i]));
    for (size_t i = 0; i < 4; ++i)
        printf(" %08" PRIx32, hud_bits(color[i]));
    printf(" %u %u %u %u %u %u %u %u %u %u %u %u %d %d %zu\n", draw.texture_enabled ? 1u : 0u,
           enabled(GL_DEPTH_TEST) ? 1u : 0u, draw.alpha_test ? 1u : 0u, enabled(GL_BLEND) ? 1u : 0u,
           enabled(GL_CULL_FACE) ? 1u : 0u, fog.enabled ? 1u : 0u, fog.lighting_enabled ? 1u : 0u,
           fog.lights[0] ? 1u : 0u, fog.lights[1] ? 1u : 0u, fog.color_material ? 1u : 0u,
           fog.rescale_normal_enabled ? 1u : 0u, depth_mask ? 1u : 0u, blend_source,
           blend_destination, nw.fields.e->position);
    printf("HUD_WORDS %u 5 6 ", index);
    for (size_t vertex = 0; vertex < 6; ++vertex)
        for (size_t component = 0; component < 5; ++component)
            printf("%08" PRIx32, (uint32_t)nw.fields.g[vertex * 8u + component]);
    putchar('\n');
    return 0;
}

static int hud_draw_profile(fixture *runtime, client_application *app, mc_math_classes *math,
                            unsigned int index, unsigned int size, unsigned int health,
                            unsigned int p, hud_enabled_fn enabled, hud_boolean_fn boolean,
                            hud_integer_fn integer) {
    const float partials[] = {.375f, 1.0f};
    CHECK(hud_reset(runtime, app) == 0);
    mc_client_gui_ingame_draw_call *call = NULL;
    mc_client_gui_ingame_draw_error error;
    const mc_client_gui_ingame_draw_status status = mc_client_gui_ingame_draw_begin(
        app->v, runtime->graphics, math, app->display, partials[p], false, 17, 23, &call, &error);
    if (status != MC_CLIENT_GUI_INGAME_DRAW_OK)
        fprintf(stderr, "Actual HUD %s.%s:%d %s\n", error.site.class_name, error.site.method,
                error.site.pc, error.operation);
    mc_client_gui_ingame_draw_state state;
    CHECK(status == MC_CLIENT_GUI_INGAME_DRAW_OK &&
          mc_client_gui_ingame_draw_inspect(call, &state) && state.returned &&
          state.projection_returned && state.hotbar_returned && state.crosshair_returned &&
          state.random_seeded && state.stats_returned && state.slots_returned &&
          state.chat_returned && state.empty_slots == 9 && state.heart_backgrounds == 10 &&
          state.heart_foregrounds >= 2);
    CHECK(hud_pending_guards(app->v, runtime->graphics, math, app->display) == 0);
    printf("HUD_INPUTS %u %u %08" PRIx32 " %d\n", index, state.fancy ? 1u : 0u,
           hud_bits(state.brightness), app->active_world->fields.f);
    CHECK(hud_row(runtime, app, index, size, health, p, enabled, boolean, integer) == 0);
    CHECK(mc_client_gui_ingame_draw_retire(&call, &error) == MC_CLIENT_GUI_INGAME_DRAW_OK &&
          call == NULL);
    return 0;
}

static int hud_profiles(fixture *runtime, client_application *app, hud_world *world,
                        mc_math_classes *math) {
    mc_entity *player = world->player;
    hud_enabled_fn enabled;
    hud_boolean_fn boolean;
    hud_integer_fn integer;
    SDL_FunctionPointer symbol = SDL_GL_GetProcAddress("glIsEnabled");
    memcpy(&enabled, &symbol, sizeof(symbol));
    symbol = SDL_GL_GetProcAddress("glGetBooleanv");
    memcpy(&boolean, &symbol, sizeof(symbol));
    symbol = SDL_GL_GetProcAddress("glGetIntegerv");
    memcpy(&integer, &symbol, sizeof(symbol));
    CHECK(enabled != NULL && boolean != NULL && integer != NULL);
    unsigned int index = 0;
    for (unsigned int fancy = 0; fancy < 2; ++fancy)
        for (unsigned int size = 0; size < 2; ++size)
            for (unsigned int health = 0; health < 3; ++health)
                for (unsigned int p = 0; p < 2; ++p) {
                    CHECK(hud_prepare(app, player, size, health, fancy != 0) == 0);
                    CHECK(hud_draw_profile(runtime, app, math, index++, size, health, p, enabled,
                                           boolean, integer) == 0);
                }
    CHECK(mc_entity_set_position(player, 8.5, 120.5, 8.5) == MC_ENTITY_OK);
    CHECK(mc_chunk_nibbles_set(world->chunks[0]->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 120, 8,
                               15));
    for (unsigned int size = 0; size < 2; ++size) {
        CHECK(hud_prepare(app, player, size, 0, true) == 0);
        CHECK(hud_draw_profile(runtime, app, math, index, size, 0, 0, enabled, boolean, integer) ==
              0);
        const mc_entity_bounds *bounds = player->bounds;
        CHECK(bounds != NULL);
        bool loaded = false;
        mc_world_movement_error area_error;
        CHECK(mc_world_runtime_area_loaded(
                  world->world, mc_math_floor_double(bounds->min_x),
                  mc_math_floor_double(bounds->min_y), mc_math_floor_double(bounds->min_z),
                  mc_math_floor_double(bounds->max_x), mc_math_floor_double(bounds->max_y),
                  mc_math_floor_double(bounds->max_z), &loaded,
                  &area_error) == MC_WORLD_STORAGE_OK &&
              loaded);
        float brightness;
        mc_world_brightness_error brightness_error;
        CHECK(mc_world_runtime_brightness(world->world, 8, 120, 8, &brightness,
                                          &brightness_error) == MC_WORLD_STORAGE_OK);
        printf("HUD_LIGHT %u 8 120 8 %u %08" PRIx32 "\n", index++, loaded ? 1u : 0u,
               hud_bits(brightness));
    }
    CHECK(index == 26);
    return 0;
}

static int hud_pixels(fixture *runtime, client_application *app, mc_entity *player,
                      mc_math_classes *math, const char *screenshot) {
    CHECK(mc_entity_set_position(player, 8.5, 200.5, 8.5) == MC_ENTITY_OK);
    CHECK(hud_prepare(app, player, 0, 0, true) == 0 && hud_reset(runtime, app) == 0);
    mc_client_capture before = {0}, after = {0};
    CHECK(mc_client_display_capture_rgba(app->display, &before, NULL));
    mc_client_gui_ingame_draw_call *call = NULL;
    mc_client_gui_ingame_draw_error error;
    CHECK(mc_client_gui_ingame_draw_begin(app->v, runtime->graphics, math, app->display, .375f,
                                          false, 17, 23, &call,
                                          &error) == MC_CLIENT_GUI_INGAME_DRAW_OK);
    mc_client_gui_ingame_draw_state state;
    CHECK(mc_client_gui_ingame_draw_inspect(call, &state) && state.returned && state.fancy &&
          state.vignette_returned && state.hotbar_returned && state.crosshair_returned &&
          state.stats_returned && state.heart_backgrounds == 10 && state.heart_foregrounds == 10 &&
          state.empty_slots == 9);
    CHECK(mc_client_display_capture_rgba(app->display, &after, NULL) && after.width == 320 &&
          after.height == 240 && before.width == after.width && before.height == after.height &&
          before.stride == after.stride);
    bool background_black = true;
    size_t green = 0, red = 0, white = 0;
    for (int y = 0; y < after.height; ++y)
        for (int x = 0; x < after.width; ++x) {
            const size_t offset = (size_t)y * after.stride + (size_t)x * 4u;
            const uint8_t *pixel = after.pixels + offset;
            background_black = background_black && before.pixels[offset] == 0 &&
                               before.pixels[offset + 1] == 0 && before.pixels[offset + 2] == 0;
            if (pixel[1] > 120 && pixel[0] < 100 && pixel[2] < 100)
                ++green;
            if (pixel[0] > 180 && pixel[1] < 100 && pixel[2] < 100)
                ++red;
            if (pixel[0] > 200 && pixel[1] > 200 && pixel[2] > 200)
                ++white;
        }
    printf("HUD_NATIVE_PIXELS %zu %zu %zu %d %d\n", green, red, white, after.width, after.height);
    CHECK(background_black && green >= 1000 && red >= 200 && white >= 8);
    SDL_Surface *surface = SDL_CreateSurfaceFrom(after.width, after.height, SDL_PIXELFORMAT_RGBA32,
                                                 after.pixels, (int)after.stride);
    CHECK(surface != NULL && SDL_SavePNG(surface, screenshot));
    SDL_DestroySurface(surface);
    mc_client_capture_free(&before);
    mc_client_capture_free(&after);
    CHECK(mc_client_gui_ingame_draw_retire(&call, &error) == MC_CLIENT_GUI_INGAME_DRAW_OK);
    return 0;
}

static int hud_failures(fixture *runtime, client_application *app, mc_entity *player,
                        mc_math_classes *math) {
    CHECK(hud_prepare(app, player, 0, 0, false) == 0 && hud_reset(runtime, app) == 0);
    float before_projection[16], before_modelview[16], after_projection[16], after_modelview[16];
    CHECK(mc_client_display_camera_matrices_inspect(app->display, before_projection,
                                                    before_modelview));
    mc_game_settings *settings = app->settings;
    app->settings = NULL;
    mc_client_overlay_setup_state projection;
    mc_client_overlay_setup_error projection_error;
    CHECK(!entity_renderer_overlay_setup(app->t, &projection, &projection_error) &&
          projection_error.status == MC_CLIENT_OVERLAY_SETUP_SOURCE_NULL &&
          projection_error.site.pc == 25 && projection_error.child.pc == 20 &&
          !projection.resolution_created && !projection.depth_cleared &&
          !projection.projection_selected);
    CHECK(mc_client_display_camera_matrices_inspect(app->display, after_projection,
                                                    after_modelview) &&
          memcmp(before_projection, after_projection, sizeof(before_projection)) == 0 &&
          memcmp(before_modelview, after_modelview, sizeof(before_modelview)) == 0);
    app->settings = settings;
    puts("HUD_CONTROL projection_null_settings_before_gl");

    mc_client_gui_ingame_draw_call *call = NULL;
    mc_client_gui_ingame_draw_error error;
    app->v->fields.g = NULL;
    CHECK(mc_client_gui_ingame_draw_begin(app->v, runtime->graphics, math, app->display, .375f,
                                          false, 17, 23, &call,
                                          &error) == MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL);
    CHECK(mc_client_gui_ingame_draw_retire(&call, &error) == MC_CLIENT_GUI_INGAME_DRAW_OK);
    app->v->fields.g = app;
    puts("HUD_CONTROL null_app");

    mc_client_texture_lookup_error *lookup = calloc(1, sizeof(*lookup));
    CHECK(lookup != NULL);
    mc_client_texture texture;
    CHECK(mc_client_render_engine_texture(app->render_engine, app->display, NULL,
                                          (mc_texture_pack_text){NULL, 0, true}, &texture,
                                          lookup) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(mc_client_texture_lookup_pending(app->render_engine));
    CHECK(mc_client_gui_ingame_draw_begin(app->v, runtime->graphics, math, app->display, .375f,
                                          false, 17, 23, &call,
                                          &error) != MC_CLIENT_GUI_INGAME_DRAW_OK);
    mc_client_gui_ingame_draw_state state;
    CHECK(mc_client_gui_ingame_draw_inspect(call, &state) && !state.returned &&
          state.projection_returned && !state.hotbar_returned && state.failure->texture != NULL &&
          state.failure->texture->status == MC_CLIENT_LOOKUP_BUSY);
    CHECK(hud_pending_guards(app->v, runtime->graphics, math, app->display) == 0);
    CHECK(mc_client_display_camera_matrices_inspect(app->display, before_projection,
                                                    before_modelview));
    const uint32_t reached_k = hud_bits(app->v->fields.k);
    const uint64_t reached_random = app->v->fields.f->state;
    CHECK(mc_client_gui_ingame_draw_retire(&call, &error) == MC_CLIENT_GUI_INGAME_DRAW_OK &&
          call == NULL && mc_client_texture_lookup_pending(app->render_engine));
    CHECK(mc_client_display_camera_matrices_inspect(app->display, after_projection,
                                                    after_modelview) &&
          memcmp(before_projection, after_projection, sizeof(before_projection)) == 0 &&
          memcmp(before_modelview, after_modelview, sizeof(before_modelview)) == 0 &&
          hud_bits(app->v->fields.k) == reached_k && app->v->fields.f->state == reached_random);
    CHECK(mc_client_render_engine_retire_lookup(app->render_engine, lookup) ==
              MC_CLIENT_LOOKUP_OK &&
          !mc_client_texture_lookup_pending(app->render_engine));
    free(lookup);
    puts("HUD_CONTROL foreign_lookup_preserved");

    CHECK(hud_prepare(app, player, 0, 0, false) == 0 && hud_reset(runtime, app) == 0);
    mc_game_settings_state options;
    CHECK(mc_game_settings_read_state(app->settings, &options));
    options.values.show_debug_info = true;
    CHECK(mc_game_settings_set_values(app->settings, &options.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_gui_ingame_draw_begin(app->v, runtime->graphics, math, app->display, .375f,
                                          false, 17, 23, &call,
                                          &error) == MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY);
    CHECK(mc_client_gui_ingame_draw_inspect(call, &state) && state.source_pc == 1148 &&
          state.hotbar_returned && state.crosshair_returned && state.stats_returned &&
          state.slots_returned && !state.chat_returned && !state.returned);
    CHECK(mc_client_gui_ingame_draw_retire(&call, &error) == MC_CLIENT_GUI_INGAME_DRAW_OK);
    puts("HUD_CONTROL debug_text_dependency_after_hud");

    CHECK(hud_prepare(app, player, 0, 0, false) == 0 && hud_reset(runtime, app) == 0);
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 1, 1, 0) == MC_ITEM_STACK_OK);
    CHECK(mc_inventory_player_set(player->local_player->fields.player.c, 0, stack, NULL) ==
          MC_INVENTORY_OK);
    CHECK(mc_client_gui_ingame_draw_begin(app->v, runtime->graphics, math, app->display, .375f,
                                          false, 17, 23, &call,
                                          &error) == MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY);
    CHECK(mc_client_gui_ingame_draw_inspect(call, &state) && state.source_pc == 1015 &&
          state.failure->child.pc == 23 && state.stats_returned && !state.slots_returned &&
          !state.returned);
    CHECK(mc_client_gui_ingame_draw_retire(&call, &error) == MC_CLIENT_GUI_INGAME_DRAW_OK);
    mc_client_source_fog_state fog;
    CHECK(mc_client_display_source_fog_inspect(app->display, &fog) && fog.lighting_enabled &&
          fog.rescale_normal_enabled);
    CHECK(mc_inventory_player_set(player->local_player->fields.player.c, 0, NULL, NULL) ==
          MC_INVENTORY_OK);
    mc_item_stack_release(stack);
    puts("HUD_CONTROL item_dependency_preserves_lighting");
    return 0;
}

static int hud_blend_pixels(fixture *runtime, client_application *app) {
    mc_client_overlay_setup_state projection;
    mc_client_overlay_setup_error projection_error;
    CHECK(entity_renderer_overlay_setup(app->t, &projection, &projection_error) &&
          projection.returned);
    CHECK(mc_client_display_source_texture_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_blend_enable(app->display, true, NULL));
    CHECK(mc_client_display_source_lighting_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_fog_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_cull_test(app->display, false, NULL));
    CHECK(mc_client_display_source_color_mask(app->display, true, true, true, true, NULL));
    CHECK(mc_client_display_source_depth_mask(app->display, true, NULL));
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(runtime->graphics, &nw) && nw.a != NULL && !nw.fields.w);
    mc_client_tessellator *tessellator = (mc_client_tessellator *)(void *)nw.a;
    const int expected[2][3] = {{38, 51, 38}, {204, 153, 102}};
    for (unsigned int mode = 0; mode < 2; ++mode) {
        CHECK(mc_client_display_source_clear_color(app->display, .2f, .4f, .6f, 1, NULL));
        CHECK(mc_client_display_source_clear(app->display,
                                             GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, NULL));
        CHECK(
            mc_client_display_source_blend_function(app->display, mode == 0 ? 0 : 775, 769, NULL));
        CHECK(mc_client_display_source_color(app->display, mode == 0 ? .25f : 1.0f,
                                             mode == 0 ? .5f : 1.0f, mode == 0 ? .75f : 1.0f, 1,
                                             NULL));
        mc_client_graphics_error error;
        CHECK(mc_client_nw_begin_quads(tessellator, &error));
        CHECK(mc_client_nw_vertex(tessellator, 0, 240, -90, &error));
        CHECK(mc_client_nw_vertex(tessellator, 320, 240, -90, &error));
        CHECK(mc_client_nw_vertex(tessellator, 320, 0, -90, &error));
        CHECK(mc_client_nw_vertex(tessellator, 0, 0, -90, &error));
        CHECK(mc_client_nw_draw(tessellator, app->display, &error));
        mc_client_capture pixels = {0};
        CHECK(mc_client_display_capture_rgba(app->display, &pixels, NULL) && pixels.width == 320 &&
              pixels.height == 240);
        const size_t center = pixels.stride * 120u + 160u * 4u;
        printf("HUD_BLEND %u %u %u %u\n", mode, (unsigned int)pixels.pixels[center],
               (unsigned int)pixels.pixels[center + 1], (unsigned int)pixels.pixels[center + 2]);
        for (size_t channel = 0; channel < 3; ++channel)
            CHECK(abs((int)pixels.pixels[center + channel] - expected[mode][channel]) <= 1);
        mc_client_capture_free(&pixels);
    }
    puts("HUD_CONTROL blend_numeric_pairs");
    return 0;
}

static int hud_device(const char *resources, const char *assets, const char *directory,
                      const char *screenshot) {
    fixture runtime;
    startup_fixture startup;
    CHECK(fixture_open(&runtime, resources) == 0 && startup_services(&startup) == 0 &&
          startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(mc_client_application_constructor_allocate(&runtime.services, NULL, &app,
                                                     runtime.error) == MC_CLIENT_APP_OK);
    const mc_client_application_constructor_arguments arguments = {NULL, NULL, NULL,
                                                                   320,  240,  false};
    CHECK(mc_client_application_constructor_construct(app, &arguments, runtime.error) ==
              MC_CLIENT_APP_OK &&
          app->constructor_phase == MC_CLIENT_APP_READY && statistics_session(app) == 0);
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options options = mc_client_application_startup_default_options();
    options.hidden = true;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
              MC_CLIENT_STARTUP_UNIMPLEMENTED &&
          startup.error->caller.pc == 32);
    CHECK(statistics_retire_resources(attempt) == 0);
    CHECK(app->v != NULL && app->v->phase == MC_CLIENT_GUI_INGAME_READY &&
          app->v->fields.f != NULL);
    mc_math_classes *math = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    hud_world world = {0};
    CHECK(hud_world_open(&world, &runtime, app, math, directory) == 0);
    CHECK(world.chunks[0]->data != NULL &&
          mc_chunk_nibbles_set(world.chunks[0]->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 127, 8,
                               15));
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(runtime.services.entities,
                                                     runtime.services.profile, 0, &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_construct(controller, app, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(app->controller == NULL);
    app->controller = controller;
    CHECK(hud_profiles(&runtime, app, &world, math) == 0);
    CHECK(hud_pixels(&runtime, app, world.player, math, screenshot) == 0);
    CHECK(hud_blend_pixels(&runtime, app) == 0);
    CHECK(hud_failures(&runtime, app, world.player, math) == 0);
    app->controller = NULL;
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(hud_world_close(&world, app) == 0);
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
    CHECK(argc == 5 && hud_device(argv[1], argv[2], argv[3], argv[4]) == 0);
    printf("GuiIngame draw: %u checks; 26 original profiles and visible default HUD\n", checks);
    return 0;
}
