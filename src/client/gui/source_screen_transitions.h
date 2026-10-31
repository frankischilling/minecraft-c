#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_H

#include "source_screen.h"

typedef struct mc_client_screen_transitions mc_client_screen_transitions;
typedef struct mc_client_source_rq mc_client_source_rq;
typedef struct mc_client_world_load_call mc_client_world_load_call;

typedef struct mc_client_screen_transition_state {
    bool active, retiring, slot_handed_off;
    size_t screens, calls;
    const mc_client_source_da *initial_screen;
    const mc_client_screen_error *failure;
    const mc_client_world_load_call *world_load; /* Actual last retained invocation, borrowed. */
    bool terminal_prepare_selected, terminal_prepared; /* Native one-way prefix only. */
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
/* Actual Minecraft.i(): a nonnull current r returns7 before NEW9. Otherwise
 * construct genuine OZ at13 and display it at16, retaining every prefix.
 * close executes inherited da.a(CI) Escape display(NULL)10 then focus17. */
mc_client_screen_status mc_client_screen_transitions_pause(mc_client_screen_transitions *,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_transitions_close(mc_client_screen_transitions *,
                                                           mc_client_source_da *,
                                                           mc_client_screen_error *);
bool mc_client_screen_transitions_inspect(const mc_client_screen_transitions *,
                                          mc_client_screen_transition_state *);
/* Passive native borrow of this owner's SAME retained screen namespace.
 * NULL-safe; keep the transition owner live until every use of the borrow and
 * its retained source-call prefixes has ended. No source operation runs. */
mc_client_screen_classes *
mc_client_screen_transitions_classes(const mc_client_screen_transitions *);
/* Native one-way admission/prefix ending before outer dispatcher retirement.
 * Reject active entry, latch this transition graph and its actual World-load
 * class admission, and terminal_prepare every retained load. Keep all nodes,
 * screens, controller, App borrows and full World/Player/Input/handler graphs.
 * No complete load/World retirement, global dispatcher action or source call.
 * NULL receiver/error is INVALID_ARGUMENT. Failures retain the same graph and
 * remaining rights for idempotent retry. A legacy retire already entered cannot
 * be upgraded (BUSY). Child cleanup views expire when consumed; inspect each
 * retained load afresh after a preparation attempt. Source failure is unchanged.
 * Success does not authorize destructive World cleanup by itself. */
mc_client_screen_status
mc_client_screen_transitions_terminal_prepare(mc_client_screen_transitions *,
                                              mc_client_screen_error *);
/* Native terminal cleanup, with no source close/save/r rollback. External
 * descendants or active calls preserve the whole remaining dependency graph. */
mc_client_screen_status mc_client_screen_transitions_retire(mc_client_screen_transitions **,
                                                            mc_client_screen_error *);

#endif
