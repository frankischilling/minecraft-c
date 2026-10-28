/* White-box native-policy controls using the exact production Display body.
 * Compile this TU once with its real SDL/dependency closure. Every replaced
 * callback forwards to the actual GL function before changing MXCSR; no fake
 * Display, successful GL acknowledgment or original-GL equality is supplied.
 * Source-only authored test: NOT compiled or executed in this package. */
#include "../../../src/client/display/display.c"

#include <SDL3/SDL_main.h>
#include <stdio.h>

static unsigned checks, clear_calls, query_calls, draw_calls;
static unsigned callback_controls;
static bool pending_controls;
static mc_client_gl_clear_fn real_clear;
static mc_client_gl_get_integer_fn real_integer;
static mc_client_gl_draw_arrays_fn real_draw;
static mc_client_gl_get_error_fn real_error;

static GLenum GLAPIENTRY error_then_controls(void) {
    const GLenum value = real_error();
    /* Keep the mutation after the last actual GL callback too: the test
     * cannot assume a driver's intervening GetError preserves MXCSR. */
    if (pending_controls) {
        _mm_setcsr(callback_controls);
        pending_controls = false;
    }
    return value;
}

static void GLAPIENTRY clear_then_controls(GLbitfield mask) {
    real_clear(mask);
    ++clear_calls;
    _mm_setcsr(callback_controls);
    pending_controls = true;
}

static void GLAPIENTRY query_then_controls(GLenum name, GLint *value) {
    real_integer(name, value);
    if (name == GL_ACTIVE_TEXTURE) {
        ++query_calls;
        _mm_setcsr(callback_controls);
        pending_controls = true;
    }
}

static void GLAPIENTRY draw_then_controls(GLenum mode, GLint first, GLsizei count) {
    real_draw(mode, first, count);
    ++draw_calls;
    _mm_setcsr(callback_controls);
    pending_controls = true;
}

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "source FP check %u failed at line %d: %s\n", checks, __LINE__,        \
                    #condition);                                                                   \
            result = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static void triangle_bytes(uint8_t bytes[96]) {
    const float positions[3][3] = {{4, 4, 0}, {20, 4, 0}, {4, 20, 0}};
    memset(bytes, 0, 96);
    for (size_t vertex = 0; vertex < 3; ++vertex) {
        memcpy(bytes + vertex * 32, positions[vertex], sizeof positions[vertex]);
        bytes[vertex * 32 + 20] = 255;
        bytes[vertex * 32 + 23] = 255;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int result = 0;
    const unsigned initial_controls = _mm_getcsr();
    mc_client_display *display = NULL;
    mc_client_display_context_lease *lease = NULL;
    mc_client_display_list_range *range = NULL;
    mc_client_capture capture = {0};
    mc_client_display_error error = {0};
    mc_client_loading_projection_stage stage = MC_CLIENT_LOADING_PROJECTION_DEPTH_CLEAR;
    uint8_t bytes[96];
    triangle_bytes(bytes);
    _mm_setcsr(0x1f80u);
    CHECK(mc_client_display_create("source FP boundary controls", 64, 64, true, &display, &error));
    real_clear = display->gl.Clear;
    real_integer = display->gl.GetIntegerv;
    real_draw = display->gl.DrawArrays;
    real_error = display->gl.GetError;
    display->gl.GetError = error_then_controls;
    CHECK(mc_client_display_loading_projection(display, 64, 64, &stage, &error));
    float projection[16];
    memcpy(projection, display->source.projection, sizeof projection);
    const mc_client_modelview modelview = display->source.modelview;

    /* The real clear has happened; ortho arithmetic and publication have not.
     * Restore controls only in this test after observing the failed call. */
    callback_controls = 0x9fc0u;
    display->gl.Clear = clear_then_controls;
    const bool projected = mc_client_display_loading_projection(display, 128, 96, &stage, &error);
    const unsigned clear_controls = _mm_getcsr();
    display->gl.Clear = real_clear;
    _mm_setcsr(0x1f80u);
    CHECK(!projected && error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP && error.gl_error == 0);
    CHECK(clear_calls == 1 && stage == MC_CLIENT_LOADING_PROJECTION_ORTHO &&
          !display->source.loading_ready);
    CHECK((clear_controls & 0xffc0u) == (callback_controls & 0xffc0u));
    /* Actual mk102/105 select PROJECTION and load identity before glOrtho126. */
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    CHECK(memcmp(identity, display->source.projection, sizeof identity) == 0 &&
          memcmp(projection, display->source.projection, sizeof projection) != 0 &&
          display->source.matrix_mode == GL_PROJECTION &&
          memcmp(&modelview, &display->source.modelview, sizeof modelview) == 0);
    CHECK(display->exact_width == 64 && display->exact_height == 64 &&
          display->source.active_calls == 0);

    /* Exception status flags alone are admitted after the same real clear. */
    callback_controls = 0x1fbfu;
    display->gl.Clear = clear_then_controls;
    const bool sticky_projection =
        mc_client_display_loading_projection(display, 64, 64, &stage, &error);
    const unsigned sticky_controls = _mm_getcsr();
    display->gl.Clear = real_clear;
    _mm_setcsr(0x1f80u);
    CHECK(sticky_projection && error.status == MC_CLIENT_DISPLAY_OK &&
          display->source.loading_ready);
    CHECK(clear_calls == 2 && (sticky_controls & 0xffc0u) == 0x1f80u);

    CHECK(mc_client_display_context_acquire(display, &lease, &error));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, bytes, 0, &error));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_COLOR_ARRAY, bytes, 20, &error));
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true, &error));
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_COLOR_ARRAY, true, &error));

    /* Actual GetIntegerv precedes conversion. No lazy GPU allocation, upload
     * or draw may run when that callback changes unsupported control bits. */
    callback_controls = 0x9fc0u;
    display->gl.GetIntegerv = query_then_controls;
    const bool converted =
        mc_client_display_source_batch(lease, bytes, bytes, sizeof bytes, 3, GL_TRIANGLES, &error);
    const unsigned query_controls = _mm_getcsr();
    display->gl.GetIntegerv = real_integer;
    _mm_setcsr(0x1f80u);
    CHECK(!converted && error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP && error.gl_error == 0);
    CHECK(query_calls == 1 && (query_controls & 0xffc0u) == (callback_controls & 0xffc0u));
    CHECK(display->source.gpu_phase == 0 && display->source.program == 0 &&
          display->source.vbo == 0 && display->source.active_calls == 0);

    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    int32_t id = 0;
    CHECK(mc_client_display_lists_generate(display, 1, 4, &range, &id, &error));
    CHECK(mc_client_display_lists_begin(display, id, &error));
    CHECK(
        mc_client_display_source_batch(lease, bytes, bytes, sizeof bytes, 3, GL_TRIANGLES, &error));
    /* Compilation copies this command; there is no matrix arithmetic yet. */
    _mm_setcsr(0x9fc0u);
    const bool compiled_translation = mc_client_display_source_translate(display, 2, 3, 0, &error);
    _mm_setcsr(0x1f80u);
    CHECK(compiled_translation);
    CHECK(mc_client_display_lists_end(display, &error));
    const size_t commands_before = display->source.command_bytes;
    const mc_client_modelview before_draw = display->source.modelview;
    display->gl.DrawArrays = draw_then_controls;
    const bool replayed = mc_client_display_lists_call(display, &id, 1, &error);
    const unsigned draw_controls = _mm_getcsr();
    display->gl.DrawArrays = real_draw;
    _mm_setcsr(0x1f80u);
    CHECK(!replayed && error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP && error.gl_error == 0);
    CHECK(draw_calls == 1 && (draw_controls & 0xffc0u) == (callback_controls & 0xffc0u));
    CHECK(display->frame_started && !display->source.color_known &&
          display->source.active_calls == 0);
    CHECK(display->source.command_bytes == commands_before && range->lists[0].defined);
    CHECK(memcmp(&before_draw, &display->source.modelview, sizeof before_draw) == 0);
    CHECK(mc_client_display_capture_rgba(display, &capture, &error));
    bool red_pixel = false;
    for (size_t row = 0; row < (size_t)capture.height; ++row)
        for (size_t column = 0; column < (size_t)capture.width; ++column)
            if (capture.pixels[row * capture.stride + column * 4] != 0)
                red_pixel = true;
    CHECK(red_pixel); /* Actual draw/readback completed before translation failed. */

    /* Direct execution likewise rejects stale callback controls before any
     * matrix write. The current owned context is already current here. */
    _mm_setcsr(0x9fc0u);
    const bool translated = mc_client_display_source_translate(display, 4, 5, 0, &error);
    const unsigned direct_controls = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    CHECK(!translated && error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP &&
          (direct_controls & 0xffc0u) == 0x9fc0u && display->source.active_calls == 0);
    CHECK(memcmp(&before_draw, &display->source.modelview, sizeof before_draw) == 0);

    /* A real draw that changes sticky flags only permits the later command. */
    callback_controls = 0x1fbfu;
    display->gl.DrawArrays = draw_then_controls;
    const bool sticky_replayed = mc_client_display_lists_call(display, &id, 1, &error);
    const unsigned sticky_draw_controls = _mm_getcsr();
    display->gl.DrawArrays = real_draw;
    _mm_setcsr(0x1f80u);
    CHECK(sticky_replayed && error.status == MC_CLIENT_DISPLAY_OK && draw_calls == 2);
    CHECK((sticky_draw_controls & 0xffc0u) == 0x1f80u &&
          display->source.modelview.column_major[12] == 2 &&
          display->source.modelview.column_major[13] == 3);
cleanup:
    _mm_setcsr(0x1f80u);
    if (display != NULL) {
        display->gl.Clear = real_clear;
        display->gl.GetIntegerv = real_integer;
        display->gl.DrawArrays = real_draw;
        display->gl.GetError = real_error;
    }
    mc_client_capture_free(&capture);
    if (!mc_client_display_lists_retire(&range, &error))
        result = 1;
    if (!mc_client_display_context_release(&lease, &error))
        result = 1;
    if (!mc_client_display_destroy(display, &error))
        result = 1;
    _mm_setcsr(initial_controls);
    if (result == 0 && printf("Source resources FP boundaries: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
