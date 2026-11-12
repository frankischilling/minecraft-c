/* Exact canonical Display TU, real GetError, real stdout and base streams.
 * The separate observer includes the exact PrintStream TU once. */
#include "../../../src/client/display/display.c"
#include "../../core/java/print_stream_test_io.h"
#include "client/startup/application_gl_output.h"
#include "print_stream_rebind_observer.h"

#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks, queries;
static mc_client_gl_get_error_fn real_error;
static bool nested_busy;
static mc_client_gl_output_call *active_call;

static GLenum GLAPIENTRY counted_error(void) {
    ++queries;
    return real_error();
}

static void reenter_call(void *context) {
    mc_client_display *display = context;
    mc_client_gl_output_call *alias = active_call;
    mc_client_gl_output_error error;
    nested_busy =
        mc_client_gl_output_run(active_call, display, NULL, NULL) == MC_CLIENT_GL_OUTPUT_BUSY &&
        mc_client_gl_output_retire(&alias, &error) == MC_CLIENT_GL_OUTPUT_BUSY &&
        alias == active_call;
}

#define NEED(x)                                                                                    \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, #x);                                 \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

/* mode0: first->second during first real write; mode1: first->NULL after
 * header; mode2: first->NULL after second line. The captured outer receivers
 * remain live while actual argument builders execute before invoke43/72. */
static bool output_case(mc_client_display *display, unsigned int mode) {
    bool ok = false;
    print_test_io io = {0};
    mc_java_system_output *system = NULL;
    mc_java_print_stream *first = NULL, *second = NULL;
    mc_client_gl_output_call *call = NULL;
    mc_java_print_error pe;
    mc_client_gl_output_error e;
    mc_client_gl_output_state state;
    mc_java_print_state a, b;
    print_observer_monitor_hold *held = NULL;
    uint8_t bytes[256];
    NEED(print_test_redirect(&io, false));
    NEED(mc_java_system_output_allocate(&system, &pe) == MC_JAVA_PRINT_OK);
    NEED(mc_java_system_output_construct(system, &pe) == MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_allocate(&first, &pe) == MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_construct_encoded_stdout(first, MC_JAVA_READER_UTF8, true, &pe) ==
         MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_allocate(&second, &pe) == MC_JAVA_PRINT_OK);
    NEED(mc_java_print_stream_construct_encoded_stdout(second, MC_JAVA_READER_UTF8, true, &pe) ==
         MC_JAVA_PRINT_OK);
    NEED(mc_java_system_output_bind(system, mode == 3 ? NULL : first, &pe) == MC_JAVA_PRINT_OK);
    NEED(mc_client_gl_output_allocate(&call, &e) == MC_CLIENT_GL_OUTPUT_OK);
    active_call = call;
    nested_busy = false;
    print_observer_install(first, system, mode == 0 ? second : NULL, mode == 2 ? 2u : 1u,
                           reenter_call, display);
    display->gl.Enable(UINT32_MAX); /* Genuine error; no synthetic GetError value. */
    queries = 0;
    const mc_client_gl_output_status status = mc_client_gl_output_run(call, display, system, NULL);
    NEED(queries == 1);
    if (mode != 3)
        NEED(print_observer_rebound() && nested_busy);
    NEED(mc_client_gl_output_inspect(call, &state));
    NEED(state.error->prefix.query_completed && state.error->prefix.gl_error == GL_INVALID_ENUM);
    NEED(mc_java_print_stream_inspect(first, &a) && mc_java_print_stream_inspect(second, &b));
    const size_t count = print_test_read(&io, bytes, sizeof(bytes));
#ifdef _WIN32
    static const char complete[] = "########## GL ERROR ##########\r\n"
                                   "@ null\r\n"
                                   "1280: Invalid enum\r\n";
#else
    static const char complete[] = "########## GL ERROR ##########\n"
                                   "@ null\n"
                                   "1280: Invalid enum\n";
#endif
    if (mode == 0) {
        NEED(status == MC_CLIENT_GL_OUTPUT_OK && state.error->source_returned &&
             state.error->site.pc == 75);
        NEED(state.captured_receivers == 3 && state.normal_println_returns == 3);
        NEED(state.receivers[0] == first && state.receivers[1] == second &&
             state.receivers[2] == second);
        NEED(a.println_returns == 1 && b.println_returns == 2);
        NEED(count == sizeof(complete) - 1u && memcmp(bytes, complete, count) == 0);
    } else if (mode == 3) {
        NEED(status == MC_CLIENT_GL_OUTPUT_NULL_PRINT_STREAM && state.error->site.pc == 18);
        NEED(state.captured_receivers == 1 && state.normal_println_returns == 0 && count == 0);
        NEED(state.receivers[0] == NULL && state.results[0] == NULL && state.builders[0] == NULL);
    } else {
        NEED(status == MC_CLIENT_GL_OUTPUT_NULL_PRINT_STREAM && !state.error->source_returned);
        const size_t index = mode == 1 ? 0u : 1u;
        NEED(state.error->site.pc == (mode == 1 ? 43u : 72u));
        NEED(state.results[index] != NULL && state.builders[index] != NULL);
        NEED(state.captured_receivers == (mode == 1 ? 2u : 3u));
        NEED(state.normal_println_returns == (mode == 1 ? 1u : 2u));
        NEED(state.receivers[mode == 1 ? 1 : 2] == NULL && b.println_returns == 0);
        NEED(count > 0 && count < sizeof(complete) - 1u && memcmp(bytes, complete, count) == 0);
    }
    const unsigned int before = queries;
    NEED(mc_client_gl_output_run(call, display, system, NULL) ==
             MC_CLIENT_GL_OUTPUT_INVALID_PHASE &&
         queries == before);
    if (mode == 0) {
        /* Reuse THIS completed source call; no synthetic child result or
         * replay. A different real thread owns the actual stream monitor. */
        const mc_client_gl_output_call *same = call;
        const size_t first_holds = a.holds, second_holds = b.holds;
        NEED(print_observer_hold_stream(first, &held));
        NEED(mc_client_gl_output_retire(&call, &e) == MC_CLIENT_GL_OUTPUT_BUSY);
        NEED(call == same && e.system.status == MC_JAVA_PRINT_BUSY && queries == before);
        NEED(mc_client_gl_output_inspect(call, &state) && state.retiring && !state.active);
        NEED(state.receivers[0] == first && state.receivers[1] == second &&
             state.receivers[2] == second && state.first_cleanup_error->status == MC_JAVA_PRINT_OK);
        NEED(state.error->source_returned && state.error->site.pc == 75 &&
             state.normal_println_returns == 3);
        NEED(mc_client_gl_output_run(call, display, system, NULL) ==
                 MC_CLIENT_GL_OUTPUT_INVALID_PHASE &&
             queries == before);
        NEED(print_observer_release_monitor(&held) && held == NULL);
        NEED(mc_java_print_stream_inspect(first, &a) && a.holds == first_holds);
        NEED(mc_java_print_stream_inspect(second, &b) && b.holds == second_holds);

        /* Receiver drops can now complete, but the real namespace occurrence
         * is still live while its monitor is held by another real thread. */
        NEED(print_observer_hold_system(system, &held));
        NEED(mc_client_gl_output_retire(&call, &e) == MC_CLIENT_GL_OUTPUT_BUSY);
        NEED(call == same && e.system.status == MC_JAVA_PRINT_BUSY && queries == before);
        NEED(mc_client_gl_output_inspect(call, &state) && state.retiring);
        NEED(state.receivers[0] == NULL && state.receivers[1] == NULL &&
             state.receivers[2] == NULL && state.first_cleanup_error->status == MC_JAVA_PRINT_OK);
        NEED(state.error->source_returned && state.error->site.pc == 75 &&
             state.normal_println_returns == 3);
        NEED(print_observer_release_monitor(&held) && held == NULL);
        mc_java_system_output_state ns;
        NEED(mc_java_system_output_inspect(system, &ns) && ns.holds == 2 && ns.out == second);
        NEED(mc_java_print_stream_inspect(first, &a) && a.holds + 1u == first_holds);
        NEED(mc_java_print_stream_inspect(second, &b) && b.holds + 2u == second_holds);

        NEED(mc_client_gl_output_retire(&call, &e) == MC_CLIENT_GL_OUTPUT_OK && call == NULL);
        NEED(e.status == MC_CLIENT_GL_OUTPUT_OK && e.system.status == MC_JAVA_PRINT_OK &&
             queries == before);
        NEED(mc_java_system_output_inspect(system, &ns) && ns.holds == 1 && ns.out == second);
        NEED(mc_java_print_stream_inspect(first, &a) && a.holds == 1 && a.println_returns == 1);
        NEED(mc_java_print_stream_inspect(second, &b) && b.holds == 2 && b.println_returns == 2);
        NEED(print_test_read(&io, bytes, sizeof(bytes)) == count);
    }
    ok = true;
done:
    if (held != NULL && !print_observer_release_monitor(&held))
        return false; /* Preserve borrowed live owners if a worker cannot join. */
    active_call = NULL;
    if (first != NULL)
        print_observer_clear(first);
    if (call != NULL && mc_client_gl_output_retire(&call, &e) != MC_CLIENT_GL_OUTPUT_OK)
        ok = false;
    if (system != NULL && mc_java_system_output_release(&system, &pe) != MC_JAVA_PRINT_OK)
        ok = false;
    if (first != NULL && mc_java_print_stream_release(&first, &pe) != MC_JAVA_PRINT_OK)
        ok = false;
    if (second != NULL && mc_java_print_stream_release(&second, &pe) != MC_JAVA_PRINT_OK)
        ok = false;
    if (!print_test_restore(&io))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int result = 1;
    mc_client_display *display = NULL;
    mc_client_display_error de;
    mc_client_gl_output_call *call = NULL;
    mc_client_gl_output_error e;
    mc_client_gl_output_state state;
    NEED(mc_client_display_create("real System.out GL diagnostics", 160, 120, true, &display, &de));
    real_error = display->gl.GetError;
    NEED(real_error != NULL && real_error() == GL_NO_ERROR);
    display->gl.GetError = counted_error;
    NEED(mc_client_gl_output_allocate(&call, &e) == MC_CLIENT_GL_OUTPUT_OK);
    queries = 0;
    NEED(mc_client_gl_output_run(call, display, NULL, NULL) == MC_CLIENT_GL_OUTPUT_OK &&
         queries == 1);
    NEED(mc_client_gl_output_inspect(call, &state) && state.error->site.pc == 75 &&
         state.captured_receivers == 0);
    NEED(mc_client_gl_output_retire(&call, &e) == MC_CLIENT_GL_OUTPUT_OK);
    NEED(output_case(display, 0) && output_case(display, 1) && output_case(display, 2) &&
         output_case(display, 3));
    NEED(display->source.context_holds == 0 && real_error() == GL_NO_ERROR);
    result = 0;
done:
    if (call != NULL)
        mc_client_gl_output_retire(&call, &e);
    if (display != NULL)
        display->gl.GetError = real_error;
    if (!mc_client_display_destroy(display, &de))
        result = 1;
    SDL_Quit();
    if (result == 0 && printf("Application GL output device: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
