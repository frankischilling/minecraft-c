/* Actual Display TU exactly once; actual cx methods come from their canonical
 * CPU/device archives. The sole GL interception forwards the real function
 * before attempting native reentry. No capability or source result is faked. */
#include "../../../src/client/display/display.c"
#include "../../../src/client/render/cx.h"

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

static mc_client_gl native_gl;
static mc_client_display *observed_display;
static mc_client_source_cx *observed_cx;
static mc_client_cx_classes *observed_classes;
static unsigned int checks, extension_queries;
static bool nested_cx_busy, nested_display_busy, nested_release_busy, nested_namespace_busy;

static void GLAPIENTRY observe_integer(GLenum name, GLint *value) {
    native_gl.GetIntegerv(name, value);
    if (name == GL_NUM_EXTENSIONS) {
        ++extension_queries;
        mc_client_cx_error cx_error = {0};
        mc_client_display_error display_error = {0};
        bool sentinel = true;
        nested_cx_busy = mc_client_cx_supported(observed_cx, observed_display, &sentinel,
                                                &cx_error) == MC_CLIENT_CX_BUSY &&
                         sentinel;
        nested_display_busy = !mc_client_display_source_arb_occlusion_query(
                                  observed_display, &sentinel, &display_error) &&
                              display_error.status == MC_CLIENT_DISPLAY_BUSY && sentinel;
        mc_client_source_cx *slot = observed_cx;
        nested_release_busy =
            mc_client_cx_release(&slot, &cx_error) == MC_CLIENT_CX_BUSY && slot == observed_cx;
        mc_client_cx_classes *namespace_slot = observed_classes;
        nested_namespace_busy =
            mc_client_cx_classes_release(&namespace_slot, &cx_error) == MC_CLIENT_CX_BUSY &&
            namespace_slot == observed_classes;
    }
}

static bool actual_driver_truth(bool *value) {
    const PFNGLGETSTRINGIPROC get_string_i =
        (PFNGLGETSTRINGIPROC)SDL_GL_GetProcAddress("glGetStringi");
    if (get_string_i == NULL)
        return false;
    GLint count = 0;
    native_gl.GetIntegerv(GL_NUM_EXTENSIONS, &count);
    if (count < 0)
        return false;
    bool token_present = false;
    for (GLint index = 0; index < count; ++index) {
        const GLubyte *token = get_string_i(GL_EXTENSIONS, (GLuint)index);
        if (token == NULL)
            return false;
        if (strcmp((const char *)token, "GL_ARB_occlusion_query") == 0)
            token_present = true;
    }
    bool functions = true;
    if (token_present) {
        const char *const names[] = {"glGenQueriesARB",       "glDeleteQueriesARB",
                                     "glIsQueryARB",          "glBeginQueryARB",
                                     "glEndQueryARB",         "glGetQueryivARB",
                                     "glGetQueryObjectivARB", "glGetQueryObjectuivARB"};
        for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
            const SDL_FunctionPointer address = SDL_GL_GetProcAddress(names[index]);
            functions = address != NULL && functions;
        }
    }
    *value = token_present && functions;
    return true;
}

static bool retire_owned_display(mc_client_display **slot, mc_client_display_error *error) {
    bool consumed = false;
    const bool ok = destroy_native(*slot, false, &consumed, NULL, error);
    if (consumed)
        *slot = NULL;
    return ok;
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
    mc_client_display *display = NULL, *second = NULL;
    mc_client_cx_classes *classes = NULL;
    mc_client_source_cx *cx = NULL;
    mc_client_display_context_lease *lease = NULL;
    mc_client_cx_error cx_error = {0};
    mc_client_display_error error = {0};
    bool expected = false, value = false, instrumented = false, hint_changed = false;
    char *saved_hint = NULL;
    CHECK(mc_client_cx_classes_create(&classes, &cx_error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_cx_allocate(classes, &cx, &cx_error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_cx_construct(cx, &cx_error) == MC_CLIENT_CX_OK);
    CHECK(mc_client_display_create("actual cx capability", 64, 64, true, &display, &error));
    native_gl = display->gl;
    CHECK(!display->occlusion.initialized && display->native_context_generation == 1);
    CHECK(actual_driver_truth(&expected));
    CHECK(native_gl.GetError() == GL_NO_ERROR);
    const char *hint = SDL_GetHint("GL_ARB_occlusion_query");
    if (hint != NULL) {
        saved_hint = malloc(strlen(hint) + 1);
        CHECK(saved_hint != NULL);
        memcpy(saved_hint, hint, strlen(hint) + 1);
    }
    CHECK(SDL_SetHintWithPriority("GL_ARB_occlusion_query", "0", SDL_HINT_OVERRIDE));
    hint_changed = true;
    observed_display = display;
    observed_cx = cx;
    observed_classes = classes;
    display->gl.GetIntegerv = observe_integer;
    instrumented = true;
    /* A real invalid GL enum leaves a pending flag. Capability lookup must not
     * drain it or translate it into false/failure, including the first fill. */
    native_gl.Enable(UINT32_MAX);
    value = !expected;
    CHECK(mc_client_cx_supported(cx, display, &value, &cx_error) == MC_CLIENT_CX_OK);
    CHECK(value == expected && cx_error.source_returned && cx_error.capability_read &&
          cx_error.source_pc == 20);
    CHECK(native_gl.GetError() == GL_INVALID_ENUM);
    CHECK(native_gl.GetError() == GL_NO_ERROR);
    CHECK(extension_queries == 1 && nested_cx_busy && nested_display_busy && nested_release_busy &&
          nested_namespace_busy);
    CHECK(display->occlusion.initialized && display->occlusion.supported == expected &&
          display->occlusion.generation == display->native_context_generation);
    CHECK(display->source.active_calls == 0 && display->source.context_holds == 0);
    CHECK(mc_client_cx_supported(cx, display, &value, &cx_error) == MC_CLIENT_CX_OK);
    CHECK(value == expected && extension_queries == 1); /* Same generation cache hit. */
    display->gl = native_gl;
    instrumented = false;

    CHECK(mc_client_display_create("distinct actual context", 32, 32, true, &second, &error));
    CHECK(!second->occlusion.initialized);
    value = true;
    CHECK(mc_client_cx_supported(cx, display, &value, &cx_error) == MC_CLIENT_CX_DISPLAY_FAILED);
    CHECK(value && !cx_error.source_returned && cx_error.source_pc == 6 &&
          cx_error.display.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(!second->occlusion.initialized && display->occlusion.initialized);
    CHECK(mc_client_display_make_current(display, &error));
    CHECK(mc_client_cx_supported(cx, display, &value, &cx_error) == MC_CLIENT_CX_OK &&
          value == expected);
    CHECK(mc_client_display_context_acquire(display, &lease, &error));
    CHECK(!mc_client_display_destroy(display, &error) && error.status == MC_CLIENT_DISPLAY_BUSY);
    CHECK(mc_client_display_context_release(&lease, &error) && lease == NULL);
    CHECK(mc_client_cx_classes_release(&classes, &cx_error) == MC_CLIENT_CX_OK && classes == NULL);
    CHECK(mc_client_cx_supported(cx, display, &value, &cx_error) == MC_CLIENT_CX_OK &&
          value == expected);
    CHECK(mc_client_cx_release(&cx, &cx_error) == MC_CLIENT_CX_OK && cx == NULL);
    CHECK(retire_owned_display(&second, &error) && second == NULL);
    CHECK(retire_owned_display(&display, &error) && display == NULL);
    printf("cx device: %u checks PASS capability=%d\n", checks, expected ? 1 : 0);
    result = 0;
cleanup:
    if (instrumented)
        display->gl = native_gl;
    observed_display = NULL;
    observed_cx = NULL;
    observed_classes = NULL;
    if (hint_changed) {
        if (saved_hint != NULL)
            (void)SDL_SetHintWithPriority("GL_ARB_occlusion_query", saved_hint, SDL_HINT_OVERRIDE);
        else
            (void)SDL_ResetHint("GL_ARB_occlusion_query");
    }
    free(saved_hint);
    (void)mc_client_cx_release(&cx, NULL);
    (void)mc_client_cx_classes_release(&classes, NULL);
    (void)mc_client_display_context_release(&lease, NULL);
    (void)retire_owned_display(&second, NULL);
    (void)retire_owned_display(&display, NULL);
    return result;
}
