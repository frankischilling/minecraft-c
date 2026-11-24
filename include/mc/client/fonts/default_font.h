#ifndef MC_CLIENT_FONTS_DEFAULT_FONT_H
#define MC_CLIENT_FONTS_DEFAULT_FONT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_client_default_font mc_client_default_font;

typedef enum mc_client_font_status {
    MC_CLIENT_FONT_OK,
    MC_CLIENT_FONT_INVALID_ARGUMENT,
    MC_CLIENT_FONT_LIMIT,
    MC_CLIENT_FONT_ALLOCATION_FAILED,
    MC_CLIENT_FONT_DISCOVERY_FAILED,
    MC_CLIENT_FONT_IO_FAILED,
    MC_CLIENT_FONT_RASTER_FAILED,
    MC_CLIENT_FONT_FAILED,
    MC_CLIENT_FONT_BUSY
} mc_client_font_status;

typedef struct mc_client_font_error {
    mc_client_font_status status;
    const char *operation;
    unsigned long system_error;
    int freetype_error;
    size_t glyph_index, completed_glyphs;
} mc_client_font_error;

typedef struct mc_client_font_glyph_state {
    uint32_t glyph, width, height;
    int64_t origin_x, advance_x, bounds_x, bounds_y;
} mc_client_font_glyph_state;

typedef struct mc_client_font_snapshot {
    size_t native_holds, font_bytes;
    bool ready, failed;
    int freetype_major, freetype_minor, freetype_patch;
    char family[96];
    size_t completed_glyphs;
    mc_client_font_glyph_state glyphs[10];
} mc_client_font_snapshot;

typedef struct mc_client_font_argb_view {
    uint32_t *pixels;
    size_t width, height, stride_pixels;
} mc_client_font_argb_view;

/* Concrete default Dialog/plain/12 native provider for the reached ASCII
 * missingtex draw. Creation allocates an inert owner; real system discovery,
 * font bytes and FreeType activate only at the draw. Windows resolves Arial
 * through GDI (the selected OpenJDK Windows Dialog alphabetic mapping), while
 * Linux performs the real Fontconfig sans match. No captured glyph/image,
 * bundled font, Java process or provider fallback is used. Native policy uses
 * FreeType monochrome hinting/default interpreter35 (honoring the actual
 * FreeType environment interpreter override), independently qualified against
 * the actual JDK profiles. This does not implement every logical-font fallback.
 *
 * max_font_bytes==0 admits no font; SIZE_MAX is allowed. Native FreeType/config
 * internals have their own allocations, outside this source-byte bound.
 * Keep owners exclusive/live. Failed activation is retained and cannot retry.
 * Release every retained hold; final release retires face/library/font bytes. */
mc_client_font_status mc_client_default_font_create(size_t max_font_bytes,
                                                    mc_client_default_font **out);
mc_client_font_status mc_client_default_font_retain(mc_client_default_font *font);
mc_client_font_status mc_client_default_font_release(mc_client_default_font *font);
bool mc_client_default_font_inspect(const mc_client_default_font *font,
                                    mc_client_font_snapshot *out);

/* Real black, non-antialiased default-font drawString("missingtex",1,10).
 * The actual Graphics/image owner supplies its live mutable ARGB32 storage;
 * existing pixels are preserved except covered glyph bits. Clip to that image.
 * Native failures preserve every completed glyph/pixel prefix and report it.
 * This native failure contract is distinct from Java font-manager recovery.
 * Output/errors/views must be disjoint from dependencies and owner storage. */
mc_client_font_status mc_client_default_font_draw_missingtex(mc_client_default_font *font,
                                                             mc_client_font_argb_view image,
                                                             mc_client_font_error *error);

#endif
