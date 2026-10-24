#ifndef MC_CLIENT_GUI_FONT_RENDERER_H
#define MC_CLIENT_GUI_FONT_RENDERER_H

#include "mc/resources/image/image.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MC_FONT_RENDERER_ATLAS_SIZE 128u
#define MC_FONT_RENDERER_CELL_SIZE 8u
#define MC_FONT_RENDERER_GLYPH_EXTENT 7.99f
#define MC_FONT_RENDERER_MAX_MAPPING_UNITS 224u
#define MC_FONT_RENDERER_MAX_TEXT_UNITS 65536u

typedef struct mc_font_renderer mc_font_renderer;

typedef enum mc_font_renderer_status {
    MC_FONT_RENDERER_OK = 0,
    MC_FONT_RENDERER_INVALID_ARGUMENT,
    MC_FONT_RENDERER_LIMIT,
    MC_FONT_RENDERER_ALLOCATION_FAILED,
    MC_FONT_RENDERER_UNSUPPORTED_LOWERCASE
} mc_font_renderer_status;

typedef struct mc_font_text {
    const uint16_t *units;
    size_t length;
} mc_font_text;

typedef struct mc_font_glyph {
    uint16_t glyph_index;
    float x, y;
    uint32_t argb;
} mc_font_glyph;

typedef struct mc_font_run {
    const mc_font_glyph *glyphs;
    size_t count;
    /* Source measure result, which can differ from drawn advance for a
     * trailing section sign. It is not recomputed from the glyph records. */
    int32_t width;
} mc_font_run;

/* Copy the source-default 128x128 RGBA8 atlas advances, font.txt mapping and
 * anaglyph-dependent palette into an independent owner. Atlas rows are top-down;
 * stride is at least 512 bytes and its complete span must be readable.
 * Occupancy uses blue > 0, including transparent pixels. Alpha is not tested.
 * font_txt is decoded UTF-16: CR/LF/CRLF delimit lines, first-character '#'
 * lines are skipped and all other lines are concatenated without trimming.
 * Raw input is bounded by MAX_TEXT_UNITS; the joined mapping by 224 units.
 * Inputs are borrowed only during creation; NULL font_txt requires zero units.
 * out is required and unchanged on failure; release any previous owner before
 * replacing it on success. No file, image decode or GL call. */
mc_font_renderer_status mc_font_renderer_create(const mc_image_view *atlas,
                                                const uint16_t *font_txt, size_t units,
                                                bool anaglyph, mc_font_renderer **out);
void mc_font_renderer_free(mc_font_renderer *font); /* NULL is harmless. */

/* NULL text is a Java null String. A nonnull empty view may have NULL units.
 * Measure uses UTF-16 unit lookup/first occurrence and Java int addition;
 * every section sign skips itself and the following unit, even at the end.
 * Read-only calls may share an owner. Invalid/limited calls preserve out. */
mc_font_renderer_status mc_font_renderer_measure(const mc_font_renderer *font,
                                                 const mc_font_text *text, int32_t *out);

/* Flatten the source glyph order; shadow=true emits the complete (x+1,y+1)
 * shadow pass before foreground. Initial x/y additions wrap as Java ints, then
 * positions are binary32 and advance in source order. Each glyph uses a 7.99f
 * quad within its 8x8 atlas cell. A zero input alpha means 255; section colors
 * reset alpha to 255, as glColor3f does. Unsupported mapping units draw nothing.
 *
 * Unformatted UTF-16 and ASCII formatted text (plus section signs) are admitted.
 * A real format pair anywhere alongside any other non-ASCII unit returns
 * UNSUPPORTED_LOWERCASE: source whole-String/default-locale lowercasing remains
 * unfinished. Measurement is independent of that restriction.
 *
 * The run borrows owner scratch until its next successful layout or free.
 * Failure preserves the previous scratch contents and out. Text/out must be
 * disjoint from owner/scratch and remain live through the call. Layout/free
 * require exclusive owner access. At most twice MAX_TEXT_UNITS glyphs are kept;
 * replacement allocation retains the old buffer until success (at most two
 * full buffers live). Native allocation bounds do not reproduce Java OOME.
 * No renderer callbacks, GL/SDL calls, World access or mutable global state. */
mc_font_renderer_status mc_font_renderer_layout(mc_font_renderer *font, const mc_font_text *text,
                                                int32_t x, int32_t y, uint32_t argb, bool shadow,
                                                mc_font_run *out);

#endif
