#ifndef MC_CLIENT_GUI_SOURCE_SIGN_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_SIGN_DRAWING_H

#include "bitmap_font.h"
#include "mc/client/gui/scaled_resolution.h"
#include "mc/client/render/tile_entity_renderer/draw.h"
#include "source_sign_screen.h"

typedef struct mc_client_sign_draw_call mc_client_sign_draw_call;

typedef struct mc_client_sign_draw_services {
    mc_client_manager_draw_services draw;
    mc_client_render_classes *models;
    mc_world_runtime_classes *world_classes;
    mc_registry_classes *registry;
    mc_java_random_profile *profile;
} mc_client_sign_draw_services;

typedef struct mc_client_sign_draw_limits {
    size_t max_bytes, max_buttons, max_fonts;
    mc_client_tile_renderer_draw_limits preview;
} mc_client_sign_draw_limits;

typedef struct mc_client_sign_draw_error {
    mc_client_screen_error screen;
    mc_client_display_error display;
    mc_client_graphics_error graphics;
    mc_client_bitmap_font_error font;
    mc_client_tile_renderer_error dispatcher;
    mc_client_tile_renderer_draw_error preview;
    mc_registry_error registry;
    mc_tile_entity_status tile;
    const mc_client_texture_lookup_error *texture;
    const mc_client_render_global_error *query;
    const mc_cv_world_error *query_world;
    mc_client_screen_site caller, helper;
} mc_client_sign_draw_error;

typedef struct mc_client_sign_draw_state {
    bool active, returned, retiring, background_returned, title_returned;
    bool matrix_pushed, matrix_popped, block_returned, metadata_returned, standing;
    bool marker_stored, marker_cleared, preview_returned, buttons_returned;
    int32_t tick, selected_row, metadata, title_width, title_x;
    float angle;
    size_t buttons_seen, buttons_drawn, font_measurements, font_draws, allocation_bytes;
    unsigned int source_pc;
    mc_client_source_yc *receiver; /* Borrowed while this call's Screen hold is live. */
    const mc_client_tile_renderer_draw_state *preview;
    const mc_client_sign_draw_error *failure, *cleanup;
} mc_client_sign_draw_state;

mc_client_sign_draw_limits mc_client_source_sign_draw_default_limits(void);
/* Actual YC.a(IIF), with real UB/NW background, captured DA.g title, LL
 * coordinates preview and ordinary KE draws using each fresh App.q operand.
 * Caller preserves the actual incoming projection/GL state; the ordinary
 * runtime has its reached PX setup, while direct components may use genuine
 * incoming defaults. No numeric projection gate is added. Explicit code
 * services and their complete live graph remain exclusive through retirement.
 * No Canvas glyph-only substitute, frame setup, swap, input, time sampling,
 * source close, marker cleanup or GL unwind is inserted. out starts NULL;
 * after allocation even a failed call is published for exact-prefix inspection.
 * The call owns a generic Screen hold and every reached child occurrence; end
 * it before YC terminal preparation and dispatcher/storage retirement. Failed
 * child views, including producer-owned exception names, expire as that child
 * is consumed during partial retirement; Source flags/sites/scalars remain.
 * The no-World DA dirt-background descendant remains an explicit frontier at
 * DA.a(I)32; the ordinary loaded-World gradient path executes its real body. */
mc_client_screen_status mc_client_source_sign_draw_begin(mc_client_source_yc *,
                                                         mc_gui_mouse_position, float partial,
                                                         const mc_client_sign_draw_services *,
                                                         const mc_client_sign_draw_limits *,
                                                         mc_client_sign_draw_call **out,
                                                         mc_client_sign_draw_error *);
bool mc_client_source_sign_draw_inspect(const mc_client_sign_draw_call *,
                                        mc_client_sign_draw_state *);
mc_client_screen_status mc_client_source_sign_draw_retire(mc_client_sign_draw_call **,
                                                          mc_client_sign_draw_error *);

#endif
