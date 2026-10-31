/* Qualification-only exact Display TU, compiled once. Every intercepted
 * GetError forwards to the real native GL function. Counters/reentry observe
 * call ownership; no fabricated error value or successful provider is supplied. */
#include "../../../src/client/display/display.c"
#include "client/startup/application_gl_diagnostics.h"

#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <string.h>

static mc_client_gl_get_error_fn real_error;
static mc_client_display *observed_display;
static unsigned checks, queries;
static bool probe_reentry, nested_ok;

static GLenum GLAPIENTRY observed_error(void) {
    const GLenum value = real_error();
    ++queries;
    if (probe_reentry) {
        probe_reentry = false;
        mc_client_application_gl_diagnostic_error nested;
        nested_ok = mc_client_application_gl_diagnostic(observed_display, NULL, &nested) ==
                        MC_CLIENT_GL_DIAGNOSTIC_DISPLAY_FAILED &&
                    nested.display.status == MC_CLIENT_DISPLAY_BUSY && !nested.query_completed;
    }
    return value;
}

#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int result = 1;
    mc_client_display *display = NULL;
    mc_client_display_error cause = {0};
    mc_client_application_gl_diagnostic_error error;
    CHECK(mc_client_display_create("real source single glGetError", 160, 120, true, &display,
                                   &cause));
    real_error = display->gl.GetError;
    observed_display = display;
    CHECK(real_error != NULL && real_error() == GL_NO_ERROR);
    display->gl.GetError = observed_error;
    queries = 0;
    CHECK(mc_client_application_gl_diagnostic(display, NULL, &error) == MC_CLIENT_GL_DIAGNOSTIC_OK);
    CHECK(queries == 1 && error.query_completed && error.gl_error == 0 && !error.glu_returned &&
          error.source_returned && error.site.pc == 75);
    CHECK(display->source.active_calls == 0);

    queries = 0;
    CHECK(mc_client_application_gl_diagnostic(display, NULL, NULL) ==
          MC_CLIENT_GL_DIAGNOSTIC_INVALID_ARGUMENT);
    CHECK(queries == 0);
    CHECK(!mc_client_display_source_get_error(display, NULL, &cause));
    CHECK(queries == 0 && cause.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);

    /* Actual invalid-enum GL operation, without any intervening error drain. */
    display->gl.Enable(UINT32_MAX);
    queries = 0;
    probe_reentry = true;
    CHECK(mc_client_application_gl_diagnostic(display, NULL, &error) ==
          MC_CLIENT_GL_DIAGNOSTIC_SYSTEM_OUT_UNIMPLEMENTED);
    CHECK(queries == 1 && nested_ok && error.query_completed && error.glu_returned &&
          error.gl_error == GL_INVALID_ENUM && error.site.pc == 13 && !error.source_returned);
    CHECK(error.display.status == MC_CLIENT_DISPLAY_OK && error.native_policy);
    CHECK(!error.glu_text.is_null && error.glu_text.value.length == 12);
    CHECK(strcmp(error.dependency_class, "java/lang/System") == 0 &&
          strcmp(error.dependency_name, "out") == 0 &&
          strcmp(error.dependency_descriptor, "Ljava/io/PrintStream;") == 0);
    CHECK(real_error() == GL_NO_ERROR && display->source.active_calls == 0);

    uint32_t sentinel = UINT32_C(0x98765432);
    display->gl.GetError = NULL; /* Real missing-function native admission. */
    queries = 0;
    CHECK(!mc_client_display_source_get_error(display, &sentinel, &cause));
    CHECK(sentinel == UINT32_C(0x98765432) && cause.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL &&
          queries == 0 && display->source.active_calls == 0);
    display->gl.GetError = observed_error;
    queries = 0;
    CHECK(mc_client_application_gl_diagnostic(display, NULL, &error) == MC_CLIENT_GL_DIAGNOSTIC_OK);
    CHECK(queries == 1 && error.source_returned && error.site.pc == 75);
    result = 0;
cleanup:
    if (display != NULL && real_error != NULL)
        display->gl.GetError = real_error;
    if (!mc_client_display_destroy(display, &cause))
        result = 1;
    SDL_Quit(); /* Native process cleanup; not a source Display/MC method. */
    if (result == 0 && printf("Application GL diagnostic device: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
