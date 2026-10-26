#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "application_state.h"
#include "display/display.h"
#include "gui/gui_main_menu.h"
#include "resources/assets.h"
#include "startup/game_directory.h"
#include "world/session.h"
#include "world/view.h"

#include "mc/block/material/material.h"
#include "mc/block/registry/registry.h"
#include "mc/client/gui/font_renderer.h"
#include "mc/client/gui/scaled_resolution.h"
#include "mc/client/settings/game_settings.h"
#include "mc/client/timer/timer.h"
#include "mc/resources/files/files.h"
#include "mc/world/runtime/entities.h"
#include "millis.h"
#include "nanotime.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool resource_error(mc_client_assets_status status, const mc_client_assets_error *error) {
    if (status == MC_CLIENT_ASSETS_OK)
        return false;
    fprintf(stderr, "%s: %s\n", error->operation == NULL ? "Client resource" : error->operation,
            mc_client_assets_status_string(status));
    return true;
}

/* Native diagnostics retain counted text, including embedded NUL. They use
 * UTF8 output and the host line separator, without inventing a Java stack. */
static bool diagnostic_text(FILE *stream, mc_game_settings_text text) {
    if (text.is_null)
        return fputs("null", stream) >= 0;
    for (size_t i = 0; i < text.length; ++i) {
        uint32_t point = text.units[i];
        if (point >= 0xd800u && point <= 0xdbffu && i + 1u < text.length &&
            text.units[i + 1u] >= 0xdc00u && text.units[i + 1u] <= 0xdfffu) {
            point = UINT32_C(0x10000) + ((point - 0xd800u) << 10u) + (text.units[++i] - 0xdc00u);
        } else if (point >= 0xd800u && point <= 0xdfffu)
            point = '?';
        uint8_t bytes[4];
        size_t count;
        if (point <= 0x7fu) {
            bytes[0] = (uint8_t)point;
            count = 1;
        } else if (point <= 0x7ffu) {
            bytes[0] = (uint8_t)(0xc0u | (point >> 6u));
            bytes[1] = (uint8_t)(0x80u | (point & 0x3fu));
            count = 2;
        } else if (point <= 0xffffu) {
            bytes[0] = (uint8_t)(0xe0u | (point >> 12u));
            bytes[1] = (uint8_t)(0x80u | ((point >> 6u) & 0x3fu));
            bytes[2] = (uint8_t)(0x80u | (point & 0x3fu));
            count = 3;
        } else {
            bytes[0] = (uint8_t)(0xf0u | (point >> 18u));
            bytes[1] = (uint8_t)(0x80u | ((point >> 12u) & 0x3fu));
            bytes[2] = (uint8_t)(0x80u | ((point >> 6u) & 0x3fu));
            bytes[3] = (uint8_t)(0x80u | (point & 0x3fu));
            count = 4;
        }
        if (fwrite(bytes, 1, count, stream) != count)
            return false;
    }
    return true;
}

static mc_game_settings_status settings_diagnostic(void *context,
                                                   const mc_game_settings_diagnostic *event,
                                                   mc_game_settings_error *error) {
    (void)context;
    errno = 0;
    if (fputs(event->message, stdout) < 0 ||
        (event->kind == MC_GAME_SETTINGS_BAD_OPTION && !diagnostic_text(stdout, event->line)) ||
        fputc('\n', stdout) == EOF || fflush(stdout) != 0) {
        if (error != NULL)
            *error =
                (mc_game_settings_error){MC_GAME_SETTINGS_DIAGNOSTIC_FAILED,
                                         errno == 0 ? EIO : errno, "write settings diagnostic"};
        return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
    }
    return MC_GAME_SETTINGS_OK;
}

static bool initialize_settings(client_application *app) {
    mc_client_game_directory_error directory_error = {0};
    if (mc_client_game_directory_create(app->options.game_directory, NULL, &app->game_directory,
                                        NULL, &directory_error) != MC_CLIENT_GAME_DIRECTORY_OK) {
        fprintf(stderr, "%s: native error %d\n",
                directory_error.operation == NULL ? "Select game directory"
                                                  : directory_error.operation,
                directory_error.system_error);
        return false;
    }
    const mc_game_settings_diagnostics diagnostics = {NULL, settings_diagnostic};
    mc_game_settings_error error = {0};
    mc_game_settings_status status =
        mc_game_settings_create_file(app, mc_client_game_directory_path(app->game_directory), NULL,
                                     &diagnostics, &app->settings, NULL, &error);
    if (status == MC_GAME_SETTINGS_OK && app->options.gui_scale_override) {
        mc_game_settings_state state = {0};
        if (!mc_game_settings_read_state(app->settings, &state))
            return false;
        state.values.gui_scale = app->options.gui_scale;
        status = mc_game_settings_set_values(app->settings, &state.values, &error);
    }
    if (status != MC_GAME_SETTINGS_OK) {
        fprintf(stderr, "%s: status %d, native error %d\n",
                error.operation == NULL ? "Load settings" : error.operation, (int)status,
                error.system_error);
        return false;
    }
    return true;
}

static bool gui_failure(client_application *app, const mc_gui_main_menu_error *error) {
    if (error->status == MC_GUI_MAIN_MENU_DISPLAY_FAILED)
        app->display_error = error->display;
    else if (error->status == MC_GUI_MAIN_MENU_ASSETS_FAILED &&
             error->assets.status != MC_CLIENT_ASSETS_OK) {
        fprintf(stderr, "%s: ", error->operation == NULL ? "GuiMainMenu" : error->operation);
        (void)resource_error(error->assets.status, &error->assets);
    } else if (error->status == MC_GUI_MAIN_MENU_FONT_FAILED)
        fprintf(stderr, "%s: FontRenderer status %d\n",
                error->operation == NULL ? "GuiMainMenu" : error->operation, (int)error->font);
    else
        fprintf(stderr, "%s: GUI status %d\n",
                error->operation == NULL ? "GuiMainMenu" : error->operation, (int)error->status);
    return false;
}

static bool hit(const client_button *button, int32_t x, int32_t y) {
    return x >= button->x && y >= button->y && x < button->x + button->width &&
           y < button->y + button->height;
}

static bool holiday_splash(client_application *app) {
    const time_t instant = time(NULL);
    struct tm calendar = {0};
#ifdef _WIN32
    if (localtime_s(&calendar, &instant) != 0)
        return false;
#else
    if (localtime_r(&instant, &calendar) == NULL)
        return false;
#endif
    const char *message = NULL;
    if (calendar.tm_mon == 10 && calendar.tm_mday == 9)
        message = "Happy birthday, ez!";
    else if (calendar.tm_mon == 5 && calendar.tm_mday == 1)
        message = "Happy birthday, Notch!";
    else if (calendar.tm_mon == 11 && calendar.tm_mday == 24)
        message = "Merry X-mas!";
    else if (calendar.tm_mon == 0 && calendar.tm_mday == 1)
        message = "Happy new year!";
    if (message == NULL)
        return true;
    const size_t length = strlen(message);
    uint16_t *units = malloc(length * sizeof(*units));
    if (units == NULL)
        return false;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)message[i];
    mc_client_string_free(&app->splash);
    app->splash.units = units;
    app->splash.length = length;
    return true;
}

static bool layout(client_application *app, int width, int height) {
    mc_game_settings_state settings = {0};
    if (!holiday_splash(app) || !mc_game_settings_read_state(app->settings, &settings) ||
        !mc_scaled_resolution_init(width, height, settings.values.gui_scale, &app->resolution))
        return false;
    static const int32_t ids[] = {1, 2, 3, 0, 4};
    static const char *const keys[] = {"menu.singleplayer", "menu.multiplayer", "menu.mods",
                                       "menu.options", "menu.quit"};
    const int32_t middle = app->resolution.width / 2;
    const int32_t top = app->resolution.height / 4 + 48;
    for (size_t i = 0; i < 5; ++i) {
        client_button *button = &app->buttons[i];
        button->id = ids[i];
        button->x = middle - 100;
        button->y = top + (int32_t)i * 24;
        button->width = i < 3 ? 200 : 98;
        button->height = 20;
        if (i >= 3)
            button->y = top + 84;
        if (i == 4)
            button->x = middle + 2;
        mc_client_assets_error error = {0};
        if (resource_error(mc_client_assets_label(app->assets, keys[i], &button->label, &error),
                           &error))
            return false;
    }
    app->display_width = width;
    app->display_height = height;
    return true;
}

static bool title_millis(void *context, int64_t *out) {
    const client_application *app = context;
    if (app->options.fixed_time) {
        *out = app->options.time_millis;
        return true;
    }
    return mc_platform_current_time_millis(out) == MC_PLATFORM_MILLIS_OK;
}

static bool draw(client_application *app, mc_gui_mouse_position mouse) {
    mc_gui_button buttons[5];
    for (size_t i = 0; i < 5; ++i) {
        const client_button *button = &app->buttons[i];
        buttons[i] = (mc_gui_button){.id = button->id,
                                     .x = button->x,
                                     .y = button->y,
                                     .width = button->width,
                                     .height = button->height,
                                     .label = &button->label,
                                     .enabled = true,
                                     .visible = true};
    }
    mc_timer_view timer = {0};
    if (!mc_timer_view_get(app->timer, &timer))
        return false;
    const mc_font_text splash = {app->splash.units, app->splash.length};
    const mc_gui_main_menu_frame frame = {.resolution = app->resolution,
                                          .splash = &splash,
                                          .buttons = buttons,
                                          .button_count = 5,
                                          .mouse = mouse,
                                          .partial_tick = timer.partial_ticks};
    mc_gui_main_menu_error error = {0};
    return mc_gui_main_menu_draw(app->main_menu, &frame, title_millis, app, &error) ||
           gui_failure(app, &error);
}

static bool capture(client_application *app) {
    mc_client_capture pixels = {0};
    if (!mc_client_display_capture_rgba(app->display, &pixels, &app->display_error))
        return false;
    /* The desktop and the game's RGB screenshot path are opaque, independent
     * of the alpha precision selected for the native framebuffer. */
    for (size_t y = 0; y < (size_t)pixels.height; ++y)
        for (size_t x = 0; x < (size_t)pixels.width; ++x)
            pixels.pixels[y * pixels.stride + x * 4u + 3u] = 255;
    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        pixels.width, pixels.height, SDL_PIXELFORMAT_RGBA32, pixels.pixels, (int)pixels.stride);
    const bool result = surface != NULL && SDL_SavePNG(surface, app->options.capture);
    if (!result)
        fprintf(stderr, "Capture failed: %s\n", SDL_GetError());
    if (surface != NULL)
        SDL_DestroySurface(surface);
    mc_client_capture_free(&pixels);
    return result;
}

static bool mouse_position(client_application *app, float x, float y, int width, int height,
                           mc_gui_mouse_position *out) {
    int window_width = 0, window_height = 0;
    if (!SDL_GetWindowSize(mc_client_display_window(app->display), &window_width, &window_height))
        return false;
    if (window_width > 0 && window_height > 0) {
        app->input_window_width = window_width;
        app->input_window_height = window_height;
    } else {
        /* A minimized native drawable may have no extent. Keep the last
         * positive GUI/input geometry while still servicing Timer ticks. */
        window_width = app->input_window_width;
        window_height = app->input_window_height;
    }
    const int32_t pixel_x = mc_java_double_to_i32((double)x * width / window_width);
    const int32_t pixel_y = mc_java_double_to_i32((double)y * height / window_height);
    return mc_scaled_resolution_map_mouse(&app->resolution, width, height, pixel_x,
                                          mc_java_i32_sub(height - 1, pixel_y), out);
}

static bool timer_sample(client_application *app, mc_timer_clock_sample *out) {
    mc_timer_clock_sample sample = {0};
    if (mc_platform_current_time_millis(&sample.wall_millis) != MC_PLATFORM_MILLIS_OK ||
        mc_platform_nanotime_sample(&app->clock, &sample.nano_time) != MC_PLATFORM_NANOTIME_OK)
        return false;
    *out = sample;
    return true;
}

/* Keep pending mouse/keyboard records for the next Timer tick. Window close
 * and other platform events are serviced even between ticks or while there is
 * no drawable. The application owns this SDL event queue. */
static bool platform_events(client_application *app) {
    if (app->world_view != NULL) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE))
                app->running = false;
        return true;
    }

    static const struct {
        Uint32 first, last;
    } ranges[] = {{SDL_EVENT_FIRST, SDL_EVENT_KEY_DOWN - 1},
                  {SDL_EVENT_SCREEN_KEYBOARD_HIDDEN + 1, SDL_EVENT_MOUSE_MOTION - 1},
                  {SDL_EVENT_MOUSE_REMOVED + 1, SDL_EVENT_LAST}};

    for (size_t i = 0; i < sizeof(ranges) / sizeof(ranges[0]); ++i) {
        SDL_Event event;
        int count;
        while ((count = SDL_PeepEvents(&event, 1, SDL_GETEVENT, ranges[i].first, ranges[i].last)) >
               0) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                app->running = false;
        }
        if (count < 0)
            return false;
    }
    return true;
}

static bool toggle_fullscreen(client_application *app) {
    SDL_Window *window = mc_client_display_window(app->display);
    const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    int width = 0, height = 0;
    if (!SDL_SetWindowFullscreen(window, !fullscreen) ||
        !mc_client_display_pixel_size(app->display, &width, &height, &app->display_error))
        return false;
    if (width > 0 && height > 0 && !layout(app, width, height))
        return false;
    /* The source toggle also reaches Display.update after the mode switch. */
    return mc_client_display_swap(app->display, &app->display_error);
}

static bool finish_window_frame(client_application *app, bool waiting_for_drawable) {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(mc_client_display_window(app->display));
    const bool inactive = (flags & SDL_WINDOW_INPUT_FOCUS) == 0;
    if (inactive && (flags & SDL_WINDOW_FULLSCREEN) != 0 && !toggle_fullscreen(app))
        return false;
    if (inactive || waiting_for_drawable)
        SDL_Delay(10);
    return true;
}

/* GuiScreen.e() drains Mouse.next() before Keyboard.next(), once per elapsed
 * Timer tick. Unused motion/wheel/release and text events are still consumed.
 * GuiMainMenu ignores ordinary keys; its base input path handles F11. */
static bool gui_tick(client_application *app, int *width, int *height) {
    SDL_Event event;
    int count;
    while ((count = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_MOUSE_MOTION,
                                   SDL_EVENT_MOUSE_REMOVED)) > 0) {
        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN || event.button.button != SDL_BUTTON_LEFT)
            continue;
        mc_gui_mouse_position mouse = {0};
        if (!mouse_position(app, event.button.x, event.button.y, *width, *height, &mouse))
            return false;
        for (size_t i = 0; i < 5; ++i) {
            if (!hit(&app->buttons[i], mouse.x, mouse.y))
                continue;
            if (app->buttons[i].id == 4)
                app->running = false;
            else
                fprintf(stderr, "Client screen %d is not available\n", app->buttons[i].id);
        }
    }
    if (count < 0)
        return false;
    while ((count = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_KEY_DOWN,
                                   SDL_EVENT_SCREEN_KEYBOARD_HIDDEN)) > 0) {
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.key != SDLK_F11 || event.key.repeat)
            continue;
        if (!toggle_fullscreen(app))
            return false;
        *width = app->display_width;
        *height = app->display_height;
    }
    return count >= 0;
}

static bool initialize_world(client_application *app) {
    if (app->options.inspect_world == NULL)
        return true;
    app->trig = malloc(sizeof(*app->trig));
    if (app->trig == NULL || !mc_math_trig_init(app->trig) ||
        mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, app->profile, app->trig,
                                        &app->world_classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_bind_registry(app->world_classes, app->registry, NULL) !=
            MC_REGISTRY_OK ||
        mc_world_runtime_classes_bind_entities(app->world_classes, app->entities) != MC_ENTITY_OK)
        return false;
    mc_client_world_session_error session_error = {0};
    const uint16_t name[] = {'W', 'o', 'r', 'l', 'd'};
    if (!mc_client_world_session_allocate(app->world_classes, app->registry, &app->world_session,
                                          &session_error) ||
        !mc_client_world_session_open(app->world_session, app->options.inspect_world,
                                      (mc_nbt_string){name, sizeof(name) / sizeof(name[0])},
                                      app->options.world_seed, 512, &session_error)) {
        fprintf(stderr, "World construction: %s: %s (Registry %d, name %d, clock %d)\n",
                session_error.storage.operation == NULL ? "session"
                                                        : session_error.storage.operation,
                mc_world_storage_status_string(session_error.storage.status),
                (int)session_error.registry.status, (int)session_error.name_status,
                (int)session_error.clock_status);
        return false;
    }
    mc_game_settings_state settings = {0};
    mc_client_world_view_error view_error = {0};
    if (!mc_game_settings_read_state(app->settings, &settings) ||
        !mc_client_world_view_create(
            mc_client_world_session_world(app->world_session), app->display, app->assets, app->trig,
            settings.values.anaglyph, settings.values.ambient_occlusion,
            settings.values.fancy_graphics, &app->world_view, &view_error)) {
        app->display_error = view_error.display;
        fprintf(stderr, "World view: %s (mesh %d, block %d, type %d at %d,%d,%d)\n",
                view_error.operation == NULL ? "settings" : view_error.operation,
                (int)view_error.mesher.status, (int)view_error.mesher.block_id,
                (int)view_error.mesher.render_type, (int)view_error.mesher.x,
                (int)view_error.mesher.y, (int)view_error.mesher.z);
        return false;
    }
    return true;
}

static bool initialize(client_application *app) {
    mc_timer_clock_sample clocks = {0};
    if (mc_platform_nanotime_init(&app->clock) != MC_PLATFORM_NANOTIME_OK ||
        !timer_sample(app, &clocks) || !mc_timer_create(20.0f, clocks, &app->timer) ||
        mc_java_random_profile_create(1024u * 1024u, &app->profile) != MC_JAVA_RANDOM_PROFILE_OK ||
        !mc_materials_create(&app->materials) ||
        mc_entity_classes_create(MC_ENTITY_CLIENT, app->profile, NULL, &app->entities) !=
            MC_ENTITY_OK ||
        mc_client_render_classes_create(app->entities, app->profile, &app->render_classes) !=
            MC_CLIENT_MODEL_OK)
        return false;
    const size_t root_length = strlen(app->options.assets);
    if (root_length > SIZE_MAX - 5)
        return false;
    char *jar_root = malloc(root_length + 5);
    if (jar_root == NULL)
        return false;
    memcpy(jar_root, app->options.assets, root_length);
    memcpy(jar_root + root_length, "/jar", 5);
    const mc_registry_status files_status = mc_resource_files_create(jar_root, &app->files);
    free(jar_root);
    if (files_status != MC_REGISTRY_OK)
        return false;
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = app->profile,
                                         .materials = app->materials,
                                         .resources = mc_resource_files_source(app->files)};
    mc_registry_error registry_error = {0};
    mc_client_notice_error notice_error = {0};
    if (mc_registry_classes_allocate(&options, &app->registry, &registry_error) != MC_REGISTRY_OK ||
        mc_client_achievement_notice_allocate(app->render_classes, app->registry, app->profile,
                                              &app->achievement_notice,
                                              &notice_error) != MC_CLIENT_NOTICE_OK ||
        mc_client_achievement_notice_construct(app->achievement_notice, app, &notice_error) !=
            MC_CLIENT_NOTICE_OK ||
        mc_client_assets_create(options.resources, app->registry, &app->assets) !=
            MC_CLIENT_ASSETS_OK ||
        !mc_client_display_create("Minecraft", app->options.width, app->options.height,
                                  app->options.hidden, &app->display, &app->display_error))
        return false;
    if (!initialize_settings(app))
        return false;
    mc_game_settings_state settings = {0};
    if (!mc_game_settings_read_state(app->settings, &settings))
        return false;
    mc_gui_main_menu_error menu_error = {0};
    if (!mc_gui_main_menu_create(app->display, app->assets, settings.values.anaglyph,
                                 &app->main_menu, &menu_error))
        return gui_failure(app, &menu_error);
    mc_client_assets_error error = {0};
    if (mc_java_random_construct_default(app->profile, &app->title_random) !=
            MC_JAVA_RANDOM_PROFILE_OK ||
        resource_error(
            mc_client_assets_choose_splash(app->assets, &app->title_random, &app->splash, &error),
            &error))
        return false;
    int width = 0, height = 0;
    if (!mc_client_display_pixel_size(app->display, &width, &height, &app->display_error))
        return false;
    return layout(app, width > 0 ? width : app->options.width,
                  height > 0 ? height : app->options.height) &&
           initialize_world(app);
}

int mc_client_application_run(const mc_client_application_options *options) {
    client_application *app = calloc(1, sizeof(*app));
    if (app == NULL)
        return 1;
    app->options = *options;
    app->input_window_width = options->width;
    app->input_window_height = options->height;
    int result = 1;
    if (!initialize(app))
        goto cleanup;
    const mc_client_display_info *info = mc_client_display_info_get(app->display);
    fprintf(stdout, "OpenGL %d.%d core: %s / %s\n", info->gl_major, info->gl_minor, info->vendor,
            info->renderer);
    app->running = true;
    while (app->running) {
        SDL_PumpEvents();
        if (!platform_events(app))
            goto cleanup;
        if (!app->running)
            break;
        int width = 0, height = 0;
        if (!mc_client_display_pixel_size(app->display, &width, &height, &app->display_error)) {
            if (app->display_error.status != MC_CLIENT_DISPLAY_NOT_DRAWABLE)
                goto cleanup;
            width = height = 0;
        }
        if (width > 0 && height > 0 &&
            (width != app->display_width || height != app->display_height) &&
            !layout(app, width, height))
            goto cleanup;
        mc_timer_clock_sample clocks = {0};
        mc_timer_view timer = {0};
        if (!timer_sample(app, &clocks) || !mc_timer_update(app->timer, clocks) ||
            !mc_timer_view_get(app->timer, &timer))
            goto cleanup;
        int input_width = app->display_width, input_height = app->display_height;
        for (int32_t tick = 0; tick < timer.ticks && app->running; ++tick)
            if (app->world_view == NULL && !gui_tick(app, &input_width, &input_height))
                goto cleanup;
        if (!app->running)
            break;
        if (!mc_client_display_pixel_size(app->display, &width, &height, &app->display_error))
            goto cleanup;
        if (width > 0 && height > 0 &&
            (width != app->display_width || height != app->display_height) &&
            !layout(app, width, height))
            goto cleanup;
        if (width <= 0 || height <= 0) {
            if (!finish_window_frame(app, true))
                goto cleanup;
            continue;
        }
        const bool *keys = SDL_GetKeyboardState(NULL);
        if (!keys[SDL_SCANCODE_F7] && !mc_client_display_swap(app->display, &app->display_error))
            goto cleanup;
        float x = 0, y = 0;
        (void)SDL_GetMouseState(&x, &y);
        mc_gui_mouse_position mouse = {0};
        if (!mouse_position(app, x, y, width, height, &mouse))
            goto cleanup;
        if (!(app->world_view == NULL
                  ? draw(app, mouse)
                  : mc_client_world_view_draw(app->world_view, &app->display_error))) {
            if (app->display_error.status == MC_CLIENT_DISPLAY_NOT_DRAWABLE) {
                if (!finish_window_frame(app, true))
                    goto cleanup;
                continue;
            }
            goto cleanup;
        }
        if (!app->captured && options->capture != NULL) {
            if (!capture(app))
                goto cleanup;
            app->captured = true;
        }
        if (!finish_window_frame(app, false))
            goto cleanup;
        keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_F7] && !mc_client_display_swap(app->display, &app->display_error))
            goto cleanup;
        ++app->frames;
        if (options->frame_limit != 0 && app->frames >= options->frame_limit) {
            /* Diagnostic finite runs present the captured last frame before
             * terminating, independently of the ordinary F7 swap gate. */
            if (!keys[SDL_SCANCODE_F7] &&
                !mc_client_display_swap(app->display, &app->display_error))
                goto cleanup;
            app->running = false;
        }
    }
    result = 0;
cleanup:
    if (result != 0) {
        fprintf(stderr, "Client startup/frame failed: %s: %s\n",
                app->display_error.operation == NULL ? "resource or GUI operation"
                                                     : app->display_error.operation,
                app->display_error.detail);
    }
    if (mc_client_achievement_notice_free(&app->achievement_notice, NULL) != MC_CLIENT_NOTICE_OK ||
        mc_client_render_classes_free(app->render_classes) != MC_CLIENT_MODEL_OK)
        return 1;
    mc_gui_main_menu_free(app->main_menu);
    if (!mc_client_world_view_retire(&app->world_view, &app->display_error))
        result = 1;
    if (!mc_client_world_session_retire(&app->world_session, NULL) ||
        mc_world_runtime_classes_free(app->world_classes, NULL) != MC_WORLD_STORAGE_OK) {
        fprintf(stderr, "World cleanup retained a live owner\n");
        return 1;
    }
    mc_client_string_free(&app->splash);
    if (mc_game_settings_retire(&app->settings, NULL) != MC_GAME_SETTINGS_OK)
        result = 1;
    mc_client_game_directory_free(app->game_directory);
    if (!mc_client_display_destroy(app->display, &app->display_error))
        result = 1;
    mc_client_assets_free(app->assets);
    mc_registry_error free_error = {0};
    if (mc_registry_classes_free(app->registry, &free_error) != MC_REGISTRY_OK)
        return 1;
    if (mc_entity_classes_free(app->entities) != MC_ENTITY_OK)
        return 1;
    free(app->trig);
    mc_materials_destroy(app->materials);
    if (mc_resource_files_free(app->files) != MC_REGISTRY_OK)
        result = 1;
    mc_java_random_profile_free(app->profile);
    mc_timer_free(app->timer);
    free(app);
    return result;
}
