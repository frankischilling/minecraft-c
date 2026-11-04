/* Genuine standalone startup and returning BB GUI calls; no World or frame loop. */
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/application_state.h"
#include "client/display/camera_surface.h"
#include "client/render/blocks/internal.h"
#include "client/render/item_gui.h"
#include "client/render/item_renderer_internal.h"
#include "client/render_engine/internal.h"
#include "client/runtime/bootstrap.h"
#include "client/window/frame.h"
#include "mc/client/settings/game_settings.h"
#include "mc/item/registry/runtime.h"
#include "mc/item/stack/stack.h"

#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Foliage GUI check%u line%d: %s\n", checks, __LINE__, #value);         \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct foliage_fixture {
    mc_client_bootstrap *bootstrap;
    client_application *app;
    mc_client_render_engine_classes *graphics;
    mc_client_item_renderer *renderer;
    mc_item_stack *stack;
    mc_client_item_gui_call *call;
    uint32_t frame_id, canvas_id;
} foliage_fixture;

static uint32_t bits(float value) {
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return raw;
}

static bool draw_equal(const mc_client_source_draw_state *a, const mc_client_source_draw_state *b) {
    return memcmp(a->color, b->color, sizeof(a->color)) == 0 && a->color_known == b->color_known &&
           a->texture_enabled == b->texture_enabled && a->alpha_test == b->alpha_test &&
           a->shade_flat == b->shade_flat && a->lighting_enabled == b->lighting_enabled &&
           a->alpha_function_set == b->alpha_function_set &&
           bits(a->alpha_reference) == bits(b->alpha_reference) &&
           bits(a->line_width) == bits(b->line_width) &&
           bits(a->polygon_offset_factor) == bits(b->polygon_offset_factor) &&
           bits(a->polygon_offset_units) == bits(b->polygon_offset_units) &&
           a->polygon_offset_fill == b->polygon_offset_fill &&
           a->depth_mask_known == b->depth_mask_known &&
           a->requested_depth_mask == b->requested_depth_mask && a->blend_known == b->blend_known &&
           a->requested_blend_enabled == b->requested_blend_enabled &&
           a->blend_function_known == b->blend_function_known &&
           a->requested_blend_source == b->requested_blend_source &&
           a->requested_blend_destination == b->requested_blend_destination;
}

static bool scene(foliage_fixture *f) {
    mc_client_display_error error;
    mc_client_display *d = f->app->display;
    /* Declared native scene inputs, outside the entered GUI parent. */
    CHECK(mc_client_display_begin_scene(d, .0625f, .125f, .1875f, &error) &&
          mc_client_display_source_matrix_mode(d, 5889, &error) &&
          mc_client_display_source_load_identity(d, &error) &&
          mc_client_display_source_ortho(d, 0, 256, 256, 0, 1000, 3000, &error) &&
          mc_client_display_source_matrix_mode(d, 5888, &error) &&
          mc_client_display_source_load_identity(d, &error) &&
          mc_client_display_source_translate(d, 0, 0, -2000, &error) &&
          mc_client_display_source_color(d, .25f, .5f, .75f, 1, &error));
    return true;
}

static bool pixels(foliage_fixture *f, const char *directory, size_t row, int *width, int *height) {
    mc_client_capture capture = {0};
    mc_client_display_error error;
    CHECK(mc_client_display_capture_rgba(f->app->display, &capture, &error) &&
          capture.pixels != NULL && capture.width > 0 && capture.height > 0 &&
          capture.width <= 1024 && capture.height <= 1024 &&
          capture.stride == (size_t)capture.width * 4u);
    char path[8192];
    const int length = snprintf(path, sizeof(path), "%s/row-%02zu.rgba", directory, row);
    if (length <= 0 || (size_t)length >= sizeof(path)) {
        mc_client_capture_free(&capture);
        CHECK(false);
    }
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        mc_client_capture_free(&capture);
        CHECK(false);
    }
    const size_t bytes = capture.stride * (size_t)capture.height;
    const bool written = fwrite(capture.pixels, 1, bytes, output) == bytes;
    const bool closed = fclose(output) == 0;
    *width = capture.width;
    *height = capture.height;
    mc_client_capture_free(&capture);
    CHECK(written && closed);
    return true;
}

static bool profile(foliage_fixture *f, const char *directory, size_t row, int32_t id,
                    int32_t metadata, bool tint) {
    static const int32_t sapling_icons[] = {15, 63, 79, 15};
    static const int32_t leaf_icons[] = {52, 132, 52, 52};
    static const int32_t leaf_colors[] = {4764952, 6396257, 8431445, 6396257};
    const bool leaf = id == 18;
    const int32_t expected_color = leaf ? leaf_colors[metadata] : 0xffffff;
    const int32_t expected_icon = leaf ? leaf_icons[metadata] : sapling_icons[metadata];
    CHECK(f->stack == NULL && f->call == NULL &&
          mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, id, 2, metadata) == MC_ITEM_STACK_OK);
    mc_item_stack_fields stack_before, stack_after;
    CHECK(mc_item_stack_fields_get(f->stack, &stack_before) == MC_ITEM_STACK_OK);
    f->renderer->fields.a = tint; /* Declared actual BB field input. */
    CHECK(scene(f));
    float projection[16], modelview[16], after_projection[16], after_modelview[16];
    int32_t mode, after_mode;
    size_t depth, projection_depth, after_depth, after_projection_depth;
    mc_client_render_engine_class_state graphics_before, graphics_after;
    mc_client_render_engine_state engine_before, engine_after;
    const size_t lease_before = f->graphics->scene_lease_count;
    const size_t bytes_before = f->renderer->bytes;
    const size_t classes_before = f->app->render_classes->owners;
    CHECK(
        mc_client_display_camera_matrices_inspect(f->app->display, projection, modelview) &&
        mc_client_display_camera_stack_inspect(f->app->display, &mode, &depth, &projection_depth) &&
        mc_client_render_engine_classes_inspect(f->graphics, &graphics_before) &&
        mc_client_render_engine_inspect(f->app->render_engine, &engine_before));
    mc_client_item_gui_error error;
    CHECK(mc_client_item_gui_begin(f->renderer, f->graphics, f->app->registry, f->app->display,
                                   NULL, f->app->render_engine, f->stack, 112, 112, &f->call,
                                   &error) == MC_CLIENT_ITEM_GUI_OK);
    mc_client_item_gui_state state;
    CHECK(mc_client_item_gui_inspect(f->call, &state) && state.returned && !state.active &&
          !state.retiring && state.entry_pc == 27 && state.raw_pc == 367 && state.id_read &&
          state.metadata_read && state.id == id && state.metadata == metadata &&
          state.owner == f->renderer && state.registry == f->app->registry &&
          state.graphics == f->graphics && state.engine == f->app->render_engine &&
          state.display == f->app->display && state.stack == f->stack && state.icon_returned &&
          state.icon == expected_icon && state.color_returned && state.color == expected_color &&
          state.color_set == tint && state.texture_bound && state.block_type_returned &&
          state.block_3d == leaf && state.block_inventory_entered == leaf &&
          state.block_inventory_returned == leaf && state.block_faces == (leaf ? 6u : 0u) &&
          !state.block_matrix_pushed && state.quad_drawn == !leaf && state.quad_returned == !leaf &&
          state.vertices_returned == (leaf ? 0u : 4u) && state.culling_enabled &&
          !state.failure->native_policy && !state.failure->source_exception &&
          state.failure->status == MC_CLIENT_ITEM_GUI_OK && state.failure->item.returned &&
          strcmp(state.failure->item.site.class_name, leaf ? "uy" : "gm") == 0 &&
          strcmp(state.failure->item.site.method, "f") == 0 &&
          state.failure->item.colorizer_entered == leaf &&
          state.failure->item.colorizer_returned == leaf);
    if (leaf) {
        CHECK(strcmp(state.failure->item.child.class_name, "jh") == 0 &&
              strcmp(state.failure->item.child.method, metadata & 1   ? "a"
                                                       : metadata & 2 ? "b"
                                                                      : "c") == 0 &&
              state.failure->item.child.pc == 2 && state.failure->inventory != NULL &&
              state.failure->inventory->returned && state.failure->inventory->inventory_faces == 6);
    }
    for (size_t i = 0; i < 3; ++i) {
        const unsigned int shift = (unsigned int)(16 - 8 * i);
        volatile float expected = (float)(((uint32_t)expected_color >> shift) & 255u) / 255.0f;
        CHECK(bits(state.color_rgba[i]) == bits(expected));
    }
    CHECK(state.color_rgba[3] == 1 &&
          mc_client_display_camera_matrices_inspect(f->app->display, after_projection,
                                                    after_modelview) &&
          mc_client_display_camera_stack_inspect(f->app->display, &after_mode, &after_depth,
                                                 &after_projection_depth) &&
          mode == after_mode && depth == after_depth &&
          projection_depth == after_projection_depth &&
          memcmp(projection, after_projection, sizeof(projection)) == 0 &&
          memcmp(modelview, after_modelview, sizeof(modelview)) == 0);
    mc_client_item_renderer *alias = f->renderer;
    mc_client_item_gui_call *second = NULL;
    mc_client_item_gui_error refused;
    CHECK(mc_client_item_renderer_free(&alias) == MC_CLIENT_MODEL_BUSY && alias == f->renderer &&
          mc_client_item_renderer_construct(f->renderer, NULL) == MC_CLIENT_MODEL_BUSY &&
          mc_client_render_classes_free(f->app->render_classes) == MC_CLIENT_MODEL_BUSY &&
          f->app->render_classes->owners == classes_before &&
          mc_client_item_gui_begin(f->renderer, f->graphics, f->app->registry, f->app->display,
                                   NULL, f->app->render_engine, f->stack, 112, 112, &second,
                                   &refused) == MC_CLIENT_ITEM_GUI_BUSY &&
          second == NULL);
    int width, height;
    CHECK(pixels(f, directory, row, &width, &height));
    mc_client_source_draw_state draw_before, draw_after;
    CHECK(mc_client_display_source_draw_inspect(f->app->display, &draw_before));
    static const float untinted[] = {.25f, .5f, .75f};
    for (size_t i = 0; i < 3; ++i)
        CHECK(bits(draw_before.color[i]) == bits(tint ? state.color_rgba[i] : untinted[i]));
    CHECK(mc_client_item_gui_retire(&f->call, &error) == MC_CLIENT_ITEM_GUI_OK && f->call == NULL &&
          mc_client_display_source_draw_inspect(f->app->display, &draw_after) &&
          draw_equal(&draw_before, &draw_after) &&
          mc_client_display_camera_matrices_inspect(f->app->display, after_projection,
                                                    after_modelview) &&
          memcmp(projection, after_projection, sizeof(projection)) == 0 &&
          memcmp(modelview, after_modelview, sizeof(modelview)) == 0 &&
          mc_client_render_engine_classes_inspect(f->graphics, &graphics_after) &&
          mc_client_render_engine_inspect(f->app->render_engine, &engine_after) &&
          graphics_after.native_holds == graphics_before.native_holds &&
          engine_after.native_holds == engine_before.native_holds &&
          f->graphics->scene_lease_count == lease_before && f->renderer->bytes == bytes_before &&
          f->app->render_classes->owners == classes_before && !f->renderer->gui_active &&
          f->renderer->gui_call == NULL && f->app->active_world == NULL &&
          mc_item_stack_fields_get(f->stack, &stack_after) == MC_ITEM_STACK_OK &&
          memcmp(&stack_before, &stack_after, sizeof(stack_before)) == 0);
    printf("FOLIAGE_GUI {\"row\":%zu,\"id\":%d,\"damage\":%d,\"tint\":%s,\"icon\":%d,"
           "\"color\":%d,\"owner\":\"%s\",\"jh\":%s,\"faces\":%zu,\"quad_vertices\":%zu,"
           "\"returned\":true,\"matrices\":true,\"retired\":true,\"aliases_refused\":true,"
           "\"width\":%d,\"height\":%d,\"rgba\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32
           "\",\"%08" PRIx32 "\"]}\n",
           row, state.id, state.metadata, tint ? "true" : "false", state.icon, state.color,
           leaf ? "uy" : "gm", leaf ? "true" : "false", state.block_faces, state.vertices_returned,
           width, height, bits(state.color_rgba[0]), bits(state.color_rgba[1]),
           bits(state.color_rgba[2]), bits(state.color_rgba[3]));
    mc_item_stack_release(f->stack);
    f->stack = NULL;
    return true;
}

static bool components(foliage_fixture *f, const char *assets, const char *directory) {
    const mc_client_application_options options = {
        .assets = assets, .username = "Foliage GUI", .width = 256, .height = 256, .hidden = true};
    CHECK(mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f->app = mc_client_bootstrap_app(f->bootstrap);
    f->graphics = mc_client_bootstrap_graphics(f->bootstrap);
    CHECK(f->app != NULL && f->graphics != NULL && f->app->display != NULL &&
          f->app->render_engine != NULL && f->app->active_world == NULL);
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f->app->settings, &settings) && !settings.values.anaglyph);
    mc_client_frame_state frame;
    CHECK(mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &frame) &&
          frame.frame_window != NULL && frame.canvas_window != NULL &&
          frame.frame_window != frame.canvas_window &&
          mc_client_display_window(f->app->display) == frame.canvas_window);
    f->frame_id = SDL_GetWindowID(frame.frame_window);
    f->canvas_id = SDL_GetWindowID(frame.canvas_window);
    mc_client_model_error model;
    CHECK(f->frame_id != 0 && f->canvas_id != 0 &&
          mc_client_item_renderer_allocate(f->app->render_classes, 0, &f->renderer) ==
              MC_CLIENT_MODEL_OK &&
          mc_client_item_renderer_construct(f->renderer, &model) == MC_CLIENT_MODEL_OK);
    size_t row = 0;
    for (int32_t id = 6; id <= 18; id += 12)
        for (int32_t damage = 0; damage < 4; ++damage)
            for (unsigned int tint = 0; tint < 2; ++tint)
                CHECK(profile(f, directory, row++, id, damage, tint != 0));
    CHECK(row == 16);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "components") != 0) {
        fprintf(stderr, "usage: foliage-gui <assets> components <capture-directory>\n");
        return 2;
    }
    foliage_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    bool ok = components(f, argv[1], argv[3]);
    mc_client_item_gui_error error;
    if (f->call != NULL && mc_client_item_gui_retire(&f->call, &error) != MC_CLIENT_ITEM_GUI_OK)
        ok = false;
    if (f->stack != NULL)
        mc_item_stack_release(f->stack);
    if (f->renderer != NULL && mc_client_item_renderer_free(&f->renderer) != MC_CLIENT_MODEL_OK)
        ok = false;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_OK;
    size_t attempts = 0, busy = 0;
    if (f->bootstrap != NULL)
        do {
            closed = mc_client_bootstrap_close(&f->bootstrap);
            ++attempts;
            if (closed == MC_CLIENT_BOOTSTRAP_BUSY) {
                ++busy;
                SDL_Delay(1);
            }
        } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && attempts < 35000);
    const bool absent =
        SDL_GetWindowFromID(f->frame_id) == NULL && SDL_GetWindowFromID(f->canvas_id) == NULL;
    ok = ok && closed == MC_CLIENT_BOOTSTRAP_OK && f->bootstrap == NULL && f->renderer == NULL &&
         f->call == NULL && absent;
    if (ok)
        printf("FOLIAGE_GUI_FINISH {\"profiles\":16,\"checks\":%u,\"cleanup\":true,"
               "\"windows_absent\":true,\"join_attempts\":%zu,\"join_busy\":%zu}\n",
               checks, attempts, busy);
    free(f);
    return ok ? 0 : 1;
}
