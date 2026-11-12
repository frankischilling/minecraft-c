#include "client/startup/application_gl_diagnostics.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct mapping_case {
    int32_t code;
    const char *text, *owner;
    int32_t pc;
} mapping_case;

static const mapping_case mappings[] = {
    {100900, "Invalid enum (glu)", "org/lwjgl/util/glu/GLU", 28},
    {100901, "Invalid value (glu)", "org/lwjgl/util/glu/GLU", 31},
    {100902, "Out of memory (glu)", "org/lwjgl/util/glu/GLU", 34},
    {0, "No error", "org/lwjgl/opengl/Util", 84},
    {1280, "Invalid enum", "org/lwjgl/opengl/Util", 87},
    {1281, "Invalid value", "org/lwjgl/opengl/Util", 90},
    {1282, "Invalid operation", "org/lwjgl/opengl/Util", 93},
    {1283, "Stack overflow", "org/lwjgl/opengl/Util", 96},
    {1284, "Stack underflow", "org/lwjgl/opengl/Util", 99},
    {1285, "Out of memory", "org/lwjgl/opengl/Util", 102},
    {1286, "Invalid framebuffer operation", "org/lwjgl/opengl/Util", 108},
    {32817, "Table too large", "org/lwjgl/opengl/Util", 105}};

int main(void) {
    for (size_t i = 0; i < sizeof(mappings) / sizeof(mappings[0]); ++i) {
        const mapping_case *c = &mappings[i];
        const mc_client_glu_error_text a = mc_client_application_glu_error_text(c->code);
        CHECK(!a.is_null && a.value.units != NULL && a.value.length == strlen(c->text));
        CHECK(a.literal_pc == c->pc && strcmp(a.source_class, c->owner) == 0);
        for (size_t j = 0; j < a.value.length; ++j)
            CHECK(a.value.units[j] == (unsigned char)c->text[j]);
        const mc_client_glu_error_text b = mc_client_application_glu_error_text(c->code);
        CHECK(a.value.units == b.value.units && a.value.length == b.value.length);
    }
    static const int32_t unknown[] = {INT32_MIN, -1,    1,      1279,   1287,
                                      32816,     32818, 100899, 100903, INT32_MAX};
    for (size_t i = 0; i < sizeof(unknown) / sizeof(unknown[0]); ++i) {
        const mc_client_glu_error_text a = mc_client_application_glu_error_text(unknown[i]);
        CHECK(a.is_null && a.value.units == NULL && a.value.length == 0 && a.literal_pc == 111);
        CHECK(strcmp(a.source_class, "org/lwjgl/opengl/Util") == 0);
    }
    uint32_t sentinel = UINT32_C(0x12345678);
    mc_client_display_error display_error = {0};
    CHECK(!mc_client_display_source_get_error(NULL, &sentinel, &display_error));
    CHECK(sentinel == UINT32_C(0x12345678) &&
          display_error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    mc_client_application_gl_diagnostic_error error;
    CHECK(mc_client_application_gl_diagnostic(NULL, NULL, &error) ==
          MC_CLIENT_GL_DIAGNOSTIC_DISPLAY_FAILED);
    CHECK(error.site.pc == 0 && !error.query_completed && !error.glu_returned &&
          !error.source_returned && error.native_policy &&
          error.display.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(mc_client_application_gl_diagnostic(NULL, NULL, NULL) ==
          MC_CLIENT_GL_DIAGNOSTIC_INVALID_ARGUMENT);
    return printf("Application GL diagnostic CPU: %u checks\n", checks) < 0;
}
