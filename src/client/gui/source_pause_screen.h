#ifndef MC_CLIENT_GUI_SOURCE_PAUSE_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_PAUSE_SCREEN_H

#include "source_screen.h"

typedef struct mc_client_source_oz mc_client_source_oz;

typedef struct mc_client_pause_screen_state {
    mc_client_screen_phase phase;
    size_t holds, initialization_count;
    int32_t save_pass, ticks;
    unsigned int last_pc, last_button_count;
    bool last_initialization_returned, multiplayer_label;
    const mc_client_screen_error *failure;
} mc_client_pause_screen_state;

/* Genuine oz() constructor: da1, a=0 at6, i=0 at11, return14. This uses the
 * same DA representation and namespace; no FU Random/splash activation occurs.
 * Failed constructors and repeated initialization prefixes remain owned until
 * common screen retirement. Native cleanup never replays source close/save. */
mc_client_screen_status mc_client_screen_pause_allocate(mc_client_screen_classes *,
                                                        const mc_client_screen_options *,
                                                        mc_client_source_oz **,
                                                        mc_client_screen_error *);
mc_client_screen_status mc_client_screen_pause_construct(mc_client_source_oz *,
                                                         mc_client_screen_error *);
mc_client_source_da *mc_client_screen_pause_base(mc_client_source_oz *);
/* Actual oz.b() only. Ordinary callers use common da.a(App,II), including
 * its separate clear before this body's a reset and second clear. i survives
 * reinitialization. The real five Buttons and literal/translated labels are
 * retained in their original order, including the remote Disconnect rewrite. */
mc_client_screen_status mc_client_screen_pause_initialize(mc_client_source_oz *,
                                                          mc_client_screen_error *);
/* Actual oz.a() tick through the same locked body used by virtual DA dispatch.
 * Native admission requires an idle READY pause owner and a live namespace;
 * no initialization, event/audio owner, save or source field other than i is
 * touched. The Java-wrapping increment returns at14. */
mc_client_screen_status mc_client_screen_pause_tick(mc_client_source_oz *,
                                                    mc_client_screen_error *);
mc_client_screen_status mc_client_screen_pause_inspect(mc_client_source_oz *,
                                                       mc_client_pause_screen_state *,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_pause_release(mc_client_source_oz **,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_pause_retire(mc_client_source_oz **,
                                                      mc_client_screen_error *);

#endif
