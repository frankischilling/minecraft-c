/* Actual Display implementation and native context, with the canonical NW
 * owner linked below. Authored controls qualify this selected GL3.3 bridge. */
#include "../../../src/client/display/display.c"

#include "client/render_engine/bounds_draw.h"
#include <SDL3/SDL_main.h>
#include <stdio.h>

static unsigned checks, queries;
static GLenum(GLAPIENTRY *native_get_error)(void);

static GLenum GLAPIENTRY observe_error(void) {
    ++queries;
    return native_get_error();
}

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static void triangle(uint8_t bytes[96], unsigned component) {
    const float positions[3][3] = {{4, 4, 0}, {60, 4, 0}, {4, 60, 0}};
    memset(bytes, 0, 96);
    for (size_t vertex = 0; vertex < 3; ++vertex) {
        memcpy(bytes + vertex * 32, positions[vertex], sizeof(positions[vertex]));
        bytes[vertex * 32 + 20 + component] = 255;
        bytes[vertex * 32 + 23] = 255;
    }
}

static bool color_pixels(const mc_client_capture *capture, int component) {
    for (int y = 0; y < capture->height; ++y)
        for (int x = 0; x < capture->width; ++x) {
            const uint8_t *pixel = capture->pixels + (size_t)y * capture->stride + (size_t)x * 4;
            if (component < 0 ? pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0
                              : pixel[component] > 240 && pixel[(component + 1) % 3] == 0 &&
                                    pixel[(component + 2) % 3] == 0)
                return true;
        }
    return false;
}

static bool triangle_arrays(mc_client_display *display, mc_client_display_context_lease *lease,
                            uint8_t *bytes, mc_client_display_error *error) {
    return mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, bytes, 0,
                                            error) &&
           mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_COLOR_ARRAY, bytes, 20,
                                            error) &&
           mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true, error) &&
           mc_client_display_source_array(display, MC_CLIENT_SOURCE_COLOR_ARRAY, true, error);
}

static bool bbox_vertices(const mc_client_source_batch_view *batch) {
    /* Six source quads in BW order, decoded through NW's real b=true profile. */
    static const float corners[24][3] = {
        {-6, 22, -6}, {22, 22, -6}, {22, -6, -6}, {-6, -6, -6}, {-6, -6, 22}, {22, -6, 22},
        {22, 22, 22}, {-6, 22, 22}, {-6, -6, -6}, {22, -6, -6}, {22, -6, 22}, {-6, -6, 22},
        {-6, 22, 22}, {22, 22, 22}, {22, 22, -6}, {-6, 22, -6}, {-6, -6, 22}, {-6, 22, 22},
        {-6, 22, -6}, {-6, -6, -6}, {22, -6, -6}, {22, 22, -6}, {22, 22, 22}, {22, -6, 22}};
    static const size_t decoded[6] = {0, 1, 2, 0, 2, 3};
    if (batch->vertices != 36 || batch->mode != GL_TRIANGLES || batch->byte_count != 1152 ||
        batch->arrays != (1u << MC_CLIENT_SOURCE_VERTEX_ARRAY))
        return false;
    for (size_t quad = 0; quad < 6; ++quad)
        for (size_t vertex = 0; vertex < 6; ++vertex)
            if (memcmp(batch->bytes + (quad * 6 + vertex) * 32, corners[quad * 4 + decoded[vertex]],
                       sizeof(corners[0])) != 0)
                return false;
    return true;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int result = 1;
    const unsigned original_controls = _mm_getcsr();
    mc_client_display *display = NULL;
    mc_client_display_context_lease *lease = NULL;
    mc_client_display_list_range *range = NULL, *limited = NULL, *large = NULL,
                                 *byte_limited = NULL;
    mc_client_render_engine_classes *classes = NULL;
    mc_client_display_error error = {0};
    mc_client_graphics_error graphics = {0};
    mc_client_capture capture = {0};
    mc_client_loading_projection_stage stage;
    mc_client_source_list_state state;
    mc_client_source_batch_view committed, pending;
    mc_client_nw_state nw;
    uint8_t bytes[96], red[96];
    uint8_t *large_bytes = NULL;
    int32_t id = 0, limited_id = 0, large_id = 0, byte_id = 0;
    uint32_t sticky = 0;
    _mm_setcsr(0x1f80u);
    CHECK(mc_client_display_create("source list replacement controls", 64, 64, true, &display,
                                   &error));
    native_get_error = display->gl.GetError;
    display->gl.GetError = observe_error;
    display->gl.Disable(GL_CULL_FACE);
    display->gl.Disable(GL_SCISSOR_TEST);
    display->gl.Disable(GL_BLEND);
    display->gl.Disable(GL_DEPTH_TEST);
    display->gl.Viewport(0, 0, 64, 64);
    display->gl.ClearColor(0, 0, 0, 0);
    CHECK(mc_client_display_loading_projection(display, 64, 64, &stage, &error));
    CHECK(mc_client_display_source_texture_enable(display, false, &error));
    CHECK(mc_client_display_context_acquire(display, &lease, &error));
    CHECK(triangle_arrays(display, lease, bytes, &error));
    CHECK(mc_client_display_lists_generate(display, 1, 8, &range, &id, &error));
    triangle(bytes, 0);
    memcpy(red, bytes, sizeof(red));
    const unsigned before_queries = queries;
    display->gl.Enable((GLenum)UINT32_MAX); /* Genuine sticky GL error. */
    CHECK(mc_client_display_lists_begin(display, id, &error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(bytes), 3, GL_TRIANGLES,
                                         &error));
    CHECK(mc_client_display_list_inspect(display, id, &state) && !state.defined && state.building &&
          state.commands == 0 && state.pending_commands == 1 &&
          state.pending_bytes > sizeof(bytes));
    CHECK(!mc_client_display_list_batch(display, id, 0, &committed));
    CHECK(mc_client_display_list_pending_batch(display, id, 0, &pending) &&
          memcmp(pending.bytes, red, sizeof(red)) == 0);
    CHECK(mc_client_display_lists_end(display, &error));
    CHECK(mc_client_display_list_batch(display, id, 0, &committed));
    const uint8_t *old_bytes = committed.bytes;
    const size_t old_charge = display->source.command_bytes;
    triangle(bytes, 2);
    CHECK(mc_client_display_lists_begin(display, id, &error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(bytes), 3, GL_TRIANGLES,
                                         &error));
    CHECK(mc_client_display_list_inspect(display, id, &state) && state.defined && state.building &&
          state.commands == 1 && state.pending_commands == 1 &&
          display->source.command_bytes > old_charge);
    CHECK(mc_client_display_list_batch(display, id, 0, &committed) &&
          committed.bytes == old_bytes && memcmp(committed.bytes, red, sizeof(red)) == 0);
    CHECK(mc_client_display_list_pending_batch(display, id, 0, &pending) &&
          memcmp(pending.bytes, bytes, sizeof(bytes)) == 0);
    CHECK(!mc_client_display_lists_begin(display, id, &error) &&
          error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
    CHECK(mc_client_display_lists_end(display, &error));
    CHECK(mc_client_display_list_inspect(display, id, &state) && state.defined && !state.building &&
          state.commands == 1 && state.pending_commands == 0 && state.pending_bytes == 0 &&
          display->source.command_bytes == old_charge && range->command_count == 1);
    CHECK(mc_client_display_list_batch(display, id, 0, &committed) &&
          memcmp(committed.bytes, bytes, sizeof(bytes)) == 0);
    CHECK(queries == before_queries);
    CHECK(mc_client_display_source_get_error(display, &sticky, &error) &&
          sticky == GL_INVALID_ENUM);
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_lists_call(display, &id, 1, &error));
    CHECK(mc_client_display_capture_rgba(display, &capture, &error) && color_pixels(&capture, 2) &&
          !color_pixels(&capture, 0));
    mc_client_capture_free(&capture);

    CHECK(mc_client_display_lists_begin(display, id, &error));
    CHECK(mc_client_display_lists_end(display, &error));
    CHECK(mc_client_display_list_inspect(display, id, &state) && state.defined && !state.building &&
          state.commands == 0 && state.draws == 0 && state.vertices == 0 &&
          state.pending_commands == 0 && state.pending_bytes == 0 &&
          display->source.command_bytes == 0 && range->command_count == 0);
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_lists_call(display, &id, 1, &error));
    CHECK(mc_client_display_capture_rgba(display, &capture, &error) && !color_pixels(&capture, -1));
    mc_client_capture_free(&capture);

    CHECK(mc_client_display_lists_generate(display, 1, 2, &limited, &limited_id, &error));
    triangle(bytes, 0);
    CHECK(mc_client_display_lists_begin(display, limited_id, &error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(bytes), 3, GL_TRIANGLES,
                                         &error));
    CHECK(mc_client_display_lists_end(display, &error));
    triangle(bytes, 2);
    CHECK(mc_client_display_lists_begin(display, limited_id, &error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(bytes), 3, GL_TRIANGLES,
                                         &error));
    CHECK(!mc_client_display_source_color(display, 0, 1, 0, 1, &error) &&
          error.status == MC_CLIENT_DISPLAY_LIMIT);
    CHECK(!mc_client_display_lists_abort(display, id, &error) &&
          error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(mc_client_display_lists_abort(display, limited_id, &error));
    const size_t interrupted_bytes = display->source.command_bytes;
    CHECK(mc_client_display_list_inspect(display, limited_id, &state) && state.defined &&
          state.building && state.commands == 1 && state.pending_commands == 1 &&
          limited->command_count == 2);
    CHECK(mc_client_display_list_batch(display, limited_id, 0, &committed) &&
          memcmp(committed.bytes, red, sizeof(red)) == 0);
    CHECK(mc_client_display_list_pending_batch(display, limited_id, 0, &pending) &&
          memcmp(pending.bytes, bytes, sizeof(bytes)) == 0);
    CHECK(!mc_client_display_lists_begin(display, limited_id, &error) &&
          error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL &&
          display->source.command_bytes == interrupted_bytes);
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_lists_call(display, &limited_id, 1, &error));
    CHECK(mc_client_display_capture_rgba(display, &capture, &error) && color_pixels(&capture, 0) &&
          !color_pixels(&capture, 2));
    mc_client_capture_free(&capture);
    CHECK(mc_client_display_lists_retire(&limited, &error) && limited == NULL &&
          display->source.command_bytes == 0);

    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_COLOR_ARRAY, false, &error));
    CHECK(mc_client_display_lists_generate(display, 786432, 65536, &large, &large_id, &error));
    const mc_entity_bounds box = {-6, -6, -6, 22, 22, 22};
    const unsigned bbox_queries = queries;
    display->gl.Enable((GLenum)UINT32_MAX);
    for (int32_t index = 0; index < 5408; ++index) {
        CHECK(mc_client_display_lists_begin(display, large_id + index * 3 + 2, &error));
        CHECK(mc_client_render_bounds_draw(classes, display, &box, &graphics));
        CHECK(mc_client_display_lists_end(display, &error));
    }
    CHECK(mc_client_display_list_batch(display, large_id + 2, 0, &committed) &&
          bbox_vertices(&committed));
    const size_t bbox_charge = display->source.command_bytes;
    CHECK(large->command_count == 5408 && bbox_charge < SOURCE_COMMAND_BYTES &&
          queries == bbox_queries);
    for (int32_t index = 0; index < 5408; ++index) {
        CHECK(mc_client_display_lists_begin(display, large_id + index * 3 + 2, &error));
        CHECK(mc_client_render_bounds_draw(classes, display, &box, &graphics));
        CHECK(mc_client_display_lists_end(display, &error));
    }
    CHECK(large->command_count == 5408 && display->source.command_bytes == bbox_charge &&
          queries == bbox_queries);
    CHECK(mc_client_display_source_get_error(display, &sticky, &error) &&
          sticky == GL_INVALID_ENUM);
    CHECK(mc_client_nw_inspect(classes, &nw) && nw.static_b && !nw.fields.w && nw.fields.h == 0 &&
          nw.fields.o == 0 && nw.fields.p == 0);

    /* A real native byte quota with simultaneously owned old/new commands.
     * The old definition fits; an equal replacement cannot fit beside it. */
    const int32_t large_vertices = (16 * 1024 * 1024 / 32 / 3) * 3;
    const size_t large_size = (size_t)large_vertices * 32;
    large_bytes = calloc(1, large_size);
    CHECK(large_bytes != NULL);
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, large_bytes, 0,
                                           &error));
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true, &error));
    CHECK(mc_client_display_lists_generate(display, 1, 8, &byte_limited, &byte_id, &error));
    CHECK(mc_client_display_lists_begin(display, byte_id, &error));
    CHECK(mc_client_display_source_batch(lease, large_bytes, large_bytes, large_size,
                                         large_vertices, GL_TRIANGLES, &error));
    CHECK(mc_client_display_lists_end(display, &error));
    CHECK(mc_client_display_list_batch(display, byte_id, 0, &committed));
    old_bytes = committed.bytes;
    CHECK(mc_client_display_lists_begin(display, byte_id, &error));
    CHECK(!mc_client_display_source_batch(lease, large_bytes, large_bytes, large_size,
                                          large_vertices, GL_TRIANGLES, &error) &&
          error.status == MC_CLIENT_DISPLAY_LIMIT);
    CHECK(mc_client_display_list_inspect(display, byte_id, &state) && state.defined &&
          state.building && state.commands == 1 && state.pending_commands == 0 &&
          state.pending_bytes > 0);
    CHECK(mc_client_display_lists_abort(display, byte_id, &error));
    CHECK(mc_client_display_list_batch(display, byte_id, 0, &committed) &&
          committed.bytes == old_bytes && committed.byte_count == large_size &&
          byte_limited->command_count == 1);
    CHECK(mc_client_display_lists_retire(&byte_limited, &error) &&
          display->source.command_bytes == bbox_charge);

    CHECK(!mc_client_render_bounds_draw(classes, display, NULL, &graphics) &&
          graphics.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE && !graphics.native_policy &&
          strcmp(graphics.site.class_name, "bw") == 0 && graphics.site.pc == 10 &&
          mc_client_nw_inspect(classes, &nw) && nw.fields.w && nw.fields.h == 0 &&
          nw.fields.r == 7);
    CHECK(!mc_client_render_bounds_draw(classes, display, &box, &graphics) &&
          graphics.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE &&
          strcmp(graphics.site.class_name, "nw") == 0 && graphics.site.pc == 16 &&
          strcmp(graphics.parent.class_name, "bw") == 0 && graphics.parent.pc == 5);
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "Display %d: %s; graphics %d: %s\n", (int)error.status,
                error.operation == NULL ? "none" : error.operation, (int)graphics.status,
                graphics.operation == NULL ? "none" : graphics.operation);
    _mm_setcsr(0x1f80u);
    mc_client_capture_free(&capture);
    free(large_bytes);
    if (classes != NULL &&
        mc_client_render_engine_classes_release(classes) != MC_CLIENT_RENDER_ENGINE_OK)
        result = 1;
    if (!mc_client_display_lists_retire(&byte_limited, &error))
        result = 1;
    if (!mc_client_display_lists_retire(&large, &error))
        result = 1;
    if (!mc_client_display_lists_retire(&limited, &error))
        result = 1;
    if (!mc_client_display_lists_retire(&range, &error))
        result = 1;
    if (!mc_client_display_context_release(&lease, &error))
        result = 1;
    if (display != NULL) {
        if (display->source.command_bytes != 0)
            result = 1;
        if (native_get_error != NULL)
            display->gl.GetError = native_get_error;
    }
    if (!mc_client_display_destroy(display, &error))
        result = 1;
    SDL_Quit();
    _mm_setcsr(original_controls);
    if (result == 0 && printf("Source list replacement and BW bounds: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
