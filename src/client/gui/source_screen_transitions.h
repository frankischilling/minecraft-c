#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_H

#include "source_screen.h"

typedef struct mc_client_screen_transitions mc_client_screen_transitions;
typedef struct mc_client_source_rq mc_client_source_rq;

typedef struct mc_client_screen_transition_state {
    bool active, retiring, slot_handed_off;
    size_t screens, calls;
    const mc_client_source_da *initial_screen;
    const mc_client_screen_error *failure;
} mc_client_screen_transition_state;

/* Native ownership for actual synchronous screen calls on one genuine App.
 * The completed startup owns its initial FU. This module retains later screens,
 * constructor/display failure prefixes and their exact parent identities (up
 * to 4096 native records; screen arenas have their own explicit limits). It
 * leases the complete startup graph through successful retirement. All calls
 * require exclusive use on the video thread; no source call is deferred to a
 * later tick/frame. Error storage is mandatory, disjoint and caller-owned.
 *
 * Button callers keep their ORIGINAL receiver/list alive through the entire
 * source input drain after this call changes App.r. The caller releases the
 * class monitor before entering; its active source-call occurrence must remain
 * held. Selection during MG drawing has the same synchronous contract. */
mc_client_screen_status mc_client_screen_transitions_create(client_application *,
                                                            mc_client_screen_transitions **,
                                                            mc_client_screen_error *);
mc_client_screen_status mc_client_screen_transitions_button(mc_client_screen_transitions *,
                                                            mc_client_source_da *,
                                                            mc_client_source_ke *,
                                                            mc_client_screen_error *);
mc_client_screen_status mc_client_screen_transitions_select(mc_client_screen_transitions *,
                                                            mc_client_source_rq *, int32_t index,
                                                            mc_client_screen_error *);
bool mc_client_screen_transitions_inspect(const mc_client_screen_transitions *,
                                          mc_client_screen_transition_state *);
/* Native terminal cleanup, with no source close/save/r rollback. External
 * descendants or active calls preserve the whole remaining dependency graph. */
mc_client_screen_status mc_client_screen_transitions_retire(mc_client_screen_transitions **,
                                                            mc_client_screen_error *);

#endif
