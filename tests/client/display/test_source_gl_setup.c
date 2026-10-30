/* The real Display TU is compiled once. Interceptions below always dispatch
 * their actual native function; no successful GL provider is fabricated.
 * Authored native controls; original parity is a separate qualification. */
#include "../../../src/client/display/display.c"

#include <SDL3/SDL_main.h>
#include <stdio.h>

typedef GLboolean(GLAPIENTRY *is_enabled_fn)(GLenum);
typedef void(GLAPIENTRY *get_double_fn)(GLenum, GLdouble *);

static mc_client_gl native_gl;
static mc_client_display *observed;
static unsigned checks, queries, clears, draws, dispatches[8], dispatch_count;
static bool reenter, nested_busy, change_controls, pending_controls;
static unsigned controls;

static GLenum GLAPIENTRY observe_error(void) {
    const GLenum value = native_gl.GetError();
    ++queries;
    if (pending_controls) {
        _mm_setcsr(controls);
        pending_controls = false;
    }
    return value;
}

static void GLAPIENTRY observe_clear(GLbitfield mask) {
    native_gl.Clear(mask);
    ++clears;
    if (change_controls) {
        _mm_setcsr(controls);
        pending_controls = true;
    }
}

static void GLAPIENTRY observe_draw(GLenum mode, GLint first, GLsizei count) {
    native_gl.DrawArrays(mode, first, count);
    ++draws;
}

static void GLAPIENTRY observe_clear_depth(GLclampd depth) {
    native_gl.ClearDepth(depth);
    if (dispatch_count < sizeof(dispatches) / sizeof(dispatches[0]))
        dispatches[dispatch_count] = 1;
    ++dispatch_count;
    if (reenter) {
        mc_client_display_error error;
        reenter = false;
        nested_busy = !mc_client_display_source_matrix_mode(observed, GL_PROJECTION, &error) &&
                      error.status == MC_CLIENT_DISPLAY_BUSY;
    }
}

static void GLAPIENTRY observe_enable(GLenum cap) {
    native_gl.Enable(cap);
    if (dispatch_count < sizeof(dispatches) / sizeof(dispatches[0]))
        dispatches[dispatch_count] = cap == GL_DEPTH_TEST ? 2u : 0u;
    ++dispatch_count;
}

static void GLAPIENTRY observe_depth(GLenum function) {
    native_gl.DepthFunc(function);
    if (dispatch_count < sizeof(dispatches) / sizeof(dispatches[0]))
        dispatches[dispatch_count] = 3;
    ++dispatch_count;
}

static void GLAPIENTRY observe_cull(GLenum mode) {
    native_gl.CullFace(mode);
    if (dispatch_count < sizeof(dispatches) / sizeof(dispatches[0]))
        dispatches[dispatch_count] = 4;
    ++dispatch_count;
}

#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

#define UNSUPPORTED(call) CHECK(!(call) && error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL)

static float alpha_reference(void) {
    const uint32_t bits = UINT32_C(0x3dcccccd);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool setup(mc_client_display *display, mc_client_display_error *error) {
    return mc_client_display_source_texture_enable(display, true, error) &&
           mc_client_display_source_shade_model(display, GL_SMOOTH, error) &&
           mc_client_display_source_clear_depth(display, 1.0, error) &&
           mc_client_display_source_depth_test(display, true, error) &&
           mc_client_display_source_depth_function(display, GL_LEQUAL, error) &&
           mc_client_display_source_alpha_test(display, true, error) &&
           mc_client_display_source_alpha_function(display, GL_GREATER, alpha_reference(), error) &&
           mc_client_display_source_cull_face(display, GL_BACK, error) &&
           mc_client_display_source_matrix_mode(display, GL_PROJECTION, error) &&
           mc_client_display_source_load_identity(display, error) &&
           mc_client_display_source_matrix_mode(display, GL_MODELVIEW, error);
}

static void triangle(uint8_t bytes[96], bool gradient, uint8_t alpha) {
    const float positions[3][3] = {{4, 4, 0}, {60, 4, 0}, {4, 60, 0}};
    memset(bytes, 0, 96);
    for (size_t vertex = 0; vertex < 3; ++vertex) {
        memcpy(bytes + vertex * 32, positions[vertex], sizeof(positions[vertex]));
        bytes[vertex * 32 + 20 + (gradient ? vertex : 0)] = 255;
        bytes[vertex * 32 + 23] = alpha;
    }
}

static bool pixels_have_rgb(const mc_client_capture *capture, bool mixed) {
    for (int row = 0; row < capture->height; ++row)
        for (int column = 0; column < capture->width; ++column) {
            const uint8_t *pixel =
                capture->pixels + (size_t)row * capture->stride + (size_t)column * 4;
            if (mixed ? (pixel[0] > 16 && pixel[1] > 16 && pixel[2] > 16 && pixel[0] < 240 &&
                         pixel[1] < 240 && pixel[2] < 240)
                      : (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0))
                return true;
        }
    return false;
}

static bool pixels_have_flat_pair(const mc_client_capture *capture, unsigned first_channel,
                                  unsigned second_channel) {
    bool saw_first = false, saw_second = false;
    for (int row = 0; row < capture->height; ++row)
        for (int column = 0; column < capture->width; ++column) {
            const uint8_t *pixel =
                capture->pixels + (size_t)row * capture->stride + (size_t)column * 4u;
            if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 0)
                continue;
            const bool first = column < capture->width / 2;
            const unsigned channel = first ? first_channel : second_channel;
            for (unsigned component = 0; component < 3; ++component)
                if (pixel[component] != (component == channel ? 255 : 0))
                    return false;
            if (pixel[3] != 255)
                return false;
            saw_first = saw_first || first;
            saw_second = saw_second || !first;
        }
    return saw_first && saw_second;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int result = 1;
    const unsigned original_controls = _mm_getcsr();
    mc_client_display *display = NULL;
    mc_client_display_context_lease *lease = NULL;
    mc_client_display_list_range *range = NULL;
    mc_client_capture capture = {0};
    mc_client_display_error error = {0};
    mc_client_startup_projection_stage startup_stage;
    mc_client_loading_projection_stage loading_stage;
    bool have_native_gl = false, fixture_active = false, fixture_retiring = false;
    const mc_client_modelview identity = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    uint8_t bytes[96], snapshot[sizeof(mc_client_source_state)];
    int32_t list = 0;
    _mm_setcsr(0x1f80u);
    CHECK(!mc_client_display_source_shade_model(NULL, GL_SMOOTH, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(!mc_client_display_source_clear_depth(NULL, 1.0, &error));
    CHECK(!mc_client_display_source_depth_test(NULL, true, &error));
    CHECK(!mc_client_display_source_depth_function(NULL, GL_LEQUAL, &error));
    CHECK(!mc_client_display_source_alpha_function(NULL, GL_GREATER, alpha_reference(), &error));
    CHECK(!mc_client_display_source_cull_face(NULL, GL_BACK, &error));
    CHECK(!mc_client_display_source_matrix_mode(NULL, GL_MODELVIEW, &error));
    CHECK(!mc_client_display_source_load_identity(NULL, &error));
    CHECK(mc_client_display_create("selected startup GL controls", 64, 64, true, &display, &error));
    native_gl = display->gl;
    have_native_gl = true;
    observed = display;
    const is_enabled_fn is_enabled = (is_enabled_fn)SDL_GL_GetProcAddress("glIsEnabled");
    const get_double_fn get_double = (get_double_fn)SDL_GL_GetProcAddress("glGetDoublev");
    CHECK(is_enabled != NULL && get_double != NULL && native_gl.CullFace != NULL);
    native_gl.Disable(GL_CULL_FACE);
    CHECK(mc_client_display_source_cull_face(display, GL_BACK, &error));
    CHECK(!is_enabled(GL_CULL_FACE));
    CHECK(mc_client_display_startup_projection(display, 64, 64, &startup_stage, &error));
    CHECK(mc_client_display_startup_alpha(display, &error));
    CHECK(display->source.modelview.column_major[14] == -2000 &&
          display->source.matrix_mode == GL_MODELVIEW);
    const mc_client_modelview logo_modelview = display->source.modelview;
    native_gl.Enable(GL_BLEND);
    native_gl.Enable(GL_SCISSOR_TEST);
    native_gl.Enable(GL_CULL_FACE);
    native_gl.CullFace(GL_FRONT);
    native_gl.ClearDepth(0.25);
    native_gl.DepthFunc(GL_GREATER);
    native_gl.Disable(GL_DEPTH_TEST);
    CHECK(native_gl.GetError() == GL_NO_ERROR);

    display->gl.GetError = observe_error;
    display->gl.Clear = observe_clear;
    display->gl.DrawArrays = observe_draw;
    display->gl.ClearDepth = observe_clear_depth;
    display->gl.Enable = observe_enable;
    display->gl.DepthFunc = observe_depth;
    display->gl.CullFace = observe_cull;
    native_gl.Enable(UINT32_MAX); /* Genuine queued error, never fabricated. */
    reenter = true;
    CHECK(setup(display, &error));
    CHECK(nested_busy && queries == 0 && clears == 0 && draws == 0);
    CHECK(dispatch_count == 4 && dispatches[0] == 1 && dispatches[1] == 2 && dispatches[2] == 3 &&
          dispatches[3] == 4);
    CHECK(memcmp(display->source.projection, identity.column_major, sizeof(identity)) == 0 &&
          memcmp(&display->source.modelview, &logo_modelview, sizeof(logo_modelview)) == 0 &&
          display->source.matrix_mode == GL_MODELVIEW && display->source.depth == 0 &&
          display->source.projection_depth == 0 && display->source.active_calls == 0);
    CHECK(display->source.program == 0 && display->source.gpu_phase == 0 &&
          display->source.texture_enabled && display->source.alpha_test);
    CHECK(is_enabled(GL_BLEND) && is_enabled(GL_SCISSOR_TEST) && is_enabled(GL_CULL_FACE) &&
          is_enabled(GL_DEPTH_TEST));
    GLint integer = 0;
    GLdouble depth = 0;
    native_gl.GetIntegerv(GL_CULL_FACE_MODE, &integer);
    CHECK(integer == GL_BACK);
    native_gl.GetIntegerv(GL_DEPTH_FUNC, &integer);
    CHECK(integer == GL_LEQUAL);
    get_double(GL_DEPTH_CLEAR_VALUE, &depth);
    CHECK(depth == 1.0 && queries == 0);
    CHECK(native_gl.GetError() == GL_INVALID_ENUM && native_gl.GetError() == GL_NO_ERROR);

    /* Copy/enum calls neither inspect nor repair unsupported arithmetic state. */
    _mm_setcsr(0x9fc0u);
    const bool copied =
        mc_client_display_source_shade_model(display, GL_SMOOTH, &error) &&
        mc_client_display_source_alpha_function(display, GL_GREATER, alpha_reference(), &error) &&
        mc_client_display_source_matrix_mode(display, GL_PROJECTION, &error) &&
        mc_client_display_source_load_identity(display, &error);
    const unsigned copied_controls = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    CHECK(copied && (copied_controls & 0xffc0u) == 0x9fc0u);

    /* Distinct matrix stacks and the actual existing rounded translation body. */
    CHECK(mc_client_display_source_push(display, &error));
    CHECK(mc_client_display_source_translate(display, 2, 3, 4, &error));
    CHECK(display->source.projection[12] == 2 && display->source.projection[13] == 3 &&
          display->source.projection[14] == 4 && display->source.projection_depth == 1 &&
          display->source.depth == 0);
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &error));
    CHECK(mc_client_display_source_push(display, &error));
    CHECK(mc_client_display_source_translate(display, 7, 8, 9, &error));
    CHECK(display->source.modelview.column_major[14] == -1991);
    CHECK(mc_client_display_source_pop(display, &error));
    CHECK(memcmp(&display->source.modelview, &logo_modelview, sizeof(logo_modelview)) == 0);
    UNSUPPORTED(mc_client_display_source_pop(display, &error));
    CHECK(display->source.projection_depth == 1 && display->source.depth == 0);
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, &error));
    CHECK(mc_client_display_source_pop(display, &error));
    CHECK(memcmp(display->source.projection, identity.column_major, sizeof(identity)) == 0);

    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &error));
    CHECK(mc_client_display_lists_generate(display, 1, 2, &range, &list, &error));
    CHECK(mc_client_display_lists_begin(display, list, &error));
    _mm_setcsr(0x9fc0u); /* Compilation copies only; the kernel runs at execution. */
    const bool compiled = mc_client_display_source_translate(display, 5, 6, 7, &error);
    _mm_setcsr(0x1f80u);
    CHECK(compiled);
    UNSUPPORTED(mc_client_display_source_matrix_mode(display, GL_PROJECTION, &error));
    UNSUPPORTED(mc_client_display_source_load_identity(display, &error));
    UNSUPPORTED(mc_client_display_source_cull_face(display, GL_BACK, &error));
    CHECK(display->source.matrix_mode == GL_MODELVIEW);
    CHECK(mc_client_display_lists_end(display, &error));
    const size_t commands = display->source.command_bytes;
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, &error));
    CHECK(mc_client_display_lists_call(display, &list, 1, &error));
    CHECK(display->source.projection[12] == 5 && display->source.projection[13] == 6 &&
          display->source.projection[14] == 7 && display->source.command_bytes == commands &&
          memcmp(&display->source.modelview, &logo_modelview, sizeof(logo_modelview)) == 0);
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &error));
    CHECK(mc_client_display_lists_call(display, &list, 1, &error));
    CHECK(display->source.modelview.column_major[12] == 5 &&
          display->source.modelview.column_major[14] == -1993);

    CHECK(mc_client_display_source_shade_model(display, GL_FLAT, &error) &&
          display->source.shade_flat);
    CHECK(mc_client_display_source_shade_model(display, GL_SMOOTH, &error) &&
          !display->source.shade_flat);
    /* Invalid profiles/function admission cannot publish a new state. */
    memcpy(snapshot, &display->source, sizeof(snapshot));
    const unsigned before_dispatch = dispatch_count;
    UNSUPPORTED(mc_client_display_source_shade_model(display, 0, &error));
    UNSUPPORTED(mc_client_display_source_clear_depth(display, 0.5, &error));
    UNSUPPORTED(mc_client_display_source_depth_function(display, GL_LESS, &error));
    UNSUPPORTED(
        mc_client_display_source_alpha_function(display, GL_LESS, alpha_reference(), &error));
    UNSUPPORTED(mc_client_display_source_alpha_function(display, GL_GREATER, 0.0f, &error));
    UNSUPPORTED(mc_client_display_source_cull_face(display, GL_FRONT, &error));
    UNSUPPORTED(mc_client_display_source_matrix_mode(display, GL_TEXTURE, &error));
    display->gl.ClearDepth = NULL;
    UNSUPPORTED(mc_client_display_source_clear_depth(display, 1.0, &error));
    display->gl.ClearDepth = observe_clear_depth;
    display->gl.Enable = NULL;
    UNSUPPORTED(mc_client_display_source_depth_test(display, true, &error));
    display->gl.Enable = observe_enable;
    display->gl.Disable = NULL;
    UNSUPPORTED(mc_client_display_source_depth_test(display, false, &error));
    display->gl.Disable = native_gl.Disable;
    display->gl.DepthFunc = NULL;
    UNSUPPORTED(mc_client_display_source_depth_function(display, GL_LEQUAL, &error));
    display->gl.DepthFunc = observe_depth;
    display->gl.CullFace = NULL;
    UNSUPPORTED(mc_client_display_source_cull_face(display, GL_BACK, &error));
    display->gl.CullFace = observe_cull;
    display->source.active_calls = 1; /* An actual owner admission state, no GL mock. */
    fixture_active = true;
    CHECK(!mc_client_display_source_load_identity(display, &error) &&
          error.status == MC_CLIENT_DISPLAY_BUSY);
    display->source.active_calls = 0;
    fixture_active = false;
    display->native_retiring = true;
    fixture_retiring = true;
    CHECK(!mc_client_display_source_matrix_mode(display, GL_PROJECTION, &error) &&
          error.status == MC_CLIENT_DISPLAY_BUSY);
    display->native_retiring = false;
    fixture_retiring = false;
    CHECK(memcmp(snapshot, &display->source, sizeof(snapshot)) == 0 &&
          dispatch_count == before_dispatch && queries == 0 && clears == 0 && draws == 0);

    /* Real mk102/105 complete before the retained native ortho refusal. */
    const mc_client_modelview before_refusal = display->source.modelview;
    change_controls = true;
    controls = 0x9fc0u;
    const bool projected =
        mc_client_display_loading_projection(display, 128, 96, &loading_stage, &error);
    const unsigned refused_controls = _mm_getcsr();
    change_controls = false;
    pending_controls = false;
    _mm_setcsr(0x1f80u);
    CHECK(!projected && error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP &&
          loading_stage == MC_CLIENT_LOADING_PROJECTION_ORTHO && clears == 1);
    CHECK((refused_controls & 0xffc0u) == 0x9fc0u && display->source.matrix_mode == GL_PROJECTION &&
          !display->source.loading_ready &&
          memcmp(display->source.projection, identity.column_major, sizeof(identity)) == 0 &&
          memcmp(&display->source.modelview, &before_refusal, sizeof(before_refusal)) == 0 &&
          display->exact_width == 64 && display->exact_height == 64 &&
          display->source.active_calls == 0);

    /* Separate authored draw surface after inspecting the setup matrices. */
    display->gl = native_gl;
    native_gl.Disable(GL_BLEND);
    native_gl.Disable(GL_SCISSOR_TEST);
    native_gl.Disable(GL_CULL_FACE);
    native_gl.Disable(GL_MULTISAMPLE);
    native_gl.Disable(GL_FRAMEBUFFER_SRGB);
    native_gl.Viewport(0, 0, 64, 64);
    native_gl.ClearColor(0, 0, 0, 0);
    CHECK(mc_client_display_loading_projection(display, 64, 64, &loading_stage, &error));
    CHECK(display->source.matrix_mode == GL_MODELVIEW);
    CHECK(mc_client_display_source_texture_enable(display, false, &error));
    CHECK(mc_client_display_source_alpha_test(display, true, &error));
    CHECK(mc_client_display_source_alpha_function(display, GL_GREATER, alpha_reference(), &error));
    CHECK(mc_client_display_context_acquire(display, &lease, &error));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, bytes, 0, &error));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_COLOR_ARRAY, bytes, 20, &error));
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true, &error));
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_COLOR_ARRAY, true, &error));
    triangle(bytes, true, 255);
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(bytes), 3, GL_TRIANGLES,
                                         &error));
    CHECK(mc_client_display_capture_rgba(display, &capture, &error));
    CHECK(pixels_have_rgb(&capture, true)); /* Actual interpolated RGB, no flat vertex color. */
    mc_client_capture_free(&capture);
    triangle(bytes, false, 25); /* 25/255 is below real float0.1. */
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(bytes), 3, GL_TRIANGLES,
                                         &error));
    CHECK(mc_client_display_capture_rgba(display, &capture, &error));
    CHECK(!pixels_have_rgb(&capture, false));
    mc_client_capture_free(&capture);
    triangle(bytes, false, 26); /* 26/255 is above real float0.1. */
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(bytes), 3, GL_TRIANGLES,
                                         &error));
    CHECK(mc_client_display_capture_rgba(display, &capture, &error));
    CHECK(pixels_have_rgb(&capture, false));
    mc_client_capture_free(&capture);

    /* Real LG FLAT survives into a later source draw. Each independent
     * triangle must use its own last vertex's color, including gradients. */
    uint8_t flat_bytes[8u * 32u] = {0};
    const float flat_positions[6][3] = {{4, 4, 0},   {28, 4, 0},  {4, 28, 0},
                                        {36, 36, 0}, {60, 36, 0}, {36, 60, 0}};
    for (size_t vertex = 0; vertex < 6; ++vertex) {
        memcpy(flat_bytes + vertex * 32u, flat_positions[vertex], sizeof(flat_positions[vertex]));
        flat_bytes[vertex * 32u + 20u + (vertex < 3 ? 0u : 1u)] = 255;
        flat_bytes[vertex * 32u + 23u] = 255;
    }
    CHECK(mc_client_display_world_list_flat(display, &error) && display->source.shade_flat);
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, flat_bytes, 0,
                                           &error));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_COLOR_ARRAY, flat_bytes, 20,
                                           &error));
    display->gl.DrawArrays = observe_draw;
    const unsigned before_flat = draws;
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_source_batch(lease, flat_bytes, flat_bytes, sizeof(flat_bytes), 6,
                                         GL_TRIANGLES, &error));
    CHECK(draws == before_flat + 1 && display->source.shade_flat);
    CHECK(mc_client_display_capture_rgba(display, &capture, &error));
    CHECK(pixels_have_flat_pair(&capture, 0, 1));
    mc_client_capture_free(&capture);

    CHECK(mc_client_display_lists_retire(&range, &error));
    CHECK(mc_client_display_lists_generate(display, 2, 2, &range, &list, &error));
    CHECK(mc_client_display_lists_begin(display, list, &error));
    CHECK(mc_client_display_source_batch(lease, flat_bytes, flat_bytes, sizeof(flat_bytes), 6,
                                         GL_TRIANGLES, &error));
    CHECK(mc_client_display_lists_end(display, &error));
    for (size_t vertex = 0; vertex < 6; ++vertex) {
        memset(flat_bytes + vertex * 32u + 20u, 0, 3);
        flat_bytes[vertex * 32u + 20u + (vertex + vertex / 3u) % 3u] = 255;
    } /* RGB then GBR: last vertices are blue and red. */
    const int32_t gradient_list = list + 1;
    CHECK(mc_client_display_lists_begin(display, gradient_list, &error));
    CHECK(mc_client_display_source_batch(lease, flat_bytes, flat_bytes, sizeof(flat_bytes), 6,
                                         GL_TRIANGLES, &error));
    CHECK(mc_client_display_lists_end(display, &error));
    native_gl.Enable(UINT32_MAX); /* Rejected native admission must not drain a sticky GL error. */
    UNSUPPORTED(mc_client_display_source_batch(lease, flat_bytes, flat_bytes, sizeof(flat_bytes), 6,
                                               GL_QUADS, &error));
    CHECK(draws == before_flat + 1 && display->source.shade_flat && error.gl_error == 0 &&
          native_gl.GetError() == GL_INVALID_ENUM && native_gl.GetError() == GL_NO_ERROR);
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_source_batch(lease, flat_bytes, flat_bytes, sizeof(flat_bytes), 6,
                                         GL_TRIANGLES, &error));
    CHECK(draws == before_flat + 2 && display->source.shade_flat);
    CHECK(mc_client_display_capture_rgba(display, &capture, &error));
    CHECK(pixels_have_flat_pair(&capture, 2, 0));
    mc_client_capture_free(&capture);
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_lists_call(display, &list, 1, &error));
    CHECK(draws == before_flat + 3 && display->source.shade_flat);
    CHECK(mc_client_display_capture_rgba(display, &capture, &error));
    CHECK(pixels_have_flat_pair(&capture, 0, 1));
    mc_client_capture_free(&capture);
    CHECK(mc_client_display_loading_clear_color_depth(display, &error));
    CHECK(mc_client_display_lists_call(display, &gradient_list, 1, &error));
    CHECK(draws == before_flat + 4 && display->source.shade_flat);
    CHECK(mc_client_display_capture_rgba(display, &capture, &error));
    CHECK(pixels_have_flat_pair(&capture, 2, 0));
    mc_client_capture_free(&capture);
    UNSUPPORTED(mc_client_display_source_batch(lease, flat_bytes, flat_bytes, sizeof(flat_bytes), 6,
                                               GL_QUADS, &error));
    CHECK(draws == before_flat + 4 && display->source.shade_flat);
    for (size_t vertex = 0; vertex < 6; ++vertex) {
        memset(flat_bytes + vertex * 32u + 20u, 0, 3);
        flat_bytes[vertex * 32u + 20u + (vertex < 3 ? 0u : 1u)] = 255;
    }
    flat_bytes[6u * 32u + 20u] = 255;
    flat_bytes[7u * 32u + 21u] = 255;
    CHECK(mc_client_display_source_batch(lease, flat_bytes, flat_bytes, sizeof(flat_bytes), 8,
                                         GL_TRIANGLES, &error));
    CHECK(draws == before_flat + 5 && display->source.shade_flat);
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "Display: %s: %s (status%d, GL%u)\n",
                error.operation == NULL ? "test assertion" : error.operation, error.detail,
                (int)error.status, error.gl_error);
    _mm_setcsr(0x1f80u);
    if (display != NULL && have_native_gl) {
        display->gl = native_gl;
        if (fixture_active)
            display->source.active_calls = 0;
        if (fixture_retiring)
            display->native_retiring = false;
    }
    mc_client_capture_free(&capture);
    if (!mc_client_display_lists_retire(&range, &error))
        result = 1;
    if (!mc_client_display_context_release(&lease, &error))
        result = 1;
    if (!mc_client_display_destroy(display, &error))
        result = 1;
    SDL_Quit(); /* Native test-process cleanup, never a source startup call. */
    _mm_setcsr(original_controls);
    if (result == 0 && printf("Source startup GL setup: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
