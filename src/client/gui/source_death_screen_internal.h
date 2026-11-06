#ifndef MC_CLIENT_GUI_SOURCE_DEATH_SCREEN_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_DEATH_SCREEN_INTERNAL_H

#include "source_death_screen.h"
#include "source_screen_internal.h"

typedef struct mc_death_initialization {
    struct mc_death_initialization *next;
    mc_client_source_ke *buttons[2];
    unsigned int button_count;
    bool returned, title_disabled;
    mc_client_screen_error failure;
} mc_death_initialization;

struct mc_client_source_ch {
    mc_client_source_da base;
    unsigned int last_pc;
    mc_death_initialization *initializations, *last_initialization;
    size_t initialization_count;
    mc_client_screen_error failure;
};

bool mc_screen_death_initialize_locked(mc_client_source_ch *, mc_client_screen_error *);
bool mc_screen_death_cleanup_locked(mc_client_source_ch *, mc_client_screen_error *);

#endif
