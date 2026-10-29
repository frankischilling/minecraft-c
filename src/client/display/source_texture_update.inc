#include "source_texture_update.h"

bool mc_client_display_source_texture_bind(mc_client_display *display, mc_client_texture id,
                                           mc_client_display_error *error) {
    if (!source_enter(display, "source glBindTexture", error))
        return false;
    bool ok = true;
    mc_client_texture_resource *resource = source_live_texture(display, id, 0, false);
    if (display->texture_resource_calls != 0)
        ok = fail(error, MC_CLIENT_DISPLAY_BUSY, "source glBindTexture",
                  "texture resource operation active", 0);
    else if (display->source.compiling != NULL || resource == NULL ||
             display->gl.BindTexture == NULL)
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glBindTexture",
                  "live generated texture and immediate source path required", 0);
    else {
        display->gl.BindTexture(GL_TEXTURE_2D, id);
        display->source.bound_texture = id;
        display->source.bound_texture_generation = resource->generation;
        display->source.texture_binding_known = true;
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_texture_sub_image(mc_client_display *display, int32_t level,
                                                int32_t x, int32_t y, int32_t width, int32_t height,
                                                const uint8_t *bytes, size_t remaining,
                                                mc_client_display_error *error) {
    if (width < 0 || height < 0 || bytes == NULL ||
        (uint64_t)(uint32_t)width * (uint32_t)height * 4u > remaining)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source glTexSubImage2D buffer",
                    "insufficient direct-buffer remaining bytes", 0);
    if (!source_enter(display, "source glTexSubImage2D", error))
        return false;
    bool ok = true;
    if (display->texture_resource_calls != 0)
        ok = fail(error, MC_CLIENT_DISPLAY_BUSY, "source glTexSubImage2D",
                  "texture resource operation active", 0);
    else if (display->source.compiling != NULL || display->gl.TexSubImage2D == NULL ||
             display->gl.GetIntegerv == NULL)
        ok = fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glTexSubImage2D",
                  "immediate source path and actual function addresses required", 0);
    else {
        GLint pbo = 0, alignment = 0, row_length = 0, skip_pixels = 0, skip_rows = 0;
        display->gl.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pbo);
        display->gl.GetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        display->gl.GetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
        display->gl.GetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
        display->gl.GetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
        if (pbo != 0 || row_length != 0 || skip_pixels != 0 || skip_rows != 0 || alignment <= 0 ||
            ((size_t)(uint32_t)width * 4u) % (size_t)alignment != 0)
            ok =
                fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source glTexSubImage2D unpack state",
                     "tightly packed client memory and disabled unpack PBO required", 0);
        else
            display->gl.TexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, GL_RGBA,
                                      GL_UNSIGNED_BYTE, bytes);
    }
    return source_leave(display, ok, error);
}
