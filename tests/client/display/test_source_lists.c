/* Actual Display implementation and native context, with the canonical NW
 * owner linked below. Authored controls qualify this selected GL3.3 bridge. */
#include "../../../src/client/display/display.c"

#include "client/render_engine/bounds_draw.h"
#include "client/render_engine/context_internal.h"
#include <SDL3/SDL_main.h>
#include <stdio.h>

static unsigned checks, queries;
static GLenum(GLAPIENTRY *native_get_error)(void);

enum {
    REPLAY_PROGRAM,
    REPLAY_VAO,
    REPLAY_BUFFER,
    REPLAY_DRAW,
    REPLAY_PROJECTION,
    REPLAY_MODELVIEW,
    REPLAY_PROVOKING,
    REPLAY_TEXTURED,
    REPLAY_ALPHA,
    REPLAY_FOG,
    REPLAY_LIGHTING,
    REPLAY_FLAT,
    REPLAY_COUNTERS
};

static mc_client_gl replay_native;
static mc_client_display *replay_display;
static unsigned replay_calls[REPLAY_COUNTERS];
static GLuint replay_names[3][4];

static void replay_name(unsigned int kind, GLuint name) {
    if (replay_calls[kind] < 4)
        replay_names[kind][replay_calls[kind]] = name;
    ++replay_calls[kind];
}

static void GLAPIENTRY replay_program(GLuint program) {
    replay_native.UseProgram(program);
    replay_name(REPLAY_PROGRAM, program);
}

static void GLAPIENTRY replay_vao(GLuint vao) {
    replay_native.BindVertexArray(vao);
    replay_name(REPLAY_VAO, vao);
}

static void GLAPIENTRY replay_buffer(GLenum target, GLuint buffer) {
    replay_native.BindBuffer(target, buffer);
    if (target == GL_ARRAY_BUFFER)
        replay_name(REPLAY_BUFFER, buffer);
}

static void GLAPIENTRY replay_draw(GLenum mode, GLint first, GLsizei count) {
    replay_native.DrawArrays(mode, first, count);
    ++replay_calls[REPLAY_DRAW];
}

static void GLAPIENTRY replay_integer(GLint location, GLint value) {
    replay_native.Uniform1i(location, value);
    const mc_client_source_state *s = &replay_display->source;
    const GLint locations[] = {s->textured_uniform, s->alpha_uniform, s->fog_enabled_uniform,
                               s->lighting_uniform, s->flat_uniform};
    for (size_t i = 0; i < sizeof(locations) / sizeof(*locations); ++i)
        if (location == locations[i])
            ++replay_calls[REPLAY_TEXTURED + i];
}

static void GLAPIENTRY replay_matrix(GLint location, GLsizei count, GLboolean transpose,
                                     const GLfloat *values) {
    replay_native.UniformMatrix4fv(location, count, transpose, values);
    if (location == replay_display->source.projection_uniform)
        ++replay_calls[REPLAY_PROJECTION];
    if (location == replay_display->source.modelview_uniform)
        ++replay_calls[REPLAY_MODELVIEW];
}

static void GLAPIENTRY replay_provoking(GLenum convention) {
    replay_native.ProvokingVertex(convention);
    ++replay_calls[REPLAY_PROVOKING];
}

static bool replay_payload_counts(unsigned int draws, unsigned int projections,
                                  unsigned int modelviews) {
    if (replay_calls[REPLAY_PROGRAM] != 1 || replay_calls[REPLAY_DRAW] != draws ||
        replay_calls[REPLAY_PROJECTION] != projections ||
        replay_calls[REPLAY_MODELVIEW] != modelviews || replay_calls[REPLAY_PROVOKING] != 1)
        return false;
    for (unsigned int kind = REPLAY_TEXTURED; kind < REPLAY_COUNTERS; ++kind)
        if (replay_calls[kind] != 1)
            return false;
    return true;
}

static GLenum GLAPIENTRY observe_error(void) {
    ++queries;
    return native_get_error();
}

/* Observe the completed Color command without adding a query that would
 * change Mesa's pending attributes during the subsequent stack controls. */
static bool retained_color_setter(const mc_client_display *display, const float rgba[4]) {
    const mc_client_source_state *s = &display->source;
    if (source_material_is_staged(s))
        return (s->attribute_staging.enabled & (UINT32_C(1) << SOURCE_STAGE_COLOR)) != 0 &&
               s->attribute_staging.color_known &&
               memcmp(s->attribute_staging.color, rgba, sizeof(s->color)) == 0;
    return s->color_known && memcmp(s->color, rgba, sizeof(s->color)) == 0;
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

static bool chunk_matrix_prefix(mc_client_display *display, mc_client_display_error *error) {
    /* Actual DK.a264..332 for a 16-cube at its local origin. */
    return mc_client_display_source_push(display, error) &&
           mc_client_display_source_translate(display, -8, -8, -8, error) &&
           mc_client_display_source_scale(display, (double)1.000001f, (double)1.000001f,
                                          (double)1.000001f, error) &&
           mc_client_display_source_translate(display, 8, 8, 8, error);
}

static bool matrix_controls(mc_client_display *display, mc_client_display_context_lease *lease,
                            uint8_t bytes[96], mc_client_display_error *error) {
    bool result = false;
    mc_client_display_list_range *range = NULL, *limited = NULL;
    int32_t base = 0, limited_id = 0;
    mc_client_capture immediate = {0}, compiled = {0};
    mc_client_source_list_state list;
    const mc_client_modelview before = display->source.modelview;
    float projection[16];
    memcpy(projection, display->source.projection, sizeof(projection));
    const unsigned before_queries = queries;
    CHECK(display->source.depth == 0 && display->source.projection_depth == 0);
    CHECK(mc_client_display_lists_generate(display, 5, 64, &range, &base, error));
    CHECK(mc_client_display_lists_begin(display, base, error));
    CHECK(chunk_matrix_prefix(display, error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, 96, 3, GL_TRIANGLES, error));
    CHECK(mc_client_display_source_pop(display, error));
    CHECK(mc_client_display_lists_end(display, error));
    CHECK(mc_client_display_list_inspect(display, base, &list) && list.commands == 6 &&
          list.draws == 1 && list.vertices == 3);
    CHECK(display->source.depth == 0 && display->source.projection_depth == 0 &&
          memcmp(&display->source.modelview, &before, sizeof(before)) == 0 &&
          memcmp(display->source.projection, projection, sizeof(projection)) == 0 &&
          queries == before_queries);
    CHECK(mc_client_display_loading_clear_color_depth(display, error));
    CHECK(chunk_matrix_prefix(display, error));
    const mc_client_modelview transformed = display->source.modelview;
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, 96, 3, GL_TRIANGLES, error));
    CHECK(mc_client_display_source_pop(display, error));
    CHECK(mc_client_display_capture_rgba(display, &immediate, error));
    CHECK(mc_client_display_loading_clear_color_depth(display, error));
    CHECK(mc_client_display_lists_call(display, &base, 1, error));
    CHECK(mc_client_display_capture_rgba(display, &compiled, error));
    CHECK(immediate.width == compiled.width && immediate.height == compiled.height &&
          immediate.stride == compiled.stride &&
          memcmp(immediate.pixels, compiled.pixels, immediate.stride * (size_t)immediate.height) ==
              0 &&
          color_pixels(&compiled, 0) && display->source.depth == 0 &&
          memcmp(&display->source.modelview, &before, sizeof(before)) == 0);

    /* Compilation does not evaluate its current stack, selected matrix or FP
     * controls. The selected matrix and reached stack errors belong to replay. */
    int32_t prefix_id = base + 1, pop_id = base + 2, underflow_id = base + 3, scale_id = base + 4;
    CHECK(mc_client_display_lists_begin(display, prefix_id, error));
    _mm_setcsr(0x9fc0u);
    const bool recorded = chunk_matrix_prefix(display, error);
    _mm_setcsr(0x1f80u);
    CHECK(recorded && mc_client_display_lists_end(display, error));
    CHECK(mc_client_display_lists_begin(display, pop_id, error));
    CHECK(mc_client_display_source_pop(display, error));
    CHECK(mc_client_display_lists_end(display, error));
    CHECK(mc_client_display_lists_begin(display, scale_id, error));
    CHECK(mc_client_display_source_push(display, error));
    CHECK(mc_client_display_source_scale(display, 2, 3, 4, error));
    CHECK(mc_client_display_lists_end(display, error));
    CHECK(mc_client_display_lists_call(display, &prefix_id, 1, error) &&
          display->source.depth == 1 &&
          memcmp(&display->source.modelview, &transformed, sizeof(transformed)) == 0);
    CHECK(mc_client_display_lists_call(display, &pop_id, 1, error) && display->source.depth == 0 &&
          memcmp(&display->source.modelview, &before, sizeof(before)) == 0);
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, error));
    CHECK(mc_client_display_lists_call(display, &prefix_id, 1, error) &&
          display->source.projection_depth == 1 && display->source.depth == 0 &&
          memcmp(display->source.projection, projection, sizeof(projection)) != 0 &&
          memcmp(&display->source.modelview, &before, sizeof(before)) == 0);
    CHECK(mc_client_display_lists_call(display, &pop_id, 1, error) &&
          display->source.projection_depth == 0 &&
          memcmp(display->source.projection, projection, sizeof(projection)) == 0);
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, error));
    CHECK(mc_client_display_lists_begin(display, underflow_id, error));
    CHECK(mc_client_display_source_color(display, 0.25f, 0.5f, 0.75f, 1, error));
    CHECK(mc_client_display_source_pop(display, error));
    CHECK(mc_client_display_lists_end(display, error));
    const float underflow_color[4] = {0.25f, 0.5f, 0.75f, 1};
    CHECK(!mc_client_display_lists_call(display, &underflow_id, 1, error) &&
          error->status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL && display->source.depth == 0 &&
          retained_color_setter(display, underflow_color) && display->source.active_calls == 0);
    /* Native stack admission happens on replay; a preceding push remains when
     * later arithmetic rejects changed controls. No list or matrix rollback. */
    _mm_setcsr(0x9fc0u);
    const bool replayed = mc_client_display_lists_call(display, &scale_id, 1, error);
    _mm_setcsr(0x1f80u);
    CHECK(!replayed && error->status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP &&
          display->source.depth == 1 && display->source.active_calls == 0 &&
          memcmp(&display->source.modelview, &before, sizeof(before)) == 0);
    CHECK(mc_client_display_lists_call(display, &pop_id, 1, error));
    for (size_t index = 0; index < SOURCE_MATRIX_DEPTH; ++index)
        CHECK(mc_client_display_source_push(display, error));
    CHECK(!mc_client_display_lists_call(display, &prefix_id, 1, error) &&
          error->status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL &&
          display->source.depth == SOURCE_MATRIX_DEPTH && display->source.active_calls == 0);
    for (size_t index = 0; index < SOURCE_MATRIX_DEPTH; ++index)
        CHECK(mc_client_display_source_pop(display, error));

    /* Independently captured real compatibility-GL operands from the authored
     * DK sequence. Exact bits qualify this finite profile, not generic driver
     * matrix precision or execution of the full original DK compiler. */
    static const uint32_t original_matrix[16] = {
        0x3f800008u, 0, 0,           0, 0,           0x3f800008u, 0,           0,
        0,           0, 0x3f800008u, 0, 0x40400020u, 0xc09ffff0u, 0x40e00010u, 0x3f800000u};
    const int32_t modes[2] = {GL_MODELVIEW, GL_PROJECTION};
    for (size_t index = 0; index < 2; ++index) {
        CHECK(mc_client_display_source_matrix_mode(display, modes[index], error));
        CHECK(mc_client_display_source_push(display, error));
        CHECK(mc_client_display_source_load_identity(display, error));
        CHECK(mc_client_display_source_translate(display, 3, -5, 7, error));
        CHECK(mc_client_display_lists_call(display, &prefix_id, 1, error));
        float actual[16];
        if (modes[index] == GL_MODELVIEW)
            memcpy(actual, display->source.modelview.column_major, sizeof(actual));
        else
            memcpy(actual, display->source.projection, sizeof(actual));
        CHECK(memcmp(actual, original_matrix, sizeof(actual)) == 0);
        CHECK(mc_client_display_lists_call(display, &pop_id, 1, error));
        CHECK(mc_client_display_source_pop(display, error));
    }
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, error));
    CHECK(display->source.depth == 0 && display->source.projection_depth == 0 &&
          memcmp(&display->source.modelview, &before, sizeof(before)) == 0 &&
          memcmp(display->source.projection, projection, sizeof(projection)) == 0);

    CHECK(mc_client_display_lists_generate(display, 1, 2, &limited, &limited_id, error));
    CHECK(mc_client_display_lists_begin(display, limited_id, error));
    CHECK(mc_client_display_source_color(display, 1, 1, 1, 1, error));
    CHECK(mc_client_display_lists_end(display, error));
    CHECK(mc_client_display_lists_begin(display, limited_id, error));
    CHECK(mc_client_display_source_push(display, error));
    CHECK(!mc_client_display_source_scale(display, 2, 3, 4, error) &&
          error->status == MC_CLIENT_DISPLAY_LIMIT);
    CHECK(mc_client_display_list_inspect(display, limited_id, &list) && list.defined &&
          list.building && list.commands == 1 && list.pending_commands == 1 &&
          display->source.depth == 0 &&
          memcmp(&display->source.modelview, &before, sizeof(before)) == 0);
    CHECK(mc_client_display_lists_abort(display, limited_id, error));
    const float white[4] = {1, 1, 1, 1};
    CHECK(mc_client_display_lists_call(display, &limited_id, 1, error) &&
          retained_color_setter(display, white) && display->source.depth == 0);
    result = true;
cleanup:
    _mm_setcsr(0x1f80u);
    mc_client_capture_free(&immediate);
    mc_client_capture_free(&compiled);
    if (!mc_client_display_lists_retire(&limited, error))
        result = false;
    if (!mc_client_display_lists_retire(&range, error))
        result = false;
    return result;
}

static bool replay_native_controls(mc_client_display *display,
                                   mc_client_display_context_lease *lease, uint8_t resume_bytes[96],
                                   mc_client_display_error *error) {
    bool result = false, observing = false;
    mc_client_display_list_range *range = NULL, *other = NULL;
    mc_client_capture capture = {0};
    int32_t base = 0;

    enum { CACHE_VERTICES = ((SOURCE_CACHE_MIN_VERTICES + 2) / 3) * 3 };

    uint8_t small[2][96], cached[CACHE_VERTICES * 32];
    static const float positions[2][3][3] = {{{4, 4, 0}, {28, 4, 0}, {4, 28, 0}},
                                             {{36, 36, 0}, {60, 36, 0}, {36, 60, 0}}};
    for (size_t side = 0; side < 2; ++side) {
        triangle(small[side], (unsigned int)side);
        for (size_t vertex = 0; vertex < 3; ++vertex)
            memcpy(small[side] + vertex * 32, positions[side][vertex],
                   sizeof(positions[side][vertex]));
    }
    for (size_t vertex = 0; vertex < CACHE_VERTICES; ++vertex) {
        uint8_t *raw = cached + vertex * 32;
        memcpy(raw, small[0] + (vertex % 3u) * 32, 32);
        raw[20] = 0;
        raw[22] = 255;
    }
    CHECK(display->source.gpu_phase == 2 && display->source.program != 0 &&
          display->source.vao != 0 && display->source.vbo != 0 && display->program != 0 &&
          display->vao != 0 && display->vbo != 0 && display->program != display->source.program &&
          display->vao != display->source.vao && display->vbo != display->source.vbo);
    CHECK(mc_client_display_source_lighting_enable(display, false, error) &&
          mc_client_display_source_fog_enable(display, false, error) &&
          mc_client_display_source_alpha_test(display, false, error) &&
          mc_client_display_source_shade_model(display, GL_SMOOTH, error));
    CHECK(mc_client_display_lists_generate(display, 5, 8, &range, &base, error));
    for (int32_t i = 0; i < 3; ++i) {
        uint8_t *bytes = i == 2 ? cached : small[i];
        const int32_t count = i == 2 ? CACHE_VERTICES : 3;
        CHECK(triangle_arrays(display, lease, bytes, error));
        CHECK(mc_client_display_lists_begin(display, base + i, error));
        CHECK(mc_client_display_source_batch(lease, bytes, bytes, (size_t)count * 32, count,
                                             GL_TRIANGLES, error));
        CHECK(mc_client_display_lists_end(display, error));
    }
    const int32_t transform_id = base + 3, pop_id = base + 4;
    CHECK(mc_client_display_lists_begin(display, transform_id, error));
    CHECK(mc_client_display_source_push(display, error));
    CHECK(mc_client_display_source_translate(display, 1, 0, 0, error));
    CHECK(mc_client_display_source_scale(display, 2, 2, 2, error));
    CHECK(mc_client_display_lists_end(display, error));
    CHECK(mc_client_display_lists_begin(display, pop_id, error));
    CHECK(mc_client_display_source_pop(display, error));
    CHECK(mc_client_display_lists_end(display, error));
    int32_t other_id;
    CHECK(mc_client_display_lists_generate(display, 2, 1, &other, &other_id, error));
    CHECK(mc_client_display_lists_begin(display, other_id, error));
    CHECK(mc_client_display_lists_end(display, error));
    /* Populate the optional command storage before observing warm replay. */
    const int32_t cached_id = base + 2;
    CHECK(mc_client_display_lists_call(display, &cached_id, 1, error));
    mc_client_source_batch_view view;
    CHECK(mc_client_display_list_batch(display, cached_id, 0, &view) &&
          display->source_cache != NULL);
    const mc_client_source_list *recorded = source_find_list(display, cached_id, NULL);
    CHECK(recorded != NULL && recorded->first != NULL &&
          recorded->first->kind == SOURCE_COMMAND_DRAW);
    const source_cached_vertices *entry = source_vertex_cache_resident(
        display->source_cache, view.bytes, recorded->first->value.draw.cache_slot_hint);
    CHECK(entry != NULL && entry->data == view.bytes && entry->vao != 0 && entry->vbo != 0 &&
          entry->vao != display->source.vao && entry->vbo != display->source.vbo);
    const GLuint cached_vao = entry->vao, cached_vbo = entry->vbo;
    replay_native = display->gl;
    replay_display = display;
    display->gl.UseProgram = replay_program;
    display->gl.BindVertexArray = replay_vao;
    display->gl.BindBuffer = replay_buffer;
    display->gl.DrawArrays = replay_draw;
    display->gl.Uniform1i = replay_integer;
    display->gl.UniformMatrix4fv = replay_matrix;
    display->gl.ProvokingVertex = replay_provoking;
    observing = true;

    const int32_t pair[] = {base, base + 1};
    const int32_t mixed[] = {base, base + 1, cached_id, base};
    for (unsigned int attempt = 0; attempt < 3; ++attempt) {
        if (attempt == 2) {
            /* Real caller-owned native state between public invocations. A
             * replay-local receipt must not remember the previous bindings or
             * uniforms. Use the Display's other actual program and storage. */
            replay_native.UseProgram(display->source.program);
            replay_native.Uniform1i(display->source.textured_uniform, 1);
            replay_native.Uniform1i(display->source.alpha_uniform, 1);
            replay_native.Uniform1i(display->source.fog_enabled_uniform, 1);
            replay_native.Uniform1i(display->source.lighting_uniform, 1);
            replay_native.Uniform1i(display->source.flat_uniform, 1);
            const GLfloat zero[16] = {0};
            replay_native.UniformMatrix4fv(display->source.projection_uniform, 1, GL_FALSE, zero);
            replay_native.UniformMatrix4fv(display->source.modelview_uniform, 1, GL_FALSE, zero);
            replay_native.ProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
            replay_native.UseProgram(display->program);
            replay_native.BindVertexArray(display->vao);
            replay_native.BindBuffer(GL_ARRAY_BUFFER, display->vbo);
            CHECK(replay_native.GetError() == GL_NO_ERROR);
        }
        CHECK(mc_client_display_loading_clear_color_depth(display, error));
        memset(replay_calls, 0, sizeof(replay_calls));
        memset(replay_names, 0, sizeof(replay_names));
        const bool transition = attempt == 1;
        CHECK(mc_client_display_lists_call(display, transition ? mixed : pair, transition ? 4 : 2,
                                           error));
        CHECK(replay_payload_counts(transition ? 4 : 2, 1, 1) &&
              replay_names[REPLAY_PROGRAM][0] == display->source.program &&
              replay_calls[REPLAY_VAO] == (transition ? 3u : 1u) &&
              replay_calls[REPLAY_BUFFER] == (transition ? 3u : 1u) &&
              replay_names[REPLAY_VAO][0] == display->source.vao &&
              replay_names[REPLAY_BUFFER][0] == display->source.vbo);
        if (transition)
            CHECK(replay_names[REPLAY_VAO][1] == cached_vao &&
                  replay_names[REPLAY_BUFFER][1] == cached_vbo &&
                  replay_names[REPLAY_VAO][2] == display->source.vao &&
                  replay_names[REPLAY_BUFFER][2] == display->source.vbo);
        GLint program = 0, vao = 0, buffer = 0;
        replay_native.GetIntegerv(GL_CURRENT_PROGRAM, &program);
        replay_native.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        replay_native.GetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        CHECK((GLuint)program == display->source.program && (GLuint)vao == display->source.vao &&
              (GLuint)buffer == display->source.vbo && replay_native.GetError() == GL_NO_ERROR);
        CHECK(mc_client_display_capture_rgba(display, &capture, error) &&
              color_pixels(&capture, 0) && color_pixels(&capture, 1) && !color_pixels(&capture, 2));
        mc_client_capture_free(&capture);
    }
    const int32_t changed[] = {base, transform_id, base + 1, pop_id, base};
    const int32_t modes[] = {GL_MODELVIEW, GL_PROJECTION};
    for (size_t i = 0; i < sizeof(modes) / sizeof(*modes); ++i) {
        CHECK(mc_client_display_source_matrix_mode(display, modes[i], error));
        memset(replay_calls, 0, sizeof(replay_calls));
        CHECK(mc_client_display_lists_call(display, changed, sizeof(changed) / sizeof(*changed),
                                           error));
        const bool projection = modes[i] == GL_PROJECTION;
        CHECK(replay_payload_counts(3, projection ? 3u : 1u, projection ? 1u : 3u) &&
              display->source.depth == 0 && display->source.projection_depth == 0);
    }
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, error));
    const int32_t undefined[] = {base, other_id, other_id + 1, base + 1};
    memset(replay_calls, 0, sizeof(replay_calls));
    CHECK(!mc_client_display_lists_call(display, undefined, sizeof(undefined) / sizeof(*undefined),
                                        error) &&
          error->status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL && replay_payload_counts(1, 1, 1));
    CHECK(mc_client_display_lists_retire(&other, error));
    const int32_t missing[] = {other_id, INT32_MIN};
    for (size_t i = 0; i < sizeof(missing) / sizeof(*missing); ++i) {
        const int32_t replay[] = {base, missing[i], base + 1};
        memset(replay_calls, 0, sizeof(replay_calls));
        CHECK(!mc_client_display_lists_call(display, replay, sizeof(replay) / sizeof(*replay),
                                            error) &&
              error->status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL && replay_payload_counts(1, 1, 1));
    }
    CHECK(mc_client_display_lists_retire(&range, error));
    CHECK(entry->data == NULL && entry->vao == cached_vao && entry->vbo == cached_vbo);
    result = true;
cleanup:
    if (observing) {
        display->gl = replay_native;
        replay_display = NULL;
    }
    mc_client_capture_free(&capture);
    if (!mc_client_display_lists_retire(&other, error))
        result = false;
    if (!mc_client_display_lists_retire(&range, error))
        result = false;
    const float white[4] = {1, 1, 1, 1};
    if (!triangle_arrays(display, lease, resume_bytes, error) ||
        !mc_client_display_source_color(display, white[0], white[1], white[2], white[3], error))
        result = false;
    return result;
}

static bool retained_byte_controls(mc_client_display *display,
                                   mc_client_display_context_lease *lease, uint8_t bytes[96],
                                   mc_client_display_error *error) {
    bool result = false;
    mc_client_display_list_range *range = NULL;
    int32_t base = 0;
    size_t simulated_bytes = 0;
    const size_t before = display->source.command_bytes;
    mc_client_source_list_state state;
    mc_client_source_batch_view committed, pending;
    const void *pending_identity = NULL, *after_identity = NULL;
    uint8_t red[96];
    CHECK(triangle_arrays(display, lease, bytes, error));
    CHECK(mc_client_display_lists_generate(display, 2, 8, &range, &base, error));
    triangle(bytes, 0);
    memcpy(red, bytes, sizeof(red));
    CHECK(mc_client_display_lists_begin(display, base, error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(red), 3, GL_TRIANGLES, error));
    CHECK(mc_client_display_lists_end(display, error));
    CHECK(mc_client_display_list_batch(display, base, 0, &committed));
    const uint8_t *old_bytes = committed.bytes;
    CHECK(display->source.command_bytes <= SOURCE_COMMAND_BYTES);

    /* Charge other live definitions without allocating their payload. Every
     * new pending record and command below still takes the real admission path. */
    simulated_bytes = SOURCE_COMMAND_BYTES - display->source.command_bytes;
    display->source.command_bytes += simulated_bytes;
    triangle(bytes, 2);
    CHECK(mc_client_display_lists_begin(display, base, error));
    CHECK(mc_client_display_source_batch(lease, bytes, bytes, sizeof(red), 3, GL_TRIANGLES, error));
    CHECK(mc_client_display_pending_list_identity(display, base, &pending_identity) &&
          pending_identity != NULL && display->source.command_bytes > SOURCE_COMMAND_BYTES &&
          mc_client_display_list_inspect(display, base, &state) && state.defined &&
          state.building && state.commands == 1 && state.pending_commands == 1 &&
          range->command_count == 2 && mc_client_display_list_batch(display, base, 0, &committed) &&
          committed.bytes == old_bytes && memcmp(committed.bytes, red, sizeof(red)) == 0 &&
          mc_client_display_list_pending_batch(display, base, 0, &pending) &&
          memcmp(pending.bytes, bytes, sizeof(red)) == 0);
    const size_t prefix_charge = display->source.command_bytes;
    CHECK(source_append(display, SOURCE_COMMAND_DRAW, SOURCE_COMMAND_BYTES, error) == NULL &&
          error->status == MC_CLIENT_DISPLAY_LIMIT &&
          display->source.command_bytes == prefix_charge && range->command_count == 2);

    simulated_bytes += SOURCE_RETAINED_COMMAND_BYTES - display->source.command_bytes;
    display->source.command_bytes = SOURCE_RETAINED_COMMAND_BYTES;
    CHECK(!mc_client_display_source_color(display, 0, 1, 0, 1, error) &&
          error->status == MC_CLIENT_DISPLAY_LIMIT &&
          display->source.command_bytes == SOURCE_RETAINED_COMMAND_BYTES &&
          range->command_count == 2 &&
          mc_client_display_pending_list_identity(display, base, &after_identity) &&
          after_identity == pending_identity);
    CHECK(mc_client_display_lists_abort(display, base, error));
    CHECK(!mc_client_display_lists_begin(display, base + 1, error) &&
          error->status == MC_CLIENT_DISPLAY_LIMIT &&
          display->source.command_bytes == SOURCE_RETAINED_COMMAND_BYTES &&
          display->source.compiling == NULL && display->source.compiling_range == NULL &&
          mc_client_display_list_inspect(display, base + 1, &state) && !state.defined &&
          !state.building && state.pending_commands == 0 && state.pending_bytes == 0);
    CHECK(mc_client_display_list_inspect(display, base, &state) && state.defined &&
          state.building && state.commands == 1 && state.pending_commands == 1 &&
          range->command_count == 2 && mc_client_display_list_batch(display, base, 0, &committed) &&
          committed.bytes == old_bytes && memcmp(committed.bytes, red, sizeof(red)) == 0 &&
          mc_client_display_list_pending_batch(display, base, 0, &pending) &&
          memcmp(pending.bytes, bytes, sizeof(red)) == 0);

    CHECK(mc_client_display_lists_call(display, &base, 1, error) &&
          display->source.command_bytes == SOURCE_RETAINED_COMMAND_BYTES);
    simulated_bytes -= sizeof(mc_client_source_definition);
    display->source.command_bytes -= sizeof(mc_client_source_definition);
    CHECK(mc_client_display_lists_begin(display, base + 1, error) &&
          display->source.command_bytes == SOURCE_RETAINED_COMMAND_BYTES);
    CHECK(mc_client_display_lists_end(display, error));
    result = true;
cleanup:
    display->source.command_bytes -= simulated_bytes;
    if (!mc_client_display_lists_retire(&range, error) || display->source.command_bytes != before)
        result = false;
    return result;
}

static GLuint eviction_names[8];
static size_t eviction_calls;
static GLenum eviction_error;

static void GLAPIENTRY observe_eviction(GLsizei count, const GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        if (eviction_calls < sizeof(eviction_names) / sizeof(*eviction_names))
            eviction_names[eviction_calls] = names[i];
        ++eviction_calls;
    }
}

static GLenum GLAPIENTRY observe_eviction_error(void) {
    const GLenum error = eviction_error;
    eviction_error = GL_NO_ERROR;
    return error;
}

static void GLAPIENTRY eviction_bind_vao(GLuint name) {
    (void)name;
}

static void GLAPIENTRY eviction_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void GLAPIENTRY eviction_upload(GLenum target, GLsizeiptr bytes, const void *data,
                                       GLenum usage) {
    (void)target;
    (void)bytes;
    (void)data;
    (void)usage;
}

static bool eviction_invalidation_control(void) {
    bool result = false;
    mc_client_display display = {0};

    enum { SETS = SOURCE_CACHE_SLOTS / SOURCE_CACHE_WAYS };

    uint8_t storage[SOURCE_CACHE_SLOTS + 1];
    const uint8_t *keys_by_set[SETS][SOURCE_CACHE_WAYS + 1];
    size_t counts[SETS] = {0};
    const uint8_t **keys = NULL;
    /* One more key than there are slots guarantees an overfull set. */
    for (size_t i = 0; i < sizeof(storage); ++i) {
        const size_t set = source_vertex_cache_index(&storage[i]) / SOURCE_CACHE_WAYS;
        keys_by_set[set][counts[set]++] = &storage[i];
        if (counts[set] > SOURCE_CACHE_WAYS) {
            keys = keys_by_set[set];
            break;
        }
    }
    CHECK(keys != NULL);

    const size_t tiny = SOURCE_CACHE_MIN_VERTICES * sizeof(mc_client_source_gpu_vertex);
    const size_t small = SOURCE_CACHE_REGULAR_VERTICES * sizeof(mc_client_source_gpu_vertex);
    const size_t large = SOURCE_CACHE_LARGE_VERTICES * sizeof(mc_client_source_gpu_vertex);
    source_cached_vertices *first = source_vertex_cache_slot(&display, keys[0], large, 0);
    CHECK(first != NULL);
    source_vertex_cache *cache = display.source_cache;
    cache->bytes = SOURCE_CACHE_BYTES;
    CHECK(source_vertex_cache_slot(&display, keys[0], small, 0) == NULL);
    cache->bytes = 0;
    for (size_t i = 0; i < SOURCE_CACHE_WAYS; ++i) {
        source_cached_vertices *entry = source_vertex_cache_slot(&display, keys[i], large, 0);
        CHECK(entry == first + i);
        entry->data = keys[i];
        source_vertex_cache_set_bytes(cache, entry, large);
        source_vertex_cache_touch(cache, entry);
    }
    const uint8_t *extra = keys[SOURCE_CACHE_WAYS];
    CHECK(source_vertex_cache_slot(&display, extra, small, 0) == NULL &&
          source_vertex_cache_slot(&display, extra, tiny, 0) == NULL);
    const uint32_t first_hint = (uint32_t)(first - cache->slots) + 1u;
    for (uint32_t i = 0; i < SOURCE_CACHE_WAYS; ++i)
        CHECK(source_vertex_cache_slot(&display, keys[i], large, first_hint + i) == first + i);

    source_vertex_cache_touch(cache, first);
    CHECK(source_vertex_cache_slot(&display, extra, small, first_hint) == NULL);
    CHECK(source_vertex_cache_slot(&display, extra, large, first_hint) == first + 1);
    source_vertex_cache_invalidate(&display, extra, first_hint);
    CHECK(first->data == keys[0]);
    source_cached_vertices *last = first + SOURCE_CACHE_WAYS - 1;
    source_vertex_cache_invalidate(&display, keys[SOURCE_CACHE_WAYS - 1],
                                   first_hint + SOURCE_CACHE_WAYS - 1u);
    cache->bytes = SOURCE_CACHE_BYTES;
    CHECK(last->data == NULL && last->last_used == 0 && last->bytes == large &&
          source_vertex_cache_oldest(cache, first) == last &&
          source_vertex_cache_slot(&display, extra, small, 0) == last &&
          source_vertex_cache_slot(&display, extra, tiny, 0) == last &&
          source_vertex_cache_slot(&display, keys[0], large, first_hint) == first);

    last->data = keys[SOURCE_CACHE_WAYS - 1];
    source_vertex_cache_set_bytes(cache, last, tiny);
    cache->bytes = SOURCE_CACHE_BYTES;
    source_vertex_cache_touch(cache, last);
    CHECK(source_vertex_cache_slot(&display, extra, tiny, 0) == last &&
          source_vertex_cache_slot(&display, extra, 2 * tiny, 0) == NULL &&
          source_vertex_cache_slot(&display, extra, small, 0) == NULL &&
          source_vertex_cache_slot(&display, extra, large, 0) == last);
    /* A regular batch can reclaim the tiny storage held by another set. */
    const size_t other_set =
        ((size_t)(first - cache->slots) + SOURCE_CACHE_WAYS) % SOURCE_CACHE_SLOTS;
    cache->bytes -= small - tiny;
    source_vertex_cache_set_bytes(cache, &cache->slots[other_set], small - tiny);
    CHECK(source_vertex_cache_slot(&display, extra, small, 0) == last &&
          source_vertex_cache_slot(&display, extra, 2 * tiny, 0) == NULL);
    result = true;
cleanup:
    free(display.source_cache);
    return result;
}

static bool eviction_controls(void) {
    bool result = false;
    mc_client_display display = {0};
    source_vertex_cache *cache = calloc(1, sizeof(*cache));
    mc_client_display_error error = {0};
    CHECK(cache != NULL);
    display.source_cache = cache;
    display.gl.DeleteBuffers = observe_eviction;
    display.gl.DeleteVertexArrays = observe_eviction;
    display.gl.GetError = observe_eviction_error;
    CHECK(source_vertex_cache_oldest(cache, NULL) == NULL);
    const size_t slots[] = {63, 64, 65, SOURCE_CACHE_SLOTS - 1u};
    const size_t charge = SOURCE_CACHE_BYTES / 4u;
    /* Synthetic charges drive pressure and injected failures without needing
     * large device allocations. The normal test exercises real GL draws. */
    for (size_t i = 0; i < sizeof(slots) / sizeof(*slots); ++i) {
        source_cached_vertices *entry = &cache->slots[slots[i]];
        entry->vbo = (GLuint)(2u * i + 1u);
        entry->vao = (GLuint)(2u * i + 2u);
        source_vertex_cache_set_bytes(cache, entry, charge);
    }
    source_cached_vertices *replaced = &cache->slots[64];
    CHECK(source_vertex_cache_oldest(cache, &cache->slots[63]) == replaced);
    CHECK(source_vertex_cache_oldest(cache, replaced) == &cache->slots[63]);
    CHECK(source_vertex_cache_oldest(cache, NULL) == &cache->slots[63]);
    cache->sequence = UINT64_MAX;
    source_vertex_cache_touch(cache, &cache->slots[63]);
    CHECK(source_vertex_cache_oldest(cache, replaced) == &cache->slots[63]);
    source_vertex_cache_touch(cache, &cache->slots[63]);
    CHECK(source_vertex_cache_oldest(cache, replaced) == &cache->slots[65]);

    eviction_error = GL_INVALID_OPERATION;
    eviction_calls = 0;
    CHECK(!source_vertex_cache_make_room(&display, replaced, 3u * charge, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_GL_ERROR && cache->bytes == SOURCE_CACHE_BYTES &&
          cache->slots[65].bytes == charge && cache->occupied[1] == 3 &&
          cache->occupied_count[1] == 2 && eviction_calls == 2);
    CHECK(source_vertex_cache_make_room(&display, replaced, 3u * charge, &error));
    const GLuint expected[] = {5, 6, 5, 6, 7, 8};
    CHECK(eviction_calls == sizeof(expected) / sizeof(*expected) &&
          memcmp(eviction_names, expected, sizeof(expected)) == 0);
    CHECK(cache->bytes == 2u * charge && replaced->bytes == charge && replaced->vbo == 3 &&
          cache->slots[63].bytes == charge && cache->slots[65].bytes == 0 &&
          cache->slots[SOURCE_CACHE_SLOTS - 1u].bytes == 0 && cache->occupied[1] == 1 &&
          cache->occupied_count[1] == 1);

    /* The room check refreshes the incoming group while it is still empty.
     * The completed upload must refresh it again when storage is published. */
    display.gl.BindVertexArray = eviction_bind_vao;
    display.gl.BindBuffer = eviction_bind_buffer;
    display.gl.BufferData = eviction_upload;
    source_vertex_cache_set_bytes(cache, &cache->slots[63], 3u * charge);
    source_cached_vertices *incoming = &cache->slots[SOURCE_CACHE_SLOTS - 1u];
    incoming->layout_ready = true;
    incoming->vao = 9;
    incoming->vbo = 10;
    const uint8_t key = 0;
    mc_client_source_draw_command draw = {.bytes = &key, .count = 1};
    const mc_client_source_gpu_vertex vertex = {0};
    source_vertex_cache_touch(cache, incoming);
    CHECK(source_vertex_cache_upload(&display, incoming, &draw, charge,
                                     SOURCE_ATTRIBUTE_PROFILE_UNKNOWN, NULL, NULL, &vertex,
                                     &error));
    CHECK(cache->bytes == SOURCE_CACHE_BYTES && replaced->bytes == 0 && incoming->bytes == charge &&
          eviction_calls == 8 && eviction_names[6] == 3 && eviction_names[7] == 4 &&
          source_vertex_cache_oldest(cache, &cache->slots[63]) == incoming &&
          draw.cache_slot_hint == SOURCE_CACHE_SLOTS);

    memset(cache, 0, sizeof(*cache));
    draw.cache_slot_hint = 0;
    const size_t tiny = SOURCE_CACHE_MIN_VERTICES * sizeof(mc_client_source_gpu_vertex);
    const size_t small = SOURCE_CACHE_REGULAR_VERTICES * sizeof(mc_client_source_gpu_vertex);
    source_cached_vertices *kept = &cache->slots[0];
    source_cached_vertices *neighbor = &cache->slots[65];
    source_cached_vertices *tiny_victim = &cache->slots[128];
    incoming = &cache->slots[64];
    source_vertex_cache_set_bytes(cache, kept, SOURCE_CACHE_BYTES - small - 3 * tiny);
    source_vertex_cache_set_bytes(cache, neighbor, small);
    neighbor->last_used = 1;
    source_vertex_cache_set_bytes(cache, incoming, tiny);
    incoming->layout_ready = true;
    incoming->vao = 9;
    incoming->vbo = 10;
    source_vertex_cache_set_bytes(cache, tiny_victim, 2 * tiny);
    tiny_victim->vao = 11;
    tiny_victim->vbo = 12;
    cache->sequence = neighbor->last_used;
    source_vertex_cache_touch(cache, incoming);
    source_vertex_cache_touch(cache, tiny_victim);
    CHECK(source_vertex_cache_oldest(cache, incoming) == tiny_victim);
    eviction_calls = 0;
    eviction_error = GL_INVALID_OPERATION;
    CHECK(!source_vertex_cache_upload(&display, incoming, &draw, small,
                                      SOURCE_ATTRIBUTE_PROFILE_UNKNOWN, NULL, NULL, &vertex,
                                      &error));
    CHECK(cache->tiny_bytes == 3 * tiny && cache->bytes == SOURCE_CACHE_BYTES &&
          tiny_victim->bytes == 2 * tiny && eviction_calls == 2 && draw.cache_slot_hint == 0);
    CHECK(source_vertex_cache_upload(&display, incoming, &draw, small,
                                     SOURCE_ATTRIBUTE_PROFILE_UNKNOWN, NULL, NULL, &vertex,
                                     &error));
    CHECK(cache->tiny_bytes == 0 && cache->bytes == SOURCE_CACHE_BYTES && tiny_victim->bytes == 0 &&
          eviction_calls == 4 && eviction_names[2] == 12 && eviction_names[3] == 11 &&
          source_vertex_cache_oldest(cache, kept) == neighbor && draw.cache_slot_hint == 65);

    memset(cache, 0, sizeof(*cache));
    for (size_t i = 0; i <= SOURCE_CACHE_GROUP_SIZE / 2; ++i)
        source_vertex_cache_set_bytes(cache, &cache->slots[i], small);
    CHECK(cache->occupied_count[0] == SOURCE_CACHE_GROUP_SIZE / 2 + 1 &&
          source_vertex_cache_oldest(cache, &cache->slots[0]) == &cache->slots[1]);
    source_vertex_cache_set_bytes(cache, &cache->slots[0], 0);
    CHECK(cache->occupied_count[0] == SOURCE_CACHE_GROUP_SIZE / 2 &&
          source_vertex_cache_oldest(cache, &cache->slots[1]) == &cache->slots[2]);

    memset(cache, 0, sizeof(*cache));
    source_cached_vertices *older = &cache->slots[0], *newer = &cache->slots[1];
    source_vertex_cache_set_bytes(cache, older, small);
    source_vertex_cache_set_bytes(cache, newer, small);
    source_vertex_cache_touch(cache, older);
    source_vertex_cache_touch(cache, newer);
    CHECK(source_vertex_cache_oldest(cache, NULL) == older && cache->dirty_groups == 0);
    source_vertex_cache_touch(cache, newer);
    CHECK(cache->dirty_groups == 0 && source_vertex_cache_oldest(cache, NULL) == older);
    source_vertex_cache_touch(cache, older);
    CHECK(cache->dirty_groups == 1 && source_vertex_cache_oldest(cache, NULL) == newer);
    cache->sequence = UINT64_MAX;
    source_vertex_cache_touch(cache, older);
    CHECK(older->last_used == 0 && cache->dirty_groups == 1 &&
          source_vertex_cache_oldest(cache, NULL) == older);
    result = true;
cleanup:
    free(cache);
    return result;
}

static bool texture_lookup_controls(void) {
    bool result = false;
    mc_client_display display = {
        .context_lost = true, .texture_count = 4, .texture_resource_count = 1};
    mc_client_texture_resource *retiring = calloc(1, sizeof(*retiring));
    CHECK(retiring != NULL);
    retiring->display = &display;
    retiring->id = 1;
    display.textures[0] = (display_texture){.id = 1, .resource = retiring};
    display.textures[1].id = 33;
    display.textures[2].id = 65;
    CHECK(source_texture_slot(&display, 33) == &display.textures[1] &&
          source_texture_slot(&display, 33) == &display.textures[1] &&
          source_texture_slot(&display, 65) == &display.textures[2] &&
          source_texture_slot(&display, 33) == &display.textures[1] &&
          source_texture_slot(&display, 0) == &display.textures[3] &&
          source_texture_slot(&display, 97) == NULL);
    mc_client_display_error error = {0};
    CHECK(!mc_client_display_retire_texture(&retiring, &error) &&
          error.status == MC_CLIENT_DISPLAY_CONTEXT_LOST && retiring == NULL &&
          display.texture_count == 3 && source_texture_slot(&display, 1) == NULL &&
          source_texture_slot(&display, 33) == &display.textures[0] &&
          source_texture_slot(&display, 65) == &display.textures[1]);
    display.textures[display.texture_count++] = (display_texture){.id = 1};
    CHECK(source_texture_slot(&display, 1) == &display.textures[3] &&
          source_texture_slot(&display, 0) == &display.textures[2]);
    mc_client_texture_source_state *sampling = &display.textures[3].source;
    *sampling = source_texture_defaults(1);
    sampling->wrap_s = GL_CLAMP;
    sampling->min_filter = GL_LINEAR;
    sampling->image_uploaded = true;
    int clamp_s, clamp_t;
    CHECK(source_texture_sampling(&display, 1, &clamp_s, &clamp_t, &error) && clamp_s == 1 &&
          clamp_t == 0);
    sampling->known = false;
    CHECK(!source_texture_sampling(&display, 1, &clamp_s, &clamp_t, &error) &&
          error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
    result = true;
cleanup:
    free(retiring);
    return result;
}

static bool tessellator_context_controls(mc_client_display *display) {
    bool result = false;
    mc_client_render_engine_classes *classes = NULL;
    mc_client_tessellator *tessellator = NULL;
    mc_client_graphics_error graphics = {0};
    const mc_client_graphics_site caller = {"fixture", "tessellator_context", "()V", -1};
    const size_t starting_holds = display->source.context_holds;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_nw_active_use(classes, caller, &tessellator, &graphics));
    CHECK(mc_client_nw_begin_quads(tessellator, &graphics) &&
          graphics.status == MC_CLIENT_GRAPHICS_OK && classes->context_lease == NULL &&
          display->source.context_holds == starting_holds);

    classes->scene_lease_count = 1;
    classes->active_source_calls = 1;
    const bool bound = mc_client_render_context_bind(classes, display, caller, &graphics);
    classes->active_source_calls = 0;
    CHECK(bound && classes->context_lease != NULL &&
          display->source.context_holds == starting_holds + 1);
    mc_client_display_context_lease *lease = classes->context_lease;
    CHECK(!mc_client_nw_begin_quads(tessellator, &graphics) &&
          graphics.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE && graphics.site.pc == 16 &&
          classes->context_lease == lease && classes->bound_display == display &&
          display->source.context_holds == starting_holds + 1);
    classes->scene_lease_count = 0;
    CHECK(!mc_client_nw_begin_quads(tessellator, &graphics) &&
          graphics.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE && graphics.site.pc == 16 &&
          !graphics.cleanup_failed && classes->context_lease == NULL &&
          classes->bound_display == NULL && display->source.context_holds == starting_holds);
    result = true;
cleanup:
    if (classes != NULL) {
        classes->scene_lease_count = 0;
        if (!mc_client_render_context_unbind_empty(classes, &graphics))
            result = false;
        if (mc_client_render_engine_classes_release(classes) != MC_CLIENT_RENDER_ENGINE_OK)
            result = false;
    }
    return result;
}

int main(int argc, char **argv) {
    if (!texture_lookup_controls())
        return 1;
    if (argc == 2 && strcmp(argv[1], "--eviction-only") == 0) {
        if (!eviction_controls() || !eviction_invalidation_control())
            return 1;
        return printf("Vertex cache eviction: %u checks\n", checks) < 0 ? 1 : 0;
    }
    int result = 1;
    const unsigned original_controls = _mm_getcsr();
    mc_client_display *display = NULL;
    mc_client_display_context_lease *lease = NULL;
    mc_client_display_list_range *range = NULL, *limited = NULL, *large = NULL;
    mc_client_render_engine_classes *classes = NULL;
    mc_client_display_error error = {0};
    mc_client_graphics_error graphics = {0};
    mc_client_capture capture = {0};
    mc_client_loading_projection_stage stage;
    mc_client_source_list_state state;
    mc_client_source_batch_view committed, pending;
    mc_client_nw_state nw;
    uint8_t bytes[96], red[96];
    int32_t id = 0, limited_id = 0, large_id = 0;
    uint32_t sticky = 0;
    _mm_setcsr(0x1f80u);
    CHECK(mc_client_display_create("source list replacement controls", 64, 64, true, &display,
                                   &error));
    CHECK(tessellator_context_controls(display));
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
    CHECK(matrix_controls(display, lease, bytes, &error));
    CHECK(replay_native_controls(display, lease, bytes, &error));
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

    CHECK(retained_byte_controls(display, lease, bytes, &error) &&
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
    if (classes != NULL &&
        mc_client_render_engine_classes_release(classes) != MC_CLIENT_RENDER_ENGINE_OK)
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
