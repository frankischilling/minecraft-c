#ifndef MC_CLIENT_DISPLAY_GL_INTERNAL_H
#define MC_CLIENT_DISPLAY_GL_INTERNAL_H

#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

/* SDL_opengl.h makes its GL1.x PFN typedefs optional behind
 * SDL_OPENGL_1_FUNCTION_TYPEDEFS. Use private names with the exact base API
 * prototypes instead of depending on that caller-controlled header option.
 * GLAPIENTRY preserves the base header's Windows calling convention. */
typedef GLenum(GLAPIENTRY *mc_client_gl_get_error_fn)(void);
typedef void(GLAPIENTRY *mc_client_gl_get_integer_fn)(GLenum name, GLint *value);
typedef const GLubyte *(GLAPIENTRY *mc_client_gl_get_string_fn)(GLenum name);
typedef void(GLAPIENTRY *mc_client_gl_viewport_fn)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void(GLAPIENTRY *mc_client_gl_clear_fn)(GLbitfield mask);
typedef void(GLAPIENTRY *mc_client_gl_clear_depth_fn)(GLclampd depth);
typedef void(GLAPIENTRY *mc_client_gl_clear_color_fn)(GLfloat red, GLfloat green, GLfloat blue,
                                                      GLfloat alpha);
typedef void(GLAPIENTRY *mc_client_gl_enable_fn)(GLenum capability);
typedef void(GLAPIENTRY *mc_client_gl_disable_fn)(GLenum capability);
typedef void(GLAPIENTRY *mc_client_gl_depth_function_fn)(GLenum function);
typedef void(GLAPIENTRY *mc_client_gl_depth_mask_fn)(GLboolean flag);
typedef void(GLAPIENTRY *mc_client_gl_blend_function_fn)(GLenum source, GLenum destination);
typedef void(GLAPIENTRY *mc_client_gl_generate_textures_fn)(GLsizei count, GLuint *textures);
typedef void(GLAPIENTRY *mc_client_gl_delete_textures_fn)(GLsizei count, const GLuint *textures);
typedef void(GLAPIENTRY *mc_client_gl_bind_texture_fn)(GLenum target, GLuint texture);
typedef void(GLAPIENTRY *mc_client_gl_texture_parameter_fn)(GLenum target, GLenum name,
                                                            GLint value);
typedef void(GLAPIENTRY *mc_client_gl_texture_image_fn)(GLenum target, GLint level,
                                                        GLint internal_format, GLsizei width,
                                                        GLsizei height, GLint border, GLenum format,
                                                        GLenum type, const GLvoid *pixels);
typedef void(GLAPIENTRY *mc_client_gl_pixel_store_fn)(GLenum name, GLint value);
typedef void(GLAPIENTRY *mc_client_gl_draw_arrays_fn)(GLenum mode, GLint first, GLsizei count);
typedef void(GLAPIENTRY *mc_client_gl_read_buffer_fn)(GLenum mode);
typedef void(GLAPIENTRY *mc_client_gl_read_pixels_fn)(GLint x, GLint y, GLsizei width,
                                                      GLsizei height, GLenum format, GLenum type,
                                                      GLvoid *pixels);

/* SDL returns a generic function pointer. The loader converts directly between
 * function-pointer types and calls only through each exact GL declaration. */
#define MC_CLIENT_GL_FUNCTIONS(X)                                                                  \
    X(mc_client_gl_get_error_fn, GetError, "glGetError")                                           \
    X(mc_client_gl_get_integer_fn, GetIntegerv, "glGetIntegerv")                                   \
    X(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC, GetFramebufferAttachmentParameteriv,           \
      "glGetFramebufferAttachmentParameteriv")                                                     \
    X(mc_client_gl_get_string_fn, GetString, "glGetString")                                        \
    X(mc_client_gl_viewport_fn, Viewport, "glViewport")                                            \
    X(mc_client_gl_clear_fn, Clear, "glClear")                                                     \
    X(mc_client_gl_clear_depth_fn, ClearDepth, "glClearDepth")                                     \
    X(mc_client_gl_clear_color_fn, ClearColor, "glClearColor")                                     \
    X(mc_client_gl_enable_fn, Enable, "glEnable")                                                  \
    X(mc_client_gl_disable_fn, Disable, "glDisable")                                               \
    X(mc_client_gl_depth_function_fn, DepthFunc, "glDepthFunc")                                    \
    X(mc_client_gl_depth_mask_fn, DepthMask, "glDepthMask")                                        \
    X(mc_client_gl_blend_function_fn, BlendFunc, "glBlendFunc")                                    \
    X(mc_client_gl_generate_textures_fn, GenTextures, "glGenTextures")                             \
    X(mc_client_gl_delete_textures_fn, DeleteTextures, "glDeleteTextures")                         \
    X(mc_client_gl_bind_texture_fn, BindTexture, "glBindTexture")                                  \
    X(mc_client_gl_texture_parameter_fn, TexParameteri, "glTexParameteri")                         \
    X(mc_client_gl_texture_image_fn, TexImage2D, "glTexImage2D")                                   \
    X(mc_client_gl_pixel_store_fn, PixelStorei, "glPixelStorei")                                   \
    X(PFNGLACTIVETEXTUREPROC, ActiveTexture, "glActiveTexture")                                    \
    X(PFNGLCREATESHADERPROC, CreateShader, "glCreateShader")                                       \
    X(PFNGLSHADERSOURCEPROC, ShaderSource, "glShaderSource")                                       \
    X(PFNGLCOMPILESHADERPROC, CompileShader, "glCompileShader")                                    \
    X(PFNGLGETSHADERIVPROC, GetShaderiv, "glGetShaderiv")                                          \
    X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog, "glGetShaderInfoLog")                           \
    X(PFNGLDELETESHADERPROC, DeleteShader, "glDeleteShader")                                       \
    X(PFNGLCREATEPROGRAMPROC, CreateProgram, "glCreateProgram")                                    \
    X(PFNGLATTACHSHADERPROC, AttachShader, "glAttachShader")                                       \
    X(PFNGLLINKPROGRAMPROC, LinkProgram, "glLinkProgram")                                          \
    X(PFNGLGETPROGRAMIVPROC, GetProgramiv, "glGetProgramiv")                                       \
    X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog, "glGetProgramInfoLog")                        \
    X(PFNGLDELETEPROGRAMPROC, DeleteProgram, "glDeleteProgram")                                    \
    X(PFNGLUSEPROGRAMPROC, UseProgram, "glUseProgram")                                             \
    X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation, "glGetUniformLocation")                     \
    X(PFNGLUNIFORM1IPROC, Uniform1i, "glUniform1i")                                                \
    X(PFNGLUNIFORM2FPROC, Uniform2f, "glUniform2f")                                                \
    X(PFNGLUNIFORMMATRIX4FVPROC, UniformMatrix4fv, "glUniformMatrix4fv")                           \
    X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays, "glGenVertexArrays")                              \
    X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays, "glDeleteVertexArrays")                     \
    X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray, "glBindVertexArray")                              \
    X(PFNGLGENBUFFERSPROC, GenBuffers, "glGenBuffers")                                             \
    X(PFNGLDELETEBUFFERSPROC, DeleteBuffers, "glDeleteBuffers")                                    \
    X(PFNGLBINDBUFFERPROC, BindBuffer, "glBindBuffer")                                             \
    X(PFNGLBUFFERDATAPROC, BufferData, "glBufferData")                                             \
    X(PFNGLBUFFERSUBDATAPROC, BufferSubData, "glBufferSubData")                                    \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray, "glEnableVertexAttribArray")      \
    X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer, "glVertexAttribPointer")                  \
    X(mc_client_gl_draw_arrays_fn, DrawArrays, "glDrawArrays")                                     \
    X(mc_client_gl_read_buffer_fn, ReadBuffer, "glReadBuffer")                                     \
    X(mc_client_gl_read_pixels_fn, ReadPixels, "glReadPixels")

typedef struct mc_client_gl {
#define MC_CLIENT_GL_FIELD(type, field, name) type field;
    MC_CLIENT_GL_FUNCTIONS(MC_CLIENT_GL_FIELD)
#undef MC_CLIENT_GL_FIELD
} mc_client_gl;

#endif
