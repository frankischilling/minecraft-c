#ifndef MC_CLIENT_GUI_LOADING_INTERNAL_H
#define MC_CLIENT_GUI_LOADING_INTERNAL_H
#include "loading_screen.h"
#include "mc/client/gui/scaled_resolution.h"

struct mc_client_loading_classes {
    size_t holds;
    mc_player_string *empty;
    mc_client_render_engine_classes *graphics;
    const mc_registry_resources *resources;
    mc_client_font_mapping *mapping;
};

struct mc_client_loading_screen {
    size_t holds;
    mc_client_loading_classes *classes;
    mc_client_loading_phase phase;
    mc_client_loading_fields fields;
    int32_t last_store_pc;
    bool active;
};

/* Implemented in the dedicated normal draw file against the actual App slots
 * and concrete resource/presentation owners. No drawing callback is admitted.
 * It must return a genuine failure if any reached lower body is absent. */
mc_client_loading_status mc_client_loading_draw(mc_client_loading_screen *, int32_t,
                                                mc_client_loading_error *);
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
