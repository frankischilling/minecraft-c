#include "renderer.h"
#include "gl_internal.h"

#include <SDL3/SDL_video.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef void(GLAPIENTRY *world_clear_color_fn)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void(GLAPIENTRY *world_cull_face_fn)(GLenum);

typedef struct world_gl {
    mc_client_gl base;
    world_clear_color_fn ClearColor;
    world_cull_face_fn CullFace;
    PFNGLUNIFORM1FPROC Uniform1f;
    PFNGLUNIFORM3FPROC Uniform3f;
} world_gl;

typedef struct world_vertex {
    GLfloat x, y, z, u, v;
    GLubyte color[4];
} world_vertex;

struct mc_client_world_mesh {
    mc_client_world_renderer *renderer;
    struct mc_client_world_mesh *previous, *next;
    GLuint vao, vbo;
    int32_t x, y, z;
    size_t count, bytes;
};

struct mc_client_world_renderer {
    mc_client_display *display;
    world_gl gl;
    mc_client_world_renderer_limits limits;
    mc_client_texture terrain;
    GLuint vertex_shader, fragment_shader, program;
    GLint projection, modelview, alpha_test;
    GLint fog_mode, fog_color, fog_start, fog_end, fog_density, fog_radial;
    mc_client_world_camera camera;
    float camera_matrix[16];
    mc_client_world_mesh *meshes;
    size_t mesh_count, vertex_bytes;
    bool frame_started;
};

static bool failure(mc_client_display_error *error, mc_client_display_status status,
                    const char *operation) {
    if (error != NULL)
        *error = (mc_client_display_error){.status = status, .operation = operation};
    return false;
}

static bool success(mc_client_display_error *error) {
    if (error != NULL)
        *error = (mc_client_display_error){0};
    return true;
}

static bool check(mc_client_world_renderer *renderer, const char *operation,
                  mc_client_display_error *error) {
    return mc_client_display_check_gl(renderer->display, operation, error);
}

static bool load_gl(mc_client_world_renderer *renderer, mc_client_display_error *error) {
#define WORLD_LOAD(type, field, name)                                                              \
    renderer->gl.base.field = (type)SDL_GL_GetProcAddress(name);                                   \
    if (renderer->gl.base.field == NULL)                                                           \
        return failure(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, name);
    MC_CLIENT_GL_FUNCTIONS(WORLD_LOAD)
#undef WORLD_LOAD
#define WORLD_EXTRA(type, field, name)                                                             \
    renderer->gl.field = (type)SDL_GL_GetProcAddress(name);                                        \
    if (renderer->gl.field == NULL)                                                                \
    return failure(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, name)
    WORLD_EXTRA(world_clear_color_fn, ClearColor, "glClearColor");
    WORLD_EXTRA(world_cull_face_fn, CullFace, "glCullFace");
    WORLD_EXTRA(PFNGLUNIFORM1FPROC, Uniform1f, "glUniform1f");
    WORLD_EXTRA(PFNGLUNIFORM3FPROC, Uniform3f, "glUniform3f");
#undef WORLD_EXTRA
    return check(renderer, "load World rendering functions", error);
}

static bool shader(mc_client_world_renderer *renderer, GLenum kind, const char *text, GLuint *out,
                   mc_client_display_error *error) {
    mc_client_gl *gl = &renderer->gl.base;
    *out = gl->CreateShader(kind);
    if (!check(renderer, "create World shader", error))
        return false;
    if (*out == 0)
        return failure(error, MC_CLIENT_DISPLAY_GL_ERROR, "zero World shader ID");
    gl->ShaderSource(*out, 1, &text, NULL);
    gl->CompileShader(*out);
    GLint compiled = GL_FALSE;
    gl->GetShaderiv(*out, GL_COMPILE_STATUS, &compiled);
    if (!check(renderer, "compile World shader", error))
        return false;
    if (compiled == GL_TRUE)
        return true;
    failure(error, MC_CLIENT_DISPLAY_GL_ERROR, "compile World shader");
    if (error != NULL) {
        gl->GetShaderInfoLog(*out, (GLsizei)sizeof(error->detail), NULL, error->detail);
        error->detail[sizeof(error->detail) - 1u] = '\0';
    }
    return false;
}

static bool program(mc_client_world_renderer *renderer, mc_client_display_error *error) {
    static const char vertex[] =
        "#version 330 core\n"
        "layout(location=0) in vec3 position;layout(location=1) in vec2 uv;"
        "layout(location=2) in vec4 color;\n"
        "uniform mat4 projection;uniform mat4 modelview;"
        "uniform int radialFog;out vec2 fragmentUV;out vec4 fragmentColor;out float fogDistance;\n"
        "void main(){vec4 eye=modelview*vec4(position,1.0);"
        "gl_Position=projection*eye;fragmentUV=uv;fragmentColor=color;"
        "fogDistance=radialFog!=0?length(eye.xyz):abs(eye.z);}\n";
    static const char fragment[] =
        "#version 330 core\n"
        "in vec2 fragmentUV;in vec4 fragmentColor;in float fogDistance;"
        "layout(location=0) out vec4 outputColor;uniform sampler2D terrain;"
        "uniform int alphaTest;uniform int fogMode;uniform vec3 fogColor;"
        "uniform float fogStart,fogEnd,fogDensity;\n"
        "void main(){vec4 value=texture(terrain,fragmentUV)*fragmentColor;"
        "if(alphaTest!=0&&value.a<=0.1)discard;float factor=1.0;"
        "if(fogMode==1)factor=clamp((fogEnd-fogDistance)/(fogEnd-fogStart),0.0,1.0);"
        "if(fogMode==2)factor=clamp(exp(-fogDensity*fogDistance),0.0,1.0);"
        "outputColor=vec4(mix(fogColor,value.rgb,factor),value.a);}\n";
    mc_client_gl *gl = &renderer->gl.base;
    if (!shader(renderer, GL_VERTEX_SHADER, vertex, &renderer->vertex_shader, error) ||
        !shader(renderer, GL_FRAGMENT_SHADER, fragment, &renderer->fragment_shader, error))
        return false;
    renderer->program = gl->CreateProgram();
    if (!check(renderer, "create World program", error))
        return false;
    if (renderer->program == 0)
        return failure(error, MC_CLIENT_DISPLAY_GL_ERROR, "zero World program ID");
    gl->AttachShader(renderer->program, renderer->vertex_shader);
    gl->AttachShader(renderer->program, renderer->fragment_shader);
    gl->LinkProgram(renderer->program);
    GLint linked = GL_FALSE;
    gl->GetProgramiv(renderer->program, GL_LINK_STATUS, &linked);
    if (!check(renderer, "link World program", error))
        return false;
    if (linked != GL_TRUE) {
        failure(error, MC_CLIENT_DISPLAY_GL_ERROR, "link World program");
        if (error != NULL) {
            gl->GetProgramInfoLog(renderer->program, (GLsizei)sizeof(error->detail), NULL,
                                  error->detail);
            error->detail[sizeof(error->detail) - 1u] = '\0';
        }
        return false;
    }
#define UNIFORM(field, name)                                                                       \
    renderer->field = gl->GetUniformLocation(renderer->program, name);                             \
    if (renderer->field < 0)                                                                       \
    return failure(error, MC_CLIENT_DISPLAY_GL_ERROR, "locate World uniform " name)
    UNIFORM(projection, "projection");
    UNIFORM(modelview, "modelview");
    UNIFORM(alpha_test, "alphaTest");
    UNIFORM(fog_mode, "fogMode");
    UNIFORM(fog_color, "fogColor");
    UNIFORM(fog_start, "fogStart");
    UNIFORM(fog_end, "fogEnd");
    UNIFORM(fog_density, "fogDensity");
    UNIFORM(fog_radial, "radialFog");
#undef UNIFORM
    gl->UseProgram(renderer->program);
    const GLint sampler = gl->GetUniformLocation(renderer->program, "terrain");
    if (sampler < 0)
        return failure(error, MC_CLIENT_DISPLAY_GL_ERROR, "locate World terrain sampler");
    gl->Uniform1i(sampler, 0);
    return check(renderer, "initialize World program", error);
}

mc_client_world_renderer_limits mc_client_world_renderer_default_limits(void) {
    return (mc_client_world_renderer_limits){2048u, 147456u, 512u * 1024u * 1024u};
}

bool mc_client_world_renderer_create(mc_client_display *display, const mc_image_view *terrain,
                                     bool anaglyph, const mc_client_world_renderer_limits *limits,
                                     mc_client_world_renderer **out,
                                     mc_client_display_error *error) {
    if (display == NULL || terrain == NULL || out == NULL)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World renderer admission");
    const mc_client_world_renderer_limits selected =
        limits == NULL ? mc_client_world_renderer_default_limits() : *limits;
    if (selected.max_meshes == 0 || selected.max_meshes > 65536u ||
        selected.max_vertices_per_mesh == 0 || selected.max_vertices_per_mesh > INT_MAX ||
        selected.max_vertex_bytes == 0 || selected.max_vertex_bytes > (size_t)PTRDIFF_MAX)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World renderer limits");
    mc_client_world_renderer *renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL)
        return failure(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "World renderer allocation");
    renderer->display = display;
    renderer->limits = selected;
    if (!mc_client_display_make_current(display, error) || !load_gl(renderer, error) ||
        !program(renderer, error) ||
        !mc_client_display_upload_texture(display, terrain, anaglyph, &renderer->terrain, error)) {
        mc_client_display_error original = error == NULL ? (mc_client_display_error){0} : *error;
        mc_client_world_renderer_retire(&renderer, NULL);
        if (error != NULL)
            *error = original;
        return false;
    }
    *out = renderer;
    return success(error);
}

bool mc_client_world_mesh_create(mc_client_world_renderer *renderer, int32_t origin_x,
                                 int32_t origin_y, int32_t origin_z,
                                 const mc_client_world_vertex *vertices, size_t vertex_count,
                                 mc_client_world_mesh **out, mc_client_display_error *error) {
    if (renderer == NULL || out == NULL || (vertex_count != 0 && vertices == NULL) ||
        vertex_count % 3u != 0)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World mesh admission");
    if (renderer->mesh_count >= renderer->limits.max_meshes ||
        vertex_count > renderer->limits.max_vertices_per_mesh ||
        vertex_count > SIZE_MAX / sizeof(world_vertex))
        return failure(error, MC_CLIENT_DISPLAY_LIMIT, "World mesh limits");
    const size_t bytes = vertex_count * sizeof(world_vertex);
    if (bytes > renderer->limits.max_vertex_bytes - renderer->vertex_bytes)
        return failure(error, MC_CLIENT_DISPLAY_LIMIT, "World vertex byte budget");
    for (size_t i = 0; i < vertex_count; ++i)
        if (!isfinite(vertices[i].x) || !isfinite(vertices[i].y) || !isfinite(vertices[i].z) ||
            !isfinite(vertices[i].u) || !isfinite(vertices[i].v))
            return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World vertex finite values");
    world_vertex *converted = malloc(bytes == 0 ? 1u : bytes);
    mc_client_world_mesh *mesh = calloc(1, sizeof(*mesh));
    if (converted == NULL || mesh == NULL) {
        free(converted);
        free(mesh);
        return failure(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "World mesh allocation");
    }
    mesh->renderer = renderer;
    mesh->x = origin_x;
    mesh->y = origin_y;
    mesh->z = origin_z;
    mesh->count = vertex_count;
    mesh->bytes = bytes;
    for (size_t i = 0; i < vertex_count; ++i) {
        converted[i] = (world_vertex){vertices[i].x, vertices[i].y, vertices[i].z,
                                      vertices[i].u, vertices[i].v, {0}};
        converted[i].color[0] = (GLubyte)(vertices[i].argb >> 16u);
        converted[i].color[1] = (GLubyte)(vertices[i].argb >> 8u);
        converted[i].color[2] = (GLubyte)vertices[i].argb;
        converted[i].color[3] = (GLubyte)(vertices[i].argb >> 24u);
    }
    mc_client_gl *gl = &renderer->gl.base;
    bool uploaded = mc_client_display_make_current(renderer->display, error);
    if (uploaded) {
        gl->GenVertexArrays(1, &mesh->vao);
        gl->GenBuffers(1, &mesh->vbo);
        gl->BindVertexArray(mesh->vao);
        gl->BindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
        gl->BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, converted, GL_STATIC_DRAW);
        gl->EnableVertexAttribArray(0);
        gl->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(world_vertex),
                                (const void *)offsetof(world_vertex, x));
        gl->EnableVertexAttribArray(1);
        gl->VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(world_vertex),
                                (const void *)offsetof(world_vertex, u));
        gl->EnableVertexAttribArray(2);
        gl->VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, (GLsizei)sizeof(world_vertex),
                                (const void *)offsetof(world_vertex, color));
        uploaded = check(renderer, "upload World mesh", error);
    }
    free(converted);
    if (!uploaded) {
        if (mc_client_display_make_current(renderer->display, NULL)) {
            gl->DeleteBuffers(1, &mesh->vbo);
            gl->DeleteVertexArrays(1, &mesh->vao);
        }
        free(mesh);
        return false;
    }
    mesh->next = renderer->meshes;
    if (mesh->next != NULL)
        mesh->next->previous = mesh;
    renderer->meshes = mesh;
    ++renderer->mesh_count;
    renderer->vertex_bytes += bytes;
    *out = mesh;
    return success(error);
}

static bool valid_camera(const mc_client_world_camera *camera) {
    return camera != NULL && isfinite(camera->eye_x) && isfinite(camera->eye_y) &&
           isfinite(camera->eye_z) && isfinite(camera->yaw) && isfinite(camera->pitch) &&
           isfinite(camera->vertical_fov) && camera->vertical_fov > 0.0f &&
           camera->vertical_fov < 180.0f && isfinite(camera->near_clip) &&
           isfinite(camera->far_clip) && camera->near_clip > 0.0f &&
           camera->far_clip > camera->near_clip;
}

static bool valid_fog(const mc_client_world_fog *fog) {
    return fog != NULL && fog->kind >= MC_CLIENT_WORLD_FOG_NONE &&
           fog->kind <= MC_CLIENT_WORLD_FOG_EXPONENTIAL && isfinite(fog->red) &&
           isfinite(fog->green) && isfinite(fog->blue) && isfinite(fog->start) &&
           isfinite(fog->end) && isfinite(fog->density) &&
           (fog->kind != MC_CLIENT_WORLD_FOG_LINEAR || fog->end > fog->start) &&
           (fog->kind != MC_CLIENT_WORLD_FOG_EXPONENTIAL || fog->density >= 0.0f);
}

static bool begin_matrices(mc_client_world_renderer *renderer, const mc_client_world_camera *camera,
                           const float projection[16], const float modelview[16],
                           const mc_client_world_fog *fog, mc_client_display_error *error) {
    if (renderer != NULL)
        renderer->frame_started = false;
    if (renderer == NULL || camera == NULL || !isfinite(camera->eye_x) ||
        !isfinite(camera->eye_y) || !isfinite(camera->eye_z) || !valid_fog(fog))
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World frame admission");
    int width = 0, height = 0;
    if (!mc_client_display_make_current(renderer->display, error) ||
        !mc_client_display_pixel_size(renderer->display, &width, &height, error))
        return false;
    if (width == 0 || height == 0)
        return failure(error, MC_CLIENT_DISPLAY_NOT_DRAWABLE, "World drawable size");
    for (size_t i = 0; i < 16; ++i)
        if (!isfinite(projection[i]) || !isfinite(modelview[i]))
            return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World matrix finite values");
    if (!mc_client_display_begin_scene(renderer->display, fog->red, fog->green, fog->blue, error))
        return false;
    mc_client_gl *gl = &renderer->gl.base;
    gl->Viewport(0, 0, width, height);
    gl->Disable(GL_MULTISAMPLE);
    gl->Disable(GL_FRAMEBUFFER_SRGB);
    gl->Disable(GL_SCISSOR_TEST);
    gl->Disable(GL_STENCIL_TEST);
    gl->Enable(GL_DEPTH_TEST);
    gl->DepthFunc(GL_LEQUAL);
    gl->DepthMask(GL_TRUE);
    gl->Enable(GL_CULL_FACE);
    renderer->gl.CullFace(GL_BACK);
    gl->UseProgram(renderer->program);
    gl->UniformMatrix4fv(renderer->projection, 1, GL_FALSE, projection);
    gl->UniformMatrix4fv(renderer->modelview, 1, GL_FALSE, modelview);
    gl->Uniform1i(renderer->fog_mode, (int)fog->kind);
    gl->Uniform1i(renderer->fog_radial, fog->radial ? 1 : 0);
    renderer->gl.Uniform3f(renderer->fog_color, fog->red, fog->green, fog->blue);
    renderer->gl.Uniform1f(renderer->fog_start, fog->start);
    renderer->gl.Uniform1f(renderer->fog_end, fog->end);
    renderer->gl.Uniform1f(renderer->fog_density, fog->density);
    if (!check(renderer, "begin World frame", error))
        return false;
    renderer->camera = *camera;
    memcpy(renderer->camera_matrix, modelview, 16u * sizeof(*modelview));
    renderer->frame_started = true;
    return success(error);
}

bool mc_client_world_renderer_begin(mc_client_world_renderer *renderer,
                                    const mc_client_world_camera *camera,
                                    const mc_client_world_fog *fog,
                                    mc_client_display_error *error) {
    if (renderer != NULL)
        renderer->frame_started = false;
    if (renderer == NULL || !valid_camera(camera) || !valid_fog(fog))
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World frame admission");
    int width = 0, height = 0;
    if (!mc_client_display_make_current(renderer->display, error) ||
        !mc_client_display_pixel_size(renderer->display, &width, &height, error))
        return false;
    if (width == 0 || height == 0)
        return failure(error, MC_CLIENT_DISPLAY_NOT_DRAWABLE, "World drawable size");
    /* LWJGL2.9.0 Project.gluPerspective(FFFF)V: float divide/multiply/divide,
     * Math double sin/cos narrowed separately, then float matrix arithmetic. */
    const float radians = (camera->vertical_fov / 2.0f * 3.1415927f) / 180.0f;
    const float sine = (float)sin((double)radians);
    const float cotangent = (float)cos((double)radians) / sine;
    const float delta = camera->far_clip - camera->near_clip;
    const float aspect = (float)width / (float)height;
    const float projection[16] = {cotangent / aspect,
                                  0,
                                  0,
                                  0,
                                  0,
                                  cotangent,
                                  0,
                                  0,
                                  0,
                                  0,
                                  -(camera->far_clip + camera->near_clip) / delta,
                                  -1,
                                  0,
                                  0,
                                  -2.0f * camera->near_clip * camera->far_clip / delta,
                                  0};
    /* Ordinary pitch/yaw view. Hurt, bobbing, sleeping, third-person and
     * anaglyph camera composition belong to the EntityRenderer caller. */
    const float pitch = camera->pitch * 0.0174532925199432958f;
    const float yaw = (camera->yaw + 180.0f) * 0.0174532925199432958f;
    const float cp = cosf(pitch), sp = sinf(pitch), cy = cosf(yaw), sy = sinf(yaw);
    const float modelview[16] = {cy, sp * sy,  -cp * sy, 0, 0, cp, sp,    0,
                                 sy, -sp * cy, cp * cy,  0, 0, 0,  -0.1f, 1};
    return begin_matrices(renderer, camera, projection, modelview, fog, error);
}

bool mc_client_world_renderer_begin_first_person(mc_client_world_renderer *renderer,
                                                 const mc_client_camera_frame *frame,
                                                 const mc_client_world_fog *fog,
                                                 mc_client_display_error *error) {
    if (renderer != NULL)
        renderer->frame_started = false;
    if (frame == NULL || !frame->complete)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "Incomplete Player camera frame");
    int width = 0, height = 0;
    if (renderer == NULL ||
        !mc_client_display_pixel_size(renderer->display, &width, &height, error))
        return renderer == NULL
                   ? failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "Player renderer receiver")
                   : false;
    if (width != frame->width || height != frame->height)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "Player camera drawable changed");
    const mc_client_world_camera origin = {
        .eye_x = frame->render_x, .eye_y = frame->render_y, .eye_z = frame->render_z};
    return begin_matrices(renderer, &origin, frame->projection, frame->modelview, fog, error);
}

static void matrix_translate(float matrix[16], float x, float y, float z) {
    for (size_t row = 0; row < 4; ++row) {
        volatile float value = matrix[row] * x;
        value = value + matrix[4u + row] * y;
        value = value + matrix[8u + row] * z;
        value = value + matrix[12u + row];
        matrix[12u + row] = value;
    }
}

bool mc_client_world_renderer_draw(mc_client_world_renderer *renderer,
                                   const mc_client_world_mesh *mesh, bool blend, bool alpha_test,
                                   bool depth_write, mc_client_display_error *error) {
    if (renderer == NULL || mesh == NULL || mesh->renderer != renderer || !renderer->frame_started)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World draw admission");
    if (fabs(renderer->camera.eye_x) > FLT_MAX || fabs(renderer->camera.eye_y) > FLT_MAX ||
        fabs(renderer->camera.eye_z) > FLT_MAX)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World section offset");
    const int32_t clip_x = (int32_t)((uint32_t)mesh->x & 1023u);
    const int32_t clip_z = (int32_t)((uint32_t)mesh->z & 1023u);
    const float x = (float)((int64_t)mesh->x - clip_x) - (float)renderer->camera.eye_x;
    const float y = -(float)renderer->camera.eye_y;
    const float z = (float)((int64_t)mesh->z - clip_z) - (float)renderer->camera.eye_z;
    float modelview[16];
    memcpy(modelview, renderer->camera_matrix, sizeof(modelview));
    /* Preserve RenderList high-origin/camera narrowing, then the section's
     * clipped origin and ordered center dilation. */
    matrix_translate(modelview, x, y, z);
    matrix_translate(modelview, (float)clip_x, (float)mesh->y, (float)clip_z);
    matrix_translate(modelview, -8.0f, -8.0f, -8.0f);
    for (size_t i = 0; i < 12; ++i)
        modelview[i] *= 1.000001f;
    matrix_translate(modelview, 8.0f, 8.0f, 8.0f);
    for (size_t i = 0; i < 16; ++i)
        if (!isfinite(modelview[i]))
            return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World section matrix");
    if (!mc_client_display_make_current(renderer->display, error))
        return false;
    mc_client_gl *gl = &renderer->gl.base;
    gl->UseProgram(renderer->program);
    gl->UniformMatrix4fv(renderer->modelview, 1, GL_FALSE, modelview);
    gl->Uniform1i(renderer->alpha_test, alpha_test ? 1 : 0);
    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindTexture(GL_TEXTURE_2D, renderer->terrain);
    gl->Enable(GL_DEPTH_TEST);
    gl->DepthFunc(GL_LEQUAL);
    gl->DepthMask(depth_write ? GL_TRUE : GL_FALSE);
    gl->Enable(GL_CULL_FACE);
    if (blend) {
        gl->Enable(GL_BLEND);
        gl->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        gl->Disable(GL_BLEND);
    }
    gl->BindVertexArray(mesh->vao);
    gl->DrawArrays(GL_TRIANGLES, 0, (GLsizei)mesh->count);
    return check(renderer, "draw World mesh", error);
}

bool mc_client_world_mesh_retire(mc_client_world_mesh **owner, mc_client_display_error *error) {
    if (owner == NULL)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World mesh retirement");
    mc_client_world_mesh *mesh = *owner;
    if (mesh == NULL)
        return success(error);
    mc_client_world_renderer *renderer = mesh->renderer;
    bool result = mc_client_display_make_current(renderer->display, error);
    if (result) {
        renderer->gl.base.DeleteBuffers(1, &mesh->vbo);
        renderer->gl.base.DeleteVertexArrays(1, &mesh->vao);
        result = check(renderer, "retire World mesh", error);
    }
    if (mesh->previous != NULL)
        mesh->previous->next = mesh->next;
    else
        renderer->meshes = mesh->next;
    if (mesh->next != NULL)
        mesh->next->previous = mesh->previous;
    --renderer->mesh_count;
    renderer->vertex_bytes -= mesh->bytes;
    free(mesh);
    *owner = NULL;
    return result;
}

bool mc_client_world_renderer_retire(mc_client_world_renderer **owner,
                                     mc_client_display_error *error) {
    if (owner == NULL)
        return failure(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "World renderer retirement");
    mc_client_world_renderer *renderer = *owner;
    if (renderer == NULL)
        return success(error);
    bool result = true;
    mc_client_display_error first = {0};
    while (renderer->meshes != NULL) {
        mc_client_world_mesh *mesh = renderer->meshes;
        mc_client_display_error current_error = {0};
        if (!mc_client_world_mesh_retire(&mesh, &current_error) && result) {
            result = false;
            first = current_error;
        }
    }
    if (mc_client_display_make_current(renderer->display, result ? &first : NULL)) {
        mc_client_gl *gl = &renderer->gl.base;
        if (renderer->program != 0)
            gl->DeleteProgram(renderer->program);
        if (renderer->vertex_shader != 0)
            gl->DeleteShader(renderer->vertex_shader);
        if (renderer->fragment_shader != 0)
            gl->DeleteShader(renderer->fragment_shader);
        if (!check(renderer, "retire World renderer", result ? &first : NULL))
            result = false;
    } else {
        result = false;
    }
    free(renderer);
    *owner = NULL;
    if (result)
        return success(error);
    if (error != NULL)
        *error = first;
    return false;
}
