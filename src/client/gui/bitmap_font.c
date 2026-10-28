#include "../display/source_surface.h"
#include "../render_engine/ge.h"
#include "../render_engine/nw.h"
#include "bitmap_font_internal.h"
#include "font_kernels.h"
#include "mc/core/java/java.h"
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static const char constructor[] = "(Lkv;Ljava/lang/String;Lji;)V";

mc_client_bitmap_font_status mc_client_bitmap_font_fail(mc_client_bitmap_font_error *e,
                                                        mc_client_bitmap_font_status status,
                                                        const char *operation, const char *method,
                                                        const char *descriptor, int32_t pc,
                                                        bool native_policy) {
    e->status = status;
    e->operation = operation;
    e->method = method;
    e->descriptor = descriptor;
    e->pc = pc;
    e->native_policy = native_policy;
    return status;
}

static bool failure(mc_client_bitmap_font_error *e, mc_client_bitmap_font_status status,
                    const char *operation, int32_t pc, bool native_policy) {
    mc_client_bitmap_font_fail(e, status, operation, "<init>", constructor, pc, native_policy);
    return false;
}

static bool graphics_failed(mc_client_bitmap_font_error *e, int32_t pc) {
    return failure(e,
                   e->graphics.status == MC_CLIENT_GRAPHICS_LIMIT ? MC_CLIENT_BITMAP_FONT_LIMIT
                   : e->graphics.status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
                       ? MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED
                   : e->graphics.status == MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
                       ? MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY
                   : e->graphics.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
                       ? MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP
                       : MC_CLIENT_BITMAP_FONT_RENDER_FAILED,
                   "sj genuine graphics child", pc, e->graphics.native_policy);
}

static bool device_failed(mc_client_bitmap_font_error *e, int32_t pc) {
    return failure(e,
                   e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                       ? MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP
                   : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                       ? MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY
                       : MC_CLIENT_BITMAP_FONT_DISPLAY_FAILED,
                   "sj genuine Display child", pc, true);
}

mc_client_bitmap_font_status
mc_client_bitmap_font_allocate(mc_client_loading_classes *gui,
                               const mc_client_render_engine_options *options,
                               mc_client_bitmap_font **out) {
    if (gui == NULL || out == NULL)
        return MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT;
    mc_client_bitmap_font *font = calloc(1, sizeof(*font));
    if (font == NULL)
        return MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED;
    if (mc_client_loading_classes_retain(gui) != MC_CLIENT_LOADING_OK) {
        free(font);
        return MC_CLIENT_BITMAP_FONT_LIMIT;
    }
    font->holds = 1;
    font->gui = gui;
    font->arena.options = options == NULL ? mc_client_render_engine_default_options() : *options;
    font->last_store_pc = -1;
    *out = font;
    return MC_CLIENT_BITMAP_FONT_OK;
}

mc_client_bitmap_font_status mc_client_bitmap_font_retain(mc_client_bitmap_font *font) {
    if (font == NULL)
        return MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT;
    if (font->active || font->phase == MC_CLIENT_BITMAP_FONT_CONSTRUCTING)
        return MC_CLIENT_BITMAP_FONT_BUSY;
    if (font->phase == MC_CLIENT_BITMAP_FONT_RETIRING)
        return MC_CLIENT_BITMAP_FONT_INVALID_PHASE;
    if (font->holds == SIZE_MAX)
        return MC_CLIENT_BITMAP_FONT_LIMIT;
    ++font->holds;
    return MC_CLIENT_BITMAP_FONT_OK;
}

mc_client_bitmap_font_status mc_client_bitmap_font_admit(mc_client_bitmap_font *font,
                                                         const char *method, const char *descriptor,
                                                         mc_client_bitmap_font_error *error) {
    if (error != NULL)
        memset(error, 0, sizeof(*error));
    const mc_client_bitmap_font_status status = font == NULL ? MC_CLIENT_BITMAP_FONT_NULL_REFERENCE
                                                : font->active ? MC_CLIENT_BITMAP_FONT_BUSY
                                                : font->phase != MC_CLIENT_BITMAP_FONT_READY
                                                    ? MC_CLIENT_BITMAP_FONT_INVALID_PHASE
                                                    : MC_CLIENT_BITMAP_FONT_OK;
    if (status != MC_CLIENT_BITMAP_FONT_OK && error != NULL)
        mc_client_bitmap_font_fail(error, status, "sj native call admission", method, descriptor,
                                   -1, true);
    return status;
}

mc_client_bitmap_font_status
mc_client_bitmap_font_call_enter(mc_client_bitmap_font *font, const mc_player_string *text,
                                 const char *method, const char *descriptor,
                                 mc_player_string_view *view, mc_client_bitmap_font_error **scratch,
                                 mc_client_bitmap_font_error *error) {
    mc_client_bitmap_font_status status =
        mc_client_bitmap_font_admit(font, method, descriptor, error);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    if (text == NULL || !mc_player_string_read(text, view) || view->length > 65536u)
        status =
            text == NULL ? MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT : MC_CLIENT_BITMAP_FONT_LIMIT;
    else if (font->holds == SIZE_MAX)
        status = MC_CLIENT_BITMAP_FONT_LIMIT;
    if (status != MC_CLIENT_BITMAP_FONT_OK) {
        if (error != NULL)
            mc_client_bitmap_font_fail(error, status, "sj native String/owner bound", method,
                                       descriptor, -1, true);
        return status;
    }
    mc_client_bitmap_font_error *e = calloc(1, sizeof(*e));
    if (e == NULL) {
        if (error != NULL)
            mc_client_bitmap_font_fail(error, MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED,
                                       "sj native call diagnostics", method, descriptor, -1, true);
        return MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED;
    }
    if (mc_player_string_retain((mc_player_string *)text) != MC_PLAYER_OK) {
        free(e);
        if (error != NULL)
            mc_client_bitmap_font_fail(error, MC_CLIENT_BITMAP_FONT_LIMIT, "sj native String hold",
                                       method, descriptor, -1, true);
        return MC_CLIENT_BITMAP_FONT_LIMIT;
    }
    ++font->holds;
    font->active = true;
    *scratch = e;
    return MC_CLIENT_BITMAP_FONT_OK;
}

mc_client_bitmap_font_status mc_client_bitmap_font_call_leave(mc_client_bitmap_font *font,
                                                              const mc_player_string *text,
                                                              mc_client_bitmap_font_error *e,
                                                              mc_client_bitmap_font_error *error) {
    font->active = false;
    --font->holds;
    mc_player_string_release((mc_player_string *)text);
    const mc_client_bitmap_font_status status = e->status;
    if (error != NULL)
        *error = *e;
    free(e);
    return status;
}

static bool image(mc_client_bitmap_font *font, mc_player_string *name,
                  mc_client_bitmap_font_error *e) {
    if (name == NULL)
        return failure(e, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE,
                       "sj actual Class.getResourceAsStream name", 31, false);
    if (font->gui->resources == NULL)
        return failure(e, MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY,
                       "actual ji.class resource source", 31, false);
    const mc_client_class_stream_status opened = mc_client_class_stream_open(
        font->gui->resources, name, 1024, 16u * 1024u * 1024u, &font->stream, &e->stream);
    if (opened != MC_CLIENT_CLASS_STREAM_OK)
        return failure(e,
                       opened == MC_CLIENT_CLASS_STREAM_SOURCE_NULL_NAME
                           ? MC_CLIENT_BITMAP_FONT_NULL_REFERENCE
                       : opened == MC_CLIENT_CLASS_STREAM_LIMIT ? MC_CLIENT_BITMAP_FONT_LIMIT
                       : opened == MC_CLIENT_CLASS_STREAM_ALLOCATION_FAILED
                           ? MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED
                       : opened == MC_CLIENT_CLASS_STREAM_UNIMPLEMENTED
                           ? MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY
                           : MC_CLIENT_BITMAP_FONT_RENDER_FAILED,
                       "sj actual class resource child", 31,
                       opened != MC_CLIENT_CLASS_STREAM_SOURCE_NULL_NAME);
    const mc_client_image_io_input input = mc_client_class_stream_image_input(font->stream);
    const mc_client_image_io_limits limits = {16u * 1024u * 1024u, 64u * 1024u * 1024u,
                                              32u * 1024u * 1024u};
    const mc_client_image_io_status decoded =
        mc_client_image_io_read(&input, &limits, &font->image, &e->image);
    if (font->stream != NULL)
        (void)mc_client_class_stream_inspect(font->stream, &e->stream);
    if (decoded == MC_CLIENT_IMAGE_IO_OK)
        return true;
    /* Actual IOException42 is wrapped by RuntimeException50. Missing input
     * is ImageIO IllegalArgumentException34 and is not caught by that handler. */
    return failure(
        e,
        decoded == MC_CLIENT_IMAGE_IO_SOURCE_IO ? MC_CLIENT_BITMAP_FONT_SOURCE_RUNTIME_EXCEPTION
        : decoded == MC_CLIENT_IMAGE_IO_SOURCE_ILLEGAL_ARGUMENT
            ? MC_CLIENT_BITMAP_FONT_SOURCE_ILLEGAL_ARGUMENT
        : decoded == MC_CLIENT_IMAGE_IO_LIMIT             ? MC_CLIENT_BITMAP_FONT_LIMIT
        : decoded == MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED ? MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED
        : decoded == MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE
            ? MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY
            : MC_CLIENT_BITMAP_FONT_RENDER_FAILED,
        "sj actual ImageIO child", decoded == MC_CLIENT_IMAGE_IO_SOURCE_IO ? 50 : 34,
        decoded != MC_CLIENT_IMAGE_IO_SOURCE_IO &&
            decoded != MC_CLIENT_IMAGE_IO_SOURCE_ILLEGAL_ARGUMENT);
}

static bool construct(mc_client_bitmap_font *font, mc_game_settings *settings,
                      mc_player_string *name, mc_client_render_engine *engine,
                      mc_client_display *display, mc_client_bitmap_font_error *e) {
    int32_t *widths = mc_client_render_arena_allocate(
        &font->arena, 256u * sizeof(*widths),
        (mc_client_graphics_site){"sj", "<init>", constructor, 8}, &e->graphics);
    if (widths == NULL)
        return graphics_failed(e, 8);
    font->fields.b = widths;
    font->last_store_pc = 10;
    font->fields.a = 0;
    font->last_store_pc = 15;
    mc_client_direct_int_buffer *buffer;
    if (!mc_client_ge_d(font->gui->graphics, &font->arena, 1024,
                        (mc_client_graphics_site){"sj", "<init>", constructor, 22}, &buffer,
                        &e->graphics))
        return graphics_failed(e, 22);
    font->fields.d = buffer;
    font->last_store_pc = 25;
    if (!image(font, name, e))
        return false;
    if (font->image == NULL)
        return failure(e, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE, "sj actual image.getWidth receiver",
                       56, false);
    mc_client_argb_image_state pixels;
    if (!mc_client_argb_image_inspect(font->image, &pixels) || !pixels.constructed)
        return failure(e, MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT,
                       "sj actual constructed getRGB owner", 56, true);
    const int32_t width = (int32_t)pixels.width, height = (int32_t)pixels.height;
    const int32_t length = mc_java_i32_mul(width, height);
    if (length < 0)
        return failure(e, MC_CLIENT_BITMAP_FONT_SOURCE_ILLEGAL_ARGUMENT,
                       "sj negative getRGB array length", 73, false);
    uint32_t *rgb = mc_client_render_arena_allocate(
        &font->arena, (size_t)length * sizeof(*rgb),
        (mc_client_graphics_site){"sj", "<init>", constructor, 73}, &e->graphics);
    if (rgb == NULL)
        return graphics_failed(e, 73);
    if ((size_t)length != pixels.pixel_count)
        return failure(e, MC_CLIENT_BITMAP_FONT_ARRAY_INDEX, "sj getRGB actual destination span",
                       90, false);
    memcpy(rgb, pixels.pixels, (size_t)length * sizeof(*rgb));
    for (int32_t glyph = 0; glyph < 256; ++glyph) {
        const int32_t cell_x = glyph % 16, cell_y = glyph / 16;
        int32_t column = 7;
        for (; column >= 0; --column) {
            const int32_t x = mc_java_i32_add(mc_java_i32_mul(cell_x, 8), column);
            bool empty = true;
            for (int32_t row = 0; row < 8 && empty; ++row) {
                const int32_t offset =
                    mc_java_i32_mul(mc_java_i32_add(mc_java_i32_mul(cell_y, 8), row), width);
                const int32_t index = mc_java_i32_add(x, offset);
                if (index < 0 || index >= length) {
                    e->actual_index = index;
                    return failure(e, MC_CLIENT_BITMAP_FONT_ARRAY_INDEX,
                                   "sj actual blue occupancy read", 176, false);
                }
                if ((rgb[index] & 255u) > 0)
                    empty = false;
            }
            if (!empty)
                break;
        }
        if (glyph == 32)
            column = 2;
        widths[glyph] = mc_java_i32_add(column, 2); /* actual b[ ] store231 */
        ++font->widths_completed;
    }
    if (engine == NULL)
        return failure(e, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE,
                       "sj actual ji registration receiver", 242, false);
    if (mc_client_render_engine_class_identity(engine) != font->gui->graphics)
        return failure(e, MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT,
                       "sj native same actual class namespace", 242, true);
    mc_client_texture id;
    if (mc_client_render_engine_register_image(engine, display, NULL, font->image, &id,
                                               &e->texture) != MC_CLIENT_RENDER_TEXTURE_OK)
        return failure(e, MC_CLIENT_BITMAP_FONT_RENDER_FAILED, "sj actual ji.a(image)", 242, true);
    memcpy(&font->fields.a, &id, sizeof(id));
    font->last_store_pc = 245;
    if (!mc_client_render_engine_font_lease(
            engine, display, id, (mc_client_graphics_site){"sj", "<init>", constructor, 245},
            &font->lease, &e->graphics))
        return graphics_failed(e, 245);
    int32_t base;
    if (!mc_client_ge_generate_lists(font->gui->graphics, display, 288,
                                     (mc_client_graphics_site){"sj", "<init>", constructor, 252},
                                     &base, &e->graphics))
        return graphics_failed(e, 252);
    font->fields.c = base;
    font->last_store_pc = 255;
    mc_client_tessellator *tessellator;
    if (!mc_client_nw_active_use(font->gui->graphics,
                                 (mc_client_graphics_site){"sj", "<init>", constructor, 258},
                                 &tessellator, &e->graphics))
        return graphics_failed(e, 258);
    for (int32_t glyph = 0; glyph < 256; ++glyph) {
        if (!mc_client_display_lists_begin(display, mc_java_i32_add(base, glyph), &e->display))
            return device_failed(e, 284);
        if (!mc_client_nw_begin_quads(tessellator, &e->graphics))
            return graphics_failed(e, 289);
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
            return failure(e, MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP,
                           "sj reached glyph FP arithmetic", 328, true);
        const float extent = 7.99f;
        const float x = (float)((glyph % 16) * 8), y = (float)((glyph / 16) * 8);
        volatile float right = x + extent, bottom = y + extent;
        const float u0 = x / 128.0f, u1 = right / 128.0f, v0 = y / 128.0f, v1 = bottom / 128.0f;
        if (!mc_client_nw_vertex_uv(tessellator, 0, extent, 0, u0, v1, &e->graphics))
            return graphics_failed(e, 354);
        if (!mc_client_nw_vertex_uv(tessellator, extent, extent, 0, u1, v1, &e->graphics))
            return graphics_failed(e, 396);
        if (!mc_client_nw_vertex_uv(tessellator, extent, 0, 0, u1, v0, &e->graphics))
            return graphics_failed(e, 431);
        if (!mc_client_nw_vertex_uv(tessellator, 0, 0, 0, u0, v0, &e->graphics))
            return graphics_failed(e, 459);
        if (!mc_client_nw_draw(tessellator, display, &e->graphics))
            return graphics_failed(e, 464);
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
            return failure(e, MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP,
                           "sj reached width translation integer conversion", 474, true);
        if (!mc_client_display_source_translate(display, (float)widths[glyph], 0, 0, &e->display))
            return device_failed(e, 477);
        if (!mc_client_display_lists_end(display, &e->display))
            return device_failed(e, 480);
        ++font->glyphs_defined;
    }
    for (unsigned index = 0; index < 32; ++index) {
        if (settings == NULL)
            return failure(e, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE,
                           "sj actual late Settings.g receiver", 580, false);
        mc_game_settings_state state;
        if (!mc_game_settings_read_state(settings, &state))
            return failure(e, MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT, "sj live Settings read", 580,
                           true);
        const uint32_t color = mc_font_source_palette(index, state.values.anaglyph);
        if (!mc_client_display_lists_begin(
                display, mc_java_i32_add(mc_java_i32_add(base, 256), (int32_t)index), &e->display))
            return device_failed(e, 689);
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
            return failure(e, MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP,
                           "sj reached palette FP arithmetic", 697, true);
        const float r = (float)((color >> 16u) & 255u) / 255.0f;
        const float g = (float)((color >> 8u) & 255u) / 255.0f;
        const float b = (float)(color & 255u) / 255.0f;
        if (!mc_client_display_source_color(display, r, g, b, 1, &e->display))
            return device_failed(e, 710);
        if (!mc_client_display_lists_end(display, &e->display))
            return device_failed(e, 713);
        ++font->palettes_defined;
    }
    return true;
}

mc_client_bitmap_font_status
mc_client_bitmap_font_construct(mc_client_bitmap_font *font, mc_game_settings *settings,
                                mc_player_string *name, mc_client_render_engine *engine,
                                mc_client_display *display, mc_client_bitmap_font_error *error) {
    if (font == NULL || font->phase != MC_CLIENT_BITMAP_FONT_RAW) {
        if (error != NULL) {
            memset(error, 0, sizeof(*error));
            mc_client_bitmap_font_fail(error,
                                       font == NULL ? MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT
                                                    : MC_CLIENT_BITMAP_FONT_INVALID_PHASE,
                                       "sj constructor native admission", "<init>", constructor, -1,
                                       true);
        }
        return font == NULL ? MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT
                            : MC_CLIENT_BITMAP_FONT_INVALID_PHASE;
    }
    mc_client_bitmap_font_error *e = calloc(1, sizeof(*e));
    if (e == NULL) {
        if (error != NULL) {
            memset(error, 0, sizeof(*error));
            failure(error, MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED, "sj native call diagnostics",
                    -1, true);
        }
        return MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED;
    }
    if (engine != NULL && mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK) {
        failure(e, MC_CLIENT_BITMAP_FONT_LIMIT, "sj native Engine argument hold", -1, true);
        goto done;
    }
    font->retained_engine = engine;
    if (name != NULL && mc_player_string_retain(name) != MC_PLAYER_OK) {
        failure(e, MC_CLIENT_BITMAP_FONT_LIMIT, "sj native name argument hold", -1, true);
        goto failed_admission;
    }
    font->retained_name = name;
    font->borrowed_settings = settings;
    font->display = display;
    font->phase = MC_CLIENT_BITMAP_FONT_CONSTRUCTING;
    const bool ok = construct(font, settings, name, engine, display, e);
    font->phase = ok ? MC_CLIENT_BITMAP_FONT_READY : MC_CLIENT_BITMAP_FONT_FAILED;
    if (ok) {
        (void)mc_client_render_engine_release(font->retained_engine);
        font->retained_engine = NULL;
        mc_player_string_release(font->retained_name);
        font->retained_name = NULL;
        font->borrowed_settings = NULL;
    } else
        font->failure = *e;
    goto done;
failed_admission:
    (void)mc_client_render_engine_release(font->retained_engine);
    font->retained_engine = NULL;
done:;
    const mc_client_bitmap_font_status status = e->status;
    if (error != NULL)
        *error = *e;
    free(e);
    return status;
}

bool mc_client_bitmap_font_inspect(const mc_client_bitmap_font *font,
                                   mc_client_bitmap_font_state *out) {
    if (font == NULL || out == NULL)
        return false;
    *out = (mc_client_bitmap_font_state){font->phase,
                                         font->fields,
                                         font->holds,
                                         font->arena.bytes,
                                         font->arena.allocations,
                                         font->widths_completed,
                                         font->glyphs_defined,
                                         font->palettes_defined,
                                         font->last_store_pc,
                                         font->active,
                                         font->lease != NULL,
                                         font->image,
                                         font->failure};
    return true;
}

mc_client_bitmap_font_status mc_client_bitmap_font_release(mc_client_bitmap_font **slot,
                                                           mc_client_bitmap_font_error *error) {
    if (error != NULL)
        memset(error, 0, sizeof(*error));
    if (slot == NULL) {
        if (error != NULL)
            mc_client_bitmap_font_fail(error, MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT,
                                       "sj native retirement slot", NULL, NULL, -1, true);
        return MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT;
    }
    mc_client_bitmap_font *font = *slot;
    if (font == NULL)
        return MC_CLIENT_BITMAP_FONT_OK;
    if (font->active || font->phase == MC_CLIENT_BITMAP_FONT_CONSTRUCTING) {
        if (error != NULL)
            mc_client_bitmap_font_fail(error, MC_CLIENT_BITMAP_FONT_BUSY,
                                       "sj native active retirement", NULL, NULL, -1, true);
        return MC_CLIENT_BITMAP_FONT_BUSY;
    }
    if (font->holds > 1) {
        --font->holds;
        *slot = NULL;
        return MC_CLIENT_BITMAP_FONT_OK;
    }
    /* Final native cleanup can call an external stream closer. Keep this
     * actual owner entered until every callback-capable cleanup has returned;
     * a failed attempt leaves its one occurrence available for retry. */
    font->phase = MC_CLIENT_BITMAP_FONT_RETIRING;
    font->active = true;
    mc_client_class_stream_error stream_error = {0};
    mc_client_graphics_error graphics_error = {0};
    const bool stream_ok =
        mc_client_class_stream_retire(&font->stream, &stream_error) == MC_CLIENT_CLASS_STREAM_OK;
    const bool lease_ok = mc_client_font_lease_release(&font->lease, &graphics_error);
    if (!stream_ok || !lease_ok) {
        if (error != NULL) {
            error->stream = stream_error;
            error->graphics = graphics_error;
            mc_client_bitmap_font_fail(
                error,
                !stream_ok ? MC_CLIENT_BITMAP_FONT_RENDER_FAILED : MC_CLIENT_BITMAP_FONT_BUSY,
                "sj native retained resource retirement", NULL, NULL, -1, true);
        }
        font->active = false;
        return !stream_ok ? MC_CLIENT_BITMAP_FONT_RENDER_FAILED : MC_CLIENT_BITMAP_FONT_BUSY;
    }
    if (font->retained_engine != NULL) {
        if (mc_client_render_engine_release(font->retained_engine) != MC_CLIENT_RENDER_ENGINE_OK) {
            if (error != NULL)
                mc_client_bitmap_font_fail(error, MC_CLIENT_BITMAP_FONT_BUSY,
                                           "sj retained argument Engine retirement", NULL, NULL, -1,
                                           true);
            font->active = false;
            return MC_CLIENT_BITMAP_FONT_BUSY;
        }
        font->retained_engine = NULL;
    }
    if (mc_client_loading_classes_release(font->gui) != MC_CLIENT_LOADING_OK) {
        if (error != NULL)
            mc_client_bitmap_font_fail(error, MC_CLIENT_BITMAP_FONT_BUSY,
                                       "sj native GUI class retirement", NULL, NULL, -1, true);
        font->active = false;
        return MC_CLIENT_BITMAP_FONT_BUSY;
    }
    mc_player_string_release(font->retained_name);
    mc_client_argb_image_release(font->image);
    mc_client_render_arena_free(&font->arena);
    free(font);
    *slot = NULL;
    return MC_CLIENT_BITMAP_FONT_OK;
}

mc_client_bitmap_font_status mc_client_bitmap_font_retire(mc_client_bitmap_font **slot,
                                                          mc_client_bitmap_font_error *error) {
    if (slot != NULL && *slot != NULL &&
        ((*slot)->holds != 1 || (*slot)->active ||
         (*slot)->phase == MC_CLIENT_BITMAP_FONT_CONSTRUCTING)) {
        if (error != NULL) {
            memset(error, 0, sizeof(*error));
            mc_client_bitmap_font_fail(error, MC_CLIENT_BITMAP_FONT_BUSY,
                                       "sj strict final native retirement still borrowed/active",
                                       NULL, NULL, -1, true);
        }
        return MC_CLIENT_BITMAP_FONT_BUSY;
    }
    return mc_client_bitmap_font_release(slot, error);
}

bool mc_client_bitmap_font_uses_context(const mc_client_bitmap_font *font,
                                        const mc_client_loading_classes *classes,
                                        const mc_client_display *display) {
    return font != NULL && font->gui == classes &&
           (font->display == NULL || font->display == display);
}
