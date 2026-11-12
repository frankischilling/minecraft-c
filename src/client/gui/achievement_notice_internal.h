#ifndef MC_CLIENT_GUI_ACHIEVEMENT_NOTICE_INTERNAL_H
#define MC_CLIENT_GUI_ACHIEVEMENT_NOTICE_INTERNAL_H
#include "mc/client/gui/achievement_notice.h"

/* The CPU constructor/state owner does not reference runtime rendering code. */
struct mc_client_achievement_notice {
    mc_client_render_classes *models;
    mc_registry_classes *registry;
    mc_client_model_phase phase;
    mc_client_achievement_notice_fields fields;
    mc_client_description_result description;
    mc_client_item_renderer *item;
    bool draw_active;
    struct mc_client_notice_draw_call *draw_call;
};
#endif
