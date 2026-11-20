#ifndef MC_CLIENT_GUI_SOURCE_SIGN_DRAWING_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_SIGN_DRAWING_INTERNAL_H

#include "client/application_state.h"
#include "client/display/camera_surface.h"
#include "client/display/source_surface.h"
#include "client/render/blocks/internal.h"
#include "client/render_engine/texture_lookup.h"
#include "entity/player/tick_math_internal.h"
#include "mc/core/java/java.h"
#include "source_screen_initialize.h"
#include "source_sign_drawing.h"

typedef struct mc_sign_draw_font {
    struct mc_sign_draw_font *next;
    mc_client_bitmap_font *font;
} mc_sign_draw_font;

typedef struct mc_sign_draw_button {
    struct mc_sign_draw_button *next;
    mc_client_source_ke *button;
} mc_sign_draw_button;

struct mc_client_sign_draw_call {
    mc_client_sign_draw_state state;
    mc_client_sign_draw_error error, cleanup;
    mc_client_sign_draw_services services;
    mc_client_sign_draw_limits limits;
    mc_gui_mouse_position mouse;
    float partial;
    mc_client_source_da *screen;
    mc_client_source_yc *owner;
    mc_sign_draw_font *fonts;
    mc_sign_draw_button *buttons;
    size_t font_count;
    mc_cv_context query;
    mc_client_render_blocks query_fields;
    mc_client_render_global_error query_cleanup;
    mc_world_runtime *worlds[2];
    size_t world_count;
    mc_client_tile_renderer_dispatcher *dispatcher;
    mc_client_tile_renderer_draw_call *preview;
    mc_client_tile_renderer_draw_state preview_state;
    mc_client_render_engine *pending_lookup;
    mc_client_texture_lookup_error texture, texture_cleanup;
    bool exception_from_query, exception_from_preview;
};

bool mc_sign_draw_fail(mc_client_sign_draw_call *, mc_client_screen_status, const char *,
                       const char *, const char *, const char *, unsigned int, bool);
bool mc_sign_draw_gl(mc_client_sign_draw_call *, bool, const char *, const char *, const char *,
                     unsigned int);
bool mc_sign_draw_nw(mc_client_sign_draw_call *, bool, const char *, const char *, unsigned int);
bool mc_sign_draw_fp(mc_client_sign_draw_call *, const char *, const char *, const char *,
                     unsigned int);
void *mc_sign_draw_allocate(mc_client_sign_draw_call *, size_t, const char *, unsigned int);
bool mc_sign_draw_base(mc_client_sign_draw_call *, mc_client_screen_state *);
bool mc_sign_draw_owner(mc_client_sign_draw_call *, mc_client_sign_screen_state *);
bool mc_sign_draw_centered(mc_client_sign_draw_call *, mc_client_bitmap_font *,
                           const mc_player_string_view *, int32_t, int32_t, uint32_t,
                           mc_client_screen_site, int32_t *, int32_t *);
bool mc_sign_draw_gradient(mc_client_sign_draw_call *, int32_t, int32_t);
bool mc_sign_draw_buttons(mc_client_sign_draw_call *);

#endif
