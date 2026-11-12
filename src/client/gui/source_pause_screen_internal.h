#ifndef MC_CLIENT_GUI_SOURCE_PAUSE_SCREEN_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_PAUSE_SCREEN_INTERNAL_H

#include "source_pause_screen.h"
#include "source_screen_internal.h"

typedef struct mc_pause_initialization {
    struct mc_pause_initialization *next;
    mc_client_source_ke *buttons[5];
    unsigned int button_count;
    bool returned, multiplayer;
    mc_client_screen_error failure;
} mc_pause_initialization;

struct mc_client_source_oz {
    mc_client_source_da base;
    int32_t save_pass, ticks; /* Actual private oz.a/i, distinct from inherited da.a. */
    unsigned int last_pc;
    mc_pause_initialization *initializations, *last_initialization;
    size_t initialization_count;
    mc_client_screen_error failure;
};

bool mc_screen_pause_initialize_locked(mc_client_source_oz *, mc_client_screen_error *);
bool mc_screen_pause_cleanup_locked(mc_client_source_oz *, mc_client_screen_error *);
void mc_screen_pause_tick_locked(mc_client_source_oz *, mc_client_screen_error *);

#endif
