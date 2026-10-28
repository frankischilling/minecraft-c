#include "client/display/sdk_display_internal.h"
#include "client/display/staged.h"
#include "client/input/mouse_helper.h"
#include "mc/resources/files/files.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool cpu_prefixes(void) {
    bool ok = false;
    mc_client_render_engine_classes *classes = NULL;
    mc_client_source_vy *owner = NULL, *alias = NULL;
    mc_client_mouse_helper_error error = {0};
    mc_client_mouse_helper_state state = {0}, unchanged = {0};
    mc_client_render_engine_class_state class_state = {0};
    mc_client_sdk_input_error input_error = {0};
    CHECK(mc_client_sdk_keyboard_create(NULL, &input_error) ==
          MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT);
    CHECK(mc_client_sdk_mouse_create(NULL, &input_error) == MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT);
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_inspect(classes, &class_state) &&
          class_state.ge_phase == MC_CLIENT_RENDER_ENGINE_RAW);
    for (size_t bound = 1; bound <= 8; ++bound) {
        mc_client_mouse_helper_options options = mc_client_mouse_helper_default_options();
        options.max_allocations = bound;
        CHECK(mc_client_mouse_helper_allocate(classes, &options, &owner, &error) ==
              MC_CLIENT_MOUSE_HELPER_OK);
        CHECK(mc_client_mouse_helper_inspect(owner, &state) &&
              state.phase == MC_CLIENT_MOUSE_HELPER_RAW && !state.source_c_stored &&
              !state.source_e_stored && !state.source_d_stored);
        const mc_client_mouse_helper_status status =
            mc_client_mouse_helper_construct(owner, NULL, NULL, NULL, &error);
        CHECK(status == MC_CLIENT_MOUSE_HELPER_LIMIT ||
              status == MC_CLIENT_MOUSE_HELPER_CURSOR_FAILED);
        CHECK(mc_client_mouse_helper_inspect(owner, &state) &&
              state.phase == MC_CLIENT_MOUSE_HELPER_FAILED && state.source_c_stored &&
              state.source_e_stored && state.source_c == NULL && state.source_e == 10 &&
              state.source_a == 0 && state.source_b == 0 && state.last_store_pc == 12 &&
              !state.source_d_stored && state.source_d == NULL);
        CHECK(mc_client_render_engine_classes_inspect(classes, &class_state) &&
              class_state.ge_phase == MC_CLIENT_RENDER_ENGINE_READY);
        if (state.delay_buffer != NULL) {
            int32_t delay = -1;
            mc_client_graphics_error graphics = {0};
            CHECK(mc_client_ge_int_remaining(state.delay_buffer) == 1 &&
                  mc_client_ge_int_get(
                      state.delay_buffer, 0, &delay,
                      (mc_client_graphics_site){"vy", "<init>", "(Ljava/awt/Component;)V", 22},
                      &graphics) &&
                  delay == 0);
        }
        if (state.image_buffer != NULL) {
            CHECK(state.pending_pc == 39 && mc_client_ge_int_remaining(state.image_buffer) == 1024);
            for (int32_t index = 0; index < 1024; ++index) {
                int32_t pixel = -1;
                mc_client_graphics_error graphics = {0};
                CHECK(mc_client_ge_int_get(
                          state.image_buffer, index, &pixel,
                          (mc_client_graphics_site){"vy", "<init>", "(Ljava/awt/Component;)V", 34},
                          &graphics) &&
                      pixel == 0);
            }
        }
        CHECK(mc_client_mouse_helper_construct(owner, NULL, NULL, NULL, &error) ==
              MC_CLIENT_MOUSE_HELPER_INVALID_PHASE);
        CHECK(mc_client_mouse_helper_inspect(owner, &unchanged) &&
              unchanged.delay_buffer == state.delay_buffer &&
              unchanged.image_buffer == state.image_buffer &&
              unchanged.native_bytes == state.native_bytes &&
              unchanged.last_store_pc == state.last_store_pc);
        CHECK(mc_client_mouse_helper_retain(owner, &error) == MC_CLIENT_MOUSE_HELPER_OK);
        alias = owner;
        CHECK(mc_client_mouse_helper_release(&owner, &error) == MC_CLIENT_MOUSE_HELPER_OK &&
              owner == NULL);
        CHECK(mc_client_mouse_helper_inspect(alias, &unchanged) && unchanged.holds == 1);
        CHECK(mc_client_mouse_helper_release(&alias, &error) == MC_CLIENT_MOUSE_HELPER_OK &&
              alias == NULL);
    }
    ok = true;
cleanup:
    if (owner != NULL &&
        mc_client_mouse_helper_release(&owner, &error) != MC_CLIENT_MOUSE_HELPER_OK)
        ok = false;
    if (alias != NULL &&
        mc_client_mouse_helper_release(&alias, &error) != MC_CLIENT_MOUSE_HELPER_OK)
        ok = false;
    if (classes != NULL &&
        mc_client_render_engine_classes_release(classes) != MC_CLIENT_RENDER_ENGINE_OK)
        ok = false;
    return ok;
}

static bool native_window(const char *root) {
    bool ok = false, selected = false;
    mc_resource_files *files = NULL;
    mc_client_sdk_icons *icons = NULL;
    mc_client_sdk_icons_error icons_error = {0};
    mc_client_display *display = NULL;
    mc_client_display_mode *mode = NULL;
    mc_client_display_stage_error display_error = {0};
    mc_client_render_engine_classes *classes = NULL;
    mc_client_source_vy *owner = NULL;
    mc_client_mouse_helper_error error = {0};
    mc_client_mouse_helper_state state = {0};
    mc_client_sdk_cursor *cursor = NULL;
    mc_client_sdk_cursor_state cursor_state = {0};
    mc_client_sdk_cursor_error cursor_error = {0};
    SDL_Cursor *restore = NULL, *owned_restore = NULL;
    CHECK(mc_resource_files_create(root, &files) == MC_REGISTRY_OK);
    CHECK(mc_client_sdk_icons_allocate(mc_resource_files_source(files), &icons, &icons_error) ==
          MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_client_sdk_icons_construct(icons, &icons_error) == MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_client_display_stage_allocate(&display, &display_error));
    CHECK(mc_client_display_stage_bind_icons(display, icons, &display_error));
    CHECK(mc_client_display_mode_allocate(&mode, &display_error));
    CHECK(mc_client_display_mode_construct(mode, 160, 120, &display_error));
    CHECK(mc_client_display_stage_set_mode(display, mode, &display_error));
    CHECK(mc_client_display_stage_set_title(display, NULL, &display_error));
    CHECK(mc_client_display_create_staged(display, true, &display_error));
    mc_client_sdk_input *input = mc_client_display_inputs(display);
    mc_client_sdk_input_state before = {0}, after = {0};
    mc_client_sdk_input_error input_error = {0};
    CHECK(mc_client_sdk_input_inspect(input, &before) && before.mouse_created &&
          before.keyboard_created);
    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(mc_client_display_window(display));
    event.key.scancode = SDL_SCANCODE_A;
    event.key.key = SDLK_A;
    CHECK(SDL_PushEvent(&event));
    CHECK(mc_client_sdk_keyboard_create(input, &input_error) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(mc_client_sdk_mouse_create(input, &input_error) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(SDL_HasEvent(SDL_EVENT_KEY_DOWN)); /* No implicit pump/consume. */
    CHECK(mc_client_sdk_input_inspect(input, &after) && before.buttons == after.buttons &&
          before.key_down == after.key_down && before.mouse_events == after.mouse_events &&
          before.keyboard_events == after.keyboard_events &&
          before.mouse_read == after.mouse_read && before.keyboard_read == after.keyboard_read &&
          before.last_mouse_store_pc == after.last_mouse_store_pc &&
          before.last_keyboard_store_pc == after.last_keyboard_store_pc &&
          before.grabbed == after.grabbed);
    restore = SDL_GetCursor();
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    bool saw_failure = false, saw_native_prefix = false, saw_completion = false;
    for (size_t bound = 0; bound < 16; ++bound) {
        mc_client_mouse_helper_options limits = mc_client_mouse_helper_default_options();
        limits.cursor.max_allocations = bound;
        CHECK(mc_client_mouse_helper_allocate(classes, &limits, &owner, &error) ==
              MC_CLIENT_MOUSE_HELPER_OK);
        const mc_client_mouse_helper_status status =
            mc_client_mouse_helper_construct(owner, NULL, display, input, &error);
        CHECK(mc_client_mouse_helper_inspect(owner, &state));
        if (status == MC_CLIENT_MOUSE_HELPER_OK) {
            CHECK(state.source_d_stored && state.source_d != NULL);
            saw_completion = true;
        } else {
            CHECK(status == MC_CLIENT_MOUSE_HELPER_LIMIT && !state.source_d_stored &&
                  state.last_store_pc == 12 && state.source_c_stored && state.source_e == 10);
            saw_failure = true;
            if (state.cursor_candidate != NULL) {
                CHECK(mc_client_sdk_cursor_inspect(state.cursor_candidate, &cursor_state) &&
                      cursor_state.source_index == 0);
                if (cursor_state.native_created) {
                    CHECK(!cursor_state.source_cursors_stored &&
                          cursor_state.native_handle != NULL);
                    saw_native_prefix = true;
                }
            }
        }
        CHECK(mc_client_mouse_helper_release(&owner, &error) == MC_CLIENT_MOUSE_HELPER_OK &&
              owner == NULL && SDL_GetCursor() == restore);
        if (saw_completion)
            break;
    }
    CHECK(saw_failure && saw_native_prefix && saw_completion);
    CHECK(mc_client_mouse_helper_allocate(classes, NULL, &owner, &error) ==
          MC_CLIENT_MOUSE_HELPER_OK);
    CHECK(mc_client_mouse_helper_construct(owner, NULL, display, input, &error) ==
          MC_CLIENT_MOUSE_HELPER_OK);
    CHECK(mc_client_mouse_helper_inspect(owner, &state) &&
          state.phase == MC_CLIENT_MOUSE_HELPER_READY && state.source_d_stored &&
          state.source_d != NULL && state.pending_pc == 70 && state.last_store_pc == 57 &&
          state.source_e == 10 && state.source_c == NULL);
    mc_client_sdk_cursor *borrowed_cursor = state.source_d;
    CHECK(mc_client_sdk_cursor_retain(borrowed_cursor, &cursor_error) == MC_CLIENT_SDK_CURSOR_OK);
    cursor = borrowed_cursor;
    CHECK(mc_client_sdk_cursor_inspect(cursor, &cursor_state) && cursor_state.source_index == 0 &&
          cursor_state.source_cursors_stored && cursor_state.native_created &&
          cursor_state.native_handle != NULL && !cursor_state.source_destroyed &&
          cursor_state.native_hotspot_x == 16 && cursor_state.native_hotspot_y == 15 &&
          cursor_state.one_bit_supported && SDL_GetCursor() == restore);
    if (cursor_state.platform == 3) {
        CHECK(cursor_state.element_delay == 0 && cursor_state.flipped_position == 1024);
    } else {
        CHECK(cursor_state.platform == 1 && cursor_state.element_delay == -1 &&
              cursor_state.element_timeout == -1 && cursor_state.flipped_position == 0);
    }
    CHECK(mc_client_mouse_helper_release(&owner, &error) == MC_CLIENT_MOUSE_HELPER_OK &&
          owner == NULL);
    CHECK(mc_client_sdk_cursor_inspect(cursor, &cursor_state) && cursor_state.holds == 1);
    mc_client_display *same_display = display;
    CHECK(!mc_client_display_stage_retire(&display, &display_error) && display == same_display &&
          display_error.display.status == MC_CLIENT_DISPLAY_BUSY);
    if (restore == NULL) {
        owned_restore = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        CHECK(owned_restore != NULL);
        restore = owned_restore;
    }
    CHECK(SDL_SetCursor((SDL_Cursor *)cursor_state.native_handle));
    selected = true;
    mc_client_sdk_cursor *same_cursor = cursor;
    CHECK(mc_client_sdk_cursor_release(&cursor, &cursor_error) == MC_CLIENT_SDK_CURSOR_BUSY &&
          cursor == same_cursor);
    CHECK(SDL_SetCursor(restore));
    selected = false;
    CHECK(mc_client_sdk_cursor_release(&cursor, &cursor_error) == MC_CLIENT_SDK_CURSOR_OK &&
          cursor == NULL);
    ok = true;
cleanup:
    if (selected && restore != NULL && !SDL_SetCursor(restore))
        ok = false;
    if (cursor != NULL &&
        mc_client_sdk_cursor_release(&cursor, &cursor_error) != MC_CLIENT_SDK_CURSOR_OK)
        ok = false;
    if (owner != NULL &&
        mc_client_mouse_helper_release(&owner, &error) != MC_CLIENT_MOUSE_HELPER_OK)
        ok = false;
    if (owned_restore != NULL)
        SDL_DestroyCursor(owned_restore);
    if (classes != NULL &&
        mc_client_render_engine_classes_release(classes) != MC_CLIENT_RENDER_ENGINE_OK)
        ok = false;
    if (!mc_client_display_stage_retire(&display, &display_error))
        ok = false;
    mc_client_display_mode_release(&mode);
    if (mc_client_sdk_icons_release(&icons, &icons_error) != MC_CLIENT_SDK_ICONS_OK)
        ok = false;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    bool ok = false;
    if (argc == 1)
        ok = cpu_prefixes();
    else if (argc == 3 && strcmp(argv[1], "--window") == 0)
        ok = native_window(argv[2]);
    else
        fprintf(stderr, "usage: mouse-helper-control [--window imported-resource-root]\n");
    SDL_Quit(); /* Test-process cleanup, never a source startup operation. */
    if (!ok)
        return 1;
    printf("mouse helper controls: %u checks\n", checks);
    return 0;
}
