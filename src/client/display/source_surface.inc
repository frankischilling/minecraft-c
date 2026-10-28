/* Compiled in the real Display translation unit. It uses the single actual
 * owner and GL table; no second Display layout or provider is admitted. */
#include <xmmintrin.h>
/* Native arithmetic admission is reached after effectful context/GL calls.
 * Bits0..5 are sticky exception flags, not unsupported control state. Never
 * repair a driver/callback's controls or roll back its completed GL effects. */
static bool source_fp_admit(const char *operation, mc_client_display_error *error) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u) return true;
    return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_FP, operation,
                "strict native FP control state required; prior GL effects retained", 0);
}
static void source_initialize(mc_client_display *display) {
    mc_client_source_state *s = &display->source;
    if (s->initialized) return;
    s->color[0] = s->color[1] = s->color[2] = s->color[3] = 1.0f;
    s->color_known = true;
    const mc_client_modelview identity = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    s->modelview = identity;
    memcpy(s->projection, identity.column_major, sizeof(s->projection));
    s->next_list_base = 1;
    s->initialized = true;
}

static bool source_enter(mc_client_display *display, const char *operation, mc_client_display_error *error) {
    if (display == NULL || display->source.active_calls != 0)
        return fail(error, display == NULL ? MC_CLIENT_DISPLAY_INVALID_ARGUMENT : MC_CLIENT_DISPLAY_BUSY,
                    operation, "source context operation active/null", 0);
    if (!current(display, operation, error)) return false;
    source_initialize(display);
    ++display->source.active_calls;
    return true;
}

static bool source_leave(mc_client_display *display, bool ok, mc_client_display_error *error) {
    --display->source.active_calls;
    return ok ? success(error) : false;
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
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "graphics context lease", "null argument", 0);
    if (display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "graphics context lease", "source operation active", 0);
    if (!current(display, "graphics context lease", error)) return false;
    if (display->source.context_holds == SIZE_MAX || display->source.next_context_generation == UINT64_MAX)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "graphics context lease", "hold limit", 0);
    mc_client_display_context_lease *lease = calloc(1, sizeof(*lease));
    if (lease == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "graphics context lease", "allocation failed", 0);
    lease->display = display;
    lease->generation = ++display->source.next_context_generation;
    ++display->source.context_holds;
    *out = lease;
    return success(error);
}

bool mc_client_display_context_release(mc_client_display_context_lease **slot,
                                       mc_client_display_error *error) {
    if (slot == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "release graphics context lease", "null slot", 0);
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

bool mc_client_display_source_pointer(mc_client_display_context_lease *lease, mc_client_source_array array,
                                      const void *buffer, size_t offset, mc_client_display_error *error) {
    if (lease == NULL || buffer == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source client pointer", "actual context/buffer required", 0);
    mc_client_display *display = lease->display;
    if ((unsigned)array > (unsigned)MC_CLIENT_SOURCE_NORMAL_ARRAY)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source client pointer", "invalid selector", 0);
    if (!source_enter(display, "source client pointer", error)) return false;
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
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source client array", "invalid selector", 0);
    if (!source_enter(display, "source client array", error)) return false;
    const unsigned mask = 1u << (unsigned)array;
    if (enabled) display->source.arrays |= mask;
    else display->source.arrays &= ~mask;
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
        fail(error, MC_CLIENT_DISPLAY_LIMIT, "source command definition", "command byte/count limit", 0);
        return NULL;
    }
    mc_client_source_command *command = calloc(1, sizeof(*command) + bytes);
    if (command == NULL) {
        fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source command definition", "allocation failed", 0);
        return NULL;
    }
    command->kind = kind;
    command->charge = sizeof(*command) + bytes;
    if (s->compiling->last == NULL) s->compiling->first = command;
    else s->compiling->last->next = command;
    s->compiling->last = command;
    ++range->command_count;
    s->command_bytes += command->charge;
    return command;
}

static bool source_gpu_construct(mc_client_display *display, mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    if (s->program != 0 && s->vao != 0 && s->vbo != 0) return true;
    /* A failed lazy GPU constructor retains its real partial names for native
     * Display cleanup. It is not retried over a retained partial program. */
    if (s->vertex_shader != 0 || s->fragment_shader != 0 || s->program != 0 || s->vao != 0 || s->vbo != 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU constructor", "previous partial GPU constructor", 0);
    static const char vertex[] =
        "#version 330 core\nlayout(location=0) in vec3 position;\n"
        "layout(location=1) in vec2 uv;layout(location=2) in vec4 color;\n"
        "uniform mat4 projection;uniform mat4 modelview;out vec2 texUV;out vec4 tint;\n"
        "void main(){vec4 eye=modelview*vec4(position,1.0);gl_Position=projection*eye;texUV=uv;tint=color;}\n";
    static const char fragment[] =
        "#version 330 core\nin vec2 texUV;in vec4 tint;layout(location=0) out vec4 outputColor;\n"
        "uniform sampler2D image;uniform int textured;uniform int alphaTest;\n"
        "void main(){vec4 value=tint;if(textured!=0)value*=texture(image,texUV);"
        "if(alphaTest!=0 && value.a<=0.1)discard;outputColor=value;}\n";
    if (!shader(display, GL_VERTEX_SHADER, vertex, &s->vertex_shader, error) ||
        !shader(display, GL_FRAGMENT_SHADER, fragment, &s->fragment_shader, error)) return false;
    s->program = display->gl.CreateProgram();
    if (!gl_check(display, "create source GPU program", error)) return false;
    if (s->program == 0) return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "create source GPU program", "zero name", 0);
    display->gl.AttachShader(s->program, s->vertex_shader);
    display->gl.AttachShader(s->program, s->fragment_shader);
    display->gl.LinkProgram(s->program);
    GLint linked = GL_FALSE;
    display->gl.GetProgramiv(s->program, GL_LINK_STATUS, &linked);
    if (!gl_check(display, "link source GPU program", error)) return false;
    if (linked != GL_TRUE) {
        char log[256] = {0};
        display->gl.GetProgramInfoLog(s->program, (GLsizei)sizeof(log), NULL, log);
        log[sizeof(log) - 1] = '\0';
        if (!gl_check(display, "source GPU link diagnostic", error)) return false;
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "link source GPU program", log, 0);
    }
    display->gl.UseProgram(s->program);
    s->projection_uniform = display->gl.GetUniformLocation(s->program, "projection");
    s->modelview_uniform = display->gl.GetUniformLocation(s->program, "modelview");
    s->textured_uniform = display->gl.GetUniformLocation(s->program, "textured");
    s->alpha_uniform = display->gl.GetUniformLocation(s->program, "alphaTest");
    const GLint image = display->gl.GetUniformLocation(s->program, "image");
    if (!gl_check(display, "source GPU uniforms", error)) return false;
    if (s->projection_uniform < 0 || s->modelview_uniform < 0 || s->textured_uniform < 0 || s->alpha_uniform < 0 || image < 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU uniforms", "missing uniform", 0);
    display->gl.Uniform1i(image, 0);
    display->gl.GenVertexArrays(1, &s->vao);
    display->gl.GenBuffers(1, &s->vbo);
    if (!gl_check(display, "source GPU vertex storage", error)) return false;
    if (s->vao == 0 || s->vbo == 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU vertex storage", "zero name", 0);
    display->gl.BindVertexArray(s->vao);
    display->gl.BindBuffer(GL_ARRAY_BUFFER, s->vbo);
    const GLsizei stride = (GLsizei)sizeof(mc_client_source_gpu_vertex);
    display->gl.EnableVertexAttribArray(0);
    display->gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offsetof(mc_client_source_gpu_vertex, position));
    display->gl.EnableVertexAttribArray(1);
    display->gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)offsetof(mc_client_source_gpu_vertex, uv));
    display->gl.EnableVertexAttribArray(2);
    display->gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (const void *)offsetof(mc_client_source_gpu_vertex, color));
    return gl_check(display, "source GPU vertex layout", error);
}

static bool source_gpu_create(mc_client_display *display, mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    if (s->gpu_phase == 2) return true;
    if (s->gpu_phase == 3) {
        if (error != NULL) *error = s->gpu_failure;
        return false;
    }
    s->gpu_phase = 1;
    mc_client_display_error detail = {0};
    const bool ok = source_gpu_construct(display, &detail);
    s->gpu_phase = ok ? 2u : 3u;
    if (!ok) s->gpu_failure = detail;
    if (error != NULL) *error = detail;
    return ok;
}

static bool source_execute_batch(mc_client_display *display, const uint8_t *data, size_t bytes,
                                  int32_t count, int32_t mode, unsigned arrays,
                                  mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    const unsigned vertex_bit = 1u << MC_CLIENT_SOURCE_VERTEX_ARRAY;
    const unsigned uv_bit = 1u << MC_CLIENT_SOURCE_UV_ARRAY;
    const unsigned color_bit = 1u << MC_CLIENT_SOURCE_COLOR_ARRAY;
    if (s->vbo_capacity_uncertain)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source VBO capacity", "prior upload allocation is uncertain", 0);
    if (!s->loading_ready)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source projection state", "actual source projection surface not prepared", 0);
    if (s->lighting_enabled || s->fog_enabled)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source shading state", "enabled lighting/fog requires its renderer", 0);
    if (count < 0 || mode != GL_TRIANGLES || (arrays & vertex_bit) == 0 ||
        (size_t)count > bytes / 32u || (size_t)count > SOURCE_COMMAND_BYTES / sizeof(mc_client_source_gpu_vertex))
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source interleaved draw", "unsupported mode/span/client vertex state", 0);
    if ((arrays & color_bit) == 0 && !s->color_known)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source current color", "unspecified color after prior color-array draw", 0);
    if (s->texture_enabled && (arrays & uv_bit) == 0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source current texture coordinate", "nonarray textured-coordinate path", 0);
    if (!source_fp_admit("source native draw conversion FP", error)) return false;
    GLint unit = 0;
    display->gl.GetIntegerv(GL_ACTIVE_TEXTURE, &unit);
    if (!gl_check(display, "source active texture unit", error)) return false;
    if (unit != GL_TEXTURE0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source active texture unit", "nondefault unit path", 0);
    if (!source_fp_admit("source native draw conversion FP after texture query", error)) return false;
    const size_t allocation = (size_t)count * sizeof(mc_client_source_gpu_vertex);
    mc_client_source_gpu_vertex *vertices = allocation == 0 ? NULL : malloc(allocation);
    if (allocation != 0 && vertices == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source draw conversion", "allocation failed", 0);
    for (size_t i = 0; i < (size_t)count; ++i) {
        const uint8_t *raw = data + i * 32u;
        memcpy(vertices[i].position, raw, 3u * sizeof(float));
        if ((arrays & uv_bit) != 0) memcpy(vertices[i].uv, raw + 12, 2u * sizeof(float));
        else vertices[i].uv[0] = vertices[i].uv[1] = 0;
        if ((arrays & color_bit) != 0)
            for (size_t channel = 0; channel < 4; ++channel)
                vertices[i].color[channel] = (float)raw[20 + channel] / 255.0f;
        else memcpy(vertices[i].color, s->color, sizeof(s->color));
    }
    bool ok = source_gpu_create(display, error);
    if (ok) {
        display->gl.UseProgram(s->program);
        display->gl.BindVertexArray(s->vao);
        display->gl.BindBuffer(GL_ARRAY_BUFFER, s->vbo);
        if (allocation > s->vbo_bytes) {
            display->gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)allocation, vertices, GL_STREAM_DRAW);
            if (!gl_check(display, "source VBO allocation/upload", error)) {
                s->vbo_capacity_uncertain = true;
                ok = false;
            } else s->vbo_bytes = allocation;
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
        /* Preserve inherited blend/depth/viewport/scissor/cull/clear state. */
        display->gl.DrawArrays(GL_TRIANGLES, 0, count);
        if ((arrays & color_bit) != 0) s->color_known = false;
        ok = gl_check(display, "source interleaved draw", error);
        if (ok) display->frame_started = true;
    }
    free(vertices);
    return ok;
}

bool mc_client_display_source_batch(mc_client_display_context_lease *lease, const void *buffer,
                                    const uint8_t *bytes, size_t size,
                                    int32_t count, int32_t mode, mc_client_display_error *error) {
    if (lease == NULL || buffer == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source interleaved batch", "actual context/buffer required", 0);
    mc_client_display *display = lease->display;
    if ((bytes == NULL && size != 0) || count < 0 || (size_t)count > size / 32u)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source interleaved batch", "invalid data/count", 0);
    if (!source_enter(display, "source interleaved batch", error)) return false;
    mc_client_source_state *s = &display->source;
    bool ok;
    bool pointers_match = (s->arrays & ~s->pointers) == 0;
    static const size_t offsets[4] = {0, 12, 20, 24};
    for (unsigned i = 0; i < 4; ++i)
        if ((s->arrays & (1u << i)) != 0 &&
            (s->array_buffers[i] != buffer || s->array_contexts[i] != lease->generation || s->array_offsets[i] != offsets[i]))
            pointers_match = false;
    if (!pointers_match) {
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source client pointers", "enabled array is expired, foreign or has a different layout", 0);
    } else if (s->compiling != NULL) {
        const size_t needed = (size_t)count * 32u;
        mc_client_source_command *command = source_append(display, SOURCE_COMMAND_DRAW, needed, error);
        ok = command != NULL;
        if (ok) {
            command->value.draw.bytes = (uint8_t *)(command + 1);
            command->value.draw.size = needed;
            command->value.draw.count = count;
            command->value.draw.mode = mode;
            command->value.draw.arrays = s->arrays;
            if (needed != 0) memcpy(command->value.draw.bytes, bytes, needed);
        }
    } else {
        ok = source_execute_batch(display, bytes, size, count, mode, s->arrays, error);
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_lists_generate(mc_client_display *display, int32_t count, size_t max_commands,
                                      mc_client_display_list_range **out, int32_t *base,
                                      mc_client_display_error *error) {
    if (out == NULL || base == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source list range", "null output", 0);
    if (!source_enter(display, "source list range", error)) return false;
    mc_client_source_state *s = &display->source;
    bool ok = false;
    if (count <= 0) {
        fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source list range", "nonpositive/GL-zero path", 0);
        goto done;
    }
    if (s->range_count >= SOURCE_RANGES || (uint32_t)count > INT32_MAX - s->next_list_base ||
        (size_t)count > SOURCE_COMMAND_BYTES / sizeof(mc_client_source_list)) {
        fail(error, MC_CLIENT_DISPLAY_LIMIT, "source list range", "context range/name/allocation limit", 0);
        goto done;
    }
    mc_client_display_list_range *range = calloc(1, sizeof(*range));
    if (range == NULL) {
        fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source list range", "range allocation failed", 0);
        goto done;
    }
    range->lists = calloc((size_t)count, sizeof(*range->lists));
    if (range->lists == NULL) {
        free(range);
        fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "source list range", "definition storage failed", 0);
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
    for (mc_client_display_list_range *range = display->source.ranges; range != NULL; range = range->next)
        if (id >= range->base && (uint32_t)(id - range->base) < (uint32_t)range->count) {
            if (owner != NULL) *owner = range;
            return &range->lists[id - range->base];
        }
    return NULL;
}

bool mc_client_display_lists_begin(mc_client_display *display, int32_t id, mc_client_display_error *error) {
    if (!source_enter(display, "source glNewList", error)) return false;
    bool ok = false;
    mc_client_display_list_range *range = NULL;
    mc_client_source_list *list = source_find_list(display, id, &range);
    if (display->source.compiling != NULL || list == NULL || list->defined || list->building) {
        fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glNewList", "unregistered/redefined/nested list path", 0);
    } else {
        list->building = true;
        display->source.compiling = list;
        display->source.compiling_range = range;
        ok = true;
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_lists_end(mc_client_display *display, mc_client_display_error *error) {
    if (!source_enter(display, "source glEndList", error)) return false;
    bool ok = false;
    if (display->source.compiling == NULL) {
        fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glEndList", "no source definition active", 0);
    } else {
        display->source.compiling->defined = true;
        display->source.compiling->building = false;
        display->source.compiling = NULL;
        display->source.compiling_range = NULL;
        ok = true;
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_color(mc_client_display *display, float r, float g, float b, float a,
                                    mc_client_display_error *error) {
    if (!source_enter(display, "source color", error)) return false;
    const float color[4] = {r, g, b, a};
    bool ok = true;
    if (display->source.compiling != NULL) {
        mc_client_source_command *command = source_append(display, SOURCE_COMMAND_COLOR, 0, error);
        ok = command != NULL;
        if (ok) memcpy(command->value.vector, color, sizeof(color));
    } else {
        memcpy(display->source.color, color, sizeof(color));
        display->source.color_known = true;
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_translate(mc_client_display *display, float x, float y, float z,
                                        mc_client_display_error *error) {
    if (!source_enter(display, "source matrix translation", error)) return false;
    bool ok = true;
    if (display->source.compiling != NULL) {
        mc_client_source_command *command = source_append(display, SOURCE_COMMAND_TRANSLATE, 0, error);
        ok = command != NULL;
        if (ok) { command->value.vector[0] = x; command->value.vector[1] = y; command->value.vector[2] = z; }
    } else {
        ok = source_fp_admit("source matrix translation FP", error);
        if (ok) mc_client_source_translate(&display->source.modelview, x, y, z);
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_push(mc_client_display *display, mc_client_display_error *error) {
    if (!source_enter(display, "source matrix push", error)) return false;
    bool ok = false;
    if (display->source.compiling != NULL || display->source.depth >= SOURCE_MATRIX_DEPTH) {
        fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source matrix push", "compiled push/stack limit", 0);
    } else {
        display->source.stack[display->source.depth++] = display->source.modelview;
        ok = true;
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_pop(mc_client_display *display, mc_client_display_error *error) {
    if (!source_enter(display, "source matrix pop", error)) return false;
    bool ok = false;
    if (display->source.compiling != NULL || display->source.depth == 0) {
        fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source matrix pop", "compiled pop/stack underflow", 0);
    } else {
        display->source.modelview = display->source.stack[--display->source.depth];
        ok = true;
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_texture_enable(mc_client_display *display, bool enabled,
                                             mc_client_display_error *error) {
    if (!source_enter(display, "source texture enable", error)) return false;
    if (display->source.compiling != NULL)
        return source_leave(display, fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture enable", "compiled enable path", 0), error);
    display->source.texture_enabled = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_alpha_test(mc_client_display *display, bool enabled,
                                         mc_client_display_error *error) {
    if (!source_enter(display, "source alpha test", error)) return false;
    if (display->source.compiling != NULL)
        return source_leave(display, fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source alpha test", "compiled alpha path", 0), error);
    display->source.alpha_test = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_lists_call(mc_client_display *display, const int32_t *ids, size_t count,
                                  mc_client_display_error *error) {
    if (ids == NULL && count != 0)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source glCallLists", "null input span", 0);
    if (!source_enter(display, "source glCallLists", error)) return false;
    bool ok = true;
    if (display->source.compiling != NULL) {
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glCallLists", "compiled/nested-call path", 0);
        goto done;
    }
    for (size_t i = 0; i < count && ok; ++i) {
        mc_client_source_list *list = source_find_list(display, ids[i], NULL);
        if (list == NULL || !list->defined) {
            ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glCallLists", "undefined/deleted/unregistered name path", 0);
            break;
        }
        for (const mc_client_source_command *command = list->first; command != NULL && ok; command = command->next) {
            if (command->kind == SOURCE_COMMAND_COLOR) {
                memcpy(display->source.color, command->value.vector, sizeof(display->source.color));
                display->source.color_known = true;
            } else if (command->kind == SOURCE_COMMAND_TRANSLATE) {
                ok = source_fp_admit("source list matrix translation FP", error);
                if (ok) mc_client_source_translate(&display->source.modelview, command->value.vector[0], command->value.vector[1], command->value.vector[2]);
            } else {
                ok = source_execute_batch(display, command->value.draw.bytes, command->value.draw.size,
                                           command->value.draw.count, command->value.draw.mode,
                                           command->value.draw.arrays, error);
            }
        }
    }
done:
    return source_leave(display, ok, error);
}

bool mc_client_display_lists_retire(mc_client_display_list_range **slot, mc_client_display_error *error) {
    if (slot == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "retire native source range", "null slot", 0);
    mc_client_display_list_range *range = *slot;
    if (range == NULL) return success(error);
    mc_client_display *display = range->display;
    if (display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "retire native source range", "source call active", 0);
    if (display->source.compiling_range == range) {
        /* Native terminal abort, never a source successful glEndList. */
        display->source.compiling = NULL;
        display->source.compiling_range = NULL;
    }
    mc_client_display_list_range **link = &display->source.ranges;
    while (*link != range) link = &(*link)->next;
    *link = range->next;
    for (int32_t i = 0; i < range->count; ++i) {
        mc_client_source_command *command = range->lists[i].first;
        while (command != NULL) {
            mc_client_source_command *next = command->next;
            display->source.command_bytes -= command->charge;
            free(command);
            command = next;
        }
    }
    --display->source.range_count;
    free(range->lists);
    free(range);
    *slot = NULL;
    return success(error);
}

static void source_gpu_retire(mc_client_display *display, bool *ok, mc_client_display_error *first) {
    mc_client_source_state *s = &display->source;
    if (display->context_lost) return;
    if (s->vbo != 0) { display->gl.DeleteBuffers(1, &s->vbo); cleanup_check(display, "delete source VBO", ok, first); }
    if (!display->context_lost && s->vao != 0) { display->gl.DeleteVertexArrays(1, &s->vao); cleanup_check(display, "delete source VAO", ok, first); }
    if (!display->context_lost && s->program != 0) { display->gl.DeleteProgram(s->program); cleanup_check(display, "delete source program", ok, first); }
    if (!display->context_lost && s->vertex_shader != 0) { display->gl.DeleteShader(s->vertex_shader); cleanup_check(display, "delete source vertex shader", ok, first); }
    if (!display->context_lost && s->fragment_shader != 0) { display->gl.DeleteShader(s->fragment_shader); cleanup_check(display, "delete source fragment shader", ok, first); }
}
