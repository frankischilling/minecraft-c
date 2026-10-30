#include "menu_loop.h"
#include "gameplay_input.h"
#include "world_tick.h"

#include "../../world/runtime/chunk_provider_internal.h"
#include "../application_state.h"
#include "../display/sdk_update.h"
#include "../display/source_surface.h"
#include "../display/staged.h"
#include "../gui/source_pause_drawing.h"
#include "../gui/source_screen_drawing.h"
#include "../gui/source_screen_events.h"
#include "../gui/source_screen_transitions.h"
#include "../gui/source_screen_transitions_terminal_internal.h"
#include "../gui/source_world_selection_drawing.h"
#include "../render/model_graph_internal.h"
#include "../render_engine/chunk_statistics.h"
#include "../render_engine/effect_upload.h"
#include "../startup/application_gl_diagnostics.h"
#include "../startup/application_startup.h"
#include "../world/entity_renderer/internal.h"
#include "mc/client/controller/render_interpolation.h"
#include "mc/client/controller/singleplayer_tick.h"
#include "mc/client/gui/gui_ingame.h"
#include "mc/client/sound/sound_manager.h"
#include "mc/client/statistics/writer.h"
#include "mc/client/world/picking.h"
#include "mc/client/world/renderer_focus.h"
#include "mc/client/world/world_frame.h"
#include "mc/core/java/java.h"
#include "mc/entity/bounds/bounds.h"
#include "mc/entity/player/player.h"
#include "mc/entity/tick/environment.h"
#include "mc/entity/vector/vector.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/propagation.h"

#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

struct mc_client_menu_loop {
    mc_client_bootstrap *bootstrap; /* Actual creator, borrowed through checked retirement. */
    client_application *app;
    mc_gui_main_menu *renderer;
    mc_client_assets *assets;
    mc_client_render_engine_classes *graphics;
    mc_client_screen_transitions *transitions;
    mc_math_classes *math; /* Same bootstrap namespace, borrowed through loop retirement. */
    mc_client_entity_renderer_pick_call *pick;
    mc_client_entity_renderer_pick_state pick_state;
    mc_client_entity_renderer_pick_error pick_cleanup;
    mc_client_entity_renderer_focus_call *focus;
    mc_client_renderer_focus_state focus_state;
    mc_client_renderer_focus_error focus_error, focus_cleanup;
    mc_client_world_frame_call *world_frame;
    mc_client_world_frame_state world_frame_state;
    mc_client_world_frame_error world_frame_error, world_frame_cleanup;
    mc_singleplayer_tick_call *controller_tick;
    mc_singleplayer_tick_state controller_state;
    mc_singleplayer_tick_error controller_failure, controller_cleanup;
    mc_singleplayer_tick_error controller_failure_copy;
    mc_sound_error controller_sound_copy;
    mc_sound_manager_report controller_sound_report_copy;
    mc_client_display_screen_call *screen_display;
    mc_client_display_screen_call screen_display_copy;
    mc_local_player_snapshot player_snapshot;
    mc_player_error player_error;
    mc_client_gameplay_input gameplay;
    mc_client_world_tick world_tick;
    mc_world_light_operation_error world_light;
    mc_entity_tick_error suffocation;
    mc_game_settings_error frame_settings;
    mc_singleplayer_render_interpolation_report interpolation;
    mc_singleplayer_render_interpolation_error interpolation_error;
    bool active, retiring, setup_complete;
    mc_world_storage_error retirement_world;
    mc_client_menu_loop_state state;
    int64_t last_second;
    int32_t second_frames;
    mc_client_source_screen_draw_error draw;
    mc_client_pause_draw_error pause_draw;
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

static bool provider_tick(mc_client_menu_loop *loop) {
    client_application *app = loop->app;
    if (app->h == NULL)
        return true;
    if (app->active_world == NULL)
        return stop(loop, "actual fd.w null World receiver", "net/minecraft/client/Minecraft", "k",
                    47);
    mc_world_runtime_fields world;
    if (mc_world_runtime_fields_get(app->active_world, &world) != MC_WORLD_STORAGE_OK)
        return stop(loop, "native actual fd.v field access", "net/minecraft/client/Minecraft", "k",
                    47);
    /* Actual fd.w() reads v1/returns4. Neither NULL nor the constructed ok
     * map-cache owner is kx. The unrelated SaveHandler w is not read here. */
    switch (mc_chunk_provider_concrete_kind_of(world.v)) {
        case MC_CHUNK_PROVIDER_NULL:
        case MC_CHUNK_PROVIDER_MAP_CACHE:
            return true;
        default:
            return stop(loop, "native unresolved actual ChunkProvider class provenance",
                        "net/minecraft/client/Minecraft", "k", 52);
    }
}

static bool controller_tick(mc_client_menu_loop *loop) {
    if (loop->app->controller == NULL)
        return stop(loop, "actual ob.c null controller receiver", "net/minecraft/client/Minecraft",
                    "k", 119);
    const mc_singleplayer_tick_status status = mc_client_singleplayer_controller_tick_begin(
        loop->app->controller, &loop->controller_tick, &loop->controller_failure);
    if (loop->controller_tick != NULL) {
        (void)mc_client_singleplayer_controller_tick_inspect(loop->controller_tick,
                                                             &loop->controller_state);
        loop->state.controller_tick = &loop->controller_state;
    }
    if (status != MC_SINGLEPLAYER_TICK_OK) {
        const mc_singleplayer_tick_error *e = loop->controller_tick == NULL
                                                  ? &loop->controller_failure
                                                  : loop->controller_state.error;
        return stop(loop, e->operation, e->site.class_name, e->site.method, e->site.pc);
    }
    if (mc_client_singleplayer_controller_tick_retire(
            &loop->controller_tick, &loop->controller_cleanup) != MC_SINGLEPLAYER_TICK_OK)
        return stop(loop, loop->controller_cleanup.operation, "os", "c", 18);
    loop->state.controller_tick = NULL;
    loop->controller_state = (mc_singleplayer_tick_state){0};
    return true;
}

static bool health_tick(mc_client_menu_loop *loop) {
    client_application *app = loop->app;
    if (app->r != NULL || app->h == NULL)
        return true;
    if (mc_local_player_get_snapshot(app->h, &loop->player_snapshot) != MC_ENTITY_OK)
        return stop(loop, "native actual dc.Y field access", "net/minecraft/client/Minecraft", "k",
                    169);
    if (loop->player_snapshot.living.Y <= 0) {
        const mc_client_display_screen_status status = mc_client_display_screen_run(
            app, mc_client_screen_transitions_classes(loop->transitions), NULL,
            &loop->screen_display);
        loop->state.display_call = loop->screen_display;
        if (status != MC_CLIENT_DISPLAY_SCREEN_OK) {
            if (loop->screen_display == NULL)
                return stop(loop, "actual display(NULL) native admission",
                            "net/minecraft/client/Minecraft", "k", 177);
            return stop(loop, loop->screen_display->operation,
                        loop->screen_display->site.class_name, loop->screen_display->site.method,
                        loop->screen_display->site.pc);
        }
        if (mc_client_display_screen_retire(&loop->screen_display) != MC_CLIENT_DISPLAY_SCREEN_OK)
            return stop(loop, "actual display(NULL) native retirement",
                        "net/minecraft/client/Minecraft", "k", 177);
        loop->state.display_call = NULL;
        return true;
    }
    bool sleeping;
    if (mc_player_is_sleeping(app->h, &sleeping, &loop->player_error) != MC_PLAYER_OK) {
        loop->state.player = &loop->player_error;
        return stop(loop, loop->player_error.operation, "gs", "N", 1);
    }
    if (sleeping && app->active_world != NULL) {
        mc_world_runtime_fields world;
        if (mc_world_runtime_fields_get(app->active_world, &world) != MC_WORLD_STORAGE_OK)
            return stop(loop, "native actual fd.B field access", "net/minecraft/client/Minecraft",
                        "k", 204);
        if (world.B)
            return stop(loop, "actual gh sleeping-screen allocation is not connected",
                        "net/minecraft/client/Minecraft", "k", 211);
    }
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
    if (app->t == NULL)
        return stop(loop, "actual null EntityRenderer receiver", "net/minecraft/client/Minecraft",
                    "k", 33);
    if (mc_client_entity_renderer_pick_begin(app->t, loop->math, 1.0f, &loop->pick) !=
        MC_CLIENT_PICK_OK) {
        if (loop->pick == NULL)
            return stop(loop, "actual PX picking native admission", "px", "a", 0);
        (void)mc_client_entity_renderer_pick_inspect(loop->pick, &loop->pick_state);
        const mc_client_entity_renderer_pick_error *e = loop->pick_state.failure;
        return stop(loop, e->operation, e->site.class_name, e->site.method, e->site.pc);
    }
    if (mc_client_entity_renderer_pick_retire(&loop->pick) != MC_CLIENT_PICK_OK)
        return stop(loop, "actual PX picking native call retirement", "px", "a", 452);
    if (!provider_tick(loop))
        return false;
    if (!atomic_load(&app->o) && app->active_world != NULL && !controller_tick(loop))
        return false;
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
    if (!health_tick(loop))
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
    bool gameplay = app->r == NULL;
    if (!gameplay) {
        mc_client_screen_state state;
        if (!screen_state(loop, &state))
            return false;
        gameplay = state.da.f;
    }
    if (gameplay) {
        const bool returned = mc_client_gameplay_input_run(app, loop->transitions, &loop->gameplay);
        loop->state.gameplay_input = &loop->gameplay;
        loop->state.mouse_events += loop->gameplay.mouse_events;
        loop->state.keyboard_events += loop->gameplay.keyboard_events;
        if (!returned) {
            const mc_client_screen_site *site = &loop->gameplay.site;
            return stop(loop, loop->gameplay.operation, site->class_name, site->method, site->pc);
        }
    }
    if (app->active_world != NULL) {
        const bool returned = mc_client_world_tick_run(app, loop->math, &loop->world_tick);
        loop->state.world_tick = &loop->world_tick;
        if (!returned) {
            const mc_client_graphics_site *site = &loop->world_tick.site;
            return stop(loop, loop->world_tick.operation, site->class_name, site->method, site->pc);
        }
        if (!mc_client_world_tick_cleanup(&loop->world_tick))
            return stop(loop, "client World tick native child cleanup",
                        "net/minecraft/client/Minecraft", "k", 1328);
    }
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
        if (mc_java_i64_sub(now, px->fields.H) > 500 &&
            mc_client_screen_transitions_pause(loop->transitions, &loop->screen) !=
                MC_CLIENT_SCREEN_OK)
            return stop(loop, loop->screen.operation, loop->screen.site.class_name,
                        loop->screen.site.method, loop->screen.site.pc);
    } else {
        if (!millis(NULL, &now))
            return stop(loop, "actual active-render clock", "px", "b", 32);
        px->fields.H = now;
    }
    if (app->N) {
        const mc_client_renderer_focus_status status = mc_client_entity_renderer_focus_begin(
            px, mc_client_display_inputs(app->display), &loop->focus, &loop->focus_error);
        if (loop->focus != NULL) {
            (void)mc_client_entity_renderer_focus_inspect(loop->focus, &loop->focus_state);
            loop->state.renderer_focus = &loop->focus_state;
        }
        if (status != MC_CLIENT_RENDERER_FOCUS_OK)
            return stop(loop, loop->focus_error.operation, loop->focus_error.site.class_name,
                        loop->focus_error.site.method, loop->focus_error.site.pc);
        if (mc_client_entity_renderer_focus_retire(&loop->focus, &loop->focus_cleanup) !=
            MC_CLIENT_RENDERER_FOCUS_OK)
            return stop(loop, loop->focus_cleanup.operation, "px", "b", 194);
        loop->state.renderer_focus = NULL;
    }
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
    if (app->active_world != NULL) {
        const int32_t target = settings.values.performance_mode == 1   ? 120
                               : settings.values.performance_mode == 2 ? 40
                                                                       : 200;
        const int64_t deadline = settings.values.performance_mode == 0
                                     ? 0
                                     : mc_java_i64_add(px->fields.I, 1000000000 / target);
        const mc_client_world_frame_status status = mc_client_entity_renderer_world_frame_begin(
            px, loop->math, partial, deadline, &loop->world_frame, &loop->world_frame_error);
        if (loop->world_frame != NULL) {
            (void)mc_client_entity_renderer_world_frame_inspect(loop->world_frame,
                                                                &loop->world_frame_state);
            loop->state.world_frame = &loop->world_frame_state;
        }
        if (status != MC_CLIENT_WORLD_FRAME_OK)
            return stop(loop, loop->world_frame_error.operation,
                        loop->world_frame_error.site.class_name,
                        loop->world_frame_error.site.method, loop->world_frame_error.site.pc);
        /* The source World draw still has an explicit missing descendant.
         * Later frame delay/HUD work is never replaced by the menu bridge. */
        return stop(loop, "actual post-World frame delay/HUD composition dependency", "px", "b",
                    383);
    }
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
        } else if (drawn.kind == MC_CLIENT_SCREEN_PAUSE) {
            if (!mc_client_source_pause_draw((mc_client_source_oz *)app->r, loop->renderer,
                                             &projection, mouse, partial, loop->math,
                                             &loop->pause_draw))
                return stop(loop, loop->pause_draw.screen.operation,
                            loop->pause_draw.screen.site.class_name,
                            loop->pause_draw.screen.site.method, loop->pause_draw.screen.site.pc);
            loop->state.font_measurements += loop->pause_draw.font_measurements;
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
    loop->bootstrap = bootstrap;
    loop->app = app;
    loop->math = mc_client_bootstrap_math(bootstrap);
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
        if (!mc_client_menu_loop_retire(&loop)) {
            loop->state.failed = true;
            loop->state.operation = "retained native menu setup cleanup";
            *out = loop;
        }
        return false;
    }
    loop->setup_complete = true;
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
    const bool paused_world = atomic_load(&app->o) && app->active_world != NULL;
    mc_timer_view previous_timer = {0};
    if (paused_world && !mc_timer_view_get(app->timer, &previous_timer))
        return run_stop(loop, "actual paused Timer partial field", 106);
    mc_timer_clock_sample clocks;
    const unsigned int timer_pc = paused_world ? 115u : 134u;
    if (!millis(NULL, &clocks.wall_millis) || !nanoseconds(loop, &clocks.nano_time, timer_pc) ||
        !mc_timer_update(app->timer, clocks))
        return run_stop(loop, "actual Timer update", timer_pc);
    if (paused_world && !mc_timer_set_partial_ticks(app->timer, previous_timer.partial_ticks))
        return run_stop(loop, "actual paused Timer partial restore", 124);
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
    mc_sound_manager_listener listener;
    const mc_sound_manager_listener *listener_argument = NULL;
    if (app->h != NULL) {
        mc_entity_snapshot player;
        if (mc_entity_get_snapshot(app->h, &player) != MC_ENTITY_OK)
            return run_stop(loop, "actual sound-listener Player fields", 242);
        listener = (mc_sound_manager_listener){player.base.aU, player.base.yaw, player.base.aJ,
                                               player.base.aK, player.base.aL,  player.base.x,
                                               player.base.y,  player.base.z};
        listener_argument = &listener;
    }
    if (mc_sound_manager_listener_update(app->B, listener_argument, timer.partial_ticks,
                                         mc_math_classes_table(loop->math), NULL,
                                         &loop->sound) != MC_SOUND_OK)
        return run_stop(loop, loop->sound.operation, 242);
    if (!mc_client_display_source_texture_enable(app->display, true, &loop->display))
        return run_stop(loop, loop->display.operation, 248);
    if (app->active_world != NULL) {
        bool more;
        if (mc_world_runtime_light_drain(app->active_world, 0, &more, &loop->world_light) !=
            MC_WORLD_STORAGE_OK)
            return run_stop(loop, loop->world_light.operation, 262);
    }
    bool f7;
    if (!key_down(loop, 65, &f7, 268) || (!f7 && !display_update(loop, 274)))
        return false;
    if (app->h != NULL) {
        bool suffocating;
        if (mc_entity_tick_suffocating(app->h, &suffocating, &loop->suffocation) != MC_ENTITY_OK)
            return run_stop(loop, loop->suffocation.operation, 288);
        if (suffocating && mc_game_settings_source_render_clear_third_person(
                               app->settings, &loop->frame_settings) != MC_GAME_SETTINGS_OK)
            return run_stop(loop, loop->frame_settings.operation, 299);
    }
    if (!app->w) {
        if (app->controller != NULL &&
            mc_client_singleplayer_controller_render_interpolation(
                app->controller, timer.partial_ticks, &loop->interpolation,
                &loop->interpolation_error) != MC_SINGLEPLAYER_RENDER_INTERPOLATION_OK)
            return stop(loop, loop->interpolation_error.operation,
                        loop->interpolation_error.site.class_name,
                        loop->interpolation_error.site.method, loop->interpolation_error.site.pc);
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
    /* Actual !l() short-circuits before the screen pause virtual. */
    bool pauses = false;
    bool remote = false;
    if (app->active_world != NULL) {
        mc_world_runtime_fields world;
        if (mc_world_runtime_fields_get(app->active_world, &world) != MC_WORLD_STORAGE_OK)
            return run_stop(loop, "actual pause World multiplayer field", 535);
        remote = world.B;
    }
    if (!remote && app->r != NULL) {
        mc_client_screen_state screen;
        if (!screen_state(loop, &screen))
            return false;
        if (screen.kind != MC_CLIENT_SCREEN_MAIN_MENU &&
            screen.kind != MC_CLIENT_SCREEN_WORLD_SELECTION &&
            screen.kind != MC_CLIENT_SCREEN_PAUSE)
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
    if (loop == NULL || !loop->setup_complete || loop->active || loop->retiring)
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

static bool retire_calls(mc_client_menu_loop *loop) {
    if (!mc_client_world_tick_cleanup(&loop->world_tick))
        return false;
    if (!mc_client_gameplay_input_retire(&loop->gameplay))
        return false;
    if (loop->controller_tick != NULL) {
        const bool failed = loop->controller_state.status != MC_SINGLEPLAYER_TICK_OK;
        if (failed) {
            loop->controller_failure_copy = *loop->controller_state.error;
            if (loop->controller_state.sound != NULL) {
                loop->controller_sound_copy = *loop->controller_state.sound;
                loop->controller_failure_copy.sound = &loop->controller_sound_copy;
            }
            if (loop->controller_state.sound_report != NULL) {
                loop->controller_sound_report_copy = *loop->controller_state.sound_report;
                loop->controller_failure_copy.sound_report = &loop->controller_sound_report_copy;
            }
        }
        if (mc_client_singleplayer_controller_tick_retire(
                &loop->controller_tick, &loop->controller_cleanup) != MC_SINGLEPLAYER_TICK_OK)
            return false;
        if (failed) {
            loop->controller_state.error = &loop->controller_failure_copy;
            loop->controller_state.sound = loop->controller_failure_copy.sound;
            loop->controller_state.sound_report = loop->controller_failure_copy.sound_report;
            loop->controller_state.controller = NULL; /* Child's occurrence ended. */
        } else {
            loop->state.controller_tick = NULL;
            loop->controller_state = (mc_singleplayer_tick_state){0};
        }
    }
    if (loop->screen_display != NULL) {
        /* The health branch reaches NEW ch82, or its preceding Stats failure,
         * before any fallback/focus owner. Preserve that exact value record if
         * later renderer/World cleanup refuses retirement. */
        if (loop->screen_display->fallback != NULL || loop->screen_display->focus_display != NULL)
            return false;
        loop->screen_display_copy = *loop->screen_display;
        if (mc_client_display_screen_retire(&loop->screen_display) != MC_CLIENT_DISPLAY_SCREEN_OK)
            return false;
        loop->state.display_call = &loop->screen_display_copy;
    }
    if (mc_client_entity_renderer_focus_retire(&loop->focus, &loop->focus_cleanup) !=
        MC_CLIENT_RENDERER_FOCUS_OK)
        return false;
    loop->state.renderer_focus = NULL; /* The child-owned error view has ended. */
    if (mc_client_entity_renderer_world_frame_retire(
            &loop->world_frame, &loop->world_frame_cleanup) != MC_CLIENT_WORLD_FRAME_OK)
        return false;
    loop->state.world_frame = NULL;
    if (mc_client_entity_renderer_pick_retire(&loop->pick) != MC_CLIENT_PICK_OK ||
        mc_client_entity_renderer_pick_detach(loop->app->t, &loop->pick_cleanup) !=
            MC_CLIENT_PICK_OK)
        return false;
    return mc_client_screen_event_retire(&loop->events) == MC_CLIENT_SCREEN_OK &&
           mc_client_source_pause_draw_retire(&loop->pause_draw) == MC_CLIENT_SCREEN_OK &&
           mc_client_source_world_selection_draw_retire(&loop->world_draw) == MC_CLIENT_SCREEN_OK;
}

bool mc_client_menu_loop_terminal_prepare(mc_client_menu_loop *loop) {
    if (loop == NULL || loop->active || (loop->retiring && !loop->state.terminal_prepare_selected))
        return false;
    loop->retiring = true;
    loop->state.terminal_prepare_selected = true;
    if (loop->state.terminal_prepared)
        return true;
    if (!retire_calls(loop) ||
        (loop->transitions != NULL && mc_client_screen_transitions_terminal_prepare(
                                          loop->transitions, &loop->screen) != MC_CLIENT_SCREEN_OK))
        return false;
    loop->state.terminal_prepared = true;
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
    if (loop->state.terminal_prepare_selected) {
        if (!mc_client_menu_loop_terminal_prepare(loop))
            return false;
    } else {
        loop->retiring = true;
        if (!retire_calls(loop))
            return false;
    }
    if (mc_client_screen_transitions_retire(&loop->transitions, &loop->screen) !=
        MC_CLIENT_SCREEN_OK)
        return false;
    mc_gui_main_menu_free(loop->renderer);
    mc_client_assets_free(loop->assets);
    free(loop);
    *slot = NULL;
    return true;
}

bool mc_client_menu_loop_terminal_retire(mc_client_menu_loop **slot) {
    if (slot == NULL)
        return false;
    mc_client_menu_loop *loop = *slot;
    if (loop == NULL)
        return true;
    if (loop->active)
        return false;
    loop->state.retirement_operation = NULL;
    loop->state.retirement_world = NULL;
    loop->state.retirement_bootstrap = MC_CLIENT_BOOTSTRAP_OK;
    if (loop->bootstrap == NULL) {
        loop->state.retirement_operation = "actual menu bootstrap association required";
        return false;
    }
    /* No frame/source entry is possible before create completes. This is the
     * original retained setup cleanup, including a partial class association;
     * it is not a fallback for a refused live storage graph. */
    if (!loop->setup_complete) {
        loop->state.retirement_operation = "retained native menu setup cleanup";
        return mc_client_menu_loop_retire(slot);
    }
    if (!loop->state.terminal_tail_entered) {
        if (!mc_client_menu_loop_terminal_prepare(loop)) {
            loop->state.retirement_operation = "menu terminal prefix remains owned";
            return false;
        }
        loop->state.retirement_bootstrap =
            mc_client_bootstrap_terminal_dispatchers(loop->bootstrap);
        if (loop->state.retirement_bootstrap != MC_CLIENT_BOOTSTRAP_OK) {
            loop->state.retirement_operation = "menu terminal dispatcher retirement";
            return false;
        }
        mc_client_terminal_storage_scope scope;
        mc_world_storage_status status = mc_client_screen_transitions_terminal_classify(
            loop->transitions, &scope, &loop->retirement_world);
        if (status == MC_WORLD_STORAGE_OK && scope == MC_CLIENT_TERMINAL_SELECTED_LOAD) {
            mc_world_terminal_inventory inventory;
            status = mc_client_screen_transitions_terminal_admit(
                loop->transitions, NULL, &inventory, &loop->retirement_world);
        }
        if (status != MC_WORLD_STORAGE_OK) {
            loop->state.retirement_world = &loop->retirement_world;
            loop->state.retirement_operation = loop->retirement_world.operation;
            return false;
        }
        /* Producer admission is closed and the exclusive, freshly admitted
         * graph enters its real destructive tail now. This records progress,
         * never an admission permit for a different or resumed source graph. */
        loop->state.terminal_tail_entered = true;
    }
    loop->state.retirement_operation = "menu terminal checked owner cleanup";
    if (mc_client_menu_loop_retire(slot))
        return true;
    if (loop->screen.operation != NULL)
        loop->state.retirement_operation = loop->screen.operation;
    return false;
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
        if (!mc_client_menu_loop_terminal_retire(&loop))
            SDL_Delay(1);
    }
    if (loop != NULL) {
        fprintf(stderr, "Native menu cleanup retains its live graph: %s\n",
                loop->state.retirement_operation == NULL ? "active native menu"
                                                         : loop->state.retirement_operation);
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
