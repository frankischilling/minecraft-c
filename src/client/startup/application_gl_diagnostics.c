#include "application_gl_diagnostics.h"

#include "mc/core/java/java.h"

#define MC_CLASS "net/minecraft/client/Minecraft"
#define DESCRIPTOR "(Ljava/lang/String;)V"

mc_client_application_gl_diagnostic_status
mc_client_application_gl_diagnostic(mc_client_display *display, const mc_player_string *label,
                                    mc_client_application_gl_diagnostic_error *error) {
    (void)label; /* Actual first String argument use is append37, still later. */
    if (error == NULL)
        return MC_CLIENT_GL_DIAGNOSTIC_INVALID_ARGUMENT;
    *error = (mc_client_application_gl_diagnostic_error){.site = {MC_CLASS, "c", DESCRIPTOR, 0},
                                                         .glu_text = {.is_null = true}};
    uint32_t value = 0;
    if (!mc_client_display_source_get_error(display, &value, &error->display)) {
        error->status = MC_CLIENT_GL_DIAGNOSTIC_DISPLAY_FAILED;
        error->operation = "Minecraft source glGetError native admission";
        error->native_policy = true;
        return error->status;
    }
    error->query_completed = true;
    error->gl_error = mc_java_i32_narrow((int64_t)value);
    if (error->gl_error == 0) {
        error->site.pc = 75;
        error->source_returned = true;
        return MC_CLIENT_GL_DIAGNOSTIC_OK;
    }
    error->site.pc = 9;
    error->glu_text = mc_client_application_glu_error_text(error->gl_error);
    error->glu_returned = true;
    error->site.pc = 13;
    error->status = MC_CLIENT_GL_DIAGNOSTIC_SYSTEM_OUT_UNIMPLEMENTED;
    error->operation = "Minecraft reached canonical System.out PrintStream dependency";
    error->native_policy = true;
    error->dependency_class = "java/lang/System";
    error->dependency_name = "out";
    error->dependency_descriptor = "Ljava/io/PrintStream;";
    return error->status;
}
