#ifndef MC_CLIENT_STARTUP_APPLICATION_GL_DIAGNOSTICS_H
#define MC_CLIENT_STARTUP_APPLICATION_GL_DIAGNOSTICS_H

#include "../display/source_surface.h"
#include "application_owner.h"
#include "mc/entity/player/player.h"

/* Counted immutable UTF16 literal VALUE, not a new String allocation or a
 * replacement global intern pool. NULL and an empty String remain distinct.
 * These finite GLU/Util literals need no mutable owner and stay live forever;
 * no allocation/OOM scheduling or literal-identity observation is claimed. */
typedef struct mc_client_glu_error_text {
    mc_player_string_view value;
    bool is_null;
    const char *source_class;
    int32_t literal_pc;
} mc_client_glu_error_text;

mc_client_glu_error_text mc_client_application_glu_error_text(int32_t error);

typedef enum mc_client_application_gl_diagnostic_status {
    MC_CLIENT_GL_DIAGNOSTIC_OK,
    MC_CLIENT_GL_DIAGNOSTIC_INVALID_ARGUMENT,
    MC_CLIENT_GL_DIAGNOSTIC_DISPLAY_FAILED,
    MC_CLIENT_GL_DIAGNOSTIC_SYSTEM_OUT_UNIMPLEMENTED
} mc_client_application_gl_diagnostic_status;

typedef struct mc_client_application_gl_diagnostic_error {
    mc_client_application_gl_diagnostic_status status;
    const char *operation;
    mc_client_application_owner_site site;
    bool native_policy, query_completed, glu_returned, source_returned;
    int32_t gl_error;
    mc_client_glu_error_text glu_text;
    /* A field dependency, not a fabricated entered method/exception frame. */
    const char *dependency_class, *dependency_name, *dependency_descriptor;
    mc_client_display_error display;
} mc_client_application_gl_diagnostic_error;

/* Actual Minecraft.c(String): glGetError0 exactly once, zero -> return75.
 * Nonzero completes the finite GLU9 mapping, retains its value/null and stops
 * before the missing canonical System.out field13. No output, argument
 * String read, builder, caught Java exception or method completion is invented.
 * label may be NULL and is not read by this implemented prefix (first use37).
 * This is a one-shot source invocation, not a retry/resume operation. Calling
 * again performs another source glGetError; keep the returned prefix instead.
 * Caller keeps the genuine current Display live/exclusive. Error storage is
 * mandatory, disjoint and live through callbacks; its text views are static.
 * No App receiver/layout, coordinator, swap or event pump is supplied. */
mc_client_application_gl_diagnostic_status
mc_client_application_gl_diagnostic(mc_client_display *, const mc_player_string *label,
                                    mc_client_application_gl_diagnostic_error *);

#endif
