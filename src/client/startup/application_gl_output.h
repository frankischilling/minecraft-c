#ifndef MC_CLIENT_STARTUP_APPLICATION_GL_OUTPUT_H
#define MC_CLIENT_STARTUP_APPLICATION_GL_OUTPUT_H

#include "application_gl_diagnostics.h"
#include "diagnostic_builder.h"
#include "mc/core/java/print_stream.h"

typedef struct mc_client_gl_output_call mc_client_gl_output_call;

typedef enum mc_client_gl_output_status {
    MC_CLIENT_GL_OUTPUT_OK,
    MC_CLIENT_GL_OUTPUT_INVALID_ARGUMENT,
    MC_CLIENT_GL_OUTPUT_INVALID_PHASE,
    MC_CLIENT_GL_OUTPUT_BUSY,
    MC_CLIENT_GL_OUTPUT_ALLOCATION_FAILED,
    MC_CLIENT_GL_OUTPUT_DISPLAY_FAILED,
    MC_CLIENT_GL_OUTPUT_SYSTEM_FAILED,
    MC_CLIENT_GL_OUTPUT_BUILDER_FAILED,
    MC_CLIENT_GL_OUTPUT_STRING_FAILED,
    MC_CLIENT_GL_OUTPUT_NULL_PRINT_STREAM
} mc_client_gl_output_status;

typedef struct mc_client_gl_output_error {
    mc_client_gl_output_status status;
    const char *operation;
    mc_client_application_owner_site site;
    bool native_policy, source_returned;
    mc_client_application_gl_diagnostic_error prefix;
    mc_java_print_error system;
    mc_java_print_error lines[3];
    mc_client_diagnostic_builder_error builder;
    mc_player_status string;
    mc_client_display_error display;
} mc_client_gl_output_error;

typedef struct mc_client_gl_output_state {
    bool started, active, retiring;
    unsigned int captured_receivers, normal_println_returns;
    const mc_java_print_stream *receivers[3];
    const mc_player_string *results[2];
    const mc_client_diagnostic_builder *builders[2];
    const mc_client_gl_output_error *error;
    const mc_java_print_error *first_cleanup_error;
} mc_client_gl_output_state;

/* One retained source call. Caller supplies the genuine current Display and
 * canonical System-output namespace, whose lifetime is separately retained.
 * Does not create a default stream, attach an App field, adopt a coordinator
 * or execute caller383. Base-class PrintStream only; no Java subclass success.
 * The inherited prefix queries GetError EXACTLY ONCE. Zero returns75 without
 * a System.out read. Nonzero performs fresh receiver reads13/21/46, each before
 * its arguments; nullable receivers fail only at18/43/72 after the arguments.
 * All evaluated arguments, actual receiver occurrences, pending builders and
 * full causes stay owned by this call until explicit native retirement.
 * Failure cannot be resumed/re-run and never drains another GL error. */
mc_client_gl_output_status mc_client_gl_output_allocate(mc_client_gl_output_call **,
                                                        mc_client_gl_output_error *);
mc_client_gl_output_status mc_client_gl_output_run(mc_client_gl_output_call *, mc_client_display *,
                                                   mc_java_system_output *,
                                                   const mc_player_string *nullable_label);
bool mc_client_gl_output_inspect(const mc_client_gl_output_call *, mc_client_gl_output_state *);
/* No source close/flush. Active is BUSY. Native child BUSY preserves the
 * exact occurrence and permits retirement retry, never a source-call retry.
 * Caller keeps the returned error disjoint from the owner through cleanup. */
mc_client_gl_output_status mc_client_gl_output_retire(mc_client_gl_output_call **,
                                                      mc_client_gl_output_error *);

#endif
