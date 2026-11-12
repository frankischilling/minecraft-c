#ifndef MC_CLIENT_STARTUP_MENU_H
#define MC_CLIENT_STARTUP_MENU_H

#include "../gui/display_screen.h"

typedef struct mc_client_startup_menu_call {
    bool active, allocated, constructed, r_stored, source_startup_returned, retired;
    bool slot_handed_off; /* Native runtime owns later r publications. */
    unsigned int last_pc;
    mc_client_screen_classes *classes; /* Borrowed from the actual App class namespace. */
    mc_client_source_fu *candidate;
    mc_client_display_screen_call *display;
    mc_client_screen_error error, cleanup;
} mc_client_startup_menu_call;

#endif
