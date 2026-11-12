#include "mc/client/fonts/default_font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fontconfig/fontconfig.h>
#endif

struct mc_client_default_font {
    size_t holds, max_bytes, bytes;
    uint8_t *data;
    FT_Library library;
    FT_Face face;
    bool active, ready, failed;
    mc_client_font_error failure;
    size_t completed_glyphs;
    mc_client_font_glyph_state glyphs[10];
};

static mc_client_font_status fail(mc_client_font_error *e, mc_client_font_status status,
                                  const char *operation) {
    e->status = status;
    e->operation = operation;
    return status;
}

#ifdef _WIN32
static bool discover(mc_client_default_font *font, long *index, mc_client_font_error *e) {
    HDC dc = CreateCompatibleDC(NULL);
    HFONT face = NULL;
    HGDIOBJ previous = NULL;
    bool ok = false;
    if (dc == NULL) {
        e->system_error = GetLastError();
        fail(e, MC_CLIENT_FONT_DISCOVERY_FAILED, "default Dialog GDI context");
        return false;
    }
    face = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    if (face == NULL) {
        e->system_error = GetLastError();
        fail(e, MC_CLIENT_FONT_DISCOVERY_FAILED, "default Dialog registered Arial");
        goto done;
    }
    previous = SelectObject(dc, face);
    if (previous == NULL || previous == HGDI_ERROR) {
        e->system_error = GetLastError();
        fail(e, MC_CLIENT_FONT_DISCOVERY_FAILED, "default Dialog select Arial");
        previous = NULL;
        goto done;
    }
    wchar_t name[96];
    if (GetTextFaceW(dc, 96, name) == 0 || wcscmp(name, L"Arial") != 0) {
        fail(e, MC_CLIENT_FONT_DISCOVERY_FAILED, "default Dialog Arial substituted");
        goto done;
    }
    const DWORD size = GetFontData(dc, 0, 0, NULL, 0);
    if (size == GDI_ERROR || size == 0) {
        e->system_error = GetLastError();
        fail(e, MC_CLIENT_FONT_DISCOVERY_FAILED, "default Dialog GDI font bytes size");
        goto done;
    }
    if ((size_t)size > font->max_bytes) {
        fail(e, MC_CLIENT_FONT_LIMIT, "default Dialog font byte limit");
        goto done;
    }
    font->data = malloc(size);
    if (font->data == NULL) {
        fail(e, MC_CLIENT_FONT_ALLOCATION_FAILED, "default Dialog font bytes");
        goto done;
    }
    font->bytes = size;
    if (GetFontData(dc, 0, 0, font->data, size) != size) {
        e->system_error = GetLastError();
        fail(e, MC_CLIENT_FONT_IO_FAILED, "default Dialog read GDI font bytes");
        goto done;
    }
    *index = 0;
    ok = true;
done:
    if (previous != NULL && SelectObject(dc, previous) == NULL && ok) {
        e->system_error = GetLastError();
        fail(e, MC_CLIENT_FONT_IO_FAILED, "default Dialog restore GDI font");
        ok = false;
    }
    if (face != NULL && !DeleteObject(face) && ok) {
        e->system_error = GetLastError();
        fail(e, MC_CLIENT_FONT_IO_FAILED, "default Dialog delete GDI font");
        ok = false;
    }
    if (!DeleteDC(dc) && ok) {
        e->system_error = GetLastError();
        fail(e, MC_CLIENT_FONT_IO_FAILED, "default Dialog delete GDI context");
        ok = false;
    }
    return ok;
}
#else
static bool discover(mc_client_default_font *font, long *index, mc_client_font_error *e) {
    FcConfig *config = FcInitLoadConfigAndFonts();
    FcPattern *pattern = NULL, *matched = NULL;
    FILE *file = NULL;
    bool ok = false;
    if (config == NULL) {
        fail(e, MC_CLIENT_FONT_DISCOVERY_FAILED, "default Dialog Fontconfig");
        goto done;
    }
    pattern = FcPatternCreate();
    if (pattern == NULL || !FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)"sans") ||
        !FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_REGULAR) ||
        !FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN) ||
        !FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 12.0) ||
        !FcConfigSubstitute(config, pattern, FcMatchPattern)) {
        fail(e, MC_CLIENT_FONT_DISCOVERY_FAILED, "default Dialog Fontconfig pattern");
        goto done;
    }
    FcDefaultSubstitute(pattern);
    FcResult result;
    matched = FcFontMatch(config, pattern, &result);
    FcChar8 *path = NULL;
    int face_index = 0;
    if (matched == NULL || result != FcResultMatch ||
        FcPatternGetString(matched, FC_FILE, 0, &path) != FcResultMatch ||
        FcPatternGetInteger(matched, FC_INDEX, 0, &face_index) != FcResultMatch) {
        fail(e, MC_CLIENT_FONT_DISCOVERY_FAILED, "default Dialog Fontconfig match");
        goto done;
    }
    file = fopen((const char *)path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        e->system_error = (unsigned long)errno;
        fail(e, MC_CLIENT_FONT_IO_FAILED, "default Dialog open font file");
        goto done;
    }
    const long size = ftell(file);
    if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        e->system_error = (unsigned long)errno;
        fail(e, MC_CLIENT_FONT_IO_FAILED, "default Dialog font file extent");
        goto done;
    }
    if ((unsigned long)size > font->max_bytes) {
        fail(e, MC_CLIENT_FONT_LIMIT, "default Dialog font byte limit");
        goto done;
    }
    font->data = malloc((size_t)size);
    if (font->data == NULL) {
        fail(e, MC_CLIENT_FONT_ALLOCATION_FAILED, "default Dialog font bytes");
        goto done;
    }
    font->bytes = (size_t)size;
    if (fread(font->data, 1, font->bytes, file) != font->bytes) {
        e->system_error = (unsigned long)errno;
        fail(e, MC_CLIENT_FONT_IO_FAILED, "default Dialog read font file");
        goto done;
    }
    *index = face_index;
    ok = true;
done:
    if (file != NULL && fclose(file) != 0 && ok) {
        e->system_error = (unsigned long)errno;
        fail(e, MC_CLIENT_FONT_IO_FAILED, "default Dialog close font file");
        ok = false;
    }
    if (matched != NULL)
        FcPatternDestroy(matched);
    if (pattern != NULL)
        FcPatternDestroy(pattern);
    if (config != NULL)
        FcConfigDestroy(config);
    return ok;
}
#endif

static bool activate(mc_client_default_font *font, mc_client_font_error *e) {
    if (font->ready)
        return true;
    if (font->failed) {
        *e = font->failure;
        return false;
    }
    long index = 0;
    if (!discover(font, &index, e))
        goto failed;
    e->freetype_error = FT_Init_FreeType(&font->library);
    if (e->freetype_error != 0) {
        fail(e, MC_CLIENT_FONT_RASTER_FAILED, "default Dialog FreeType library");
        goto failed;
    }
    const unsigned int interpreter = 35;
    const char *properties = getenv("FREETYPE_PROPERTIES");
    if (properties == NULL || strstr(properties, "interpreter-version") == NULL)
        e->freetype_error =
            FT_Property_Set(font->library, "truetype", "interpreter-version", &interpreter);
    if (e->freetype_error != 0 || font->bytes > (size_t)LONG_MAX) {
        fail(e, MC_CLIENT_FONT_RASTER_FAILED, "default Dialog FreeType interpreter35");
        goto failed;
    }
    e->freetype_error = FT_New_Memory_Face(font->library, font->data, (FT_Long)font->bytes,
                                           (FT_Long)index, &font->face);
    if (e->freetype_error != 0) {
        fail(e, MC_CLIENT_FONT_RASTER_FAILED, "default Dialog FreeType face");
        goto failed;
    }
    e->freetype_error = FT_Set_Char_Size(font->face, 0, 12 * 64, 72, 72);
    if (e->freetype_error != 0) {
        fail(e, MC_CLIENT_FONT_RASTER_FAILED, "default Dialog plain12 size");
        goto failed;
    }
    font->ready = true;
    return true;
failed:
    font->failed = true;
    font->failure = *e;
    return false;
}

mc_client_font_status mc_client_default_font_create(size_t max_bytes,
                                                    mc_client_default_font **out) {
    if (out == NULL)
        return MC_CLIENT_FONT_INVALID_ARGUMENT;
    mc_client_default_font *font = calloc(1, sizeof(*font));
    if (font == NULL)
        return MC_CLIENT_FONT_ALLOCATION_FAILED;
    font->holds = 1;
    font->max_bytes = max_bytes;
    *out = font;
    return MC_CLIENT_FONT_OK;
}

mc_client_font_status mc_client_default_font_retain(mc_client_default_font *font) {
    if (font == NULL)
        return MC_CLIENT_FONT_INVALID_ARGUMENT;
    if (font->holds == SIZE_MAX)
        return MC_CLIENT_FONT_LIMIT;
    ++font->holds;
    return MC_CLIENT_FONT_OK;
}

mc_client_font_status mc_client_default_font_release(mc_client_default_font *font) {
    if (font == NULL)
        return MC_CLIENT_FONT_OK;
    if (font->active)
        return MC_CLIENT_FONT_BUSY;
    if (--font->holds != 0)
        return MC_CLIENT_FONT_OK;
    if (font->face != NULL)
        FT_Done_Face(font->face);
    if (font->library != NULL)
        FT_Done_FreeType(font->library);
    free(font->data);
    free(font);
    return MC_CLIENT_FONT_OK;
}

bool mc_client_default_font_inspect(const mc_client_default_font *font,
                                    mc_client_font_snapshot *out) {
    if (font == NULL || out == NULL)
        return false;
    *out = (mc_client_font_snapshot){.native_holds = font->holds,
                                     .font_bytes = font->bytes,
                                     .ready = font->ready,
                                     .failed = font->failed};
    if (font->library != NULL)
        FT_Library_Version(font->library, &out->freetype_major, &out->freetype_minor,
                           &out->freetype_patch);
    if (font->face != NULL && font->face->family_name != NULL) {
        size_t n = strlen(font->face->family_name);
        if (n >= sizeof(out->family))
            n = sizeof(out->family) - 1;
        memcpy(out->family, font->face->family_name, n);
    }
    out->completed_glyphs = font->completed_glyphs;
    memcpy(out->glyphs, font->glyphs, sizeof(out->glyphs));
    return true;
}

mc_client_font_status mc_client_default_font_draw_missingtex(mc_client_default_font *font,
                                                             mc_client_font_argb_view image,
                                                             mc_client_font_error *error) {
    mc_client_font_error e = {0};
    if (font == NULL || image.pixels == NULL || image.width == 0 || image.height == 0 ||
        image.width > INT32_MAX || image.height > INT32_MAX || image.stride_pixels < image.width ||
        image.stride_pixels > SIZE_MAX / image.height)
        fail(&e, MC_CLIENT_FONT_INVALID_ARGUMENT, "default Dialog draw arguments");
    else if (font->active)
        fail(&e, MC_CLIENT_FONT_BUSY, "default Dialog active draw");
    else {
        font->active = true;
        font->completed_glyphs = 0;
        memset(font->glyphs, 0, sizeof(font->glyphs));
        if (activate(font, &e)) {
            int64_t x = 1;
            const char text[] = "missingtex";
            for (size_t i = 0; i < sizeof(text) - 1; ++i) {
                e.glyph_index = i;
                const FT_UInt glyph =
                    FT_Get_Char_Index(font->face, (FT_ULong)(unsigned char)text[i]);
                if (glyph == 0 ||
                    (e.freetype_error = FT_Load_Glyph(font->face, glyph, FT_LOAD_TARGET_MONO)) !=
                        0 ||
                    (e.freetype_error = FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_MONO)) !=
                        0) {
                    fail(&e, MC_CLIENT_FONT_RASTER_FAILED, "default Dialog glyph raster");
                    break;
                }
                const FT_GlyphSlot g = font->face->glyph;
                if (g->bitmap.pixel_mode != FT_PIXEL_MODE_MONO || g->bitmap.pitch < 0 ||
                    (g->advance.x % 64) != 0) {
                    fail(&e, MC_CLIENT_FONT_RASTER_FAILED,
                         "default Dialog monochrome integer glyph");
                    break;
                }
                font->glyphs[i] =
                    (mc_client_font_glyph_state){.glyph = glyph,
                                                 .width = g->bitmap.width,
                                                 .height = g->bitmap.rows,
                                                 .origin_x = x - 1,
                                                 .advance_x = g->advance.x / 64,
                                                 .bounds_x = x + g->bitmap_left,
                                                 .bounds_y = 10 - (int64_t)g->bitmap_top};
                for (unsigned int row = 0; row < g->bitmap.rows; ++row)
                    for (unsigned int col = 0; col < g->bitmap.width; ++col) {
                        const int64_t px = x + g->bitmap_left + col,
                                      py = 10 - (int64_t)g->bitmap_top + row;
                        if (px >= 0 && (uint64_t)px < image.width && py >= 0 &&
                            (uint64_t)py < image.height &&
                            (g->bitmap.buffer[(size_t)row * (size_t)g->bitmap.pitch + col / 8] &
                             (0x80u >> (col & 7))))
                            image.pixels[(size_t)py * image.stride_pixels + (size_t)px] =
                                UINT32_C(0xff000000);
                    }
                x += g->advance.x / 64;
                ++e.completed_glyphs;
                ++font->completed_glyphs;
            }
        }
        font->active = false;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
