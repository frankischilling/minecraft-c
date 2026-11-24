/* Authored passive Linux GLdraw observer. No game classes, fields or calls modified.
 * The exact requested DrawArrays/Elements entry forwards once to the real GL dispatcher.
 * glGet queries read live driver state before forwarding; output is a separate journal. */
#define _GNU_SOURCE
#include <GL/gl.h>
#include <GL/glx.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long sequence;

static void *actual_symbol(void *handle, const char *name) {
    void *(*real)(void *, const char *);
    *(void **)(&real) = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
    if (!real)
        abort();
    return real(handle, name);
}

static void floats(FILE *f, const char *name, GLenum key, int n) {
    GLfloat a[16];
    glGetFloatv(key, a);
    fprintf(f, "\"%s\":[", name);
    for (int j = 0; j < n; j++) {
        uint32_t u;
        memcpy(&u, &a[j], 4);
        fprintf(f, "%s\"%08x\"", j ? "," : "", u);
    }
    fputc(']', f);
}

static void record(const char *method, GLenum mode, GLint first, GLsizei count, GLenum type) {
    const char *p = getenv("BILLBOARD_GL_LOG");
    if (!p)
        return;
    FILE *f = fopen(p, "a");
    if (!f)
        abort();
    GLint a;
    GLboolean b;
    fprintf(f,
            "{\"event\":\"driver-draw\",\"ordinal\":%lu,\"method\":\"%s\",\"mode\":%u,\"first\":%d,"
            "\"count\":%d,\"type\":%u,",
            sequence++, method, mode, first, count, type);
    floats(f, "modelview", GL_MODELVIEW_MATRIX, 16);
    fputc(',', f);
    floats(f, "projection", GL_PROJECTION_MATRIX, 16);
    fputc(',', f);
    floats(f, "color", GL_CURRENT_COLOR, 4);
    fputc(',', f);
    floats(f, "normal", GL_CURRENT_NORMAL, 3);
#define INT(n, k)                                                                                  \
    glGetIntegerv(k, &a);                                                                          \
    fprintf(f, ",\"" n "\":%d", a)
    INT("matrix_mode", GL_MATRIX_MODE);
    INT("matrix_depth", GL_MODELVIEW_STACK_DEPTH);
    INT("projection_depth", GL_PROJECTION_STACK_DEPTH);
    INT("texture", GL_TEXTURE_BINDING_2D);
    INT("depth_function", GL_DEPTH_FUNC);
    INT("blend_src", GL_BLEND_SRC);
    INT("blend_dst", GL_BLEND_DST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &b);
    fprintf(f, ",\"depth_mask\":%s,\"caps\":[", b ? "true" : "false");
    GLenum caps[] = {GL_CULL_FACE,      GL_BLEND,     GL_ALPHA_TEST, GL_DEPTH_TEST, GL_LIGHTING,
                     GL_RESCALE_NORMAL, GL_NORMALIZE, GL_TEXTURE_2D, GL_FOG};
    for (unsigned j = 0; j < sizeof(caps) / sizeof(caps[0]); j++)
        fprintf(f, "%s{\"cap\":%u,\"enabled\":%s}", j ? "," : "", caps[j],
                glIsEnabled(caps[j]) ? "true" : "false");
    fputs("]}\n", f);
    if (fclose(f))
        abort();
}

void glDrawArrays(GLenum m, GLint f, GLsizei c) {
    static void (*real)(GLenum, GLint, GLsizei);
    if (!real) {
        *(void **)(&real) = actual_symbol(RTLD_NEXT, "glDrawArrays");
        if (!real)
            abort();
    }
    record("glDrawArrays", m, f, c, 0);
    real(m, f, c);
}

void glDrawElements(GLenum m, GLsizei c, GLenum t, const void *p) {
    static void (*real)(GLenum, GLsizei, GLenum, const void *);
    if (!real) {
        *(void **)(&real) = actual_symbol(RTLD_NEXT, "glDrawElements");
        if (!real)
            abort();
    }
    record("glDrawElements", m, 0, c, t);
    real(m, c, t, p);
}

static __GLXextFuncPtr resolve(const GLubyte *n, const char *which) {
    if (strcmp((const char *)n, "glDrawArrays") == 0)
        return (__GLXextFuncPtr)glDrawArrays;
    if (strcmp((const char *)n, "glDrawElements") == 0)
        return (__GLXextFuncPtr)glDrawElements;
    __GLXextFuncPtr (*real)(const GLubyte *);
    *(void **)(&real) = actual_symbol(RTLD_NEXT, which);
    if (!real)
        abort();
    return real(n);
}

__GLXextFuncPtr glXGetProcAddress(const GLubyte *n) {
    return resolve(n, "glXGetProcAddress");
}

__GLXextFuncPtr glXGetProcAddressARB(const GLubyte *n) {
    return resolve(n, "glXGetProcAddressARB");
}

/* LWJGL2 obtains function pointers with explicit library handles; observe that
 * resolution too. Every other symbol is returned directly unchanged. */
void *dlsym(void *handle, const char *name) {
    if (strcmp(name, "glDrawArrays") == 0)
        return (void *)glDrawArrays;
    if (strcmp(name, "glDrawElements") == 0)
        return (void *)glDrawElements;
    if (strcmp(name, "glXGetProcAddress") == 0)
        return (void *)glXGetProcAddress;
    if (strcmp(name, "glXGetProcAddressARB") == 0)
        return (void *)glXGetProcAddressARB;
    return actual_symbol(handle, name);
}
