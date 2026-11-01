#include "item_overlay.h"
#include "client/display/fog_surface.h"

#include "client/display/source_surface.h"
#include "item/registry/registry_internal.h"
#include "item/stack/source_internal.h"
#include "item_renderer_internal.h"
#include "mc/core/java/java.h"

#include <string.h>

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
    const mc_registry_item_array *array;
    c->child = (mc_client_graphics_site){"iz", "e", "()Z", 0};
    if (mc_registry_item_c_get(c->registry, &array, &c->registry_error) != MC_REGISTRY_OK)
        return registry_failure(c, 91);
    int32_t id;
    c->child.pc = 4;
    c->stack_error = mc_item_stack_source_id_get(c->stack, &id);
    if (c->stack_error != MC_ITEM_STACK_OK)
        return fail(c, "Item overlay actual durability Stack ID", 91, true);
    const mc_item *item;
    c->child.pc = 7;
    if (mc_registry_item_c_at(c->registry, array, id, &item, &c->registry_error) != MC_REGISTRY_OK)
        return registry_failure(c, 91);
    c->child.pc = 8;
    if (item == NULL)
        return null_reference(c, "Item overlay actual durability Item null receiver", 91);
    if (item->kind != MC_ITEM_BASE && item->kind != MC_ITEM_BOW && item->kind != MC_ITEM_BLOCK)
        return fail(c, "Item overlay actual maximum-durability declaring-class proof", 91, true);
    c->maximum = item->fields.a; /* Actual inherited GM.f(): getfield1 / ireturn4. */
    c->durability_returned = true;
    if (c->maximum > 0) {
        c->child = (mc_client_graphics_site){"iz", "g", "()Z", 8};
        c->stack_error = mc_item_stack_source_damage_get(c->stack, &c->damage);
        if (c->stack_error != MC_ITEM_STACK_OK)
            return fail(c, "Item overlay actual positive-maximum damage read", 91, true);
        c->damage_read = true;
        c->damaged = c->damage > 0;
    }
    if (c->damaged)
        return fail(c, "Item overlay actual damaged-item bar geometry", 97, true);
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
