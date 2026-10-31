#ifndef MC_CLIENT_STARTUP_GUI_INGAME_H
#define MC_CLIENT_STARTUP_GUI_INGAME_H

#include "mc/client/gui/gui_ingame.h"

typedef struct mc_client_startup_gui_ingame_call {
    bool active, allocated, constructor_returned, v_stored, retired;
    unsigned int last_pc;
    /* Namespace is borrowed from the same actual Minecraft class namespace.
     * candidate is this call's owned occurrence, including failed prefixes. */
    mc_client_gui_ingame_classes *classes;
    mc_client_source_uq *candidate;
    mc_client_gui_ingame_error error, cleanup;
} mc_client_startup_gui_ingame_call;

#endif
