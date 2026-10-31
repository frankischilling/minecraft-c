#ifndef MC_CLIENT_STARTUP_APPLICATION_GRAPHICS_H
#define MC_CLIENT_STARTUP_APPLICATION_GRAPHICS_H

#include "application_gl_output.h"

typedef struct mc_client_startup_graphics_call mc_client_startup_graphics_call;

/* Complete scalar/static-text causes survive consumed call retirement. The
 * stream observations are final per-receiver states, not per-line deltas.
 * Results/builders remain borrowed only from each still-live full call. */
typedef struct mc_client_startup_diagnostic_snapshot {
    bool valid, receiver_valid[3];
    unsigned int captured_receivers, normal_println_returns;
    mc_client_gl_output_error error;
    mc_java_print_state receivers[3];
} mc_client_startup_diagnostic_snapshot;

/* Passive invocation history and native owned call occurrences. No App field,
 * source successful return, later constructor or event work is represented. */
struct mc_client_startup_graphics_call {
    bool active, entered, setup_completed, retired;
    unsigned int last_pc, setup_calls_completed, diagnostic_calls_completed;
    mc_java_system_output *namespace_identity; /* Borrowed actual class service. */
    mc_player_string *labels[2];               /* Borrowed actual MC class literals. */
    mc_client_gl_output_call *diagnostics[2];
    mc_client_startup_diagnostic_snapshot snapshots[2];
    mc_client_display_error display;
    mc_client_gl_output_error allocation_error, cleanup_error;
    mc_java_print_error namespace_error;
};

#endif
