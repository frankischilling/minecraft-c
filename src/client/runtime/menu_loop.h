#ifndef MC_CLIENT_RUNTIME_MENU_LOOP_H
#define MC_CLIENT_RUNTIME_MENU_LOOP_H

#include "bootstrap.h"

typedef struct mc_client_menu_loop mc_client_menu_loop;

typedef struct mc_client_menu_loop_state {
    uint64_t frames, ticks, texture_ticks, account_checks_omitted;
    size_t font_measurements, mouse_events, keyboard_events;
    bool failed, running;
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
void mc_client_menu_loop_free(mc_client_menu_loop *);
/* Retryable native retirement preserves the same slot and graph on refusal. */
bool mc_client_menu_loop_retire(mc_client_menu_loop **);
int mc_client_menu_run(const mc_client_application_options *);

#endif
