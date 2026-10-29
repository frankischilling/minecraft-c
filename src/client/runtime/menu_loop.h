#ifndef MC_CLIENT_RUNTIME_MENU_LOOP_H
#define MC_CLIENT_RUNTIME_MENU_LOOP_H

#include "bootstrap.h"

typedef struct mc_client_menu_loop mc_client_menu_loop;

typedef struct mc_client_menu_loop_state {
    uint64_t frames, ticks, texture_ticks, account_checks_omitted;
    size_t font_measurements, mouse_events, keyboard_events;
    bool failed, running;
    const char *operation, *source_class, *source_method;
    unsigned int source_pc;
} mc_client_menu_loop_state;

/* Native frame composition for the genuine constructed, initialized no-World
 * main menu. Borrows the complete bootstrap through free. It uses the actual
 * App Timer, SDK, screen, statistics, sound and texture-effect owners. The
 * existing OpenGL3.3 GUI kernels present source-owned screen records. Unsupported
 * World/controller/focus/fullscreen/debug/screenshot routes retain a reached
 * diagnostic rather than entering the old inspection scene.
 *
 * Calls and callbacks require exclusive use of this whole graph on the video
 * thread. A failed frame is terminal; inspect remains available. Output stays
 * unchanged on create failure. Free performs only native renderer cleanup.
 * Source run's exception handlers and d() shutdown are separate dependencies. */
bool mc_client_menu_loop_create(mc_client_bootstrap *, mc_client_menu_loop **);
bool mc_client_menu_loop_frame(mc_client_menu_loop *);
bool mc_client_menu_loop_inspect(const mc_client_menu_loop *, mc_client_menu_loop_state *);
void mc_client_menu_loop_free(mc_client_menu_loop *);
int mc_client_menu_run(const mc_client_application_options *);

#endif
