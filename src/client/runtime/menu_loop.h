#ifndef MC_CLIENT_RUNTIME_MENU_LOOP_H
#define MC_CLIENT_RUNTIME_MENU_LOOP_H

#include "bootstrap.h"

typedef struct mc_client_menu_loop mc_client_menu_loop;
typedef struct mc_world_storage_error mc_world_storage_error;

typedef struct mc_client_menu_loop_state {
    uint64_t frames, ticks, texture_ticks, account_checks_omitted;
    size_t font_measurements, mouse_events, keyboard_events;
    bool failed, running;
    bool terminal_prepare_selected, terminal_prepared; /* Native cleanup progress only. */
    bool terminal_tail_entered;
    const char *retirement_operation; /* Native cleanup diagnosis, separate from source failure. */
    const mc_world_storage_error *retirement_world;
    mc_client_bootstrap_status retirement_bootstrap;
    const char *operation, *source_class, *source_method;
    int64_t source_pc; /* Source unsigned PCs and native -1 both retain their value. */
    /* Borrowed child diagnostics remain live until this loop retires. Gameplay
     * value diagnostics survive cleanup retry; released focus/monitor views
     * end with their successful native cleanup. */
    const struct mc_singleplayer_tick_state *controller_tick;
    const struct mc_client_display_screen_call *display_call;
    const struct mc_player_error *player;
    const struct mc_client_gameplay_input *gameplay_input;
    const struct mc_client_world_tick *world_tick;
    const struct mc_client_renderer_focus_state *renderer_focus;
    const struct mc_client_world_frame_state *world_frame;
} mc_client_menu_loop_state;

/* Native frame composition for the genuine constructed, initialized no-World
 * main menu and World selection. Borrows the complete bootstrap through retirement.
 * It uses the actual
 * App Timer, SDK, screen, statistics, sound and texture-effect owners. The
 * existing OpenGL3.3 GUI kernels present source-owned screen records. Unsupported
 * World-tick/gameplay-action/fullscreen/debug/screenshot routes retain a reached
 * diagnostic rather than entering the old inspection scene.
 *
 * Calls and callbacks require exclusive use of this whole graph on the video
 * thread. A failed frame is terminal; inspect remains available. Output stays
 * unchanged on ordinary create failure. A blocked setup cleanup returns its
 * retained failed loop for checked retirement. Retirement releases native renderer and transition
 * ownership without running source shutdown or restoring historical fields.
 * Source run's exception handlers and d() shutdown are separate dependencies. */
bool mc_client_menu_loop_create(mc_client_bootstrap *, mc_client_menu_loop **);
bool mc_client_menu_loop_frame(mc_client_menu_loop *);
bool mc_client_menu_loop_inspect(const mc_client_menu_loop *, mc_client_menu_loop_state *);
/* End this loop's source admission and drain its owned call/World-load prefix,
 * retaining screens, App, full Worlds/Players/Inputs/handlers and all services.
 * This one-way operation performs no World container/cache/provider teardown
 * and does not end shared TH/LL classes. Repeated calls retry the same slots;
 * success means only this prefix completed. Before complete retire, the outer
 * owner must separately end dispatcher admissions/captures and admit every
 * World graph. Do not use this as source World clearing or resume frames after
 * the first attempt. NULL/active or previously entered legacy retire refuses. */
bool mc_client_menu_loop_terminal_prepare(mc_client_menu_loop *);
/* Normal native shutdown through this loop's original bootstrap association.
 * End the real prefix and dispatchers, then prove no storage was acquired or
 * admit the intact selected-load World graph before entering checked cleanup.
 * Unsupported/partial loads remain live for diagnosis; no legacy fallback is
 * selected. A retained setup failure uses only its never-entered setup cleanup.
 * Once the destructive tail starts, retries continue its remaining checked
 * owner slots instead of demanding an already consumed World. Keep the same
 * bootstrap and exclusive graph alive throughout. Source failure is preserved;
 * retirement_* fields report native cleanup separately. Success nulls *slot. */
bool mc_client_menu_loop_terminal_retire(mc_client_menu_loop **);
void mc_client_menu_loop_free(mc_client_menu_loop *);
/* Separate low-level native retirement. The caller supplies outer dispatcher
 * and storage admission; normal menu-run shutdown uses terminal_retire. */
bool mc_client_menu_loop_retire(mc_client_menu_loop **);
int mc_client_menu_run(const mc_client_application_options *);

#endif
