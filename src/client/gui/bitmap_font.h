#ifndef MC_CLIENT_GUI_BITMAP_FONT_H
#define MC_CLIENT_GUI_BITMAP_FONT_H
#include "../images/class_stream.h"
#include "../render_engine/font_lease.h"
#include "../render_engine/textures.h"
#include "font_mapping.h"

typedef struct mc_client_loading_classes mc_client_loading_classes;
typedef struct mc_client_bitmap_font mc_client_bitmap_font;

typedef enum mc_client_bitmap_font_phase {
    MC_CLIENT_BITMAP_FONT_RAW,
    MC_CLIENT_BITMAP_FONT_CONSTRUCTING,
    MC_CLIENT_BITMAP_FONT_READY,
    MC_CLIENT_BITMAP_FONT_FAILED,
    MC_CLIENT_BITMAP_FONT_RETIRING
} mc_client_bitmap_font_phase;

typedef enum mc_client_bitmap_font_status {
    MC_CLIENT_BITMAP_FONT_OK,
    MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT,
    MC_CLIENT_BITMAP_FONT_INVALID_PHASE,
    MC_CLIENT_BITMAP_FONT_BUSY,
    MC_CLIENT_BITMAP_FONT_LIMIT,
    MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED,
    MC_CLIENT_BITMAP_FONT_NULL_REFERENCE,
    MC_CLIENT_BITMAP_FONT_ARRAY_INDEX,
    MC_CLIENT_BITMAP_FONT_MAPPING_FAILED,
    MC_CLIENT_BITMAP_FONT_RENDER_FAILED,
    MC_CLIENT_BITMAP_FONT_DISPLAY_FAILED,
    MC_CLIENT_BITMAP_FONT_UNSUPPORTED_LOWERCASE,
    MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY,
    MC_CLIENT_BITMAP_FONT_SOURCE_RUNTIME_EXCEPTION,
    MC_CLIENT_BITMAP_FONT_SOURCE_ILLEGAL_ARGUMENT,
    MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP
} mc_client_bitmap_font_status;

typedef struct mc_client_bitmap_font_error {
    mc_client_bitmap_font_status status;
    const char *operation, *method, *descriptor;
    int32_t pc, actual_index;
    const char *parent_method, *parent_descriptor;
    int32_t parent_pc;
    const char *entry_method, *entry_descriptor;
    int32_t entry_pc;
    bool native_policy;
    mc_client_font_mapping_error mapping;
    mc_client_render_engine_error owner;
    mc_client_render_texture_error texture;
    mc_client_display_error display;
    mc_client_graphics_error graphics;
    mc_client_graphics_error cleanup_graphics;
    bool cleanup_failed;
    mc_client_class_stream_error stream;
    mc_client_image_io_error image;
} mc_client_bitmap_font_error;

typedef struct mc_client_bitmap_font_fields {
    const int32_t *b;
    int32_t a, c;
    const mc_client_direct_int_buffer *d;
} mc_client_bitmap_font_fields;

typedef struct mc_client_bitmap_font_state {
    mc_client_bitmap_font_phase phase;
    mc_client_bitmap_font_fields fields;
    size_t holds, constructor_bytes, constructor_allocations;
    unsigned widths_completed, glyphs_defined, palettes_defined;
    int32_t last_store_pc;
    bool active, has_native_generation;
    const mc_client_argb_image *retained_image;
    mc_client_bitmap_font_error failure;
} mc_client_bitmap_font_state;

/* Genuine sj owner on the same cold GUI/ge/nw context retained before mk.
 * Allocation performs no source field initialization, resource active use,
 * display/GL call or fp initialization. Constructor preserves the complete
 * b8/a15/d22/resource31/image34/register242/a245/range252/c255/nw258/glyph/
 * late Settings580/palette sequence. Null Engine/Settings/name are reached at
 * their real use, never rejected eagerly as fabricated completed children.
 * Actual resources/Settings remain live through retained failed prefixes.
 * Native quota is separate from Java OOME scheduling. No constructor retry. */
mc_client_bitmap_font_status mc_client_bitmap_font_allocate(mc_client_loading_classes *,
                                                            const mc_client_render_engine_options *,
                                                            mc_client_bitmap_font **out);
mc_client_bitmap_font_status
mc_client_bitmap_font_construct(mc_client_bitmap_font *, mc_game_settings *nullable_settings,
                                mc_player_string *nullable_name,
                                mc_client_render_engine *nullable_engine,
                                mc_client_display *actual_display, mc_client_bitmap_font_error *);
/* Native retain returns BUSY during construction or an active source/terminal operation and
 * INVALID_PHASE for a retained RETIRING owner between failed cleanup attempts.
 * It never resurrects terminal ownership or gains a hidden callback hold. */
mc_client_bitmap_font_status mc_client_bitmap_font_retain(mc_client_bitmap_font *);
/* Native occurrence release: success consumes and clears this slot, including
 * when other counted holds keep the same font alive. BUSY/failure preserves
 * the slot and its hold. Final resource retirement can partially complete;
 * retry that same retained slot after the reported dependency is released.
 * External native close runs under the font active guard: nested release and
 * retain return BUSY without consuming slots or changing holds. Retained
 * RETIRING owners cannot acquire a new hold between failed cleanup attempts. */
mc_client_bitmap_font_status mc_client_bitmap_font_release(mc_client_bitmap_font **,
                                                           mc_client_bitmap_font_error *);
bool mc_client_bitmap_font_inspect(const mc_client_bitmap_font *, mc_client_bitmap_font_state *);

mc_client_bitmap_font_status mc_client_bitmap_font_measure(mc_client_bitmap_font *,
                                                           const mc_player_string *nullable_text,
                                                           int32_t *out,
                                                           mc_client_bitmap_font_error *);
mc_client_bitmap_font_status
mc_client_bitmap_font_draw_shadow(mc_client_bitmap_font *, const mc_player_string *nullable_text,
                                  int32_t x, int32_t y, uint32_t argb,
                                  mc_client_bitmap_font_error *);
/* Actual sj.a(String,IIIZ). Null text returns before bind, floating point,
 * buffer operations or fp active use. Failure preserves source GL/NIO state. */
mc_client_bitmap_font_status mc_client_bitmap_font_draw(mc_client_bitmap_font *,
                                                        const mc_player_string *nullable_text,
                                                        int32_t x, int32_t y, uint32_t argb,
                                                        bool shadow, mc_client_bitmap_font_error *);
#endif
