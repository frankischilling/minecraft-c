/* Compiled in the real Display translation unit. It uses the single actual
 * owner and GL table; no second Display layout or provider is admitted. */
#include <emmintrin.h>
#include <xmmintrin.h>

/* Native arithmetic admission is reached after effectful context/GL calls.
 * Bits0..5 are sticky exception flags, not unsupported control state. Never
 * repair a driver/callback's controls or roll back its completed GL effects. */
static bool source_fp_admit(const char *operation, mc_client_display_error *error) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_FP, operation,
                "strict native FP control state required; prior GL effects retained", 0);
}

static void source_initialize(mc_client_display *display) {
    mc_client_source_state *s = &display->source;
    if (s->initialized)
        return;
    s->color[0] = s->color[1] = s->color[2] = s->color[3] = 1.0f;
    s->color_known = true;
    s->fog.mode = GL_EXP;
    s->fog.distance_mode = 34140; /* GL_EYE_PLANE_ABSOLUTE_NV. */
    s->fog.density = 1.0f;
    s->fog.end = 1.0f;
    s->fog.normal[2] = 1.0f;
    s->fog.material_face = GL_FRONT_AND_BACK;
    s->fog.material_mode = GL_AMBIENT_AND_DIFFUSE;
    const mc_client_modelview identity = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    s->modelview = identity;
    memcpy(s->projection, identity.column_major, sizeof(s->projection));
    s->matrix_mode = GL_MODELVIEW;
    s->next_list_base = 1;
    source_lighting_initialize(s);
    source_material_initialize(&s->fog);
    s->initialized = true;
}

static bool source_enter(mc_client_display *display, const char *operation,
                         mc_client_display_error *error) {
    if (display == NULL || display->source.active_calls != 0)
        return fail(error,
                    display == NULL ? MC_CLIENT_DISPLAY_INVALID_ARGUMENT : MC_CLIENT_DISPLAY_BUSY,
                    operation, "source context operation active/null", 0);
    if (!current(display, operation, error))
        return false;
    source_initialize(display);
    ++display->source.active_calls;
    return true;
}

static bool source_leave(mc_client_display *display, bool ok, mc_client_display_error *error) {
    --display->source.active_calls;
    return ok ? success(error) : false;
}

static bool source_setting_enter(mc_client_display *display, const char *operation,
                                 mc_client_display_error *error) {
    if (!source_enter(display, operation, error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "compiled selected-state path", 0),
                            error);
    return true;
}

static void source_matrix_copy(const mc_client_source_state *state, mc_client_modelview *matrix) {
    if (state->matrix_mode == GL_PROJECTION)
        memcpy(matrix->column_major, state->projection, sizeof(state->projection));
    else
        *matrix = state->modelview;
}

static void source_matrix_store(mc_client_source_state *state, const mc_client_modelview *matrix) {
    if (state->matrix_mode == GL_PROJECTION)
        memcpy(state->projection, matrix->column_major, sizeof(state->projection));
    else
        state->modelview = *matrix;
}

/* Called only after the existing reached FP admission. Copy through the real
 * matrix value type, never alias-cast projection's float-array storage. */
static void source_matrix_translate(mc_client_source_state *state, float x, float y, float z) {
    mc_client_modelview matrix;
    source_matrix_copy(state, &matrix);
    mc_client_source_translate(&matrix, x, y, z);
    source_matrix_store(state, &matrix);
}

/* Immediate execution and list replay share the actual selected matrix and
 * stacks. GL_COMPILE records these operations without reading either stack. */
static bool source_matrix_push(mc_client_source_state *state, mc_client_display_error *error) {
    size_t *depth = state->matrix_mode == GL_PROJECTION ? &state->projection_depth : &state->depth;
    mc_client_modelview *stack =
        state->matrix_mode == GL_PROJECTION ? state->projection_stack : state->stack;
    if (*depth >= SOURCE_MATRIX_DEPTH)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source matrix push", "stack limit",
                    0);
    source_matrix_copy(state, &stack[(*depth)++]);
    return true;
}

static bool source_matrix_pop(mc_client_source_state *state, mc_client_display_error *error) {
    size_t *depth = state->matrix_mode == GL_PROJECTION ? &state->projection_depth : &state->depth;
    mc_client_modelview *stack =
        state->matrix_mode == GL_PROJECTION ? state->projection_stack : state->stack;
    if (*depth == 0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source matrix pop", "stack underflow",
                    0);
    source_matrix_store(state, &stack[--(*depth)]);
    return true;
}

static void source_matrix_scale(mc_client_source_state *state, double x, double y, double z) {
    mc_client_modelview matrix;
    source_matrix_copy(state, &matrix);
    const float scale[3] = {_mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(x))),
                            _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(y))),
                            _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(z)))};
    for (size_t col = 0; col < 3; ++col)
        for (size_t row = 0; row < 4; ++row)
            matrix.column_major[col * 4 + row] =
                mc_client_source_product(matrix.column_major[col * 4 + row], scale[col]);
    source_matrix_store(state, &matrix);
}

bool mc_client_display_source_get_error(mc_client_display *display, uint32_t *value,
                                        mc_client_display_error *error) {
    if (value == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source glGetError output",
                    "null output", 0);
    if (!source_enter(display, "source glGetError", error))
        return false;
    if (display->gl.GetError == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glGetError",
                                 "actual function address unavailable", 0),
                            error);
    const uint32_t result = (uint32_t)display->gl.GetError();
    *value = result;
    return source_leave(display, true, error);
}

bool mc_client_display_context_acquire(mc_client_display *display,
                                       mc_client_display_context_lease **out,
                                       mc_client_display_error *error) {
    if (out == NULL || display == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "graphics context lease",
                    "null argument", 0);
    if (display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "graphics context lease",
                    "source operation active", 0);
    if (!current(display, "graphics context lease", error))
        return false;
    if (display->source.context_holds == SIZE_MAX ||
        display->source.next_context_generation == UINT64_MAX)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "graphics context lease", "hold limit", 0);
    mc_client_display_context_lease *lease = calloc(1, sizeof(*lease));
    if (lease == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "graphics context lease",
                    "allocation failed", 0);
    lease->display = display;
    lease->generation = ++display->source.next_context_generation;
    ++display->source.context_holds;
    *out = lease;
    return success(error);
}

bool mc_client_display_context_release(mc_client_display_context_lease **slot,
                                       mc_client_display_error *error) {
    if (slot == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "release graphics context lease",
                    "null slot", 0);
    if (*slot != NULL) {
        for (unsigned i = 0; i < 4; ++i)
            if ((*slot)->display->source.array_contexts[i] == (*slot)->generation)
                (*slot)->display->source.pointers &= ~(1u << i);
        --(*slot)->display->source.context_holds;
        free(*slot);
        *slot = NULL;
    }
    return success(error);
}

bool mc_client_display_source_pointer(mc_client_display_context_lease *lease,
                                      mc_client_source_array array, const void *buffer,
                                      size_t offset, mc_client_display_error *error) {
    if (lease == NULL || buffer == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source client pointer",
                    "actual context/buffer required", 0);
    mc_client_display *display = lease->display;
    if ((unsigned)array > (unsigned)MC_CLIENT_SOURCE_NORMAL_ARRAY)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source client pointer",
                    "invalid selector", 0);
    if (!source_enter(display, "source client pointer", error))
        return false;
    /* Pointer offsets/types are the genuine nw32-byte layout. Host pointers
     * remain borrowed only by the following batch call, never retained. */
    display->source.pointers |= 1u << (unsigned)array;
    display->source.array_buffers[array] = buffer;
    display->source.array_contexts[array] = lease->generation;
    display->source.array_offsets[array] = offset;
    return source_leave(display, true, error);
}

bool mc_client_display_source_array(mc_client_display *display, mc_client_source_array array,
                                    bool enabled, mc_client_display_error *error) {
    if ((unsigned)array > (unsigned)MC_CLIENT_SOURCE_NORMAL_ARRAY)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source client array",
                    "invalid selector", 0);
    if (!source_enter(display, "source client array", error))
        return false;
    const unsigned mask = 1u << (unsigned)array;
    if (enabled)
        display->source.arrays |= mask;
    else
        display->source.arrays &= ~mask;
    return source_leave(display, true, error);
}

static mc_client_source_command *source_append(mc_client_display *display,
                                               mc_client_source_command_kind kind, size_t bytes,
                                               mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    mc_client_display_list_range *range = s->compiling_range;
    if (bytes > SIZE_MAX - sizeof(mc_client_source_command) ||
        sizeof(mc_client_source_command) + bytes > SOURCE_COMMAND_BYTES - s->command_bytes ||
        range->command_count >= range->max_commands) {
        fail(error, MC_CLIENT_DISPLAY_LIMIT, "source command definition",
             "command byte/count limit", 0);
        return NULL;
    }
    mc_client_source_command *command = calloc(1, sizeof(*command) + bytes);
    if (command == NULL) {
        fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source command definition",
             "allocation failed", 0);
        return NULL;
    }
    command->kind = kind;
    command->charge = sizeof(*command) + bytes;
    mc_client_source_definition *pending = s->compiling->pending;
    if (pending->last == NULL)
        pending->first = command;
    else
        pending->last->next = command;
    pending->last = command;
    ++pending->commands;
    pending->bytes += command->charge;
    ++range->command_count;
    s->command_bytes += command->charge;
    return command;
}

#include "source_lighting_draw.inc"
#include "source_lighting_numeric.h"
#include "source_lighting_shader.h"

static bool source_gpu_construct(mc_client_display *display, mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    if (s->program != 0 && s->vao != 0 && s->vbo != 0)
        return true;
    /* A failed lazy GPU constructor retains its real partial names for native
     * Display cleanup. It is not retried over a retained partial program. */
    if (s->vertex_shader != 0 || s->fragment_shader != 0 || s->program != 0 || s->vao != 0 ||
        s->vbo != 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU constructor",
                    "previous partial GPU constructor", 0);
    if (!shader(display, GL_VERTEX_SHADER, source_vertex_shader, &s->vertex_shader, error) ||
        !shader(display, GL_FRAGMENT_SHADER, source_fragment_shader, &s->fragment_shader, error))
        return false;
    s->program = display->gl.CreateProgram();
    if (!gl_check(display, "create source GPU program", error))
        return false;
    if (s->program == 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "create source GPU program", "zero name", 0);
    display->gl.AttachShader(s->program, s->vertex_shader);
    display->gl.AttachShader(s->program, s->fragment_shader);
    display->gl.LinkProgram(s->program);
    GLint linked = GL_FALSE;
    display->gl.GetProgramiv(s->program, GL_LINK_STATUS, &linked);
    if (!gl_check(display, "link source GPU program", error))
        return false;
    if (linked != GL_TRUE) {
        char log[256] = {0};
        display->gl.GetProgramInfoLog(s->program, (GLsizei)sizeof(log), NULL, log);
        log[sizeof(log) - 1] = '\0';
        if (!gl_check(display, "source GPU link diagnostic", error))
            return false;
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "link source GPU program", log, 0);
    }
    display->gl.UseProgram(s->program);
    s->projection_uniform = display->gl.GetUniformLocation(s->program, "projection");
    s->modelview_uniform = display->gl.GetUniformLocation(s->program, "modelview");
    s->textured_uniform = display->gl.GetUniformLocation(s->program, "textured");
    s->alpha_uniform = display->gl.GetUniformLocation(s->program, "alphaTest");
    s->fog_enabled_uniform = display->gl.GetUniformLocation(s->program, "fogEnabled");
    s->fog_mode_uniform = display->gl.GetUniformLocation(s->program, "fogMode");
    s->fog_distance_uniform = display->gl.GetUniformLocation(s->program, "fogDistance");
    s->fog_color_uniform = display->gl.GetUniformLocation(s->program, "fogColor");
    s->fog_density_uniform = display->gl.GetUniformLocation(s->program, "fogDensity");
    s->fog_start_uniform = display->gl.GetUniformLocation(s->program, "fogStart");
    s->fog_end_uniform = display->gl.GetUniformLocation(s->program, "fogEnd");
    const GLint image = display->gl.GetUniformLocation(s->program, "image");
    if (!gl_check(display, "source GPU uniforms", error))
        return false;
    if (s->projection_uniform < 0 || s->modelview_uniform < 0 || s->textured_uniform < 0 ||
        s->alpha_uniform < 0 || image < 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU uniforms", "missing uniform", 0);
    if (s->fog_enabled_uniform < 0 || s->fog_mode_uniform < 0 || s->fog_distance_uniform < 0 ||
        s->fog_color_uniform < 0 || s->fog_density_uniform < 0 || s->fog_start_uniform < 0 ||
        s->fog_end_uniform < 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU fog uniforms", "missing uniform",
                    0);
    if (!source_lighting_uniforms(display, error))
        return false;
    display->gl.Uniform1i(image, 0);
    display->gl.GenVertexArrays(1, &s->vao);
    display->gl.GenBuffers(1, &s->vbo);
    if (!gl_check(display, "source GPU vertex storage", error))
        return false;
    if (s->vao == 0 || s->vbo == 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU vertex storage", "zero name", 0);
    display->gl.BindVertexArray(s->vao);
    display->gl.BindBuffer(GL_ARRAY_BUFFER, s->vbo);
    const GLsizei stride = (GLsizei)sizeof(mc_client_source_gpu_vertex);
    display->gl.EnableVertexAttribArray(0);
    display->gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                                    (const void *)offsetof(mc_client_source_gpu_vertex, position));
    display->gl.EnableVertexAttribArray(1);
    display->gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                                    (const void *)offsetof(mc_client_source_gpu_vertex, uv));
    display->gl.EnableVertexAttribArray(2);
    display->gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                                    (const void *)offsetof(mc_client_source_gpu_vertex, color));
    display->gl.EnableVertexAttribArray(3);
    display->gl.VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                                    (const void *)offsetof(mc_client_source_gpu_vertex, normal));
    return gl_check(display, "source GPU vertex layout", error);
}

static bool source_gpu_create(mc_client_display *display, mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    if (s->gpu_phase == 2)
        return true;
    if (s->gpu_phase == 3) {
        if (error != NULL)
            *error = s->gpu_failure;
        return false;
    }
    s->gpu_phase = 1;
    mc_client_display_error detail = {0};
    const bool ok = source_gpu_construct(display, &detail);
    s->gpu_phase = ok ? 2u : 3u;
    if (!ok)
        s->gpu_failure = detail;
    if (error != NULL)
        *error = detail;
    return ok;
}

static bool source_execute_batch(mc_client_display *display, const uint8_t *data, size_t bytes,
                                 int32_t count, int32_t mode, unsigned arrays, bool compiled,
                                 mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    const unsigned vertex_bit = 1u << MC_CLIENT_SOURCE_VERTEX_ARRAY;
    const unsigned uv_bit = 1u << MC_CLIENT_SOURCE_UV_ARRAY;
    const unsigned color_bit = 1u << MC_CLIENT_SOURCE_COLOR_ARRAY;
    const unsigned normal_bit = 1u << MC_CLIENT_SOURCE_NORMAL_ARRAY;
    if (s->vbo_capacity_uncertain)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source VBO capacity",
                    "prior upload allocation is uncertain", 0);
    /* source_enter initialized the actual projection/modelview defaults.
     * Camera matrix calls replace that same ledger; drawing does not require
     * the unrelated loading-screen projection method to have run first. */
    if (count < 0 || (mode != GL_TRIANGLES && mode != GL_TRIANGLE_FAN) ||
        (arrays & vertex_bit) == 0 || (size_t)count > bytes / 32u ||
        (size_t)count > SOURCE_COMMAND_BYTES / sizeof(mc_client_source_gpu_vertex))
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source interleaved draw",
                    "unsupported mode/span/client vertex state", 0);
    if (!s->lighting_enabled && (arrays & color_bit) == 0 && !s->color_known)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source current color",
                    "unspecified color after prior color-array draw", 0);
    if (!source_lighting_admit(s, arrays, error))
        return false;
    if (s->texture_enabled && (arrays & uv_bit) == 0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source current texture coordinate",
                    "nonarray textured-coordinate path", 0);
    if (!source_fp_admit("source native draw conversion FP", error))
        return false;
    GLint unit = 0;
    display->gl.GetIntegerv(GL_ACTIVE_TEXTURE, &unit);
    if (!gl_check(display, "source active texture unit", error))
        return false;
    if (unit != GL_TEXTURE0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source active texture unit",
                    "nondefault unit path", 0);
    if (!source_fp_admit("source native draw conversion FP after texture query", error))
        return false;
    float normal_matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float normal_scale = 1;
    if (s->lighting_enabled && !source_normal_transform(s, normal_matrix, &normal_scale, error))
        return false;
    const size_t allocation = (size_t)count * sizeof(mc_client_source_gpu_vertex);
    mc_client_source_gpu_vertex *vertices = allocation == 0 ? NULL : malloc(allocation);
    if (allocation != 0 && vertices == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source draw conversion",
                    "allocation failed", 0);
    for (size_t i = 0; i < (size_t)count; ++i) {
        const uint8_t *raw = data + i * 32u;
        memcpy(vertices[i].position, raw, 3u * sizeof(float));
        if ((arrays & uv_bit) != 0)
            memcpy(vertices[i].uv, raw + 12, 2u * sizeof(float));
        else
            vertices[i].uv[0] = vertices[i].uv[1] = 0;
        if ((arrays & color_bit) != 0)
            for (size_t channel = 0; channel < 4; ++channel)
                vertices[i].color[channel] = compiled ? (float)raw[20 + channel] * (1.0f / 255.0f)
                                                      : (float)raw[20 + channel] / 255.0f;
        else
            memcpy(vertices[i].color, s->color, sizeof(s->color));
        if ((arrays & normal_bit) != 0)
            for (size_t component = 0; component < 3; ++component) {
                /* The captured legacy GL_BYTE array profile replicates each
                 * byte into a signed 16-bit normalized value. All 256 values
                 * were observed through gl_Normal transform feedback with
                 * type GL_BYTE, stride 32 and client memory. This is the
                 * selected reference conversion, not glNormal3b's formula. */
                const int byte = raw[24 + component];
                if (compiled) {
                    /* Compiled array commands use the immediate attribute
                     * conversion. All 256 captured list normals equal
                     * glNormal3b, including reciprocal-multiply rounding. */
                    const int value = byte < 128 ? byte : byte - 256;
                    vertices[i].normal[component] = (float)(2 * value + 1) * (1.0f / 255.0f);
                } else {
                    int value = byte * 257;
                    if (value >= 32768)
                        value -= 65536;
                    vertices[i].normal[component] = (float)value / 32767.0f;
                }
            }
        else
            memcpy(vertices[i].normal, s->fog.normal, sizeof(vertices[i].normal));
    }
    if (!source_lighting_numeric_admit(s, vertices, (size_t)count, normal_matrix, normal_scale,
                                       error)) {
        free(vertices);
        return false;
    }
    bool ok = source_gpu_create(display, error);
    if (ok) {
        display->gl.UseProgram(s->program);
        display->gl.BindVertexArray(s->vao);
        display->gl.BindBuffer(GL_ARRAY_BUFFER, s->vbo);
        if (allocation > s->vbo_bytes) {
            display->gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)allocation, vertices,
                                   GL_STREAM_DRAW);
            if (!gl_check(display, "source VBO allocation/upload", error)) {
                s->vbo_capacity_uncertain = true;
                ok = false;
            } else
                s->vbo_bytes = allocation;
        } else if (allocation != 0) {
            display->gl.BufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)allocation, vertices);
            ok = gl_check(display, "source VBO upload", error);
        }
    }
    if (ok) {
        display->gl.UniformMatrix4fv(s->projection_uniform, 1, GL_FALSE, s->projection);
        display->gl.UniformMatrix4fv(s->modelview_uniform, 1, GL_FALSE, s->modelview.column_major);
        display->gl.Uniform1i(s->textured_uniform, s->texture_enabled ? 1 : 0);
        display->gl.Uniform1i(s->alpha_uniform, s->alpha_test ? 1 : 0);
        display->gl.Uniform1i(s->fog_enabled_uniform, s->fog_enabled ? 1 : 0);
        display->gl.Uniform1i(s->fog_mode_uniform, s->fog.mode);
        display->gl.Uniform1i(s->fog_distance_uniform, s->fog.distance_mode);
        display->gl.Uniform4fv(s->fog_color_uniform, 1, s->fog.color);
        display->gl.Uniform1f(s->fog_density_uniform, s->fog.density);
        display->gl.Uniform1f(s->fog_start_uniform, s->fog.start);
        display->gl.Uniform1f(s->fog_end_uniform, s->fog.end);
        source_lighting_upload(display, normal_matrix, normal_scale);
        display->gl.ProvokingVertex(GL_LAST_VERTEX_CONVENTION);
        /* Preserve inherited blend/depth/viewport/scissor/cull/clear state. */
        display->gl.DrawArrays((GLenum)mode, 0, count);
        if ((arrays & color_bit) != 0) {
            if (compiled && count > 0) {
                memcpy(s->color, vertices[count - 1].color, sizeof(s->color));
                s->color_known = true;
                source_material_color(&s->fog, s->color, true);
            } else if (!compiled) {
                s->color_known = false;
                source_material_color_unknown(&s->fog);
            }
        }
        if ((arrays & normal_bit) != 0) {
            if (compiled && count > 0) {
                memcpy(s->fog.normal, vertices[count - 1].normal, sizeof(s->fog.normal));
                s->fog.normal_known = true;
            } else if (!compiled)
                s->fog.normal_known = false;
        }
        ok = gl_check(display, "source interleaved draw", error);
        if (ok)
            display->frame_started = true;
    }
    free(vertices);
    return ok;
}

bool mc_client_display_source_batch(mc_client_display_context_lease *lease, const void *buffer,
                                    const uint8_t *bytes, size_t size, int32_t count, int32_t mode,
                                    mc_client_display_error *error) {
    if (lease == NULL || buffer == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source interleaved batch",
                    "actual context/buffer required", 0);
    mc_client_display *display = lease->display;
    if ((bytes == NULL && size != 0) || count < 0 || (size_t)count > size / 32u)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source interleaved batch",
                    "invalid data/count", 0);
    if (!source_enter(display, "source interleaved batch", error))
        return false;
    mc_client_source_state *s = &display->source;
    bool ok;
    bool pointers_match = (s->arrays & ~s->pointers) == 0;
    static const size_t offsets[4] = {0, 12, 20, 24};
    for (unsigned i = 0; i < 4; ++i)
        if ((s->arrays & (1u << i)) != 0 &&
            (s->array_buffers[i] != buffer || s->array_contexts[i] != lease->generation ||
             s->array_offsets[i] != offsets[i]))
            pointers_match = false;
    if (!pointers_match) {
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source client pointers",
                  "enabled array is expired, foreign or has a different layout", 0);
    } else if (s->compiling != NULL) {
        const size_t needed = (size_t)count * 32u;
        mc_client_source_command *command =
            source_append(display, SOURCE_COMMAND_DRAW, needed, error);
        ok = command != NULL;
        if (ok) {
            command->value.draw.bytes = (uint8_t *)(command + 1);
            command->value.draw.size = needed;
            command->value.draw.count = count;
            command->value.draw.mode = mode;
            command->value.draw.arrays = s->arrays;
            if (needed != 0)
                memcpy(command->value.draw.bytes, bytes, needed);
        }
    } else {
        ok = source_execute_batch(display, bytes, size, count, mode, s->arrays, false, error);
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_lists_generate(mc_client_display *display, int32_t count,
                                      size_t max_commands, mc_client_display_list_range **out,
                                      int32_t *base, mc_client_display_error *error) {
    if (out == NULL || base == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source list range", "null output",
                    0);
    if (!source_enter(display, "source list range", error))
        return false;
    mc_client_source_state *s = &display->source;
    bool ok = false;
    if (count <= 0) {
        fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source list range",
             "nonpositive/GL-zero path", 0);
        goto done;
    }
    if (s->range_count >= SOURCE_RANGES || (uint32_t)count > INT32_MAX - s->next_list_base ||
        (size_t)count > SOURCE_COMMAND_BYTES / sizeof(mc_client_source_list)) {
        fail(error, MC_CLIENT_DISPLAY_LIMIT, "source list range",
             "context range/name/allocation limit", 0);
        goto done;
    }
    mc_client_display_list_range *range = calloc(1, sizeof(*range));
    if (range == NULL) {
        fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source list range",
             "range allocation failed", 0);
        goto done;
    }
    range->lists = calloc((size_t)count, sizeof(*range->lists));
    if (range->lists == NULL) {
        free(range);
        fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source list range",
             "definition storage failed", 0);
        goto done;
    }
    range->display = display;
    range->base = (int32_t)s->next_list_base;
    range->count = count;
    range->max_commands = max_commands;
    range->next = s->ranges;
    s->ranges = range;
    s->next_list_base += (uint32_t)count;
    ++s->range_count;
    *base = range->base;
    *out = range;
    ok = true;
done:
    return source_leave(display, ok, error);
}

static mc_client_source_list *source_find_list(mc_client_display *display, int32_t id,
                                               mc_client_display_list_range **owner) {
    for (mc_client_display_list_range *range = display->source.ranges; range != NULL;
         range = range->next)
        if (id >= range->base && (uint32_t)(id - range->base) < (uint32_t)range->count) {
            if (owner != NULL)
                *owner = range;
            return &range->lists[id - range->base];
        }
    return NULL;
}

static void source_free_commands(mc_client_display_list_range *range,
                                 mc_client_source_command *command) {
    while (command != NULL) {
        mc_client_source_command *next = command->next;
        range->display->source.command_bytes -= command->charge;
        --range->command_count;
        free(command);
        command = next;
    }
}

bool mc_client_display_lists_begin(mc_client_display *display, int32_t id,
                                   mc_client_display_error *error) {
    if (!source_enter(display, "source glNewList", error))
        return false;
    bool ok = false;
    mc_client_display_list_range *range = NULL;
    mc_client_source_list *list = source_find_list(display, id, &range);
    if (display->source.compiling != NULL || list == NULL || list->building) {
        fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glNewList",
             "unregistered/interrupted/nested list path", 0);
    } else if (sizeof(mc_client_source_definition) >
               SOURCE_COMMAND_BYTES - display->source.command_bytes) {
        fail(error, MC_CLIENT_DISPLAY_LIMIT, "source glNewList", "pending definition byte limit",
             0);
    } else {
        mc_client_source_definition *pending = calloc(1, sizeof(*pending));
        if (pending == NULL)
            return source_leave(display,
                                fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source glNewList",
                                     "pending definition allocation failed", 0),
                                error);
        /* GL_COMPILE preserves the current definition until the matching
         * EndList. Old and pending commands both remain owned and charged. */
        list->pending = pending;
        display->source.command_bytes += sizeof(*pending);
        list->building = true;
        display->source.compiling = list;
        display->source.compiling_range = range;
        ok = true;
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_lists_end(mc_client_display *display, mc_client_display_error *error) {
    if (!source_enter(display, "source glEndList", error))
        return false;
    bool ok = false;
    if (display->source.compiling == NULL) {
        fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glEndList",
             "no source definition active", 0);
    } else {
        mc_client_source_list *list = display->source.compiling;
        mc_client_display_list_range *range = display->source.compiling_range;
        mc_client_source_definition *pending = list->pending;
        mc_client_source_command *old = list->first;
        list->first = pending->first;
        list->last = pending->last;
        list->pending = NULL;
        list->defined = true;
        list->building = false;
        display->source.compiling = NULL;
        display->source.compiling_range = NULL;
        display->source.command_bytes -= sizeof(*pending);
        free(pending);
        source_free_commands(range, old);
        ok = true;
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_color(mc_client_display *display, float r, float g, float b, float a,
                                    mc_client_display_error *error) {
    if (!source_enter(display, "source color", error))
        return false;
    const float color[4] = {r, g, b, a};
    bool ok = true;
    if (display->source.compiling != NULL) {
        mc_client_source_command *command = source_append(display, SOURCE_COMMAND_COLOR, 0, error);
        ok = command != NULL;
        if (ok)
            memcpy(command->value.vector, color, sizeof(color));
    } else {
        memcpy(display->source.color, color, sizeof(color));
        display->source.color_known = true;
        source_material_color(&display->source.fog, color, true);
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_translate(mc_client_display *display, float x, float y, float z,
                                        mc_client_display_error *error) {
    if (!source_enter(display, "source matrix translation", error))
        return false;
    bool ok = true;
    if (display->source.compiling != NULL) {
        mc_client_source_command *command =
            source_append(display, SOURCE_COMMAND_TRANSLATE, 0, error);
        ok = command != NULL;
        if (ok) {
            command->value.vector[0] = x;
            command->value.vector[1] = y;
            command->value.vector[2] = z;
        }
    } else {
        ok = source_fp_admit("source matrix translation FP", error);
        if (ok)
            source_matrix_translate(&display->source, x, y, z);
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_push(mc_client_display *display, mc_client_display_error *error) {
    if (!source_enter(display, "source matrix push", error))
        return false;
    const bool ok = display->source.compiling != NULL
                        ? source_append(display, SOURCE_COMMAND_PUSH, 0, error) != NULL
                        : source_matrix_push(&display->source, error);
    return source_leave(display, ok, error);
}

bool mc_client_display_source_pop(mc_client_display *display, mc_client_display_error *error) {
    if (!source_enter(display, "source matrix pop", error))
        return false;
    const bool ok = display->source.compiling != NULL
                        ? source_append(display, SOURCE_COMMAND_POP, 0, error) != NULL
                        : source_matrix_pop(&display->source, error);
    return source_leave(display, ok, error);
}

bool mc_client_display_source_texture_enable(mc_client_display *display, bool enabled,
                                             mc_client_display_error *error) {
    if (!source_enter(display, "source texture enable", error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture enable",
                                 "compiled enable path", 0),
                            error);
    display->source.texture_enabled = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_alpha_test(mc_client_display *display, bool enabled,
                                         mc_client_display_error *error) {
    if (!source_enter(display, "source alpha test", error))
        return false;
    if (display->source.compiling != NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source alpha test",
                                 "compiled alpha path", 0),
                            error);
    display->source.alpha_test = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_shade_model(mc_client_display *display, int32_t mode,
                                          mc_client_display_error *error) {
    if (!source_setting_enter(display, "source glShadeModel", error))
        return false;
    if (mode != GL_SMOOTH && mode != GL_FLAT)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glShadeModel",
                                 "only actual smooth/flat source modes are supported", 0),
                            error);
    /* The existing source shader already supplies smooth interpolation. */
    display->source.shade_flat = mode == GL_FLAT;
    return source_leave(display, true, error);
}

bool mc_client_display_source_clear_depth(mc_client_display *display, double depth,
                                          mc_client_display_error *error) {
    if (!source_setting_enter(display, "source glClearDepth", error))
        return false;
    uint64_t bits;
    memcpy(&bits, &depth, sizeof(bits));
    if (bits != UINT64_C(0x3ff0000000000000) || display->gl.ClearDepth == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glClearDepth",
                                 "depth1 and actual function address required", 0),
                            error);
    display->gl.ClearDepth(depth);
    return source_leave(display, true, error);
}

bool mc_client_display_source_depth_test(mc_client_display *display, bool enabled,
                                         mc_client_display_error *error) {
    if (!source_setting_enter(display, "source depth-test enable", error))
        return false;
    if ((enabled && display->gl.Enable == NULL) || (!enabled && display->gl.Disable == NULL))
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                 "source depth-test enable", "actual function address unavailable",
                                 0),
                            error);
    if (enabled)
        display->gl.Enable(GL_DEPTH_TEST);
    else
        display->gl.Disable(GL_DEPTH_TEST);
    return source_leave(display, true, error);
}

bool mc_client_display_source_depth_function(mc_client_display *display, int32_t function,
                                             mc_client_display_error *error) {
    if (!source_setting_enter(display, "source glDepthFunc", error))
        return false;
    if (function != GL_LEQUAL || display->gl.DepthFunc == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glDepthFunc",
                                 "LEQUAL and actual function address required", 0),
                            error);
    display->gl.DepthFunc((GLenum)function);
    return source_leave(display, true, error);
}

bool mc_client_display_source_alpha_function(mc_client_display *display, int32_t function,
                                             float reference, mc_client_display_error *error) {
    if (!source_setting_enter(display, "source glAlphaFunc", error))
        return false;
    uint32_t bits;
    memcpy(&bits, &reference, sizeof(bits));
    if (function != GL_GREATER || bits != UINT32_C(0x3dcccccd))
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glAlphaFunc",
                                 "only actual GREATER/float0.1 shader profile is supported", 0),
                            error);
    /* The existing alphaTest uniform gates that real fragment comparison.
     * No new program, uniform, floating arithmetic or GL error query occurs. */
    return source_leave(display, true, error);
}

bool mc_client_display_source_cull_face(mc_client_display *display, int32_t mode,
                                        mc_client_display_error *error) {
    if (!source_setting_enter(display, "source glCullFace", error))
        return false;
    if (mode != GL_BACK || display->gl.CullFace == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glCullFace",
                                 "BACK and actual function address required", 0),
                            error);
    display->gl.CullFace((GLenum)mode);
    return source_leave(display, true, error);
}

bool mc_client_display_source_matrix_mode(mc_client_display *display, int32_t mode,
                                          mc_client_display_error *error) {
    if (!source_setting_enter(display, "source glMatrixMode", error))
        return false;
    if (mode != GL_MODELVIEW && mode != GL_PROJECTION)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glMatrixMode",
                                 "only actual MODELVIEW/PROJECTION matrices are supported", 0),
                            error);
    display->source.matrix_mode = mode;
    return source_leave(display, true, error);
}

bool mc_client_display_source_load_identity(mc_client_display *display,
                                            mc_client_display_error *error) {
    if (!source_setting_enter(display, "source glLoadIdentity", error))
        return false;
    const mc_client_modelview identity = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    source_matrix_store(&display->source, &identity);
    return source_leave(display, true, error);
}

bool mc_client_display_lists_call(mc_client_display *display, const int32_t *ids, size_t count,
                                  mc_client_display_error *error) {
    if (ids == NULL && count != 0)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source glCallLists",
                    "null input span", 0);
    if (!source_enter(display, "source glCallLists", error))
        return false;
    bool ok = true;
    if (display->source.compiling != NULL) {
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glCallLists",
                  "compiled/nested-call path", 0);
        goto done;
    }
    for (size_t i = 0; i < count && ok; ++i) {
        mc_client_source_list *list = source_find_list(display, ids[i], NULL);
        if (list == NULL || !list->defined) {
            ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glCallLists",
                      "undefined/deleted/unregistered name path", 0);
            break;
        }
        for (const mc_client_source_command *command = list->first; command != NULL && ok;
             command = command->next) {
            if (command->kind == SOURCE_COMMAND_COLOR) {
                memcpy(display->source.color, command->value.vector, sizeof(display->source.color));
                display->source.color_known = true;
                source_material_color(&display->source.fog, display->source.color, true);
            } else if (command->kind == SOURCE_COMMAND_TRANSLATE) {
                ok = source_fp_admit("source list matrix translation FP", error);
                if (ok)
                    source_matrix_translate(&display->source, command->value.vector[0],
                                            command->value.vector[1], command->value.vector[2]);
            } else if (command->kind == SOURCE_COMMAND_PUSH) {
                ok = source_matrix_push(&display->source, error);
            } else if (command->kind == SOURCE_COMMAND_POP) {
                ok = source_matrix_pop(&display->source, error);
            } else if (command->kind == SOURCE_COMMAND_SCALE) {
                ok = source_fp_admit("source list matrix scale FP", error);
                if (ok)
                    source_matrix_scale(&display->source, command->value.scale[0],
                                        command->value.scale[1], command->value.scale[2]);
            } else if (command->kind == SOURCE_COMMAND_DRAW) {
                ok = source_execute_batch(display, command->value.draw.bytes,
                                          command->value.draw.size, command->value.draw.count,
                                          command->value.draw.mode, command->value.draw.arrays,
                                          true, error);
            } else {
                ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glCallLists",
                          "unrecognized native list command", 0);
            }
        }
    }
done:
    return source_leave(display, ok, error);
}

bool mc_client_display_lists_retire(mc_client_display_list_range **slot,
                                    mc_client_display_error *error) {
    if (slot == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "retire native source range",
                    "null slot", 0);
    mc_client_display_list_range *range = *slot;
    if (range == NULL)
        return success(error);
    mc_client_display *display = range->display;
    if (display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "retire native source range",
                    "source call active", 0);
    if (display->source.compiling_range == range) {
        /* Native terminal abort, never a source successful glEndList. */
        display->source.compiling = NULL;
        display->source.compiling_range = NULL;
    }
    mc_client_display_list_range **link = &display->source.ranges;
    while (*link != range)
        link = &(*link)->next;
    *link = range->next;
    for (int32_t i = 0; i < range->count; ++i) {
        mc_client_source_list *list = &range->lists[i];
        source_free_commands(range, list->first);
        if (list->pending != NULL) {
            source_free_commands(range, list->pending->first);
            display->source.command_bytes -= sizeof(*list->pending);
            free(list->pending);
        }
    }
    --display->source.range_count;
    free(range->lists);
    free(range);
    *slot = NULL;
    return success(error);
}

static void source_gpu_retire(mc_client_display *display, bool *ok,
                              mc_client_display_error *first) {
    mc_client_source_state *s = &display->source;
    if (display->context_lost)
        return;
    if (s->vbo != 0) {
        display->gl.DeleteBuffers(1, &s->vbo);
        cleanup_check(display, "delete source VBO", ok, first);
    }
    if (!display->context_lost && s->vao != 0) {
        display->gl.DeleteVertexArrays(1, &s->vao);
        cleanup_check(display, "delete source VAO", ok, first);
    }
    if (!display->context_lost && s->program != 0) {
        display->gl.DeleteProgram(s->program);
        cleanup_check(display, "delete source program", ok, first);
    }
    if (!display->context_lost && s->vertex_shader != 0) {
        display->gl.DeleteShader(s->vertex_shader);
        cleanup_check(display, "delete source vertex shader", ok, first);
    }
    if (!display->context_lost && s->fragment_shader != 0) {
        display->gl.DeleteShader(s->fragment_shader);
        cleanup_check(display, "delete source fragment shader", ok, first);
    }
}
