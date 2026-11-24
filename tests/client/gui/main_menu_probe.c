#include "gui/gui_main_menu.h"

#include "mc/block/material/material.h"
#include "mc/block/registry/registry.h"
#include "mc/core/java/random_profile.h"
#include "mc/resources/files/files.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
    SCENE_BYTES = 64 * 1024,
    SCENE_TEXT_UNITS = 16384,
    SCENE_STRING_UNITS = 4096,
    SCENE_TOKENS = SCENE_STRING_UNITS + 12,
    SCENE_PIXELS = 4 * 1024 * 1024
};

typedef struct scene {
    char bytes[SCENE_BYTES + 1];
    size_t size, cursor, units_used;
    uint16_t units[SCENE_TEXT_UNITS];
    mc_font_text splash, labels[MC_GUI_MAIN_MENU_MAX_BUTTONS];
    mc_gui_button buttons[MC_GUI_MAIN_MENU_MAX_BUTTONS];
    mc_gui_main_menu_frame frame;
    int32_t width, height, gui_scale;
    int64_t millis;
    uint32_t partial_tick_bits;
} scene;

typedef struct conditional_clock {
    int64_t millis;
    unsigned int calls;
} conditional_clock;

static bool clock_read(void *context, int64_t *out) {
    conditional_clock *clock = context;
    if (out == NULL || clock->calls != 0)
        return false;
    ++clock->calls;
    *out = clock->millis;
    return true;
}

static bool i64(const char *token, int64_t *out) {
    const bool negative = token[0] == '-';
    const char *digits = token + (negative ? 1 : 0);
    if (digits[0] == '\0' || (digits[0] == '0' && digits[1] != '\0') ||
        (negative && digits[0] == '0'))
        return false;
    const uint64_t limit = (uint64_t)INT64_MAX + (negative ? UINT64_C(1) : UINT64_C(0));
    uint64_t value = 0;
    for (size_t i = 0; digits[i] != '\0'; ++i) {
        if (digits[i] < '0' || digits[i] > '9')
            return false;
        const unsigned int digit = (unsigned int)(digits[i] - '0');
        if (value > (limit - digit) / UINT64_C(10))
            return false;
        value = value * UINT64_C(10) + digit;
    }
    *out = negative ? (value == (uint64_t)INT64_MAX + UINT64_C(1) ? INT64_MIN : -(int64_t)value)
                    : (int64_t)value;
    return true;
}

static bool i32(const char *token, int32_t *out) {
    int64_t value = 0;
    if (!i64(token, &value) || value < INT32_MIN || value > INT32_MAX)
        return false;
    *out = (int32_t)value;
    return true;
}

static bool hex(const char *token, size_t count, uint32_t *out) {
    if (strlen(token) != count)
        return false;
    uint32_t value = 0;
    for (size_t i = 0; i < count; ++i) {
        const char c = token[i];
        const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                   : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                          : 16u;
        if (digit >= 16)
            return false;
        value = (value << 4) | digit;
    }
    *out = value;
    return true;
}

static bool line(scene *value, char **tokens, size_t *count) {
    if (value->cursor >= value->size)
        return false;
    size_t start = value->cursor, total = 0;
    for (;;) {
        if (total >= SCENE_TOKENS || start >= value->size || value->bytes[start] == ' ' ||
            value->bytes[start] == '\n')
            return false;
        tokens[total++] = value->bytes + start;
        size_t end = start;
        while (end < value->size && value->bytes[end] != ' ' && value->bytes[end] != '\n') {
            const unsigned char c = (unsigned char)value->bytes[end];
            if (c < 0x21u || c > 0x7eu)
                return false;
            ++end;
        }
        if (end == value->size)
            return false;
        const char delimiter = value->bytes[end];
        value->bytes[end] = '\0';
        if (delimiter == '\n') {
            value->cursor = end + 1;
            *count = total;
            return true;
        }
        start = end + 1;
    }
}

static bool text(scene *value, char **tokens, size_t count, size_t offset, mc_font_text *storage,
                 const mc_font_text **out) {
    int32_t length = 0;
    if (offset >= count || !i32(tokens[offset], &length) || length < -1 ||
        length > SCENE_STRING_UNITS)
        return false;
    if (length == -1) {
        if (count != offset + 1)
            return false;
        *out = NULL;
        return true;
    }
    if ((size_t)length != count - offset - 1 ||
        (size_t)length > SCENE_TEXT_UNITS - value->units_used)
        return false;
    uint16_t *units = value->units + value->units_used;
    for (int32_t i = 0; i < length; ++i) {
        uint32_t unit = 0;
        if (!hex(tokens[offset + 1 + (size_t)i], 4, &unit))
            return false;
        units[i] = (uint16_t)unit;
    }
    *storage = (mc_font_text){units, (size_t)length};
    *out = storage;
    value->units_used += (size_t)length;
    return true;
}

static bool read_scene(const char *path, scene *value) {
    FILE *input = NULL;
#ifdef _MSC_VER
    if (fopen_s(&input, path, "rb") != 0)
        return false;
#else
    input = fopen(path, "rb");
#endif
    if (input == NULL)
        return false;
    const size_t count = fread(value->bytes, 1, sizeof(value->bytes), input);
    const bool read_ok = !ferror(input) && count <= SCENE_BYTES && feof(input);
    const bool close_ok = fclose(input) == 0;
    if (!read_ok || !close_ok || count == 0)
        return false;
    value->size = count;
    char *tokens[SCENE_TOKENS];
    size_t fields = 0;
    if (!line(value, tokens, &fields) || fields != 1 ||
        strcmp(tokens[0], "MC_GUI_MAIN_MENU_SCENE_V1") != 0)
        return false;
    if (!line(value, tokens, &fields) || fields != 3 || strcmp(tokens[0], "display") != 0 ||
        !i32(tokens[1], &value->width) || !i32(tokens[2], &value->height) || value->width <= 0 ||
        value->height <= 0 || value->width > 4096 || value->height > 4096 ||
        (size_t)value->width * (size_t)value->height > SCENE_PIXELS)
        return false;
    if (!line(value, tokens, &fields) || fields != 2 || strcmp(tokens[0], "gui-scale") != 0 ||
        !i32(tokens[1], &value->gui_scale))
        return false;
    if (!line(value, tokens, &fields) || fields != 3 || strcmp(tokens[0], "mouse") != 0 ||
        !i32(tokens[1], &value->frame.mouse.x) || !i32(tokens[2], &value->frame.mouse.y))
        return false;
    if (!line(value, tokens, &fields) || fields != 2 || strcmp(tokens[0], "partial-tick") != 0 ||
        !hex(tokens[1], 8, &value->partial_tick_bits))
        return false;
    _Static_assert(sizeof(value->frame.partial_tick) == sizeof(value->partial_tick_bits),
                   "the scene carries binary32 partial tick bits");
    memcpy(&value->frame.partial_tick, &value->partial_tick_bits, sizeof(value->partial_tick_bits));
    if (!line(value, tokens, &fields) || fields != 2 || strcmp(tokens[0], "clock-millis") != 0 ||
        !i64(tokens[1], &value->millis))
        return false;
    if (!line(value, tokens, &fields) || strcmp(tokens[0], "splash") != 0 ||
        !text(value, tokens, fields, 1, &value->splash, &value->frame.splash))
        return false;
    int32_t buttons = 0;
    if (!line(value, tokens, &fields) || fields != 2 || strcmp(tokens[0], "buttons") != 0 ||
        !i32(tokens[1], &buttons) || buttons < 0 || buttons > MC_GUI_MAIN_MENU_MAX_BUTTONS)
        return false;
    value->frame.buttons = value->buttons;
    value->frame.button_count = (size_t)buttons;
    for (int32_t i = 0; i < buttons; ++i) {
        mc_gui_button *button = &value->buttons[i];
        int32_t enabled = 0, visible = 0;
        if (!line(value, tokens, &fields) || fields < 9 || strcmp(tokens[0], "button") != 0 ||
            !i32(tokens[1], &button->id) || !i32(tokens[2], &button->x) ||
            !i32(tokens[3], &button->y) || !i32(tokens[4], &button->width) ||
            !i32(tokens[5], &button->height) || !i32(tokens[6], &enabled) ||
            !i32(tokens[7], &visible) || (enabled != 0 && enabled != 1) ||
            (visible != 0 && visible != 1) ||
            !text(value, tokens, fields, 8, &value->labels[i], &button->label))
            return false;
        button->enabled = enabled != 0;
        button->visible = visible != 0;
    }
    return line(value, tokens, &fields) && fields == 1 && strcmp(tokens[0], "end") == 0 &&
           value->cursor == value->size;
}

static bool json_string(const char *value) {
    if (fputc('"', stdout) == EOF)
        return false;
    for (size_t i = 0; value[i] != '\0'; ++i) {
        const unsigned char c = (unsigned char)value[i];
        if (c == '"' || c == '\\') {
            if (fputc('\\', stdout) == EOF || fputc(c, stdout) == EOF)
                return false;
        } else if (c < 32 || c >= 127) {
            if (fprintf(stdout, "\\u%04x", (unsigned int)c) < 0)
                return false;
        } else if (fputc(c, stdout) == EOF)
            return false;
    }
    return fputc('"', stdout) != EOF;
}

static bool json_text(const mc_font_text *value) {
    if (value == NULL)
        return fputs("null", stdout) != EOF;
    if (fputc('[', stdout) == EOF)
        return false;
    for (size_t i = 0; i < value->length; ++i)
        if (fprintf(stdout, "%s%u", i == 0 ? "" : ",", (unsigned int)value->units[i]) < 0)
            return false;
    return fputc(']', stdout) != EOF;
}

static bool metadata(const scene *value, const conditional_clock *clock,
                     const mc_client_display_info *display, const mc_client_framebuffer_info *fb,
                     const mc_client_capture *capture) {
    uint64_t width_bits = 0, height_bits = 0;
    _Static_assert(sizeof(width_bits) == sizeof(value->frame.resolution.width_exact),
                   "binary64 projection");
    memcpy(&width_bits, &value->frame.resolution.width_exact, sizeof(width_bits));
    memcpy(&height_bits, &value->frame.resolution.height_exact, sizeof(height_bits));
    if (fprintf(stdout,
                "{\"schema\":1,\"scope\":\"conditional-native-GuiMainMenu-draw\","
                "\"renderer\":\"shared-production-GuiMainMenu\",\"render_and_cleanup_status\":0,"
                "\"whole_startup_qualified\":false,\"original_native_parity_qualified\":false,"
                "\"format\":\"one JSON LF followed by exact top-down RGBA8 bytes and EOF\","
                "\"width\":%d,\"height\":%d,\"stride\":%zu,\"raw_RGBA_bytes\":%zu,"
                "\"gui_scale_setting\":%" PRId32 ",\"screen_width\":%" PRId32
                ",\"screen_height\":%" PRId32 ","
                "\"scale_factor\":%" PRId32 ",\"projection_width_raw64\":\"%016" PRIx64 "\","
                "\"projection_height_raw64\":\"%016" PRIx64 "\",\"mouse\":[%" PRId32 ",%" PRId32
                "],"
                "\"partial_tick_raw32\":\"%08" PRIx32 "\",\"clock_read_count\":%u,"
                "\"clock_millis_signed64\":\"%" PRId64 "\",\"splash_UTF16\":",
                capture->width, capture->height, capture->stride,
                capture->stride * (size_t)capture->height, value->gui_scale,
                value->frame.resolution.width, value->frame.resolution.height,
                value->frame.resolution.scale_factor, width_bits, height_bits, value->frame.mouse.x,
                value->frame.mouse.y, value->partial_tick_bits, clock->calls, clock->millis) < 0 ||
        !json_text(value->frame.splash) || fputs(",\"buttons\":[", stdout) == EOF)
        return false;
    for (size_t i = 0; i < value->frame.button_count; ++i) {
        const mc_gui_button *b = &value->buttons[i];
        if (fprintf(stdout,
                    "%s{\"id\":%" PRId32 ",\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"width\":%" PRId32
                    ",\"height\":%" PRId32 ",\"enabled\":%s,\"visible\":%s,\"label_UTF16\":",
                    i == 0 ? "" : ",", b->id, b->x, b->y, b->width, b->height,
                    b->enabled ? "true" : "false", b->visible ? "true" : "false") < 0 ||
            !json_text(b->label) || fputc('}', stdout) == EOF)
            return false;
    }
    if (fprintf(stdout, "],\"GL\":{\"major\":%d,\"minor\":%d,\"profile\":%d,\"vendor\":",
                display->gl_major, display->gl_minor, display->profile) < 0 ||
        !json_string(display->vendor) || fputs(",\"renderer\":", stdout) == EOF ||
        !json_string(display->renderer) || fputs(",\"version\":", stdout) == EOF ||
        !json_string(display->version))
        return false;
    return fprintf(stdout,
                   "},\"effective_framebuffer\":{\"RGBA_bits\":[%d,%d,%d,%d],"
                   "\"depth_bits\":%d,\"stencil_bits\":%d,\"double_buffer\":%s,\"stereo\":%s,"
                   "\"sample_buffers\":%d,\"samples\":%d,\"srgb_enabled\":%s,"
                   "\"color_attachment\":%" PRIu32 ",\"color_encoding\":%" PRIu32
                   ",\"draw_buffer\":%" PRIu32 ",\"read_buffer\":%" PRIu32 "},"
                   "\"requested_display_api\":\"mc_client_display_create default profile; no "
                   "overrides\","
                   "\"profile_note\":\"Actual native default framebuffer; no format matching or "
                   "alpha normalization requested\"}\n",
                   fb->red_bits, fb->green_bits, fb->blue_bits, fb->alpha_bits, fb->depth_bits,
                   fb->stencil_bits, fb->double_buffer ? "true" : "false",
                   fb->stereo ? "true" : "false", fb->sample_buffers, fb->samples,
                   fb->srgb_enabled ? "true" : "false", fb->color_attachment, fb->color_encoding,
                   fb->draw_buffer, fb->read_buffer) >= 0;
}

int main(int argc, char **argv) {
    int result = 1;
    scene *value = NULL;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_resource_files *files = NULL;
    mc_registry_classes *registry = NULL;
    mc_client_assets *assets = NULL;
    mc_client_display *display = NULL;
    mc_gui_main_menu *menu = NULL;
    mc_client_capture capture = {0};
    mc_client_display_error display_error = {0};
    mc_gui_main_menu_error menu_error = {0};
    mc_client_display_info display_info = {0};
    mc_client_framebuffer_info framebuffer = {0};
    conditional_clock clock = {0};
    if (argc != 3) {
        fputs("usage: main_menu_probe RESOURCE_ROOT SCENE_FILE\n", stderr);
        return 1;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
#endif
    value = calloc(1, sizeof(*value));
    if (value == NULL || !read_scene(argv[2], value)) {
        fputs("invalid or unreadable bounded GuiMainMenu scene\n", stderr);
        goto cleanup;
    }
    if (mc_java_random_profile_create(1024u * 1024u, &profile) != MC_JAVA_RANDOM_PROFILE_OK ||
        !mc_materials_create(&materials) ||
        mc_resource_files_create(argv[1], &files) != MC_REGISTRY_OK)
        goto cleanup;
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .resources = mc_resource_files_source(files)};
    mc_registry_error registry_error = {0};
    if (mc_registry_classes_allocate(&options, &registry, &registry_error) != MC_REGISTRY_OK ||
        mc_client_assets_create(options.resources, registry, &assets) != MC_CLIENT_ASSETS_OK ||
        !mc_client_display_create("Conditional GuiMainMenu", value->width, value->height, true,
                                  &display, &display_error))
        goto cleanup;
    int width = 0, height = 0;
    if (!mc_client_display_pixel_size(display, &width, &height, &display_error) ||
        width != value->width || height != value->height ||
        !mc_scaled_resolution_init(width, height, value->gui_scale, &value->frame.resolution) ||
        !mc_gui_main_menu_create(display, assets, false, &menu, &menu_error))
        goto cleanup;
    clock.millis = value->millis;
    if (!mc_gui_main_menu_draw(menu, &value->frame, clock_read, &clock, &menu_error) ||
        clock.calls != 1 || !mc_client_display_capture_rgba(display, &capture, &display_error) ||
        capture.pixels == NULL || capture.width != width || capture.height != height ||
        capture.stride != (size_t)width * 4u ||
        !mc_client_display_framebuffer_info_get(display, &framebuffer, &display_error))
        goto cleanup;
    const mc_client_display_info *info = mc_client_display_info_get(display);
    if (info == NULL)
        goto cleanup;
    display_info = *info;
    result = 0;
cleanup:
    if (result != 0)
        fprintf(
            stderr, "GuiMainMenu probe failed: menu=%d/%s display=%d/%s\n", (int)menu_error.status,
            menu_error.operation == NULL ? "none" : menu_error.operation, (int)display_error.status,
            display_error.operation == NULL ? "none" : display_error.operation);
    mc_gui_main_menu_free(menu);
    menu = NULL;
    mc_client_display *retiring = display;
    display = NULL;
    if (!mc_client_display_destroy(retiring, &display_error)) {
        result = 1;
        fputs("GuiMainMenu display teardown failed\n", stderr);
    }
    mc_client_assets_free(assets);
    assets = NULL;
    mc_registry_error free_error = {0};
    if (mc_registry_classes_free(registry, &free_error) != MC_REGISTRY_OK) {
        result = 1;
        fputs("GuiMainMenu Registry teardown failed\n", stderr);
    } else {
        registry = NULL;
    }
    if (registry == NULL) {
        mc_materials_destroy(materials);
        materials = NULL;
        if (mc_resource_files_free(files) != MC_REGISTRY_OK) {
            result = 1;
            fputs("GuiMainMenu resource-source teardown failed\n", stderr);
        }
        files = NULL;
        mc_java_random_profile_free(profile);
        profile = NULL;
    } else {
        /* Preserve live borrowed dependencies if Registry refuses retirement;
         * process exit retires this failed prefix rather than dangling them. */
        fputs("GuiMainMenu live Registry dependencies retained on failed teardown\n", stderr);
    }
    if (result == 0 && (!metadata(value, &clock, &display_info, &framebuffer, &capture) ||
                        fwrite(capture.pixels, 1, capture.stride * (size_t)capture.height,
                               stdout) != capture.stride * (size_t)capture.height ||
                        fflush(stdout) != 0))
        result = 1;
    mc_client_capture_free(&capture);
    free(value);
    return result;
}
