#include "item_gui.h"
#include "../../block/registry/runtime_internal.h"
#include "../../item/registry/registry_internal.h"
#include "../../item/stack/source_internal.h"
#include "../display/camera_surface.h"
#include "../display/fog_surface.h"
#include "../display/source_surface.h"
#include "../display/world_frame_surface.h"
#include "../render_engine/internal.h"
#include "blocks/internal.h"
#include "item_renderer_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

struct mc_client_item_gui_call {
    mc_client_item_gui_state state;
    mc_client_item_gui_error failure, cleanup;
    mc_client_item_renderer *owner;
    mc_client_render_engine_classes *graphics;
    mc_registry_classes *registry;
    mc_client_render_engine *engine;
    mc_item_stack *stack;
    mc_client_tessellator *nw;
    mc_cv_context *inventory;
    bool graphics_lease, graphics_held, registry_held, stack_held, engine_held, pending_owned;
    uint16_t key[32];
};

static const char stack_descriptor[] = "(Lsj;Lji;Liz;II)V";
static const char raw_descriptor[] = "(Lsj;Lji;IIIII)V";

static mc_client_graphics_site site(const char *descriptor, int32_t pc) {
    return (mc_client_graphics_site){"bb", "a", descriptor, pc};
}

static mc_client_item_gui_status
admission(mc_client_item_gui_error *e, mc_client_item_gui_status status, const char *operation) {
    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->status = status;
        e->operation = operation;
        e->native_policy = status != MC_CLIENT_ITEM_GUI_OK;
        e->site.pc = e->caller.pc = e->child.pc = -1;
    }
    return status;
}

static bool fail(mc_client_item_gui_call *c, mc_client_item_gui_status status,
                 const char *operation, const char *descriptor, int32_t pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = site(descriptor, pc);
    c->failure.native_policy = native;
    if (descriptor == stack_descriptor)
        c->state.entry_pc = pc;
    else if (strcmp(descriptor, "(IIIIII)V") == 0) {
        c->state.quad_pc = pc;
        c->failure.caller = site(raw_descriptor, 352);
    } else
        c->state.raw_pc = pc;
    return false;
}

static bool fp(mc_client_item_gui_call *c, int32_t pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_CLIENT_ITEM_GUI_UNSUPPORTED_FP, "GUI Item strict floating controls",
                raw_descriptor, pc, true);
}

static bool gl(mc_client_item_gui_call *c, bool ok, int32_t pc) {
    c->state.raw_pc = pc;
    return ok || fail(c, MC_CLIENT_ITEM_GUI_DISPLAY_FAILED, c->failure.display.operation,
                      raw_descriptor, pc, true);
}

static bool graphics(mc_client_item_gui_call *c, bool ok, const char *descriptor, int32_t pc) {
    if (ok)
        return true;
    c->failure.child = c->failure.graphics.site;
    return fail(c, MC_CLIENT_ITEM_GUI_GRAPHICS_FAILED, c->failure.graphics.operation, descriptor,
                pc, c->failure.graphics.native_policy);
}

static bool registry_failure(mc_client_item_gui_call *c, const char *operation, int32_t pc) {
    const char *exception = NULL;
    mc_item_render_throwable throwable = MC_ITEM_RENDER_NO_THROWABLE;
    mc_item_render_registry_classification(c->registry, &c->failure.registry, &throwable,
                                           &exception);
    c->failure.throwable = throwable;
    c->failure.exception_class = exception;
    c->failure.source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    return fail(c, MC_CLIENT_ITEM_GUI_REGISTRY_FAILED, operation, raw_descriptor, pc,
                throwable == MC_ITEM_RENDER_NO_THROWABLE);
}

static bool null_reference(mc_client_item_gui_call *c, const char *operation, int32_t pc) {
    c->failure.source_exception = true;
    c->failure.throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    c->failure.exception_class = "java/lang/NullPointerException";
    return fail(c, MC_CLIENT_ITEM_GUI_NULL_REFERENCE, operation, raw_descriptor, pc, false);
}

static bool hold_graphics(mc_client_item_gui_call *c, int32_t pc) {
    if (c->graphics_lease)
        return true;
    if (c->graphics == NULL || c->graphics->resource_retirement_started ||
        c->graphics->scene_lease_count == SIZE_MAX)
        return fail(c, MC_CLIENT_ITEM_GUI_BUSY, "GUI Item graphics occurrence", raw_descriptor, pc,
                    true);
    c->failure.owner = mc_client_render_engine_classes_retain(c->graphics);
    if (c->failure.owner != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(c, MC_CLIENT_ITEM_GUI_BUSY, "GUI Item graphics occurrence", raw_descriptor, pc,
                    true);
    ++c->graphics->scene_lease_count;
    c->graphics_lease = c->graphics_held = true;
    return true;
}

static bool lookup(mc_client_item_gui_call *c, const char *literal, int32_t invoke, int32_t bind) {
    if (c->engine == NULL)
        return null_reference(c, "actual GUI Item null Engine lookup receiver", invoke);
    if (c->engine->classes != c->graphics)
        return fail(c, MC_CLIENT_ITEM_GUI_BUSY, "GUI Item same-namespace Engine occurrence",
                    raw_descriptor, invoke, true);
    c->failure.owner = mc_client_render_engine_retain(c->engine);
    if (c->failure.owner != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(c, MC_CLIENT_ITEM_GUI_BUSY, "GUI Item Engine occurrence", raw_descriptor,
                    invoke, true);
    c->engine_held = true;
    const size_t length = strlen(literal);
    if (length > sizeof(c->key) / sizeof(*c->key))
        return fail(c, MC_CLIENT_ITEM_GUI_LIMIT, "GUI Item literal bound", raw_descriptor, invoke,
                    true);
    for (size_t i = 0; i < length; ++i)
        c->key[i] = (unsigned char)literal[i];
    c->state.texture_key = (mc_texture_pack_text){c->key, length, false};
    bool foreign = mc_client_texture_lookup_pending(c->engine);
    bool found = false;
    mc_client_texture prior_id;
    (void)mc_client_render_engine_cached_texture(c->engine, c->state.texture_key, &found,
                                                 &prior_id);
    mc_client_texture_lookup_status status =
        mc_client_render_engine_texture(c->engine, c->state.display, NULL, c->state.texture_key,
                                        &c->state.texture_id, &c->failure.texture);
    c->state.pending_lookup_owned = c->pending_owned =
        !foreign && mc_client_texture_lookup_pending(c->engine);
    if (status != MC_CLIENT_LOOKUP_OK) {
        c->failure.child = c->failure.texture.site;
        return fail(c, MC_CLIENT_ITEM_GUI_TEXTURE_FAILED, c->failure.texture.operation,
                    raw_descriptor, invoke, c->failure.texture.native_policy);
    }
    c->state.lookup_cache_hit = found;
    c->state.lookup_returned = true;
    /* Actual JI.b(int): negative IDs return4 without glBindTexture. */
    const int32_t numeric_id = mc_java_i32_narrow((int64_t)c->state.texture_id);
    if (numeric_id >= 0 && !gl(c,
                               mc_client_display_source_bind_id(
                                   c->state.display, c->state.texture_id, &c->failure.display),
                               bind))
        return false;
    c->state.texture_bound = numeric_id >= 0;
    return true;
}

static bool quad(mc_client_item_gui_call *c) {
    const char *descriptor = "(IIIIII)V";
    c->state.quad_pc = 11;
    if (!hold_graphics(c, 352) ||
        !graphics(c,
                  mc_client_nw_active_use(c->graphics, site(descriptor, 11), &c->nw,
                                          &c->failure.graphics),
                  descriptor, 11))
        return false;
    c->state.nw = c->nw;
    c->state.quad_pc = 18;
    if (!graphics(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics), descriptor, 18))
        return false;
    c->state.quad_started = true;
    static const int32_t vertex_pc[4] = {53, 90, 125, 158};
    for (size_t i = 0; i < 4; ++i) {
        if (!fp(c, 352))
            return false;
        int32_t x = mc_java_i32_add(c->state.x, i == 1 || i == 2 ? 16 : 0);
        int32_t y = mc_java_i32_add(c->state.y, i < 2 ? 16 : 0);
        int32_t u = mc_java_i32_add(c->state.texture_x, i == 1 || i == 2 ? 16 : 0);
        int32_t v = mc_java_i32_add(c->state.texture_y, i < 2 ? 16 : 0);
        /* Java i2f/fmul/f2d; the exact power-of-two product is binary32. */
        volatile float uf = (float)u * 0.00390625f;
        volatile float vf = (float)v * 0.00390625f;
        double *p = c->state.vertices[i];
        p[0] = (double)x;
        p[1] = (double)y;
        p[2] = 0.0;
        p[3] = (double)uf;
        p[4] = (double)vf;
        c->state.quad_pc = vertex_pc[i];
        if (!graphics(
                c,
                mc_client_nw_vertex_uv(c->nw, p[0], p[1], p[2], p[3], p[4], &c->failure.graphics),
                descriptor, vertex_pc[i]))
            return false;
        ++c->state.vertices_returned;
    }
    c->state.quad_pc = 163;
    if (!graphics(c, mc_client_nw_draw(c->nw, c->state.display, &c->failure.graphics), descriptor,
                  163))
        return false;
    c->state.quad_drawn = c->state.quad_returned = true;
    c->state.quad_pc = 166;
    return true;
}

static bool item_color(mc_client_item_gui_call *c, int32_t active_pc, int32_t index_pc,
                       int32_t invoke_pc) {
    if (mc_registry_item_c_get(c->registry, &c->state.item_array, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return registry_failure(c, "GUI Item actual GM.c active use", active_pc);
    if (mc_registry_item_c_at(c->registry, c->state.item_array, c->state.id, &c->state.color_item,
                              &c->failure.registry) != MC_REGISTRY_OK)
        return registry_failure(c, "GUI Item actual GM.c aaload", index_pc);
    if (c->state.color_item == NULL)
        return null_reference(c, "actual GUI Item null color Item", invoke_pc);
    if (c->state.color_item->kind == MC_ITEM_LEAVES_BLOCK && !hold_graphics(c, invoke_pc))
        return false;
    const mc_item_render_color_context colors = {c->graphics, mc_client_colorizer_read};
    if (!mc_item_render_color_with_context(c->registry, c->state.color_item, c->state.metadata,
                                           &colors, &c->state.color, &c->failure.item)) {
        c->failure.child =
            (mc_client_graphics_site){c->failure.item.site.class_name, c->failure.item.site.method,
                                      c->failure.item.site.descriptor, c->failure.item.site.pc};
        c->failure.source_exception = c->failure.item.source_exception;
        c->failure.exception_class = c->failure.item.exception_class;
        c->failure.throwable = c->failure.item.throwable;
        return fail(c, MC_CLIENT_ITEM_GUI_ITEM_FAILED, c->failure.item.operation, raw_descriptor,
                    invoke_pc, c->failure.item.native_policy);
    }
    c->state.color_returned = true;
    return true;
}

static void inventory_throwable(mc_client_item_gui_call *c,
                                const mc_client_render_global_error *e) {
    if (e->native_policy)
        return;
    const char *name = NULL;
    if (e->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE)
        name = "java/lang/NullPointerException";
    else if (e->status == MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED && !e->graphics.native_policy) {
        switch (e->graphics.status) {
            case MC_CLIENT_GRAPHICS_NULL_REFERENCE:
                name = "java/lang/NullPointerException";
                break;
            case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT:
                name = "java/lang/IllegalArgumentException";
                break;
            case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE:
                name = "java/lang/IllegalStateException";
                break;
            case MC_CLIENT_GRAPHICS_ARRAY_INDEX:
                name = "java/lang/ArrayIndexOutOfBoundsException";
                break;
            case MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW:
                name = "java/nio/BufferOverflowException";
                break;
            case MC_CLIENT_GRAPHICS_BUFFER_UNDERFLOW:
                name = "java/nio/BufferUnderflowException";
                break;
            default:
                break;
        }
    }
    if (name != NULL) {
        c->failure.throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
        c->failure.source_exception = true;
        c->failure.exception_class = name;
    }
}

static bool block_inventory(mc_client_item_gui_call *c) {
    if (!lookup(c, "/terrain.png", 28, 31))
        return false;
    mc_registry_block_m_view blocks;
    if (mc_registry_block_m_get(c->registry, &blocks, &c->failure.registry) != MC_REGISTRY_OK)
        return registry_failure(c, "GUI Item actual second UU.m active use", 34);
    if (mc_registry_block_m_at(&blocks, c->state.id, &c->state.block, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return registry_failure(c, "GUI Item actual second UU.m aaload", 39);
    if (!gl(c, mc_client_display_source_push(c->state.display, &c->failure.display), 42))
        return false;
    c->state.block_matrix_pushed = true;
    if (!fp(c, 57) ||
        !gl(c,
            mc_client_display_source_translate(
                c->state.display, (float)mc_java_i32_sub(c->state.x, 2),
                (float)mc_java_i32_add(c->state.y, 3), -3.0f, &c->failure.display),
            57) ||
        !gl(c, mc_client_display_source_scale(c->state.display, 10, 10, 10, &c->failure.display),
            66) ||
        !gl(c,
            mc_client_display_source_translate(c->state.display, 1, 0.5f, 1, &c->failure.display),
            73) ||
        !gl(c, mc_client_display_source_scale(c->state.display, 1, 1, -1, &c->failure.display),
            80) ||
        !gl(c, mc_client_display_source_rotate(c->state.display, 210, 1, 0, 0, &c->failure.display),
            88) ||
        !gl(c, mc_client_display_source_rotate(c->state.display, 45, 0, 1, 0, &c->failure.display),
            96))
        return false;
    if (!item_color(c, 99, 103, 106) || !fp(c, 123))
        return false;
    for (size_t i = 0; i < 3; ++i) {
        volatile float component =
            (float)(((uint32_t)c->state.color >> (unsigned int)(16 - 8 * i)) & 255u) / 255.0f;
        c->state.color_rgba[i] = component;
    }
    c->state.color_rgba[3] = 1.0f;
    if (c->owner->fields.a) {
        if (!gl(c,
                mc_client_display_source_color(c->state.display, c->state.color_rgba[0],
                                               c->state.color_rgba[1], c->state.color_rgba[2], 1,
                                               &c->failure.display),
                167))
            return false;
        c->state.color_set = true;
    }
    if (!gl(c, mc_client_display_source_rotate(c->state.display, -90, 0, 1, 0, &c->failure.display),
            175))
        return false;
    mc_client_render_blocks *fields = c->owner->fields.e;
    if (fields == NULL)
        return null_reference(c, "GUI Item actual CV.b store receiver", 186);
    fields->b = c->owner->fields.a;
    c->state.block_color_stored = true;
    fields = c->owner->fields.e;
    if (fields == NULL)
        return null_reference(c, "GUI Item actual inventory CV receiver", 198);
    if (!hold_graphics(c, 198))
        return false;
    if (c->owner->bytes > c->owner->max_bytes ||
        sizeof(*c->inventory) > c->owner->max_bytes - c->owner->bytes)
        return fail(c, MC_CLIENT_ITEM_GUI_LIMIT, "GUI Item inventory diagnostic allocation ceiling",
                    raw_descriptor, 198, true);
    c->inventory = calloc(1, sizeof(*c->inventory));
    if (c->inventory == NULL)
        return fail(c, MC_CLIENT_ITEM_GUI_ALLOCATION_FAILED,
                    "GUI Item inventory diagnostic allocation", raw_descriptor, 198, true);
    c->owner->bytes += sizeof(*c->inventory);
    *c->inventory = (mc_cv_context){.fields = fields,
                                    .registry = c->registry,
                                    .models = c->owner->classes,
                                    .graphics = c->graphics,
                                    .display = c->state.display};
    c->failure.inventory = c->inventory;
    c->state.block_inventory_entered = true;
    if (!mc_cv_inventory(c->inventory, c->state.block, c->state.metadata, 1.0f)) {
        const mc_client_render_global_error *e = &c->inventory->failure;
        c->failure.child = (mc_client_graphics_site){e->source_class, e->source_method,
                                                     e->source_descriptor, (int32_t)e->source_pc};
        inventory_throwable(c, e);
        return fail(c, MC_CLIENT_ITEM_GUI_GRAPHICS_FAILED, e->operation, raw_descriptor, 198,
                    e->native_policy);
    }
    c->state.block_inventory_returned = true;
    c->state.block_faces = c->inventory->inventory_faces;
    fields = c->owner->fields.e;
    if (fields == NULL)
        return null_reference(c, "GUI Item actual returned CV.b store receiver", 206);
    fields->b = true;
    if (!gl(c, mc_client_display_source_pop(c->state.display, &c->failure.display), 209))
        return false;
    c->state.block_matrix_pushed = false;
    return true;
}

static bool body(mc_client_item_gui_call *c) {
    if (c->stack == NULL) {
        c->state.null_stack = c->state.returned = true;
        c->state.entry_pc = 4;
        return true;
    }
    c->failure.stack = mc_item_stack_retain(c->stack);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_CLIENT_ITEM_GUI_BUSY, "GUI Item reached Stack occurrence",
                    stack_descriptor, 9, true);
    c->stack_held = true;
    c->state.entry_pc = 9;
    c->failure.stack = mc_item_stack_source_id_get(c->stack, &c->state.id);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_CLIENT_ITEM_GUI_ITEM_FAILED, "GUI Item actual Stack.c field",
                    stack_descriptor, 9, true);
    c->state.id_read = true;
    c->state.entry_pc = 13;
    c->failure.stack = mc_item_stack_source_damage_get(c->stack, &c->state.metadata);
    if (c->failure.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_CLIENT_ITEM_GUI_ITEM_FAILED, "GUI Item actual IZ.i field",
                    stack_descriptor, 13, true);
    c->state.metadata_read = true;
    c->state.entry_pc = 17;
    if (mc_registry_classes_retain(c->registry, MC_REGISTRY_CLIENT, c->owner->classes->profile,
                                   &c->failure.registry) != MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_ITEM_GUI_REGISTRY_FAILED, "GUI Item Registry occurrence",
                    stack_descriptor, 17, true);
    c->registry_held = true;
    if (!mc_item_stack_render_icon(c->registry, c->stack, &c->state.icon, &c->failure.item)) {
        c->failure.child =
            (mc_client_graphics_site){c->failure.item.site.class_name, c->failure.item.site.method,
                                      c->failure.item.site.descriptor, c->failure.item.site.pc};
        c->failure.source_exception = c->failure.item.source_exception;
        c->failure.exception_class = c->failure.item.exception_class;
        c->failure.throwable = c->failure.item.throwable;
        return fail(c, MC_CLIENT_ITEM_GUI_ITEM_FAILED, c->failure.item.operation, stack_descriptor,
                    17, c->failure.item.native_policy);
    }
    c->state.icon_returned = true;
    c->state.entry_pc = 24;
    c->state.raw_entered = true;
    c->failure.caller = site(stack_descriptor, 24);
    if (c->state.id < 256) {
        mc_registry_block_m_view blocks;
        if (mc_registry_block_m_get(c->registry, &blocks, &c->failure.registry) != MC_REGISTRY_OK)
            return registry_failure(c, "GUI Item actual UU.m active use", 7);
        if (mc_registry_block_m_at(&blocks, c->state.id, &c->state.block, &c->failure.registry) !=
            MC_REGISTRY_OK)
            return registry_failure(c, "GUI Item actual UU.m aaload", 11);
        if (c->state.block == NULL)
            return null_reference(c, "actual GUI Item null Block render-type receiver", 12);
        mc_block_render_description description;
        if (mc_block_render_describe(c->state.block, &description) != MC_REGISTRY_OK)
            return fail(c, MC_CLIENT_ITEM_GUI_UNIMPLEMENTED,
                        "GUI Item actual Block render-type dispatch", raw_descriptor, 12, true);
        c->state.block_type = description.type;
        c->state.block_type_returned = true;
        c->state.block_3d = mc_cv_inventory_type_3d(description.type);
        if (c->state.block_3d) {
            if (!block_inventory(c))
                return false;
            goto culling;
        }
    }
    c->state.raw_pc = 215;
    c->state.negative_icon = c->state.icon < 0;
    if (!c->state.negative_icon) {
        if (!gl(c,
                mc_client_display_source_lighting_enable(c->state.display, false,
                                                         &c->failure.display),
                223))
            return false;
        c->state.lighting_disabled = true;
        if (!lookup(c, c->state.id < 256 ? "/terrain.png" : "/gui/items.png",
                    c->state.id < 256 ? 237 : 250, c->state.id < 256 ? 240 : 253))
            return false;
        if (!item_color(c, 256, 260, 263))
            return false;
        if (!fp(c, 280))
            return false;
        for (size_t i = 0; i < 3; ++i) {
            unsigned int shift = (unsigned int)(16 - 8 * i);
            volatile float component = (float)(((uint32_t)c->state.color >> shift) & 255u) / 255.0f;
            c->state.color_rgba[i] = component;
        }
        c->state.color_rgba[3] = 1.0f;
        if (c->owner->fields.a) {
            if (!gl(c,
                    mc_client_display_source_color(c->state.display, c->state.color_rgba[0],
                                                   c->state.color_rgba[1], c->state.color_rgba[2],
                                                   1.0f, &c->failure.display),
                    324))
                return false;
            c->state.color_set = true;
        }
        c->state.texture_x = mc_java_i32_mul(c->state.icon % 16, 16);
        c->state.texture_y = mc_java_i32_mul(c->state.icon / 16, 16);
        if (!quad(c))
            return false;
        if (!gl(c,
                mc_client_display_source_lighting_enable(c->state.display, true,
                                                         &c->failure.display),
                358))
            return false;
        c->state.lighting_enabled = true;
    }
culling:
    if (!gl(c, mc_client_display_source_cull_test(c->state.display, true, &c->failure.display),
            364))
        return false;
    c->state.culling_enabled = c->state.returned = true;
    c->state.raw_pc = 367;
    c->state.entry_pc = 27;
    return true;
}

mc_client_item_gui_status
mc_client_item_gui_begin(mc_client_item_renderer *owner, mc_client_render_engine_classes *graphics,
                         mc_registry_classes *registry, mc_client_display *display,
                         mc_client_bitmap_font *font, mc_client_render_engine *engine,
                         mc_item_stack *stack, int32_t x, int32_t y, mc_client_item_gui_call **out,
                         mc_client_item_gui_error *error) {
    (void)font;
    if (owner == NULL || out == NULL || *out != NULL || error == NULL)
        return admission(error, MC_CLIENT_ITEM_GUI_INVALID_ARGUMENT, "GUI Item native admission");
    if (owner->phase != MC_CLIENT_MODEL_READY || owner->gui_active || owner->gui_call != NULL)
        return admission(error, MC_CLIENT_ITEM_GUI_BUSY, "GUI Item actual owner active/pending");
    mc_client_item_gui_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_CLIENT_ITEM_GUI_ALLOCATION_FAILED,
                         "GUI Item native call record");
    c->state = (mc_client_item_gui_state){.owner = owner,
                                          .graphics = graphics,
                                          .registry = registry,
                                          .display = display,
                                          .engine = engine,
                                          .stack = stack,
                                          .x = x,
                                          .y = y,
                                          .entry_pc = 0,
                                          .raw_pc = -1,
                                          .quad_pc = -1,
                                          .failure = &c->failure,
                                          .cleanup = &c->cleanup,
                                          .active = true};
    c->owner = owner;
    c->graphics = graphics;
    c->registry = registry;
    c->engine = engine;
    c->stack = stack;
    owner->gui_call = c;
    owner->gui_active = true;
    *out = c;
    (void)body(c);
    owner->gui_active = c->state.active = false;
    *error = c->failure;
    return c->failure.status;
}

bool mc_client_item_gui_inspect(const mc_client_item_gui_call *c, mc_client_item_gui_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

mc_client_item_gui_status mc_client_item_gui_retire(mc_client_item_gui_call **slot,
                                                    mc_client_item_gui_error *error) {
    if (slot == NULL || error == NULL)
        return admission(error, MC_CLIENT_ITEM_GUI_INVALID_ARGUMENT, "GUI Item retirement slot");
    mc_client_item_gui_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_CLIENT_ITEM_GUI_OK, NULL);
    if (c->state.active || c->owner->gui_active || c->owner->gui_call != c)
        return admission(error, MC_CLIENT_ITEM_GUI_BUSY, "GUI Item retirement active/changed");
    c->state.retiring = true;
    c->owner->gui_active = c->state.active = true;
    if (c->pending_owned) {
        mc_client_texture_lookup_status status =
            mc_client_render_engine_retire_lookup(c->engine, &c->cleanup.texture);
        c->state.pending_lookup_owned = c->pending_owned =
            mc_client_texture_lookup_pending(c->engine);
        if (status != MC_CLIENT_LOOKUP_OK)
            goto refused;
    }
    if (c->engine_held) {
        c->cleanup.owner = mc_client_render_engine_release(c->engine);
        if (c->cleanup.owner != MC_CLIENT_RENDER_ENGINE_OK)
            goto refused;
        c->engine_held = false;
    }
    if (c->graphics_lease) {
        if (c->graphics->scene_lease_count == 0)
            goto refused;
        --c->graphics->scene_lease_count;
        c->graphics_lease = false;
        c->state.nw = c->nw = NULL;
    }
    if (c->graphics_held) {
        c->cleanup.owner = mc_client_render_engine_classes_release(c->graphics);
        if (c->cleanup.owner != MC_CLIENT_RENDER_ENGINE_OK)
            goto refused;
        c->graphics_held = false;
    }
    if (c->stack_held) {
        mc_item_stack_release(c->stack);
        c->stack_held = false;
    }
    if (c->registry_held) {
        if (mc_registry_classes_release(c->registry, &c->cleanup.registry) != MC_REGISTRY_OK)
            goto refused;
        c->registry_held = false;
    }
    c->owner->gui_call = NULL;
    if (c->inventory != NULL) {
        c->owner->bytes -= sizeof(*c->inventory);
        free(c->inventory);
    }
    c->owner->gui_active = false;
    free(c);
    *slot = NULL;
    return admission(error, MC_CLIENT_ITEM_GUI_OK, NULL);
refused:
    c->owner->gui_active = c->state.active = false;
    c->cleanup.status = MC_CLIENT_ITEM_GUI_CLEANUP_FAILED;
    c->cleanup.operation = "GUI Item checked remaining native retirement";
    c->cleanup.native_policy = true;
    *error = c->cleanup;
    return error->status;
}
