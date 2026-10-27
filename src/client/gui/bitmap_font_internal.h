#ifndef MC_CLIENT_GUI_BITMAP_FONT_INTERNAL_H
#define MC_CLIENT_GUI_BITMAP_FONT_INTERNAL_H
#include "../render_engine/arena.h"
#include "bitmap_font.h"
#include "loading_internal.h"

struct mc_client_bitmap_font {
    size_t holds;
    mc_client_bitmap_font_phase phase;
    mc_client_bitmap_font_fields fields;
    mc_client_loading_classes *gui;
    mc_client_render_arena arena;
    mc_client_class_stream *stream;
    mc_client_argb_image *image;
    mc_client_font_lease *lease;
    mc_client_render_engine *retained_engine;
    mc_player_string *retained_name;
    mc_game_settings *borrowed_settings;
    mc_client_display *display;
    unsigned widths_completed, glyphs_defined, palettes_defined;
    int32_t last_store_pc;
    bool active;
    mc_client_bitmap_font_error failure;
};

mc_client_bitmap_font_status mc_client_bitmap_font_fail(mc_client_bitmap_font_error *,
                                                        mc_client_bitmap_font_status, const char *,
                                                        const char *, const char *, int32_t, bool);
mc_client_bitmap_font_status mc_client_bitmap_font_admit(mc_client_bitmap_font *, const char *,
                                                         const char *,
                                                         mc_client_bitmap_font_error *);
mc_client_bitmap_font_status
mc_client_bitmap_font_call_enter(mc_client_bitmap_font *, const mc_player_string *, const char *,
                                 const char *, mc_player_string_view *,
                                 mc_client_bitmap_font_error **, mc_client_bitmap_font_error *);
mc_client_bitmap_font_status mc_client_bitmap_font_call_leave(mc_client_bitmap_font *,
                                                              const mc_player_string *,
                                                              mc_client_bitmap_font_error *,
                                                              mc_client_bitmap_font_error *);
#endif
