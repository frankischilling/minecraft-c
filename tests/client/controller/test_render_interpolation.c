/* Finite os.a(F) body checks with genuine owners and prepared source fields. */
#include "client/application_state.h"
#include "client/controller/singleplayer_internal.h"
#include "client/gui/gui_ingame_internal.h"
#include "client/world/render_global_internal.h"
#include "mc/client/controller/render_interpolation.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Controller interpolation check%u line%d: %s\n", checks, __LINE__,     \
                    #value);                                                                       \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_client_render_classes *models;
    mc_client_gui_ingame_classes *gui_classes;
    mc_client_render_engine_classes *graphics;
    mc_client_source_uq *gui;
    mc_client_render_global *global;
    mc_client_singleplayer_controller *controller;
    client_application *app;
} fixture;

static uint32_t bits(const float *value) {
    uint32_t raw;
    memcpy(&raw, value, sizeof(raw));
    return raw;
}

static void install(float *value, uint32_t raw) {
    memcpy(value, &raw, sizeof(raw));
}

static bool open_fixture(fixture *f) {
    memset(f, 0, sizeof(*f));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &f->models) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_client_gui_ingame_classes_create(f->models, f->profile, 0, &f->gui_classes, NULL) ==
          MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_allocate(f->gui_classes, NULL, &f->gui, NULL) ==
          MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_global_allocate(f->graphics, NULL, &f->global, NULL) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    f->app = calloc(1, sizeof(*f->app));
    CHECK(f->app != NULL);
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &f->controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_construct(f->controller, f->app, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    return true;
}

static bool close_fixture(fixture *f) {
    CHECK(mc_client_singleplayer_controller_free(&f->controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_render_global_retire(&f->global, NULL) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_gui_ingame_retire(&f->gui, NULL) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_classes_release(&f->gui_classes, NULL) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_render_engine_classes_release(f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(f->models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    free(f->app);
    return true;
}

static bool original_rows(fixture *f) {
    static const uint32_t cases[][3] = {
        {0x00000000, 0x3f800000, 0x7fc54321}, {0x80000000, 0x40000000, 0xff800000},
        {0xbf800000, 0x7fc12345, 0x7fc54321}, {0xff800000, 0x7fc12345, 0x7fc54321},
        {0x00000001, 0x00000000, 0x3f800000}, {0x3f800000, 0x00000000, 0x00000000},
        {0x3f800000, 0x00000000, 0x3f000000}, {0x3f800000, 0x00000000, 0x3f800000},
        {0x3f800000, 0x40000000, 0xbf800000}, {0x3f800000, 0x40000000, 0x40000000},
        {0x7f800000, 0x3f800000, 0x3f000000}, {0x7fc12345, 0x40000000, 0x3f000000},
        {0xffc12345, 0x40000000, 0x3f000000}, {0x7fa12345, 0x40000000, 0x3f000000},
        {0x3f800000, 0x7fc12345, 0x3f000000}, {0x3f800000, 0x40000000, 0x7fc12345},
        {0x7f800000, 0x7f800000, 0x00000000}, {0x00000001, 0x80000001, 0x3f800000}};
    for (unsigned int mode = 0; mode < 4; ++mode) {
        for (size_t n = 0; n < sizeof(cases) / sizeof(cases[0]); ++n) {
            f->controller->fields.a = mode == 0 ? NULL : f->app;
            f->app->v = mode == 1 ? NULL : f->gui;
            f->app->g = mode == 2 ? NULL : f->global;
            install(&f->controller->fields.f, cases[n][0]);
            install(&f->controller->fields.g, cases[n][1]);
            float partial;
            install(&partial, cases[n][2]);
            install(&f->gui->fields.b, UINT32_C(0x3f2468ac));
            install(&f->global->fields.i, UINT32_C(0xbf2468ac));
            mc_singleplayer_render_interpolation_report report;
            mc_singleplayer_render_interpolation_error error;
            const mc_singleplayer_render_interpolation_status status =
                mc_client_singleplayer_controller_render_interpolation(f->controller, partial,
                                                                       &report, &error);
            CHECK(status == (mode == 3 ? MC_SINGLEPLAYER_RENDER_INTERPOLATION_OK
                                       : MC_SINGLEPLAYER_RENDER_INTERPOLATION_SOURCE_NULL));
            CHECK(report.source_entered && report.gui_stored == (mode >= 2) &&
                  report.global_stored == (mode == 3) && report.source_returned == (mode == 3) &&
                  !f->controller->fields.entered);
            CHECK(bits(&f->controller->fields.f) == cases[n][0] &&
                  bits(&f->controller->fields.g) == cases[n][1]);
            CHECK(error.site.pc == (mode == 0   ? (report.arithmetic_branch ? 55u : 13u)
                                    : mode == 1 ? (report.arithmetic_branch ? 59u : 17u)
                                    : mode == 2 ? (report.arithmetic_branch ? 70u : 28u)
                                                : 73u));
            printf("INTERPOLATE %u %zu %d %08" PRIx32 " %08" PRIx32 "\n", mode, n, mode == 3,
                   bits(&f->gui->fields.b), bits(&f->global->fields.i));
        }
    }
    f->controller->fields.a = f->app;
    f->app->v = f->gui;
    f->app->g = f->global;
    return true;
}

static bool controls(fixture *f) {
    mc_singleplayer_render_interpolation_report report, before;
    mc_singleplayer_render_interpolation_error error;
    memset(&report, 0x5a, sizeof(report));
    before = report;
    f->controller->fields.entered = true;
    CHECK(mc_client_singleplayer_controller_render_interpolation(
              f->controller, 0.5f, &report, &error) == MC_SINGLEPLAYER_RENDER_INTERPOLATION_BUSY &&
          memcmp(&report, &before, sizeof(report)) == 0);
    f->controller->fields.entered = false;
    install(&f->gui->fields.b, UINT32_C(0x3f2468ac));
    install(&f->global->fields.i, UINT32_C(0xbf2468ac));
    f->controller->fields.f = -1.0f;
    const unsigned int floating = _mm_getcsr();
    _mm_setcsr(floating | 0x8000u);
    const mc_singleplayer_render_interpolation_status literal =
        mc_client_singleplayer_controller_render_interpolation(f->controller, 0.5f, &report,
                                                               &error);
    _mm_setcsr(floating);
    CHECK(literal == MC_SINGLEPLAYER_RENDER_INTERPOLATION_OK && !report.arithmetic_branch &&
          bits(&f->gui->fields.b) == 0 && bits(&f->global->fields.i) == 0);
    f->controller->fields.f = 1.0f;
    f->controller->fields.g = 0.0f;
    _mm_setcsr(floating | 0x8000u);
    const mc_singleplayer_render_interpolation_status arithmetic =
        mc_client_singleplayer_controller_render_interpolation(f->controller, 0.5f, &report,
                                                               &error);
    _mm_setcsr(floating);
    CHECK(arithmetic == MC_SINGLEPLAYER_RENDER_INTERPOLATION_FLOATING && error.native_policy &&
          error.site.pc == 46 && !report.gui_stored && !report.global_stored);
    f->global->active = true;
    CHECK(mc_client_singleplayer_controller_render_interpolation(f->controller, 0.5f, &report,
                                                                 &error) ==
              MC_SINGLEPLAYER_RENDER_INTERPOLATION_GLOBAL_OWNER &&
          report.gui_stored && !report.global_stored && f->gui->fields.b == 0.5f);
    f->global->active = false;
    return true;
}

int main(void) {
    fixture f;
    if (!open_fixture(&f) || !original_rows(&f) || !controls(&f) || !close_fixture(&f))
        return 1;
    printf("%u controller interpolation checks passed\n", checks);
    return 0;
}
