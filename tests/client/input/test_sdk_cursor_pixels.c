/* Qualification-only translation unit: inspect the actual cursor's owned
 * pixel transport while executing its real constructor and SDL native child.
 * The archive copy of sdk_cursor.c must not be extracted into this target. */
#include "client/display/sdk_cursor.c"
#include "client/display/staged.h"
#include "client/render_engine/ge.h"
#include "mc/resources/files/files.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t pattern(uint32_t x, uint32_t y) {
    const uint32_t alpha = (x + y) % 3u == 0u ? 255u : (x + y) % 3u == 1u ? 128u : 0u;
    return (alpha << 24) | ((x * 7u + y) << 16) | ((y * 5u + x) << 8) | (x * 3u + y * 2u);
}

int main(int argc, char **argv) {
    bool ok = false;
    mc_resource_files *files = NULL;
    mc_client_sdk_icons *icons = NULL;
    mc_client_sdk_icons_error icons_error = {0};
    mc_client_display *display = NULL;
    mc_client_display_mode *mode = NULL;
    mc_client_display_stage_error display_error = {0};
    mc_client_render_engine_classes *classes = NULL;
    mc_client_render_arena input_arena = {0};
    mc_client_direct_int_buffer *images = NULL, *delays = NULL;
    mc_client_graphics_error graphics = {0};
    mc_client_sdk_cursor *cursor = NULL;
    mc_client_sdk_cursor_error error = {0};
    const mc_client_graphics_site site = {"qualification", "pixels", "()V", -1};
    CHECK(argc == 2);
    CHECK(mc_resource_files_create(argv[1], &files) == MC_REGISTRY_OK);
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
    CHECK(mc_client_sdk_input_inspect(input, &before));
    SDL_Cursor *selected_before = SDL_GetCursor();
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    input_arena.options = (mc_client_render_engine_options){65536u, 32u};
    CHECK(mc_client_ge_d(classes, &input_arena, 1024, site, &images, &graphics));
    CHECK(mc_client_ge_d(classes, &input_arena, 1, site, &delays, &graphics));
    for (uint32_t y = 0; y < 32u; ++y) {
        for (uint32_t x = 0; x < 32u; ++x) {
            const uint32_t bits = pattern(x, y);
            int32_t value;
            memcpy(&value, &bits, sizeof(value));
            CHECK(mc_client_ge_int_put(images, value, site, &graphics));
        }
    }
    CHECK(mc_client_ge_int_flip(images, site, &graphics));
    CHECK(mc_client_ge_int_put(delays, 0, site, &graphics));
    CHECK(mc_client_ge_int_flip(delays, site, &graphics));
    CHECK(mc_client_sdk_cursor_allocate(display, NULL, &cursor, &error) == MC_CLIENT_SDK_CURSOR_OK);
    CHECK(mc_client_sdk_cursor_construct32(cursor, input, images, delays, &error) ==
          MC_CLIENT_SDK_CURSOR_OK);
    CHECK(cursor->native != NULL && cursor->surface == NULL && cursor->rgba != NULL &&
          cursor->source_index == 0 && !cursor->source_destroyed &&
          cursor->native_hotspot_x == 16 && cursor->native_hotspot_y == 15 &&
          SDL_GetCursor() == selected_before);
    CHECK(mc_client_sdk_input_inspect(input, &after) && before.buttons == after.buttons &&
          before.key_down == after.key_down && before.mouse_events == after.mouse_events &&
          before.keyboard_events == after.keyboard_events &&
          before.mouse_read == after.mouse_read && before.keyboard_read == after.keyboard_read &&
          before.grabbed == after.grabbed);
    /* The source buffer owner can disappear after the actual constructor.
     * Neither the completed cursor nor the native child borrows those bytes. */
    mc_client_render_arena_free(&input_arena);
    images = delays = NULL;
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    classes = NULL;
    for (uint32_t y = 0; y < 32u; ++y) {
        for (uint32_t x = 0; x < 32u; ++x) {
            uint32_t expected = pattern(x, 31u - y), actual = 0;
            if (cursor->platform.platform == 3 && (expected >> 24) != 255u)
                expected = 0;
            const size_t index = (size_t)y * 32u + x;
            memcpy(&actual, cursor->flipped->bytes->data + index * 4u, sizeof(actual));
            CHECK(actual == expected);
            CHECK(cursor->rgba[index * 4u] == (uint8_t)(expected >> 16));
            CHECK(cursor->rgba[index * 4u + 1u] == (uint8_t)(expected >> 8));
            CHECK(cursor->rgba[index * 4u + 2u] == (uint8_t)expected);
            CHECK(cursor->rgba[index * 4u + 3u] == (uint8_t)(expected >> 24));
        }
    }
    CHECK(SDL_GetCursor() == selected_before);
    CHECK(mc_client_sdk_cursor_release(&cursor, &error) == MC_CLIENT_SDK_CURSOR_OK &&
          cursor == NULL);
    ok = true;
cleanup:
    if (cursor != NULL && mc_client_sdk_cursor_release(&cursor, &error) != MC_CLIENT_SDK_CURSOR_OK)
        ok = false;
    mc_client_render_arena_free(&input_arena);
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
    SDL_Quit();
    if (!ok)
        return 1;
    printf("cursor pixel controls: %u checks\n", checks);
    return 0;
}
