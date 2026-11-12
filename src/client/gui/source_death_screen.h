#ifndef MC_CLIENT_GUI_SOURCE_DEATH_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_DEATH_SCREEN_H

#include "source_screen.h"

typedef struct mc_client_source_ch mc_client_source_ch;

typedef struct mc_client_death_screen_state {
    mc_client_screen_phase phase;
    size_t holds, initialization_count;
    unsigned int last_pc, last_button_count;
    bool last_initialization_returned, title_disabled;
    const mc_client_screen_error *failure;
} mc_client_death_screen_state;

/* Genuine CH constructor and b() initializer over the same DA namespace.
 * Its two labels are original literal Strings. Native records retain each
 * actual allocation and repeated initialization prefix until screen cleanup;
 * no replacement buttons, implicit App or source rollback are introduced. */
mc_client_screen_status mc_client_screen_death_allocate(mc_client_screen_classes *,
                                                        const mc_client_screen_options *,
                                                        mc_client_source_ch **,
                                                        mc_client_screen_error *);
mc_client_screen_status mc_client_screen_death_construct(mc_client_source_ch *,
                                                         mc_client_screen_error *);
mc_client_source_da *mc_client_screen_death_base(mc_client_source_ch *);
mc_client_screen_status mc_client_screen_death_initialize(mc_client_source_ch *,
                                                          mc_client_screen_error *);
mc_client_screen_status mc_client_screen_death_inspect(mc_client_source_ch *,
                                                       mc_client_death_screen_state *,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_death_release(mc_client_source_ch **,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_death_retire(mc_client_source_ch **,
                                                      mc_client_screen_error *);

#endif
