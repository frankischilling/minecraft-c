/* Authored direct YC component observations. Actual whole App constructor,
 * staged Display/Input, Fonts, source-loaded Worlds, DA initialization and
 * YC init/tick/key/draw bodies. Public App component operands and Sign marker3
 * are declared fixture prestates. No private Source writes, frame setup,
 * Source close, event polling, swap or failed GL/NW prefix repair. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
int mc_yc_drawing_startup_fixture_main(int, char **);
#define main mc_yc_drawing_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/staged.h"
#include "client/gui/bitmap_font.h"
#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_sign_drawing.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/texture_lookup.h"
#include "mc/core/math/classes.h"
#include "mc/entity/mob/construction.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/source_construction.h"
#include "mc/world/storage/source_regions.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/construction_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <inttypes.h>

enum { YD_WORLDS = 5 };

static const int32_t yd_blocks[YD_WORLDS] = {63, 68, 68, 68, 68};
static const int32_t yd_metadata[YD_WORLDS] = {8, 2, 3, 4, 5};
static const int32_t yd_ticks[] = {0, 5, 6, 11, 12};
static const float yd_angles[YD_WORLDS] = {180, 180, 0, 90, -90};

typedef struct yd_world {
    mc_world_store *store;
    mc_world_runtime *world;
    mc_chunk_provider_fields provider;
    mc_chunk_state *chunks[4];
    mc_tile_entity *tile;
    mc_entity *viewer;
} yd_world;

typedef struct yd_snapshot {
    mc_client_source_draw_state draw;
    mc_client_nw_fields nw;
    float projection[16], modelview[16];
    int32_t matrix_mode, marker;
    size_t modelview_depth, projection_depth;
    GLint bound_texture;
    GLboolean depth_write, blend;
    GLenum error;
} yd_snapshot;

typedef struct yd_fixture {
    fixture runtime;
    client_application *app;
    mc_resource_files *assets, *icons_source;
    mc_client_sdk_icons *icons;
    mc_client_display *display;
    mc_client_display_mode *mode;
    mc_client_sdk_input *input;
    mc_client_texture_pack_list *packs;
    mc_client_default_font *default_font;
    mc_client_render_engine *engine;
    mc_client_bitmap_font *font1, *font2;
    mc_client_screen_classes *screens;
    mc_client_source_yc *yc;
    mc_client_sign_draw_call *call;
    mc_math_classes *math;
    mc_world_runtime_classes *world_classes;
    mc_java_files *files;
    mc_java_logging *logging;
    mc_world_source_regions *regions;
    mc_world_store_source_classes *handlers;
    mc_java_file *root;
    mc_client_tile_renderer_classes *renderers;
    mc_client_tile_renderer_dispatcher *manager;
    mc_client_tessellator *nw;
    mc_world_source_error *source_error;
    mc_world_source_region_error *region_error;
    mc_live_tile_entity_error *tile_error;
    mc_client_sign_draw_error *draw_error;
    mc_client_texture_lookup_error *texture_error;
    yd_world worlds[YD_WORLDS];
    FILE *output;
    size_t world_count, rows;
    SDL_DisplayID monitor;
} yd_fixture;

static uint32_t yd_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void yd_string(FILE *out, const char *value) {
    if (value == NULL) {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)value; *p != 0; ++p) {
        if (*p == '"' || *p == '\\')
            fputc('\\', out);
        if (*p < 32)
            fprintf(out, "\\u%04x", (unsigned int)*p);
        else
            fputc(*p, out);
    }
    fputc('"', out);
}

static mc_world_info_name *yd_text(const char *value) {
    uint16_t units[128];
    const size_t count = strlen(value);
    if (count > sizeof(units) / sizeof(*units))
        return NULL;
    for (size_t i = 0; i < count; ++i) {
        if ((unsigned char)value[i] >= 128)
            return NULL;
        units[i] = (unsigned char)value[i];
    }
    mc_world_info_name *name = NULL;
    return mc_world_info_name_copy((mc_nbt_string){units, count}, SIZE_MAX, &name) ==
                   MC_WORLD_INFO_OK
               ? name
               : NULL;
}

static int yd_font(yd_fixture *f, mc_client_bitmap_font **slot) {
    static const uint16_t path[] = {'/', 'f', 'o', 'n', 't', '/', 'd', 'e', 'f',
                                    'a', 'u', 'l', 't', '.', 'p', 'n', 'g'};
    mc_player_string *name = NULL;
    mc_client_bitmap_font_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL &&
          mc_player_string_create(path, sizeof(path) / sizeof(*path), &name) == MC_PLAYER_OK &&
          mc_client_bitmap_font_allocate(f->runtime.services.gui_classes, NULL, slot) ==
              MC_CLIENT_BITMAP_FONT_OK &&
          mc_client_bitmap_font_construct(*slot, f->app->settings, name, f->engine, f->display,
                                          error) == MC_CLIENT_BITMAP_FONT_OK);
    mc_player_string_release(name);
    free(error);
    return 0;
}

static int yd_open(yd_fixture *f, const char *resources, const char *assets, const char *worlds,
                   const char *icons) {
    CHECK(
        fixture_open(&f->runtime, resources) == 0 && actual_app(&f->runtime, false, &f->app) == 0 &&
        mc_registry_classes_initialize_statistics(f->runtime.services.registry, NULL) ==
            MC_REGISTRY_OK &&
        mc_game_settings_create_defaults(NULL, &f->app->settings, NULL) == MC_GAME_SETTINGS_OK &&
        mc_resource_files_create(assets, &f->assets) == MC_REGISTRY_OK &&
        mc_client_loading_classes_bind_resources(f->runtime.services.gui_classes,
                                                 mc_resource_files_source(f->assets),
                                                 NULL) == MC_CLIENT_LOADING_OK &&
        mc_client_texture_pack_list_allocate(mc_resource_files_source(f->assets), NULL,
                                             &f->packs) == MC_TEXTURE_PACK_OK &&
        mc_client_texture_pack_list_construct(f->packs, f->app, worlds, NULL) ==
            MC_TEXTURE_PACK_OK &&
        mc_client_default_font_create(32u * 1024u * 1024u, &f->default_font) == MC_CLIENT_FONT_OK &&
        mc_client_render_engine_allocate(f->runtime.graphics, f->default_font, NULL, &f->engine) ==
            MC_CLIENT_RENDER_ENGINE_OK &&
        mc_client_render_engine_construct(f->engine, f->packs, f->app->settings, NULL) ==
            MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_sdk_icons_error icons_error;
    mc_client_display_stage_error display_error;
    CHECK(mc_resource_files_create(icons, &f->icons_source) == MC_REGISTRY_OK &&
          mc_client_sdk_icons_allocate(mc_resource_files_source(f->icons_source), &f->icons,
                                       &icons_error) == MC_CLIENT_SDK_ICONS_OK &&
          mc_client_sdk_icons_construct(f->icons, &icons_error) == MC_CLIENT_SDK_ICONS_OK &&
          mc_client_display_stage_allocate(&f->display, &display_error) &&
          mc_client_display_stage_bind_icons(f->display, f->icons, &display_error) &&
          mc_client_display_mode_allocate(&f->mode, &display_error) &&
          mc_client_display_mode_construct(f->mode, 320, 240, &display_error) &&
          mc_client_display_stage_set_mode(f->display, f->mode, &display_error) &&
          mc_client_display_create_staged(f->display, true, &display_error));
#ifdef _WIN32
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    CHECK(displays != NULL && count >= 2);
    f->monitor = displays[1];
    SDL_free(displays);
    CHECK(SDL_SetWindowPosition(mc_client_display_window(f->display),
                                (int)SDL_WINDOWPOS_CENTERED_DISPLAY(f->monitor),
                                (int)SDL_WINDOWPOS_CENTERED_DISPLAY(f->monitor)) &&
          SDL_GetDisplayForWindow(mc_client_display_window(f->display)) == f->monitor);
#else
    /* An actual headless/native Linux Display has no monitor2 claim. Preserve
     * its real SDL identity without inventing a physical monitor ordinal. */
    f->monitor = SDL_GetDisplayForWindow(mc_client_display_window(f->display));
    CHECK(f->monitor != 0);
#endif
    f->input = mc_client_display_inputs(f->display);
    mc_client_sdk_input_state input;
    CHECK(f->input != NULL && mc_client_sdk_input_inspect(f->input, &input) &&
          input.phase == MC_CLIENT_SDK_INPUT_READY && yd_font(f, &f->font1) == 0 &&
          yd_font(f, &f->font2) == 0);
    mc_client_bitmap_font_state font1, font2;
    CHECK(mc_client_bitmap_font_inspect(f->font1, &font1) &&
          mc_client_bitmap_font_inspect(f->font2, &font2) && f->font1 != f->font2 &&
          font1.fields.a != font2.fields.a && font1.fields.c != font2.fields.c &&
          mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->runtime.services.profile,
                                          mc_math_classes_table(f->math), &f->world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->world_classes, f->runtime.services.registry,
                                                 NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(f->world_classes, f->runtime.services.entities) ==
              MC_ENTITY_OK &&
          mc_client_screen_classes_create(
              f->runtime.services.profile, mc_resource_files_source(f->runtime.files),
              f->runtime.services.registry, &f->screens, NULL) == MC_CLIENT_SCREEN_OK);
    f->source_error = calloc(1, sizeof(*f->source_error));
    f->region_error = calloc(1, sizeof(*f->region_error));
    f->tile_error = calloc(1, sizeof(*f->tile_error));
    f->draw_error = calloc(1, sizeof(*f->draw_error));
    f->texture_error = calloc(1, sizeof(*f->texture_error));
    CHECK(f->source_error && f->region_error && f->tile_error && f->draw_error &&
          f->texture_error &&
          mc_java_files_create(NULL, &f->files, &f->source_error->file) == MC_JAVA_FILE_OK &&
          mc_java_logging_create(NULL, &f->logging, &f->source_error->logging) ==
              MC_JAVA_LOGGING_OK &&
          mc_world_source_regions_create(f->files, NULL, &f->regions, f->region_error) ==
              MC_WORLD_SOURCE_OK &&
          mc_world_store_source_classes_create(f->files, f->logging, NULL, &f->handlers,
                                               f->source_error) == MC_WORLD_SOURCE_OK &&
          mc_world_store_source_classes_bind_regions(f->handlers, f->regions, f->source_error) ==
              MC_WORLD_SOURCE_OK);
    mc_world_info_name *root = yd_text(worlds);
    CHECK(root != NULL &&
          mc_java_file_allocate(f->files, &f->root, &f->source_error->file) == MC_JAVA_FILE_OK &&
          mc_java_file_construct_string(f->root, root, &f->source_error->file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(root);
    CHECK(mc_client_tile_renderer_classes_bind(f->runtime.services.render_classes, f->world_classes,
                                               f->runtime.services.profile, NULL, &f->renderers,
                                               NULL) == MC_CLIENT_TILE_RENDERER_OK &&
          mc_client_tile_renderer_static_get(f->renderers, &f->manager, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f->manager != NULL);
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->runtime.graphics,
                                  (mc_client_graphics_site){"YCDrawFixture", "setup", "()V", -1},
                                  &f->nw, &graphics));
    const mc_client_display_info *device = mc_client_display_info_get(f->display);
    CHECK(device != NULL);
    fputs("{\"schema\":\"NATIVE_YC_DIRECT_DRAW_V1\",\"device\":{\"vendor\":", f->output);
    yd_string(f->output, device->vendor);
    fputs(",\"renderer\":", f->output);
    yd_string(f->output, device->renderer);
    fputs(",\"version\":", f->output);
    yd_string(f->output, device->version);
    fprintf(f->output,
            ",\"hidden\":true,\"monitor\":%u,\"display_id\":%" PRIu32
            "},\"font1\":{\"texture\":%" PRId32 ",\"lists\":%" PRId32
            "},\"font2\":{\"texture\":%" PRId32 ",\"lists\":%" PRId32 "},\"rows\":[\n",
#ifdef _WIN32
            2u,
#else
            0u,
#endif
            f->monitor, font1.fields.a, font1.fields.c, font2.fields.a, font2.fields.c);
    return 0;
}

static int yd_world_open(yd_fixture *f, size_t index) {
    yd_world *w = &f->worlds[index];
    char folder[16];
    CHECK(snprintf(folder, sizeof(folder), "case%02zu", index) > 0);
    mc_world_info_name *path = yd_text(folder), *name = yd_text("Native YC draw");
    CHECK(path != NULL && name != NULL &&
          mc_world_store_source_allocate(f->handlers, &w->store, f->source_error) ==
              MC_WORLD_SOURCE_OK &&
          mc_world_store_source_construct(w->store, f->root, path, false, f->source_error) ==
              MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(path);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->world_classes,
                                          &limits, &w->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {.store = w->store, .name = name, .seed = 23};
    CHECK(mc_world_runtime_construct_world(w->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_info_name_release(name);
    mc_world_runtime_fields fields;
    CHECK(mc_world_runtime_fields_get(w->world, &fields) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(fields.v, &w->provider, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_source_construction_state source;
    CHECK(mc_world_runtime_source_construction_inspect(w->world, &source) &&
          source.metadata_returned && !source.loaded_info_was_null);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state **chunk = &w->chunks[(size_t)(x * 2 + z)];
            CHECK(mc_world_runtime_get(w->world, x, z, chunk) == MC_CHUNK_ACQUIRE_OK &&
                  *chunk != NULL && !(*chunk)->blank && (*chunk)->active &&
                  (*chunk)->terrain_populated);
        }
    int32_t id = -1, metadata = -1;
    CHECK(mc_world_runtime_block_get(w->world, 3, 64, 3, &id, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(w->world, 3, 64, 3, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          id == yd_blocks[index] && metadata == yd_metadata[index] &&
          mc_live_tile_entities_get(w->world, 3, 64, 3, &w->tile, f->tile_error) ==
              MC_WORLD_STORAGE_OK &&
          w->tile != NULL);
    mc_tile_entity_snapshot tile;
    CHECK(mc_tile_entity_get_snapshot(w->tile, &tile) == MC_TILE_ENTITY_OK &&
          tile.kind == MC_TILE_ENTITY_SIGN && tile.world == w->world && tile.sign_line_count == 4 &&
          tile.sign_edit_row == -1 && !tile.sign_read_flag);
    const mc_entity_class_literal *pig = NULL;
    mc_mob_error mob;
    CHECK(mc_entity_class_literal_get(f->runtime.services.entities, MC_ENTITY_CLASS_KIND_PIG,
                                      &pig) == MC_ENTITY_OK &&
          mc_mob_allocate(f->runtime.services.entities, f->runtime.services.registry, pig,
                          &w->viewer, &mob) == MC_MOB_OK &&
          mc_mob_construct(w->viewer, w->world, &mob) == MC_MOB_OK);
    ++f->world_count;
    return 0;
}

static int yd_screen(yd_fixture *f, yd_world *w, bool captured_null, bool manager_null) {
    /* Declared public component operands, under actual native App ownership.
     * DA.a captures q==Font1 (or NULL); subsequent q==Font2 is a separate
     * public prestate. LL's frame receives Font1 independently of App.q. */
    f->app->active_world = w->world;
    f->app->render_engine = f->engine;
    f->app->font_renderer = captured_null ? NULL : f->font1;
    const mc_client_sign_screen_dependencies dependencies = {
        .mapping = mc_client_loading_classes_mapping(f->runtime.services.gui_classes),
        .input = f->input,
        .literal_classes = f->world_classes};
    mc_client_screen_init_error *init = calloc(1, sizeof(*init));
    CHECK(init != NULL &&
          mc_client_screen_sign_allocate(f->screens, &dependencies, NULL, &f->yc, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_construct(f->yc, w->tile, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_initialize(mc_client_screen_sign_base(f->yc), f->app, 320, 240, init) ==
              MC_CLIENT_SCREEN_OK);
    free(init);
    f->app->font_renderer = f->font2;
    mc_client_screen_state base;
    mc_client_sign_screen_state screen;
    mc_client_sdk_input_state input;
    CHECK(mc_client_screen_inspect(mc_client_screen_sign_base(f->yc), &base, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          base.da.b == f->app && base.da.c == 320 && base.da.d == 240 &&
          base.da.g == (captured_null ? NULL : f->font1) &&
          mc_client_screen_sign_inspect(f->yc, &screen, NULL) == MC_CLIENT_SCREEN_OK &&
          screen.initialization_count == 1 && screen.tick == 0 && screen.row == 0 &&
          mc_client_sdk_input_inspect(f->input, &input) && input.repeat_enabled &&
          mc_client_tile_renderer_bind_frame(f->manager, w->world, f->engine,
                                             manager_null ? NULL : f->font1, w->viewer, 0,
                                             NULL) == MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_state ll;
    CHECK(mc_client_tile_renderer_inspect(f->renderers, &ll) &&
          ll.fields.n == (manager_null ? NULL : f->font1) && ll.fields.f == w->world &&
          ll.fields.e == f->engine && f->app->font_renderer == f->font2);
    return 0;
}

static int yd_snapshot_get(yd_fixture *f, yd_world *w, yd_snapshot *s) {
    mc_tile_entity_snapshot tile;
    memset(s, 0, sizeof(*s));
    CHECK(mc_client_display_make_current(f->display, NULL) &&
          mc_client_display_source_draw_inspect(f->display, &s->draw) &&
          mc_client_display_camera_matrices_inspect(f->display, s->projection, s->modelview) &&
          mc_client_display_camera_stack_inspect(f->display, &s->matrix_mode, &s->modelview_depth,
                                                 &s->projection_depth) &&
          mc_client_nw_fields_get(f->nw, &s->nw) &&
          mc_tile_entity_get_snapshot(w->tile, &tile) == MC_TILE_ENTITY_OK);
    s->marker = tile.sign_edit_row;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->bound_texture);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &s->depth_write);
    s->blend = glIsEnabled(GL_BLEND);
    s->error = glGetError();
    CHECK(s->error == GL_NO_ERROR);
    return 0;
}

static void yd_snapshot_write(FILE *out, const yd_snapshot *s) {
    fprintf(out,
            "{\"marker\":%" PRId32 ",\"matrix_mode\":%" PRId32
            ",\"modelview_depth\":%zu,\"projection_depth\":%zu,\"modelview\":[",
            s->marker, s->matrix_mode, s->modelview_depth, s->projection_depth);
    for (size_t i = 0; i < 16; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", yd_bits(s->modelview[i]));
    fputs("],\"projection\":[", out);
    for (size_t i = 0; i < 16; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", yd_bits(s->projection[i]));
    fprintf(out,
            "],\"texture\":%d,\"depth_write\":%s,\"blend\":%s,\"error\":%u,"
            "\"source\":{\"texture\":%s,\"alpha\":%s,\"flat\":%s,\"blend_known\":%s,"
            "\"blend\":%s,\"depth_known\":%s,\"depth_write\":%s},"
            "\"nw\":{\"h\":%" PRId32 ",\"o\":%" PRId32 ",\"p\":%" PRId32
            ",\"w\":%s,\"x\":%s,\"q\":%s}}",
            s->bound_texture, s->depth_write ? "true" : "false", s->blend ? "true" : "false",
            (unsigned int)s->error, s->draw.texture_enabled ? "true" : "false",
            s->draw.alpha_test ? "true" : "false", s->draw.shade_flat ? "true" : "false",
            s->draw.blend_known ? "true" : "false",
            s->draw.requested_blend_enabled ? "true" : "false",
            s->draw.depth_mask_known ? "true" : "false",
            s->draw.requested_depth_mask ? "true" : "false", s->nw.h, s->nw.o, s->nw.p,
            s->nw.w ? "true" : "false", s->nw.x ? "true" : "false", s->nw.q ? "true" : "false");
}

static void yd_site(FILE *out, mc_client_screen_site site) {
    fputs("{\"class\":", out);
    yd_string(out, site.class_name);
    fputs(",\"method\":", out);
    yd_string(out, site.method);
    fputs(",\"descriptor\":", out);
    yd_string(out, site.descriptor);
    fprintf(out, ",\"pc\":%u}", site.pc);
}

static int yd_draw(yd_fixture *f, size_t world_index, size_t stage, const char *mode) {
    yd_world *w = &f->worlds[world_index];
    const bool healthy = strcmp(mode, "healthy") == 0;
    const bool captured_null = strcmp(mode, "captured-null") == 0;
    const bool manager_null = strncmp(mode, "manager-null", 12) == 0;
    const bool open_batch = strcmp(mode, "nw-open") == 0;
    const int32_t tick = healthy ? yd_ticks[stage] : strcmp(mode, "manager-null-off") == 0 ? 6 : 0;
    mc_client_sign_screen_state screen;
    CHECK(mc_client_screen_sign_inspect(f->yc, &screen, NULL) == MC_CLIENT_SCREEN_OK);
    for (int32_t i = screen.tick; i < tick; ++i)
        CHECK(mc_client_screen_sign_tick(f->yc, NULL) == MC_CLIENT_SCREEN_OK);
    if (healthy && stage != 0)
        CHECK(mc_client_screen_sign_key(f->yc, 0, 208, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_sign_inspect(f->yc, &screen, NULL) == MC_CLIENT_SCREEN_OK &&
          screen.tick == tick && screen.row == (healthy ? (int32_t)(stage % 4) : 0) &&
          mc_tile_entity_sign_set_edit_row(w->tile, 3) == MC_TILE_ENTITY_OK);
    if (open_batch) {
        mc_client_graphics_error graphics;
        CHECK(mc_client_nw_begin_quads(f->nw, &graphics));
    }
    yd_snapshot before, after, retired;
    CHECK(yd_snapshot_get(f, w, &before) == 0 && before.marker == 3);
    const mc_client_sign_draw_services services = {
        .draw = {f->math, f->runtime.graphics, f->display, f->runtime.services.application_classes},
        .models = f->runtime.services.render_classes,
        .world_classes = f->world_classes,
        .registry = f->runtime.services.registry,
        .profile = f->runtime.services.profile};
    const mc_gui_mouse_position mouse = {.x = stage % 2 == 0 ? 160 : 0,
                                         .y = stage % 2 == 0 ? 185 : 0};
    const mc_client_screen_status result =
        mc_client_source_sign_draw_begin(f->yc, mouse, 0, &services, NULL, &f->call, f->draw_error);
    mc_client_sign_draw_state state;
    CHECK(f->call != NULL && mc_client_source_sign_draw_inspect(f->call, &state) && !state.active &&
          state.receiver == f->yc && yd_snapshot_get(f, w, &after) == 0 &&
          mc_client_screen_sign_terminal_prepare(f->yc, NULL) == MC_CLIENT_SCREEN_BUSY);
    if (healthy) {
        CHECK(result == MC_CLIENT_SCREEN_OK && state.returned && state.background_returned &&
              state.title_returned && state.matrix_pushed && state.matrix_popped &&
              state.block_returned && state.metadata_returned && state.preview_returned &&
              state.marker_cleared && state.buttons_returned && state.buttons_seen == 1 &&
              state.buttons_drawn == 1 && state.font_measurements == 2 && state.font_draws == 2 &&
              state.source_pc == 236 && state.tick == tick && state.selected_row == screen.row &&
              state.metadata == yd_metadata[world_index] && state.standing == (world_index == 0) &&
              yd_bits(state.angle) == yd_bits(yd_angles[world_index]) &&
              state.marker_stored == ((tick / 6) % 2 == 0) && state.preview != NULL &&
              state.preview->coordinates_only && state.preview->x == -0.5 &&
              state.preview->y == -0.75 && state.preview->z == -0.5 &&
              state.preview->partial == 0 && state.preview->sign_rows_measured == 4 &&
              state.preview->sign_rows_drawn == 4 && state.preview->sign_font_returned &&
              after.marker == -1 && after.modelview_depth == before.modelview_depth &&
              after.projection_depth == before.projection_depth && !after.nw.w);
        for (size_t row = 0; row < 4; ++row)
            CHECK(state.preview->sign_rows[row].captured &&
                  state.preview->sign_rows[row].selected ==
                      ((int32_t)row == (state.marker_stored ? screen.row : 3)));
        mc_client_bitmap_font_state font2;
        CHECK(mc_client_bitmap_font_inspect(f->font2, &font2) &&
              after.bound_texture == font2.fields.a &&
              mc_client_bitmap_font_retire(&f->font2, NULL) == MC_CLIENT_BITMAP_FONT_BUSY);
    } else {
        CHECK(result != MC_CLIENT_SCREEN_OK && !state.returned &&
              f->draw_error->screen.source_exception && !f->draw_error->screen.native_policy &&
              f->draw_error->screen.exception_class != NULL && !state.marker_cleared &&
              !state.matrix_popped && !state.buttons_returned);
        if (captured_null)
            CHECK(result == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE && state.background_returned &&
                  !state.title_returned && !state.matrix_pushed && !state.marker_stored &&
                  f->draw_error->screen.site.pc == 5 &&
                  strcmp(f->draw_error->screen.site.class_name, "ub") == 0 &&
                  strcmp(f->draw_error->screen.exception_class, "java/lang/NullPointerException") ==
                      0 &&
                  after.marker == 3 && after.modelview_depth == before.modelview_depth);
        else if (manager_null)
            CHECK(state.background_returned && state.title_returned && state.matrix_pushed &&
                  state.marker_stored == (tick == 0) && !state.preview_returned &&
                  state.source_pc == 215 && state.preview != NULL &&
                  state.preview->sign_model_returned && state.preview->sign_model_popped &&
                  state.preview->sign_depth_disabled && !state.preview->sign_depth_restored &&
                  strcmp(f->draw_error->screen.exception_class, "java/lang/NullPointerException") ==
                      0 &&
                  after.marker == (tick == 0 ? 0 : 3) &&
                  after.modelview_depth == before.modelview_depth + 2 && !after.depth_write);
        else {
            CHECK(open_batch && !state.background_returned && !state.title_returned &&
                  !state.matrix_pushed && !state.marker_stored && after.marker == 3 &&
                  f->draw_error->graphics.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE &&
                  strcmp(f->draw_error->screen.site.class_name, "nw") == 0 &&
                  strcmp(f->draw_error->screen.exception_class,
                         "java/lang/IllegalStateException") == 0 &&
                  after.nw.w && after.nw.h == before.nw.h && after.nw.o == before.nw.o &&
                  !after.draw.texture_enabled && !after.draw.alpha_test && !after.draw.shade_flat &&
                  after.draw.requested_blend_enabled && after.blend);
        }
    }
    FILE *out = f->output;
    fprintf(out,
            "%s{\"index\":%zu,\"world\":%zu,\"stage\":%zu,\"tick\":%" PRId32 ",\"row\":%" PRId32
            ",\"returned\":%s,\"before\":",
            f->rows ? ",\n" : "", f->rows, world_index, stage, tick, screen.row,
            state.returned ? "true" : "false");
    yd_snapshot_write(out, &before);
    fprintf(out,
            ",\"state\":{\"pc\":%u,\"background\":%s,\"title\":%s,\"push\":%s,"
            "\"pop\":%s,\"preview\":%s,\"marker_stored\":%s,\"marker_cleared\":%s,"
            "\"buttons\":%zu,\"measurements\":%zu,\"font_draws\":%zu,\"angle\":\"%08" PRIx32
            "\",\"title_width\":%" PRId32 ",\"title_x\":%" PRId32 "},\"preview_rows\":[",
            state.source_pc, state.background_returned ? "true" : "false",
            state.title_returned ? "true" : "false", state.matrix_pushed ? "true" : "false",
            state.matrix_popped ? "true" : "false", state.preview_returned ? "true" : "false",
            state.marker_stored ? "true" : "false", state.marker_cleared ? "true" : "false",
            state.buttons_drawn, state.font_measurements, state.font_draws, yd_bits(state.angle),
            state.title_width, state.title_x);
    if (state.preview != NULL)
        for (size_t i = 0; i < 4; ++i) {
            const mc_client_tile_renderer_sign_row_receipt *r = &state.preview->sign_rows[i];
            fprintf(out,
                    "%s{\"captured\":%s,\"selected\":%s,\"units\":%zu,\"digest\":\"%016" PRIx64
                    "\",\"measured\":%s,\"drawn\":%s,\"width\":%" PRId32 ",\"x\":%" PRId32
                    ",\"y\":%" PRId32 "}",
                    i ? "," : "", r->captured ? "true" : "false", r->selected ? "true" : "false",
                    r->text_units, r->text_digest, r->measure_returned ? "true" : "false",
                    r->draw_returned ? "true" : "false", r->width, r->x, r->y);
        }
    fputs("],\"failure\":{\"status\":", out);
    fprintf(out, "%d", (int)result);
    fputs(",\"site\":", out);
    yd_site(out, f->draw_error->screen.site);
    fputs(",\"caller\":", out);
    yd_site(out, f->draw_error->caller);
    fputs(",\"helper\":", out);
    yd_site(out, f->draw_error->helper);
    fprintf(out, ",\"native\":%s,\"exception\":%s,\"error\":%s,\"class\":",
            f->draw_error->screen.native_policy ? "true" : "false",
            f->draw_error->screen.source_exception ? "true" : "false",
            f->draw_error->screen.source_error ? "true" : "false");
    yd_string(out, f->draw_error->screen.exception_class);
    fputs("},\"after\":", out);
    yd_snapshot_write(out, &after);
    CHECK(fflush(out) == 0);
    CHECK(mc_client_source_sign_draw_retire(&f->call, f->draw_error) == MC_CLIENT_SCREEN_OK &&
          f->call == NULL && yd_snapshot_get(f, w, &retired) == 0 &&
          retired.marker == after.marker && retired.matrix_mode == after.matrix_mode &&
          retired.modelview_depth == after.modelview_depth &&
          retired.projection_depth == after.projection_depth &&
          memcmp(retired.modelview, after.modelview, sizeof(after.modelview)) == 0 &&
          memcmp(retired.projection, after.projection, sizeof(after.projection)) == 0 &&
          retired.depth_write == after.depth_write && retired.blend == after.blend &&
          retired.bound_texture == after.bound_texture && retired.nw.w == after.nw.w &&
          retired.nw.h == after.nw.h && retired.nw.o == after.nw.o && retired.nw.p == after.nw.p &&
          mc_client_source_sign_draw_retire(&f->call, NULL) == MC_CLIENT_SCREEN_OK);
    fputs(",\"after_retire\":", out);
    yd_snapshot_write(out, &retired);
    fputs(",\"retired\":true,\"retire_idempotent\":true}", out);
    CHECK(fflush(out) == 0);
    ++f->rows;
    return 0;
}

static int yd_cleanup_result(yd_fixture *f, const char *call, int actual, int expected,
                             const char *operation) {
    ++checks;
    if (actual == expected)
        return 0;
    fprintf(stderr, "YC_DRAW_CLEANUP call=%s status=%d expected=%d operation=%s\n", call, actual,
            expected, operation == NULL ? "(no child operation)" : operation);
    /* Passive, still-live owner evidence only. No terminal retry, extra Source
     * operation, owner reset, field repair or inferred successful cleanup. */
    mc_client_bitmap_font_state font;
    if (f->font1 != NULL && mc_client_bitmap_font_inspect(f->font1, &font))
        fprintf(stderr, "YC_DRAW_CLEANUP font=1 phase=%d holds=%zu active=%d\n", (int)font.phase,
                font.holds, font.active ? 1 : 0);
    if (f->font2 != NULL && mc_client_bitmap_font_inspect(f->font2, &font))
        fprintf(stderr, "YC_DRAW_CLEANUP font=2 phase=%d holds=%zu active=%d\n", (int)font.phase,
                font.holds, font.active ? 1 : 0);
    mc_client_font_mapping_state mapping;
    if (mc_client_font_mapping_inspect(
            mc_client_loading_classes_mapping(f->runtime.services.gui_classes), &mapping))
        fprintf(stderr, "YC_DRAW_CLEANUP fp-phase=%d fp-holds=%zu\n", (int)mapping.phase,
                mapping.native_holds);
    return 1;
}

static int yd_close(yd_fixture *f) {
    mc_world_storage_error storage = {0};
    mc_math_class_error math = {0};
    mc_client_display_stage_error stage = {0};
#define YD_CLOSE(call, expected, operation)                                                        \
    do {                                                                                           \
        const int result = (int)(call);                                                            \
        if (yd_cleanup_result(f, #call, result, (int)(expected), (operation)) != 0)                \
            return 1;                                                                              \
    } while (0)
    CHECK(f->call == NULL);
    CHECK(f->yc == NULL);
    YD_CLOSE(mc_client_screen_sign_class_terminal_prepare(f->screens, &f->draw_error->screen),
             MC_CLIENT_SCREEN_OK, f->draw_error->screen.operation);
    YD_CLOSE(mc_client_screen_classes_release(&f->screens, &f->draw_error->screen),
             MC_CLIENT_SCREEN_OK, f->draw_error->screen.operation);
    YD_CLOSE(mc_client_tile_renderer_classes_retire(&f->renderers, &f->draw_error->dispatcher),
             MC_CLIENT_TILE_RENDERER_OK, f->draw_error->dispatcher.operation);
    f->app->active_world = NULL;
    f->app->font_renderer = NULL;
    f->app->render_engine = NULL; /* End only declared native fixture views. */
    for (size_t i = 0; i < f->world_count; ++i) {
        yd_world *w = &f->worlds[i];
        mc_entity_release(w->viewer);
        mc_tile_entity_release(w->tile);
        YD_CLOSE(mc_world_runtime_retire_entity_containers(w->world, &storage), MC_WORLD_STORAGE_OK,
                 storage.operation);
        YD_CLOSE(mc_chunk_pending_clear(w->provider.pending), MC_CHUNK_ACQUIRE_OK, NULL);
        YD_CLOSE(mc_chunk_cache_init(w->provider.cache, w->provider.cache->entries,
                                     w->provider.cache->capacity, w->provider.cache->insertions,
                                     w->provider.cache->insertion_capacity),
                 true, NULL);
        YD_CLOSE(mc_world_runtime_retire_world(w->world, &storage), MC_WORLD_STORAGE_OK,
                 storage.operation);
        for (size_t j = 0; j < 4; ++j)
            YD_CLOSE(mc_world_store_retire_chunk(w->store, w->chunks[j], &storage),
                     MC_WORLD_STORAGE_OK, storage.operation);
        YD_CLOSE(mc_world_runtime_free(w->world, &storage), MC_WORLD_STORAGE_OK, storage.operation);
        YD_CLOSE(mc_world_store_source_release(&w->store, f->source_error), MC_WORLD_SOURCE_OK,
                 f->source_error->operation);
    }
    YD_CLOSE(mc_client_bitmap_font_release(&f->font1, &f->draw_error->font),
             MC_CLIENT_BITMAP_FONT_OK, f->draw_error->font.operation);
    YD_CLOSE(mc_client_bitmap_font_release(&f->font2, &f->draw_error->font),
             MC_CLIENT_BITMAP_FONT_OK, f->draw_error->font.operation);
    YD_CLOSE(mc_world_runtime_classes_free(f->world_classes, &storage), MC_WORLD_STORAGE_OK,
             storage.operation);
    YD_CLOSE(mc_world_store_source_classes_retire(&f->handlers, f->source_error),
             MC_WORLD_SOURCE_OK, f->source_error->operation);
    YD_CLOSE(mc_world_source_regions_retire(&f->regions, f->region_error), MC_WORLD_SOURCE_OK,
             f->region_error->source.operation);
    YD_CLOSE(mc_java_logging_retire(&f->logging, &f->source_error->logging), MC_JAVA_LOGGING_OK,
             NULL);
    YD_CLOSE(mc_java_file_release(&f->root, &f->source_error->file), MC_JAVA_FILE_OK, NULL);
    YD_CLOSE(mc_java_files_release(&f->files, &f->source_error->file), MC_JAVA_FILE_OK, NULL);
    YD_CLOSE(mc_math_classes_release(&f->math, &math), MC_MATH_CLASS_OK, math.operation);
    YD_CLOSE(mc_client_render_engine_retire_lookup(f->engine, f->texture_error),
             MC_CLIENT_LOOKUP_OK, f->texture_error->operation);
    YD_CLOSE(mc_client_texture_lookup_free_metadata(f->engine), true, NULL);
    YD_CLOSE(mc_client_render_engine_release(f->engine), MC_CLIENT_RENDER_ENGINE_OK, NULL);
    YD_CLOSE(
        mc_client_render_engine_retire_resources(f->runtime.graphics, &f->draw_error->font.texture),
        MC_CLIENT_RENDER_TEXTURE_OK, f->draw_error->font.texture.operation);
    YD_CLOSE(mc_client_default_font_release(f->default_font), MC_CLIENT_FONT_OK, NULL);
    /* This is the same staged owner that created the SDK/Input namespace.
     * Diagnostic Display.destroy deliberately refuses every sdk_profile;
     * native staged retirement owns its real update/input/icon descendants.
     * All failed Source/GL/NW observations were preserved above; no Source
     * close, repeat store, batch draw or matrix repair is performed here. */
    const bool display_retired = mc_client_display_stage_retire(&f->display, &stage);
    if (!display_retired)
        fprintf(stderr,
                "YC_DRAW_DISPLAY_RETIRE kind=%d owner-retained=%d slot-live=%d "
                "display-status=%d display-detail=%s input-status=%d update-status=%d "
                "icons-status=%d cleanup-status=%d cleanup-detail=%s\n",
                (int)stage.kind, stage.native_owner_retained ? 1 : 0, f->display != NULL ? 1 : 0,
                (int)stage.display.status, stage.display.detail, (int)stage.input.status,
                (int)stage.update.status, (int)stage.icons.status,
                (int)stage.cleanup_display.status, stage.cleanup_display.detail);
    YD_CLOSE(display_retired, true, stage.operation);
    CHECK(f->display == NULL);
    f->input = NULL; /* The actual native child was consumed; end its borrowed fixture view. */
    mc_client_display_mode_release(&f->mode);
    YD_CLOSE(mc_client_sdk_icons_release(&f->icons, NULL), MC_CLIENT_SDK_ICONS_OK, NULL);
    YD_CLOSE(mc_resource_files_free(f->icons_source), MC_REGISTRY_OK, NULL);
    YD_CLOSE(mc_client_texture_pack_list_release(f->packs, NULL), MC_TEXTURE_PACK_OK, NULL);
    YD_CLOSE(mc_game_settings_retire(&f->app->settings, NULL), MC_GAME_SETTINGS_OK, NULL);
    /* The constructor's non-counted namespace borrow first ends through its
     * real native stop/join and child cleanup. The actual static MC.a still
     * holds this App, so final App retirement must report BUSY until the caller
     * releases the class namespace. No source J/a store is supplied here. */
    YD_CLOSE(mc_client_application_constructor_retire(&f->app, f->runtime.error),
             MC_CLIENT_APP_BUSY, f->runtime.error->operation);
    CHECK(f->app != NULL && f->runtime.error->child == MC_CLIENT_APP_CHILD_CLASS &&
          f->runtime.error->operation != NULL &&
          strcmp(f->runtime.error->operation, "actual App worker/static lifetime retirement") == 0);
    mc_client_application_class_state app_classes;
    CHECK(mc_client_application_classes_inspect(f->runtime.services.application_classes,
                                                &app_classes) &&
          app_classes.native_app_borrows == 0 && app_classes.native_holds == 1 &&
          app_classes.a == f->app);
    YD_CLOSE(mc_client_application_classes_release(&f->runtime.services.application_classes,
                                                   &f->runtime.error->cause.owner),
             MC_CLIENT_APPLICATION_OWNER_OK, f->runtime.error->cause.owner.operation);
    YD_CLOSE(mc_client_application_constructor_retire(&f->app, f->runtime.error), MC_CLIENT_APP_OK,
             f->runtime.error->operation);
    CHECK(f->app == NULL);
    YD_CLOSE(mc_registry_classes_free(f->runtime.services.registry, NULL), MC_REGISTRY_OK, NULL);
    f->runtime.services.registry = NULL;
    CHECK(fixture_close(&f->runtime) == 0);
    CHECK(mc_resource_files_free(f->assets) == MC_REGISTRY_OK);
    free(f->source_error);
    free(f->region_error);
    free(f->tile_error);
    free(f->draw_error);
    free(f->texture_error);
#undef YD_CLOSE
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 8);
    const char *mode = argv[6];
    const bool healthy = strcmp(mode, "healthy") == 0;
    CHECK(healthy || strcmp(mode, "captured-null") == 0 || strcmp(mode, "manager-null-on") == 0 ||
          strcmp(mode, "manager-null-off") == 0 || strcmp(mode, "nw-open") == 0);
    SDL_SetMainReady();
    yd_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    f->output = fopen(argv[7], "wb");
    CHECK(f->output != NULL && yd_open(f, argv[1], argv[2], argv[3], argv[4]) == 0);
    /* argv[5] is a closed declaration token; paths and expected outputs are
     * never interpreted as executable input or passed to lower Source calls. */
    CHECK(strcmp(argv[5], "saved-signs-v1") == 0);
    for (size_t i = 0; i < (healthy ? YD_WORLDS : 1u); ++i) {
        CHECK(yd_world_open(f, i) == 0 &&
              yd_screen(f, &f->worlds[i], strcmp(mode, "captured-null") == 0,
                        strncmp(mode, "manager-null", 12) == 0) == 0);
        for (size_t stage = 0; stage < (healthy ? sizeof(yd_ticks) / sizeof(*yd_ticks) : 1u);
             ++stage)
            CHECK(yd_draw(f, i, stage, mode) == 0);
        CHECK(mc_client_screen_sign_retire(&f->yc, NULL) == MC_CLIENT_SCREEN_OK && f->yc == NULL);
    }
    CHECK(yd_close(f) == 0);
    fprintf(f->output,
            "\n],\"cleanup\":true,\"rows_complete\":%zu,\"source_close\":false,"
            "\"terminal_gl_repair\":false,\"terminal_batch_draw\":false,\"swaps\":0,\"polls\":0}\n",
            f->rows);
    CHECK(!ferror(f->output) && fclose(f->output) == 0);
    printf("PASS: native YC direct draw mode=%s rows=%zu checks=%u cleanup=1\n", mode, f->rows,
           checks);
    free(f);
    return 0;
}
