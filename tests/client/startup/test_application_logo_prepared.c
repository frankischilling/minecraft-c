/* QUALIFICATION ONLY: PREPARED-CALLEE MC.x()V. The real coordinator remains at
 * its actual MC324 frontier. No skipped324..354 call is credited. Compile the
 * exact production Display TU once, matching the existing loading/font
 * controls; every intercepted GL operation forwards to the actual function
 * first. */
#include "../../../src/client/display/display.c"
#define main qualified_existing_startup_main
#include "test_application_startup.c"
#undef main
#include "client/display/sdk_display_internal.h"
#include "client/display/sdk_update.h"
#include "client/startup/application_logo.h"

static mc_client_gl_draw_arrays_fn actual_logo_draw;
static mc_client_gl_clear_fn actual_logo_clear;

typedef struct logo_observation {
    mc_client_application_startup *startup;
    mc_client_startup_error *probe_error;
    mc_client_display *display;
    unsigned draws, clears;
    bool callback_ok, probe_reentry, late_fp;
    GLenum modes[2];
    GLsizei counts[2];
} logo_observation;

static logo_observation *logo_current;

static bool logo_probe(logo_observation *p) {
    mc_client_application_startup *slot = p->startup;
    return p->startup->active && p->startup->logo != NULL && p->startup->logo->active &&
           application_startup_logo(p->startup) == MC_CLIENT_STARTUP_BUSY &&
           mc_client_application_startup_retire(&slot, p->probe_error) == MC_CLIENT_STARTUP_BUSY &&
           slot == p->startup && p->startup->app->display == p->display &&
           p->startup->app->render_engine == p->startup->engine_candidate;
}

static void GLAPIENTRY logo_draw(GLenum mode, GLint first, GLsizei count) {
    actual_logo_draw(mode, first, count);
    logo_observation *p = logo_current;
    if (p == NULL)
        return;
    if (p->draws < 2) {
        p->modes[p->draws] = mode;
        p->counts[p->draws] = count;
    }
    ++p->draws;
    p->callback_ok = p->callback_ok && first == 0;
    if (p->probe_reentry && p->draws == 1)
        p->callback_ok = p->callback_ok && logo_probe(p);
}

static void GLAPIENTRY logo_clear(GLbitfield bits) {
    actual_logo_clear(bits);
    logo_observation *p = logo_current;
    if (p == NULL)
        return;
    ++p->clears;
    p->callback_ok = p->callback_ok && bits == (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (p->late_fp)
        _mm_setcsr(0x5f80u);
}

static int prepared_logo_control(const char *resources, const char *assets, const char *mode) {
    const unsigned saved_fp = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    CHECK(startup_services(&startup) == 0);
    CHECK(startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options options = mc_client_application_startup_default_options();
    options.hidden = true;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
          MC_CLIENT_STARTUP_UNIMPLEMENTED);
    CHECK(startup.error->caller.pc == 324 && attempt->last_startup_store == 307 &&
          attempt->last_manager_pc == 321 && attempt->manager_f_stored && app->t != NULL &&
          app->render_engine == attempt->engine_candidate &&
          app->settings == attempt->settings_candidate &&
          app->font_renderer == attempt->font_candidate);
    const mc_client_startup_phase prefix_phase = attempt->phase;
    const bool j_before = atomic_load_explicit(&app->J, memory_order_seq_cst);
    const bool o_before = atomic_load_explicit(&app->o, memory_order_seq_cst);
    CHECK(j_before && !o_before && attempt->logo == NULL &&
          app->source_namespace->startup_logo_resource == NULL);
    CHECK(application_startup_logo(NULL) == MC_CLIENT_STARTUP_INVALID_PHASE);
    mc_client_events_state events_before = {0}, events_after = {0};
    CHECK(mc_client_events_inspect(mc_client_display_event_port(attempt->display), &events_before));
    mc_client_startup_error *probe = calloc(1, sizeof(*probe));
    CHECK(probe != NULL);
    logo_observation observation = {.startup = attempt,
                                    .probe_error = probe,
                                    .display = attempt->display,
                                    .callback_ok = true,
                                    .probe_reentry = strcmp(mode, "success") == 0,
                                    .late_fp = strcmp(mode, "late-fp") == 0};
    actual_logo_draw = attempt->display->gl.DrawArrays;
    actual_logo_clear = attempt->display->gl.Clear;
    attempt->display->gl.DrawArrays = logo_draw;
    attempt->display->gl.Clear = logo_clear;
    logo_current = &observation;
    mc_game_settings *settings = app->settings;
    mc_client_render_engine *engine = app->render_engine;
    /* Explicit prepared late-null receiver profiles. The actual completed
     * owners remain retained by startup; restore the SAME identity for native
     * cleanup. These are not source stores or normal coordinator executions. */
    if (strcmp(mode, "null-settings") == 0)
        app->settings = NULL;
    if (strcmp(mode, "null-engine") == 0)
        app->render_engine = NULL;
    if (strcmp(mode, "fp") == 0)
        _mm_setcsr(0x5f80u);
    const mc_client_startup_status result = application_startup_logo(attempt);
    _mm_setcsr(0x1f80u);
    app->settings = settings;
    app->render_engine = engine;
    logo_current = NULL;
    attempt->display->gl.DrawArrays = actual_logo_draw;
    attempt->display->gl.Clear = actual_logo_clear;
    mc_client_startup_logo_call *call = attempt->logo;
    CHECK(call != NULL && !call->active && !attempt->active && observation.callback_ok);
    CHECK(attempt->last_startup_store == 307 && attempt->last_manager_pc == 321 &&
          attempt->phase == prefix_phase && app->constructor_phase == MC_CLIENT_APP_READY);
    CHECK(atomic_load_explicit(&app->J, memory_order_seq_cst) == j_before &&
          atomic_load_explicit(&app->o, memory_order_seq_cst) == o_before);
    CHECK(mc_client_events_inspect(mc_client_display_event_port(attempt->display), &events_after) &&
          events_after.pump_generation == events_before.pump_generation);
    if (strcmp(mode, "success") == 0) {
        CHECK(result == MC_CLIENT_STARTUP_OK && call->source_returned && call->background_drawn &&
              call->logo_drawn && call->swapped && call->pc == 270 && !call->native_policy);
        CHECK(observation.draws == 2 && observation.clears == 1 &&
              observation.modes[0] == GL_TRIANGLES && observation.modes[1] == GL_TRIANGLES &&
              observation.counts[0] == 6 && observation.counts[1] == 6);
        CHECK(call->literal != NULL &&
              call->literal == app->source_namespace->startup_logo_resource);
        mc_player_string_view text;
        CHECK(mc_player_string_read(call->literal, &text) && text.length == 17);
        static const char expected[] = "/title/mojang.png";
        CHECK(text.length == sizeof(expected) - 1);
        for (size_t i = 0; i < text.length; ++i)
            CHECK(text.units[i] == (uint16_t)(unsigned char)expected[i]);
        bool found = false;
        mc_client_texture cached = 0;
        CHECK(mc_client_render_engine_cached_texture(
                  app->render_engine, (mc_texture_pack_text){text.units, text.length, false},
                  &found, &cached) &&
              found && cached == call->texture_id);
        CHECK(attempt->display->source.texture_binding_known &&
              attempt->display->source.bound_texture == call->texture_id &&
              attempt->display->source.texture_enabled && attempt->display->source.alpha_test &&
              !attempt->display->source.lighting_enabled && !attempt->display->source.fog_enabled &&
              !attempt->display->frame_started);
        GLint viewport[4] = {-1, -1, -1, -1};
        glGetIntegerv(GL_VIEWPORT, viewport);
        CHECK(glGetError() == GL_NO_ERROR && viewport[0] == 0 && viewport[1] == 0 &&
              viewport[2] == app->d && viewport[3] == app->e);
        CHECK(attempt->display->source.loading_ready &&
              attempt->display->source.modelview.column_major[14] == -2000.0f);
    } else {
        CHECK(result == MC_CLIENT_STARTUP_LOGO_FAILED && !call->source_returned &&
              !call->background_drawn && !call->logo_drawn && !call->swapped &&
              observation.draws == 0);
        if (strcmp(mode, "null-settings") == 0)
            CHECK(call->pc == 16 && !call->native_policy && call->literal == NULL &&
                  call->resolution_error.status == MC_CLIENT_LOADING_NULL_REFERENCE &&
                  call->child.pc == 20 && observation.clears == 0);
        else if (strcmp(mode, "null-engine") == 0)
            CHECK(call->pc == 121 && !call->native_policy && call->literal != NULL &&
                  call->literal == app->source_namespace->startup_logo_resource &&
                  observation.clears == 1);
        else if (strcmp(mode, "fp") == 0)
            CHECK(call->pc == 16 && call->native_policy && call->literal == NULL &&
                  call->resolution_error.status == MC_CLIENT_LOADING_UNSUPPORTED_FP &&
                  call->child.pc == 102 && observation.clears == 0);
        else {
            CHECK(strcmp(mode, "late-fp") == 0 && call->pc == 51 && call->native_policy &&
                  call->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP &&
                  observation.clears == 1 && call->literal == NULL &&
                  !attempt->display->source.loading_ready);
        }
    }
    const unsigned old_draws = observation.draws;
    CHECK(application_startup_logo(attempt) == MC_CLIENT_STARTUP_INVALID_PHASE &&
          attempt->logo == call && observation.draws == old_draws);
    mc_client_render_manager_classes *manager = attempt->manager_classes;
    CHECK(manager != NULL);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(attempt != NULL && attempt->logo == call && app->display != NULL);
    CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
              MC_CLIENT_MANAGER_OK &&
          manager == NULL);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(attempt != NULL && attempt->logo == call && app->display != NULL);
    /* SAME-runtime final outer namespace authority, with the actual Display
     * alive. The callee/per-App retirement cannot infer terminal ownership. */
    CHECK(mc_client_render_engine_retire_resources(f.graphics, NULL) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL);
    CHECK(app->startup == NULL && app->display == NULL && app->render_engine == NULL &&
          app->settings == NULL && app->font_renderer == NULL && app->t == NULL &&
          atomic_load_explicit(&app->J, memory_order_seq_cst) == j_before);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_OK &&
          app == NULL);
    CHECK(startup_services_close(&startup, &f) == 0);
    free(probe);
    _mm_setcsr(saved_fp);
    printf("PREPARED-CALLEE MC.x %s: %u checks; coordinator stopped324; no "
           "skipped calls\n",
           mode, checks);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4);
    CHECK(strcmp(argv[3], "success") == 0 || strcmp(argv[3], "null-settings") == 0 ||
          strcmp(argv[3], "null-engine") == 0 || strcmp(argv[3], "fp") == 0 ||
          strcmp(argv[3], "late-fp") == 0);
    return prepared_logo_control(argv[1], argv[2], argv[3]);
}
