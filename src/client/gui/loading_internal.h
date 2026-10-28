#ifndef MC_CLIENT_GUI_LOADING_INTERNAL_H
#define MC_CLIENT_GUI_LOADING_INTERNAL_H
#include "../display/sdk_update.h"
#include "../render_engine/nw.h"
#include "../render_engine/texture_lookup.h"
#include "loading_screen.h"
#include "mc/client/gui/scaled_resolution.h"

struct mc_client_loading_classes {
    size_t holds;
    mc_player_string *empty;
    mc_client_render_engine_classes *graphics;
    const mc_registry_resources *resources;
    mc_client_font_mapping *mapping;
    mc_player_string *background_literal;
};

typedef struct mc_client_loading_call {
    mc_client_loading_error error;
    mc_client_sdk_update_error update;
    mc_client_texture_lookup_error lookup, lookup_cleanup_work, lookup_cleanup_last;
    mc_client_loading_cleanup_error cleanup;
    mc_client_bitmap_font_error font_cleanup_work;
    mc_client_bitmap_font_state font_state;
    mc_client_render_engine_state engine_state;
    mc_client_render_engine *engine;
    mc_client_bitmap_font *fonts[4];
    mc_player_string *strings[5];
    bool lookup_called, lookup_failed, lookup_retired, update_failed;
} mc_client_loading_call;

struct mc_client_loading_screen {
    size_t holds;
    mc_client_loading_classes *classes;
    mc_client_loading_phase phase;
    mc_client_loading_fields fields;
    int32_t last_store_pc;
    bool active;
    mc_client_loading_call *call;
};

mc_client_loading_call *mc_client_loading_call_create(void);
bool mc_client_loading_call_engine(mc_client_loading_screen *, mc_client_render_engine *,
                                   int32_t pc);
bool mc_client_loading_call_font(mc_client_loading_screen *, size_t slot, mc_client_bitmap_font *,
                                 int32_t pc);
bool mc_client_loading_call_string(mc_client_loading_screen *, size_t slot,
                                   const mc_player_string *, int32_t pc);
bool mc_client_loading_background_literal(mc_client_loading_screen *, const mc_player_string **);
mc_client_loading_status mc_client_loading_call_finish(mc_client_loading_screen *,
                                                       mc_client_loading_status,
                                                       mc_client_loading_error *);

/* Implemented in the dedicated normal draw file against the actual App slots
 * and concrete resource/presentation owners. No drawing callback is admitted.
 * It must return a genuine failure if any reached lower body is absent. */
mc_client_loading_status mc_client_loading_draw(mc_client_loading_screen *, int32_t,
                                                mc_client_loading_error *);
mc_client_loading_status mc_client_loading_draw_body(mc_client_loading_screen *,
                                                     mc_client_tessellator *, mc_client_display *,
                                                     const mc_scaled_resolution *, int32_t,
                                                     mc_client_texture, mc_client_loading_error *);
mc_client_loading_status mc_client_loading_projection(client_application *,
                                                      const mc_scaled_resolution *,
                                                      mc_client_loading_error *);
mc_client_loading_status mc_client_loading_progress_projection(client_application *,
                                                               const mc_scaled_resolution *,
                                                               mc_client_loading_error *);

/* The source clock seam is the existing concrete platform function. Tests may
 * link a separately pinned implementation of that exact symbol; production
 * always links platform/time/millis.c. No clock is supplied by a caller. */
mc_client_loading_status mc_client_loading_fail(mc_client_loading_error *, mc_client_loading_status,
                                                const char *, const char *, const char *, int32_t);
mc_client_loading_status mc_client_loading_resolution(client_application *, mc_scaled_resolution *,
                                                      mc_client_loading_error *, int32_t);
#endif
