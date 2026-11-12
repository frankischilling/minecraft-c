#include "item_overlay.h"
#include "block/registry/registry_internal.h"
#include "client/display/fog_surface.h"

#include "client/display/source_surface.h"
#include "item/registry/registry_internal.h"
#include "item/stack/source_internal.h"
#include "item_renderer_internal.h"
#include "mc/core/java/java.h"

#include <string.h>
#include <xmmintrin.h>

static bool fail(mc_client_item_overlay_context *c, const char *operation, unsigned int pc,
                 bool native) {
    c->operation = operation;
    c->source_pc = pc;
    c->site = (mc_client_graphics_site){"bb", "b", "(Lsj;Lji;Liz;II)V", (int32_t)pc};
    c->native_policy = native;
    return false;
}

static bool null_reference(mc_client_item_overlay_context *c, const char *operation,
                           unsigned int pc) {
    c->source_exception = true;
    c->throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    c->exception_class = "java/lang/NullPointerException";
    return fail(c, operation, pc, false);
}

static bool gl(mc_client_item_overlay_context *c, bool ok, unsigned int pc) {
    c->source_pc = pc;
    return ok || fail(c, c->display_error.operation, pc, true);
}

static bool registry_failure(mc_client_item_overlay_context *c, unsigned int pc) {
    mc_item_render_registry_classification(c->registry, &c->registry_error, &c->throwable,
                                           &c->exception_class);
    c->source_exception = c->throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    return fail(c, c->registry_error.message, pc, c->throwable == MC_ITEM_RENDER_NO_THROWABLE);
}

static bool font_result(mc_client_item_overlay_context *c, mc_client_bitmap_font_status status,
                        unsigned int pc) {
    if (status == MC_CLIENT_BITMAP_FONT_OK)
        return true;
    c->child = (mc_client_graphics_site){"sj", c->font_error.method, c->font_error.descriptor,
                                         c->font_error.pc};
    if (!c->font_error.native_policy) {
        switch (status) {
            case MC_CLIENT_BITMAP_FONT_NULL_REFERENCE:
                c->exception_class = "java/lang/NullPointerException";
                break;
            case MC_CLIENT_BITMAP_FONT_ARRAY_INDEX:
                c->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
                break;
            case MC_CLIENT_BITMAP_FONT_STRING_INDEX:
                c->exception_class = "java/lang/StringIndexOutOfBoundsException";
                break;
            case MC_CLIENT_BITMAP_FONT_SOURCE_ILLEGAL_ARGUMENT:
                c->exception_class = "java/lang/IllegalArgumentException";
                break;
            case MC_CLIENT_BITMAP_FONT_SOURCE_RUNTIME_EXCEPTION:
                c->exception_class = "java/lang/RuntimeException";
                break;
            default:
                break;
        }
        c->source_exception = c->exception_class != NULL;
        if (c->source_exception)
            c->throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    }
    return fail(c, c->font_error.operation, pc, c->font_error.native_policy);
}

static size_t decimal(int32_t value, uint16_t out[12]) {
    uint16_t reverse[10];
    const bool negative = value < 0;
    uint32_t magnitude = negative ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    size_t digits = 0, length = 0;
    do {
        reverse[digits++] = (uint16_t)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);
    if (negative)
        out[length++] = '-';
    while (digits != 0)
        out[length++] = reverse[--digits];
    return length;
}

static bool maximum(mc_client_item_overlay_context *c, const char *method, unsigned int pc,
                    int32_t *out) {
    const mc_registry_item_array *array;
    c->child = (mc_client_graphics_site){"iz", method, method[0] == 'e' ? "()Z" : "()I", 0};
    if (mc_registry_item_c_get(c->registry, &array, &c->registry_error) != MC_REGISTRY_OK)
        return registry_failure(c, pc);
    int32_t id;
    c->child.pc = 4;
    c->stack_error = mc_item_stack_source_id_get(c->stack, &id);
    if (c->stack_error != MC_ITEM_STACK_OK)
        return fail(c, "Item overlay actual durability Stack ID", pc, true);
    const mc_item *item;
    c->child.pc = 7;
    if (mc_registry_item_c_at(c->registry, array, id, &item, &c->registry_error) != MC_REGISTRY_OK)
        return registry_failure(c, pc);
    c->child.pc = 8;
    if (item == NULL)
        return null_reference(c, "Item overlay actual durability Item null receiver", pc);
    if (item->classes != c->registry || c->registry->entered != 0)
        return fail(c, "Item overlay actual maximum Item storage namespace", pc, true);
    /* All 38 supplied concrete classes inherit GM.f(): field1/return4.
     * This resolution is independent of GM.g(), GM.bj and constructor phase. */
    switch (item->kind) {
        case MC_ITEM_BASE:
        case MC_ITEM_SHOVEL:
        case MC_ITEM_PICKAXE:
        case MC_ITEM_AXE:
        case MC_ITEM_FLINT_AND_STEEL:
        case MC_ITEM_FOOD:
        case MC_ITEM_BOW:
        case MC_ITEM_COAL:
        case MC_ITEM_SWORD:
        case MC_ITEM_HOE:
        case MC_ITEM_SEEDS:
        case MC_ITEM_ARMOR:
        case MC_ITEM_PAINTING:
        case MC_ITEM_SIGN:
        case MC_ITEM_DOOR:
        case MC_ITEM_BUCKET:
        case MC_ITEM_MINECART:
        case MC_ITEM_SADDLE:
        case MC_ITEM_REDSTONE:
        case MC_ITEM_SNOWBALL:
        case MC_ITEM_BOAT:
        case MC_ITEM_BLOCK_PLACER:
        case MC_ITEM_EGG:
        case MC_ITEM_FISHING_ROD:
        case MC_ITEM_DYE:
        case MC_ITEM_BED:
        case MC_ITEM_SHEARS:
        case MC_ITEM_RECORD:
        case MC_ITEM_SOUP:
        case MC_ITEM_COOKIE:
        case MC_ITEM_MAP:
        case MC_ITEM_BLOCK:
        case MC_ITEM_WOOL_BLOCK:
        case MC_ITEM_LOG_BLOCK:
        case MC_ITEM_SLAB_BLOCK:
        case MC_ITEM_SAPLING_BLOCK:
        case MC_ITEM_LEAVES_BLOCK:
        case MC_ITEM_PISTON_BLOCK:
            *out = item->fields.a;
            return true;
        default:
            return fail(c, "Item overlay unresolved maximum-durability declaration", pc, true);
    }
}

static bool damage(mc_client_item_overlay_context *c, unsigned int pc, int32_t *out) {
    c->child = (mc_client_graphics_site){"iz", "h", "()I", 1};
    c->stack_error = mc_item_stack_source_damage_get(c->stack, out);
    return c->stack_error == MC_ITEM_STACK_OK ||
           fail(c, "Item overlay actual current damage field", pc, true);
}

static bool fp(mc_client_item_overlay_context *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, "Item overlay strict floating controls", pc, true);
}

/* Exact Math.round(D)J significand operation, including negative ties and
 * saturating NaN/infinity conversion. floor(value+.5) can double-round. */
static int64_t round_double(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const int32_t shift = 1074 - (int32_t)((bits >> 52) & 0x7ffu);
    if (shift >= 0 && shift < 64) {
        int64_t significand =
            (int64_t)((bits & UINT64_C(0x000fffffffffffff)) | UINT64_C(0x0010000000000000));
        if ((bits >> 63) != 0)
            significand = -significand;
        return mc_java_i64_shr(mc_java_i64_add(mc_java_i64_shr(significand, shift), 1), 1);
    }
    return mc_java_double_to_i64(value);
}

static bool graphics(mc_client_item_overlay_context *c, bool ok, unsigned int pc) {
    if (ok)
        return true;
    if (!c->graphics_error.native_policy) {
        switch (c->graphics_error.status) {
            case MC_CLIENT_GRAPHICS_NULL_REFERENCE:
                c->exception_class = "java/lang/NullPointerException";
                break;
            case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT:
                c->exception_class = "java/lang/IllegalArgumentException";
                break;
            case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE:
                c->exception_class = "java/lang/IllegalStateException";
                break;
            case MC_CLIENT_GRAPHICS_ARRAY_INDEX:
                c->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
                break;
            case MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW:
                c->exception_class = "java/nio/BufferOverflowException";
                break;
            default:
                break;
        }
        c->source_exception = c->exception_class != NULL;
        if (c->source_exception)
            c->throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    }
    return fail(c, c->graphics_error.operation, pc, c->graphics_error.native_policy);
}

bool mc_client_item_overlay_rectangle_entered(mc_client_item_overlay_rectangle_context *c) {
    if (c == NULL)
        return false;
    c->source_pc = 1;
    if (c->nw == NULL) {
        c->source_exception = true;
        c->exception_class = "java/lang/NullPointerException";
        return false;
    }
    if (!mc_client_nw_begin_quads(c->nw, &c->error))
        return false;
    c->source_pc = 7;
    if (!mc_client_nw_rgb(c->nw, c->color, &c->error))
        return false;
    static const int32_t vertex_pc[4] = {20, 34, 49, 63};
    for (size_t i = 0; i < 4; ++i) {
        c->source_pc = (unsigned int)vertex_pc[i];
        const int32_t vx = mc_java_i32_add(c->x, i >= 2 ? c->width : 0);
        const int32_t vy = mc_java_i32_add(c->y, i == 1 || i == 2 ? c->height : 0);
        if (!mc_client_nw_vertex(c->nw, (double)vx, (double)vy, 0, &c->error))
            return false;
        ++c->vertices;
    }
    c->source_pc = 67;
    return true;
}

/* Actual private BB.a(NW,IIIII), preserving the captured NW receiver and
 * Java additions before each vertex conversion. No source state is unwound. */
static bool rectangle(mc_client_item_overlay_context *c, int32_t width, int32_t height,
                      int32_t color, unsigned int pc) {
    mc_client_item_overlay_rectangle_context prefix = {.nw = c->nw,
                                                       .x = mc_java_i32_add(c->x, 2),
                                                       .y = mc_java_i32_add(c->y, 13),
                                                       .width = width,
                                                       .height = height,
                                                       .color = color};
    const bool entered = mc_client_item_overlay_rectangle_entered(&prefix);
    c->bar_vertices += prefix.vertices;
    c->child = (mc_client_graphics_site){"bb", "a", "(Lnw;IIIII)V", (int32_t)prefix.source_pc};
    if (!entered) {
        if (prefix.source_exception) {
            c->source_exception = true;
            c->throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
            c->exception_class = prefix.exception_class;
            return fail(c, "Item overlay rectangle actual NW null receiver", pc, false);
        }
        c->graphics_error = prefix.error;
        return graphics(c, false, pc);
    }
    mc_client_nw_fields fields;
    if (c->bar_rectangles < 3 && mc_client_nw_fields_get(c->nw, &fields) && fields.g != NULL &&
        fields.o >= 48) {
        memcpy(c->bar_words[c->bar_rectangles], fields.g, sizeof(c->bar_words[0]));
        c->bar_word_counts[c->bar_rectangles] = 48;
    }
    c->child = (mc_client_graphics_site){"bb", "a", "(Lnw;IIIII)V", 67};
    if (!graphics(c, mc_client_nw_draw(c->nw, c->display, &c->graphics_error), pc))
        return false;
    ++c->bar_rectangles;
    return true;
}

void mc_client_item_overlay_bar_colors(int32_t intensity, int32_t *color, int32_t *shade) {
    const int32_t red = mc_java_i32_sub(255, intensity);
    *color = mc_java_i32_shl(red, 16) | mc_java_i32_shl(intensity, 8);
    int32_t divided;
    (void)mc_java_i32_div(red, 4, &divided);
    *shade = mc_java_i32_shl(divided, 16) | 16128;
}

static bool bar(mc_client_item_overlay_context *c) {
    for (size_t i = 0; i < 2; ++i) {
        const unsigned int damage_pc = i == 0 ? 101u : 126u;
        const unsigned int maximum_pc = i == 0 ? 110u : 135u;
        const unsigned int multiply_pc = i == 0 ? 108u : 133u;
        const unsigned int divide_pc = i == 0 ? 114u : 139u;
        const double scale = i == 0 ? 13.0 : 255.0;
        if (!damage(c, damage_pc, &c->bar_damage[i]) || !fp(c, multiply_pc))
            return false;
        volatile double product = (double)c->bar_damage[i] * scale;
        if (!maximum(c, "j", maximum_pc, &c->bar_maximum[i]) || !fp(c, divide_pc))
            return false;
        volatile double quotient = product / (double)c->bar_maximum[i];
        volatile double remaining = scale - quotient;
        const int32_t rounded = mc_java_i32_narrow(round_double(remaining));
        if (i == 0)
            c->bar_width = rounded;
        else
            c->bar_intensity = rounded;
        ++c->bar_samples;
    }
    if (!gl(c, mc_client_display_source_lighting_enable(c->display, false, &c->display_error),
            150) ||
        !gl(c, mc_client_display_source_depth_test(c->display, false, &c->display_error), 156) ||
        !gl(c, mc_client_display_source_texture_enable(c->display, false, &c->display_error), 162))
        return false;
    const mc_client_graphics_site active = {"bb", "b", "(Lsj;Lji;Liz;II)V", 165};
    if (!mc_client_nw_active_use(c->graphics, active, &c->nw, &c->graphics_error)) {
        c->child = c->graphics_error.site;
        return graphics(c, false, 165);
    }
    mc_client_item_overlay_bar_colors(c->bar_intensity, &c->bar_color, &c->bar_shade);
    if (!rectangle(c, 13, 2, 0, 220) || !rectangle(c, 12, 1, c->bar_shade, 240) ||
        !rectangle(c, c->bar_width, 1, c->bar_color, 260) ||
        !gl(c, mc_client_display_source_texture_enable(c->display, true, &c->display_error), 266) ||
        !gl(c, mc_client_display_source_lighting_enable(c->display, true, &c->display_error),
            272) ||
        !gl(c, mc_client_display_source_depth_test(c->display, true, &c->display_error), 278) ||
        !gl(c, mc_client_display_source_color(c->display, 1, 1, 1, 1, &c->display_error), 285))
        return false;
    c->bar_returned = true;
    return true;
}

static bool body(mc_client_item_overlay_context *c) {
    if (c->stack == NULL) {
        c->null_stack = c->returned = true;
        c->source_pc = 4;
        return true;
    }
    c->stack_error = mc_item_stack_source_count_get(c->stack, &c->count);
    if (c->stack_error != MC_ITEM_STACK_OK)
        return fail(c, "Item overlay actual Stack count read", 6, true);
    c->count_read = true;
    if (c->count > 1) {
        int32_t count;
        c->stack_error = mc_item_stack_source_count_get(c->stack, &count);
        if (c->stack_error != MC_ITEM_STACK_OK)
            return fail(c, "Item overlay current Stack count read", 26, true);
        const mc_player_string_view text = {c->count_units, decimal(count, c->count_units)};
        if (!gl(c, mc_client_display_source_lighting_enable(c->display, false, &c->display_error),
                40) ||
            !gl(c, mc_client_display_source_depth_test(c->display, false, &c->display_error), 46))
            return false;
        if (c->font == NULL)
            return null_reference(c, "Item overlay actual Font measure null receiver", 62);
        int32_t width;
        if (!font_result(
                c, mc_client_bitmap_font_measure_view(c->font, &text, &width, &c->font_error), 62))
            return false;
        const int32_t x = mc_java_i32_sub(mc_java_i32_sub(mc_java_i32_add(c->x, 19), 2), width);
        const int32_t y = mc_java_i32_add(mc_java_i32_add(c->y, 6), 3);
        if (!font_result(c,
                         mc_client_bitmap_font_draw_shadow_view(c->font, &text, x, y, 16777215,
                                                                &c->font_error),
                         75))
            return false;
        c->count_drawn = true;
        if (!gl(c, mc_client_display_source_lighting_enable(c->display, true, &c->display_error),
                81) ||
            !gl(c, mc_client_display_source_depth_test(c->display, true, &c->display_error), 87))
            return false;
    }
    /* Actual IZ.g -> IZ.e: GM.c active use precedes the current Stack ID,
     * array access and GM.f() maximum-durability leaf. A zero maximum skips
     * the damage read; it does not substitute a Dirt-specific false result. */
    if (!maximum(c, "e", 91, &c->maximum))
        return false;
    c->durability_returned = true;
    if (c->maximum > 0) {
        c->child = (mc_client_graphics_site){"iz", "g", "()Z", 8};
        c->stack_error = mc_item_stack_source_damage_get(c->stack, &c->damage);
        if (c->stack_error != MC_ITEM_STACK_OK)
            return fail(c, "Item overlay actual positive-maximum damage read", 91, true);
        c->damage_read = true;
        c->damaged = c->damage > 0;
    }
    if (c->damaged && !bar(c))
        return false;
    c->child = (mc_client_graphics_site){.pc = -1};
    c->source_pc = 288;
    c->returned = true;
    return true;
}

bool mc_client_item_overlay_draw(mc_client_item_overlay_context *c) {
    if (c == NULL)
        return false;
    if (c->owner == NULL)
        return null_reference(c, "Item overlay actual BB null receiver", 0);
    if (c->owner->phase != MC_CLIENT_MODEL_READY || c->owner->gui_active ||
        c->owner->gui_call != NULL)
        return fail(c, "Item overlay actual BB active/pending native occurrence", 0, true);
    c->owner->gui_active = true;
    const bool ok = body(c);
    c->owner->gui_active = false;
    return ok;
}
