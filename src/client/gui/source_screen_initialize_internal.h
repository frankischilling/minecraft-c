#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_INITIALIZE_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_INITIALIZE_INTERNAL_H
#include "source_screen_initialize.h"
#include "source_screen_internal.h"

struct mc_client_screen_initialization {
    struct mc_client_screen_initialization *next;
    mc_client_screen_init_phase phase;
    bool da_entry, app_held, app_counted, date_allocated, children_retired;
    client_application *app;
    mc_client_source_du *particles;
    mc_client_bitmap_font *font;
    mc_platform_calendar *calendar;
    mc_platform_date date;
    unsigned int da_pc, fu_pc, store_count, button_count;
    mc_client_screen_site stores[8];
    mc_client_source_ke *buttons[5], *replaced_l;
    const mc_translation *translation;
    bool applet_branch, session_present;
    int32_t row_origin;
    mc_client_screen_init_error failure, cleanup;
};
#endif
