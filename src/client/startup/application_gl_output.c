#include "application_gl_output.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"
#define MC_C "(Ljava/lang/String;)V"

struct mc_client_gl_output_call {
    bool started, active, retiring;
    unsigned int captured, returned;
    mc_client_display_context_lease *context;
    mc_java_system_output *system;
    mc_player_string *label;
    mc_java_print_stream *receivers[3];
    mc_client_diagnostic_builder *builders[2];
    mc_player_string *results[2];
    mc_client_gl_output_error error;
    mc_java_print_error first_cleanup;
};

static mc_client_gl_output_status output_fail(mc_client_gl_output_error *e,
                                              mc_client_gl_output_status status,
                                              const char *operation, bool native_policy) {
    e->status = status;
    e->operation = operation;
    e->native_policy = native_policy;
    return status;
}

static void site(mc_client_gl_output_call *c, unsigned int pc) {
    c->error.site = (mc_client_application_owner_site){MC_CLASS, "c", MC_C, pc};
}

mc_client_gl_output_status mc_client_gl_output_allocate(mc_client_gl_output_call **out,
                                                        mc_client_gl_output_error *e) {
    if (e == NULL)
        return MC_CLIENT_GL_OUTPUT_INVALID_ARGUMENT;
    *e = (mc_client_gl_output_error){0};
    if (out == NULL)
        return output_fail(e, MC_CLIENT_GL_OUTPUT_INVALID_ARGUMENT, "GL output call slot", true);
    mc_client_gl_output_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return output_fail(e, MC_CLIENT_GL_OUTPUT_ALLOCATION_FAILED, "GL output retained call",
                           true);
    *out = c;
    return MC_CLIENT_GL_OUTPUT_OK;
}

static bool capture(mc_client_gl_output_call *c, size_t index, unsigned int pc) {
    site(c, pc);
    if (mc_java_system_output_get(c->system, &c->receivers[index], &c->error.system) !=
        MC_JAVA_PRINT_OK) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_SYSTEM_FAILED, "actual System.out field read",
                    true);
        return false;
    }
    ++c->captured;
    return true;
}

static bool println(mc_client_gl_output_call *c, size_t index, const mc_player_string_view *text,
                    unsigned int pc) {
    site(c, pc);
    if (c->receivers[index] == NULL) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_NULL_PRINT_STREAM,
                    "null PrintStream receiver at actual invoke", false);
        return false;
    }
    const mc_java_properties_string_view view = {text->units, text->length};
    if (mc_java_print_stream_println(c->receivers[index], &view, &c->error.lines[index]) !=
        MC_JAVA_PRINT_OK) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_SYSTEM_FAILED,
                    "actual PrintStream.println child", true);
        return false;
    }
    ++c->returned;
    return true;
}

static bool builder_new(mc_client_gl_output_call *c, size_t index, unsigned int new_pc,
                        unsigned int ctor_pc) {
    site(c, new_pc);
    if (mc_client_diagnostic_builder_allocate(&c->builders[index], &c->error.builder) !=
        MC_PLAYER_OK) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_BUILDER_FAILED,
                    "actual new StringBuilder candidate", true);
        return false;
    }
    site(c, ctor_pc);
    if (mc_client_diagnostic_builder_construct(c->builders[index], &c->error.builder) !=
        MC_PLAYER_OK) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_BUILDER_FAILED,
                    "actual StringBuilder constructor", true);
        return false;
    }
    return true;
}

static bool append(mc_client_gl_output_call *c, size_t index, const mc_player_string_view *text,
                   unsigned int pc) {
    site(c, pc);
    if (mc_client_diagnostic_builder_append(c->builders[index], text, &c->error.builder) ==
        MC_PLAYER_OK)
        return true;
    output_fail(&c->error, MC_CLIENT_GL_OUTPUT_BUILDER_FAILED,
                "actual StringBuilder.append(String)", true);
    return false;
}

static bool built(mc_client_gl_output_call *c, size_t index, unsigned int pc,
                  mc_player_string_view *view) {
    site(c, pc);
    if (mc_client_diagnostic_builder_string(c->builders[index], &c->results[index],
                                            &c->error.builder) != MC_PLAYER_OK) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_BUILDER_FAILED, "actual StringBuilder.toString",
                    true);
        return false;
    }
    if (!mc_player_string_read(c->results[index], view)) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_STRING_FAILED, "immutable result native read",
                    true);
        return false;
    }
    return true;
}

mc_client_gl_output_status mc_client_gl_output_run(mc_client_gl_output_call *c,
                                                   mc_client_display *display,
                                                   mc_java_system_output *system,
                                                   const mc_player_string *label) {
    static const uint16_t header[] = {'#', '#', '#', '#', '#', '#', '#', '#', '#', '#',
                                      ' ', 'G', 'L', ' ', 'E', 'R', 'R', 'O', 'R', ' ',
                                      '#', '#', '#', '#', '#', '#', '#', '#', '#', '#'};
    static const uint16_t at[] = {'@', ' '}, separator[] = {':', ' '};
    const mc_player_string_view head = {header, sizeof(header) / sizeof(header[0])};
    const mc_player_string_view prefix = {at, sizeof(at) / sizeof(at[0])};
    const mc_player_string_view sep = {separator, sizeof(separator) / sizeof(separator[0])};
    if (c == NULL)
        return MC_CLIENT_GL_OUTPUT_INVALID_ARGUMENT;
    if (c->active)
        return MC_CLIENT_GL_OUTPUT_BUSY;
    if (c->started || c->retiring)
        return MC_CLIENT_GL_OUTPUT_INVALID_PHASE;
    c->started = true;
    c->active = true;
    site(c, 0);
    c->error.string = mc_player_string_retain((mc_player_string *)label);
    if (c->error.string != MC_PLAYER_OK) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_STRING_FAILED, "retain actual String argument",
                    true);
        goto done;
    }
    c->label = (mc_player_string *)label;
    if (!mc_client_display_context_acquire(display, &c->context, &c->error.display)) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_DISPLAY_FAILED, "hold actual current GL context",
                    true);
        goto done;
    }
    const mc_client_application_gl_diagnostic_status status =
        mc_client_application_gl_diagnostic(display, label, &c->error.prefix);
    if (status == MC_CLIENT_GL_DIAGNOSTIC_OK) {
        site(c, 75);
        c->error.source_returned = true;
        goto done;
    }
    if (status != MC_CLIENT_GL_DIAGNOSTIC_SYSTEM_OUT_UNIMPLEMENTED) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_DISPLAY_FAILED, "source GetError prefix child",
                    true);
        goto done;
    }
    site(c, 13);
    if (mc_java_system_output_retain(system, &c->error.system) != MC_JAVA_PRINT_OK) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_SYSTEM_FAILED,
                    "retain canonical System namespace", true);
        goto done;
    }
    c->system = system;
    if (!capture(c, 0, 13))
        goto done;
    site(c, 16); /* Actual literal argument evaluated before null receiver at18. */
    if (!println(c, 0, &head, 18) || !capture(c, 1, 21) || !builder_new(c, 0, 24, 28))
        goto done;
    site(c, 31);
    if (!append(c, 0, &prefix, 33))
        goto done;
    mc_player_string_view label_view, result;
    site(c, 37);
    if (label != NULL && !mc_player_string_read(label, &label_view)) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_STRING_FAILED, "actual label String argument",
                    true);
        goto done;
    }
    if (!append(c, 0, label == NULL ? NULL : &label_view, 37) || !built(c, 0, 40, &result) ||
        !println(c, 1, &result, 43) || !capture(c, 2, 46) || !builder_new(c, 1, 49, 53))
        goto done;
    site(c, 57);
    if (mc_client_diagnostic_builder_append_int(c->builders[1], c->error.prefix.gl_error,
                                                &c->error.builder) != MC_PLAYER_OK) {
        output_fail(&c->error, MC_CLIENT_GL_OUTPUT_BUILDER_FAILED,
                    "actual StringBuilder.append(int)", true);
        goto done;
    }
    site(c, 60);
    if (!append(c, 1, &sep, 62) ||
        !append(c, 1, c->error.prefix.glu_text.is_null ? NULL : &c->error.prefix.glu_text.value,
                66) ||
        !built(c, 1, 69, &result) || !println(c, 2, &result, 72))
        goto done;
    site(c, 75);
    c->error.source_returned = true;
done:
    c->active = false;
    return c->error.status;
}

bool mc_client_gl_output_inspect(const mc_client_gl_output_call *c,
                                 mc_client_gl_output_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = (mc_client_gl_output_state){.started = c->started,
                                       .active = c->active,
                                       .retiring = c->retiring,
                                       .captured_receivers = c->captured,
                                       .normal_println_returns = c->returned,
                                       .error = &c->error,
                                       .first_cleanup_error = &c->first_cleanup};
    for (size_t i = 0; i < 3; ++i)
        out->receivers[i] = c->receivers[i];
    for (size_t i = 0; i < 2; ++i) {
        out->results[i] = c->results[i];
        out->builders[i] = c->builders[i];
    }
    return true;
}

mc_client_gl_output_status mc_client_gl_output_retire(mc_client_gl_output_call **slot,
                                                      mc_client_gl_output_error *e) {
    if (e == NULL)
        return MC_CLIENT_GL_OUTPUT_INVALID_ARGUMENT;
    *e = (mc_client_gl_output_error){0};
    if (slot == NULL)
        return output_fail(e, MC_CLIENT_GL_OUTPUT_INVALID_ARGUMENT, "GL output retirement slot",
                           true);
    mc_client_gl_output_call *c = *slot;
    if (c == NULL)
        return MC_CLIENT_GL_OUTPUT_OK;
    if (c->active)
        return output_fail(e, MC_CLIENT_GL_OUTPUT_BUSY, "active GL output call", true);
    c->retiring = true;
    for (size_t i = 0; i < 3; ++i) {
        if (mc_java_print_stream_release(&c->receivers[i], &e->system) != MC_JAVA_PRINT_OK) {
            const bool held = c->receivers[i] != NULL;
            if (!held && c->first_cleanup.status == MC_JAVA_PRINT_OK)
                c->first_cleanup = e->system;
            output_fail(e, held ? MC_CLIENT_GL_OUTPUT_BUSY : MC_CLIENT_GL_OUTPUT_SYSTEM_FAILED,
                        "captured PrintStream occurrence retirement", true);
            if (held)
                return e->status;
        }
    }
    if (mc_java_system_output_release(&c->system, &e->system) != MC_JAVA_PRINT_OK) {
        if (c->system == NULL && c->first_cleanup.status == MC_JAVA_PRINT_OK)
            c->first_cleanup = e->system;
        output_fail(
            e, c->system != NULL ? MC_CLIENT_GL_OUTPUT_BUSY : MC_CLIENT_GL_OUTPUT_SYSTEM_FAILED,
            "System namespace occurrence retirement", true);
        if (c->system != NULL)
            return e->status;
    }
    for (size_t i = 0; i < 2; ++i) {
        if (mc_client_diagnostic_builder_retire(&c->builders[i], &e->builder) != MC_PLAYER_OK)
            return output_fail(e, MC_CLIENT_GL_OUTPUT_BUSY, "pending StringBuilder retirement",
                               true);
        mc_player_string_release(c->results[i]);
        c->results[i] = NULL;
    }
    mc_player_string_release(c->label);
    c->label = NULL;
    if (!mc_client_display_context_release(&c->context, &e->display))
        return output_fail(e, MC_CLIENT_GL_OUTPUT_DISPLAY_FAILED, "GL context lease retirement",
                           true);
    if (c->first_cleanup.status != MC_JAVA_PRINT_OK) {
        e->system = c->first_cleanup;
        output_fail(e, MC_CLIENT_GL_OUTPUT_SYSTEM_FAILED, "prior consumed native cleanup failure",
                    true);
    }
    free(c);
    *slot = NULL;
    return e->status;
}
