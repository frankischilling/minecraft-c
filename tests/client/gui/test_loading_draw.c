/* Compile the exact production Display TU once, as the existing font control
 * does. Interception forwards each real GL call before any authored action.
 * These are unexecuted source controls, not captured answers or fake owners. */
#include "../../../src/client/application_state.h"
#include "../../../src/client/display/display.c"
#include "../../../src/client/display/sdk_queue.h"
#include "../../../src/client/display/staged.h"
#include "../../../src/client/gui/bitmap_font_internal.h"
#include "../../../src/client/render_engine/internal.h"
#include "../../../src/client/render_engine/nw.h"
#include "../../../src/client/render_engine/texture_lookup.h"
#include "mc/resources/files/files.h"
#include <threads.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <xmmintrin.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "loading prefix check%u line%d: %s\n", checks, __LINE__, #x);          \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    client_application app;
    mc_resource_files *files;
    const mc_registry_resources *base;
    mc_registry_resources forwarding;
    void *font_input;
    mc_client_texture_pack_list *packs;
    mc_client_render_engine_classes *classes;
    mc_client_loading_classes *gui;
    mc_client_default_font *system_font;
    mc_client_render_engine *engine;
    mc_client_bitmap_font *a, *b, *captured_a;
    mc_client_loading_screen *screen;
    mc_client_display *display;
    mc_client_display_mode *display_mode;
    mc_client_sdk_icons *icons;
    mc_client_display_stage_error *stage_error;
    mc_client_sdk_update_error *update_error;
    mc_client_sdk_input_error *input_error;
    mc_client_sdk_icons_error icons_error;
    mc_client_sdk_update *updates;
    mc_client_sdk_input *input;
    mc_client_event_port *port;
    Uint32 event_type;
    unsigned pump_callbacks, yields;
    bool watch_registered;
    mc_player_string *name, *text;
    mc_client_loading_error *error, *reentry;
    mc_client_bitmap_font_error *font_error;
    const char *mode;
    bool armed, switched, callback_ok, drop_done, drop_font_done;
    unsigned draws, binds_a, binds_b, callbacks;
    int32_t id_a, id_b;
} fixture;

static fixture *current_fixture;
static mc_client_gl_bind_texture_fn real_bind;
static mc_client_gl_draw_arrays_fn real_draw;

static mc_registry_status resource_open(void *context, const char *name,
                                        mc_java_properties_input *out) {
    fixture *f = context;
    const mc_registry_status status = f->base->open(f->base->context, name, out);
    if (status == MC_REGISTRY_OK && strcmp(name, "/font.txt") == 0)
        f->font_input = out->context;
    return status;
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    return f->base->ready != NULL && f->base->ready(f->base->context, input);
}

static mc_registry_status resource_close(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    const mc_registry_status status = f->base->close(f->base->context, input);
    if (status == MC_REGISTRY_OK && f->armed && !f->switched && input->context == f->font_input &&
        (strcmp(f->mode, "font-reload") == 0 || strcmp(f->mode, "font-null-late") == 0 ||
         strcmp(f->mode, "font-retained") == 0)) {
        mc_client_loading_call_state state;
        if (!mc_client_loading_screen_call_inspect(f->screen, &state) || state.fonts[0] != f->a ||
            state.fonts[1] != f->a)
            f->callback_ok = false;
        f->app.font_renderer = strcmp(f->mode, "font-null-late") == 0 ? NULL : f->b;
        f->switched = true;
    }
    return status;
}

static void GLAPIENTRY bind_texture(GLenum target, GLuint id) {
    real_bind(target, id);
    fixture *f = current_fixture;
    if (f == NULL || !f->armed)
        return;
    if ((int32_t)id == f->id_a)
        ++f->binds_a;
    if ((int32_t)id == f->id_b)
        ++f->binds_b;
    if (f->engine == NULL || f->engine->active_resource_calls != 0)
        return;
    ++f->callbacks;
    if (mc_client_loading_screen_message(f->screen, f->text, f->reentry) !=
            MC_CLIENT_LOADING_BUSY ||
        mc_client_loading_screen_retire_call(f->screen, f->reentry) != MC_CLIENT_LOADING_BUSY)
        f->callback_ok = false;
    if (strcmp(f->mode, "fp-bind") == 0 && f->callbacks == 1)
        _mm_setcsr(_mm_getcsr() | 0x8000u);
    if (strcmp(f->mode, "font-retained") == 0 && f->switched && (int32_t)id == f->id_b &&
        !f->drop_font_done) {
        if (mc_client_bitmap_font_release(&f->a, f->font_error) != MC_CLIENT_BITMAP_FONT_OK)
            f->callback_ok = false;
        else
            f->drop_font_done = true;
    }
    if (strcmp(f->mode, "drop-engine") == 0 && !f->drop_done) {
        mc_client_render_engine *before = f->engine;
        f->app.render_engine = NULL;
        if (mc_client_render_engine_release(before) != MC_CLIENT_RENDER_ENGINE_OK)
            f->callback_ok = false;
        else {
            f->engine = NULL;
            f->drop_done = true;
        }
    }
}

static void GLAPIENTRY draw_arrays(GLenum mode, GLint first, GLsizei count) {
    real_draw(mode, first, count);
    fixture *f = current_fixture;
    if (f != NULL && f->armed) {
        ++f->draws;
        if (strcmp(f->mode, "fp-background") == 0 && f->draws == 1)
            _mm_setcsr(_mm_getcsr() | 0x8000u);
    }
}

/* This watch is reached by the real SDL_PushEvent called only AFTER the
 * exact event transport's real pump, while canonical update/port guards hold. */
static bool SDLCALL loading_event_watch(void *context, SDL_Event *event) {
    fixture *f = context;
    if (!f->armed || event->type != f->event_type)
        return true;
    ++f->pump_callbacks;
    mc_client_display *same = f->display;
    mc_client_sdk_input *input = f->input;
    if (mc_client_loading_screen_progress(f->screen, 4, f->reentry) != MC_CLIENT_LOADING_BUSY ||
        mc_client_loading_screen_retire_call(f->screen, f->reentry) != MC_CLIENT_LOADING_BUSY ||
        mc_client_sdk_display_update(f->updates, f->update_error) != MC_CLIENT_SDK_UPDATE_BUSY ||
        f->update_error->swapped || mc_client_display_stage_retire(&f->display, f->stage_error) ||
        f->display != same || f->stage_error->display.status != MC_CLIENT_DISPLAY_BUSY ||
        mc_client_sdk_input_retire(&input, f->input_error) != MC_CLIENT_SDK_INPUT_BUSY ||
        input != f->input)
        f->callback_ok = false;
    if (strcmp(f->mode, "update-fp") == 0)
        _mm_setcsr(_mm_getcsr() | 0x8000u);
    return true;
}

void loading_real_pump_then_event(void) {
    SDL_PumpEvents();
    fixture *f = current_fixture;
    if (f != NULL && f->armed) {
        SDL_Event event = {0};
        event.type = f->event_type;
        event.user.windowID = SDL_GetWindowID(mc_client_display_window(f->display));
        if (!SDL_PushEvent(&event))
            f->callback_ok = false;
    }
}

/* Forward the real void C17 operation before inspecting the exact source call
 * frame. This seam cannot manufacture an exception or a successful update. */
void loading_real_yield_then_probe(void) {
    thrd_yield();
    fixture *f = current_fixture;
    if (f != NULL && f->armed) {
        ++f->yields;
        if (!f->screen->active || !f->screen->call->error.update_completed ||
            f->screen->call->error.yield_completed ||
            mc_client_loading_screen_retire_call(f->screen, f->reentry) != MC_CLIENT_LOADING_BUSY)
            f->callback_ok = false;
    }
}

static bool string(const char *value, mc_player_string **out) {
    uint16_t units[64];
    const size_t count = strlen(value);
    if (count > sizeof(units) / sizeof(units[0]))
        return false;
    for (size_t i = 0; i < count; ++i)
        units[i] = (uint8_t)value[i];
    return mc_player_string_create(units, count, out) == MC_PLAYER_OK;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fputs("resource-root game-directory mode required\n", stderr);
        return 2;
    }
    static const char *const modes[] = {
        "normal",      "negative",       "zero",        "over",          "wrapped",
        "null-engine", "null-font",      "begin",       "fp-bind",       "fp-background",
        "font-reload", "font-null-late", "drop-engine", "font-retained", "message",
        "repeat",      "input",          "update-fp",   "no-sdk"};
    bool known = false;
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
        known = known || strcmp(argv[3], modes[i]) == 0;
    if (!known)
        return 2;
    const unsigned saved_fp = _mm_getcsr();
    if ((saved_fp & 0xffc0u) != 0x1f80u)
        return 2;
    int result = 1;
    fixture f = {0};
    bool registered = false;
    mc_client_loading_call_state pending;
    mc_client_nw_state nw;
    mc_client_tessellator *tessellator = NULL;
    mc_client_events_state port_before = {0}, port_after = {0};
    mc_client_event_record *record = NULL;
    f.mode = argv[3];
    f.callback_ok = true;
    f.error = calloc(1, sizeof(*f.error));
    f.reentry = calloc(1, sizeof(*f.reentry));
    f.font_error = calloc(1, sizeof(*f.font_error));
    f.stage_error = calloc(1, sizeof(*f.stage_error));
    f.update_error = calloc(1, sizeof(*f.update_error));
    f.input_error = calloc(1, sizeof(*f.input_error));
    CHECK(f.error != NULL && f.reentry != NULL && f.font_error != NULL && f.stage_error != NULL &&
          f.update_error != NULL && f.input_error != NULL);
    CHECK(mc_client_application_lifetime_register(&f.app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    registered = true;
    CHECK(mc_resource_files_create(argv[1], &f.files) == MC_REGISTRY_OK);
    f.base = mc_resource_files_source(f.files);
    CHECK(f.base != NULL && f.base->open != NULL && f.base->close != NULL);
    f.forwarding = (mc_registry_resources){&f, resource_open, resource_close, resource_ready};
    CHECK(mc_game_settings_create_file(&f.app, argv[2], NULL, NULL, &f.app.settings, NULL, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_client_texture_pack_list_allocate(&f.forwarding, NULL, &f.packs) ==
          MC_TEXTURE_PACK_OK);
    CHECK(mc_client_texture_pack_list_construct(f.packs, &f.app, argv[2], NULL) ==
          MC_TEXTURE_PACK_OK);
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &f.classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &f.system_font) == MC_CLIENT_FONT_OK);
    CHECK(mc_client_render_engine_allocate(f.classes, f.system_font, NULL, &f.engine) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(f.engine, f.packs, f.app.settings, NULL) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    if (strcmp(f.mode, "no-sdk") == 0) {
        CHECK(mc_client_display_create("real Display without SDK profile", 320, 240, true,
                                       &f.display, NULL));
    } else {
        CHECK(mc_client_sdk_icons_allocate(f.base, &f.icons, &f.icons_error) ==
              MC_CLIENT_SDK_ICONS_OK);
        CHECK(mc_client_sdk_icons_construct(f.icons, &f.icons_error) == MC_CLIENT_SDK_ICONS_OK);
        CHECK(mc_client_display_stage_allocate(&f.display, f.stage_error));
        CHECK(mc_client_display_stage_bind_icons(f.display, f.icons, f.stage_error));
        CHECK(mc_client_display_mode_allocate(&f.display_mode, f.stage_error));
        CHECK(mc_client_display_mode_construct(f.display_mode, 320, 240, f.stage_error));
        CHECK(mc_client_display_stage_set_mode(f.display, f.display_mode, f.stage_error));
        CHECK(mc_client_display_stage_set_title(f.display, NULL, f.stage_error));
        CHECK(mc_client_display_create_staged(f.display, true, f.stage_error));
        f.updates = mc_client_display_update_owner(f.display);
        f.input = mc_client_display_inputs(f.display);
        f.port = mc_client_display_event_port(f.display);
        CHECK(f.updates != NULL && f.input != NULL && f.port != NULL);
        f.event_type = SDL_RegisterEvents(1);
        CHECK(f.event_type != (Uint32)-1);
        CHECK(SDL_AddEventWatch(loading_event_watch, &f));
        f.watch_registered = true;
    }
    CHECK(mc_client_loading_classes_create(f.classes, &f.gui) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_classes_bind_resources(f.gui, &f.forwarding, f.error) ==
          MC_CLIENT_LOADING_OK);
    CHECK(string("/font/white.png", &f.name) && string("A B", &f.text));
    /* Font generation leases really retain the Engine. The last-Engine case
     * deliberately has no Font owner, so its subsequent actual measure480
     * fails on NULL after the background and p-rebinding effects. */
    if (strcmp(f.mode, "null-engine") != 0 && strcmp(f.mode, "null-font") != 0 &&
        strcmp(f.mode, "drop-engine") != 0) {
        CHECK(mc_client_bitmap_font_allocate(f.gui, NULL, &f.a) == MC_CLIENT_BITMAP_FONT_OK);
        CHECK(mc_client_bitmap_font_construct(f.a, f.app.settings, f.name, f.engine, f.display,
                                              f.font_error) == MC_CLIENT_BITMAP_FONT_OK);
        f.id_a = f.a->fields.a;
        f.captured_a = f.a;
        CHECK(mc_client_bitmap_font_allocate(f.gui, NULL, &f.b) == MC_CLIENT_BITMAP_FONT_OK);
        CHECK(mc_client_bitmap_font_construct(f.b, f.app.settings, f.name, f.engine, f.display,
                                              f.font_error) == MC_CLIENT_BITMAP_FONT_OK);
        f.id_b = f.b->fields.a;
        CHECK(f.id_a != f.id_b);
    }
    f.app.display = f.display;
    f.app.render_engine = strcmp(f.mode, "null-engine") == 0 ? NULL : f.engine;
    f.app.font_renderer = f.a;
    f.app.D = f.packs;
    f.app.d = 320;
    f.app.e = 240;
    f.app.display_width = 7;
    f.app.display_height = 11;
    atomic_store_explicit(&f.app.J, true, memory_order_seq_cst);
    CHECK(mc_client_loading_screen_allocate(f.gui, &f.screen) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_construct(f.screen, &f.app, f.error) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_title(f.screen, f.text, f.error) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_display_source_texture_enable(f.display, true, NULL));
    CHECK(mc_client_nw_active_use(f.classes,
                                  (mc_client_graphics_site){"native-control", "prepare", "()V", -1},
                                  &tessellator, &f.error->graphics));
    if (strcmp(f.mode, "begin") == 0)
        CHECK(mc_client_nw_begin_quads(tessellator, &f.error->graphics));
    real_bind = f.display->gl.BindTexture;
    real_draw = f.display->gl.DrawArrays;
    current_fixture = &f;
    f.display->gl.BindTexture = bind_texture;
    f.display->gl.DrawArrays = draw_arrays;
    if (f.port != NULL)
        CHECK(mc_client_events_inspect(f.port, &port_before));
    if (strcmp(f.mode, "input") == 0) {
        SDL_Event event = {0};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = SDL_GetWindowID(mc_client_display_window(f.display));
        event.key.scancode = SDL_SCANCODE_A;
        event.key.key = SDLK_A;
        event.key.down = true;
        CHECK(SDL_PushEvent(&event));
        event = (SDL_Event){0};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.windowID = SDL_GetWindowID(mc_client_display_window(f.display));
        event.button.button = SDL_BUTTON_LEFT;
        event.button.down = true;
        event.button.x = 10.0f;
        event.button.y = 15.0f;
        CHECK(SDL_PushEvent(&event));
    }
    f.armed = true;
    /* Set only the genuine saved timestamp as an explicit source input, so
     * the real progress clock test is reached for every supplied value. */
    int32_t progress = strcmp(f.mode, "negative") == 0  ? -1
                       : strcmp(f.mode, "zero") == 0    ? 0
                       : strcmp(f.mode, "over") == 0    ? 125
                       : strcmp(f.mode, "wrapped") == 0 ? INT32_MAX
                                                        : 25;
    f.screen->fields.d = 0;
    const bool message = strcmp(f.mode, "message") == 0;
    const mc_client_loading_status status =
        message ? mc_client_loading_screen_message(f.screen, f.text, f.error)
                : mc_client_loading_screen_progress(f.screen, progress, f.error);
    f.armed = false;
    _mm_setcsr(saved_fp); /* Restore only authored test mutation on every path. */
    CHECK(f.callback_ok && f.error->sampled_clock_committed);
    CHECK(!f.screen->active);
    if (message)
        CHECK(f.screen->fields.d == 0 && f.screen->last_store_pc == 43);
    else
        CHECK(f.screen->fields.d != 0 && f.screen->last_store_pc == 46);
    CHECK(f.app.d == 320 && f.app.e == 240 && f.app.display_width == 7 &&
          f.app.display_height == 11);
    if (strcmp(f.mode, "null-engine") == 0) {
        mc_player_string_view literal;
        CHECK(status == MC_CLIENT_LOADING_NULL_REFERENCE && f.error->site.pc == 165 &&
              f.error->lookup == NULL);
        CHECK(f.gui->background_literal != NULL &&
              mc_player_string_read(f.gui->background_literal, &literal) && literal.length == 19);
    } else if (strcmp(f.mode, "null-font") == 0 || strcmp(f.mode, "font-null-late") == 0 ||
               strcmp(f.mode, "drop-engine") == 0) {
        CHECK(status == MC_CLIENT_LOADING_NULL_REFERENCE &&
              f.error->site.pc == (strcmp(f.mode, "font-null-late") == 0 ? 524 : 480));
        CHECK(strcmp(f.mode, "font-null-late") != 0 ||
              (f.switched && f.binds_a >= 2 && f.binds_b == 0));
    } else if (strcmp(f.mode, "begin") == 0) {
        CHECK(status == MC_CLIENT_LOADING_GRAPHICS_FAILED && f.error->site.pc == 184 &&
              f.error->graphics.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE && f.draws == 0);
    } else if (strcmp(f.mode, "fp-bind") == 0 || strcmp(f.mode, "fp-background") == 0) {
        CHECK(status == MC_CLIENT_LOADING_UNSUPPORTED_FP &&
              f.error->site.pc == (strcmp(f.mode, "fp-bind") == 0 ? 199 : 324));
        CHECK(f.draws == (strcmp(f.mode, "fp-bind") == 0 ? 0u : 1u));
    } else if (strcmp(f.mode, "update-fp") == 0 || strcmp(f.mode, "no-sdk") == 0) {
        CHECK(f.error->site.pc == 544 && f.error->draw_prefix_completed &&
              !f.error->update_completed && !f.error->yield_completed &&
              !f.error->source_returned && f.yields == 0 && f.error->update != NULL);
        if (strcmp(f.mode, "update-fp") == 0) {
            CHECK(status == MC_CLIENT_LOADING_UNSUPPORTED_FP && f.error->messages_pumped &&
                  f.error->update->status == MC_CLIENT_SDK_UPDATE_INPUT_FAILED &&
                  f.error->update->input.status == MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP &&
                  f.pump_callbacks == 1);
        } else
            CHECK(status == MC_CLIENT_LOADING_DISPLAY_FAILED &&
                  f.error->update->status == MC_CLIENT_SDK_UPDATE_INVALID_ARGUMENT &&
                  !f.error->swapped && !f.error->messages_pumped);
    } else {
        CHECK((status == MC_CLIENT_LOADING_OK ||
               (strcmp(f.mode, "font-retained") == 0 && status == MC_CLIENT_LOADING_BUSY)) &&
              f.error->draw_prefix_completed && f.error->update_completed &&
              f.error->yield_completed && f.error->source_returned && f.error->update == NULL &&
              f.yields == 1 && f.pump_callbacks == 1 && f.error->messages_pumped);
        CHECK(f.draws != 0);
        CHECK(mc_client_events_inspect(f.port, &port_after) &&
              port_after.pump_generation == port_before.pump_generation + 1 &&
              mc_client_display_event_port(f.display) == f.port);
        if (strcmp(f.mode, "font-reload") == 0)
            CHECK(f.switched && f.binds_a >= 2 && f.binds_b >= 2);
    }
    /* Earlier failures cannot credit update, pump, yield or a source return. */
    if (!f.error->draw_prefix_completed)
        CHECK(!f.error->update_completed && !f.error->swapped && !f.error->messages_pumped &&
              !f.error->yield_completed && !f.error->source_returned && f.yields == 0 &&
              f.pump_callbacks == 0);
    if (strcmp(f.mode, "input") == 0) {
        bool next = false, key_seen = false, mouse_seen = false;
        mc_client_sdk_key_event key = {0};
        mc_client_sdk_mouse_event mouse = {0};
        do {
            CHECK(mc_client_sdk_keyboard_next(f.input, &next, &key, f.input_error) ==
                  MC_CLIENT_SDK_INPUT_OK);
            key_seen = key_seen || (next && key.key == 30 && key.pressed);
        } while (next);
        do {
            CHECK(mc_client_sdk_mouse_next(f.input, &next, &mouse, f.input_error) ==
                  MC_CLIENT_SDK_INPUT_OK);
            mouse_seen = mouse_seen || (next && mouse.button == 0 && mouse.pressed);
        } while (next);
        CHECK(key_seen && mouse_seen); /* SDK buffers, not earlier loop actions. */
    }
    if (f.pump_callbacks != 0) {
        CHECK(mc_client_events_take(f.port, f.event_type, f.event_type, false, &record, NULL) ==
                  MC_CLIENT_EVENTS_OK &&
              record != NULL);
        CHECK(mc_client_event_read(record)->type == f.event_type);
        mc_client_event_release(&record); /* Unrelated user record survived update. */
    }
    if (strcmp(f.mode, "repeat") == 0) {
        CHECK(f.error->native_finalized);
        CHECK(mc_client_loading_screen_call_inspect(f.screen, &pending) && !pending.retained);
        f.armed = true;
        f.screen->fields.d = 0;
        CHECK(mc_client_loading_screen_progress(f.screen, 26, f.error) == MC_CLIENT_LOADING_OK);
        f.armed = false;
        CHECK(f.error->source_returned && f.error->native_finalized && f.yields == 2 &&
              f.pump_callbacks == 2);
        CHECK(mc_client_events_take(f.port, f.event_type, f.event_type, false, &record, NULL) ==
                  MC_CLIENT_EVENTS_OK &&
              record != NULL);
        mc_client_event_release(&record);
    }
    if (strcmp(f.mode, "drop-engine") == 0) {
        CHECK(f.drop_done && f.engine == NULL && f.app.render_engine == NULL);
        CHECK(mc_client_loading_screen_call_inspect(f.screen, &pending) && pending.retained &&
              pending.engine != NULL);
        CHECK(mc_client_loading_screen_retire_call(f.screen, f.error) == MC_CLIENT_LOADING_BUSY);
        mc_client_render_engine *captured = (mc_client_render_engine *)pending.engine;
        CHECK(mc_client_render_engine_retire_lookup(captured, NULL) == MC_CLIENT_LOOKUP_OK);
        CHECK(mc_client_texture_lookup_free_metadata(captured));
        CHECK(mc_client_loading_screen_retire_call(f.screen, f.error) == MC_CLIENT_LOADING_OK);
    }
    if (strcmp(f.mode, "font-retained") == 0) {
        CHECK(f.drop_font_done && f.a == NULL && f.switched && f.binds_b >= 2);
        CHECK(mc_client_loading_screen_call_inspect(f.screen, &pending) && pending.retained &&
              pending.fonts[1] == f.captured_a && pending.engine == f.engine);
    }
    CHECK(mc_client_nw_inspect(f.classes, &nw));
    if (nw.fields.q)
        CHECK(mc_client_nw_draw(tessellator, f.display, &f.error->graphics));
    CHECK(mc_client_loading_screen_retire_call(f.screen, f.error) == MC_CLIENT_LOADING_OK);
    result = 0;

cleanup:
    _mm_setcsr(saved_fp);
    f.armed = false;
    current_fixture = NULL;
    if (f.display != NULL && real_bind != NULL) {
        f.display->gl.BindTexture = real_bind;
        f.display->gl.DrawArrays = real_draw;
    }
    if (f.watch_registered) {
        SDL_RemoveEventWatch(loading_event_watch, &f);
        f.watch_registered = false;
    }
    mc_client_event_release(&record);
#define CLEANUP_OK(x)                                                                              \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "loading prefix cleanup line%d: %s\n", __LINE__, #x);                  \
            result = 1;                                                                            \
        }                                                                                          \
    } while (0)
    if (f.screen != NULL) {
        const mc_client_loading_status retired = mc_client_loading_screen_release(f.screen);
        if (retired == MC_CLIENT_LOADING_OK)
            f.screen = NULL;
        else
            CLEANUP_OK(retired == MC_CLIENT_LOADING_OK);
    }
    /* Do not free a prepared App or its dependencies under an actual retained
     * mk on any failing control path. Such a path is already a failing child. */
    if (f.screen != NULL)
        return 1;
    f.app.font_renderer = NULL;
    f.app.render_engine = NULL;
    if (f.a != NULL)
        CLEANUP_OK(mc_client_bitmap_font_release(&f.a, f.font_error) == MC_CLIENT_BITMAP_FONT_OK);
    if (f.b != NULL)
        CLEANUP_OK(mc_client_bitmap_font_release(&f.b, f.font_error) == MC_CLIENT_BITMAP_FONT_OK);
    if (f.a != NULL || f.b != NULL)
        return 1;
    if (f.engine != NULL) {
        CLEANUP_OK(mc_client_render_engine_retire_lookup(f.engine, NULL) == MC_CLIENT_LOOKUP_OK);
        CLEANUP_OK(mc_client_texture_lookup_free_metadata(f.engine));
        const mc_client_render_engine_status retired = mc_client_render_engine_release(f.engine);
        if (retired == MC_CLIENT_RENDER_ENGINE_OK)
            f.engine = NULL;
        else
            CLEANUP_OK(retired == MC_CLIENT_RENDER_ENGINE_OK);
    }
    if (f.engine != NULL)
        return 1;
    if (f.classes != NULL)
        CLEANUP_OK(mc_client_render_engine_retire_resources(f.classes, NULL) ==
                   MC_CLIENT_RENDER_TEXTURE_OK);
    if (f.gui != NULL)
        CLEANUP_OK(mc_client_loading_classes_release(f.gui) == MC_CLIENT_LOADING_OK);
    if (f.classes != NULL)
        CLEANUP_OK(mc_client_render_engine_classes_release(f.classes) ==
                   MC_CLIENT_RENDER_ENGINE_OK);
    if (f.system_font != NULL)
        CLEANUP_OK(mc_client_default_font_release(f.system_font) == MC_CLIENT_FONT_OK);
    if (f.display != NULL) {
        if (strcmp(f.mode, "no-sdk") == 0) {
            const bool destroyed = mc_client_display_destroy(f.display, NULL);
            CLEANUP_OK(destroyed);
            if (destroyed)
                f.display = NULL;
        } else
            CLEANUP_OK(mc_client_display_stage_retire(&f.display, f.stage_error));
    }
    if (f.display != NULL && strcmp(f.mode, "no-sdk") != 0)
        return 1;
    mc_client_display_mode_release(&f.display_mode);
    f.app.display = f.display; /* End the prepared fixture view only after real cleanup. */
    if (f.icons != NULL)
        CLEANUP_OK(mc_client_sdk_icons_release(&f.icons, &f.icons_error) == MC_CLIENT_SDK_ICONS_OK);
    if (f.packs != NULL)
        CLEANUP_OK(mc_client_texture_pack_list_release(f.packs, NULL) == MC_TEXTURE_PACK_OK);
    if (f.app.settings != NULL)
        CLEANUP_OK(mc_game_settings_retire(&f.app.settings, NULL) == MC_GAME_SETTINGS_OK);
    if (registered && f.screen == NULL)
        CLEANUP_OK(mc_client_application_lifetime_retire(&f.app, NULL) ==
                   MC_CLIENT_APPLICATION_OWNER_OK);
    if (f.files != NULL)
        CLEANUP_OK(mc_resource_files_free(f.files) == MC_REGISTRY_OK);
    mc_player_string_release(f.name);
    mc_player_string_release(f.text);
    free(f.input_error);
    free(f.update_error);
    free(f.stage_error);
    free(f.font_error);
    free(f.reentry);
    free(f.error);
    if (result == 0)
        printf("loading prefix %s: %u checks passed\n", argv[3], checks);
    return result;
}
