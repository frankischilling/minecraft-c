/* Standalone, asset-free GLX compatibility device probe. */
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Failed at %d: %s (GL error %#x)\n", __LINE__, #condition,             \
                    (unsigned)glGetError());                                                       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

enum { rows = 768, words = 7 };

typedef struct {
    float xyz[3], spare[2];
    uint8_t color[4];
    int8_t normal[3];
    uint8_t padding[5];
} vertex;

_Static_assert(sizeof(vertex) == 32, "Vertex stride");

static vertex input[rows], compiled_input[rows];
static float result[rows][words];

static GLuint shader(GLenum kind, const char *code) {
    GLuint object = glCreateShader(kind);
    glShaderSource(object, 1, &code, NULL);
    glCompileShader(object);
    GLint status = 0;
    glGetShaderiv(object, GL_COMPILE_STATUS, &status);
    if (!status) {
        char message[4096] = {0};
        glGetShaderInfoLog(object, sizeof(message), NULL, message);
        fprintf(stderr, "%s\n", message);
    }
    CHECK(status);
    return object;
}

static GLuint feedback_program(void) {
    const char *code = "#version 150 compatibility\n"
                       "out vec3 probe_normal; out vec4 probe_color;\n"
                       "void main() { gl_Position = gl_Vertex; probe_normal = gl_Normal; "
                       "probe_color = gl_Color; }\n";
    GLuint vs = shader(GL_VERTEX_SHADER, code);
    GLuint fs = shader(GL_FRAGMENT_SHADER, "#version 150 compatibility\n"
                                           "void main() { gl_FragColor = vec4(1); }\n");
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    const char *varyings[] = {"probe_normal", "probe_color"};
    glTransformFeedbackVaryings(program, 2, varyings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char message[4096] = {0};
        glGetProgramInfoLog(program, sizeof(message), NULL, message);
        fprintf(stderr, "%s\n", message);
    }
    CHECK(status);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void verify_current(const char *label, int require_color) {
    static const float expected_normal[] = {.125f, -.25f, .5f};
    static const float expected_color[] = {.25f, .75f, .875f, 1};
    float normal[3], color[4];
    glGetFloatv(GL_CURRENT_NORMAL, normal);
    glGetFloatv(GL_CURRENT_COLOR, color);
    CHECK(glGetError() == GL_NO_ERROR);
    printf("%s %08x,%08x,%08x %08x,%08x,%08x,%08x\n", label, bits(normal[0]), bits(normal[1]),
           bits(normal[2]), bits(color[0]), bits(color[1]), bits(color[2]), bits(color[3]));
    CHECK(memcmp(normal, expected_normal, sizeof(normal)) == 0);
    CHECK(!require_color || memcmp(color, expected_color, sizeof(color)) == 0);
}

static void fill(void) {
    for (int row = 0; row < rows; ++row) {
        vertex *p = &input[row];
        p->xyz[0] = (row % 3 == 0 ? -.5f : .5f);
        p->xyz[1] = (row % 3 == 1 ? .5f : -.5f);
        p->xyz[2] = 0;
        for (int i = 0; i < 3; ++i) {
            const int raw = ((row / 3) + 31 * i) & 255;
            p->normal[i] = (int8_t)(raw < 128 ? raw : raw - 256);
        }
        for (int i = 0; i < 4; ++i)
            p->color[i] = (uint8_t)(((row / 3) + 21 * i) & 255);
    }
    memcpy(compiled_input, input, sizeof(input));
}

static void capture(GLuint program, GLuint buffer, GLuint list) {
    glUseProgram(program);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_TRIANGLES);
    if (list == 0)
        glDrawArrays(GL_TRIANGLES, 0, rows);
    else
        glCallList(list);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(result), result);
    glUseProgram(0);
    CHECK(glGetError() == GL_NO_ERROR);
}

static void verify_arrays(const char *label, int list) {
    size_t normal_differences = 0, color_differences = 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t row = 0; row < rows; ++row) {
        for (size_t i = 0; i < 3; ++i) {
            const int raw = compiled_input[row].normal[i];
            const float normal = list          ? (float)(2 * raw + 1) * (1.0f / 255.0f)
                                 : raw == -128 ? -1.0f
                                               : (float)raw * (1.0f / 127.0f);
            const uint32_t actual = bits(result[row][i]);
            normal_differences += actual != bits(normal);
            hash = (hash ^ actual) * UINT64_C(1099511628211);
        }
        for (size_t i = 0; i < 4; ++i) {
            const float raw = (float)compiled_input[row].color[i];
            const float color = list ? raw / 255.0f : raw * (1.0f / 255.0f);
            const uint32_t actual = bits(result[row][3 + i]);
            color_differences += actual != bits(color);
            hash = (hash ^ actual) * UINT64_C(1099511628211);
        }
    }
    printf("ARRAY %s normal_differences=%zu color_differences=%zu words=%d hash=%016llx\n", label,
           normal_differences, color_differences, rows * words, (unsigned long long)hash);
    CHECK(normal_differences == 0 && color_differences == 0);
}

static void set_current(void) {
    glNormal3f(.125f, -.25f, .5f);
    glColor4f(.25f, .75f, .875f, 1);
}

static void verify_materials(const char *stage, const float ambient[4], const float diffuse[4]) {
    for (unsigned int side = 0; side < 2; ++side) {
        const GLenum face = side == 0 ? GL_FRONT : GL_BACK;
        float actual[4];
        glGetMaterialfv(face, GL_AMBIENT, actual);
        CHECK(memcmp(actual, ambient, sizeof(actual)) == 0);
        glGetMaterialfv(face, GL_DIFFUSE, actual);
        if (memcmp(actual, diffuse, sizeof(actual)) != 0)
            fprintf(stderr, "%s diffuse %x got %.6g %.6g %.6g %.6g, expected %.6g %.6g %.6g %.6g\n",
                    stage, face, actual[0], actual[1], actual[2], actual[3], diffuse[0], diffuse[1],
                    diffuse[2], diffuse[3]);
        CHECK(memcmp(actual, diffuse, sizeof(actual)) == 0);
    }
    CHECK(glGetError() == GL_NO_ERROR);
    printf("MATERIAL %s front-and-back=match\n", stage);
}

static void material_sequence(void) {
    static const float ambient[] = {.0625f, .1875f, .3125f, 1};
    static const float diffuse[] = {.4375f, .5625f, .6875f, 1};
    static const float first[] = {.125f, .5f, .75f, 1};
    static const float second[] = {.625f, .875f, .25f, 1};
    static const float third[] = {.25f, .375f, .875f, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glColor4fv(first);
    float current[4];
    glGetFloatv(GL_CURRENT_COLOR, current);
    CHECK(memcmp(current, first, sizeof(current)) == 0);
    glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
    verify_materials("enabled", ambient, first);

    glColor4fv(second);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    verify_materials("tracked-write", ambient, second);

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT);
    glColor4fv(third);
    verify_materials("changed-selector", third, second);

    glDisable(GL_COLOR_MATERIAL);
    glColor4fv(first);
    verify_materials("disabled", third, second);

    GLuint material_list = glGenLists(1);
    CHECK(material_list != 0);
    glNewList(material_list, GL_COMPILE);
    glColor4fv(second);
    glEndList();
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
    glCallList(material_list);
    verify_materials("compiled-color", second, second);
    glDisable(GL_COLOR_MATERIAL);
    glDeleteLists(material_list, 1);
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    CHECK(display != NULL);
    int attrs[] = {GLX_RGBA, GLX_RED_SIZE,   8, GLX_GREEN_SIZE, 8,  GLX_BLUE_SIZE,
                   8,        GLX_ALPHA_SIZE, 8, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER,
                   None};
    XVisualInfo *visual = glXChooseVisual(display, DefaultScreen(display), attrs);
    CHECK(visual != NULL);
    const Window root = RootWindow(display, visual->screen);
    const Colormap colormap = XCreateColormap(display, root, visual->visual, AllocNone);
    XSetWindowAttributes wa = {.colormap = colormap};
    Window window = XCreateWindow(display, root, 0, 0, 64, 64, 0, visual->depth, InputOutput,
                                  visual->visual, CWColormap, &wa);
    GLXContext context = glXCreateContext(display, visual, NULL, True);
    CHECK(context != NULL && glXMakeCurrent(display, window, context));
    GLint mask = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
    CHECK((mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0 && glGetError() == GL_NO_ERROR);
    printf("DEVICE %s / %s / %s mask=%d\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER),
           glGetString(GL_VERSION), mask);
    GLuint program = feedback_program(), buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(result), NULL, GL_STREAM_READ);
    fill();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexPointer(3, GL_FLOAT, sizeof(vertex), input[0].xyz);
    glNormalPointer(GL_BYTE, sizeof(vertex), input[0].normal);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), input[0].color);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    set_current();
    verify_current("CURRENT authored", 1);
    capture(program, buffer, 0);
    verify_arrays("direct", 0);
    verify_current("CURRENT after_direct", 0);
    GLuint list = glGenLists(1);
    CHECK(list != 0);
    glNewList(list, GL_COMPILE);
    glDrawArrays(GL_TRIANGLES, 0, rows);
    glEndList();
    CHECK(glGetError() == GL_NO_ERROR);
    memset(input, 0x31, sizeof(input));
    set_current();
    capture(program, buffer, list);
    verify_arrays("compiled", 1);
    verify_current("CURRENT after_compiled", 1);
    capture(program, buffer, list);
    verify_arrays("replayed", 1);

    glDeleteLists(list, 1);
    glDeleteBuffers(1, &buffer);
    glDeleteProgram(program);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    material_sequence();
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(glXMakeCurrent(display, None, NULL));
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual);
    XCloseDisplay(display);
    return 0;
}
