#include "menu_loop.h"

#include "../application_state.h"
#include "../display/sdk_update.h"
#include "../display/source_surface.h"
#include "../display/staged.h"
#include "../gui/source_screen_drawing.h"
#include "../gui/source_screen_events.h"
#include "../gui/source_screen_transitions.h"
#include "../gui/source_world_selection_drawing.h"
#include "../render/model_graph_internal.h"
#include "../render_engine/chunk_statistics.h"
#include "../render_engine/effect_upload.h"
#include "../startup/application_gl_diagnostics.h"
#include "../startup/application_startup.h"
#include "../world/entity_renderer/internal.h"
#include "mc/client/gui/gui_ingame.h"
#include "mc/client/statistics/writer.h"
#include "mc/core/java/java.h"
#include "mc/entity/bounds/bounds.h"
#include "mc/entity/vector/vector.h"

#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

struct mc_client_menu_loop {
    client_application *app;
    mc_gui_main_menu *renderer;
    mc_client_assets *assets;
    mc_client_render_engine_classes *graphics;
    mc_client_screen_transitions *transitions;
    bool active, retiring;
    mc_client_menu_loop_state state;
    int64_t last_second;
    int32_t second_frames;
    mc_client_source_screen_draw_error draw;
    mc_client_world_selection_draw_error world_draw;
    mc_client_screen_event_error events;
    mc_client_screen_event_report event_report;
    mc_client_effect_upload_error effects;
    mc_client_texture_lookup_error lookup;
    mc_client_sdk_update_error update;
    mc_client_sdk_input_error input;
    mc_client_application_gl_diagnostic_error diagnostic;
    mc_client_gui_ingame_error ingame;
    mc_client_notice_error notice;
    mc_client_stats_error statistics;
    mc_sound_error sound;
    mc_client_screen_error screen;
    mc_client_display_error display;
    mc_client_graphics_error graphics_error;
};

static bool stop(mc_client_menu_loop *loop, const char *operation, const char *source_class,
                 const char *method, int64_t pc) {
    loop->state.failed = true;
    loop->state.operation = operation;
    loop->state.source_class = source_class;
    loop->state.source_method = method;
    loop->state.source_pc = pc;
    fprintf(stderr, "Client menu: %s (%s.%s:%" PRId64 ")\n", operation,
            source_class == NULL ? "native" : source_class, method == NULL ? "frame" : method, pc);
    return false;
}

static bool run_stop(mc_client_menu_loop *loop, const char *operation, unsigned int pc) {
    return stop(loop, operation, "net/minecraft/client/Minecraft", "run", pc);
}

static bool millis(void *context, int64_t *out) {
    (void)context;
    return mc_platform_current_time_millis(out) == MC_PLATFORM_MILLIS_OK;
}

static bool nanoseconds(mc_client_menu_loop *loop, int64_t *out, unsigned int pc) {
    return mc_platform_nanotime_sample(&loop->app->clock, out) == MC_PLATFORM_NANOTIME_OK ||
           run_stop(loop, "actual System.nanoTime native query", pc);
}

static bool render_nanoseconds(mc_client_menu_loop *loop, int64_t *out, unsigned int pc) {
    return mc_platform_nanotime_sample(&loop->app->clock, out) == MC_PLATFORM_NANOTIME_OK ||
           stop(loop, "actual System.nanoTime native query", "px", "b", pc);
}

static bool key_down(mc_client_menu_loop *loop, int32_t key, bool *out, unsigned int pc) {
    return mc_client_sdk_keyboard_key_down(mc_client_display_inputs(loop->app->display), key, out,
                                           &loop->input) == MC_CLIENT_SDK_INPUT_OK ||
           run_stop(loop, loop->input.operation, pc);
}

static bool display_active(mc_client_menu_loop *loop, bool *out, unsigned int pc) {
    return mc_client_sdk_display_active(mc_client_display_update_owner(loop->app->display), out,
                                        &loop->update) == MC_CLIENT_SDK_UPDATE_OK ||
           run_stop(loop, loop->update.operation, pc);
}

static bool display_update(mc_client_menu_loop *loop, unsigned int pc) {
    return mc_client_sdk_display_update(mc_client_display_update_owner(loop->app->display),
                                        &loop->update) == MC_CLIENT_SDK_UPDATE_OK ||
           run_stop(loop, loop->update.operation, pc);
}

static bool screen_failure(mc_client_menu_loop *loop) {
    const mc_client_screen_error *error = &loop->events.screen;
    return stop(loop, error->operation == NULL ? "source screen call" : error->operation,
                error->site.class_name, error->site.method, error->site.pc);
}

static bool screen_state(mc_client_menu_loop *loop, mc_client_screen_state *out) {
    return mc_client_screen_inspect(loop->app->r, out, &loop->screen) == MC_CLIENT_SCREEN_OK ||
           stop(loop, "actual screen inspection", "da", NULL, 0);
}

static bool texture_tick(mc_client_menu_loop *loop) {
    if (!mc_client_render_engine_update_effects(loop->app->render_engine, loop->app->display,
                                                &loop->effects))
        return stop(loop, loop->effects.operation, "ji", "a", loop->effects.ji_pc);
    ++loop->state.texture_ticks;
    return true;
}

static bool tick(mc_client_menu_loop *loop) {
    client_application *app = loop->app;
    /* Approved account-authentication exclusion: k11 would start kp, whose
     * only work validates gr.b/gr.c with the account-session endpoint. No kp
     * completion, request, response or license-status H store is supplied. */
    if (app->V == 6000)
        ++loop->state.account_checks_omitted;
    if (mc_client_stats_writer_service(app->stats_writer, NULL, &loop->statistics) !=
        MC_CLIENT_STATS_OK)
        return stop(loop, loop->statistics.operation, loop->statistics.class_name,
                    loop->statistics.method, loop->statistics.bytecode);
    if (mc_client_gui_ingame_tick(app->v, &loop->ingame) != MC_CLIENT_GUI_INGAME_OK)
        return stop(loop, loop->ingame.operation, "uq", "a", loop->ingame.site.pc);
    /* Actual px.a(F)V reads j.i4 then j.f15 and returns at10/21. No
     * mouseover/World/controller operation is reached in this menu profile. */
    if (app->t == NULL)
        return stop(loop, "actual null EntityRenderer receiver", "net/minecraft/client/Minecraft",
                    "k", 33);
    if (app->t->fields.j == NULL)
        return stop(loop, "actual null px.j", "px", "a", 4);
    if (app->t->fields.j->i != NULL && app->t->fields.j->active_world != NULL)
        return stop(loop, "actual World mouseover is not connected", "px", "a", 29);
    if (app->h != NULL)
        return stop(loop, "actual Player chunk-provider tick is not connected",
                    "net/minecraft/client/Minecraft", "k", 47);
    if (!atomic_load(&app->o) && app->active_world != NULL)
        return stop(loop, "actual controller World tick is not connected",
                    "net/minecraft/client/Minecraft", "k", 119);
    static const uint16_t terrain[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    mc_client_texture id = 0;
    if (mc_client_render_engine_texture(
            app->render_engine, app->display, NULL,
            (mc_texture_pack_text){terrain, sizeof(terrain) / sizeof(*terrain), false}, &id,
            &loop->lookup) != MC_CLIENT_LOOKUP_OK)
        return stop(loop, loop->lookup.operation, "ji", "b", loop->lookup.site.pc);
    if (!mc_client_display_source_bind_id(app->display, id, &loop->display))
        return stop(loop, loop->display.operation, "net/minecraft/client/Minecraft", "k", 134);
    if (!atomic_load(&app->o) && !texture_tick(loop))
        return false;
    if (app->r != NULL) {
        app->W = 10000;
        app->ag = mc_java_i32_add(app->V, 10000);
        if (mc_client_screen_handle_input_with_transitions(
                app->r, mc_client_display_inputs(app->display), loop->transitions,
                &loop->event_report, &loop->events) != MC_CLIENT_SCREEN_OK)
            return screen_failure(loop);
        loop->state.mouse_events += loop->event_report.mouse_events;
        loop->state.keyboard_events += loop->event_report.keyboard_events;
        if (app->r != NULL) {
            mc_client_screen_state state;
            if (!screen_state(loop, &state))
                return false;
            if (state.da.h == NULL)
                return stop(loop, "actual null screen-particle receiver",
                            "net/minecraft/client/Minecraft", "k", 310);
            if (mc_client_screen_particles_tick(state.da.h, &loop->events) != MC_CLIENT_SCREEN_OK ||
                mc_client_screen_tick(app->r, &loop->events) != MC_CLIENT_SCREEN_OK)
                return screen_failure(loop);
        }
    }
    if (app->r == NULL)
        return stop(loop, "actual gameplay input is not connected",
                    "net/minecraft/client/Minecraft", "k", 337);
    mc_client_screen_state state;
    if (!screen_state(loop, &state))
        return false;
    if (state.da.f)
        return stop(loop, "actual pass-through gameplay input is not connected",
                    "net/minecraft/client/Minecraft", "k", 337);
    if (app->active_world != NULL)
        return stop(loop, "native menu profile does not admit the reached World tick branch",
                    "net/minecraft/client/Minecraft", "k", 1074);
    if (!millis(NULL, &app->P))
        return stop(loop, "actual tick currentTimeMillis query", "net/minecraft/client/Minecraft",
                    "k", 1329);
    ++loop->state.ticks;
    return true;
}

static bool render_menu(mc_client_menu_loop *loop, float partial) {
    client_application *app = loop->app;
    mc_client_source_px *px = app->t;
    if (px == NULL || px->phase != MC_CLIENT_ENTITY_RENDERER_READY || px->active ||
        px->fields.j != app)
        return stop(loop, "actual EntityRenderer native admission", "px", "b", 0);
    bool active;
    if (mc_client_sdk_display_active(mc_client_display_update_owner(app->display), &active,
                                     &loop->update) != MC_CLIENT_SDK_UPDATE_OK)
        return stop(loop, loop->update.operation, "px", "b", 0);
    int64_t now;
    if (!active) {
        if (!millis(NULL, &now))
            return stop(loop, "actual inactive-render clock", "px", "b", 6);
        if (mc_java_i64_sub(now, px->fields.H) > 500 && app->r == NULL)
            return stop(loop, "actual pause-screen allocation is not connected",
                        "net/minecraft/client/Minecraft", "i", 9);
    } else {
        if (!millis(NULL, &now))
            return stop(loop, "actual active-render clock", "px", "b", 32);
        px->fields.H = now;
    }
    if (app->N)
        return stop(loop, "actual focused Player rotation is not connected", "px", "b", 55);
    if (app->w)
        return true;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return stop(loop, "actual render Settings receiver", "px", "b", 212);
    app->render_classes->px_a = settings.values.anaglyph;
    mc_scaled_resolution projection;
    if (!mc_scaled_resolution_init(app->d, app->e, settings.values.gui_scale, &projection))
        return stop(loop, "actual frame scaled resolution", "px", "b", 243);
    int32_t x, y;
    mc_client_sdk_input *input = mc_client_display_inputs(app->display);
    if (mc_client_sdk_mouse_x(input, &x, &loop->input) != MC_CLIENT_SDK_INPUT_OK)
        return stop(loop, loop->input.operation, "px", "b", 258);
    if (mc_client_sdk_mouse_y(input, &y, &loop->input) != MC_CLIENT_SDK_INPUT_OK)
        return stop(loop, loop->input.operation, "px", "b", 275);
    mc_gui_mouse_position mouse;
    if (!mc_scaled_resolution_map_mouse(&projection, app->d, app->e, x, y, &mouse))
        return stop(loop, "actual frame mouse arithmetic", "px", "b", 270);
    if (app->active_world != NULL)
        return stop(loop, "native menu profile does not admit the reached World render branch",
                    "px", "b", 339);
    /* Place the no-World overlay bridge before the source delay/I clock. The
     * screen bridge begins again at its later depth clear before FU geometry.
     * Both qq values have the same immutable Settings and physical dimensions;
     * no separately constructed source qq identity is claimed here. */
    if (!mc_client_display_begin(app->display, projection.width_exact, projection.height_exact,
                                 &loop->display))
        return stop(loop, "PX no-World overlay GL3.3 bridge", "px", "b", 550);
    if (settings.values.performance_mode == 2) {
        int64_t sample;
        if (!render_nanoseconds(loop, &sample, 578))
            return false;
        int64_t delay = mc_java_i64_sub(mc_java_i64_add(px->fields.I, 25000000), sample) / 1000000;
        if (delay < 0)
            delay = mc_java_i64_add(delay, 10);
        if (delay > 0 && delay < 500)
            SDL_Delay((uint32_t)delay);
    }
    if (!render_nanoseconds(loop, &px->fields.I, 635))
        return false;
    if (app->r != NULL) {
        mc_client_screen_state drawn;
        if (!screen_state(loop, &drawn))
            return false;
        if (drawn.kind == MC_CLIENT_SCREEN_WORLD_SELECTION) {
            if (!mc_client_source_world_selection_draw(
                    (mc_client_source_rq *)app->r, loop->renderer, &projection, mouse, partial,
                    input, millis, NULL, loop->transitions, &loop->world_draw)) {
                const mc_client_screen_error *e = &loop->world_draw.list.screen;
                return stop(loop, e->operation == NULL ? "RQ source drawing bridge" : e->operation,
                            e->site.class_name, e->site.method, e->site.pc);
            }
            loop->state.font_measurements += loop->world_draw.list.font_measurements;
        } else {
            if (!mc_client_source_screen_draw(app->r, loop->renderer, &projection, mouse, partial,
                                              millis, NULL, &loop->draw))
                return stop(loop, loop->draw.operation, "px", "b", 669);
            loop->state.font_measurements += loop->draw.font_measurements;
        }
        if (app->r != NULL) {
            mc_client_screen_state screen;
            if (!screen_state(loop, &screen))
                return false;
            if (screen.da.h != NULL &&
                mc_client_screen_particles_draw(screen.da.h, partial, &loop->events) !=
                    MC_CLIENT_SCREEN_OK)
                return screen_failure(loop);
        }
    }
    return true;
}

static bool fps(mc_client_menu_loop *loop) {
    int64_t now;
    while (millis(NULL, &now)) {
        if (now < mc_java_i64_add(loop->last_second, 1000))
            return true;
        int32_t updates;
        if (!mc_client_chunk_statistics_get(
                loop->graphics,
                (mc_client_graphics_site){"net/minecraft/client/Minecraft", "run", "()V", 589},
                &updates, &loop->graphics_error))
            return run_stop(loop, "actual dk.b class/getstatic dependency", 589);
        char text[96];
        const int length =
            snprintf(text, sizeof(text), "%" PRId32 " fps, %" PRId32 " chunk updates",
                     loop->second_frames, updates);
        if (length < 0 || (size_t)length >= sizeof(text))
            return run_stop(loop, "native FPS String bound", 600);
        uint16_t units[96];
        for (int i = 0; i < length; ++i)
            units[i] = (unsigned char)text[i];
        mc_player_string *value = NULL;
        if (mc_player_string_create(units, (size_t)length, &value) != MC_PLAYER_OK)
            return run_stop(loop, "actual FPS immutable String allocation", 600);
        mc_player_string *old = loop->app->K;
        loop->app->K = value;
        mc_player_string_release(old);
        if (!mc_client_chunk_statistics_reset(
                loop->graphics,
                (mc_client_graphics_site){"net/minecraft/client/Minecraft", "run", "()V", 607},
                &loop->graphics_error))
            return run_stop(loop, "actual dk.b putstatic dependency", 607);
        loop->last_second = mc_java_i64_add(loop->last_second, 1000);
        loop->second_frames = 0;
    }
    return run_stop(loop, "actual FPS currentTimeMillis query", 560);
}

bool mc_client_menu_loop_create(mc_client_bootstrap *bootstrap, mc_client_menu_loop **out) {
    if (bootstrap == NULL || out == NULL)
        return false;
    client_application *app = mc_client_bootstrap_app(bootstrap);
    if (app == NULL || app->constructor_phase != MC_CLIENT_APP_READY || app->r == NULL ||
        app->active_world != NULL || app->display == NULL)
        return false;
    mc_client_menu_loop *loop = calloc(1, sizeof(*loop));
    if (loop == NULL)
        return false;
    loop->app = app;
    loop->graphics = mc_client_bootstrap_graphics(bootstrap);
    mc_game_settings_state settings;
    mc_gui_main_menu_error error = {0};
    if (!mc_game_settings_read_state(app->settings, &settings) ||
        mc_client_assets_create(mc_client_bootstrap_resources(bootstrap), app->registry,
                                &loop->assets) != MC_CLIENT_ASSETS_OK ||
        !mc_gui_main_menu_create(app->display, loop->assets, settings.values.anaglyph,
                                 &loop->renderer, &error) ||
        !millis(NULL, &loop->last_second) ||
        mc_client_screen_transitions_create(app, &loop->transitions, &loop->screen) !=
            MC_CLIENT_SCREEN_OK) {
        fprintf(stderr, "Client menu renderer setup failed: %s\n",
                error.operation == NULL ? "native Settings/assets/clock prerequisite"
                                        : error.operation);
        mc_client_menu_loop_free(loop);
        return false;
    }
    loop->state.running = atomic_load(&app->J);
    *out = loop;
    return true;
}

static bool frame_body(mc_client_menu_loop *loop) {
    if (loop == NULL || loop->state.failed || !loop->state.running)
        return false;
    client_application *app = loop->app;
    if (!atomic_load(&app->J)) {
        loop->state.running = false;
        return true;
    }
    if (app->A != NULL)
        return run_stop(loop, "actual Applet activity is not connected", 56);
    mc_entity_bounds_error bounds = {0};
    mc_entity_vector_error vectors = {0};
    if (mc_entity_bounds_pool_reset(app->entities, &bounds) != MC_ENTITY_OK)
        return run_stop(loop, "actual bounds pool reset", 65);
    if (mc_entity_vector_pool_reset(app->entities, &vectors) != MC_ENTITY_OK)
        return run_stop(loop, "actual vector pool reset", 68);
    bool close;
    if (app->m == NULL) {
        if (mc_client_sdk_display_close_requested(mc_client_display_update_owner(app->display),
                                                  &close, &loop->update) != MC_CLIENT_SDK_UPDATE_OK)
            return run_stop(loop, loop->update.operation, 78);
        if (close && mc_client_application_source_stop(app, NULL) != MC_CLIENT_STARTUP_OK)
            return run_stop(loop, "actual source f stop", 85);
    }
    if (atomic_load(&app->o) && app->active_world != NULL)
        return run_stop(loop, "actual paused World frame is not connected", 115);
    mc_timer_clock_sample clocks;
    if (!millis(NULL, &clocks.wall_millis) || !nanoseconds(loop, &clocks.nano_time, 134) ||
        !mc_timer_update(app->timer, clocks))
        return run_stop(loop, "actual Timer update", 134);
    int64_t tick_start, tick_end;
    if (!nanoseconds(loop, &tick_start, 137))
        return false;
    mc_timer_view timer;
    if (!mc_timer_view_get(app->timer, &timer))
        return run_stop(loop, "actual Timer fields", 151);
    for (int32_t i = 0; i < timer.ticks; ++i) {
        app->V = mc_java_i32_add(app->V, 1);
        if (!tick(loop))
            return false;
    }
    if (!nanoseconds(loop, &tick_end, 203))
        return false;
    (void)mc_java_i64_sub(tick_end, tick_start);
    if (mc_client_application_gl_diagnostic(app->display, NULL, &loop->diagnostic) !=
        MC_CLIENT_GL_DIAGNOSTIC_OK)
        return run_stop(loop, "actual Pre render GL diagnostic", 214);
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return run_stop(loop, "actual Settings receiver", 221);
    app->render_classes->cv_a = settings.values.fancy_graphics;
    if (app->h != NULL)
        return run_stop(loop, "actual Player sound listener is not connected", 242);
    if (mc_sound_manager_listener_update(app->B, NULL, timer.partial_ticks, NULL, NULL,
                                         &loop->sound) != MC_SOUND_OK)
        return run_stop(loop, loop->sound.operation, 242);
    if (!mc_client_display_source_texture_enable(app->display, true, &loop->display))
        return run_stop(loop, loop->display.operation, 248);
    if (app->active_world != NULL)
        return run_stop(loop, "actual World pre-render service is not connected", 262);
    bool f7;
    if (!key_down(loop, 65, &f7, 268) || (!f7 && !display_update(loop, 274)))
        return false;
    if (!app->w) {
        if (app->controller != NULL)
            return run_stop(loop, "actual controller render interpolation is not connected", 327);
        if (!render_menu(loop, timer.partial_ticks))
            return false;
    }
    bool active;
    if (!display_active(loop, &active, 344))
        return false;
    if (!active) {
        if (app->Q)
            return run_stop(loop, "actual fullscreen transition is not connected", 358);
        SDL_Delay(10);
    }
    if (!mc_game_settings_read_state(app->settings, &settings))
        return run_stop(loop, "actual Settings receiver", 371);
    if (settings.values.show_debug_info)
        return run_stop(loop, "actual debug timing graph is not connected", 380);
    if (!nanoseconds(loop, &app->M, 387))
        return false;
    if (mc_client_achievement_notice_draw(app->achievement_notice, &loop->notice) !=
        MC_CLIENT_NOTICE_OK)
        return stop(loop, "actual achievement notice draw dependency", "vu", "a",
                    loop->notice.source_pc);
    thrd_yield();
    if (!key_down(loop, 65, &f7, 405) || (f7 && !display_update(loop, 411)))
        return false;
    bool f2;
    if (mc_client_sdk_keyboard_key_down(mc_client_display_inputs(app->display), 60, &f2,
                                        &loop->input) != MC_CLIENT_SDK_INPUT_OK)
        return stop(loop, loop->input.operation, "net/minecraft/client/Minecraft", "z", 2);
    if (f2) {
        if (!app->L) {
            app->L = true;
            return stop(loop, "actual F2 screenshot writer is not connected",
                        "net/minecraft/client/Minecraft", "z", 35);
        }
    } else
        app->L = false;
    if (app->m != NULL)
        return run_stop(loop, "actual Canvas sizing is not connected", 436);
    if (mc_client_application_gl_diagnostic(app->display, NULL, &loop->diagnostic) !=
        MC_CLIENT_GL_DIAGNOSTIC_OK)
        return run_stop(loop, "actual Post render GL diagnostic", 521);
    loop->second_frames = mc_java_i32_add(loop->second_frames, 1);
    /* Actual l(): no World -> false. Admitted FU inherits da.c() == true. */
    bool pauses = false;
    if (app->r != NULL) {
        mc_client_screen_state screen;
        if (!screen_state(loop, &screen))
            return false;
        if (screen.kind != MC_CLIENT_SCREEN_MAIN_MENU &&
            screen.kind != MC_CLIENT_SCREEN_WORLD_SELECTION)
            return run_stop(loop, "actual screen pause virtual route is not connected", 546);
        pauses = true;
    }
    atomic_store(&app->o, pauses);
    if (!fps(loop))
        return false;
    ++loop->state.frames;
    loop->state.running = atomic_load(&app->J);
    return true;
}

bool mc_client_menu_loop_frame(mc_client_menu_loop *loop) {
    if (loop == NULL || loop->active || loop->retiring)
        return false;
    loop->active = true;
    const bool returned = frame_body(loop);
    loop->active = false;
    return returned;
}

bool mc_client_menu_loop_inspect(const mc_client_menu_loop *loop, mc_client_menu_loop_state *out) {
    if (loop == NULL || out == NULL)
        return false;
    *out = loop->state;
    return true;
}

bool mc_client_menu_loop_retire(mc_client_menu_loop **slot) {
    if (slot == NULL)
        return false;
    mc_client_menu_loop *loop = *slot;
    if (loop == NULL)
        return true;
    if (loop->active)
        return false;
    loop->retiring = true;
    if (mc_client_screen_event_retire(&loop->events) != MC_CLIENT_SCREEN_OK ||
        mc_client_source_world_selection_draw_retire(&loop->world_draw) != MC_CLIENT_SCREEN_OK ||
        mc_client_screen_transitions_retire(&loop->transitions, &loop->screen) !=
            MC_CLIENT_SCREEN_OK)
        return false;
    mc_gui_main_menu_free(loop->renderer);
    mc_client_assets_free(loop->assets);
    free(loop);
    *slot = NULL;
    return true;
}

void mc_client_menu_loop_free(mc_client_menu_loop *loop) {
    if (!mc_client_menu_loop_retire(&loop))
        fputs("Native menu still owns an active or externally retained screen\n", stderr);
}

static bool capture(mc_client_menu_loop *loop, const char *path) {
    mc_client_capture pixels = {0};
    if (!mc_client_display_capture_rgba(loop->app->display, &pixels, &loop->display))
        return false;
    for (size_t y = 0; y < (size_t)pixels.height; ++y)
        for (size_t x = 0; x < (size_t)pixels.width; ++x)
            pixels.pixels[y * pixels.stride + x * 4 + 3] = 255;
    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        pixels.width, pixels.height, SDL_PIXELFORMAT_RGBA32, pixels.pixels, (int)pixels.stride);
    const bool okay = surface != NULL && SDL_SavePNG(surface, path);
    if (surface != NULL)
        SDL_DestroySurface(surface);
    mc_client_capture_free(&pixels);
    return okay;
}

int mc_client_menu_run(const mc_client_application_options *options) {
    mc_client_bootstrap *bootstrap = NULL;
    mc_client_menu_loop *loop = NULL;
    int result = 1;
    if (mc_client_bootstrap_open(options, &bootstrap) != MC_CLIENT_BOOTSTRAP_OK ||
        !mc_client_menu_loop_create(bootstrap, &loop))
        goto cleanup;
    bool captured = false;
    while (loop->state.running) {
        if (!mc_client_menu_loop_frame(loop))
            goto cleanup;
        if (!captured && options->capture != NULL) {
            if (!capture(loop, options->capture))
                goto cleanup;
            captured = true;
        }
        if (options->frame_limit != 0 && loop->state.frames >= options->frame_limit)
            break;
    }
    fprintf(stdout,
            "Source menu: %" PRIu64 " frames, %" PRIu64 " ticks, %" PRIu64 " texture ticks\n",
            loop->state.frames, loop->state.ticks, loop->state.texture_ticks);
    result = 0;
cleanup:
    for (unsigned int attempt = 0; loop != NULL && attempt < 35000; ++attempt) {
        if (!mc_client_menu_loop_retire(&loop))
            SDL_Delay(1);
    }
    if (loop != NULL) {
        fputs("Native menu cleanup retains a live screen graph\n", stderr);
        return 1;
    }
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    for (unsigned int attempt = 0; closed == MC_CLIENT_BOOTSTRAP_BUSY && attempt < 35000;
         ++attempt) {
        closed = mc_client_bootstrap_close(&bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    }
    if (closed != MC_CLIENT_BOOTSTRAP_OK) {
        fputs("Client bootstrap cleanup retains a live owner\n", stderr);
        result = 1;
    }
    return result;
}
