#ifndef MC_CLIENT_STARTUP_APPLICATION_CONSTRUCTION_INTERNAL_H
#define MC_CLIENT_STARTUP_APPLICATION_CONSTRUCTION_INTERNAL_H
#include "../application_state.h"
#include "application_construction.h"

struct mc_client_application_construction {
    mc_client_application_constructor_options options;
    mc_client_loading_classes *gui;
    mc_sound_manager_shared *sound_shared;
    mc_sound_backend *sound_backend; /* concrete external borrowed context */
    bool registry_hold, entity_hold, render_hold, gui_hold;
    bool retirement_active;
    mc_timer *timer_candidate;
    mc_client_achievement_notice *notice_candidate;
    mc_client_biped_owner *biped_candidate;
    mc_sound_manager *sound_candidate;
    mc_client_lava_texture_effect *lava_candidate;
    mc_client_timer_hack *worker;
    mc_client_application_construction_error failure, retirement;
    size_t source_stores;
    int32_t stores[64];
    int32_t last_source_store;
};

mc_client_application_construction_status
application_constructor_error(mc_client_application_construction_error *,
                              mc_client_application_construction_status,
                              mc_client_application_child, int32_t, const char *);
void application_constructor_copy_error(mc_client_application_construction_error *,
                                        const mc_client_application_construction_error *);
#endif
