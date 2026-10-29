#include "display.h"
#include "gl_internal.h"
#include "occlusion.h"
#include "sdk_display_internal.h"
#include "sdk_mode_internal.h"
#include "sdk_update.h"
#include "source_internal.h"
#include "startup_surface.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    DISPLAY_WINDOW_AXIS = 16384,
    DISPLAY_WINDOW_PIXELS = 16 * 1024 * 1024,
    DISPLAY_TEXTURES = 128,
    DISPLAY_TEXTURE_BYTES = 64 * 1024 * 1024,
    DISPLAY_TOTAL_TEXTURE_BYTES = 128 * 1024 * 1024,
    DISPLAY_CAPTURE_BYTES = 64 * 1024 * 1024
};

typedef struct display_texture {
    GLuint id;
    size_t bytes;
    mc_client_texture_resource *resource;
} display_texture;

struct mc_client_texture_resource {
    mc_client_display *display;
    GLuint id;
    bool active;
    uint64_t generation;
    bool live, driver_generated, deletion_right;
};

typedef struct display_vertex {
    GLfloat x, y, u, v;
    GLubyte color[4];
} display_vertex;

typedef struct display_occlusion_capability {
    uint64_t generation;
    bool initialized, advertised, supported;
    PFNGLGENQUERIESARBPROC GenQueries;
    PFNGLDELETEQUERIESARBPROC DeleteQueries;
    PFNGLISQUERYARBPROC IsQuery;
    PFNGLBEGINQUERYARBPROC BeginQuery;
    PFNGLENDQUERYARBPROC EndQuery;
    PFNGLGETQUERYIVARBPROC GetQueryiv;
    PFNGLGETQUERYOBJECTIVARBPROC GetQueryObjectiv;
    PFNGLGETQUERYOBJECTUIVARBPROC GetQueryObjectuiv;
} display_occlusion_capability;

struct mc_client_display {
    SDL_Window *window;
    SDL_GLContext context;
    bool video, functions_ready, context_lost, frame_started, scene_started;
    mc_client_gl gl;
    mc_client_display_info info;
    GLuint vertex_shader, fragment_shader, program, vao, vbo;
    GLint textured, alpha_test, projection_scale;
    GLint model_enabled, projection_matrix, modelview_matrix;
    GLint max_texture_size;
    double exact_width, exact_height;
    display_texture textures[DISPLAY_TEXTURES];
    size_t texture_count, texture_bytes;
    size_t texture_resource_count, texture_resource_calls;
    mc_client_source_state source;
    bool native_retiring, native_cleanup_issued, sdk_profile, stage_active, class_initialized,
        window_created;
    mc_client_display_phase stage_phase;
    mc_client_display_mode *initial_mode, *current_mode;
    mc_player_string *title, *empty_title;
    mc_client_sdk_icons *icons;
    mc_client_sdk_window_icons *window_icons;
    mc_client_sdk_input *inputs;
    mc_client_sdk_update *updates;
    mc_client_event_port *events;
    int32_t source_last_store, native_width, native_height;
    mc_client_display_stage_error stage_failure;
    uint64_t native_context_generation;
    display_occlusion_capability occlusion;
};

/* The real source SDK is process-wide and admits one window. Diagnostic
 * Displays still share the same SDL transport, without invented SDK flags. */
static mc_client_display *sdk_namespace;

static bool source_generation_leased(const mc_client_display *, uint64_t);
static void source_gpu_retire(mc_client_display *, bool *, mc_client_display_error *);

static void text_copy(char *out, size_t capacity, const char *text) {
    if (capacity == 0)
        return;
    size_t length = 0;
    if (text != NULL)
        while (length + 1 < capacity && text[length] != '\0') {
            out[length] = text[length];
            ++length;
        }
    out[length] = '\0';
}

static bool fail(mc_client_display_error *error, mc_client_display_status status,
                 const char *operation, const char *detail, GLenum gl_error) {
    if (error != NULL) {
        *error = (mc_client_display_error){
            .status = status, .operation = operation, .gl_error = gl_error};
        text_copy(error->detail, sizeof(error->detail), detail);
    }
    return false;
}

static bool success(mc_client_display_error *error) {
    if (error != NULL)
        *error = (mc_client_display_error){0};
    return true;
}

static bool product(size_t first, size_t second, size_t *out) {
    if (first != 0 && second > SIZE_MAX / first)
        return false;
    *out = first * second;
    return true;
}

static bool sdl_failure(mc_client_display_error *error, const char *operation) {
    return fail(error, MC_CLIENT_DISPLAY_SDL_ERROR, operation, SDL_GetError(), 0);
}

static bool gl_check(mc_client_display *display, const char *operation,
                     mc_client_display_error *error) {
    if (display->context_lost)
        return fail(error, MC_CLIENT_DISPLAY_CONTEXT_LOST, operation, "OpenGL context was lost",
                    GL_CONTEXT_LOST);
    GLenum first = GL_NO_ERROR;
    /* There are finitely many GL error flags. Bound draining even if a broken
     * driver repeats one, and stop querying as soon as loss is detected. */
    for (unsigned int count = 0; count < 16; ++count) {
        const GLenum value = display->gl.GetError();
        if (value == GL_NO_ERROR)
            break;
        if (first == GL_NO_ERROR)
            first = value;
        if (value == GL_CONTEXT_LOST) {
            display->context_lost = true;
            break;
        }
    }
    if (first != GL_NO_ERROR)
        return fail(
            error,
            display->context_lost ? MC_CLIENT_DISPLAY_CONTEXT_LOST : MC_CLIENT_DISPLAY_GL_ERROR,
            operation, display->context_lost ? "OpenGL context was lost" : "OpenGL error", first);
    return true;
}

static bool current_internal(mc_client_display *display, const char *operation,
                             mc_client_display_error *error, bool cleanup) {
    if (display == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation, "null display", 0);
    if (!cleanup && (display->native_retiring ||
                     (display->sdk_profile && mc_client_display_sdk_operation_busy(display)) ||
                     (display->sdk_profile && display->stage_phase != MC_CLIENT_DISPLAY_READY)))
        return fail(error, MC_CLIENT_DISPLAY_BUSY, operation,
                    "native Display construction/retirement active", 0);
    if (display->context_lost)
        return fail(error, MC_CLIENT_DISPLAY_CONTEXT_LOST, operation, "OpenGL context was lost",
                    GL_CONTEXT_LOST);
    if (display->context == NULL || display->window == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation, "incomplete display", 0);
    if ((SDL_GL_GetCurrentContext() != display->context ||
         SDL_GL_GetCurrentWindow() != display->window) &&
        !SDL_GL_MakeCurrent(display->window, display->context))
        return sdl_failure(error, operation);
    return true;
}

static bool current(mc_client_display *display, const char *operation,
                    mc_client_display_error *error) {
    return current_internal(display, operation, error, false);
}

bool mc_client_display_make_current(mc_client_display *display, mc_client_display_error *error) {
    if (!current(display, "make client renderer context current", error))
        return false;
    return success(error);
}

bool mc_client_display_check_gl(mc_client_display *display, const char *operation,
                                mc_client_display_error *error) {
    if (operation == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "check client renderer GL errors",
                    "null operation", 0);
    if (!current(display, operation, error) || !gl_check(display, operation, error)) {
        if (display != NULL)
            display->frame_started = display->scene_started = false;
        return false;
    }
    return success(error);
}

static bool load_gl(mc_client_display *display, mc_client_display_error *error) {
#define MC_CLIENT_GL_LOAD(type, field, name)                                                       \
    display->gl.field = (type)SDL_GL_GetProcAddress(name);                                         \
    if (display->gl.field == NULL)                                                                 \
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "load OpenGL function", name, 0);
    MC_CLIENT_GL_FUNCTIONS(MC_CLIENT_GL_LOAD)
#undef MC_CLIENT_GL_LOAD
    display->functions_ready = true;
    return gl_check(display, "load OpenGL functions", error);
}

static bool shader(mc_client_display *display, GLenum kind, const char *source, GLuint *out,
                   mc_client_display_error *error) {
    *out = display->gl.CreateShader(kind);
    if (!gl_check(display, "create GUI shader", error))
        return false;
    if (*out == 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "create GUI shader", "zero shader ID", 0);
    display->gl.ShaderSource(*out, 1, &source, NULL);
    display->gl.CompileShader(*out);
    GLint compiled = GL_FALSE;
    display->gl.GetShaderiv(*out, GL_COMPILE_STATUS, &compiled);
    if (!gl_check(display, "compile GUI shader", error))
        return false;
    if (compiled != GL_TRUE) {
        char log[256] = {0};
        display->gl.GetShaderInfoLog(*out, (GLsizei)sizeof(log), NULL, log);
        log[sizeof(log) - 1] = '\0';
        if (!gl_check(display, "read GUI shader log", error))
            return false;
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "compile GUI shader", log, 0);
    }
    return true;
}

static bool renderer_create(mc_client_display *display, mc_client_display_error *error) {
    static const char vertex_source[] =
        "#version 330 core\n"
        "layout(location=0) in vec2 position;\n"
        "layout(location=1) in vec2 uv;\n"
        "layout(location=2) in vec4 color;\n"
        "out vec2 fragmentUV; out vec4 fragmentColor;\n"
        "uniform vec2 projectionScale;\n"
        "uniform mat4 projectionMatrix; uniform mat4 modelviewMatrix;\n"
        "uniform int modelEnabled;\n"
        "void main(){if(modelEnabled!=0){"
        "vec4 eye=modelviewMatrix*vec4(position,0.0,1.0);"
        "gl_Position=projectionMatrix*eye;}else{"
        "gl_Position=vec4(position*projectionScale+"
        "vec2(-1.0,1.0),0.0,1.0);}"
        "fragmentUV=uv;fragmentColor=color;}\n";
    static const char fragment_source[] =
        "#version 330 core\n"
        "in vec2 fragmentUV; in vec4 fragmentColor; layout(location=0) out vec4 outputColor;\n"
        "uniform sampler2D guiTexture; uniform int textured; uniform int alphaTest;\n"
        "void main(){vec4 value=fragmentColor;"
        "if(textured!=0)value*=texture(guiTexture,fragmentUV);"
        "if(alphaTest!=0 && value.a<=0.1)discard;outputColor=value;}\n";
    if (!shader(display, GL_VERTEX_SHADER, vertex_source, &display->vertex_shader, error) ||
        !shader(display, GL_FRAGMENT_SHADER, fragment_source, &display->fragment_shader, error))
        return false;
    display->program = display->gl.CreateProgram();
    if (!gl_check(display, "create GUI program", error))
        return false;
    if (display->program == 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "create GUI program", "zero program ID", 0);
    display->gl.AttachShader(display->program, display->vertex_shader);
    display->gl.AttachShader(display->program, display->fragment_shader);
    display->gl.LinkProgram(display->program);
    GLint linked = GL_FALSE;
    display->gl.GetProgramiv(display->program, GL_LINK_STATUS, &linked);
    if (!gl_check(display, "link GUI program", error))
        return false;
    if (linked != GL_TRUE) {
        char log[256] = {0};
        display->gl.GetProgramInfoLog(display->program, (GLsizei)sizeof(log), NULL, log);
        log[sizeof(log) - 1] = '\0';
        if (!gl_check(display, "read GUI program log", error))
            return false;
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "link GUI program", log, 0);
    }
    display->gl.UseProgram(display->program);
    const GLint sampler = display->gl.GetUniformLocation(display->program, "guiTexture");
    display->textured = display->gl.GetUniformLocation(display->program, "textured");
    display->alpha_test = display->gl.GetUniformLocation(display->program, "alphaTest");
    display->projection_scale = display->gl.GetUniformLocation(display->program, "projectionScale");
    display->model_enabled = display->gl.GetUniformLocation(display->program, "modelEnabled");
    display->projection_matrix =
        display->gl.GetUniformLocation(display->program, "projectionMatrix");
    display->modelview_matrix = display->gl.GetUniformLocation(display->program, "modelviewMatrix");
    if (!gl_check(display, "locate GUI uniforms", error))
        return false;
    if (sampler < 0 || display->textured < 0 || display->alpha_test < 0 ||
        display->projection_scale < 0 || display->model_enabled < 0 ||
        display->projection_matrix < 0 || display->modelview_matrix < 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "locate GUI uniforms", "missing uniform", 0);
    display->gl.Uniform1i(sampler, 0);
    display->gl.GenVertexArrays(1, &display->vao);
    display->gl.GenBuffers(1, &display->vbo);
    if (!gl_check(display, "create GUI buffers", error))
        return false;
    if (display->vao == 0 || display->vbo == 0)
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "create GUI buffers", "zero buffer ID", 0);
    display->gl.BindVertexArray(display->vao);
    display->gl.BindBuffer(GL_ARRAY_BUFFER, display->vbo);
    display->gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(6u * sizeof(display_vertex)), NULL,
                           GL_STREAM_DRAW);
    display->gl.EnableVertexAttribArray(0);
    display->gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(display_vertex),
                                    (const void *)(uintptr_t)offsetof(display_vertex, x));
    display->gl.EnableVertexAttribArray(1);
    display->gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(display_vertex),
                                    (const void *)(uintptr_t)offsetof(display_vertex, u));
    display->gl.EnableVertexAttribArray(2);
    display->gl.VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                                    (GLsizei)sizeof(display_vertex),
                                    (const void *)(uintptr_t)offsetof(display_vertex, color));
    return gl_check(display, "initialize GUI buffers", error);
}

static bool context_check(mc_client_display *display, mc_client_display_error *error) {
    GLint major = 0, minor = 0, profile = 0, samples = 0;
    display->gl.GetIntegerv(GL_MAJOR_VERSION, &major);
    display->gl.GetIntegerv(GL_MINOR_VERSION, &minor);
    display->gl.GetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    display->gl.GetIntegerv(GL_SAMPLE_BUFFERS, &samples);
    display->gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, &display->max_texture_size);
    if (!gl_check(display, "query effective OpenGL context", error))
        return false;
    if (major < 3 || (major == 3 && minor < 3) || (profile & GL_CONTEXT_CORE_PROFILE_BIT) == 0 ||
        samples != 0 || display->max_texture_size <= 0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "validate effective OpenGL context",
                    "requires OpenGL 3.3 core without multisampling", 0);
    int depth = 0, double_buffered = 0;
    if (!SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth) ||
        !SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &double_buffered))
        return sdl_failure(error, "query effective framebuffer attributes");
    if (depth <= 0 || double_buffered != 1)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "validate effective framebuffer",
                    "requires a double-buffered depth framebuffer", 0);
    display->info.gl_major = major;
    display->info.gl_minor = minor;
    display->info.profile = profile;
    text_copy(display->info.vendor, sizeof(display->info.vendor),
              (const char *)display->gl.GetString(GL_VENDOR));
    text_copy(display->info.renderer, sizeof(display->info.renderer),
              (const char *)display->gl.GetString(GL_RENDERER));
    text_copy(display->info.version, sizeof(display->info.version),
              (const char *)display->gl.GetString(GL_VERSION));
    return gl_check(display, "query OpenGL implementation strings", error);
}

static bool native_create_window(mc_client_display *display, const char *title, int width,
                                 int height, bool hidden, mc_client_display_error *error) {
    if (!display->video && !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        sdl_failure(error, "initialize SDL video");
        return false;
    }
    display->video = true;
    SDL_GL_ResetAttributes();

    const struct {
        SDL_GLAttr attribute;
        int value;
    } attributes[] = {{SDL_GL_CONTEXT_MAJOR_VERSION, 3},
                      {SDL_GL_CONTEXT_MINOR_VERSION, 3},
                      {SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE},
                      {SDL_GL_RED_SIZE, 8},
                      {SDL_GL_GREEN_SIZE, 8},
                      {SDL_GL_BLUE_SIZE, 8},
                      {SDL_GL_ALPHA_SIZE, 8},
                      {SDL_GL_DEPTH_SIZE, 16},
                      {SDL_GL_STENCIL_SIZE, 0},
                      {SDL_GL_DOUBLEBUFFER, 1},
                      {SDL_GL_MULTISAMPLEBUFFERS, 0},
                      {SDL_GL_MULTISAMPLESAMPLES, 0},
                      {SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0}};

    for (size_t i = 0; i < sizeof(attributes) / sizeof(attributes[0]); ++i)
        if (!SDL_GL_SetAttribute(attributes[i].attribute, attributes[i].value)) {
            sdl_failure(error, "set OpenGL context attribute");
            return false;
        }
    const SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_HIGH_PIXEL_DENSITY | (hidden ? SDL_WINDOW_HIDDEN : 0);
    display->window = SDL_CreateWindow(title, width, height, flags);
    if (display->window == NULL) {
        sdl_failure(error, "create SDL window");
        return false;
    }
    if (!SDL_SetWindowPosition(display->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED)) {
        sdl_failure(error, "center SDL window");
        return false;
    }
    mc_client_events_error events = {0};
    if (mc_client_events_attach(display->window, 0, &display->events, &events) !=
        MC_CLIENT_EVENTS_OK)
        return fail(error,
                    events.status == MC_CLIENT_EVENTS_ALLOCATION_FAILED
                        ? MC_CLIENT_DISPLAY_ALLOCATION_FAILED
                    : events.status == MC_CLIENT_EVENTS_LIMIT ? MC_CLIENT_DISPLAY_LIMIT
                                                              : MC_CLIENT_DISPLAY_SDL_ERROR,
                    events.operation, events.detail, 0);
    display->native_width = width;
    display->native_height = height;
    return true;
}

static bool native_create_context(mc_client_display *display, mc_client_display_error *error) {
    if (display->native_context_generation == UINT64_MAX)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "create OpenGL context",
                    "native context generation limit", 0);
    display->context = SDL_GL_CreateContext(display->window);
    if (display->context == NULL) {
        sdl_failure(error, "create OpenGL context");
        return false;
    }
    ++display->native_context_generation;
    if (!load_gl(display, error) || !context_check(display, error) ||
        !renderer_create(display, error))
        return false;
    display->info.swap_interval_zero = SDL_GL_SetSwapInterval(0);
    return true;
}

bool mc_client_display_create(const char *title, int width, int height, bool hidden,
                              mc_client_display **out, mc_client_display_error *error) {
    if (title == NULL || title[0] == '\0' || width <= 0 || height <= 0 || out == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "create display", "invalid argument",
                    0);
    size_t requested_pixels;
    if (width > DISPLAY_WINDOW_AXIS || height > DISPLAY_WINDOW_AXIS ||
        !product((size_t)width, (size_t)height, &requested_pixels) ||
        requested_pixels > DISPLAY_WINDOW_PIXELS)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "create display", "window request limit", 0);
    mc_client_display *display = calloc(1, sizeof(*display));
    if (display == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "create display",
                    "allocation failed", 0);
    if (!native_create_window(display, title, width, height, hidden, error) ||
        !native_create_context(display, error))
        goto failed;
    *out = display;
    return success(error);
failed:
    mc_client_display_destroy(display, NULL);
    return false;
}

SDL_Window *mc_client_display_window(mc_client_display *display) {
    return display == NULL ? NULL : display->window;
}

const mc_client_display_info *mc_client_display_info_get(const mc_client_display *display) {
    return display == NULL ? NULL : &display->info;
}

static GLint attachment_value(mc_client_display *display, GLenum attachment, GLenum name) {
    GLint type = GL_NONE, value = 0;
    display->gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment,
                                                    GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type != GL_NONE)
        display->gl.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, name,
                                                        &value);
    return value;
}

bool mc_client_display_framebuffer_info_get(mc_client_display *display,
                                            mc_client_framebuffer_info *out,
                                            mc_client_display_error *error) {
    if (out == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "query effective framebuffer",
                    "null output", 0);
    if (!current(display, "query effective framebuffer", error))
        return false;
    if (!display->functions_ready)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "query effective framebuffer",
                    "incomplete function table", 0);
    if (!gl_check(display, "before effective framebuffer query", error))
        return false;
    GLint draw_binding = 0, read_binding = 0;
    display->gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_binding);
    display->gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_binding);
    if (!gl_check(display, "query framebuffer bindings", error))
        return false;
    if (draw_binding != 0 || read_binding != 0)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "query effective framebuffer",
                    "default draw/read framebuffer required", 0);
    GLint double_buffer = 0, stereo = 0, srgb = 0, sample_buffers = 0, samples = 0;
    GLint draw_buffer = 0, read_buffer = 0;
    display->gl.GetIntegerv(GL_DOUBLEBUFFER, &double_buffer);
    display->gl.GetIntegerv(GL_STEREO, &stereo);
    display->gl.GetIntegerv(GL_FRAMEBUFFER_SRGB, &srgb);
    display->gl.GetIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
    display->gl.GetIntegerv(GL_SAMPLES, &samples);
    display->gl.GetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
    display->gl.GetIntegerv(GL_READ_BUFFER, &read_buffer);
    const GLenum color = double_buffer != 0 ? GL_BACK_LEFT : GL_FRONT_LEFT;
    const mc_client_framebuffer_info value = {
        .red_bits = attachment_value(display, color, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE),
        .green_bits = attachment_value(display, color, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE),
        .blue_bits = attachment_value(display, color, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE),
        .alpha_bits = attachment_value(display, color, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE),
        .depth_bits = attachment_value(display, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE),
        .stencil_bits =
            attachment_value(display, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE),
        .double_buffer = double_buffer != 0,
        .stereo = stereo != 0,
        .srgb_enabled = srgb != 0,
        .sample_buffers = sample_buffers,
        .samples = samples,
        .color_attachment = (uint32_t)color,
        .color_encoding =
            (uint32_t)attachment_value(display, color, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING),
        .draw_buffer = (uint32_t)draw_buffer,
        .read_buffer = (uint32_t)read_buffer};
    if (!gl_check(display, "query effective framebuffer", error))
        return false;
    *out = value;
    return success(error);
}

bool mc_client_display_pixel_size(mc_client_display *display, int *width, int *height,
                                  mc_client_display_error *error) {
    if (display == NULL || width == NULL || height == NULL || width == height)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "query drawable size",
                    "invalid argument", 0);
    int actual_width = 0, actual_height = 0;
    if (!SDL_GetWindowSizeInPixels(display->window, &actual_width, &actual_height))
        return sdl_failure(error, "query drawable size");
    if (actual_width < 0 || actual_height < 0)
        return fail(error, MC_CLIENT_DISPLAY_SDL_ERROR, "query drawable size",
                    "negative drawable extent", 0);
    *width = actual_width;
    *height = actual_height;
    return success(error);
}

bool mc_client_display_begin_scene(mc_client_display *display, float red, float green, float blue,
                                   mc_client_display_error *error) {
    if (display != NULL)
        display->frame_started = display->scene_started = false;
    if (!isfinite(red) || !isfinite(green) || !isfinite(blue))
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "begin scene frame",
                    "nonfinite clear color", 0);
    if (!current(display, "begin scene frame", error))
        return false;
    int width = 0, height = 0;
    if (!mc_client_display_pixel_size(display, &width, &height, error))
        return false;
    if (width == 0 || height == 0)
        return fail(error, MC_CLIENT_DISPLAY_NOT_DRAWABLE, "begin scene frame", "empty drawable",
                    0);
    display->gl.Viewport(0, 0, width, height);
    display->gl.Disable(GL_SCISSOR_TEST);
    display->gl.DepthMask(GL_TRUE);
    display->gl.ClearColor(red, green, blue, 1.0f);
    display->gl.ClearDepth(1.0);
    display->gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!gl_check(display, "begin scene frame", error))
        return false;
    display->scene_started = true;
    return success(error);
}

static bool texture_known(const mc_client_display *display, mc_client_texture texture) {
    for (size_t i = 0; i < display->texture_count; ++i)
        if (display->textures[i].id == texture)
            return true;
    return false;
}

bool mc_client_display_upload_texture(mc_client_display *display, const mc_image_view *image,
                                      bool anaglyph, mc_client_texture *out,
                                      mc_client_display_error *error) {
    if (display == NULL || image == NULL || out == NULL || image->pixels == NULL ||
        image->width == 0 || image->height == 0 || image->width > INT32_MAX ||
        image->height > INT32_MAX)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "upload texture",
                    "invalid RGBA view", 0);
    if (!current(display, "upload texture", error))
        return false;
    size_t stride, bytes;
    if (!product((size_t)image->width, 4u, &stride) ||
        !product(stride, (size_t)image->height, &bytes))
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "upload texture", "RGBA byte overflow", 0);
    if (image->stride != stride)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "upload texture",
                    "nonpacked RGBA view", 0);
    if (image->width > (uint32_t)display->max_texture_size ||
        image->height > (uint32_t)display->max_texture_size || bytes > DISPLAY_TEXTURE_BYTES ||
        display->texture_count >= DISPLAY_TEXTURES ||
        bytes > DISPLAY_TOTAL_TEXTURE_BYTES - display->texture_bytes)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "upload texture", "texture limit", 0);
    uint8_t *converted = NULL;
    const uint8_t *pixels = image->pixels;
    if (anaglyph) {
        converted = malloc(bytes);
        if (converted == NULL)
            return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "convert anaglyph texture",
                        "allocation failed", 0);
        for (size_t i = 0; i < bytes; i += 4) {
            const unsigned int red = pixels[i], green = pixels[i + 1], blue = pixels[i + 2];
            converted[i] = (uint8_t)((red * 30u + green * 59u + blue * 11u) / 100u);
            converted[i + 1] = (uint8_t)((red * 30u + green * 70u) / 100u);
            converted[i + 2] = (uint8_t)((red * 30u + blue * 70u) / 100u);
            converted[i + 3] = pixels[i + 3];
        }
        pixels = converted;
    }
    GLuint texture = 0;
    display->gl.GenTextures(1, &texture);
    if (!gl_check(display, "generate texture", error)) {
        if (texture != 0 && !display->context_lost) {
            display->gl.DeleteTextures(1, &texture);
            gl_check(display, "retire failed texture ID", NULL);
        }
        free(converted);
        return false;
    }
    if (texture == 0) {
        free(converted);
        return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "generate texture", "zero texture ID", 0);
    }
    display->gl.ActiveTexture(GL_TEXTURE0);
    display->gl.BindTexture(GL_TEXTURE_2D, texture);
    display->gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    display->gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    display->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    display->gl.PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    display->gl.PixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    display->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    display->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    display->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    display->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    display->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    display->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    display->gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)image->width, (GLsizei)image->height,
                           0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    free(converted);
    if (!gl_check(display, "upload RGBA texture", error)) {
        if (!display->context_lost) {
            display->gl.DeleteTextures(1, &texture);
            gl_check(display, "retire failed texture", NULL);
        }
        return false;
    }
    display->textures[display->texture_count++] = (display_texture){.id = texture, .bytes = bytes};
    display->texture_bytes += bytes;
    *out = texture;
    return success(error);
}

static display_texture *resource_slot(mc_client_texture_resource *resource) {
    if (resource == NULL || resource->display == NULL)
        return NULL;
    mc_client_display *display = resource->display;
    for (size_t i = 0; i < display->texture_count; ++i)
        if (display->textures[i].resource == resource)
            return &display->textures[i];
    return NULL;
}

static bool resource_enter(mc_client_texture_resource *resource, const char *operation,
                           mc_client_display_error *error) {
    if (resource_slot(resource) == NULL || resource->id == 0)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                    "no generated texture resource", 0);
    mc_client_display *display = resource->display;
    if (resource->active || display->texture_resource_calls != 0 ||
        (display->source.active_calls != 0 && !display->source.internal_resource_bind))
        return fail(error, MC_CLIENT_DISPLAY_BUSY, operation, "texture operation active", 0);
    resource->active = true;
    ++display->texture_resource_calls;
    if (!current(display, operation, error)) {
        --display->texture_resource_calls;
        resource->active = false;
        return false;
    }
    return true;
}

static void resource_leave(mc_client_texture_resource *resource) {
    --resource->display->texture_resource_calls;
    resource->active = false;
}

static bool resource_bound(mc_client_texture_resource *resource, const char *operation,
                           mc_client_display_error *error) {
    mc_client_display *display = resource->display;
    GLint bound = 0;
    display->gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    if (!gl_check(display, operation, error))
        return false;
    if ((GLuint)bound != resource->id)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                    "texture resource is not bound", 0);
    return true;
}

bool mc_client_display_generate_texture(mc_client_display *display,
                                        mc_client_texture *buffer_element,
                                        mc_client_texture_generation *out,
                                        mc_client_display_error *error) {
    if (display == NULL || buffer_element == NULL || out == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "generate owned texture",
                    "invalid generation arguments", 0);
    *out = (mc_client_texture_generation){0};
    if (display->texture_resource_calls != 0 || display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "generate owned texture",
                    "texture operation active", 0);
    if (!current(display, "generate owned texture", error))
        return false;
    if (display->texture_count >= DISPLAY_TEXTURES)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "generate owned texture", "texture limit", 0);
    if (display->source.next_texture_generation == UINT64_MAX)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "generate owned texture",
                    "generation identity exhausted", 0);
    mc_client_texture_resource *resource = calloc(1, sizeof(*resource));
    if (resource == NULL)
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "reserve texture resource",
                    "allocation failed", 0);
    resource->display = display;
    resource->active = true;
    resource->generation = ++display->source.next_texture_generation;
    const size_t index = display->texture_count++;
    display->textures[index] = (display_texture){.resource = resource};
    ++display->texture_resource_count;
    ++display->texture_resource_calls;
    out->resource = resource;
    /* The actual source IntBuffer storage is the driver's output location.
     * A nonzero untracked value is retained conservatively even on GL error. */
    display->gl.GenTextures(1, buffer_element);
    out->buffer_value = *buffer_element;
    const bool duplicate = out->buffer_value != 0 && texture_known(display, out->buffer_value);
    if (!duplicate) {
        resource->id = out->buffer_value;
        display->textures[index].id = resource->id;
    }
    bool ok = gl_check(display, "generate owned texture", error);
    if (ok && (resource->id == 0 || duplicate))
        ok = fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "generate owned texture",
                  duplicate ? "duplicate generated texture ID" : "zero generated texture ID", 0);
    out->driver_generated = ok;
    resource->driver_generated = ok;
    resource->live = ok;
    resource->deletion_right = resource->id != 0;
    resource_leave(resource);
    return ok ? success(error) : false;
}

bool mc_client_display_bind_texture(mc_client_texture_resource *resource,
                                    mc_client_display_error *error) {
    if (!resource_enter(resource, "bind owned texture", error))
        return false;
    mc_client_display *display = resource->display;
    display->gl.BindTexture(GL_TEXTURE_2D, resource->id);
    const bool ok = gl_check(display, "bind owned texture", error);
    resource_leave(resource);
    return ok ? success(error) : false;
}

bool mc_client_display_texture_parameter(mc_client_texture_resource *resource,
                                         mc_client_texture_parameter parameter, int32_t value,
                                         mc_client_display_error *error) {
    if ((unsigned int)parameter > (unsigned int)MC_CLIENT_TEXTURE_WRAP_T)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "owned texture parameter",
                    "invalid parameter selector", 0);
    if (!resource_enter(resource, "owned texture parameter", error))
        return false;
    const GLenum names[] = {GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_S,
                            GL_TEXTURE_WRAP_T};
    mc_client_display *display = resource->display;
    bool ok = resource_bound(resource, "owned texture parameter binding", error);
    if (ok && (parameter == MC_CLIENT_TEXTURE_WRAP_S || parameter == MC_CLIENT_TEXTURE_WRAP_T) &&
        value == GL_CLAMP)
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "legacy GL_CLAMP texture parameter",
                  "source legacy clamp requires a compatible texture sampling implementation", 0);
    if (ok) {
        display->gl.TexParameteri(GL_TEXTURE_2D, names[(unsigned int)parameter], value);
        ok = gl_check(display, "owned texture parameter", error);
    }
    resource_leave(resource);
    return ok ? success(error) : false;
}

bool mc_client_display_upload_existing_texture(mc_client_texture_resource *resource,
                                               const mc_image_view *image,
                                               mc_client_display_error *error) {
    if (image == NULL || image->pixels == NULL || image->width == 0 || image->height == 0 ||
        image->width > INT32_MAX || image->height > INT32_MAX)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "upload owned texture",
                    "invalid RGBA view", 0);
    size_t stride, bytes;
    if (!product((size_t)image->width, 4u, &stride) ||
        !product(stride, (size_t)image->height, &bytes) || image->stride != stride)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "upload owned texture",
                    "invalid packed RGBA extent", 0);
    if (!resource_enter(resource, "upload owned texture", error))
        return false;
    mc_client_display *display = resource->display;
    display_texture *slot = resource_slot(resource);
    bool ok = resource_bound(resource, "owned texture upload binding", error);
    if (ok &&
        (image->width > (uint32_t)display->max_texture_size ||
         image->height > (uint32_t)display->max_texture_size || bytes > DISPLAY_TEXTURE_BYTES ||
         (bytes > slot->bytes &&
          bytes - slot->bytes > DISPLAY_TOTAL_TEXTURE_BYTES - display->texture_bytes)))
        ok = fail(error, MC_CLIENT_DISPLAY_LIMIT, "upload owned texture", "texture limit", 0);
    if (ok) {
        GLint pbo = 0, alignment = 0, row_length = 0, skip_pixels = 0, skip_rows = 0;
        display->gl.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pbo);
        display->gl.GetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        display->gl.GetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
        display->gl.GetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
        display->gl.GetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
        ok = gl_check(display, "owned texture unpack state", error);
        if (ok && (pbo != 0 || row_length != 0 || skip_pixels != 0 || skip_rows != 0 ||
                   alignment <= 0 || stride % (size_t)alignment != 0))
            ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "owned texture unpack state",
                      "source upload requires tightly packed client memory and disabled unpack PBO",
                      0);
    }
    if (ok) {
        /* Reserve the larger raw-input amount before issuing GL. On native
         * failure keep that conservative charge until explicit retirement. */
        if (bytes > slot->bytes) {
            display->texture_bytes += bytes - slot->bytes;
            slot->bytes = bytes;
        }
        display->gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)image->width,
                               (GLsizei)image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
        ok = gl_check(display, "upload owned texture", error);
        if (ok && bytes < slot->bytes) {
            display->texture_bytes -= slot->bytes - bytes;
            slot->bytes = bytes;
        }
    }
    resource_leave(resource);
    return ok ? success(error) : false;
}

bool mc_client_display_retire_texture(mc_client_texture_resource **owner,
                                      mc_client_display_error *error) {
    if (owner == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "retire owned texture",
                    "null owner pointer", 0);
    mc_client_texture_resource *resource = *owner;
    if (resource == NULL)
        return success(error);
    display_texture *slot = resource_slot(resource);
    if (slot == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "retire owned texture",
                    "unregistered resource", 0);
    mc_client_display *display = resource->display;
    if (resource->active || display->texture_resource_calls != 0 ||
        display->source.active_calls != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "retire owned texture",
                    "texture operation active", 0);
    if (source_generation_leased(display, resource->generation))
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "retire owned texture",
                    "font generation lease remains live", 0);
    bool ok = true;
    if (display->context_lost)
        ok = fail(error, MC_CLIENT_DISPLAY_CONTEXT_LOST, "retire owned texture",
                  "context destruction retires remaining driver state", GL_CONTEXT_LOST);
    else {
        if (!current(display, "retire owned texture", error))
            return false; /* Preserve the owner for retry. */
        resource->active = true;
        ++display->texture_resource_calls;
        if (resource->id != 0 && resource->deletion_right) {
            display->gl.DeleteTextures(1, &resource->id);
            ok = gl_check(display, "retire owned texture", error);
        }
        resource_leave(resource);
    }
    const size_t index = (size_t)(slot - display->textures);
    display->texture_bytes -= slot->bytes;
    --display->texture_resource_count;
    --display->texture_count;
    memmove(&display->textures[index], &display->textures[index + 1],
            (display->texture_count - index) * sizeof(display->textures[0]));
    memset(&display->textures[display->texture_count], 0, sizeof(display->textures[0]));
    *owner = NULL;
    free(resource);
    return ok ? success(error) : false;
}

bool mc_client_display_begin(mc_client_display *display, double exact_width, double exact_height,
                             mc_client_display_error *error) {
    if (display != NULL) {
        display->frame_started = false;
        display->scene_started = false;
    }
    if (!isfinite(exact_width) || !isfinite(exact_height) || exact_width <= 0 || exact_height <= 0)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "begin GUI frame",
                    "invalid scaled dimensions", 0);
    const double projection_x = 2.0 / exact_width, projection_y = -2.0 / exact_height;
    if (!isfinite(projection_x) || !isfinite(projection_y) || fabs(projection_x) > FLT_MAX ||
        fabs(projection_y) > FLT_MAX)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "begin GUI frame",
                    "excessive binary32 projection coefficient", 0);
    if (!current(display, "begin GUI frame", error))
        return false;
    int width = 0, height = 0;
    if (!mc_client_display_pixel_size(display, &width, &height, error))
        return false;
    if (width == 0 || height == 0)
        return fail(error, MC_CLIENT_DISPLAY_NOT_DRAWABLE, "begin GUI frame", "empty drawable", 0);
    display->gl.Viewport(0, 0, width, height);
    display->gl.Disable(GL_MULTISAMPLE);
    display->gl.Disable(GL_FRAMEBUFFER_SRGB);
    display->gl.Disable(GL_CULL_FACE);
    display->gl.Disable(GL_SCISSOR_TEST);
    display->gl.Disable(GL_BLEND);
    display->gl.Enable(GL_DEPTH_TEST);
    display->gl.DepthMask(GL_TRUE);
    display->gl.DepthFunc(GL_LEQUAL);
    display->gl.ClearDepth(1.0);
    display->gl.Clear(GL_DEPTH_BUFFER_BIT);
    display->gl.UseProgram(display->program);
    display->gl.BindVertexArray(display->vao);
    display->gl.Uniform1i(display->alpha_test, 1);
    display->gl.Uniform2f(display->projection_scale, (GLfloat)projection_x, (GLfloat)projection_y);
    const GLfloat projection[16] = {(GLfloat)projection_x,
                                    0,
                                    0,
                                    0,
                                    0,
                                    (GLfloat)projection_y,
                                    0,
                                    0,
                                    0,
                                    0,
                                    (GLfloat)(-2.0 / (3000.0 - 1000.0)),
                                    0,
                                    -1,
                                    1,
                                    (GLfloat)(-(3000.0 + 1000.0) / (3000.0 - 1000.0)),
                                    1};
    display->gl.UniformMatrix4fv(display->projection_matrix, 1, GL_FALSE, projection);
    display->gl.Uniform1i(display->model_enabled, 0);
    if (!gl_check(display, "begin GUI frame", error))
        return false;
    display->exact_width = exact_width;
    display->exact_height = exact_height;
    display->frame_started = true;
    return success(error);
}

static bool draw_quad(mc_client_display *display, mc_client_texture texture,
                      const mc_client_vertex vertices[4], const mc_client_modelview *modelview,
                      bool blend, bool alpha_test, mc_client_display_error *error) {
    if (display == NULL || vertices == NULL || !display->frame_started ||
        (texture != 0 && !texture_known(display, texture)))
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "draw GUI quad",
                    "invalid frame/texture/vertices", 0);
    if (modelview != NULL) {
        for (size_t i = 0; i < 16; ++i)
            if (!isfinite(modelview->column_major[i]))
                return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "draw modelview quad",
                            "nonfinite modelview", 0);
        if (modelview->column_major[3] != 0 || modelview->column_major[7] != 0 ||
            modelview->column_major[11] != 0 || modelview->column_major[15] != 1)
            return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "draw modelview quad",
                        "non-affine modelview", 0);
    }
    display_vertex corners[4];
    for (size_t i = 0; i < 4; ++i) {
        const double x = vertices[i].x / display->exact_width * 2.0 - 1.0;
        const double y = 1.0 - vertices[i].y / display->exact_height * 2.0;
        if (!isfinite(x) || !isfinite(y) || x < -FLT_MAX || x > FLT_MAX || y < -FLT_MAX ||
            y > FLT_MAX || fabs(vertices[i].x) > FLT_MAX || fabs(vertices[i].y) > FLT_MAX ||
            !isfinite(vertices[i].u) || !isfinite(vertices[i].v))
            return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "project GUI quad",
                        "nonfinite or excessive coordinate", 0);
        if (modelview != NULL) {
            double bound[4] = {0};
            for (size_t row = 0; row < 4; ++row) {
                bound[row] = fabs((double)modelview->column_major[row]) * fabs(vertices[i].x) +
                             fabs((double)modelview->column_major[4 + row]) * fabs(vertices[i].y) +
                             fabs((double)modelview->column_major[12 + row]);
                if (bound[row] > (double)FLT_MAX / 16.0)
                    return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "draw modelview quad",
                                "excessive modelview product bound", 0);
            }
            const double bx =
                fabs((double)(GLfloat)(2.0 / display->exact_width)) * bound[0] + bound[3];
            const double by =
                fabs((double)(GLfloat)(-2.0 / display->exact_height)) * bound[1] + bound[3];
            const double bz = fabs((double)(GLfloat)(-2.0 / 2000.0)) * bound[2] + 2.0 * bound[3];
            if (bx > (double)FLT_MAX / 16.0 || by > (double)FLT_MAX / 16.0 ||
                bz > (double)FLT_MAX / 16.0)
                return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "draw modelview quad",
                            "excessive projection product bound", 0);
        }
        corners[i] = (display_vertex){
            .x = (GLfloat)vertices[i].x,
            .y = (GLfloat)vertices[i].y,
            .u = vertices[i].u,
            .v = vertices[i].v,
            .color = {(GLubyte)(vertices[i].argb >> 16), (GLubyte)(vertices[i].argb >> 8),
                      (GLubyte)vertices[i].argb, (GLubyte)(vertices[i].argb >> 24)}};
    }
    static const size_t indices[] = {0, 1, 2, 0, 2, 3};
    display_vertex triangles[6];
    for (size_t i = 0; i < 6; ++i)
        triangles[i] = corners[indices[i]];
    if (!current(display, "draw GUI quad", error))
        return false;
    display->gl.UseProgram(display->program);
    display->gl.BindVertexArray(display->vao);
    display->gl.BindBuffer(GL_ARRAY_BUFFER, display->vbo);
    display->gl.BufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(triangles), triangles);
    display->gl.ActiveTexture(GL_TEXTURE0);
    display->gl.BindTexture(GL_TEXTURE_2D, texture);
    display->gl.Uniform1i(display->model_enabled, modelview != NULL ? 1 : 0);
    if (modelview != NULL)
        display->gl.UniformMatrix4fv(display->modelview_matrix, 1, GL_FALSE,
                                     modelview->column_major);
    display->gl.Uniform1i(display->textured, texture != 0 ? 1 : 0);
    display->gl.Uniform1i(display->alpha_test, alpha_test ? 1 : 0);
    if (blend) {
        display->gl.Enable(GL_BLEND);
        display->gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        display->gl.Disable(GL_BLEND);
    }
    display->gl.DrawArrays(GL_TRIANGLES, 0, 6);
    return gl_check(display, "draw GUI quad", error) && success(error);
}

bool mc_client_display_draw_quad(mc_client_display *display, mc_client_texture texture,
                                 const mc_client_vertex vertices[4], bool blend, bool alpha_test,
                                 mc_client_display_error *error) {
    return draw_quad(display, texture, vertices, NULL, blend, alpha_test, error);
}

bool mc_client_display_draw_quad_modelview(mc_client_display *display, mc_client_texture texture,
                                           const mc_client_vertex vertices[4],
                                           const mc_client_modelview *modelview, bool blend,
                                           bool alpha_test, mc_client_display_error *error) {
    if (modelview == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "draw modelview quad",
                    "null modelview", 0);
    return draw_quad(display, texture, vertices, modelview, blend, alpha_test, error);
}

static bool swap_internal(mc_client_display *display, bool sdk_dispatch, bool *completed,
                          mc_client_display_error *error) {
    if (completed != NULL)
        *completed = false;
    if (!current_internal(display, "swap display buffers", error, sdk_dispatch))
        return false;
    if (!gl_check(display, "before display swap", error))
        return false;
    if (!SDL_GL_SwapWindow(display->window))
        return sdl_failure(error, "swap display buffers");
    if (completed != NULL)
        *completed = true;
    display->frame_started = false;
    display->scene_started = false;
    return gl_check(display, "swap display buffers", error) && success(error);
}

bool mc_client_display_swap(mc_client_display *display, mc_client_display_error *error) {
    return swap_internal(display, false, NULL, error);
}

bool mc_client_display_sdk_swap(mc_client_display *display, bool *completed,
                                mc_client_display_error *error) {
    return swap_internal(display, true, completed, error);
}

bool mc_client_display_capture_rgba(mc_client_display *display, mc_client_capture *out,
                                    mc_client_display_error *error) {
    if (display == NULL || out == NULL || (!display->frame_started && !display->scene_started))
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "capture GUI frame",
                    "invalid frame/output", 0);
    if (!current(display, "capture GUI frame", error))
        return false;
    int width = 0, height = 0;
    if (!mc_client_display_pixel_size(display, &width, &height, error))
        return false;
    if (width == 0 || height == 0)
        return fail(error, MC_CLIENT_DISPLAY_NOT_DRAWABLE, "capture GUI frame", "empty drawable",
                    0);
    size_t stride, bytes;
    if (!product((size_t)width, 4u, &stride) || !product(stride, (size_t)height, &bytes))
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "capture GUI frame", "capture byte overflow",
                    0);
    if (bytes > DISPLAY_CAPTURE_BYTES || stride > DISPLAY_CAPTURE_BYTES - bytes)
        return fail(error, MC_CLIENT_DISPLAY_LIMIT, "capture GUI frame", "capture byte limit", 0);
    uint8_t *pixels = malloc(bytes);
    uint8_t *row = malloc(stride);
    if (pixels == NULL || row == NULL) {
        free(row);
        free(pixels);
        return fail(error, MC_CLIENT_DISPLAY_ALLOCATION_FAILED, "capture GUI frame",
                    "allocation failed", 0);
    }
    display->gl.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    display->gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    display->gl.PixelStorei(GL_PACK_ROW_LENGTH, 0);
    display->gl.PixelStorei(GL_PACK_SKIP_PIXELS, 0);
    display->gl.PixelStorei(GL_PACK_SKIP_ROWS, 0);
    display->gl.ReadBuffer(GL_BACK);
    display->gl.ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    if (!gl_check(display, "read GUI back buffer", error)) {
        free(row);
        free(pixels);
        return false;
    }
    for (size_t y = 0; y < (size_t)height / 2u; ++y) {
        uint8_t *top = pixels + y * stride;
        uint8_t *bottom = pixels + ((size_t)height - y - 1u) * stride;
        memcpy(row, top, stride);
        memcpy(top, bottom, stride);
        memcpy(bottom, row, stride);
    }
    free(row);
    *out =
        (mc_client_capture){.width = width, .height = height, .stride = stride, .pixels = pixels};
    return success(error);
}

void mc_client_capture_free(mc_client_capture *capture) {
    if (capture != NULL) {
        free(capture->pixels);
        *capture = (mc_client_capture){0};
    }
}

static void cleanup_check(mc_client_display *display, const char *operation, bool *ok,
                          mc_client_display_error *first) {
    mc_client_display_error error = {0};
    if (!gl_check(display, operation, &error)) {
        if (*ok)
            *first = error;
        *ok = false;
    }
}

static bool destroy_native(mc_client_display *display, bool preserve_context, bool *consumed,
                           mc_client_display_error *context_error, mc_client_display_error *error) {
    if (consumed != NULL)
        *consumed = false;
    if (context_error != NULL)
        *context_error = (mc_client_display_error){0};
    if (display != NULL &&
        (display->source.context_holds != 0 || display->source.range_count != 0 ||
         display->source.texture_lease_count != 0 || display->source.active_calls != 0))
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "destroy display with source owners",
                    "retire native context/range/generation holds first", 0);
    if (display != NULL && display->texture_resource_count != 0)
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "destroy display with texture owners",
                    "retire the generated texture resources and retry", 0);
    if (display == NULL) {
        if (consumed != NULL)
            *consumed = true;
        return success(error);
    }
    mc_client_events_error events = {0};
    if (mc_client_events_retire(&display->events, &events) != MC_CLIENT_EVENTS_OK)
        return fail(error,
                    events.status == MC_CLIENT_EVENTS_BUSY ? MC_CLIENT_DISPLAY_BUSY
                                                           : MC_CLIENT_DISPLAY_SDL_ERROR,
                    events.operation, events.detail, 0);
    display->native_retiring = true;
    bool ok = true;
    mc_client_display_error first = {0};
    bool can_delete = display->context != NULL && display->functions_ready &&
                      !display->context_lost && !display->native_cleanup_issued;
    if (can_delete &&
        !current_internal(display, "make context current for cleanup", &first, true)) {
        ok = false;
        can_delete = false;
    }
    if (can_delete) {
        display->gl.UseProgram(0);
        display->gl.BindVertexArray(0);
        display->gl.BindBuffer(GL_ARRAY_BUFFER, 0);
        display->gl.ActiveTexture(GL_TEXTURE0);
        display->gl.BindTexture(GL_TEXTURE_2D, 0);
        cleanup_check(display, "unbind GUI objects for cleanup", &ok, &first);
        source_gpu_retire(display, &ok, &first);
        for (size_t i = 0; i < display->texture_count && !display->context_lost; ++i) {
            display->gl.DeleteTextures(1, &display->textures[i].id);
            cleanup_check(display, "delete display texture", &ok, &first);
        }
        if (!display->context_lost && display->vbo != 0) {
            display->gl.DeleteBuffers(1, &display->vbo);
            cleanup_check(display, "delete GUI buffer", &ok, &first);
        }
        if (!display->context_lost && display->vao != 0) {
            display->gl.DeleteVertexArrays(1, &display->vao);
            cleanup_check(display, "delete GUI vertex array", &ok, &first);
        }
        if (!display->context_lost && display->program != 0) {
            display->gl.DeleteProgram(display->program);
            cleanup_check(display, "delete GUI program", &ok, &first);
        }
        if (!display->context_lost && display->vertex_shader != 0) {
            display->gl.DeleteShader(display->vertex_shader);
            cleanup_check(display, "delete GUI vertex shader", &ok, &first);
        }
        if (!display->context_lost && display->fragment_shader != 0) {
            display->gl.DeleteShader(display->fragment_shader);
            cleanup_check(display, "delete GUI fragment shader", &ok, &first);
        }
        /* Native terminal cleanup is issued once. A staged context-destroy
         * failure keeps the actual context/window for retry, without repeating
         * deletion of names whose first driver result may be uncertain. */
        display->native_cleanup_issued = true;
    }
    if (display->context != NULL && !SDL_GL_DestroyContext(display->context)) {
        mc_client_display_error retained = {0};
        sdl_failure(&retained, "destroy OpenGL context");
        if (context_error != NULL)
            *context_error = retained;
        if (ok)
            first = retained;
        ok = false;
        if (preserve_context) {
            if (error != NULL)
                *error = first;
            return false;
        }
    }
    if (display->window != NULL)
        SDL_DestroyWindow(display->window);
    if (display->video)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(display);
    if (consumed != NULL)
        *consumed = true;
    if (ok)
        return success(error);
    if (error != NULL)
        *error = first;
    return false;
}

bool mc_client_display_destroy(mc_client_display *display, mc_client_display_error *error) {
    if (display != NULL &&
        (display->sdk_profile || display->stage_active || display->native_retiring))
        return fail(error, MC_CLIENT_DISPLAY_BUSY, "destroy staged/active Display",
                    "use the staged terminal owner and preserve its typed children", 0);
    return destroy_native(display, false, NULL, NULL, error);
}

/* Implementation includes require helpers in this order. */
// clang-format off
#include "source_surface.inc"
#include "source_world_list_surface.inc"
#include "texture_leases.inc"
#include "source_texture_update.inc"
#include "loading_projection.inc"
#include "startup_surface.inc"
#include "occlusion.inc"
#include "query_names.inc"
#include "source_viewport.inc"
// clang-format on

/* Canonical staged SDK owner uses the same native constructor/destructor. */
#include "sdk_staged.inc"
